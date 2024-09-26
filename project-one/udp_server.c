#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define BUF_SIZE 2048      // Max buffer size for data in a frame
#define SERVER_PORT 2226   // Server Port

// Function to determine whether to drop a packet based on specified loss probability
int should_drop_packet(float loss_probability) {
    float random_value = ((float) rand()) / RAND_MAX;  // Generate random float between 0 and 1
    return random_value < loss_probability;            // Return true if random value is less than loss probability
}

// Function to create and bind a UDP socket
int setup_udp_socket(struct sockaddr_in *server_addr) {
    int sockfd;

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket"); // Print error if socket creation fails
        exit(EXIT_FAILURE); // Exit on failure
    }

    // Clear the server address structure
    memset(server_addr, 0, sizeof(*server_addr));
    server_addr->sin_family = AF_INET; // Use IPv4
    server_addr->sin_port = htons(SERVER_PORT); // Set server port (convert to network byte order)
    server_addr->sin_addr.s_addr = INADDR_ANY; // Allow any incoming address

    // Bind the socket to the address and port
    if (bind(sockfd, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("bind"); // Print error if binding fails
        close(sockfd); // Close the socket before exiting
        exit(EXIT_FAILURE); // Exit on failure
    }

    return sockfd; // Return the socket file descriptor
}

// Function to implement Stop-and-Wait ARQ protocol
void stop_and_wait(int sockfd, struct sockaddr_in *client_addr, socklen_t client_len, FILE *file, float loss_probability) {
    char buffer[BUF_SIZE];
    int seq_num = 0;  // Sequence number starts from 0
    int ack_num;

    while (!feof(file)) {
        int bytes_read = fread(buffer, 1, BUF_SIZE, file);  // Read a chunk from file

        // Simulate packet loss
        if (should_drop_packet(loss_probability)) {
            printf("Packet %d lost.\n", seq_num);
            continue;  // Simulate dropping packet
        }

        // Send packet with sequence number
        if (sendto(sockfd, buffer, bytes_read, 0, (struct sockaddr *) client_addr, client_len) == -1) {
            perror("sendto");
            exit(EXIT_FAILURE);
        }
        printf("Sent packet %d\n", seq_num);

        // Wait for acknowledgment
        if (recvfrom(sockfd, &ack_num, sizeof(ack_num), 0, (struct sockaddr *) client_addr, &client_len) == -1) {
            perror("recvfrom");
            exit(EXIT_FAILURE);
        }

        // Check if acknowledgment matches sequence number
        if (ack_num == seq_num) {
            printf("Received ACK for packet %d\n", seq_num);
            seq_num++;  // Move to next sequence number
        } else {
            printf("ACK mismatch or lost, resending packet %d\n", seq_num);
        }
    }
}


// Function to implement Go-Back-N ARQ protocol
void go_back_n(int sockfd, struct sockaddr_in *client_addr, socklen_t client_len, FILE *file, float loss_probability, int window_size) {
    // GBN implementation
}

int main(int argc, char *argv[]) {
    // Parse command-line arguments
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <loss_probability> <protocol_type> <file_name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // Converting command-line arguments to usable values
    float loss_probability = atof(argv[1]);  // Convert loss probability to float
    int protocol_type = atoi(argv[2]);       // Convert protocol type to integer
    char *file_name = argv[3];               // File name to transfer

    // Setup socket
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int sockfd = setup_udp_socket(&server_addr); // Setup the UDP socket

    // Open the specified file in binary write mode
    FILE *file = fopen(file_name, "rb");
    if (!file) {
        perror("fopen"); // Print error message if file opening fails
        close(sockfd);  // Close socket in case of failure
        exit(EXIT_FAILURE); // Exit on failure
    }

    // Start handling client requests (choose ARQ protocol)
    if (protocol_type == 1) {
        stop_and_wait(sockfd, &client_addr, client_len, file, loss_probability);
    } else if (protocol_type == 2) {
        int window_size = 5;  // Example window size for GBN
        go_back_n(sockfd, &client_addr, client_len, file, loss_probability, window_size);
    } else {
        fprintf(stderr, "Invalid protocol type: %d\n", protocol_type);
        exit(EXIT_FAILURE);
    }

    // Close resources
    fclose(file); // Close the file to free resources
    close(sockfd); // Close the socket when done

    return 0;
}
