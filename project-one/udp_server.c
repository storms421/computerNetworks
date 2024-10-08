#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUF_SIZE 4096    // Max buffer size for data in a frame
#define SERVER_PORT 2226 // Server's UDP port
#define WINDOW_SIZE 3    // Window size for Go-Back-N ARQ

// Structure for a frame packet
struct frame_packet {
    long int ID;       // Frame sequence number
    long int length;   // Length of data in frame
    char data[BUF_SIZE]; // Data in frame
};

// Function to print an error message and exit program
void print_error(char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// Function to simulate packet loss based on a given probability
int simulate_loss(float loss_probability) {
    int rand_val = rand() % 100;
    return rand_val < loss_probability; // Return 1 if the frame should be "dropped"
}

// Function to set a timeout for receiving
void set_socket_timeout(int sock, int sec, int usec) {
    struct timeval timeout;
    timeout.tv_sec = sec;
    timeout.tv_usec = usec;
    
    // Set the socket option for receive timeout
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Error setting socket timeout");
    }
}

// Stop-and-Wait ARQ protocol
void stop_and_wait(int s, struct sockaddr_in* c_addr, socklen_t length, FILE* fp, long int total_frame, float loss_probability) {
    struct frame_packet frame;
    int ack_num = 0;
    long int i;
    int retry_count;
    const int resend_limit = 5; // Maximum retries for a frame

    // Set timeout for receiving ACK (e.g., 2 seconds)
    set_socket_timeout(s, 2, 0);

    for (i = 1; i <= total_frame; i++) {
        memset(&frame, 0, sizeof(frame)); // Clear the frame structure
        frame.ID = i;
        frame.length = fread(frame.data, 1, BUF_SIZE, fp); // Read data from the file into the frame

        // If end of file or error occurs while reading, break
        if (frame.length <= 0) {
            break;
        }

        // Simulate packet loss
        if (simulate_loss(loss_probability)) {
            printf("Frame %ld dropped\n", frame.ID);
            continue; // Skip sending this frame
        }

        // Resend logic
        retry_count = 0;
        while (retry_count < resend_limit) {
            // Send frame to client
            if (sendto(s, &frame, sizeof(frame), 0, (struct sockaddr*)c_addr, length) == -1) {
                print_error("Server: Failed to send frame");
            } else {
                printf("Frame %ld sent\n", frame.ID);
            }

            // Wait for ACK
            if (recvfrom(s, &ack_num, sizeof(ack_num), 0, (struct sockaddr*)c_addr, &length) == -1) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    // Timeout occurred
                    printf("Timeout waiting for ACK for frame %ld. Retrying...\n", frame.ID);
                } else {
                    perror("Server: Receive ACK failed");
                }
            } else {
                // If the correct ACK is received, move to the next frame
                if (ack_num == frame.ID) {
                    printf("Frame %ld acknowledged\n", frame.ID);
                    break;
                } else {
                    printf("Incorrect ACK received. Resending frame %ld...\n", frame.ID);
                }
            }

            retry_count++;
            if (retry_count == resend_limit) {
                printf("Error: Frame %ld reached maximum resend attempts. Terminating transmission.\n", frame.ID);
                return;
            }
        }
    }

    printf("\nTransmission finished. Total frames: %ld\n", total_frame);
}

// Go-Back-N ARQ protocol
void go_back_n(int s, struct sockaddr_in* c_addr, socklen_t length, FILE* fp, long int total_frame, float loss_probability) {
    struct frame_packet frame;
    int ack_num = 0;
    long int win_start = 1, win_end = WINDOW_SIZE;
    long int i = 1;

    while (i <= total_frame) {
        for (long int j = win_start; j <= win_end && j <= total_frame; j++) {
            memset(&frame, 0, sizeof(frame));
            frame.ID = j;
            frame.length = fread(frame.data, 1, BUF_SIZE, fp);

            if (simulate_loss(loss_probability)) {
                printf("Frame %ld dropped\n", frame.ID);
                continue; // Simulate dropped frame
            }

            // Send the frame
            if (sendto(s, &frame, sizeof(frame), 0, (struct sockaddr*)c_addr, length) == -1) {
                print_error("Server: Failed to send frame");
            }

            // Wait for ACK (can be extended with timeout handling as in Stop-and-Wait)
            if (recvfrom(s, &ack_num, sizeof(ack_num), 0, (struct sockaddr*)c_addr, &length) == -1) {
                print_error("Server: Failed to receive ACK");
            }

            // Move the window forward if ACK is valid
            if (ack_num >= win_start) {
                printf("Frame %d acknowledged, moving window\n", ack_num);
                win_start = ack_num + 1;
                win_end = win_start + WINDOW_SIZE - 1;
            } else {
                printf("Resending from frame %ld\n", win_start);
                break; // Resend from the start of the window
            }
        }
        i = win_start; // Update i to next frame to be sent
    }

    printf("\nTransmission finished. Total frames: %ld\n", total_frame);
}

// Main server function
int main(int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: %s <drop_probability> <protocol_type>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    float drop_probability = atof(argv[1]); // Drop probability as a percentage
    int protocol_type = atoi(argv[2]); // 1 = Stop-and-Wait, 2 = Go-Back-N

    struct sockaddr_in s_addr, c_addr;
    socklen_t length = sizeof(c_addr);
    int s;
    FILE* fp;
    char file_name_recv[256];
    struct stat st;
    off_t f_size;
    long int total_frame;

    // Initialize server socket
    memset(&s_addr, 0, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(SERVER_PORT);
    s_addr.sin_addr.s_addr = INADDR_ANY;

    // Create a socket
    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        print_error("Server: Socket creation failed");
    }

    // Bind the socket
    if (bind(s, (struct sockaddr*)&s_addr, sizeof(s_addr)) == -1) {
        print_error("Server: Bind failed");
    }

    for (;;) {
        memset(file_name_recv, 0, sizeof(file_name_recv));

        printf("Server: Waiting for client to request a file\n");

        // Receive the file name from the client
        if (recvfrom(s, file_name_recv, sizeof(file_name_recv), 0, (struct sockaddr*)&c_addr, &length) == -1) {
            print_error("Server: Failed to receive file name");
        }
        printf("Client requested file: %s\n", file_name_recv);

        // Check if the requested file exists and is readable
        if (access(file_name_recv, F_OK | R_OK) == 0) {
            stat(file_name_recv, &st);
            f_size = st.st_size;
            fp = fopen(file_name_recv, "rb");

            // Calculate total number of frames required to send the entire file
            total_frame = (f_size % BUF_SIZE == 0) ? f_size / BUF_SIZE : f_size / BUF_SIZE + 1;
            printf("Total frames to send: %ld\n", total_frame);

            // Select the ARQ protocol
            if (protocol_type == 1) {
                stop_and_wait(s, &c_addr, length, fp, total_frame, drop_probability);
            } else if (protocol_type == 2) {
                go_back_n(s, &c_addr, length, fp, total_frame, drop_probability);
            } else {
                printf("Invalid protocol type\n");
            }

            fclose(fp); // Close the file after transmission
        } else {
            printf("File not accessible or doesn't exist\n");
        }
    }

    close(s); // Close the socket
    return 0;
}