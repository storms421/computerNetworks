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

#define BUF_SIZE (2048) // Max buffer size for data in a frame
#define SERVER_PORT 2226 // Server's UDP port
#define WINDOW_SIZE 3 // Window size for Go-Back-N ARQ

// Structure for a frame packet
struct frame_packet {
    long int ID; // Frame sequence number
    long int length; // Length of data in frame
    char data[BUF_SIZE]; // Data in frame
};

void print_error(char* msg);
void stop_and_wait(int s, struct sockaddr_in* c_addr, socklen_t length, FILE* fp, int total_frame, int* testdrop, int td);
void go_back_n(int s, struct sockaddr_in* c_addr, socklen_t length, FILE* fp, int total_frame, int* testdrop, int td);
int* generate_drops(int total_frame, float drop_percent); // Generates which frames to drop based on drop percentage

int main(int argc, char** argv) {
    if (argc != 1) { // Ensure correct usage
        printf("Usage: ./[%s]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct stat st; // Structure to get file information (size, etc.)
    struct sockaddr_in s_addr, c_addr; // Server and client socket addresses
    struct timeval t_out = { 0, 0 }; // Timeout structure (if needed)
    char msg_recv[BUF_SIZE]; // Buffer to store received message from client
    char file_name_recv[20]; // Buffer to store file name requested by client
    char protocolType_recv[10]; // Buffer to store protocol type requested (1 = Stop-and-Wait, 2 = Go-Back-N)
    char percent[10]; // Buffer to store drop percentage

    ssize_t numRead; // Number of bytes read from socket
    ssize_t length; // Length of sockaddr_in structure
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

        // Receive request message from client (protocol type, file name, drop percentage)
        if ((numRead = recvfrom(s, msg_recv, BUF_SIZE, 0, (struct sockaddr*)&c_addr, (socklen_t*)&length)) == -1) {
            print_error("Server: Received"); // Error handling for receiving data
        }
        printf("Protocol and File Name Requested: %s\n", msg_recv); // Output received request

        // Parse received message (protocol type, file name, and drop percentage)
        sscanf(msg_recv, "%s %s %s", protocolType_recv, file_name_recv, percent);

        // Check if file exists on server
        if (access(file_name_recv, F_OK) == 0) {
            stat(file_name_recv, &st); // Get file information (size, etc.)
            f_size = st.st_size; // File size in bytes
            fp = fopen(file_name_recv, "rb"); // Open file for reading in binary mode

            // Calculate total number of frames required to send entire file
            long int total_frame = (f_size % BUF_SIZE) == 0 ? (f_size / BUF_SIZE) : (f_size / BUF_SIZE) + 1;
            printf("Total number of packets that will be sent -> %d\n", total_frame);

            // Calculate frames to drop based on drop percentage
            float drop_percent = atof(percent); // Convert drop percentage to a float
            int* testdrop = generate_drops(total_frame, drop_percent); // Get array of frames to drop
            int td = (int)(drop_percent * total_frame / 100); // Calculate total number of drops
            printf("Total frame drop: %i\n\n", td); // Output total number of dropped frames
            printf("Received protocol type: '%s'\n", protocolType_recv); // Debug


            // Choose protocol based on client's request
            if (strcmp(protocolType_recv, "1") == 0) {
                // Stop-and-Wait protocol
                printf("Stop and wait"); // Debug
                stop_and_wait(s, &c_addr, length, fp, total_frame, testdrop, td);
            } else if (strcmp(protocolType_recv, "2") == 0) {
                // Go-Back-N protocol
                printf("Go Back [N]"); // Debug
                go_back_n(s, &c_addr, length, fp, total_frame, testdrop, td);
            } else {
                printf("Invalid Protocol Type\n"); // Invalid protocol type received
            }

            fclose(fp); // Close file after transmission
        } else {
            printf("Invalid Filename\n"); // File does not exist
        }
    }

    close(s); // Close socket when server is terminated
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
    srand(time(NULL)); // Seed random number generator with current time

    // Populate array with random frame numbers to drop
    for (int i = 0; i < td; i++) {
        testdrop[i] = rand() % total_frame; // Random frame number to drop
        printf("Frame to drop: %d\n", testdrop[i]);
    }

    return testdrop; // Return array of dropped frames
}

// Implementation of Stop-and-Wait ARQ protocol
void stop_and_wait(int s, struct sockaddr_in* c_addr, socklen_t length, FILE* fp, int total_frame, int* testdrop, int td) {
    struct frame_packet frame; // Frame structure for sending data
    int ack_num = 0, drop_frame = 0, resend_frame = 0; // Variables for tracking acknowledgments, drops, and resends
    long int i;

    // Loop through all frames to be sent
    for (i = 1; i <= total_frame; i++) {
        memset(&frame, 0, sizeof(frame)); // Clear frame structure
        frame.ID = i; // Set frame ID (Sequence Number)
        frame.length = fread(frame.data, 1, BUF_SIZE, fp); // Read data from file into frame

        // Check if frame should be dropped
        int drop = 0;
        for (int j = 0; j < td; j++) {
            if (frame.ID == testdrop[j]) { // If frame ID matches a drop frame
                drop = 1;
                break;
            }
        }

        if (drop) {
            printf("frame.id# %ld \t dropped\n", frame.ID); // Print dropped frame
            drop_frame++; // Increment drop count
            continue; // Skip sending this frame
        }

        // Send frame to client
        sendto(s, &frame, sizeof(frame), 0, (struct sockaddr*)c_addr, length);
        // Wait for acknowledgment from client
        recvfrom(s, &ack_num, sizeof(ack_num), 0, (struct sockaddr*)c_addr, &length);

        // Check if acknowledgment matches frame ID
        if (ack_num == frame.ID) {
            printf("frame.id# %ld \t Ack -> %ld \n", frame.ID, ack_num); // Print acknowledgment
        } else {
            printf("frame.id# %ld \t Resend\n", frame.ID); // If acknowledgment doesn't match, resend frame
            resend_frame++;
            i--; // Decrement i to resend same frame
        }
    }

    // Print summary of transmission
    printf("Total Dropped frames: %d\n", drop_frame);
    printf("Total Resent frames: %d\n", resend_frame);
}

// Implementation of Go-Back-N ARQ protocol
void go_back_n(int s, struct sockaddr_in* c_addr, socklen_t length, FILE* fp, int total_frame, int* testdrop, int td) {
    struct frame_packet frame; // Frame structure for sending data
    int ack_num = 0, drop_frame = 0, resend_frame = 0, win_start = 1, win_end = WINDOW_SIZE; // Variables for tracking acknowledgments, drops, and resends
    long int i;

    // Loop through all frames to be sent
    while (win_start <= total_frame) {
        for (i = win_start; i <= win_end && i <= total_frame; i++) {
            memset(&frame, 0, sizeof(frame)); // Clear frame structure
            frame.ID = i; // Set frame ID (Sequence Number)
            frame.length = fread(frame.data, 1, BUF_SIZE, fp); // Read data from file into frame

            // Check if frame should be dropped
            int drop = 0;
            for (int j = 0; j < td; j++) {
                if (frame.ID == testdrop[j]) { // If frame ID matches a drop frame
                    drop = 1;
                    break;
                }
            }

            if (drop) {
                printf("frame.id# %ld \t dropped\n", frame.ID); // Print dropped frame
                drop_frame++; // Increment drop count
                continue; // Skip sending this frame
            }

            // Send frame to client
            sendto(s, &frame, sizeof(frame), 0, (struct sockaddr*)c_addr, length);
        }

        // Wait for acknowledgment from client
        recvfrom(s, &ack_num, sizeof(ack_num), 0, (struct sockaddr*)c_addr, &length);

        // Check if acknowledgment is within window
        if (ack_num >= win_start) {
            printf("frame.id# %ld \t Ack -> %ld \n", win_start, ack_num); // Print acknowledgment
            win_start = ack_num + 1; // Slide window forward
            win_end = win_start + WINDOW_SIZE - 1;
        } else {
            printf("frame.id# %ld \t Resend\n", win_start); // If acknowledgment doesn't match, resend window
            resend_frame++;
            fseek(fp, (win_start - 1) * BUF_SIZE, SEEK_SET); // Move file pointer to start of window
        }
    }

    // Print summary of transmission
    printf("Total Dropped frames: %d\n", drop_frame);
    printf("Total Resent frames: %d\n", resend_frame);
}