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
    int fd;
    struct host * next_host;
    bool is_logged_in;
    bool is_server;
};

// INITIALISE GLOBAL VARIABLES
struct host * clientNew = NULL; //to store new clients
struct host * clients = NULL; //to store client details
struct host * myhost = NULL; //to store localhost details
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

// EXIT
void exitServer(int requesting_client_fd);
void exitClient();

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

/** handling message to and from server **/
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
    char data_buffer[dataSizeMaxBg];
    int dataRcvd; 
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
                    dataRcvd = recv(fd, data_buffer, sizeof data_buffer, 0);
                    if (dataRcvd <= 0) {
                        close(fd); // Close the connection
                        FD_CLR(fd, & master); // Remove the fd from master set
                    } else {
                        exCommand(data_buffer, fd);
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

/***  executing all the commands for host ***/
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

/***  executing all the commands for server ***/
void exCommandServer(char command[], int requesting_client_fd) {
    if (strstr(command, "LIST") != NULL) {
        printLoggedInClients();
    } else if (strstr(command, "LOGIN") != NULL) {
        char client_hostname[dataSizeMax], client_port[dataSizeMax], client_ip[dataSizeMax];
        sscanf(command, "LOGIN %s %s %s", client_ip, client_port, client_hostname);
        loginHandleServer(client_ip, client_port, client_hostname, requesting_client_fd);
    } else if (strstr(command, "REFRESH") != NULL) {
        serverHandleRefresh(requesting_client_fd);
    } else if (strstr(command, "EXIT") != NULL) {
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
    } else if (strstr(command, "EXIT") != NULL) {
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

    // Initalisze a listening as well to listen for P2P cibbectuibs
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
                } else if (fd == myhost -> fd) {

                    int new_peer_fd = accept(fd, (struct sockaddr * ) & new_peer_addr, & addrlen);
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
    sendCommand(requesting_client_fd, client_return_msg);
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