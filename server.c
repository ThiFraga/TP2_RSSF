#include "common.h"

#define BUFSIZE 512 // Tamanho máximo do buffer
#define MAXPENDING 100 // Número máximo de conexões pendentes

typedef struct {
    int sock;
    char type[12];
} Sensor;

struct ThreadArgs {
    int clntSock;
    Sensor *sensores[10][10];
};

void killServer(const char *msg){
    fputs(msg, stderr);
    fputs("\n", stderr);
    exit(1);
}

void handleSensor(int clntSocket) {
    sensor_message msgRcv;

    for(;;){
        ssize_t numBytesRcv = recv(clntSocket, &msgRcv, sizeof(msgRcv),0);
        if(numBytesRcv >= 0) {
            print("Mensagem:\n Type: %s\nCoord: %i %i\nmeasurement: %f\n",&(msgRcv.type),msgRcv.coords[0], msgRcv.coords[1], msgRcv.measurement);
        }
    }
}

int identifyIPVersion(const char *param) {
    if(strcmp(param,"v4") == 0) {
        return 1;
    }else if(strcmp(param,"v6") == 0) {
        return 2;
    } else {
        return 0;
    }
}


void *ThreadMain(void *threadArgs) {
    // makes sure that thread resources are deallocated upon return
    pthread_detach(pthread_self());

    //Extract socket file descriptor from argument
    int clntSock = ((struct ThreadArgs *) threadArgs)->clntSock;
    free(threadArgs);

    handleSensor(clntSock);

    return (NULL);
}

int main(int argc, char *argv[]) {
    in_port_t serverPort;
    int ipParam, domain;
    Sensor sensores[10][10];
    
    // check number of params
    if(argc != 3) killServer("Error: wrong number of arguments:\nExample: ./server v4 51511");

    //check if IP version is valid
    ipParam = identifyIPVersion(argv[1]);
    if(ipParam == 0) killServer("Error: IP version invalid.\nValid values are v4 and v6");

    domain = (ipParam == 1) ? AF_INET : AF_INET6;

    int servSock;
    if((servSock = socket(domain, SOCK_STREAM, IPPROTO_TCP)) < 0) killServer("socket() failed :(");

    serverPort = atoi(argv[2]);

    if (domain == AF_INET)
    {
        // Construct local address structure for IPv4
        struct sockaddr_in servAddr;
        memset(&servAddr, 0, sizeof(servAddr));       // Zero out structure
        servAddr.sin_family = domain;                 // IPv4 address family
        servAddr.sin_addr.s_addr = htonl(INADDR_ANY); // Any incoming interface
        servAddr.sin_port = htons(serverPort);          // Local port

        // Bind to the local address
        if (bind(servSock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
            killServer("bind() failed");
    }
    else if (domain == AF_INET6)
    {
        // Construct local address structure for IPv6
        struct sockaddr_in6 servAddr6;
        memset(&servAddr6, 0, sizeof(servAddr6)); // Zero out structure
        servAddr6.sin6_family = domain;           // IPv6 address family
        servAddr6.sin6_addr = in6addr_any;        // Any incoming interface
        servAddr6.sin6_port = htons(serverPort);    // Local port

        // Bind to the local address
        if (bind(servSock, (struct sockaddr *)&servAddr6, sizeof(servAddr6)) < 0)
            killServer("bind() failed");
    }

    if(listen(servSock, MAXPENDING) < 0) killServer("listen() failed");

    for(;;) {
        struct sockaddr_in clntAddr;
        socklen_t clntAddrLen = sizeof(clntAddr);

        int clntSock = accept(servSock, (struct sockaddr *)&clntAddr, &clntAddrLen);
        if(clntSock < 0) killServer("accept() failed");

        // create separeted memory for client
        struct ThreadArgs *threadArgs = (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs));
        if(threadArgs == NULL) killServer("malloc() failed");

        threadArgs->clntSock = clntSock;

        pthread_t threadID;

        int returnValue = pthread_create(&threadID, NULL, ThreadMain, threadArgs);

        if(returnValue != 0) {
            killServer("pthread_create() failed");
            printf("with thread %lu\n", (unsigned long int) threadID);
        }

    }

}