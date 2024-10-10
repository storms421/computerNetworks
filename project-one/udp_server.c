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

#define BUF_SIZE 4096 // Max buffer size for data in a frame
#define SERVER_PORT 2226 // Server's UDP port
#define WINDOW_SIZE 3 // Window size for Go-Back-N ARQ

// Structure for a frame packet /
struct frame_packet {
    long int ID; // Frame sequence number
    long int length; // Length of data in frame
    char data[BUF_SIZE]; // Data in frame
};

//Function prototypes
void print_error(char* msg);
void stop_and_wait(int s, struct sockaddr_in* c_addr, socklen_t length, FILE* fp, int total_frame, int* testdrop, int td);
void go_back_n(int s, struct sockaddr_in* c_addr, socklen_t length, FILE* fp, int total_frame, float drop_probability);
int* generate_drops(int total_frame, float drop_percent); // Generates which frames to drop based on drop percentage

int main(int argc, char** argv) {
    if (argc != 1) { // Ensure correct number of arguments
        printf("Usage: ./[%s]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct stat st; // Structure to get file information (size, etc.)
    struct sockaddr_in s_addr, c_addr; // Server and client socket addresses
    socklen_t length; // Length of sockaddr_in structure
    char msg_recv[BUF_SIZE]; // Buffer to store received message from client
    char file_name_recv[256]; // Increased buffer size for file name
    char protocolType_recv[10]; // Buffer to store protocol type requested (1 = Stop-and-Wait, 2 = Go-Back-N)
    char percent[10]; // Buffer to store drop percentage

    ssize_t numRead; // Number of bytes read from socket
    off_t f_size; // File size of requested file
    int s; // Socket descriptor
    FILE* fp; // File pointer for file being sent

    // Initialize server's address structure
    memset(&s_addr, 0, sizeof(s_addr)); // Clear structure
    s_addr.sin_family = AF_INET; // IPv4 protocol
    s_addr.sin_port = htons(SERVER_PORT); // Set port number (converted to network byte order)
    s_addr.sin_addr.s_addr = INADDR_ANY; // Accept connections from any address

    // Create a socket using UDP (SOCK_DGRAM)
    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        print_error("Server: Socket"); // Print error if socket creation fails
    }

    // Bind socket to server address
    if (bind(s, (struct sockaddr*)&s_addr, sizeof(s_addr)) == -1) {
        print_error("Server: Bind"); // Print error if binding fails
    }

    // Main server loop
    for (;;) {
        printf("Server: Waiting for client to connect\n");

        // Clear buffers before receiving a message
        memset(msg_recv, 0, sizeof(msg_recv));
        memset(protocolType_recv, 0, sizeof(protocolType_recv));
        memset(file_name_recv, 0, sizeof(file_name_recv));
        memset(percent, 0, sizeof(percent));
        length = sizeof(c_addr); // Length of client address

        // Receive request message from client (no timeout)
        if ((numRead = recvfrom(s, msg_recv, BUF_SIZE, 0, (struct sockaddr*)&c_addr, &length)) == -1) {
            perror("Server: Failed to receive from client");
            continue;  // Keep waiting for the next client
        }
        
        printf("Protocol and File Name Requested: %s\n", msg_recv); // Output received request
        
        // Parse received message (protocol type, file name, and drop percentage)
        sscanf(msg_recv, "%s %s %s", protocolType_recv, file_name_recv, percent);

        // Check if file exists on server and has read permissions
        if (access(file_name_recv, F_OK | R_OK) == 0) {
            // File exists, so proceed with transmission
            stat(file_name_recv, &st);
            f_size = st.st_size;  // Get file size
            fp = fopen(file_name_recv, "rb");  // Open file for reading

            long int total_frame = (f_size % BUF_SIZE) == 0 ? (f_size / BUF_SIZE) : (f_size / BUF_SIZE) + 1;
            printf("File size: %ld bytes\n", f_size);
            printf("Total number of packets that will be sent -> %ld\n", total_frame);

            // Send `total_frame` to client before starting transmission
            if (sendto(s, &total_frame, sizeof(total_frame), 0, (struct sockaddr*)&c_addr, length) == -1) {
                print_error("Server: Failed to send total frame count");
            }

            // Handle protocol type (Stop-and-Wait or Go-Back-N)
            if (strcmp(protocolType_recv, "2") == 0) {  // Go-Back-N protocol
                printf("Go Back [N]\n");
                float drop_probability = atof(percent) / 100.0;
                go_back_n(s, &c_addr, length, fp, total_frame, drop_probability);
            } else {
                printf("Invalid Protocol Type\n");
            }

            fclose(fp);  // Close file after transmission
        } else {
            printf("Invalid Filename or File Not Accessible\n");  // File does not exist or cannot be read
        }
    }

    close(s);  // Close socket when server is terminated
    return 0;
}

// Function to print an error message and exit program
void print_error(char* msg) {
    perror(msg); // Print error
    exit(EXIT_FAILURE); // Exit program with failure status
}

// Function to generate which frames to drop based on drop percentage
int* generate_drops(int total_frame, float drop_percent) {
    int td = (int)(drop_percent * total_frame / 100); // Calculate number of frames to drop
    int* testdrop = (int*)malloc(td * sizeof(int)); // Allocate memory for drop array
    if (!testdrop) { //If memory was not allocated correctly
        print_error("Memory allocation failed for testdrop");
    }
    srand(time(NULL)); // Seed random number generator with current time

    // Populate array with unique random frame numbers to drop
    for (int i = 0; i < td; i++) {
        int new_drop;
        int duplicate;
        do {
            duplicate = 0;
            new_drop = rand() % total_frame; //Make a random frame number based on the total frames
            // Check for duplicates
            for (int j = 0; j < i; j++) {
                if (testdrop[j] == new_drop) {
                    duplicate = 1;
                    break;
                }
            }
        } while (duplicate); // Keep generating until unique
        testdrop[i] = new_drop;
        printf("Frame to drop: %d\n", testdrop[i]);
    }

    return testdrop; // Return array of dropped frames
}

// Implementation of Stop-and-Wait ARQ protocol
void stop_and_wait(int s, struct sockaddr_in* c_addr, socklen_t length, FILE* fp, int total_frame, int* testdrop, int td) {
    struct frame_packet frame;  // Frame structure for sending data
    int ack_num = 0, drop_frame = 0, resend_frame = 0;  // Variables for tracking acknowledgments, drops, and resends
    long int i = 1;  // Frame counter starts at 1
    int resend_limit = 5;  // Maximum number of retries for each frame
    int retry_count = 0;   // Track how many times the frame has been resent

    // Loop through all frames to be sent
    while (i <= total_frame) {
        memset(&frame, 0, sizeof(frame));  // Clear frame structure
        frame.ID = i;  // Set frame ID (Sequence Number)
        frame.length = fread(frame.data, 1, BUF_SIZE, fp);  // Read data from file into frame

        if (frame.length <= 0) {
            // End of file or error, no need to send this frame
            break;
        }

        // Check if frame should be dropped (only simulate drop once per frame)
        int drop = 0;
        for (int j = 0; j < td; j++) {
            if (frame.ID == testdrop[j]) {  // If frame ID matches a drop frame
                drop = 1;
                break;
            }
        }

        // Handle frame dropping simulation (don't increment retry count yet)
        if (drop) {
            printf("frame.id# %ld dropped (simulated loss)\n", frame.ID);
            drop_frame++;  // Increment drop count
        }

        // Resend loop in case acknowledgment fails
        while (retry_count <= resend_limit) {
            if (!drop) {
                // Send frame to client
                if (sendto(s, &frame, sizeof(frame), 0, (struct sockaddr*)c_addr, length) == -1) {
                    print_error("Server: Send frame failed");
                } else {
                    printf("Frame# %ld sent (retry count: %d)\n", frame.ID, retry_count);
                    if (retry_count > 0) {
                        resend_frame++;  // Increment resend count for retries
                    }
                }
            }

            // Reset length before each `recvfrom()` call
            length = sizeof(*c_addr);

            // Wait for acknowledgment from client
            if (recvfrom(s, &ack_num, sizeof(ack_num), 0, (struct sockaddr*)c_addr, &length) == -1) {
                perror("Server: Receive ack failed");  // Handle timeout or receive error
                retry_count++;  // Increment retry count on timeout or failed ACK
            } else {
                printf("Received ACK for frame# %d\n", ack_num);

                // If acknowledgment matches frame ID, move to next frame
                if (ack_num == frame.ID) {
                    printf("Frame# %ld acknowledged\n", frame.ID);
                    retry_count = 0;  // Reset retry count for next frame
                    i++;  // Move to next frame
                    break;  // Exit retry loop and proceed to next frame
                } else {
                    printf("Incorrect ACK received (ACK = %d). Resending frame# %ld...\n", ack_num, frame.ID);
                    retry_count++;
                }
            }

            // If retry limit is reached, skip to next frame
            if (retry_count > resend_limit) {
                printf("Error: Frame# %ld reached maximum resend attempts. Continuing with the next frame.\n", frame.ID);
                retry_count = 0;  // Reset retry count before moving to next frame
                i++;  // Move to next frame even if retry limit is reached
                break;
            }
        }
    }

    // Print frame data after loop is complete
    printf("\nTotal frames attempted to be sent: %ld\n", total_frame);
    printf("Total frames dropped: %i\n", drop_frame);
    printf("Total frames resent: %i\n", resend_frame);
}

// Implementation of Go-Back-N ARQ protocol
void go_back_n(int s, struct sockaddr_in* c_addr, socklen_t length, FILE* fp, int total_frame, float drop_probability) {
    struct frame_packet frame;  // Frame structure for sending data
    int ack_num = -1, drop_frame = 0, resend_frame = 0;
    long int base = 1;  // Base of window (first unacknowledged frame)
    long int next_seq_num = 1;  // Next sequence number to send
    int window_size = WINDOW_SIZE;  // Size of Go-Back-N window
    int resend_limit = 5;  // Retry limit for retransmission
    int retry_count = 0;

    // Set the timeout before starting the transmission
    struct timeval timeout;
    timeout.tv_sec = 2;  // Set a 2-second timeout for ACKs
    timeout.tv_usec = 0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    while (base <= total_frame) {
        // Send frames within current window
        while (next_seq_num < base + window_size && next_seq_num <= total_frame) {
            memset(&frame, 0, sizeof(frame));
            fseek(fp, (next_seq_num - 1) * BUF_SIZE, SEEK_SET);  // Move file pointer to correct frame data
            frame.ID = next_seq_num;
            frame.length = fread(frame.data, 1, BUF_SIZE, fp);

            // Simulate packet drop
            float random_val = ((float)rand() / (float)RAND_MAX);  // Random value between 0 and 1
            if (random_val < drop_probability) {
                printf("Frame ID# %ld dropped (simulated loss)\n", frame.ID);
                drop_frame++;
            } else {
                // Send frame
                if (sendto(s, &frame, sizeof(frame), 0, (struct sockaddr*)c_addr, length) == -1) {
                    perror("Server: Send frame failed");
                } else {
                    printf("Frame# %ld sent\n", frame.ID);
                }
            }
            next_seq_num++;
        }

        // Receive ACK from client
        length = sizeof(*c_addr);
        if (recvfrom(s, &ack_num, sizeof(ack_num), 0, (struct sockaddr*)c_addr, &length) == -1) {
            // Timeout occurred, retransmit all unacknowledged frames
            perror("Server: Receive ACK failed (timeout)");
            printf("Timeout occurred, retransmitting frames starting from base frame #%ld\n", base);

            // Retransmit all frames from base to next_seq_num
            for (long int i = base; i < next_seq_num && i <= total_frame; i++) {
                memset(&frame, 0, sizeof(frame));
                fseek(fp, (i - 1) * BUF_SIZE, SEEK_SET);  // Set file pointer for retransmission
                frame.ID = i;
                frame.length = fread(frame.data, 1, BUF_SIZE, fp);

                // Resend the frame
                if (sendto(s, &frame, sizeof(frame), 0, (struct sockaddr*)c_addr, length) == -1) {
                    perror("Server: Resend frame failed");
                } else {
                    printf("Resending frame# %ld\n", frame.ID);
                    resend_frame++;
                }
            }
        } else {
            printf("Received ACK for frame# %d\n", ack_num);
            // Slide window forward if ACK is received
            if (ack_num >= base) {
                base = ack_num + 1;  // Move base to next frame
                retry_count = 0;
            }
        }
    }

    // Optionally reset the timeout to wait indefinitely for new clients after transmission
    struct timeval no_timeout;
    no_timeout.tv_sec = 0;  // Set no timeout (wait indefinitely)
    no_timeout.tv_usec = 0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &no_timeout, sizeof(no_timeout));

    printf("\nTotal frames attempted to be sent: %ld\n", total_frame);
    printf("Total frames dropped: %i\n", drop_frame);
    printf("Total frames resent: %i\n", resend_frame);
}