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
#include "../include/structs.h"
#include "../include/globalVariables.h"
#include "../include/send.h"
#include "../include/clientHelper.h"
#include "../include/send.h"
#include "../include/global.h"
#include "../include/logger.h"
#include "../include/executeCommands.h"

/*** connect sever and client***/
int connectClientServer(char server_ip[], char server_port[]) {
    server = malloc(sizeof(struct host));
    memcpy(server -> ip, server_ip, sizeof(server -> ip));
    memcpy(server -> port, server_port, sizeof(server -> port));
    int server_fd = 0;
    struct addrinfo hints, * server_ai, * temp_ai;

    // create a socket and bind
    memset( & hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int error = getaddrinfo(server -> ip, server -> port, & hints, & server_ai);
    if (error != 0) {
        return 0;
    }
    temp_ai = server_ai;
    while(temp_ai != NULL) {
        server_fd = socket(temp_ai -> ai_family, temp_ai -> ai_socktype, temp_ai -> ai_protocol);
        if (server_fd < 0) {
            temp_ai = temp_ai -> ai_next;
            continue;
        }
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, & yes, sizeof(int));
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, & yes, sizeof(int));
        if (connect(server_fd, temp_ai -> ai_addr, temp_ai -> ai_addrlen) < 0) {
            close(server_fd);
            temp_ai = temp_ai -> ai_next;
            continue;
        }
        break;
    }

    // exiting if unsuccessfull bind
    if (temp_ai == NULL) {
        return 0;
    }

    server -> fd = server_fd;

    freeaddrinfo(server_ai);

    int listening = 0;
    struct addrinfo * localhost_ai;
    error = getaddrinfo(NULL, myhost -> port, & hints, & localhost_ai);
    if (error != 0) {
        return 0;
    }
    temp_ai = localhost_ai;
    while (temp_ai != NULL) {
        listening = socket(temp_ai -> ai_family, temp_ai -> ai_socktype, temp_ai -> ai_protocol);
        if (listening ==-1) {
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

    // exiting
    if (temp_ai == NULL || listen(listening, 10) == -1) {
        return 0;
    }
    myhost -> fd = listening;
    freeaddrinfo(localhost_ai);

    return 1;
}

/** client login **/
void loginClient(char server_ip[], char server_port[]) {

    // Rgeister if its first time
    if (server_ip == NULL || server_port == NULL) {
        cse4589_print_and_log("[LOGIN:ERROR]\n");
        cse4589_print_and_log("[LOGIN:END]\n");
        return;
    }
    if (server == NULL) {
        struct sockaddr_in sa;
        int result = inet_pton(AF_INET, server_ip, & (sa.sin_addr));
        if (result==0 || connectClientServer(server_ip, server_port) == 0) {
            cse4589_print_and_log("[LOGIN:ERROR]\n");
            cse4589_print_and_log("[LOGIN:END]\n");
            return;
        }
    } else {
        if (strstr(server -> ip, server_ip) == NULL || strstr(server -> port, server_port) == NULL) {
            cse4589_print_and_log("[LOGIN:ERROR]\n");
            cse4589_print_and_log("[LOGIN:END]\n");
            return;
        }
    }

    // The client will send a login message to server with it's details here
    myhost -> loggedIn = true;
    char msg[500 * 4];
    sprintf(msg, "LOGIN %s %s %s\n", myhost -> ip, myhost -> port, myhost -> hostname);
    sendCommand(server -> fd, msg);

    // Now we have a server_fd. We add it to he master list of fds along with stdin.
    fd_set master; // master file descriptor list
    FD_ZERO( & master); // clear the master and temp sets
    FD_SET(server -> fd, & master); // Add server->fd to the master list
    FD_SET(STDIN, & master); // Add STDIN to the master list
    FD_SET(myhost -> fd, & master);
    int fdmax = server -> fd > STDIN ? server -> fd : STDIN; // maximum file descriptor number. initialised to listening    
    fdmax = fdmax > myhost -> fd ? fdmax : myhost -> fd;
    // variable initialisations
    char data_buffer[500*200]; // buffer for client data
    int fd;
    struct sockaddr_storage new_peer_addr; // client address
    socklen_t addrlen = sizeof new_peer_addr;

    // main loop
    while (myhost -> loggedIn) {
        fd_set cp_master ;
        FD_ZERO(& cp_master);
        cp_master= master; // make a copy of master set
        int socketCount = select(fdmax + 1, & cp_master, NULL, NULL, NULL) ; // determine status of one or more sockets to perfrom i/o in sync
        if (socketCount == -1) {
            exit(EXIT_FAILURE);
        }
        int fd = 0;
        while(fd <= fdmax) {
            if (FD_ISSET(fd, & cp_master)) {
                // if fd == listening, a new connection has come in.
                if (fd == STDIN) {
                    // handle data from standard input
                    char * command = (char * ) malloc(sizeof(char) * 500*200);
                    memset(command, '\0', 500*200);
                    if (fgets(command, 500*200 - 1, stdin) != NULL) {
                        exCommand(command, STDIN);
                    }
                }
                else if (fd == server -> fd) {
                    // handle data from the server
                    int dataRcvd = recv(fd, data_buffer, sizeof data_buffer, 0);
                    if (dataRcvd <= 0) {
                        close(fd); // Close the connection
                        FD_CLR(fd, & master); // Remove the fd from master set
                    }else {
                        exCommand(data_buffer, fd);
                    }
                }
            }
        }

        fflush(stdout);
    }

    return;

}

// refreshing the client list
void clientRefreshClientList(char clientListString[]) {
    char * received = strstr(clientListString, "RECEIVE");
    int rcvi = received - clientListString, cmdi = 0;
    char command[500];
    int blank_count = 0;
    for (;received != NULL && rcvi < strlen(clientListString);) {
        if (clientListString[rcvi] == ' ')
            blank_count++;
        else
            blank_count = 0;
        command[cmdi] = clientListString[rcvi];
        if (blank_count == 4) {
            command[cmdi - 3] = '\0';
            strcat(command, "\n");
            exCommandClient(command);
            cmdi = -1;
        }
        cmdi++;
        rcvi++;
    }
    bool is_refresh = false;
    clients = malloc(sizeof(struct host));
    struct host * head = clients;
    const char delimmiter[2] = "\n";
    char * token = strtok(clientListString, delimmiter);
    if (strstr(token, "NOTFIRST")) {
        is_refresh = true;
    }
    if (token != NULL) {
        token = strtok(NULL, delimmiter);
        char client_ip[500], client_port[500], client_hostname[500];
        for (;token != NULL;) {
            if (strstr(token, "ENDREFRESH") != NULL) {
                break;
            }
            struct host * clientNew = malloc(sizeof(struct host));
            sscanf(token, "%s %s %s\n", client_ip, client_port, client_hostname);
            token = strtok(NULL, delimmiter);
            memcpy(clientNew -> port, client_port, sizeof(clientNew -> port));
            memcpy(clientNew -> ip, client_ip, sizeof(clientNew -> ip));
            memcpy(clientNew -> hostname, client_hostname, sizeof(clientNew -> hostname));
            clientNew -> loggedIn = true;
            clients -> next_host = clientNew;
            clients = clients -> next_host;
        }
        clients = head -> next_host;
    }
    if (is_refresh) {
        cse4589_print_and_log("[REFRESH:SUCCESS]\n");
        cse4589_print_and_log("[REFRESH:END]\n");
    } else {
        exCommandClient("SUCCESSLOGIN");
    }
}

/** CLIENT SEND MESSAGE **/
void client__send(char command[]) {
    char client_ip[500];
    int cmdi = 5;
    int ipi = 0;
    for (;command[cmdi] != ' ';) {
        client_ip[ipi] = command[cmdi];
        cmdi += 1;
        ipi += 1;
    }
    client_ip[ipi] = '\0';
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, client_ip, & (sa.sin_addr));
    if (result==0) {
        cse4589_print_and_log("[SEND:ERROR]\n");
        cse4589_print_and_log("[SEND:END]\n");
        return;
    }
    struct host * temp = clients;
    for (;temp != NULL;temp = temp -> next_host) {
        if (strstr(temp -> ip, client_ip) != NULL) {
            sendCommand(server -> fd, command);
            break;
        }
    }
    if (temp == NULL) {
        cse4589_print_and_log("[SEND:ERROR]\n");
        cse4589_print_and_log("[SEND:END]\n");
    }
}

