#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 512

void killServer(const char *msg){
    fputs(msg, stderr);
    fputs("\n", stderr);
    exit(1);
}

int main(int argc, char *argv[]) {

    if (argc < 3 || argc > 4) // Test for correct number of arguments
    killServer("Parameter(s) \n<Server Address> <Echo Word> [<Server Port>]");

    char *servIP = argv[1]; // First arg: server IP address (dotted quad)
    char *echoString = argv[2]; // Second arg: string to echo

    // Third arg (optional): server port (numeric). 7 is well-known echo port
    in_port_t servPort = (argc == 4) ? atoi(argv[3]) : 7;

    int domain;                       // Domain type (AF_INET or AF_INET6)
    struct sockaddr_storage servAddr; // Supports both IPv4 and IPv6
    socklen_t servAddrLen;

    // Detect IP type (IPv4 or IPv6)
    if (inet_pton(AF_INET, servIP, &((struct sockaddr_in *)&servAddr)->sin_addr) == 1)
    {
        // IPv4 address
        domain = AF_INET;
        struct sockaddr_in *addr4 = (struct sockaddr_in *)&servAddr;
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(servPort);
        servAddrLen = sizeof(struct sockaddr_in);
    }
    else if (inet_pton(AF_INET6, servIP, &((struct sockaddr_in6 *)&servAddr)->sin6_addr) == 1)
    {
        // IPv6 address
        domain = AF_INET6;
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&servAddr;
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(servPort);
        servAddrLen = sizeof(struct sockaddr_in6);
    }
    else
    {
        killServer("inet_pton() failed\nInvalid IP address format");
    }

    // Create a reliable, stream socket using TCP
    int sock = socket(domain, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) killServer("socket() failed");

    // Establish the connection to the echo server
    if (connect(sock, (struct sockaddr *) &servAddr, servAddrLen) < 0) killServer("connect() failed");

    size_t echoStringLen = strlen(echoString); // Determine input length

    // Send the string to the server
    ssize_t numBytes = send(sock, echoString, echoStringLen, 0);
    if (numBytes < 0) killServer("send() failed");
    else if (numBytes != (long int)echoStringLen) killServer("send()\nsent unexpected number of bytes");

    // Receive the same string back from the server
    unsigned int totalBytesRcvd = 0; // Count of total bytes received
    fputs("Received: ", stdout); // Setup to print the echoed string
    while (totalBytesRcvd < echoStringLen) {
        char buffer[BUFSIZE]; // I/O buffer
        /* Receive up to the buffer size (minus 1 to leave space for
        a null terminator) bytes from the sender */
        numBytes = recv(sock, buffer, BUFSIZE - 1, 0);
        if (numBytes < 0) killServer("recv() failed");
        else if (numBytes == 0) killServer("recv()\nconnection closed prematurely");
        totalBytesRcvd += numBytes; // Keep tally of total bytes
        buffer[numBytes] = '\0'; // Terminate the string!
        fputs(buffer, stdout); // Print the echo buffer
    }

    fputc('\n', stdout); // Print a final linefeed

    close(sock);
    exit(0);
}