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
#include <time.h>

#define BUF_SIZE 4096  // Maximum buffer size for data in a frame
#define SERVER_PORT 2226 // Fixed server port number

// Structure for data packets
struct frame_packet {
    long int ID;         // Frame identifier (sequence number)
    long int length;     // Length of data in the frame
    char data[BUF_SIZE]; // Actual data content
};

// Function to handle errors
static void print_error(char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// Stop-and-Wait ARQ protocol
void stop_and_wait(int s, struct sockaddr_in* send_addr, socklen_t length, const char* file_name) {
    struct frame_packet frame;
    long int total_frame = 0;
    long int i = 1;  // Frame counter
    socklen_t from_len = length;
    int ack_num;
    FILE* fp_output;

    // Receive total number of frames from the server
    if (recvfrom(s, &total_frame, sizeof(total_frame), 0, (struct sockaddr*)send_addr, &from_len) == -1) {
        print_error("Client: Receive total frame count");
    }

    if (total_frame > 0) {
        printf("\nSERVER: Total number of frames to be transmitted: %ld frames\n", total_frame);

        // Open file to write received data
        fp_output = fopen("received_file_sw.txt", "wb");
        if (!fp_output) {
            print_error("Error opening output file");
        }

        while (i <= total_frame) {
            memset(&frame, 0, sizeof(frame)); // Clear frame buffer

            // Try receiving a frame
            if (recvfrom(s, &frame, sizeof(frame), 0, (struct sockaddr*)send_addr, &from_len) == -1) {
                perror("Client: Timeout or error receiving frame");
                // Timeout occurred, resend last ACK
                long last_ack = i - 1;
                if (sendto(s, &last_ack, sizeof(last_ack), 0, (struct sockaddr*)send_addr, length) == -1) {
                    print_error("Client: Resend last ACK");
                }
                continue;
            }

            printf("Received frame #%ld\n", frame.ID);

            // Check if the received frame is in order
            if (frame.ID == i) {
                // Send ACK for the received frame
                if (sendto(s, &frame.ID, sizeof(frame.ID), 0, (struct sockaddr*)send_addr, length) == -1) {
                    print_error("Client: Send ACK");
                }

                fwrite(frame.data, 1, frame.length, fp_output);  // Write frame data to file
                i++;  // Move to the next frame
            } else {
                // If out-of-order frame received, resend last correct ACK
                long last_ack = i - 1;
                printf("Out of order frame received, resending ACK for #%ld\n", last_ack);
                if (sendto(s, &last_ack, sizeof(last_ack), 0, (struct sockaddr*)send_addr, length) == -1) {
                    print_error("Client: Resend last ACK");
                }
            }
        }

        fclose(fp_output);  // Close the file after receiving all frames
        printf("Transmission completed for Stop-and-Wait!\n");
    } else {
        printf("File is empty or invalid.\n");
    }
}

// Go-Back-N ARQ protocol
void go_back_n(int s, struct sockaddr_in* send_addr, socklen_t length, const char* file_name) {
    struct frame_packet frame;
    int window_size = 4;  // Define window size
    long int base = 0;    // Base of the window
    long int next_seq_num = 0;  // Next sequence number to be sent
    long int total_frame = 0;
    long int ack_num = 0;
    FILE* fp_output;

    // Receive total number of frames from the server
    if (recvfrom(s, &total_frame, sizeof(total_frame), 0, (struct sockaddr*)send_addr, &length) == -1) {
        print_error("Client: Receive total frame count");
    }

    if (total_frame > 0) {
        printf("\nSERVER: Total number of frames to be transmitted: %ld frames\n", total_frame);

        // Open file to write received data
        fp_output = fopen("received_file_gbn.txt", "wb");
        if (!fp_output) {
            print_error("Error opening output file");
        }

        while (base < total_frame) {
            // Send frames within the window
            while (next_seq_num < base + window_size && next_seq_num < total_frame) {
                memset(&frame, 0, sizeof(frame));

                // Receive frame from the server
                if (recvfrom(s, &frame, sizeof(frame), 0, (struct sockaddr*)send_addr, &length) == -1) {
                    perror("Client: Error or timeout receiving frame");
                    printf("Resending frames from base #%ld\n", base);
                    next_seq_num = base;  // Reset to base for resending
                    break;
                }

                printf("Received frame #%ld\n", frame.ID);

                // Send ACK for the frame
                if (sendto(s, &frame.ID, sizeof(frame.ID), 0, (struct sockaddr*)send_addr, length) == -1) {
                    print_error("Client: Send ACK");
                }

                // Move the base of the window forward if ACK is valid
                if (frame.ID >= base) {
                    fwrite(frame.data, 1, frame.length, fp_output);  // Write data to file
                    base = frame.ID + 1;  // Slide the window
                }
            }
        }

        fclose(fp_output);  // Close the file after receiving all frames
        printf("Transmission completed for Go-Back-N!\n");
    } else {
        printf("File is empty or invalid.\n");
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Client: Usage: %s <server_hostname>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in send_addr;
    socklen_t length = sizeof(send_addr);
    struct hostent* h;
    char protocolType_send[50];  // Buffer for protocol input
    char protocolType[10], file_name[20], percent[10];
    int s;

    memset(&send_addr, 0, sizeof(send_addr));

    // Get server host info
    h = gethostbyname(argv[1]);
    if (!h) {
        print_error("gethostbyname failed");
    }

    // Setup server address
    send_addr.sin_family = AF_INET;
    send_addr.sin_port = htons(SERVER_PORT);
    memcpy(&send_addr.sin_addr.s_addr, h->h_addr, h->h_length);

    // Create UDP socket
    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        print_error("Client: Socket creation failed");
    }

    // Set timeout for socket
    struct timeval t_out = {2, 0};  // 2-second timeout
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&t_out, sizeof(t_out));

    for (;;) {
        // Clear buffers
        memset(protocolType_send, 0, sizeof(protocolType_send));
        memset(protocolType, 0, sizeof(protocolType));
        memset(file_name, 0, sizeof(file_name));

        // Display menu
        printf("\n-----\nMenu\n-----");
        printf("\nEnter protocol number, file name, and drop percentage:");
        printf("\n[1] Stop-and-Wait: 1 [file_name] [percentage]");
        printf("\n[2] Go-Back-N: 2 [file_name] [percentage]");
        printf("\nTo exit, type 'exit'\nInput: ");
        scanf(" %[^\n]%*c", protocolType_send);

        if (strcmp(protocolType_send, "exit") == 0) {
            printf("Sending exit command to server...\n");
            sendto(s, "EXIT", sizeof("EXIT"), 0, (struct sockaddr*)&send_addr, length);
            break;
        }

        // Parse user input
        if (sscanf(protocolType_send, "%s %s %s", protocolType, file_name, percent) != 3) {
            printf("Invalid input format. Please try again.\n");
            continue;
        }

        // Send protocol type and file name to the server
        if (sendto(s, protocolType_send, sizeof(protocolType_send), 0, (struct sockaddr*)&send_addr, length) == -1) {
            print_error("Client: Send");
        }

        // Call appropriate protocol function based on user selection
        if (strcmp(protocolType, "1") == 0) {
            stop_and_wait(s, &send_addr, length, file_name);
        } else if (strcmp(protocolType, "2") == 0) {
            go_back_n(s, &send_addr, length, file_name);
        } else {
            printf("Invalid protocol type. Please try again.\n");
        }
    }

    close(s);  // Close the socket
    return 0;
}
