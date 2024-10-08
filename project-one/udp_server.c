#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <dirent.h>
#include <math.h>
#include <time.h> // Include for random drop generation

#define BUF_SIZE 4096 // Max buffer size of the data in a frame
#define SERVER_PORT 2226 // Use your port number as specified

struct frame_packet {
    long int ID;
    long int length;
    char data[BUF_SIZE];
};

struct frame_buf {
    long int ID;
    long int length;
    char data[BUF_SIZE];
};

static void print_error(char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// Main loop
int main(int argc, char** argv) {
    if ((argc < 1) || (argc > 1)) {
        printf("Usage: ./[%s] \n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct stat st; 
    struct sockaddr_in s_addr, c_addr; 
    struct frame_buf window_buf[3]; 
    struct frame_packet frame; 
    struct timeval t_out = { 0, 0 }; 
    char msg_recv[BUF_SIZE]; 
    char file_name_recv[20]; 
    char protocolType_recv[10]; 
    char percent[10]; 
    ssize_t numRead;
    ssize_t length;
    off_t f_size;
    long int ack_num = 0;
    int ack_send = 0;
    int s;
    FILE* fp; 

    memset(&s_addr, 0, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(SERVER_PORT);
    s_addr.sin_addr.s_addr = INADDR_ANY;

    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        print_error("Server: Socket");
    }

    if (bind(s, (struct sockaddr*)&s_addr, sizeof(s_addr)) == -1) {
        print_error("Server: Bind");
    }

    for (;;) {
        printf("Server: Waiting for client to connect\n");
        memset(msg_recv, 0, sizeof(msg_recv));
        memset(protocolType_recv, 0, sizeof(protocolType_recv));
        memset(file_name_recv, 0, sizeof(file_name_recv));
        memset(percent, 0, sizeof(percent));
        length = sizeof(c_addr);

        if ((numRead = recvfrom(s, msg_recv, BUF_SIZE, 0, (struct sockaddr*)&c_addr, (socklen_t*)&length)) == -1) {
            print_error("Server: Received");
        }
        printf("Protocol and File Name Requested: %s\n", msg_recv);

        sscanf(msg_recv, "%s %s %s", protocolType_recv, file_name_recv, percent);

        if ((strcmp(protocolType_recv, "1") == 0) && (file_name_recv[0] != '\0')) {
            // Stop-and-Wait protocol
            printf("\nClient Requested File: %s\n", file_name_recv);
            if (access(file_name_recv, F_OK) == 0) { 
                int total_frame = 0, resend_frame = 0, drop_frame = 0, t_out_flag = 0;
                long int i = 0;
                stat(file_name_recv, &st); 
                f_size = st.st_size; 
                t_out.tv_sec = 2;
                t_out.tv_usec = 0;
                setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&t_out, sizeof(struct timeval)); 

                fp = fopen(file_name_recv, "rb"); 
                total_frame = (f_size % BUF_SIZE == 0) ? (f_size / BUF_SIZE) : (f_size / BUF_SIZE) + 1;
                printf("Total number of packets that will be sent -> %d\n", total_frame);

                float fpercent = atof(percent);
                float numpercent = fpercent / 100;
                int td = (int)(numpercent * total_frame);
                printf("Total frame drop: %i\n\n", td);
                srand(time(0)); 

                int testdrop[td];
                for (i = 0; i < td; i++) {
                    testdrop[i] = rand() % total_frame;
                }

                length = sizeof(c_addr);
                sendto(s, &(total_frame), sizeof(total_frame), 0, (struct sockaddr*)&c_addr, sizeof(c_addr)); 
                recvfrom(s, &(ack_num), sizeof(ack_num), 0, (struct sockaddr*)&c_addr, (socklen_t*)&length); 

                while (ack_num != total_frame) {
                    sendto(s, &(total_frame), sizeof(total_frame), 0, (struct sockaddr*)&c_addr, sizeof(c_addr));
                    recvfrom(s, &(ack_num), sizeof(ack_num), 0, (struct sockaddr*)&c_addr, (socklen_t*)&length);
                    resend_frame++;
                    if (resend_frame == 3) {
                        t_out_flag = 1;
                        break;
                    }
                }

                for (i = 1; i <= total_frame; i++) {
                    memset(&frame, 0, sizeof(frame));
                    ack_num = 0;
                    frame.ID = i;
                    frame.length = fread(frame.data, 1, BUF_SIZE, fp);
                    sendto(s, &(frame), sizeof(frame), 0, (struct sockaddr*)&c_addr, sizeof(c_addr)); 
                    recvfrom(s, &(ack_num), sizeof(ack_num), 0, (struct sockaddr*)&c_addr, (socklen_t*)&length); 

                    for (int j = 0; j < td; j++) {
                        if (frame.ID == testdrop[j]) {
                            sendto(s, &(frame), sizeof(frame), 0, (struct sockaddr*)&c_addr, sizeof(c_addr));
                            recvfrom(s, &(ack_num), sizeof(ack_num), 0, (struct sockaddr*)&c_addr, (socklen_t*)&length);
                            printf("frame.id# %ld \t dropped, %d times\n", frame.ID, ++drop_frame);
                            resend_frame++;
                            if (resend_frame == 3) {
                                t_out_flag = 1;
                                break;
                            }
                        }
                    }

                    resend_frame = 0;
                    drop_frame = 0;
                    if (t_out_flag == 1) {
                        printf("File not sent\n");
                        break;
                    }
                    printf("frame.id# %ld \t Ack -> %ld \n", i, ack_num);
                    if (total_frame == ack_num) {
                        printf("File sent\n");
                    }
                }

                fclose(fp);
                t_out.tv_sec = 0;
                t_out.tv_usec = 0;
                setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&t_out, sizeof(struct timeval)); 
            } else {
                printf("Invalid Filename\n");
            }
        }
        // Go-Back-N protocol
    }

    close(s);
    exit(EXIT_SUCCESS);
}
