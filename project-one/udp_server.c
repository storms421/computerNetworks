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
#include <time.h>

#define BUF_SIZE (2048) // Max buffer size of the data in a frame
#define SERVER_PORT 2226 // Server Port
#define WINDOW_SIZE 3 // Window size for GBN

// Frame structure
struct frame_packet {
    long int ID;
    long int length;
    char data[BUF_SIZE];
};

// Function prototypes
void print_error(char* msg);
void stop_and_wait(int s, struct sockaddr_in* c_addr, socklen_t length, FILE* fp, int total_frame, int* testdrop, int td);
void go_back_n(int s, struct sockaddr_in* c_addr, socklen_t length, FILE* fp, int total_frame, int* testdrop, int td);
int* generate_drops(int total_frame, float drop_percent);

int main(int argc, char** argv) {
    if (argc != 1) {
        printf("Usage: ./[%s]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct stat st;
    struct sockaddr_in s_addr, c_addr;
    struct timeval t_out = { 0, 0 }; // Timeout defined
    char msg_recv[BUF_SIZE]; // data that holds received message
    char file_name_recv[20]; // File name received
    char protocolType_recv[10]; // Protocol type (1 for SW, 2 for GBN)
    char percent[10]; // Drop percentage
    
    ssize_t numRead;
    ssize_t length;
    off_t f_size;
    int s;
    FILE* fp;

    // Initialize server address
    memset(&s_addr, 0, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(SERVER_PORT);
    s_addr.sin_addr.s_addr = INADDR_ANY;

    // Create socket
    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        print_error("Server: Socket");
    }

    // Bind socket
    if (bind(s, (struct sockaddr*)&s_addr, sizeof(s_addr)) == -1) {
        print_error("Server: Bind");
    }

    for (;;) {
        printf("Server: Waiting for client to connect\n");

        // Clear buffers
        memset(msg_recv, 0, sizeof(msg_recv));
        memset(protocolType_recv, 0, sizeof(protocolType_recv));
        memset(file_name_recv, 0, sizeof(file_name_recv));
        memset(percent, 0, sizeof(percent));
        length = sizeof(c_addr);

        // Receive request from client
        if ((numRead = recvfrom(s, msg_recv, BUF_SIZE, 0, (struct sockaddr*)&c_addr, (socklen_t*)&length)) == -1) {
            print_error("Server: Received");
        }
        printf("Protocol and File Name Requested: %s\n", msg_recv);

        // Parse protocol, filename, and drop percentage
        sscanf(msg_recv, "%s %s %s", protocolType_recv, file_name_recv, percent);

        if (access(file_name_recv, F_OK) == 0) { // Check if file exists
            stat(file_name_recv, &st);
            f_size = st.st_size;
            fp = fopen(file_name_recv, "rb"); // Open file to send

            long int total_frame = (f_size % BUF_SIZE) == 0 ? (f_size / BUF_SIZE) : (f_size / BUF_SIZE) + 1;
            printf("Total number of packets that will be sent -> %d\n", total_frame);
            sendto(s, &total_frame, sizeof(total_frame), 0, (struct sockaddr*)c_addr, length);

            // Calculate drop frames
            float drop_percent = atof(percent);
            int* testdrop = generate_drops(total_frame, drop_percent);
            int td = (int)(drop_percent * total_frame / 100);
            printf("Total frame drop: %i\n\n", td);

            if (strcmp(protocolType_recv, "1") == 0) {
                // Stop-and-Wait protocol
                stop_and_wait(s, &c_addr, length, fp, total_frame, testdrop, td);
            } else if (strcmp(protocolType_recv, "2") == 0) {
                // Go-Back-N protocol
                go_back_n(s, &c_addr, length, fp, total_frame, testdrop, td);
            } else {
                printf("Invalid Protocol Type\n");
            }

            fclose(fp); // Close file after sending
        } else {
            printf("Invalid Filename\n");
        }
    }

    close(s);
    return 0;
}

// Error handler
void print_error(char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// Generates random frame drops
int* generate_drops(int total_frame, float drop_percent) {
    int td = (int)(drop_percent * total_frame / 100);
    int* testdrop = (int*)malloc(td * sizeof(int));
    srand(time(NULL));

    for (int i = 0; i < td; i++) {
        testdrop[i] = rand() % total_frame;
        printf("Frame to drop: %d\n", testdrop[i]);
    }

    return testdrop;
}

// Stop-and-Wait ARQ Protocol
void stop_and_wait(int s, struct sockaddr_in* c_addr, socklen_t length, FILE* fp, int total_frame, int* testdrop, int td) {
    struct frame_packet frame;
    int ack_num = 0, drop_frame = 0, resend_frame = 0;
    long int i;

    for (i = 1; i <= total_frame; i++) {
        memset(&frame, 0, sizeof(frame));
        frame.ID = i;
        frame.length = fread(frame.data, 1, BUF_SIZE, fp);

        // Check if frame should be dropped
        int drop = 0;
        for (int j = 0; j < td; j++) {
            if (frame.ID == testdrop[j]) {
                drop = 1;
                break;
            }
        }

        if (drop) {
            printf("frame.id# %ld \t dropped\n", frame.ID);
            drop_frame++;
            continue;
        }

        // Send frame and wait for acknowledgment
        sendto(s, &frame, sizeof(frame), 0, (struct sockaddr*)c_addr, length);
        recvfrom(s, &ack_num, sizeof(ack_num), 0, (struct sockaddr*)c_addr, &length);

        if (ack_num == frame.ID) {
            printf("frame.id# %ld \t Ack -> %ld \n", frame.ID, ack_num);
        } else {
            printf("Ack mismatch. Resending frame.id# %ld\n", frame.ID);
            resend_frame++;
            i--; // Resend current frame
        }

        if (resend_frame == 3) {
            printf("File transfer failed due to excessive retries\n");
            return;
        }
    }

    printf("File sent successfully using Stop-and-Wait\n");
}

// Go-Back-N ARQ Protocol
void go_back_n(int s, struct sockaddr_in* c_addr, socklen_t length, FILE* fp, int total_frame, int* testdrop, int td) {
    struct frame_packet frame;
    int ack_num = 0, resend_frame = 0, drop_frame = 0;
    long int base = 1, next_seq_num = 1;

    while (base <= total_frame) {
        while (next_seq_num < base + WINDOW_SIZE && next_seq_num <= total_frame) {
            memset(&frame, 0, sizeof(frame));
            frame.ID = next_seq_num;
            frame.length = fread(frame.data, 1, BUF_SIZE, fp);

            // Check if frame should be dropped
            int drop = 0;
            for (int j = 0; j < td; j++) {
                if (frame.ID == testdrop[j]) {
                    drop = 1;
                    break;
                }
            }

            if (drop) {
                printf("frame.id# %ld \t dropped\n", frame.ID);
                drop_frame++;
            } else {
                sendto(s, &frame, sizeof(frame), 0, (struct sockaddr*)c_addr, length);
                printf("Sent frame.id# %ld\n", frame.ID);
            }

            next_seq_num++;
        }

        // Receive acknowledgment
        recvfrom(s, &ack_num, sizeof(ack_num), 0, (struct sockaddr*)c_addr, &length);

        if (ack_num >= base) {
            printf("Ack received for frame.id# %d\n", ack_num);
            base = ack_num + 1; // Slide the window
        }
    }
}
