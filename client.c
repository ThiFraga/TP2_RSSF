#include "common.h"

#define TIMEOUT_TEMP 5
#define TIMEOUT_HUMI 7
#define TIMEOUT_AIRQ 10

#define ACTION_NOTNEIGHBOR 0
#define ACTION_CORRECTION 1
#define ACTION_SAMELOC 2
#define ACTION_REMOVED 3


typedef struct Sens Sensor;
struct Sens{
    int coord[2];
    int type;
    double distance;
    Sensor *next;
};


typedef struct{
    Sensor *first;
}Neighbors;
struct dataT {
    sensor_message *msg;
    int timeout;
    int sock;
    int type;
    Neighbors *list;
};

double getDistance(int coordI[2],int coordThey[2]) {
    return sqrt(pow((double)(coordThey[0] - coordI[0]),2) + pow((double)(coordThey[1] - coordI[1]),2));
}

float newMeasurement(float prevMeasu, float newMeasu, float distance, int type) {
    float minVal, maxVal, measurement;
    switch (type)
    {
    case TEMPERATURE:
        minVal = 20.0;
        maxVal = 40.0;
        break;

    case HUMIDITY:
        minVal = 10.0;
        maxVal = 90.0;
        break;
    case AIRQUALITY:
    default:
        minVal = 15.0;
        maxVal = 30.0;
        
        break;
    }
    measurement = prevMeasu + 0.1*(1/(distance + 1))*(newMeasu-prevMeasu);
    if (measurement > maxVal){
        return maxVal;
    }
    if(measurement < minVal) {
        return minVal;
    }
    return measurement;
}

void killClient(const char *msg)
{
    fputs(msg, stderr);
    fputs("\n", stderr);
    exit(1);
}

void handleInputErrors(const char *msg)
{
    fputs(msg, stderr);
    fputs("\n", stderr);
    fputs("Usage .client <server_ip> <port> -type <temperature|humidity|air_quality> - coords <x> <y>\n", stderr);
    exit(1);
}

float randomMeasurement(int type) {
    float measurement = 0;
    int maxVal = 0, minVal = 0;

    srand(time(NULL));
    switch (type)
    {
    case TEMPERATURE:
        minVal = 2000;
        maxVal = 4000;
        break;

    case HUMIDITY:
        minVal = 1000;
        maxVal = 9000;
        break;
    case AIRQUALITY:
    default:
        minVal = 1500;
        maxVal = 3000;
        
        break;
    }
    measurement = (minVal + (float)(rand() % (maxVal-minVal+100)))/100;
    return measurement;
}

sensor_message handleMessage(int coords[2], int type, float measurement)
{
    sensor_message message;

    message.coords[0] = coords[0];
    message.coords[1] = coords[1];

    switch (type)
    {
    case TEMPERATURE:
        strcpy(message.type, "temperature");
        break;

    case HUMIDITY:
        strcpy(message.type, "humidity");
        break;
    case AIRQUALITY:
    default:
        strcpy(message.type, "air_quality");
        break;
    }

    message.measurement = measurement;

    return message;
}

void handleLog(sensor_message *msg, float prevMeasurement, float newMeasurement, int action){
    switch (action)
    {
    case 0:
        printf("log:\n%s sensor in (%i,%i)\nmeasurement: %.4f\naction: not neighbor\n",msg->type,msg->coords[0],msg->coords[1],msg->measurement);
        break;
    case 1:
        printf("log:\n%s sensor in (%i,%i)\nmeasurement: %.4f\naction: correction of %.4f\n",msg->type,msg->coords[0],msg->coords[1],msg->measurement, newMeasurement - prevMeasurement);
        break;
    case 2:
        printf("log:\n%s sensor in (%i,%i)\nmeasurement: %.4f\naction: same location\n",msg->type,msg->coords[0],msg->coords[1],msg->measurement);
        break;
    case 3:
        printf("log:\n%s sensor in (%i,%i)\nmeasurement: %.4f\naction: removed\n",msg->type,msg->coords[0],msg->coords[1],msg->measurement);
        break;
    default:
        break;
    }
}

void addSensor(sensor_message *msg, Sensor *prev,Sensor *it, Neighbors *nList, double distance) {

    Sensor *newS = (Sensor *)malloc(sizeof(Sensor));
    if (newS == NULL) {
        //fprintf(stderr, "Erro: falha ao alocar memória para novo sensor.\n");
        return;
    }
    newS->coord[0] = msg->coords[0];
    newS->coord[1] = msg->coords[1];
    newS->distance = (float)distance;
    newS->next = it;

    if (prev != NULL) {
        prev->next = newS;
    } else {
        nList->first = newS;
    }
}

