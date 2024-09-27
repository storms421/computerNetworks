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

#define SERVER_PORT 12345 //Arbitrary server number
#define BUF_SIZE 4096 //Block transfer size

int main(int argc, char **argv) {
	//Variable declarations
	int c;	
	int s;		
	int bytes;	
	char buf[BUF_SIZE];		//Buffer for incoming file
	struct hostent *h;		//Info from server
	struct sockaddr_in channel;	//Holds IP address

	//Get info from the server
	if(argc != 3) fatal("Usage: client server-name file-name");
	h = gethostbyname(argv[1]);		//Look up host's IP adddress
	if (!h) fatal("gethostbynamefailed");

	//Create a socket that connects to the server side socket
	createSocket();	

	//Attempt to connect to the server
	connection = connect(s, (struct sockaddr *) &channel, sizeof(channel));
	if (connection < 0) fatal("Connect failed");

	//Connection is established, send file name to server
	write(s, argv[2], strlen(argv[2])+1);

	//Get the file and write it to stdout
	while(1) {
		bytes = read(s, buf, BUF_SIZE);	//Read from server socket
		if (bytes <= 0) exit(0);	//Check for end of file
		write(1, buf, bytes);		//Write to stdout
	}

	//Call ARQType function to determine the ARQ algorithm to be used
	protocol = ARQType(arqNumber);

	//Perform either Stop and Wait or Go Back N Protocol
	if (protocol.equals("Stop and Wait")) {
		StopAndWait();
	else if (protocol.equals("GoBackN")) {
		GoBackN();
	else {
		perror("No protocol specified");
	}

}//main


//Function to create UDP socket
void createSocket() {
	//Create a socket that connects to the server side socket
        clientSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	//Print error if socket could not be made
        if(clientSocket < 0) {
		perror("Socket could not be created");
	}

	//Link the socket to the specified server port
        memset(&channel, 0, sizeof(channel));
        channel.sin_family = AF_INET;
        memcpy(&channel.sin_addr.s_addr, h->h_addr, h->h_length);
        channel.sin_port=htons(SERVER_PORT);
}//createSocket

//Stop and Wait function (SW)
void StopAndWait () {
	//Receive packets one by one
	//Simulate packet loss from server	
}/StopAndWait

//Go Back N function (GBN)
void GoBackN() {
	//Receive packets in blocks
	//Simulate packet loss from server
}//GoBackN

//Recieve dropping probability from the server


//Determine parameter protocol type (ARQ type) 1 for SW 2 for GBN
String ARQType(int arqNumber) {
	if(arqNumber == 1) {
		return "Stop and Wait";
	else if(arqNumber == 2) {
		return "Go Back N";
	else {
		perror("Unidentified protocol type entered.");
}//ARQType


//Send file name to server

//Close socket when finished
close(socket);

//Error handling
perror(char *string) {
	printf("%s\n%, string);
	exit(1);
}//perror

