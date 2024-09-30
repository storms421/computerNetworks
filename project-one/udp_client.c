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

#define BUF_SIZE 2048   // Max buffer size of the data in a frame
#define SERVER_PORT 2940 // Server Port

// Frame structure for data packets
struct frame_packet {
    long int ID;
    long int length;
    char data[BUF_SIZE];
};

// Error handler function
static void print_error(char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// Main client function
int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Client: Usage: ./[%s] Hostname \n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in send_addr, from_addr; // Socket addresses for server and client
    struct frame_packet frame;               // Packet frame structure
    struct timeval t_out = { 0, 0 };         // Timeout structure
    struct hostent* h;
    char protocolType_send[50];              // Protocol selection (Stop-and-Wait or Go-Back-N)
    char file_name[20];                      // File name to receive
    char protocolType[10];                   // Protocol type (1 = SW, 2 = GBN)
    char percent[10];                        // Drop percentage
    char ack_send[4] = "ACK";                // Acknowledgment message

    ssize_t length = 0;
    int s;
    FILE* fp;

    // Initialize buffers
    memset(ack_send, 0, sizeof(ack_send));
    memset(&send_addr, 0, sizeof(send_addr));
    memset(&from_addr, 0, sizeof(from_addr));

    // Host IP lookup
    h = gethostbyname(argv[1]);
    if (!h) {
        print_error("gethostbyname failed");
    }

    // Fill server structure with IP and port #
    send_addr.sin_family = AF_INET;
    send_addr.sin_port = htons(SERVER_PORT);
    memcpy(&send_addr.sin_addr.s_addr, h->h_addr, h->h_length);

    // Create socket
    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        print_error("Client: Socket");
    }

    for (;;) {
        memset(protocolType_send, 0, sizeof(protocolType_send));
        memset(protocolType, 0, sizeof(protocolType));
        memset(file_name, 0, sizeof(file_name));

        // Display menu and ask for user input
        printf("\n -----");
        printf("\n Menu");
        printf("\n -----");
        printf("\n Enter the protocol number followed by file name and drop percentage:");
        printf("\n ------------------------------------------------");
        printf("\n For Stop-and-Wait enter [1]: \n Example: 1 [File Name] [percentage]\n");
        printf("\n For Go-Back-N enter [2]: \n Example: 2 [File Name] [percentage]\n");
        printf("\n ------------------------------------------------");
        printf("\n INPUT: ");
        scanf(" %[^\n]%*c", protocolType_send);

        // Parse user input
        sscanf(protocolType_send, "%s %s %s", protocolType, file_name, percent);

        // Send protocol type and file name to server
        if (sendto(s, protocolType_send, sizeof(protocolType_send), 0, (struct sockaddr*)&send_addr, sizeof(send_addr)) == -1) {
            print_error("Client: Send");
        }

        // Stop-and-Wait Protocol
        if (strcmp(protocolType, "1") == 0 && file_name[0] != '\0') {
            long int total_frame = 0;
            long int bytes_rec = 0, i = 0;

            // Set timeout for receiving
            t_out.tv_sec = 2;
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&t_out, sizeof(struct timeval));

            // Receive total number of frames from server
            recvfrom(s, &total_frame, sizeof(total_frame), 0, (struct sockaddr*)&from_addr, (socklen_t*)&length);

            // Reset timeout
            t_out.tv_sec = 0;
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&t_out, sizeof(struct timeval));

            if (total_frame > 0) {
                printf("\n SERVER: Total number of frames to be transmitted: %ld frames\n", total_frame);
                fp = fopen(file_name, "wb");

                // Receive frames one by one
                for (i = 1; i <= total_frame; i++) {
                    memset(&frame, 0, sizeof(frame));
                    recvfrom(s, &frame, sizeof(frame), 0, (struct sockaddr*)&from_addr, (socklen_t*)&length);

                    // Send acknowledgment back to server
                    sendto(s, &frame.ID, sizeof(frame.ID), 0, (struct sockaddr*)&send_addr, sizeof(send_addr));

                    // Drop repeated frames
                    if (frame.ID != i) {
                        i--;
                    } else {
                        fwrite(frame.data, 1, frame.length, fp);
                        printf("Frame Number # %ld \t Frame Length -> %ld\n", frame.ID, frame.length);
                        bytes_rec += frame.length;
                    }
                }

                printf("Transmission Completed!\n");
                printf("Total bytes transmitted -> %ld\n", bytes_rec);
                fclose(fp);
            } else {
                printf("File is empty or invalid.\n");
            }
        }

        // Go-Back-N Protocol
        if (strcmp(protocolType, "2") == 0 && file_name[0] != '\0') {
            long int total_frame = 0;
            long int bytes_rec = 0, i = 0;

            // Set timeout for receiving
            t_out.tv_sec = 2;
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&t_out, sizeof(struct timeval));

            // Receive total number of frames from server
            recvfrom(s, &total_frame, sizeof(total_frame), 0, (struct sockaddr*)&from_addr, (socklen_t*)&length);

            // Reset timeout
            t_out.tv_sec = 0;
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&t_out, sizeof(struct timeval));

            if (total_frame > 0) {
                printf("\n SERVER: Total number of frames to be transmitted: %ld frames\n", total_frame);
                fp = fopen(file_name, "wb");

                // Receive frames one by one in Go-Back-N fashion
                for (i = 1; i <= total_frame; i++) {
                    memset(&frame, 0, sizeof(frame));
                    recvfrom(s, &frame, sizeof(frame), 0, (struct sockaddr*)&from_addr, (socklen_t*)&length);

                    // Send acknowledgment back to server
                    sendto(s, &frame.ID, sizeof(frame.ID), 0, (struct sockaddr*)&send_addr, sizeof(send_addr));

                    // Drop repeated frames
                    if (frame.ID != i) {
                        i--;
                    } else {
                        fwrite(frame.data, 1, frame.length, fp);
                        printf("Frame Number # %ld \t Frame Length -> %ld\n", frame.ID, frame.length);
                        bytes_rec += frame.length;
                    }
                }

                printf("Transmission Completed!\n");
                printf("Total bytes transmitted -> %ld\n", bytes_rec);
                fclose(fp);
            } else {
                printf("File is empty or invalid.\n");
            }
        }
    }

    close(s);
    exit(EXIT_SUCCESS);
}
