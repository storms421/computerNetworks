#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define BUF_SIZE 2048      // Max buffer size for data in a frame
#define SERVER_PORT 2940   // Server Port

// Function to create and bind a UDP socket
int setup_udp_socket(struct sockaddr_in *server_addr) {
    int sockfd;

    // Create a UDP socket
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

// Function to determine whether to drop a packet based on specified loss probability
void drop_packet(float loss_probability) {
    // Logic to drop packet based on loss_probability
}

// Function to implement Stop-and-Wait ARQ protocol
void stop_and_wait(/* parameters */) {
    // SW implementation
}

// Function to implement Go-Back-N ARQ protocol
void go_back_n(/* parameters */) {
    // GBN implementation
}

int main(int argc, char *argv[]) {
    // Parse command-line arguments
    
    // Setup socket
    struct sockaddr_in server_addr;
    int sockfd;

    sockfd = setup_udp_socket(&server_addr); // Setup the UDP socket

    // Open the specified file in binary write mode
    file = fopen(argv[3], "wb");
    if (!file) {
        perror("fopen"); // Print error message if file opening fails
        exit(EXIT_FAILURE); // Exit on failure
    }

    // Start handling client requests

    // Close resources
    fclose(file); // Close the file to free resources
    close(sockfd); // Close the socket when done

    return 0;
}
