#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>
#include <inttypes.h>
#include "winsock2.h"
 
#pragma comment(lib, "ws2_32.lib")


#define BUF_SIZE 4096
#define PORT 28572

uint16_t Calc_checksum(void* addr, int count) {
    register long sum = 0;
    unsigned short* ptr = (unsigned short*)addr;

    while (count > 1) {
        sum += *ptr++;
        count -= 2;
    }

    if (count > 0)
        sum += *(unsigned char*)ptr;

    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    uint16_t checksum = (uint16_t)~sum;
    return checksum;
}

void set_socket_timeout(SOCKET sock) {
    struct timeval timeout;
    timeout.tv_sec = 3;  // 3 seconds timeout
    timeout.tv_usec = 0;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) < 0) {
        perror("The server doesn't respond, the time is out\n");
        closesocket(sock);
        WSACleanup();
        exit(EXIT_FAILURE);
    }
}

void send_file_tcp(const char* server_ip, int server_port, const char* file_path) {
    SOCKET sock;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];
    size_t file_size, total_sent = 0;
    uint16_t calculated_checksum = 0, total_checksum = 0;

    // Create a TCP socket, 'sock' is the socket descriptor
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("TCP socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);//converts the port number to network byte order
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);//converts the IP address to binary form

    // Connect to the server
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("TCP connection failed");
        closesocket(sock);
        exit(EXIT_FAILURE);
    }

    // Open the file to send, 'rb' means read-only in binary mode
    FILE* file = fopen(file_path, "rb");
    if (!file) {
        perror("File open failed");
        closesocket(sock);
        exit(EXIT_FAILURE);
    }

    // The next 2 lines are used to get the file size
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET); // Reset the file pointer to the beginning

    // Calculate the checksum of the file size
    uint16_t checksum = Calc_checksum(&file_size, sizeof(file_size));
    total_checksum += checksum;

	// convert the file size to a string and send it to the server
    snprintf(buffer, sizeof(buffer), "%zu", file_size);//buffer gets the value of file_size
    // Send the file size without checksum (pure file size)
    //send(sock, &file_size,sizeof(file_size), 0);
    send(sock, buffer, sizeof(buffer), 0);
    buffer[0] = '\0'; 
    // Send the file in chunks. Each chunk is sent without checksum (pure file data)
    while ((total_sent = fread(buffer, 1, BUF_SIZE, file)) > 0) {
        calculated_checksum = Calc_checksum(buffer, total_sent);
        total_checksum = (total_checksum + calculated_checksum)%65536;

        // Send the chunk without checksum (pure chunk data)
        send(sock, buffer,total_sent, 0);
    }

    set_socket_timeout(sock);  // Set receive timeout for TCP socket, 3 seconds
    buffer[0] = '\0'; 
    // Receiving response from server
    uint16_t received_checksum;
    //int received = recv(sock, &received_checksum, sizeof(received_checksum), 0);
    int received=recv(sock,buffer,sizeof(buffer),0);
    received_checksum = atoi(buffer);
    if (received > 0) {

        if (total_checksum == received_checksum) {
            printf("Server returns: 0x%04X, checksum OK\n", received_checksum);
        }
        else {
            printf("Server returns: 0x%04X, checksum ERROR\n", received_checksum);
        }
    }

    fclose(file);
    closesocket(sock);
}

void send_file_udp(const char* server_ip, int server_port, const char* file_path) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];
    size_t file_size, total_sent = 0;
    uint16_t calculated_checksum = 0, total_checksum = 0;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("UDP socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    FILE* file = fopen(file_path, "rb");
    if (!file) {
        perror("File open failed");
        closesocket(sock);
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    snprintf(buffer, sizeof(buffer), "%zu", file_size);
    // Calculate the checksum of the file size
    uint16_t checksum = Calc_checksum(&file_size, sizeof(file_size));
    total_checksum += checksum;

    // Send the file size without checksum (pure file size)
    sendto(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));

    while ((total_sent = fread(buffer, 1, BUF_SIZE, file)) > 0) {
        calculated_checksum = Calc_checksum(buffer, total_sent);
        //total_checksum += calculated_checksum;
        total_checksum = (total_checksum + calculated_checksum) % 65536;

        // Send the chunk without checksum (pure chunk data)
        sendto(sock, buffer, total_sent, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
    }

    set_socket_timeout(sock);  // Set receive timeout for UDP socket, 3 seconds
    // Receiving response from server
    int server_addr_len = sizeof(server_addr);
    uint16_t received_checksum;
    int received = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&server_addr, &server_addr_len);
    received_checksum = atoi(buffer);
    //int received = recvfrom(sock, received_checksum,sizeof(received_checksum), 0, (struct sockaddr*)&server_addr, &server_addr_len);
    if (received > 0) {

        if (total_checksum == received_checksum)
            printf("Server returns: 0x%04X, checksum OK\n", received_checksum);
        else
            printf("Server returns: %u, checksum ERROR\n", received_checksum);
    }

    fclose(file);
    closesocket(sock);
}

int main(int argc, char* argv[]) {
    WSADATA wsaData;
    if (argc != 5) {
        fprintf(stderr, "<server_ip> <server_port> <T/U> <file_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }


    const char* server_ip = argv[1];
    int server_port = atoi(argv[2]);
    char transport = argv[3][0];
    const char* file_path = argv[4];

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        exit(EXIT_FAILURE);
    }

    if (transport == 'T') {
        printf(">my_client %s %d %c %s\n", server_ip, server_port, transport, file_path);
        send_file_tcp(server_ip, server_port, file_path);
    }
    else if (transport == 'U') {
        printf(">my_client %s %d %c %s\n", server_ip, server_port, transport, file_path);
        send_file_udp(server_ip, server_port, file_path);
    }
    else {
        fprintf(stderr, "Invalid transport type. Use 'T' for TCP or 'U' for UDP.\n");
        WSACleanup();
        exit(EXIT_FAILURE);
    }
    WSACleanup();

    return 0;
}



