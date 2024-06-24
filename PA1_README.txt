# Computer Networks Programming Assignment

## How to Run the Code

This project consists of two separate solutions: a client and a server.

### Running the Server
1. Navigate to the directory containing the server code.
2. Compile the server code.
3. Run the server.
4. Notice - running the server file before the client is necessary!

### Running the Client
1. Navigate to the directory containing the client code.
2. Put the relevant input into the the client properties: ip address, port number(28572), choose the relevant protocol (T/U), write the file name ('input.txt').
3. Compile the client code.
4. Run the client.

## General Description of the Code

This project implements a basic client-server system for file transfer and checksum calculation. The system supports both TCP and UDP protocols.

### Server (server.c)
- Listens on both TCP and UDP sockets simultaneously using `select()`
- Handles incoming connections and file transfers
- Calculates Internet Checksum for received files
- Sends calculated checksum back to the client

### Client (client.c)
- Supports both TCP and UDP file transfers
- Reads a file and sends it to the server in chunks
- Calculates local checksum of the file
- Receives server-calculated checksum and compares it with local checksum

Both client and server use the Internet Checksum algorithm for data integrity verification.

## Known Bugs and Limitations

1. The code assumes a maximum buffer size of 4096 bytes.
2. The UDP implementation doesn't handle packet loss or out-of-order delivery, which could lead to issues with large files or unstable networks.
3. The server doesn't support concurrent connections - it processes one client at a time.
4. The code doesn't handle endianness issues, which could cause problems if run on different architectures.
5. The timeout for socket operations is hardcoded to 3 seconds. In case that time is out the client will present the server response.
