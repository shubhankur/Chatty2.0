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
#include "../include/global.h"
#include "../include/logger.h"
#include "../include/executeCommands.h"
#include "../include/structs.h"
#include "../include/globalVariables.h"
#include "../include/client.h"


/***  initialize the client ***/
void initializeClient() {
    registerClientLIstener();
    while (true) {
        // handling input
        char * command = (char * ) malloc(sizeof(char) * 500*200);
        memset(command, '\0', 500*200);
        if (fgets(command, 500*200, stdin) != NULL) {
            exCommand(command, STDIN);
        }
    }
}

/*** initialize client listening ***/
int registerClientLIstener() {
    int listening = 0;
    struct addrinfo hints;
    struct addrinfo * localhost_ai;
    struct addrinfo * temp_ai;

    // create a socket and bind
    memset( & hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int error = getaddrinfo(NULL, myhost -> port, & hints, & localhost_ai);
    if (error != 0) {
        exit(EXIT_FAILURE);
    }
    temp_ai = localhost_ai;
    while (temp_ai != NULL) {
        listening = socket(temp_ai -> ai_family, temp_ai -> ai_socktype, temp_ai -> ai_protocol);
        if (listening == -1) {
            temp_ai = temp_ai -> ai_next;
            continue;
        }
        setsockopt(listening, SOL_SOCKET, SO_REUSEPORT, & yes, sizeof(int));
        setsockopt(listening, SOL_SOCKET, SO_REUSEADDR, & yes, sizeof(int));
        if (bind(listening, temp_ai -> ai_addr, temp_ai -> ai_addrlen) < 0) {
            close(listening);
            temp_ai = temp_ai -> ai_next;
            continue;
        }
        break;
    }

    // exiting
    if (temp_ai == NULL || listen(listening, 10) == -1) {
        exit(EXIT_FAILURE);
    }

    myhost -> fd = listening;

    freeaddrinfo(localhost_ai);
}