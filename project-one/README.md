NAME OF PROJECT: Project 1: Implement File Server
=================================================

MEMBERS: Ben Wilmer and Noah Storms
===================================

STATEMENT: 
===========
We have neither given nor received unauthorized assistance on this work.

HOW TO ACCESS:
==============
Open two terminals, one for the server and one for the client.

Compile the server and client code using gcc:

Terminal 1 (Server):

	gcc -o server udp_server.c

Terminal 2 (Client):

	gcc -o client udp_client.c

Run the server and client in different terminals:

Terminal 1 (Server side):

	./server

Terminal 2 (Client side):

	./client <server_hostname>

Client Menu:
Once the client starts, you'll be prompted to enter a command in the format:

	[protocol_type] [file_name] [drop_percentage]

Example:
- Stop-and-Wait Protocol (with 10% packet drop):
	1 testfile.txt 10

- Go-Back-N Protocol (with 20% packet drop):
	2 testfile.txt 20

Protocol Options:
- Stop-and-Wait (SW): Enter 1 for Stop-and-Wait protocol.
- Go-Back-N (GBN): Enter 2 for Go-Back-N protocol.

Exit Command:
- To exit the client program and stop the server, type:
	
	- exit

PROJECT DESCRIPTION:
====================
This project implements a client-server system using UDP to transmit files with simulated packet loss. It supports two ARQ (Automatic Repeat request) protocols: 

	1. Stop-and-Wait (SW)
	2. Go-Back-N (GBN) 

allowing the server to send files reliably over an unreliable network.

Key Features:
- UDP Communication: The project uses UDP for communication between the client and server.
- Packet Loss Simulation: The server simulates packet loss based on a percentage provided by the client.
- ARQ Protocols:
	- Stop-and-Wait: The client acknowledges each packet before the server sends the next one.
	- Go-Back-N: The server can send multiple packets before waiting for acknowledgments, but it retransmits unacknowledged packets if
	an error occurs.

Files:
======
udp_server.c:
==============
The server-side program that listens for file requests from the client. It simulates packet loss based on the client's input and uses either the Stop-and-Wait or Go-Back-N ARQ protocol to ensure reliable delivery.

Key functionalities:
- Listens for client requests.
- Simulates packet drops.
- Implements Stop-and-Wait and Go-Back-N protocols.

udp_client.c:
==============
The client-side program that requests a file from the server using either the Stop-and-Wait or Go-Back-N ARQ protocol.

Key functionalities:
- Sends file requests.
- Receives file data in chunks and acknowledges packets.
- Handles retransmissions in case of packet loss.

Challenges:
// insert

RESOURCES & DISCUSSIONS:
========================

Textbook:
Referenced Chapter 5 (ARQ algorithms) and Chapter 6 (Berkeley Sockets and example socket programming) for guidance on socket programming and ARQ protocols.

Canvas:
Example code of udpclient.c and udpserver.c