void handleMessagesReceived(sensor_message *msg, void *data, Neighbors *nList) {
    double distance = 0.0;
    int coord[2];
    coord[0] = ((struct dataT *)data)->msg->coords[0];
    coord[1] = ((struct dataT *)data)->msg->coords[1];
    int diffCoord = (msg->coords[0] - coord[0]) + (msg->coords[1] - coord[1]);
    int count = 0, action = ACTION_NOTNEIGHBOR;
    Sensor *prev = NULL, *it = nList->first;
    float prevMeasurement = ((struct dataT *)data)->msg->measurement;

    if (strcmp(msg->type, ((struct dataT *)data)->msg->type) == 0 && diffCoord != 0) {
        distance = getDistance(coord, msg->coords);
        if (msg->measurement != -1.0) {
            while (it != NULL) {
                // keep sort
                if(distance < it->distance){
                    addSensor(msg,prev,it,nList,distance);
                    break;
                }
                if(msg->coords[0] == it->coord[0] && msg->coords[1] == it->coord[1]) {
                    break;
                }

                prev = it;
                it = it->next;
                count++;
            }
            if(it == NULL) {
                addSensor(msg,prev,it,nList,distance);
            }
            if (count < 3) {
                ((struct dataT *)data)->msg->measurement = newMeasurement(((struct dataT *)data)->msg->measurement, msg->measurement, distance, ((struct dataT *)data)->type);
                action = ACTION_CORRECTION;
            }
        } else {
            action = ACTION_REMOVED;
            while (it != NULL) {
                if (msg->coords[0] == it->coord[0] && msg->coords[1] == it->coord[1]) {
                    if (prev != NULL) {
                        prev->next = it->next;
                    } else {
                        nList->first = it->next;
                    }
                    free(it);
                    break;
                }
                prev = it;
                it = it->next;
            }
        }
    } else {
        if (strcmp(msg->type, ((struct dataT *)data)->msg->type) == 0 && diffCoord == 0) {
            action = ACTION_SAMELOC;
        }
    }

    handleLog(msg, prevMeasurement, ((struct dataT *)data)->msg->measurement, action);
}


void *sendMessages(void *data) {
    int sock = ((struct dataT *)data)->sock;
    int timeout = ((struct dataT *)data)->timeout;
    sensor_message msg;
    msg.coords[0] = ((struct dataT *)data)->msg->coords[0];
    msg.coords[1] = ((struct dataT *)data)->msg->coords[1];
    strcpy(msg.type, ((struct dataT *)data)->msg->type);

    
    for(;;){
        sleep(timeout);
        msg.measurement = ((struct dataT *)data)->msg->measurement;
        send(sock,&msg,sizeof(msg),0);
    }
}

void *recvMessages(void *data) {
    sensor_message msg;
    int sock = ((struct dataT *)data)->sock;
    
    for(;;){
        int numBytes = recv(sock,&msg,sizeof(msg),0);
        if(numBytes > 0) {
            handleMessagesReceived(&msg,data,((struct dataT *)data)->list);
        }else{

            close(sock);
            exit(1);
        }

    }
}

int main(int argc, char *argv[])
{
    int coords[2]; //[x,y]
    int type = 0;
    sensor_message msgToSend;
    float measurement = 0;
    int timeout;
    struct dataT data;
    Neighbors list;
    
    if (argc < 8) // Test for correct number of arguments
        handleInputErrors("Error: Invalid number of arguments");

    if (strcmp(argv[3], "-type") != 0)
        handleInputErrors("Error: Expected '-type' argument");

    if (strcmp(argv[4], "temperature") == 0){
        type = TEMPERATURE;
        timeout = TIMEOUT_TEMP;
    } else if (strcmp(argv[4], "humidity") == 0){
        type = HUMIDITY;
        timeout = TIMEOUT_HUMI;
    }else if (strcmp(argv[4], "air_quality") == 0){
        type = AIRQUALITY;
        timeout = TIMEOUT_AIRQ;
    } else {
        killClient("type invalid");
    }
    if (type < TEMPERATURE || type > AIRQUALITY) // > 3 or < 1
        handleInputErrors("Error: Invalid sensor type");

    if (strcmp(argv[5], "-coords") != 0)
        handleInputErrors("Error: Expected '-coords' argument");
    
    coords[0] = atoi(argv[6]);
    coords[1] = atoi(argv[7]);

    if ((coords[0] > 9 || coords[0] < 0) || (coords[1] > 9 || coords[1] < 0))
        handleInputErrors("Error: Coordinates must be in the range 0-9");

    char *servIP = argv[1]; // First arg: server IP address (dotted quad)

    // Third arg (optional): server port (numeric). 7 is well-known echo port
    in_port_t servPort = atoi(argv[2]);

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
        killClient("inet_pton() failed\nInvalid IP address format");
    }

    // Create a reliable, stream socket using TCP
    int sock = socket(domain, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0)
        killClient("socket() failed");

    // Establish the connection to the echo server
    if (connect(sock, (struct sockaddr *)&servAddr, servAddrLen) < 0)
        killClient("connect() failed");

    measurement = randomMeasurement(type);
    msgToSend = handleMessage(coords, type, measurement);
    list.first = NULL;

    data.msg = &msgToSend;
    data.timeout = timeout;
    data.sock = sock;
    data.type = type;
    data.list = &list;


    pthread_t sending_thread, receiving_thread;
    pthread_create(&sending_thread,NULL,sendMessages,&data);
    pthread_create(&receiving_thread,NULL,recvMessages,&data);

    for(;;){

    }
    // close(sock);
    // exit(0);
}