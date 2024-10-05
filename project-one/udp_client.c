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
    // Check for correct number of arguments (hostname expected)
    if (argc != 2) {
        printf("Client: Usage: ./[%s] Hostname \n", argv[0]);
        exit(EXIT_FAILURE); // Exit if hostname not provided
    }

    struct sockaddr_in send_addr, from_addr; // Socket address structures for server and client
    struct frame_packet frame;                 // Structure for receiving packets
    struct timeval t_out = { 2, 0 };           // 2-second timeout for receiving data (consistent throughout)
    struct hostent* h;                         // Host information structure
    char protocolType_send[50];                // Buffer for user input regarding protocol, file name, and drop percentage
    char file_name[20];                        // Buffer for file name to receive
    char protocolType[10];                     // Protocol type (1 for Stop-and-Wait, 2 for Go-Back-N)
    char percent[10];                          // Drop percentage for packet simulation
    char ack_send[4] = "ACK";                  // Acknowledgment message to send to server
    int ack_num;                                // Declare ack_num here
    int expected_ack = 1;                       // Expected ACK number from server
    long int* acked;                            // Dynamic array to track acknowledged frames
    ssize_t length = 0;                        // Length of received data
    int s;                                     // Socket descriptor
    FILE* fp_input;                            // File pointer for input file
    FILE* fp_output;                           // File pointer for received file
    long int total_frame = 0;                  // Total number of frames to receive
    long int i = 0;                            // Frame counter

    // Initialize buffers to zero
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

    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&t_out, sizeof(struct timeval)); // Set consistent timeout

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
        printf("\n To exit enter [exit]: \n");
        printf("\n ------------------------------------------------");
        printf("\n INPUT: ");
        scanf(" %[^\n]%*c", protocolType_send); // Read user input

        // Check if user wants to exit
        if (strcmp(protocolType_send, "exit") == 0) {
            printf("Sending exit command to server...\n");
            sendto(s, "EXIT", sizeof("EXIT"), 0, (struct sockaddr*)&send_addr, sizeof(send_addr));
            break; // Exit loop
        }

        if (sscanf(protocolType_send, "%s %s %s", protocolType, file_name, percent) != 3) {
            fprintf(stderr, "Failed to parse input correctly\n");
            continue; // Skip this iteration if input is invalid
        }

        // Send protocol type and file name to server
        if (sendto(s, protocolType_send, sizeof(protocolType_send), 0, (struct sockaddr*)&send_addr, sizeof(send_addr)) == -1) {
            print_error("Client: Send"); // Handle send failure
        }

        // Stop-and-Wait Protocol
        if (strcmp(protocolType, "1") == 0 && file_name[0] != '\0') {
            long int total_frame = 0; // Total number of frames to receive
            long int i = 0; // Frame counter
            // Set timeout for receiving frames
            t_out.tv_sec = 2; // 2 seconds timeout
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&t_out, sizeof(struct timeval)); // Apply timeout to socket
            // Receive total number of frames from server
            if (recvfrom(s, &total_frame, sizeof(total_frame), 0, (struct sockaddr*)&from_addr, (socklen_t*)&length) == -1) {
                perror("Client: Receive total frame count");
                exit(EXIT_FAILURE);
            }
            // Reset timeout to zero for further receives
            t_out.tv_sec = 0;
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&t_out, sizeof(struct timeval));
            if (total_frame > 0) { // Check if valid total frame count received
                printf("\n SERVER: Total number of frames to be transmitted: %ld frames\n", total_frame);
                fp_output = fopen("received_file_sw.txt", "wb"); // Open new file for writing received data
                if (fp_output == NULL) {
                    perror("Error opening output file");
                    exit(EXIT_FAILURE);
                }
                // Open input file for reading
                fp_input = fopen(file_name, "rb");
                if (fp_input == NULL) {
                    perror("Error opening input file");
                    exit(EXIT_FAILURE);
                }
                // Read from input file and send to server
                while (i < total_frame) {
                    // Read data from input file
                    size_t bytes_read = fread(frame.data, 1, BUF_SIZE, fp_input);
                    if (bytes_read == 0) {
                        if (feof(fp_input)) {
                            break; // End of file reached
                        } else {
                            perror("Error reading input file");
                            break; // An error occurred
                        }
                    }
                    // Prepare frame for sending
                    frame.ID = i + 1; // Sequence number
                    frame.length = bytes_read; // Length of data read
                    // Send frame to server
                    if (sendto(s, &frame, sizeof(frame.ID) + frame.length, 0, (struct sockaddr*)&send_addr, sizeof(send_addr)) == -1) {
                        print_error("Client: Send"); // Handle send failure
                    }
                    // After receiving the acknowledgment from the server
                    if (recvfrom(s, &frame.ID, sizeof(frame.ID), 0, (struct sockaddr*)&from_addr, (socklen_t*)&length) == -1) {
                        perror("Client: Receive ACK");
                        // Optionally handle retransmission logic here
                    } else {
                        // Print the received ACK for debugging purposes
                        printf("Debug: Sent ACK for frame #%ld\n", frame.ID); // <-- Add this line

                        printf("Sent frame #%ld, received ACK for frame #%ld\n", frame.ID, frame.ID);
                        // Write received data to output file
                        fwrite(frame.data, 1, frame.length, fp_output); // Write to output file
                    }

                    i++; // Increment frame counter
                }
                // Close input file after transmission
                fclose(fp_input);
                fclose(fp_output); // Close output file
                // Completion message after all frames are received
                printf("Transmission Completed for Stop-and-Wait!\n");
            } else {
                printf("File is empty or invalid.\n"); // Handle case of empty or invalid file
            }
        }

        // Go-Back-N Protocol
        if (strcmp(protocolType, "2") == 0 && file_name[0] != '\0') {
            // Receive total number of frames from server
            if (recvfrom(s, &total_frame, sizeof(total_frame), 0, (struct sockaddr*)&from_addr, (socklen_t*)&length) == -1) {
                perror("Client: Receive total frame count");
                exit(EXIT_FAILURE);
            }

            if (total_frame > 0) {
                printf("\n SERVER: Total number of frames to be transmitted: %ld frames\n", total_frame);
                acked = (long int*)calloc(total_frame, sizeof(long int)); // Dynamically allocate ack array

                fp_output = fopen("received_file_gbn.txt", "wb");
                if (fp_output == NULL) {
                    perror("Error opening output file");
                    exit(EXIT_FAILURE);
                }

                // Set up input file reading (optional, if needed for future improvements)
                fp_input = fopen(file_name, "rb");
                if (fp_input == NULL) {
                    perror("Error opening input file");
                    exit(EXIT_FAILURE);
                }

                while (i < total_frame) {
                    // Send frame to server
                    if (sendto(s, &frame, sizeof(frame), 0, (struct sockaddr*)&send_addr, sizeof(send_addr)) == -1) {
                        print_error("Client: Send");
                    }
                    printf("Sent frame #%ld\n", frame.ID);

                    // Receive acknowledgment from server
                    if (recvfrom(s, &ack_num, sizeof(ack_num), 0, (struct sockaddr*)&from_addr, (socklen_t*)&length) == -1) {
                        perror("Client: Receive ACK");
                        i = expected_ack - 1; // Resend from expected ACK
                        continue;
                    }

                    // Process ACK and adjust the window
                    if (ack_num >= expected_ack && ack_num <= i + 1) {
                        for (int j = expected_ack; j <= ack_num; ++j) {
                            acked[j - 1] = 1; // Mark as acknowledged
                        }
                        expected_ack = ack_num + 1;
                        i = expected_ack - 1; // Slide the window
                    }

                    // Write received data to output file
                    fwrite(frame.data, 1, frame.length, fp_output);
                    i++;
                }

                fclose(fp_output); // Close output file
                free(acked); // Free dynamically allocated memory
                printf("Transmission Completed for Go-Back-N!\n");
            } else {
                printf("File is empty or invalid.\n");
            }
        }
    }

    close(s); // Close socket
    exit(EXIT_SUCCESS); // Exit program successfully
}
