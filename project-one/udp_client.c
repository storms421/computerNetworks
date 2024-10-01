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

#define BUF_SIZE 2048   // Maximum buffer size for data in a frame
#define SERVER_PORT 2226 // Fixed server port number

// Structure for data packets
struct frame_packet {
    long int ID;          // Frame identifier (sequence number)
    long int length;      // Length of data in frame
    char data[BUF_SIZE];  // Actual data content
};

// Function to handle errors
static void print_error(char* msg) {
    perror(msg);  // Print error message
    exit(EXIT_FAILURE);  // Exit program with failure status
}

// Main client function
int main(int argc, char** argv) {
    // Check for correct number of arguments (hostname expected)
    if (argc != 2) {
        printf("Client: Usage: ./[%s] Hostname \n", argv[0]);
        exit(EXIT_FAILURE); // Exit if hostname not provided
    }

    struct sockaddr_in send_addr, from_addr; // Socket address structures for server and client
    struct frame_packet frame;                 // Structure for receiving packets
    struct timeval t_out = { 0, 0 };           // Timeout structure for receiving data
    struct hostent* h;                         // Host information structure
    char protocolType_send[50];                // Buffer for user input regarding protocol, file name, and drop percentage
    char file_name[20];                        // Buffer for file name to receive
    char protocolType[10];                     // Protocol type (1 for Stop-and-Wait, 2 for Go-Back-N)
    char percent[10];                          // Drop percentage for packet simulation
    char ack_send[4] = "ACK";                  // Acknowledgment message to send to server

    ssize_t length = 0;                        // Length of received data
    int s;                                     // Socket descriptor
    FILE* fp;                                  // File pointer for received file

    // Initialize buffers to zero
    memset(ack_send, 0, sizeof(ack_send)); // Clear acknowledgment buffer
    memset(&send_addr, 0, sizeof(send_addr)); // Clear send address structure
    memset(&from_addr, 0, sizeof(from_addr)); // Clear from address structure

    // Perform DNS lookup for hostname provided by user
    h = gethostbyname(argv[1]);
    if (!h) {
        print_error("gethostbyname failed"); // Handle DNS lookup failure
    }

    // Fill server address structure with IP address and port number
    send_addr.sin_family = AF_INET; // Use IPv4
    send_addr.sin_port = htons(SERVER_PORT); // Set server port
    memcpy(&send_addr.sin_addr.s_addr, h->h_addr, h->h_length); // Copy IP address into structure

    // Create UDP socket
    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        print_error("Client: Socket"); // Handle socket creation failure
    }

    for (;;) { // Infinite loop for continuous operation
        // Clear previous input buffers
        memset(protocolType_send, 0, sizeof(protocolType_send));
        memset(protocolType, 0, sizeof(protocolType));
        memset(file_name, 0, sizeof(file_name));

        // Display menu and ask user for protocol, file name, and drop percentage
        printf("\n -----");
        printf("\n Menu");
        printf("\n -----");
        printf("\n Enter the protocol number followed by file name and drop percentage:");
        printf("\n ------------------------------------------------");
        printf("\n For Stop-and-Wait enter [1]: \n Example: 1 [File Name] [percentage]\n");
        printf("\n For Go-Back-N enter [2]: \n Example: 2 [File Name] [percentage]\n");
        printf("\n ------------------------------------------------");
        printf("\n INPUT: ");
        scanf(" %[^\n]%*c", protocolType_send); // Read user input

        // Parse user input into protocol type, file name, and drop percentage
        sscanf(protocolType_send, "%s %s %s", protocolType, file_name, percent);

        // Send protocol type and file name to server
        if (sendto(s, protocolType_send, sizeof(protocolType_send), 0, (struct sockaddr*)&send_addr, sizeof(send_addr)) == -1) {
            print_error("Client: Send"); // Handle send failure
        }

        // Stop-and-Wait Protocol
        if (strcmp(protocolType, "1") == 0 && file_name[0] != '\0') {
            long int total_frame = 0; // Total number of frames to receive
            long int bytes_rec = 0, i = 0; // Total bytes received and counter for frames

            // Set timeout for receiving frames
            t_out.tv_sec = 2; // 2 seconds timeout
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&t_out, sizeof(struct timeval)); // Apply timeout to socket

            // Receive total number of frames from server
            recvfrom(s, &total_frame, sizeof(total_frame), 0, (struct sockaddr*)&from_addr, (socklen_t*)&length);

            // Reset timeout to zero for further receives
            t_out.tv_sec = 0;
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&t_out, sizeof(struct timeval));

            if (total_frame > 0) { // Check if valid total frame count received
                printf("\n SERVER: Total number of frames to be transmitted: %ld frames\n", total_frame);
                fp = fopen(file_name, "wb"); // Open file for writing received data

                // Receive frames one by one
                for (i = 1; i <= total_frame; i++) {
                    memset(&frame, 0, sizeof(frame)); // Clear frame structure before receiving
                    recvfrom(s, &frame, sizeof(frame), 0, (struct sockaddr*)&from_addr, (socklen_t*)&length); // Receive frame

                    // Send acknowledgment back to server
                    sendto(s, &frame.ID, sizeof(frame.ID), 0, (struct sockaddr*)&send_addr, sizeof(send_addr));

                    // Check for dropped repeated frames
                    if (frame.ID != i) { // If received frame ID doesn't match expected ID
                        i--; // Stay on same frame index to receive again
                    } else {
                        fwrite(frame.data, 1, frame.length, fp); // Write received data to file
                        printf("Frame Number # %ld \t Frame Length -> %ld\n", frame.ID, frame.length); // Display received frame info
                        bytes_rec += frame.length; // Update total bytes received
                    }
                }

                // Completion message after all frames are received
                printf("Transmission Completed!\n");
                printf("Total bytes transmitted -> %ld\n", bytes_rec);
                fclose(fp); // Close file after writing
            } else {
                printf("File is empty or invalid.\n"); // Handle case of empty or invalid file
            }
        }

        // Go-Back-N Protocol
        if (strcmp(protocolType, "2") == 0 && file_name[0] != '\0') {
            long int total_frame = 0; // Total number of frames to receive
            long int bytes_rec = 0, i = 0; // Total bytes received and counter for frames

            // Set timeout for receiving frames
            t_out.tv_sec = 2; // 2 seconds timeout
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&t_out, sizeof(struct timeval)); // Apply timeout to socket

            // Receive total number of frames from server
            recvfrom(s, &total_frame, sizeof(total_frame), 0, (struct sockaddr*)&from_addr, (socklen_t*)&length);

            // Reset timeout to zero for further receives
            t_out.tv_sec = 0;
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&t_out, sizeof(struct timeval));

            if (total_frame > 0) { // Check if valid total frame count received
                printf("\n SERVER: Total number of frames to be transmitted: %ld frames\n", total_frame);
                fp = fopen(file_name, "wb"); // Open file for writing received data

                // Receive frames in Go-Back-N manner
                for (i = 1; i <= total_frame; i++) {
                    memset(&frame, 0, sizeof(frame)); // Clear frame structure before receiving
                    recvfrom(s, &frame, sizeof(frame), 0, (struct sockaddr*)&from_addr, (socklen_t*)&length); // Receive frame

                    // Send acknowledgment back to server
                    sendto(s, &frame.ID, sizeof(frame.ID), 0, (struct sockaddr*)&send_addr, sizeof(send_addr));

                    // Check for dropped repeated frames
                    if (frame.ID != i) { // If received frame ID doesn't match expected ID
                        i--; // Stay on same frame index to receive again
                    } else {
                        fwrite(frame.data, 1, frame.length, fp); // Write received data to file
                        printf("Frame Number # %ld \t Frame Length -> %ld\n", frame.ID, frame.length); // Display received frame info
                        bytes_rec += frame.length; // Update total bytes received
                    }
                }

                // Completion message after all frames are received
                printf("Transmission Completed!\n");
                printf("Total bytes transmitted -> %ld\n", bytes_rec);
                fclose(fp); // Close file after writing
            } else {
                printf("File is empty or invalid.\n"); // Handle case of empty or invalid file
            }
        }
    }

    close(s); // Close socket
    exit(EXIT_SUCCESS); // Exit program successfully
}
