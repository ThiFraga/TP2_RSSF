#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/time.h>
#include <math.h>

#define TEMPERATURE 1
#define HUMIDITY 2
#define AIRQUALITY 3


typedef struct {
    char type[12];
    int coords[2];
    float measurement;

} sensor_message;

#endif