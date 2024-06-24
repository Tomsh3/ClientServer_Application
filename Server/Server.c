#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>
#include <windows.h>
#include "winsock2.h"
#pragma comment(lib, "ws2_32.lib")

#define PORT 28572
#define BUF_SIZE 4096
#define MAX_DIGITS 10000

/* Compute Internet Checksum for "count" bytes
* beginning at location "addr".
*/
uint16_t Calc_checksum(void* addr, int count)
{
    register long sum = 0;
    unsigned short* ptr = (unsigned short*)addr;

    while (count > 1) {
        /* This is the inner loop */
        sum += *ptr++;
        count -= 2;
    }

    /* Add left-over byte, if any */
    if (count > 0)
        sum += *(unsigned char*)ptr;

    /* Fold 32-bit sum to 16 bits */
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    uint16_t checksum = ~sum;
    return checksum;
}


// Alternate TCP handler
void handle_tcp_client(SOCKET client_sock) {
    char buffer[BUF_SIZE]; // 4 kB buffer
    int received;
    size_t fileSize = 0;
    uint16_t calculated_checksum = 0, total_checksum = 0;

    // Receive the file size

    received = recv(client_sock, buffer, sizeof(buffer), 0);
    fileSize = atoi(buffer);
    if (received < 0) {
        perror("recv failed");
        closesocket(client_sock);
        return;
    }

    // Calculate checksum for the file size
    total_checksum += Calc_checksum(&fileSize, sizeof(fileSize));
    //receive the file data
    long totalReceived = 0;
    buffer[0]='\0'; 
    while (totalReceived < fileSize) { // Each round we receive 4 kB of data
        received = recv(client_sock, buffer, sizeof(buffer), 0);
        if (received < 0) {
            perror("recv failed");
            closesocket(client_sock);
            return;
        }
        //calculate the checksum for the received data
        calculated_checksum = Calc_checksum(buffer, received);
        total_checksum = (total_checksum + calculated_checksum)%65536;
        totalReceived += received;     
    }

    printf("TCP: %zu bytes, checksum=0x%04X\n", fileSize, total_checksum);
    buffer[0] = '\0'; 
    // Prepare a response, the response is the total checksum
    snprintf(buffer, sizeof(buffer), "%u", total_checksum);
    send(client_sock, buffer, sizeof(buffer), 0);
    closesocket(client_sock);
}


void handle_udp_client(SOCKET udp_sock) {
    struct sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);
    char buffer[BUF_SIZE];
    int received;
    size_t fileSize = 0;
    uint16_t calculated_checksum = 0, total_checksum = 0;

    // Receive the file size
    received = recvfrom(udp_sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &addr_len); // holds the number of bytes we are going to receive and fill the buffer with the data
    fileSize = atoi(buffer);
    if (received < 0) {
        perror("recvfrom failed");
        closesocket(udp_sock);
        return;
    }

    // Calculate checksum for the file size
    total_checksum += Calc_checksum(&fileSize, sizeof(fileSize));


    char client_ip[sizeof(struct sockaddr_in)];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(struct sockaddr_in));
    printf("Recived from %s port: %d\n", client_ip, ntohs(client_addr.sin_port));
    long totalReceived = 0;

    // Receive the file data
    while (totalReceived < fileSize) { // Each round we receive 4 kB of data
        received = recvfrom(udp_sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &addr_len);
        if (received < 0) {
            perror("recv failed");
            closesocket(udp_sock);
            return;
        }
        // Calculate checksum for the received chunk
        calculated_checksum = Calc_checksum(buffer, received);
        //total_checksum += calculated_checksum;
        totalReceived += received;
        total_checksum = (total_checksum + calculated_checksum) % 65536;
    }
    printf("UDP: %d bytes, checksum=0x%04X\n", totalReceived, total_checksum);
    buffer[0] = '\0';
    // Prepare a response, the response is the total checksum
    snprintf(buffer, sizeof(buffer), "%u", total_checksum);
    sendto(udp_sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, addr_len);
    //closesocket(udp_sock);
}

int main() {
    SOCKET tcp_sock, udp_sock;
    struct sockaddr_in server_addr, client_addr;
    int client_addr_len = sizeof(client_addr);
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        exit(EXIT_FAILURE);
    }

    tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock < 0) {
        perror("TCP socket creation failed");
        exit(EXIT_FAILURE);
    }

    udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) {
        perror("UDP socket creation failed");
        closesocket(tcp_sock);
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(tcp_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("TCP bind failed");
        closesocket(tcp_sock);
        closesocket(udp_sock);
        exit(EXIT_FAILURE);
    }

    if (bind(udp_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("UDP bind failed");
        closesocket(tcp_sock);
        closesocket(udp_sock);
        exit(EXIT_FAILURE);
    }

    if (listen(tcp_sock, 1) < 0) {
        perror("TCP listen failed");
        closesocket(tcp_sock);
        closesocket(udp_sock);
        exit(EXIT_FAILURE);
    }

    printf(">my_server\n");

    fd_set read_fds;
    int max_fd = (tcp_sock > udp_sock) ? tcp_sock : udp_sock;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(tcp_sock, &read_fds);
        FD_SET(udp_sock, &read_fds);

        int status = select(0, &read_fds, NULL, NULL, 0);
        if (status < 0) {
            perror("select failed!!!!");
            closesocket(tcp_sock);
            closesocket(udp_sock);
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(tcp_sock, &read_fds)) {
            //int client_sock = accept(tcp_sock, (struct sockaddr*)&client_addr, &client_addr_len); lior
            SOCKET client_sock = accept(tcp_sock, (struct sockaddr*)&client_addr, &client_addr_len);
            if (client_sock >= 0) {
                char client_ip[sizeof(struct sockaddr_in)];
                inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(struct sockaddr_in));
                printf("Recivied from %s Port: %d\n", client_ip, ntohs(client_addr.sin_port));
            }

            if (client_sock < 0) {
                perror("TCP accept failed");
                continue;
            }
            handle_tcp_client(client_sock);
        }

        if (FD_ISSET(udp_sock, &read_fds)) {
            handle_udp_client(udp_sock);
        }
    }

    closesocket(tcp_sock);
    closesocket(udp_sock);
    WSACleanup();
    return 0;
}

