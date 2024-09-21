#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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


//What the client does:
//Create a socket
//Bind is not required
//Connect starts the connection process
//After connection is established, send and recieve can be used to transmit data

//Check to see if client has been called with right number of arguments

//Create and initialize a socket

//Attempt to establish a connection with CONNECT

//Connection will be established if the server is running and attached to SERVER_PORT and is idle or has room in LISTEN queue

//Client sends the name of a file by writing on the socket

//Client reads the file block by block from socket and copies it to output

//Exits when finished
