#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <netdb.h>

#define BUF_SIZE 2048        // Max buffer size for data in a frame
#define SERVER_PORT 2226     // Server Port

// Function to determine whether to drop a packet based on specified loss probability
int should_drop_packet(float loss_probability) {
    float random_value = ((float)rand()) / RAND_MAX;  // Generate random float between 0 and 1
    return random_value < loss_probability;           // Return true if random value is less than loss probability
}

// Function to create and bind a UDP socket using server hostname and port
int setup_udp_socket(struct sockaddr_in *server_addr, char *server_hostname, int server_port) {
    int sockfd;
    struct hostent *host;

    // Get server IP address from hostname
    if ((host = gethostbyname(server_hostname)) == NULL) {
        perror("gethostbyname");
        exit(EXIT_FAILURE);
    }

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");  // Print error if socket creation fails
        exit(EXIT_FAILURE);
    }

    // Clear server address structure
    memset(server_addr, 0, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;  // Use IPv4
    server_addr->sin_port = htons(server_port);  // Set server port (convert to network byte order)
    server_addr->sin_addr = *((struct in_addr *)host->h_addr);  // Set server IP address

    // Bind socket to address and port
    if (bind(sockfd, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("bind");  // Print error if binding fails
        close(sockfd);   // Close socket before exiting
        exit(EXIT_FAILURE);
    }

    return sockfd;  // Return socket file descriptor
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
        if (sendto(sockfd, buffer, bytes_read, 0, (struct sockaddr *)client_addr, client_len) == -1) {
            perror("sendto");
            exit(EXIT_FAILURE);
        }
        printf("Sent packet %d\n", seq_num);

        // Wait for acknowledgment
        if (recvfrom(sockfd, &ack_num, sizeof(ack_num), 0, (struct sockaddr *)client_addr, &client_len) == -1) {
            perror("recvfrom");
            exit(EXIT_FAILURE);
        }

        // Check if acknowledgment matches sequence number
        if (ack_num == seq_num) {
            printf("Received ACK for packet %d\n", seq_num);
            seq_num++;  // Move to next sequence number
        } else {
            printf("ACK mismatch or lost, resending packet %d\n", seq
