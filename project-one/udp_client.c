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
/*
*SOCKET - Create a new communication endpoint (similar to opening a file)
*BIND - Associate a local address with a socket
*LISTEN - Announce willingness to accept connections; give queue size
*ACCEPT - Passively establish an incoming connection
*CONNECT - Actively attempt to establish a connection
*SEND - Send some data over the connection
*RECEIVE - Recieve some data from the connection
*CLOSE - Release the connection
*/

#define SERVER_PORT blahblahblah //Arbitrary server number
#define BUF_SIZE 4096 //Block transfer size

int main(int argc, char** argv) {
	//Define variables
	int bytes;		    //Size of file from server
	char buffer[BUF_SIZE];	    //Buffer for file from server
	struct hostent *hostInfo;   //Info about server
	struct sockaddr_in channel; //Holds IP address

	int clientSocket;
	char fileName[100];
	
	getHostInfo()
	clientSocket = createSocket(*hostInfo, *channel);
	serverConnect(clientSocket, *hostInfo, *channel);
	fileName = selectFile();
	requestFile(*fileName, clientSocket **argv, bytes, *buffer);	
	
}//main

//Function to get the host info
void getHostInfo() {

}//getHostInfo

//Function to create an UDP socket
int createSocket(struct hostent *hostInfo, struct sockaddr_in *channel) {
	//Create the socket
	int clientSocket;
	clientSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	//Check to make sure socket was made
	if (clientSocket < 0) {
		perror("Socket could not be created.");
	}
	
	memset(&channel, 0, sizeof(channel)); 	//Set IP address to 0
	channel.sin_family = AF_INET;		//Use IPV4		
	memcpy(&channel.sin_addr.s_addr, hostInfo->h_addr, hostInfo->h_length);	//Copy host info to IP address
	channel.sin_port_htons(SERVER_PORT);	//Set IP address port to the server port

	//Return socket
	return clientSocket;
}//createSocket

//Function to connect the client to the server
void serverConnect(int clientSocket, struct hostent *hostInfo, struct sockaddr_in *channel) {
	//Attempt to create the connection
	int connection;
	connection = connect(clientSocket, (struct sockaddr *) &channel, sizeof(channel));

	//Print error if connection failed
	if (connection < 0) {
		perror("Connection failed.");
	}
}//serverConnect

//Function to determine parameter protocol type (ARQ type) 1 for SW 2 for GBN
String ARQType(int arqNumber) {
        if(arqNumber == 1) {
                return "Stop and Wait";
        else if(arqNumber == 2) {
                return "Go Back N";
        else {
                perror("Unidentified ARQ protocol.")
}//ARQType

//Function to select a file name to receive from the server
char selectFile() {
	char fileName[100];
	
	printf("Please enter the name of the file you want to request:");
	if (fgets(fileName, sizeof(fileName), stdin) != NULL) {
		printf("You are requesting: %s", fileName);
	}
	else {
		perror("Could not read file name.");
	}

	return fileName;
}//selectFile

//Function to request the file
void requestFile(char *fileName, int clientSocket, char **argv, int bytes, char *buffer) {
	//Send file name to server
	write(clientSocket, argv[2], strlen(argv[2])+1);

	//After receiving the file, write to standard out
	int readingFile = 1;
	while(readingFile) {
		bytes = read(clientSocket, buffer, BUF_SIZE);	//Read from socket
		if (bytes <= 0) {				//Check for end of file
			exit(0);
		}
		write(1, buffer, bytes);			//Write to standard out
	}
}//requestFile

//Function for Stop and Wait (SW)
void StopAndWait () {
	//Receive packets one by one
	//Simulate packet loss from server	
}/StopAndWait

//Function for Go Back N (GBN)
void GoBackN() {
	//Receive packets in blocks
	//Simulate packet loss from server
}//GoBackN

//Function to perform error handling
perror(char *string) {
	printf("%s\n%, string);
	exit(1);
}//perror