/** CLIENT RECEIVE MESSAGE **/
void client__handle_receive(char client_ip[], char msg[]) {
    cse4589_print_and_log("[RECEIVED:SUCCESS]\n");
    cse4589_print_and_log("msg from:%s\n[msg]:%s\n", client_ip, msg);
    cse4589_print_and_log("[RECEIVED:END]\n");
}

/** BLOCK OR UNBLOCK **/
void client__block_or_unblock(char command[], bool is_a_block) {
    char client_ip[500];
    if (is_a_block) {
        sscanf(command, "BLOCK %s\n", client_ip);
    } else {
        sscanf(command, "UNBLOCK %s\n", client_ip);
    }

    // To check if its in the LIST
    struct host * temp = clients;
    for (;temp != NULL;temp = temp -> next_host) {
        if (strstr(client_ip, temp -> ip) != NULL) {
            break;
        }
    }
    struct host * blocked_client = temp;

    // To check if it's already blocked
    temp = myhost -> blocked;
    for (;temp != NULL;temp = temp -> next_host) {
        if (strstr(client_ip, temp -> ip) != NULL) {
            break;
        }
    }
    struct host * blocked_client_2 = temp;

    if (blocked_client != NULL && blocked_client_2 == NULL && is_a_block) {
        struct host * new_blocked_client = malloc(sizeof(struct host));
        memcpy(new_blocked_client -> ip, blocked_client -> ip, sizeof(new_blocked_client -> ip));
        memcpy(new_blocked_client -> port, blocked_client -> port, sizeof(new_blocked_client -> port));
        memcpy(new_blocked_client -> hostname, blocked_client -> hostname, sizeof(new_blocked_client -> hostname));
        new_blocked_client -> fd = blocked_client -> fd;
        new_blocked_client -> next_host = NULL;
        if (myhost -> blocked != NULL) {
            struct host * temp_blocked = myhost-> blocked;
            for (;temp_blocked -> next_host != NULL;temp_blocked = temp_blocked -> next_host) {
                
            }
            temp_blocked -> next_host = new_blocked_client;
        } else {
            myhost -> blocked = new_blocked_client;
        }
        sendCommand(server -> fd, command);
    } else if (blocked_client != NULL && blocked_client_2 != NULL && !is_a_block) {
        struct host * temp_blocked = myhost -> blocked;
        if (strstr(blocked_client -> ip, temp_blocked -> ip) != NULL) {
            myhost -> blocked = myhost -> blocked -> next_host;
        } else {
            struct host * previous = temp_blocked;
            for (;temp_blocked != NULL;temp_blocked = temp_blocked -> next_host) {
                if (strstr(temp_blocked -> ip, blocked_client -> ip) != NULL) {
                    previous -> next_host = temp_blocked -> next_host;
                    break;
                }
            }
        }
        sendCommand(server -> fd, command);

    } else {
        if (is_a_block) {
            cse4589_print_and_log("[BLOCK:ERROR]\n");
            cse4589_print_and_log("[BLOCK:END]\n");
        } else {
            cse4589_print_and_log("[UNBLOCK:ERROR]\n");
            cse4589_print_and_log("[UNBLOCK:END]\n");
        }
    }
}

//exiting the client
void exitClient() {
    sendCommand(server -> fd, "EXIT");
    cse4589_print_and_log("[EXIT:SUCCESS]\n");
    cse4589_print_and_log("[EXIT:END]\n");
    exit(0);
}
