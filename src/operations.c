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
#include "../include/universalMethods.h"
#include "../include/operations.h"

#define dataSizeMax 500
#define dataSizeMaxBg 500 * 200
#define STDIN 0

struct message {
    char text[dataSizeMaxBg];
    struct host * from_client;
    struct message * next_message;
    bool is_broadcast;
};

struct host {
    char hostname[dataSizeMax];
    char ip[dataSizeMax];
    char port[dataSizeMax];
    char status[dataSizeMax];
    int num_msg_sent;
    int num_msg_rcv;
    int fd;
    struct host * blocked;
    struct host * next_host;
    bool is_logged_in;
    bool is_server;
    struct message * queued_messages;
};

// INITIALISE GLOBAL VARIABLES
struct host * clientNew = NULL; //to store new clients
struct host * clients = NULL; //to store client details
struct host * myhost = NULL; //to store myhostdetails
struct host * server = NULL; // to store server details
int yes = 1; // sued to set the socket

// HELPER FUNCTIONS
int setHostNameAndIp(struct host * h);
void sendCommand(int fd, char msg[]);


// APPLICATION STARTUP
void initializeServer();
void initializeClient();
int registerClientLIstener();

// COMMAND EXECUTION
void exCommand(char command[], int requesting_client_fd);
void exCommandHost(char command[], int requesting_client_fd);
void exCommandServer(char command[], int requesting_client_fd);
void exCommandClient(char command[]);

// _LIST
void printLoggedInClients();

// LOGIN
int connectClientServer(char server_ip[], char server_port[]);
void loginClient(char server_ip[], char server_port[]);
void loginHandleServer(char client_ip[], char client_port[], char client_hostname[], int requesting_client_fd);

// REFRESH
void clientRefreshClientList(char clientListString[]);
void serverHandleRefresh(int requesting_client_fd);
// SEND
void server__handle_send(char client_ip[], char msg[], int requesting_client_fd);
void client__send(char command[]);
void client__handle_receive(char client_ip[], char msg[]);

// BROADCAST
void server__handle_broadcast(char msg[], int requesting_client_fd);

// BLOCK AND UNBLOCK
void client__block_or_unblock(char command[], bool is_a_block);
void server__block_or_unblock(char command[], bool is_a_block, int requesting_client_fd);

// BLOCKED
void server__print_blocked(char blocker_ip_addr[]);

// LOGOUT
void server__handle_logout(int requesting_client_fd);
// EXIT
void exitServer(int requesting_client_fd);
void exitClient();

// STATISTICS
void server__print_statistics();

/***  Reference : https://ubmnc.wordpress.com/2010/09/22/on-getting-the-ip-name-of-a-machine-for-chatty/ ***/
int setHostNameAndIp(struct host * h) {
    char myIP[16];
    unsigned int myPort;
    struct sockaddr_in server_addr, my_addr;
    int sockfd;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct hostent *he;
    // Set server_addr of Google DNS
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("8.8.8.8"); //IP address of Google DNS
    server_addr.sin_port = htons(53);

    // Connect to server
    connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    // Get my ip address and port
    bzero(&my_addr, sizeof(my_addr));
    socklen_t len = sizeof(my_addr);
    getsockname(sockfd, (struct sockaddr *)&my_addr, &len);
    inet_ntop(AF_INET, &my_addr.sin_addr, myIP, sizeof(myIP));
    he = gethostbyaddr(&my_addr.sin_addr, sizeof(my_addr.sin_addr), AF_INET);

    // Storing my IP and Address in myHost
    memcpy(h->ip, myIP, sizeof(h->ip));
    memcpy(h->hostname, he->h_name, sizeof(h->hostname));
    return 1;
}


//initialize the host
void initialize(bool is_server, char * port) {
    myhost = malloc(sizeof(struct host));
    memcpy(myhost -> port, port, sizeof(myhost -> port));
    myhost -> is_server = is_server;
    setHostNameAndIp(myhost);
    if (is_server) {
        initializeServer();
    } else {
        initializeClient();
    }
}

//handling message to and from server
void sendCommand(int fd, char msg[]) {
    int rv;
    send(fd, msg, strlen(msg) + 1, 0);
}

