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

#define BUF_SIZE 4096   // Maximum buffer size for data in a frame
#define SERVER_PORT 2226 // Fixed server port number
long int total_frame = 0; // Total number of frames to receive

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

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Client: Usage: ./[%s] Hostname \n", argv[0]);
        exit(EXIT_FAILURE); // Exit if hostname not provided
    }

    struct sockaddr_in send_addr, from_addr; // Socket address structures for server and client
    struct frame_packet frame;                 // Structure for receiving packets
    struct timeval t_out = { 10, 0 };           // 10-second timeout for receiving data
    struct hostent* h;                         // Host information structure
    char protocolType_send[50];                // Buffer for user input regarding protocol, file name, and drop percentage
    char file_name[20];                        // Buffer for file name to receive
    char protocolType[10];                     // Protocol type (1 for Stop-and-Wait, 2 for Go-Back-N)
    char percent[10];                          // Drop percentage for packet simulation
    char ack_send[4] = "ACK";                  // Acknowledgment message to send to server
    int ack_num;                                // ACK number received from server
    int expected_ack = 1;                       // Expected ACK number from server
    long int* acked;                            // Dynamic array to track acknowledged frames
    socklen_t length = sizeof(from_addr); // Use socklen_t for recvfrom
    int s;                                     // Socket descriptor
    FILE* fp_input;                            // File pointer for input file
    FILE* fp_output;                           // File pointer for received file
    long int i = 0;                            // Frame counter

    // Initialize buffers to zero
    memset(&send_addr, 0, sizeof(send_addr));
    memset(&from_addr, 0, sizeof(from_addr));

    h = gethostbyname(argv[1]); //Get the hostname from argument
    if (!h) {
        print_error("gethostbyname failed"); // Handle DNS lookup failure
    }

    // Fill server address structure with IP address and port number
    send_addr.sin_family = AF_INET;
    send_addr.sin_port = htons(SERVER_PORT); 
    memcpy(&send_addr.sin_addr.s_addr, h->h_addr, h->h_length); 

    // Create UDP socket
    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        print_error("Client: Socket"); 
    }

    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&t_out, sizeof(struct timeval)); //Sets option specified by SO_RCVTIMEO at level SOL_SOCKET to value of t_out 

    for (;;) { 
        //Initializing buffers to 0
        memset(protocolType_send, 0, sizeof(protocolType_send));
        memset(protocolType, 0, sizeof(protocolType));
        memset(file_name, 0, sizeof(file_name));

        //Prompt the user for input
        printf("\n -----");
        printf("\n Menu");
        printf("\n -----");
        printf("\n Enter the protocol number followed by file name and drop percentage:");
        printf("\n ------------------------------------------------");
        printf("\n For Stop-and-Wait enter [1]: \n Example: 1 [File Name] [percentage]\n");
        printf("\n For Go-Back-N enter [2]: \n Example: 2 [File Name] [percentage]\n");
        printf("\n To exit enter [exit]: \n");
        printf("\n ------------------------------------------------");
        printf("\n INPUT: ");
        scanf(" %[^\n]%*c", protocolType_send); 

        //Exit command to stop server
        if (strcmp(protocolType_send, "exit") == 0) {
            printf("Sending exit command to server...\n");
            sendto(s, "EXIT", sizeof("EXIT"), 0, (struct sockaddr*)&send_addr, sizeof(send_addr));
            break; 
        }

        //Make sure the arguments are correct
        if (sscanf(protocolType_send, "%s %s %s", protocolType, file_name, percent) != 3) {
            fprintf(stderr, "Failed to parse input correctly\n");
            continue; 
        }

        //Make sure the client can send properly
        if (sendto(s, protocolType_send, sizeof(protocolType_send), 0, (struct sockaddr*)&send_addr, sizeof(send_addr)) == -1) {
            print_error("Client: Send"); 
        }

         // Stop-and-Wait Protocol
        if (strcmp(protocolType, "1") == 0 && file_name[0] != '\0') { // Check argument for Stop-and-Wait
            long int i = 1; // Frame counter starts at 1, since server sends frame# 1 first
            socklen_t length = sizeof(from_addr);  // Changed to socklen_t

            // Receive total number of frames from server
            if (recvfrom(s, &total_frame, sizeof(total_frame), 0, (struct sockaddr*)&from_addr, &length) == -1) {
                perror("Client: Receive total frame count");
                exit(EXIT_FAILURE);
            }

            if (total_frame > 0) { // Check if valid total frame count received
                printf("\nSERVER: Total number of frames to be transmitted: %ld frames\n", total_frame);
                fp_output = fopen("received_file_sw.txt", "wb"); // Open new file for writing received data
                if (fp_output == NULL) {
                    perror("Error opening output file");
                    exit(EXIT_FAILURE);
                }

                printf("Expecting to receive %ld total frames\n", total_frame); // Debug

                int retry_limit = 5;
                int retry_count = 0;

                // Loop to receive all frames
                while (i <= total_frame) {
                    // Clear previous frame
                    memset(&frame, 0, sizeof(frame));

                    // Try receiving frame from server
                    if (recvfrom(s, &frame, sizeof(frame), 0, (struct sockaddr*)&from_addr, &length) == -1) {
                        perror("Client: Receive frame failed");
                        retry_count++;

                        if (retry_count >= retry_limit) {
                            printf("Warning: Retry limit reached while receiving frame #%ld. Skipping this frame.\n", i);
                            retry_count = 0;  // Reset retry count and move on (instead of terminating)
                            i++;  // Move to next frame as an error recovery strategy
                            continue;
                        }

                        // Timeout occurred, resend previous ACK
                        long ack_num = i - 1;  // Resend the last ACK if timeout occurs
                        if (sendto(s, &ack_num, sizeof(ack_num), 0, (struct sockaddr*)&send_addr, sizeof(send_addr)) == -1) {
                            perror("Client: Resend last ACK failed");
                        } else {
                            printf("Resent last ACK for frame #%ld due to timeout\n", ack_num);
                        }
                        continue; // Retry receiving frame
                    }

                    // Frame successfully received, reset retry count
                    retry_count = 0;

                    printf("Received frame #%ld\n", frame.ID);

                    // If the frame ID is what we expect
                    if (frame.ID == i) {
                        printf("Preparing to send ACK for frame ID: %ld\n", frame.ID); // Debug

                        // Send ACK to the server after receiving the correct frame
                        long ack_num = frame.ID; // Correct ACK for received frame
                        if (sendto(s, &ack_num, sizeof(ack_num), 0, (struct sockaddr*)&send_addr, sizeof(send_addr)) == -1) {
                            perror("Client: Send ACK failed");
                        } else {
                            printf("Sent ACK for frame #%ld\n", ack_num);
                        }

                        // Write received data to the output file
                        fwrite(frame.data, 1, frame.length, fp_output);
                        printf("Writing %ld bytes of data for frame ID: %ld\n", frame.length, frame.ID); // Debug

                        // Move to the next frame
                        i++;
                    } else {
                        // If we received an out-of-order frame, resend the last correct ACK
                        long ack_num = i - 1;  // Last successfully received frame
                        printf("Out of order frame received, resending ACK for #%ld\n", ack_num);
                        if (sendto(s, &ack_num, sizeof(ack_num), 0, (struct sockaddr*)&send_addr, sizeof(send_addr)) == -1) {
                            perror("Client: Resend ACK for out-of-order frame failed");
                        }
                    }
                }

                // Close output file
                fclose(fp_output);
                printf("Transmission Completed for Stop-and-Wait!\n");
            } else {
                printf("File is empty or invalid.\n"); // Handle case of empty or invalid file
            }
        }

        // Go-Back-N Protocol
        // Go-Back-N Protocol (Client-Side)
if (strcmp(protocolType, "2") == 0 && file_name[0] != '\0') {
    long int base = 1; // Base of the expected frames
    socklen_t length = sizeof(from_addr); // Set length for recvfrom()

    // Receive total number of frames from server
    if (recvfrom(s, &total_frame, sizeof(total_frame), 0, (struct sockaddr*)&from_addr, &length) == -1) {
        perror("Client: Receive total frame count failed");
        exit(EXIT_FAILURE);
    }

    if (total_frame > 0) {
        printf("\nSERVER: Total number of frames to be transmitted: %ld frames\n", total_frame);
        fp_output = fopen("received_file_gbn.txt", "wb");
        if (fp_output == NULL) {
            perror("Error opening output file");
            exit(EXIT_FAILURE);
        }

        printf("Expecting to receive %ld total frames\n", total_frame);

        int retry_count = 0;
        int retry_limit = 5; // Set a retry limit for resending ACKs

        while (base <= total_frame) {
            memset(&frame, 0, sizeof(frame));

            // Receive frame from server
            if (recvfrom(s, &frame, sizeof(frame), 0, (struct sockaddr*)&from_addr, &length) == -1) {
                perror("Client: Receive frame failed");

                // Timeout occurred, resend ACK for the last successfully received frame
                retry_count++;
                if (retry_count >= retry_limit) {
                    printf("Retry limit reached, giving up on frame #%ld\n", base);
                    break; // Exit loop if retry limit is reached
                }

                long ack_num = base - 1;  // Send ACK for the last correctly received frame
                if (sendto(s, &ack_num, sizeof(ack_num), 0, (struct sockaddr*)&send_addr, sizeof(send_addr)) == -1) {
                    perror("Client: Resend ACK failed");
                } else {
                    printf("Resent last ACK for frame #%ld due to timeout\n", ack_num);
                }
                continue; // Retry receiving the next frame
            }

            printf("Received frame #%ld\n", frame.ID);
            retry_count = 0;  // Reset retry count when a frame is received

            // If the frame ID is what we expect
            if (frame.ID == base) {
                // Send ACK for the received frame
                long ack_num = frame.ID;
                if (sendto(s, &ack_num, sizeof(ack_num), 0, (struct sockaddr*)&send_addr, sizeof(send_addr)) == -1) {
                    perror("Client: Send ACK failed");
                } else {
                    printf("Sent ACK for frame #%ld\n", ack_num);
                }

                // Write received data to the output file
                fwrite(frame.data, 1, frame.length, fp_output);
                printf("Writing %ld bytes of data for frame ID: %ld\n", frame.length, frame.ID);

                // Move to the next expected frame
                base++;
            } else {
                // If we received an out-of-order frame, resend the last correct ACK
                long ack_num = base - 1;  // Last successfully received frame
                printf("Out of order");
            }
        }
    }

    close(s);
    exit(EXIT_SUCCESS);
}