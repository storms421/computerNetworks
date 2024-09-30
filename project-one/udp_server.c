#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <netdb.h>

#define BUF_SIZE 2048        // Maximum buffer size for data in a frame
#define SERVER_PORT 2226     // Server port

// Structure to represent a packet (sequence number, data length, data)
struct packet {
    int seq_num;              // Packet sequence number
    int data_len;             // Number of bytes of data in the packet
    char data[BUF_SIZE];      // Data buffer
};

// Function to simulate packet loss based on a specified probability
int should_drop_packet(float loss_probability) {
    float random_value = ((float)rand()) / RAND_MAX;  // Generate a random float between 0 and 1
    return random_value < loss_probability;           // Return true if the random value is less than loss probability
}

// Function to create and bind a UDP socket
int setup_udp_socket(struct sockaddr_in *server_addr, char *server_hostname, int server_port) {
    int sockfd;
    struct hostent *host;

    // Get server IP address from hostname
    if ((host = gethostbyname(server_hostname)) == NULL) {
        perror("gethostbyname");  // Print error if DNS resolution fails
        exit(EXIT_FAILURE);       // Exit program
    }

    // Create a UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);  // Use IPv4 and UDP
    if (sockfd < 0) {
        perror("socket");  // Print error if socket creation fails
        exit(EXIT_FAILURE);
    }

    // Clear server address structure
    memset(server_addr, 0, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;  // IPv4
    server_addr->sin_port = htons(server_port);  // Set server port (convert to network byte order)
    server_addr->sin_addr = *((struct in_addr *)host->h_addr);  // Set server IP address

    // Bind socket to server address and port
    if (bind(sockfd, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("bind");  // Print error if binding fails
        close(sockfd);   // Close socket before exiting
        exit(EXIT_FAILURE);
    }

    return sockfd;  // Return socket file descriptor
}

// Function to implement Stop-and-Wait ARQ protocol
void stop_and_wait(int sockfd, struct sockaddr_in *client_addr, socklen_t client_len, FILE *file, float loss_probability) {
    struct packet pkt;        // Create a packet to store data
    int seq_num = 0;          // Sequence number starts from 0
    int ack_num;              // Variable to store acknowledgment number
    char ack_buffer[sizeof(int)];  // Buffer to receive acknowledgment messages

    // Loop until file is completely read
    while (!feof(file)) {
        // Read a chunk of data from file
        pkt.seq_num = seq_num;                                 // Assign current sequence number to packet
        pkt.data_len = fread(pkt.data, 1, BUF_SIZE, file);     // Read a chunk of data from file into packet

        // Simulate packet loss
        if (should_drop_packet(loss_probability)) {
            printf("Packet %d lost.\n", seq_num);  // Indicate that packet was lost
            continue;  // Skip sending this packet to simulate loss
        }

        // Send packet with current sequence number
        if (sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)client_addr, client_len) == -1) {
            perror("sendto");  // Print error if sending fails
            exit(EXIT_FAILURE);
        }
        printf("Sent packet %d\n", seq_num);  // Indicate that packet was sent

        // Wait for acknowledgment (ACK)
        if (recvfrom(sockfd, ack_buffer, sizeof(ack_buffer), 0, (struct sockaddr *)client_addr, &client_len) == -1) {
            perror("recvfrom");  // Print error if receiving ACK fails
            exit(EXIT_FAILURE);
        }

        // Extract acknowledgment number
        ack_num = *((int *)ack_buffer);

        // Check if acknowledgment matches current sequence number
        if (ack_num == seq_num) {
            printf("Received ACK for packet %d\n", seq_num);  // ACK received, move to next sequence number
            seq_num++;  // Increment sequence number for next packet
        } else {
            printf("ACK mismatch or lost, resending packet %d\n", seq_num);  // ACK mismatch, retransmit packet
        }
    }

    printf("File transfer completed.\n");  // Indicate that file has been fully transmitted
}

int main(int argc, char *argv[]) {
    if (argc != 4) {  // Expect three command-line arguments: server hostname, file to send, loss probability
        fprintf(stderr, "Usage: %s <server_hostname> <file_to_send> <loss_probability>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Parse command-line arguments
    char *server_hostname = argv[1];    // Server hostname
    char *file_to_send = argv[2];       // File to send
    float loss_probability = atof(argv[3]);  // Convert loss probability from string to float

    srand(time(NULL));  // Seed random number generator

    // Open file to be sent
    FILE *file = fopen(file_to_send, "rb");  // Open file in binary read mode
    if (!file) {
        perror("fopen");  // Print error if file cannot be opened
        exit(EXIT_FAILURE);
    }

    // Set up UDP socket
    struct sockaddr_in server_addr;
    int sockfd = setup_udp_socket(&server_addr, server_hostname, SERVER_PORT);

    // Client address structure
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Start Stop-and-Wait ARQ protocol for file transmission
    stop_and_wait(sockfd, &client_addr, client_len, file, loss_probability);

    // Clean up
    fclose(file);  // Close file
    close(sockfd); // Close socket

    return 0;
}
