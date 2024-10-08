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
#include <time.h> // Include for timestamp

#define BUF_SIZE 4096 // Max buffer size of the data in a frame
#define SERVER_PORT 2226 // Use your port number as specified

struct frame_packet {
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
    if (argc != 4) { // Expecting exactly 3 arguments: server_hostname, file_name, protocol_type
        printf("Client: Usage: %s server_hostname file_name protocol_type\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in send_addr, from_addr;
    struct frame_packet frame;
    struct timeval t_out = { 0, 0 };
    struct hostent* h;
    ssize_t length = 0;
    int s;
    FILE* fp;

    // Command-line arguments
    char* server_hostname = argv[1];
    char* file_name = argv[2];
    int protocol_type = atoi(argv[3]);

    memset(&send_addr, 0, sizeof(send_addr));
    memset(&from_addr, 0, sizeof(from_addr));

    // Host IP lookup
    h = gethostbyname(server_hostname);
    if (!h) {
        print_error("gethostbyname failed");
    }

    // Setup server address struct
    send_addr.sin_family = AF_INET;
    send_addr.sin_port = htons(SERVER_PORT);
    memcpy(&send_addr.sin_addr.s_addr, h->h_addr, h->h_length);

    // Create socket
    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        print_error("Client: Socket creation failed");
    }

    // Send protocol type and filename to the server
    char request_message[100];
    snprintf(request_message, sizeof(request_message), "%d %s", protocol_type, file_name);
    if (sendto(s, request_message, sizeof(request_message), 0, (struct sockaddr*)&send_addr, sizeof(send_addr)) == -1) {
        print_error("Client: Failed to send request to server");
    }

    // Create a new file name by appending "_received" to the original filename
    char new_file_name[100];
    snprintf(new_file_name, sizeof(new_file_name), "%s_received", file_name);

    // Optionally: Add a timestamp to the file name for uniqueness
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    snprintf(new_file_name, sizeof(new_file_name), "%s_%d-%02d-%02d_%02d-%02d-%02d", 
             file_name, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, 
             tm.tm_hour, tm.tm_min, tm.tm_sec);

    // Open the new file for writing
    fp = fopen(new_file_name, "wb");  // "wb" for binary write mode
    if (fp == NULL) {
        print_error("Client: Failed to open new file");
    }

    // ARQ - Stop-and-Wait Protocol
    if (protocol_type == 1) {
        long int total_frame = 0, bytes_received = 0;
        int i = 0;

        // Receive the total number of frames from the server
        t_out.tv_sec = 2; // Set timeout for receiving
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&t_out, sizeof(struct timeval));
        if (recvfrom(s, &total_frame, sizeof(total_frame), 0, (struct sockaddr*)&from_addr, (socklen_t*)&length) == -1) {
            print_error("Client: Failed to receive total frames");
        }

        printf("Total frames to receive: %ld\n", total_frame);

        // Loop to receive each frame
        for (i = 1; i <= total_frame; i++) {
            memset(&frame, 0, sizeof(frame));

            // Receive a frame
            if (recvfrom(s, &frame, sizeof(frame), 0, (struct sockaddr*)&from_addr, (socklen_t*)&length) == -1) {
                print_error("Client: Error receiving frame");
            }

            // Send ACK for the received frame
            if (sendto(s, &frame.ID, sizeof(frame.ID), 0, (struct sockaddr*)&send_addr, sizeof(send_addr)) == -1) {
                print_error("Client: Failed to send ACK");
            }

            // Only process the correct frame
            if (frame.ID == i) {
                fwrite(frame.data, 1, frame.length, fp);
                printf("Received Frame #%ld of length %ld\n", frame.ID, frame.length);
                bytes_received += frame.length;
            } else {
                i--; // If the frame ID is incorrect, re-request it
            }
        }

        printf("File received successfully and saved as %s. Total bytes received: %ld\n", new_file_name, bytes_received);
        fclose(fp);
    }

    // ARQ - Go-Back-N Protocol

    close(s); // Close the socket
    return 0;
}