//initialize the server
void initializeServer() {
    int listening = 0;
    struct addrinfo hints, * localhost_ai, * temp_ai;
    // creating a socket and binding
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
    if (temp_ai == NULL) {
        exit(EXIT_FAILURE);
    }

    // listening
    if (listen(listening, 20) == -1) {
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
    FD_SET(STDIN, & master); 
    int fdmax = listening > STDIN ? listening : STDIN;   

    // main loop
    while (1) {
        fd_set cp_master = master; // make a copy of master set
        int socketCount = select(fdmax + 1, & cp_master, NULL, NULL, NULL) ; // determine status of one or more sockets to perfrom i/o i sync
        if (socketCount == -1) {
            exit(EXIT_FAILURE);
        }
        // looking for data to read
        int fd = 0;
        while(fd <= fdmax) {
            if (FD_ISSET(fd, & cp_master)) {
                // handling new connection request
                if (fd == STDIN) {
                    // handle data from standard input
                    char * command = (char * ) malloc(sizeof(char) * dataSizeMaxBg);
                    memset(command, '\0', dataSizeMaxBg);
                    if (fgets(command, dataSizeMaxBg - 1, stdin) == NULL) { // -1 because of new line
                    } else {
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
                        clientNew -> is_logged_in = true;
                        clientNew -> next_host = NULL;
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

/***  initialize the client ***/
void initializeClient() {
    registerClientLIstener();
    while (true) {
        // handling input
        char * command = (char * ) malloc(sizeof(char) * dataSizeMaxBg);
        memset(command, '\0', dataSizeMaxBg);
        if (fgets(command, dataSizeMaxBg, stdin) != NULL) {
            exCommand(command, STDIN);
        }
    }
}

/*** initialize client listening ***/
int registerClientLIstener() {
    int listening = 0;
    struct addrinfo hints, * localhost_ai, * temp_ai;

    // create a socket and bind
    memset( & hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int error = getaddrinfo(NULL, myhost -> port, & hints, & localhost_ai);
    if (error != 0) {
        exit(EXIT_FAILURE);
    }
    for (temp_ai = localhost_ai; temp_ai != NULL; temp_ai = temp_ai -> ai_next) {
        listening = socket(temp_ai -> ai_family, temp_ai -> ai_socktype, temp_ai -> ai_protocol);
        if (listening < 0) {
            continue;
        }
        setsockopt(listening, SOL_SOCKET, SO_REUSEPORT, & yes, sizeof(int));
        setsockopt(listening, SOL_SOCKET, SO_REUSEADDR, & yes, sizeof(int));
        if (bind(listening, temp_ai -> ai_addr, temp_ai -> ai_addrlen) < 0) {
            close(listening);
            continue;
        }
        break;
    }

    // exiting
    if (temp_ai == NULL) {
        exit(EXIT_FAILURE);
    }

    // listening
    if (listen(listening, 10) == -1) {
        exit(EXIT_FAILURE);
    }

    myhost -> fd = listening;

    freeaddrinfo(localhost_ai);
}

/***  EXECUTE COMMANDS ***/
void exCommand(char command[], int requesting_client_fd){
    exCommandHost(command, requesting_client_fd);
    if (myhost -> is_server) {
        exCommandServer(command, requesting_client_fd);
    } else {
        exCommandClient(command);
    }
    fflush(stdout);
}

//executing universal commands
void exCommandHost(char command[], int requesting_client_fd) {
    if (strstr(command, "AUTHOR") != NULL) {
        printAuthor("skumar45");
    } else if (strstr(command, "IP") != NULL) {
        displayIp(myhost->ip);
    } else if (strstr(command, "PORT") != NULL) {
        displayPort(myhost -> port);
    }
    fflush(stdout);
}

//executing server commands
void exCommandServer(char command[], int requesting_client_fd) {
    if (strstr(command, "LIST") != NULL) {
        printLoggedInClients();
    }  else if (strstr(command, "STATISTICS") != NULL) {
        server__print_statistics();
    } else if (strstr(command, "BLOCKED") != NULL) {
        char client_ip[dataSizeMax];
        sscanf(command, "BLOCKED %s", client_ip);
        server__print_blocked(client_ip);
    } else if (strstr(command, "LOGIN") != NULL) {
        char client_hostname[dataSizeMax], client_port[dataSizeMax], client_ip[dataSizeMax];
        sscanf(command, "LOGIN %s %s %s", client_ip, client_port, client_hostname);
        loginHandleServer(client_ip, client_port, client_hostname, requesting_client_fd);
    } else if (strstr(command, "BROADCAST") != NULL) {
        char message[dataSizeMaxBg];
        int cmdi = 10;
        int msgi = 0;
        while (command[cmdi] != '\0') {
            message[msgi] = command[cmdi];
            cmdi += 1;
            msgi += 1;
        }
        message[msgi - 1] = '\0';
        server__handle_broadcast(message, requesting_client_fd);
    } else if (strstr(command, "REFRESH") != NULL) {
        serverHandleRefresh(requesting_client_fd);
    } else if (strstr(command, "SEND") != NULL) {
        char client_ip[dataSizeMax], message[dataSizeMax];
        int cmdi = 5;
        int ipi = 0;
        while (command[cmdi] != ' ') {
            client_ip[ipi] = command[cmdi];
            cmdi += 1;
            ipi += 1;
        }
        client_ip[ipi] = '\0';
        cmdi++;
        int msgi = 0;
        while (command[cmdi] != '\0') {
            message[msgi] = command[cmdi];
            cmdi += 1;
            msgi += 1;
        }
        message[msgi - 1] = '\0'; // Remove new line
        server__handle_send(client_ip, message, requesting_client_fd);
    } else if (strstr(command, "UNBLOCK") != NULL) {
        server__block_or_unblock(command, false, requesting_client_fd);
    } else if (strstr(command, "BLOCK") != NULL) {
        server__block_or_unblock(command, true, requesting_client_fd);
    } else if (strstr(command, "LOGOUT") != NULL) {
        server__handle_logout(requesting_client_fd);
    }else if (strstr(command, "EXIT") != NULL) {
        exitServer(requesting_client_fd);
    }
    fflush(stdout);
}

/*** executing all the commands for clients ***/
void exCommandClient(char command[]) {
    if (strstr(command, "LIST") != NULL) {
        if (myhost -> is_logged_in) {
            printLoggedInClients();
        } else {
            cse4589_print_and_log("[LIST:ERROR]\n");
            cse4589_print_and_log("[LIST:END]\n");
        }
    } else if (strstr(command, "SUCCESSLOGIN") != NULL) {
        cse4589_print_and_log("[LOGIN:SUCCESS]\n");
        cse4589_print_and_log("[LOGIN:END]\n");
    } else if (strstr(command, "ERRORLOGIN") != NULL) {
        cse4589_print_and_log("[LOGIN:ERROR]\n");
        cse4589_print_and_log("[LOGIN:END]\n");
    }  else if (strstr(command, "SUCCESSLOGOUT") != NULL) {
        myhost-> is_logged_in = false;
        cse4589_print_and_log("[LOGOUT:SUCCESS]\n");
        cse4589_print_and_log("[LOGOUT:END]\n");
    } else if (strstr(command, "ERRORLOGOUT") != NULL) {
        cse4589_print_and_log("[LOGOUT:ERROR]\n");
        cse4589_print_and_log("[LOGOUT:END]\n");
    } else if (strstr(command, "SUCCESSBROADCAST") != NULL) {
        cse4589_print_and_log("[BROADCAST:SUCCESS]\n");
        cse4589_print_and_log("[BROADCAST:END]\n");
    } else if (strstr(command, "SUCCESSUNBLOCK") != NULL) {
        cse4589_print_and_log("[UNBLOCK:SUCCESS]\n");
        cse4589_print_and_log("[UNBLOCK:END]\n");
    } else if (strstr(command, "SUCCESSBLOCK") != NULL) {
        cse4589_print_and_log("[BLOCK:SUCCESS]\n");
        cse4589_print_and_log("[BLOCK:END]\n");
    } else if (strstr(command, "ERRORUNBLOCK") != NULL) {
        cse4589_print_and_log("[UNBLOCK:ERROR]\n");
        cse4589_print_and_log("[UNBLOCK:END]\n");
    } else if (strstr(command, "ERRORBLOCK") != NULL) {
        cse4589_print_and_log("[BLOCK:ERROR]\n");
        cse4589_print_and_log("[BLOCK:END]\n");
    } else if (strstr(command, "SUCCESSSEND") != NULL) {
        cse4589_print_and_log("[SEND:SUCCESS]\n");
        cse4589_print_and_log("[SEND:END]\n");
    } else if (strstr(command, "LOGIN") != NULL) { // takes two arguments server ip and server port
        char server_ip[dataSizeMax], server_port[dataSizeMax];
        int cmdi = 6;
        int ipi = 0;
        while (command[cmdi] != ' ' && ipi < 256) {
            server_ip[ipi] = command[cmdi];
            cmdi += 1;
            ipi += 1;
        }
        server_ip[ipi] = '\0';

        cmdi += 1;
        int pi = 0;
        while (command[cmdi] != '\0') {
            server_port[pi] = command[cmdi];
            cmdi += 1;
            pi += 1;
        }
        server_port[pi - 1] = '\0'; // REMOVE THE NEW LINE
        loginClient(server_ip, server_port);
    } else if (strstr(command, "REFRESHRESPONSE") != NULL) {
        clientRefreshClientList(command);
    } else if (strstr(command, "REFRESH") != NULL) {
        if (myhost -> is_logged_in) {
            sendCommand(server -> fd, "REFRESH\n");
        } else {
            cse4589_print_and_log("[REFRESH:ERROR]\n");
            cse4589_print_and_log("[REFRESH:END]\n");
        }
    } else if (strstr(command, "SEND") != NULL) {
        if (myhost -> is_logged_in) {
            client__send(command);
        } else {
            cse4589_print_and_log("[SEND:ERROR]\n");
            cse4589_print_and_log("[SEND:END]\n");
        }
    } else if (strstr(command, "RECEIVE") != NULL) {
        char client_ip[dataSizeMax], message[dataSizeMaxBg];
        int cmdi = 8;
        int ipi = 0;
        while (command[cmdi] != ' ' && ipi < 256) {
            client_ip[ipi] = command[cmdi];
            cmdi += 1;
            ipi += 1;
        }
        client_ip[ipi] = '\0';

        cmdi += 1;
        int msgi = 0;
        while (command[cmdi] != '\0') {
            message[msgi] = command[cmdi];
            cmdi += 1;
            msgi += 1;
        }
        message[msgi - 1] = '\0'; // REMOVE THE NEW LINE
        client__handle_receive(client_ip, message);
    } else if (strstr(command, "BROADCAST") != NULL) {
        if (myhost-> is_logged_in) {
            sendCommand(server -> fd, command);
        } else {
            cse4589_print_and_log("[BROADCAST:ERROR]\n");
            cse4589_print_and_log("[BROADCAST:END]\n");
        }
    } else if (strstr(command, "UNBLOCK") != NULL) {
        if (myhost-> is_logged_in) {
            client__block_or_unblock(command, false);
        } else {
            cse4589_print_and_log("[UNBLOCK:ERROR]\n");
            cse4589_print_and_log("[UNBLOCK:END]\n");
        }
    } else if (strstr(command, "BLOCK") != NULL) {
        if (myhost-> is_logged_in) {
            client__block_or_unblock(command, true);
        } else {
            cse4589_print_and_log("[BLOCK:ERROR]\n");
            cse4589_print_and_log("[BLOCK:END]\n");
        }
    } else if (strstr(command, "LOGOUT") != NULL) {
        if (myhost-> is_logged_in) {
            sendCommand(server -> fd, command);
        } else {
            cse4589_print_and_log("[LOGOUT:ERROR]\n");
            cse4589_print_and_log("[LOGOUT:END]\n");
        }
    }else if (strstr(command, "EXIT") != NULL) {
        exitClient();
    }
    fflush(stdout);
}

/***  display all the logged in clients ***/
void printLoggedInClients() {
    cse4589_print_and_log("[LIST:SUCCESS]\n");

    struct host * temp = clients;
    int id = 1;
    while (temp != NULL) {
        // refresh
        if (temp -> is_logged_in) {
            cse4589_print_and_log("%-5d%-35s%-20s%-8s\n", id, temp -> hostname, temp -> ip, (temp -> port));
            id = id + 1;
        }
        temp = temp -> next_host;
    }

    cse4589_print_and_log("[LIST:END]\n");
}
/***  PRINT STATISTICS ***/
void server__print_statistics() {
    cse4589_print_and_log("[STATISTICS:SUCCESS]\n");

    struct host * temp = clients;
    int id = 1;
    while (temp != NULL) {
        cse4589_print_and_log("%-5d%-35s%-8d%-8d%-8s\n", id, temp -> hostname, temp -> num_msg_sent, temp -> num_msg_rcv, temp -> is_logged_in ? "logged-in" : "logged-out");
        id = id + 1;
        temp = temp -> next_host;
    }

    cse4589_print_and_log("[STATISTICS:END]\n");
}

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
    for (temp_ai = server_ai; temp_ai != NULL; temp_ai = temp_ai -> ai_next) {
        server_fd = socket(temp_ai -> ai_family, temp_ai -> ai_socktype, temp_ai -> ai_protocol);
        if (server_fd < 0) {
            continue;
        }
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, & yes, sizeof(int));
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, & yes, sizeof(int));
        if (connect(server_fd, temp_ai -> ai_addr, temp_ai -> ai_addrlen) < 0) {
            close(server_fd);
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
    for (temp_ai = localhost_ai; temp_ai != NULL; temp_ai = temp_ai -> ai_next) {
        listening = socket(temp_ai -> ai_family, temp_ai -> ai_socktype, temp_ai -> ai_protocol);
        if (listening < 0) {
            continue;
        }
        setsockopt(listening, SOL_SOCKET, SO_REUSEPORT, & yes, sizeof(int));
        setsockopt(listening, SOL_SOCKET, SO_REUSEADDR, & yes, sizeof(int));

        if (bind(listening, temp_ai -> ai_addr, temp_ai -> ai_addrlen) < 0) {
            close(listening);
            continue;
        }
        break;
    }

    // exiting
    if (temp_ai == NULL) {
        return 0;
    }

    // listening
    if (listen(listening, 10) == -1) {
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

    // myhost login succesfull
    // client sync with server
    // At this point the myhost has successfully logged in
    // we need to make sure everything reflects this

    // The client will send a login message to server with it's details here
    myhost -> is_logged_in = true;
    char msg[dataSizeMax * 4];
    sprintf(msg, "LOGIN %s %s %s\n", myhost -> ip, myhost -> port, myhost -> hostname);
    sendCommand(server -> fd, msg);

    // Now we have a server_fd. We add it to he master list of fds along with stdin.
    fd_set master; // master file descriptor list
    fd_set cp_master; // temp file descriptor list for select()
    FD_ZERO( & master); // clear the master and temp sets
    FD_ZERO( & cp_master);
    FD_SET(server -> fd, & master); // Add server->fd to the master list
    FD_SET(STDIN, & master); // Add STDIN to the master list
    FD_SET(myhost -> fd, & master);
    int fdmax = server -> fd > STDIN ? server -> fd : STDIN; // maximum file descriptor number. initialised to listening    
    fdmax = fdmax > myhost -> fd ? fdmax : myhost -> fd;
    // variable initialisations
    char data_buffer[dataSizeMaxBg]; // buffer for client data
    int dataRcvd; // holds number of bytes received and stored in data_buffer
    int fd;
    struct sockaddr_storage new_peer_addr; // client address
    socklen_t addrlen = sizeof new_peer_addr;

    // main loop
    while (myhost -> is_logged_in) {
        cp_master = master; // make a copy of master set
        if (select(fdmax + 1, & cp_master, NULL, NULL, NULL) == -1) {
            exit(EXIT_FAILURE);
        }

        // run through the existing connections looking for data to read
        for (fd = 0; fd <= fdmax; fd++) {
            if (FD_ISSET(fd, & cp_master)) {
                // if fd == listening, a new connection has come in.

                if (fd == server -> fd) {
                    // handle data from the server
                    dataRcvd = recv(fd, data_buffer, sizeof data_buffer, 0);
                    if (dataRcvd == 0) {
                        close(fd); // Close the connection
                        FD_CLR(fd, & master); // Remove the fd from master set
                    } else if (dataRcvd == -1) {
                        close(fd); // Close the connection
                        FD_CLR(fd, & master); // Remove the fd from master set
                    } else {
                        exCommand(data_buffer, fd);
                    }
                } else if (fd == STDIN) {
                    // handle data from standard input
                    char * command = (char * ) malloc(sizeof(char) * dataSizeMaxBg);
                    memset(command, '\0', dataSizeMaxBg);
                    if (fgets(command, dataSizeMaxBg - 1, stdin) != NULL) {
                        exCommand(command, STDIN);
                    }
                }
            }
        }

        fflush(stdout);
    }

    return;

}

/** server handling login and register of client **/
void loginHandleServer(char client_ip[], char client_port[], char client_hostname[], int requesting_client_fd) {
    char client_return_msg[dataSizeMaxBg] = "REFRESHRESPONSE FIRST\n";
    struct host * temp = clients;
    bool is_new = true;
    struct host * requesting_client = malloc(sizeof(struct host));

    while (temp != NULL) {
        if (temp -> fd == requesting_client_fd) {
            requesting_client = temp;
            is_new = false;
            break;
        }
        temp = temp -> next_host;
    }

    if (is_new) {
        memcpy(clientNew -> hostname, client_hostname, sizeof(clientNew -> hostname));
        memcpy(clientNew -> port, client_port, sizeof(clientNew -> port));
        requesting_client = clientNew;
        int client_port_value = atoi(client_port);
        if (clients == NULL) {
            clients = malloc(sizeof(struct host));
            clients = clientNew;
        } else if (client_port_value < atoi(clients -> port)) {
            clientNew -> next_host = clients;
            clients = clientNew;
        } else {
            struct host * temp = clients;
            while (temp -> next_host != NULL && atoi(temp -> next_host -> port) < client_port_value) {
                temp = temp -> next_host;
            }
            clientNew -> next_host = temp -> next_host;
            temp -> next_host = clientNew;
        }

    } else {
        requesting_client -> is_logged_in = true;
    }

    temp = clients;
    while (temp != NULL) {
        if (temp -> is_logged_in) {
            char clientString[dataSizeMax * 4];
            sprintf(clientString, "%s %s %s\n", temp -> ip, temp -> port, temp -> hostname);
            strcat(client_return_msg, clientString);
        }
        temp = temp -> next_host;
    }

    strcat(client_return_msg, "ENDREFRESH\n");
    struct message * temp_message = requesting_client -> queued_messages;
    char receive[dataSizeMax * 3];

    while (temp_message != NULL) {
        requesting_client -> num_msg_rcv++;
        sprintf(receive, "RECEIVE %s %s    ", temp_message -> from_client -> ip, temp_message -> text);
        strcat(client_return_msg, receive);

        if (!temp_message -> is_broadcast) {
            cse4589_print_and_log("[RELAYED:SUCCESS]\n");
            cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", temp_message -> from_client -> ip, requesting_client -> ip, temp_message -> text);
            cse4589_print_and_log("[RELAYED:END]\n");
        }
        temp_message = temp_message -> next_message;
    }
    sendCommand(requesting_client_fd, client_return_msg);
    requesting_client -> queued_messages = temp_message;
}

// refreshing the client list
void clientRefreshClientList(char clientListString[]) {
    char * received = strstr(clientListString, "RECEIVE");
    int rcvi = received - clientListString, cmdi = 0;
    char command[dataSizeMax];
    int blank_count = 0;
    while (received != NULL && rcvi < strlen(clientListString)) {
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
        char client_ip[dataSizeMax], client_port[dataSizeMax], client_hostname[dataSizeMax];
        while (token != NULL) {
            if (strstr(token, "ENDREFRESH") != NULL) {
                break;
            }
            struct host * clientNew = malloc(sizeof(struct host));
            sscanf(token, "%s %s %s\n", client_ip, client_port, client_hostname);
            token = strtok(NULL, delimmiter);
            memcpy(clientNew -> port, client_port, sizeof(clientNew -> port));
            memcpy(clientNew -> ip, client_ip, sizeof(clientNew -> ip));
            memcpy(clientNew -> hostname, client_hostname, sizeof(clientNew -> hostname));
            clientNew -> is_logged_in = true;
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

//server handling the request to refresh the client
void serverHandleRefresh(int requesting_client_fd) {
    char clientListString[dataSizeMaxBg] = "REFRESHRESPONSE NOTFIRST\n";
    struct host * temp = clients;
    while (temp != NULL) {
        if (temp -> is_logged_in) {
            char clientString[dataSizeMax * 4];
            sprintf(clientString, "%s %s %s\n", temp -> ip, temp -> port, temp -> hostname);
            strcat(clientListString, clientString);
        }
        temp = temp -> next_host;
    }
    strcat(clientListString, "ENDREFRESH\n");
    sendCommand(requesting_client_fd, clientListString);
}
/** SERVER HANDLE SEND REQUEST FROM CLIENTS **/
void server__handle_send(char client_ip[], char msg[], int requesting_client_fd) {

    char receive[dataSizeMax * 4];
    struct host * temp = clients;
    struct host * from_client = malloc(sizeof(struct host)), * to_client = malloc(sizeof(struct host));;
    while (temp != NULL) {
        if (strstr(client_ip, temp -> ip) != NULL) {
            to_client = temp;
        }
        if (requesting_client_fd == temp -> fd) {
            from_client = temp;
        }
        temp = temp -> next_host;
    }
    if (to_client == NULL || from_client == NULL) {
        // TODO: CHECK IF THIS IS REQUIRED
        cse4589_print_and_log("[RELAYED:ERROR]\n");
        cse4589_print_and_log("[RELAYED:END]\n");

        return;
    }

    from_client -> num_msg_sent++;
    // CHECK IF SENDER IS BLOCKED (FROM IS BLOCKED BY TO)

    bool is_blocked = false;

    temp = to_client -> blocked;
    while (temp != NULL) {
        if (strstr(from_client -> ip, temp -> ip) != NULL) {
            is_blocked = true;
            break;
        }
        temp = temp -> next_host;
    }
    sendCommand(from_client -> fd, "SUCCESSSEND\n");
    if (is_blocked) {
        cse4589_print_and_log("[RELAYED:SUCCESS]\n");
        cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", from_client -> ip, to_client -> ip, msg);
        cse4589_print_and_log("[RELAYED:END]\n");
        sendCommand(from_client -> fd, "SUCCESSSEND\n");
        return;
    }

    if (to_client -> is_logged_in) {
        to_client -> num_msg_rcv++;
        sprintf(receive, "RECEIVE %s %s\n", from_client -> ip, msg);
        sendCommand(to_client -> fd, receive);

        // TODO: CHECK IF THIS NEEDS TO BE SENT WHEN BLOCKED
        cse4589_print_and_log("[RELAYED:SUCCESS]\n");
        cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", from_client -> ip, to_client -> ip, msg);
        cse4589_print_and_log("[RELAYED:END]\n");
    } else {
        struct message * new_message = malloc(sizeof(struct message));
        memcpy(new_message -> text, msg, sizeof(new_message -> text));
        new_message -> from_client = from_client;
        new_message -> is_broadcast = false;
        if (to_client -> queued_messages == NULL) {
            to_client -> queued_messages = new_message;
        } else {
            struct message * temp_message = to_client -> queued_messages;
            while (temp_message -> next_message != NULL) {
                temp_message = temp_message -> next_message;
            }
            temp_message -> next_message = new_message;
        }
    }

}

/** CLIENT SEND MESSAGE **/
void client__send(char command[]) {
    char client_ip[dataSizeMax];
    int cmdi = 5;
    int ipi = 0;
    while (command[cmdi] != ' ') {
        client_ip[ipi] = command[cmdi];
        cmdi += 1;
        ipi += 1;
    }
    client_ip[ipi] = '\0';
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, client_ip, & (sa.sin_addr));
    if (result!=0) {
        cse4589_print_and_log("[SEND:ERROR]\n");
        cse4589_print_and_log("[SEND:END]\n");
        return;
    }
    struct host * temp = clients;
    while (temp != NULL) {
        if (strstr(temp -> ip, client_ip) != NULL) {
            sendCommand(server -> fd, command);
            break;
        }
        temp = temp -> next_host;
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

/** SERVER HANDLE BROADCAST REQUEST FROM CLIENT **/
void server__handle_broadcast(char msg[], int requesting_client_fd) {
    struct host * temp = clients;
    struct host * from_client = malloc(sizeof(struct host));
    while (temp != NULL) {
        if (requesting_client_fd == temp -> fd) {
            from_client = temp;
        }
        temp = temp -> next_host;
    }
    struct host * to_client = clients;
    int id = 1;
    from_client -> num_msg_sent++;
    while (to_client != NULL) {
        if (to_client -> fd == requesting_client_fd) {
            to_client = to_client -> next_host;
            continue;
        }

        bool is_blocked = false;

        struct host * temp_blocked = to_client -> blocked;
        while (temp_blocked != NULL) {
            if (temp_blocked -> fd == requesting_client_fd) {
                is_blocked = true;
                break;
            }
            temp_blocked = temp_blocked -> next_host;
        }

        if (is_blocked) {
            to_client = to_client -> next_host;
            continue;
        }

        char receive[dataSizeMax * 4];

        if (to_client -> is_logged_in) {
            to_client -> num_msg_rcv++;
            sprintf(receive, "RECEIVE %s %s\n", from_client -> ip, msg);
            sendCommand(to_client -> fd, receive);
        } else {
            struct message * new_message = malloc(sizeof(struct message));
            memcpy(new_message -> text, msg, sizeof(new_message -> text));
            new_message -> from_client = from_client;
            new_message -> is_broadcast = true;
            if (to_client -> queued_messages == NULL) {
                to_client -> queued_messages = new_message;
            } else {
                struct message * temp_message = to_client -> queued_messages;
                while (temp_message -> next_message != NULL) {
                    temp_message = temp_message -> next_message;
                }
                temp_message -> next_message = new_message;
            }
        }
        to_client = to_client -> next_host;
    }

    cse4589_print_and_log("[RELAYED:SUCCESS]\n");
    cse4589_print_and_log("msg from:%s, to:255.255.255.255\n[msg]:%s\n", from_client ->ip, msg);
    cse4589_print_and_log("[RELAYED:END]\n");
    sendCommand(from_client -> fd, "SUCCESSBROADCAST\n");
}

/** BLOCK OR UNBLOCK **/
void client__block_or_unblock(char command[], bool is_a_block) {
    char client_ip[dataSizeMax];
    if (is_a_block) {
        sscanf(command, "BLOCK %s\n", client_ip);
    } else {
        sscanf(command, "UNBLOCK %s\n", client_ip);
    }

    // To check if its in the LIST
    struct host * temp = clients;
    while (temp != NULL) {
        if (strstr(client_ip, temp -> ip) != NULL) {
            break;
        }
        temp = temp -> next_host;
    }
    struct host * blocked_client = temp;

    // To check if it's already blocked
    temp = myhost -> blocked;
    while (temp != NULL) {
        if (strstr(client_ip, temp -> ip) != NULL) {
            break;
        }
        temp = temp -> next_host;
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
            while (temp_blocked -> next_host != NULL) {
                temp_blocked = temp_blocked -> next_host;
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
            while (temp_blocked != NULL) {
                if (strstr(temp_blocked -> ip, blocked_client -> ip) != NULL) {
                    previous -> next_host = temp_blocked -> next_host;
                    break;
                }
                temp_blocked = temp_blocked -> next_host;
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

/** SERVER HANDLE BLOCK OR UNBLOCK REQUEST FROM CLIENTS **/
void server__block_or_unblock(char command[], bool is_a_block, int requesting_client_fd) {
    char client_ip[dataSizeMax], client_port[dataSizeMax];;
    if (is_a_block) {
        sscanf(command, "BLOCK %s %s\n", client_ip, client_port);
    } else {
        sscanf(command, "UNBLOCK %s %s\n", client_ip, client_port);
    }
    struct host * temp = clients;
    struct host * requesting_client = malloc(sizeof(struct host));
    struct host * blocked_client = malloc(sizeof(struct host));

    while (temp != NULL) {
        if (temp -> fd == requesting_client_fd) {
            requesting_client = temp;
        }
        if (strstr(client_ip, temp -> ip) != NULL) {
            blocked_client = temp;
        }
        temp = temp -> next_host;
    }

    if (blocked_client != NULL) {
        if (is_a_block) {
            struct host * new_blocked_client = malloc(sizeof(struct host));
            memcpy(new_blocked_client -> ip, blocked_client -> ip, sizeof(new_blocked_client -> ip));
            memcpy(new_blocked_client -> port, blocked_client -> port, sizeof(new_blocked_client -> port));
            memcpy(new_blocked_client -> hostname, blocked_client -> hostname, sizeof(new_blocked_client -> hostname));
            new_blocked_client -> fd = blocked_client -> fd;
            new_blocked_client -> next_host = NULL;
            int new_blocked_client_port_value = atoi(new_blocked_client -> port);
            if (requesting_client -> blocked == NULL) {
                requesting_client -> blocked = malloc(sizeof(struct host));
                requesting_client -> blocked = new_blocked_client;
            } else if (new_blocked_client_port_value < atoi(requesting_client -> blocked -> port)) {
                new_blocked_client -> next_host = requesting_client -> blocked;
                requesting_client -> blocked = new_blocked_client;
            } else {
                struct host * temp = requesting_client -> blocked;
                while (temp -> next_host != NULL && atoi(temp -> next_host -> port) < new_blocked_client_port_value) {
                    temp = temp -> next_host;
                }
                new_blocked_client -> next_host = temp -> next_host;
                temp -> next_host = new_blocked_client;
            }

            sendCommand(requesting_client_fd, "SUCCESSBLOCK\n");
        } else {
            struct host * temp_blocked = requesting_client -> blocked;
            if (strstr(temp_blocked ->ip, blocked_client ->ip) != NULL) {
                requesting_client -> blocked = requesting_client -> blocked -> next_host;
            } else {
                struct host * previous = temp_blocked;
                while (temp_blocked != NULL) {
                    if (strstr(temp_blocked ->ip, blocked_client ->ip) != NULL) {
                        previous -> next_host = temp_blocked -> next_host;
                        break;
                    }
                    temp_blocked = temp_blocked -> next_host;
                }
            }
            sendCommand(requesting_client_fd, "SUCCESSUNBLOCK\n");
        }
    } else {
        if (is_a_block) {
            sendCommand(requesting_client_fd, "ERRORBLOCK\n");
        } else {
            sendCommand(requesting_client_fd, "ERRORUNBLOCK\n");
        }
    }
}

/***  PRINT BLOCKED ***/
void server__print_blocked(char blocker_ip_addr[]) {
    struct host * temp = clients;
    while (temp != NULL) {
        if (strstr(blocker_ip_addr, temp ->ip) != NULL) {
            break;
        }
        temp = temp -> next_host;
    }
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, blocker_ip_addr, & (sa.sin_addr));
    if (result==0 && temp) {
        cse4589_print_and_log("[BLOCKED:SUCCESS]\n");
        struct host * temp_blocked = clients;
        temp_blocked = temp -> blocked;
        int id = 1;
        while (temp_blocked != NULL) {
            cse4589_print_and_log("%-5d%-35s%-20s%-8d\n", id, temp_blocked -> hostname, temp_blocked ->ip, atoi(temp_blocked -> port));
            id = id + 1;
            temp_blocked = temp_blocked -> next_host;
        }
    } else {
        cse4589_print_and_log("[BLOCKED:ERROR]\n");
    }

    cse4589_print_and_log("[BLOCKED:END]\n");
}

/** SERVER HANDLE LOGOUT REQUEST FROM CLIENT **/
void server__handle_logout(int requesting_client_fd) {
    struct host * temp = clients;
    while (temp != NULL) {
        if (temp -> fd == requesting_client_fd) {
            sendCommand(requesting_client_fd, "SUCCESSLOGOUT\n");
            temp -> is_logged_in = false;
            break;
        }
        temp = temp -> next_host;
    }
    if (temp == NULL) {
        sendCommand(requesting_client_fd, "ERRORLOGOUT\n");
    }
}

//exiting the client
void exitClient() {
    sendCommand(server -> fd, "EXIT");
    cse4589_print_and_log("[EXIT:SUCCESS]\n");
    cse4589_print_and_log("[EXIT:END]\n");
    exit(0);
}

//server handling the request too exit the client
void exitServer(int requesting_client_fd) {
    struct host * temp = clients;
    if (temp -> fd == requesting_client_fd) {
        clients = clients -> next_host;
    } else {
        struct host * previous = temp;
        while (temp != NULL) {
            if (temp -> fd == requesting_client_fd) {
                previous -> next_host = temp -> next_host;
                temp = temp -> next_host;
                break;
            }
            temp = temp -> next_host;
        }
    }
}