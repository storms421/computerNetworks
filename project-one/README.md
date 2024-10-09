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

CHALLENGES OVERCAME:
========================
Initially we were having trouble making the client and server connect, so we looked at the canvas and textbook resources for help. Later on, we were having trouble with implmenting the Stop and Wait and Go Back N protocols, but print messages for debugging errors was very useful, along with making sure the client and server acknowledge when something happens such as a transfer. One issue that plagued us for a while was that the requested file and the file that the client recieved were the same, so no changes in it were visible. This was fixed by making a new file for the server to place the requested data into. Right now, the server sends over some of the requested file, but is inconsistent with frame drops. Sometimes it will go all the way through, while other times it will get stuck somewhere and time out. The percentage of dropped frames works most of the time, as most percentages it will send the same amount of the file over, however it only sends part of the file rather than the entire thing. This is most likely due to some part of the text being outside of a given frame's limit, so it cuts off. 

RESOURCES & DISCUSSIONS:
========================

Textbook:
Referenced Chapter 5 (ARQ algorithms) and Chapter 6 (Berkeley Sockets and example socket programming) for guidance on socket programming and ARQ protocols.

Canvas:
Example code of udpclient.c and udpserver.c
