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
#include "../include/structs.h"
#include "../include/globalVariables.h"
#include "../include/send.h"
#include "../include/executeCommands.h"
#include "../include/server.h"
#include "../include/client.h"

// HELPER FUNCTIONS
int setHostNameAndIp(struct host * h);

//initialize the host
void initialize(bool checkServer, char * port) {
    myhost = malloc(sizeof(struct host));
    memcpy(myhost -> port, port, sizeof(myhost -> port));
    myhost -> checkServer = checkServer;
    setHostNameAndIp(myhost);
    if (checkServer) {
        initializeServer();
    } else {
        initializeClient();
    }
}

/***  Reference : https://ubmnc.wordpress.com/2010/09/22/on-getting-the-ip-name-of-a-machine-for-chatty/ ***/
int setHostNameAndIp(struct host * h) {
    char myIP[16];
    unsigned int myPort;
    struct sockaddr_in server_addr;
    struct sockaddr_in my_addr;
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