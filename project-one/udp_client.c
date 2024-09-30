#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <dirent.h>

//Client Side File

//Socket Commands for TCP:
//SOCKET - Create a new communication endpoint similar to opening a file
//BIND - Associate a local address with a socket
//LISTEN - Announce willingness to accept connections; give queue size
//ACCEPT - Passively establish an incoming connection
//CONNECT - Actively attempt to establish a connection
//SEND - Send some data over the connection
//RECEIVE - Recieve some data from the connection
//CLOSE - Release the connection

//Function declarations
int getHostInfo(int argc, char **argv, struct hostent **hostInfo);
int createSocket(struct hostent **hostInfo, struct sockaddr_in channel);
void serverConnect(int clientSocket, struct hostent **hostInfo, struct sockaddr_in channel);
char selectFile();
void requestFile(char *fileName, int clientSocket, char **argv, int bytes, char *buffer);

#define SERVER_PORT 2226 //Arbitrary server number
#define BUF_SIZE 2048 //Block transfer size

int main(int argc, char **argv) {
    // Define variables
    int bytes;                    // Size of file from server
    char buffer[BUF_SIZE];        // Buffer for file from server
    struct hostent *hostInfo;     // Info about server
    struct sockaddr_in channel;   // Holds IP address
    int clientSocket;
    char fileName[100];

    getHostInfo(argc, argv, &hostInfo);
    clientSocket = createSocket(&hostInfo, &channel);
    serverConnect(clientSocket, &hostInfo, &channel);
    selectFile(fileName);
    requestFile(fileName, clientSocket, argv, bytes, buffer);

    close(clientSocket);  // Close the client socket after transfer is complete
    return 0;
}

// Function to get the host info
int getHostInfo(int argc, char **argv, struct hostent **hostInfo) {
    // Make sure the correct number of arguments was inputted
    if (argc != 3) {
        perror("Usage: client server-name file-name");
        exit(EXIT_FAILURE);  // Exit if incorrect number of arguments
    }
    // Get the host information
    *hostInfo = gethostbyname(argv[1]);
    if (!*hostInfo) {
        perror("Getting host name failed");
        exit(EXIT_FAILURE);  // Exit if host information retrieval fails
    }
    return 0;
}

// Function to create a UDP socket
int createSocket(struct hostent **hostInfo, struct sockaddr_in *channel) {
    // Create the socket
    int clientSocket;
    clientSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);  // Use UDP instead of TCP

    // Check to make sure socket was made
    if (clientSocket < 0) {
        perror("Socket could not be created.");
        exit(EXIT_FAILURE);  // Exit if socket creation fails
    }

    memset(channel, 0, sizeof(*channel));   // Set IP address to 0
    channel->sin_family = AF_INET;          // Use IPV4
    memcpy(&channel->sin_addr.s_addr, (*hostInfo)->h_addr, (*hostInfo)->h_length);  // Copy host info to IP address
    channel->sin_port = htons(SERVER_PORT);  // Set IP address port to the server port

    // Return socket
    return clientSocket;
}

// Function to select a file name to receive from the server
void selectFile(char *fileName) {
    printf("Please enter the name of the file you want to request: ");
    if (fgets(fileName, 100, stdin) != NULL) {
        // Remove the trailing newline character if present
        size_t len = strlen(fileName);
        if (len > 0 && fileName[len - 1] == '\n') {
            fileName[len - 1] = '\0';
        }
        printf("You are requesting: %s\n", fileName);
    } else {
        perror("Could not read file name.");
        exit(EXIT_FAILURE);  // Exit if file name cannot be read
    }
}

// Function to request the file
void requestFile(char *fileName, int clientSocket, char **argv, int bytes, char *buffer) {
    // Send file name to server
    struct sockaddr_in serverAddr;
    int serverAddrLen = sizeof(serverAddr);

    // Sending the request
    if (sendto(clientSocket, fileName, strlen(fileName) + 1, 0, (struct sockaddr *)&serverAddr, serverAddrLen) == -1) {
        perror("sendto failed");
        exit(EXIT_FAILURE);  // Exit if send fails
    }

    // Receive the file data
    int readingFile = 1;
    while (readingFile) {
        bytes = recvfrom(clientSocket, buffer, BUF_SIZE, 0, (struct sockaddr *)&serverAddr, &serverAddrLen);
        if (bytes <= 0) {  // Check for end of file or error
            break;
        }
        write(1, buffer, bytes);  // Write to standard out
    }
}