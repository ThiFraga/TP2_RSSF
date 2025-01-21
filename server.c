#include "common.h"

#define MAXPENDING 12 // Max number of connections

typedef struct {
    int clntSock;
    int type;
} Sensor;

typedef struct {
    Sensor sensores[MAXPENDING];
    pthread_mutex_t mutex;
} ListaSensores;

struct ThreadArgs {
    int clntSock;
    ListaSensores *listSensor;
};

void killServer(const char *msg){
    fputs(msg, stderr);
    fputs("\n", stderr);
    exit(1);
}

void handleSensor(int clntSocket, ListaSensores *ls) {
    sensor_message msgRcv, msg;
    int type = 0, i = 0;
    //receive first message 
    ssize_t numBytesRcv = recv(clntSocket, &msgRcv, sizeof(msgRcv),0);

    if(numBytesRcv > 0) {
        if (strcmp(msgRcv.type, "temperature") == 0)
            type = TEMPERATURE;
        if (strcmp(msgRcv.type, "humidity") == 0)
            type = HUMIDITY;
        if (strcmp(msgRcv.type, "air_quality") == 0)
            type = AIRQUALITY;
        // add sensor to list
        while (i < MAXPENDING) {
            if(ls->sensores[i].type == 0 && ls->sensores[i].clntSock == -2){
                pthread_mutex_lock(&ls->mutex);
                ls->sensores[i].type = type;
                ls->sensores[i].clntSock = clntSocket;
                pthread_mutex_unlock(&ls->mutex);
                break;
            }
            i++;
        } // log sensor
        printf("log:\n%s sensor in (%i,%i)\nmeasurement: %.4f\n\n",msgRcv.type,msgRcv.coords[0],msgRcv.coords[1], msgRcv.measurement);
        // send for sensors with same type
        for(int i = 0; i < MAXPENDING; i++) {
            if(ls->sensores[i].type == type && ls->sensores[i].clntSock >= 0) {
                send(ls->sensores[i].clntSock, &msgRcv, sizeof(msgRcv),0);
            }
        }
        // main loop for receiving and delivering messages
        for(;;){
            numBytesRcv = recv(clntSocket, &msg, sizeof(msg),0);
            if(numBytesRcv > 0) {
                printf("log:\n%s sensor in (%i,%i)\nmeasurement: %.4f\n",msg.type,msg.coords[0],msg.coords[1], msg.measurement);

                for(int i = 0; i < MAXPENDING; i++) {
                    if(ls->sensores[i].type == type && ls->sensores[i].clntSock >= 0) {
                        send(ls->sensores[i].clntSock, &msg, sizeof(msg),0);
                    }
                }
            }else {
                // deal with disconnection of sensor
                printf("log:\n%s sensor in (%i,%i)\nmeasurement: %.4f\n",msg.type,msg.coords[0],msg.coords[1], (-1.0));
                msg.measurement = -1;
                msg.coords[0] = msgRcv.coords[0];
                msg.coords[1] = msgRcv.coords[1];
                strcpy(msg.type, msgRcv.type);
                pthread_mutex_lock(&ls->mutex);
                ls->sensores[i].clntSock = -2;
                ls->sensores[i].type = 0;
                pthread_mutex_unlock(&ls->mutex);
                for(int i = 0; i < MAXPENDING; i++) {
                    if(ls->sensores->type == type) {
                            send(ls->sensores[i].clntSock, &msg, sizeof(msg),0);
                    }
                }
                break;
            }
        }
    }

    return;
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
    ListaSensores *ls =  ((struct ThreadArgs *) threadArgs)->listSensor;
    free(threadArgs);

    handleSensor(clntSock, ls);

    return (NULL);
}

int main(int argc, char *argv[]) {
    in_port_t serverPort;
    int ipParam, domain;
    ListaSensores listSensores;
    
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

    for(int i = 0; i < MAXPENDING; i++){
        listSensores.sensores[i].clntSock = -2;
        listSensores.sensores[i].type = 0;
    }

    pthread_mutex_init(&listSensores.mutex, NULL);

    for(;;) {
        struct sockaddr_in clntAddr;
        socklen_t clntAddrLen = sizeof(clntAddr);

        int clntSock = accept(servSock, (struct sockaddr *)&clntAddr, &clntAddrLen);
        if(clntSock > 0) {

            // create separeted memory for client
            struct ThreadArgs *threadArgs = (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs));
            if(threadArgs != NULL) {

                threadArgs->clntSock = clntSock;
                threadArgs->listSensor = &listSensores;

                pthread_t threadID;

                int returnValue = pthread_create(&threadID, NULL, ThreadMain, threadArgs);

                if(returnValue != 0) {
                    free(threadArgs);
                    printf("pthread_create() failed");
                    printf("with thread %lu\n", (unsigned long int) threadID);
                }
            }else {
                printf("malloc() failed");
            }
        }else {
            printf("accept() failed");
        }

    }
    pthread_mutex_destroy(&listSensores.mutex);


}