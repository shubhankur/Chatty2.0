#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include "../include/send.h"

//handling message to and from server
void sendCommand(int fd, char msg[]) {
    int rv;
    send(fd, msg, strlen(msg) + 1, 0);
}