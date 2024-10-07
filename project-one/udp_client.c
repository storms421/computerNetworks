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
    if (argc != 2) {
        printf("Client: Usage: ./[%s] Hostname \n", argv[0]);
        exit(EXIT_FAILURE); // Exit if hostname not provided
    }

    struct sockaddr_in send_addr, from_addr; // Socket address structures for server and client
    struct frame_packet frame;                 // Structure for receiving packets
    struct timeval t_out = { 2, 0 };           // 2-second timeout for receiving data
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
    long int total_frame = 0;                  // Total number of frames to receive
    long int i = 0;                            // Frame counter

    // Initialize buffers to zero
    memset(&send_addr, 0, sizeof(send_addr));
    memset(&from_addr, 0, sizeof(from_addr));

    h = gethostbyname(argv[1]);
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

    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&t_out, sizeof(struct timeval)); 

    for (;;) { 
        memset(protocolType_send, 0, sizeof(protocolType_send));
        memset(protocolType, 0, sizeof(protocolType));
        memset(file_name, 0, sizeof(file_name));

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

        if (strcmp(protocolType_send, "exit") == 0) {
            printf("Sending exit command to server...\n");
            sendto(s, "EXIT", sizeof("EXIT"), 0, (struct sockaddr*)&send_addr, sizeof(send_addr));
            break; 
        }

        if (sscanf(protocolType_send, "%s %s %s", protocolType, file_name, percent) != 3) {
            fprintf(stderr, "Failed to parse input correctly\n");
            continue; 
        }

        if (sendto(s, protocolType_send, sizeof(protocolType_send), 0, (struct sockaddr*)&send_addr, sizeof(send_addr)) == -1) {
            print_error("Client: Send"); 
        }

        // Stop-and-Wait Protocol
        if (strcmp(protocolType, "1") == 0 && file_name[0] != '\0') {
            long int total_frame = 0;
            long int i = 0;
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&t_out, sizeof(struct timeval)); 

            if (recvfrom(s, &total_frame, sizeof(total_frame), 0, (struct sockaddr*)&from_addr, (socklen_t*)&length) == -1) {
                perror("Client: Receive total frame count");
                exit(EXIT_FAILURE);
            }
            t_out.tv_sec = 0;
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&t_out, sizeof(struct timeval));

            if (total_frame > 0) {
                printf("\n SERVER: Total number of frames to be transmitted: %ld frames\n", total_frame);
                fp_output = fopen("received_file_sw.txt", "wb");
                if (fp_output == NULL) {
                    perror("Error opening output file");
                    exit(EXIT_FAILURE);
                }
                fp_input = fopen(file_name, "rb");
                if (fp_input == NULL) {
                    perror("Error opening input file");
                    exit(EXIT_FAILURE);
                }

                while (i < total_frame) {
                    size_t bytes_read = fread(frame.data, 1, BUF_SIZE, fp_input);
                    if (bytes_read == 0) {
                        if (feof(fp_input)) {
                            break;
                        } else {
                            perror("Error reading input file");
                            break;
                        }
                    }
                    frame.ID = i;
                    frame.length = bytes_read;

                    // Resend until ACK is received
                    while (1) {
                        if (sendto(s, &frame, sizeof(frame.ID) + frame.length, 0, (struct sockaddr*)&send_addr, sizeof(send_addr)) == -1) {
                            print_error("Client: Send");
                        }

                        // Wait for ACK
                        if (recvfrom(s, &ack_num, sizeof(ack_num), 0, (struct sockaddr*)&from_addr, &length) == -1) {
                            printf("Timeout! Resending frame #%ld\n", frame.ID);
                        } else {
                            if (ack_num == frame.ID) {
                                printf("Received ACK for frame #%ld\n", frame.ID);
                                break; // Break if correct ACK received
                            }
                        }
                    }

                    fwrite(frame.data, 1, frame.length, fp_output);
                    i++;
                }
                fclose(fp_input);
                fclose(fp_output);
                printf("Transmission Completed for Stop-and-Wait!\n");
            } else {
                printf("File is empty or invalid.\n");
            }
        }

        // Go-Back-N Protocol
        if (strcmp(protocolType, "2") == 0 && file_name[0] != '\0') {
            int window_size = 4;  // Define window size (you can adjust this as necessary)
            long int base = 0;    // Base of the window (frames sent but not acknowledged)
            long int next_seq_num = 0;  // Next sequence number to be sent
            int max_seq_num = total_frame;  // Max sequence number (total frames to be received)
            long int ack_num = 0;  // ACK received from the server

            acked = (long int*)calloc(total_frame, sizeof(long int));  // Array to track acknowledged frames
            if (acked == NULL) {
                perror("Error allocating memory for ACK tracking");
                exit(EXIT_FAILURE);
            }

            fp_output = fopen("received_file_gbn.txt", "wb");
            if (fp_output == NULL) {
                perror("Error opening output file");
                exit(EXIT_FAILURE);
            }

            // Open input file for sending frames
            fp_input = fopen(file_name, "rb");
            if (fp_input == NULL) {
                perror("Error opening input file");
                exit(EXIT_FAILURE);
            }

            // GBN protocol loop
            while (base < total_frame) {
                // Send frames within the window
                while (next_seq_num < base + window_size && next_seq_num < total_frame) {
                    // Prepare frame for sending
                    frame.ID = next_seq_num;
                    frame.length = fread(frame.data, 1, BUF_SIZE, fp_input);  // Read data from input file

                    // Send frame to server
                    if (sendto(s, &frame, sizeof(frame.ID) + frame.length, 0, (struct sockaddr*)&send_addr, sizeof(send_addr)) == -1) {
                        print_error("Client: Send frame");
                    }
                    printf("Sent frame #%ld\n", frame.ID);

                    next_seq_num++;  // Move to the next frame to send
                }

                // Wait for ACK from the server
                if (recvfrom(s, &ack_num, sizeof(ack_num), 0, (struct sockaddr*)&from_addr, (socklen_t*)&length) == -1) {
                    perror("Client: Receive ACK");
                    // Timeout or error, resend unacknowledged frames from base
                    printf("Timeout occurred, resending frames from base #%ld\n", base);
                    next_seq_num = base;  // Go back to base to resend
                } else {
                    printf("Received ACK for frame #%ld\n", ack_num);
                    if (ack_num >= base) {
                        // Slide window forward
                        base = ack_num + 1;
                    }
                }

                // Write received data to output file
                fwrite(frame.data, 1, frame.length, fp_output);
            }

            fclose(fp_input);  // Close input file after all frames are sent
            fclose(fp_output); // Close output file after all frames are received
            free(acked);  // Free dynamically allocated memory
            printf("Transmission Completed for Go-Back-N!\n");
        }
    }

    close(s);
    exit(EXIT_SUCCESS);
}
