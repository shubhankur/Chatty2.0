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
#include "../include/server.h"

//initialize the server
void initializeServer() {
    int listening = 0;
    struct addrinfo hints, * localhost_ai, * temp_ai;
    memset( & hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int error = getaddrinfo(NULL, myhost -> port, & hints, & localhost_ai);
    if (error != 0) {
        exit(EXIT_FAILURE);
    }
    temp_ai = localhost_ai;
    while(temp_ai != NULL){
        //Creating listening socket
        listening = socket(temp_ai -> ai_family, temp_ai -> ai_socktype, temp_ai -> ai_protocol);
        if (listening == -1) {
            temp_ai = temp_ai -> ai_next;
            continue;
        }
        setsockopt(listening, SOL_SOCKET, SO_REUSEPORT, & yes, sizeof(int));
        setsockopt(listening, SOL_SOCKET, SO_REUSEADDR, & yes, sizeof(int));
        //Binding the listening socket
        if (bind(listening, temp_ai -> ai_addr, temp_ai -> ai_addrlen) < 0) {
            close(listening);
            temp_ai = temp_ai -> ai_next;
            continue;
        }
        break;
    }
    if (temp_ai == NULL || listen(listening,20)==-1) {
        exit(EXIT_FAILURE);
    }

    // assigning to myhost file descriptor
    myhost -> fd = listening;
    freeaddrinfo(localhost_ai);

    //  initialising variables
    int clientNewFd; 
    struct sockaddr_storage newClientAddr; 
    socklen_t addrlen;
    char newClientIP[INET6_ADDRSTRLEN]; 
    // add the listening fd to master fd
    fd_set master; 
    FD_ZERO( & master); // clear the master and temp sets
    FD_SET(listening, & master);
    FD_SET(0, & master); 
    int fdmax = listening > 0 ? listening : 0;   

    // main loop
    while (1) {
        fd_set cp_master = master; // make a copy of master set
        int socketCount = select(fdmax + 1, & cp_master, NULL, NULL, NULL) ; // determine status of one or more sockets to perfrom i/o in sync
        if (socketCount == -1) {
            exit(EXIT_FAILURE);
        }
        // looking for data to read
        int fd = 0;
        while(fd <= fdmax) {
            if (FD_ISSET(fd, & cp_master)) {
                if (fd == 0) {
                    // handle data from standard input
                    char * command = (char * ) malloc(sizeof(char) * 500*200);
                    memset(command, '\0', 500*200);
                    if (fgets(command, 500*200 - 1, 0) != NULL) { // -1 because of new line
                        exCommand(command, fd);
                    }
                    fflush(stdout);
                } else if (fd == listening) {
                    addrlen = sizeof newClientAddr;
                    clientNewFd = accept(listening, (struct sockaddr * ) & newClientAddr, & addrlen);
                    if (clientNewFd != -1) {
                        // registering new client
                        clientNew = malloc(sizeof(struct host));
                        FD_SET(clientNewFd, & master); // add to master set
                        if (clientNewFd > fdmax) { // keep track of the max
                            fdmax = clientNewFd;
                        }
                        struct sockaddr * sa = (struct sockaddr * ) & newClientAddr;
                        if (sa -> sa_family == AF_INET) {
                            memcpy(clientNew -> ip,
                            inet_ntop(
                                newClientAddr.ss_family,
                                &(((struct sockaddr_in * ) sa) -> sin_addr), // even though newClientAddr is of type sockaddr_storage, they can be cast into each other. Refer beej docs.
                                newClientIP,
                                INET6_ADDRSTRLEN
                            ), sizeof(clientNew -> ip));
                        }
                        else{
                            memcpy(clientNew -> ip,
                            inet_ntop(
                                newClientAddr.ss_family,
                                &(((struct sockaddr_in6 * ) sa) -> sin6_addr), // even though newClientAddr is of type sockaddr_storage, they can be cast into each other. Refer beej docs.
                                newClientIP,
                                INET6_ADDRSTRLEN
                            ), sizeof(clientNew -> ip));
                        }
                        clientNew -> fd = clientNewFd;
                        clientNew -> recvMsgCount = 0;
                        clientNew -> sentMsgCount = 0;
                        clientNew -> loggedIn = true;
                        clientNew -> next_host = NULL;
                        clientNew -> blocked = NULL;
                    }
                    fflush(stdout);
                }  else {
                    // handle data from a client
                    char buf[4096];
                    int dataRcvd = recv(fd, buf, sizeof buf, 0);
                    if (dataRcvd <= 0) {
                        close(fd); // Close the connection
                        FD_CLR(fd, & master); // Remove the fd from master set
                    } else {
                        exCommand(buf, fd);
                    }
                    fflush(stdout);
                }
            }
            fd++;
        }
    }
    return;
}
