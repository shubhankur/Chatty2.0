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
#include "../include/structs.h"
#include "../include/globalVariables.h"
#include "../include/executeCommands.h"
#include "../include/universalMethods.h"
#include "../include/serverHelper.h"
#include "../include/send.h"
#include "../include/clientHelper.h"


/***  EXECUTE COMMANDS ***/
void exCommand(char command[], int requesting_client_fd){
    exCommandHost(command, requesting_client_fd);
    if (myhost -> checkServer) {
        exCommandServer(command, requesting_client_fd);
    } else {
        exCommandClient(command);
    }
    fflush(stdout);
}

//executing universal commands
void exCommandHost(char command[], int requesting_client_fd) {
    int len = strlen(command);
    if(command[len-1] == '\n' ){
        command[len-1]=0;
    }
    if (strcmp(command, "AUTHOR") == 0) {
        printAuthor("skumar45");
    } else if (strcmp(command, "IP") == 0) {
        displayIp(myhost->ip);
    } else if (strcmp(command, "PORT") == 0) {
        displayPort(myhost -> port);
    }
    fflush(stdout);
}

//executing server commands
int exCommandServer(char command[], int requesting_client_fd) {
    int len = strlen(command);
    if(command[len-1] == '\n' ){
        command[len-1]=0;
    }
    if (strcmp(command, "STATISTICS") == 0) {
        serverPrintStatistics();
    } else if (strcmp(command, "BLOCKED") == 0) {
        char client_ip[500];
        sscanf(command, "BLOCKED %s", client_ip);
        server__print_blocked(client_ip);
    } else if (strcmp(command, "LOGIN") == 0) {
        char client_hostname[500], client_port[500], client_ip[500];
        sscanf(command, "LOGIN %s %s %s", client_ip, client_port, client_hostname);
        loginHandleServer(client_ip, client_port, client_hostname, requesting_client_fd);
    } else if (strcmp(command, "BROADCAST") == 0) {
        char message[500*200];
        int cmdi = 10;
        int msgi = 0;
        while (command[cmdi] != '\0') {
            message[msgi] = command[cmdi];
            cmdi += 1;
            msgi += 1;
        }
        message[msgi - 1] = '\0';
        server__handle_broadcast(message, requesting_client_fd);
    } else if (strcmp(command, "REFRESH") == 0) {
        serverHandleRefresh(requesting_client_fd);
    } else if (strcmp(command, "SEND") == 0) {
        char client_ip[500], message[500];
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
    } else if (strcmp(command, "UNBLOCK") == 0) {
        server__block_or_unblock(command, false, requesting_client_fd);
    } else if (strcmp(command, "BLOCK") == 0) {
        server__block_or_unblock(command, true, requesting_client_fd);
    } else if (strcmp(command, "LOGOUT") == 0) {
        server__handle_logout(requesting_client_fd);
    }else if (strcmp(command, "EXIT") == 0) {
        exitServer(requesting_client_fd);
    }
    fflush(stdout);
    return 1;
}

int exCommandClient(char command[]) {
    int len = strlen(command);
    if(command[len-1] == '\n' ){
        command[len-1]=0;
    }
    if (strcmp(command, "SUCCESSLOGIN") == 0) {
        cse4589_print_and_log("[LOGIN:SUCCESS]\n");
        cse4589_print_and_log("[LOGIN:END]\n");
    } else if (strcmp(command, "ERRORLOGIN") == 0) {
        cse4589_print_and_log("[LOGIN:ERROR]\n");
        cse4589_print_and_log("[LOGIN:END]\n");
    }  else if (strcmp(command, "SUCCESSLOGOUT") == 0) {
        myhost-> loggedIn = false;
        cse4589_print_and_log("[LOGOUT:SUCCESS]\n");
        cse4589_print_and_log("[LOGOUT:END]\n");
    } else if (strcmp(command, "ERRORLOGOUT") == 0) {
        cse4589_print_and_log("[LOGOUT:ERROR]\n");
        cse4589_print_and_log("[LOGOUT:END]\n");
    } else if (strcmp(command, "SUCCESSBROADCAST") == 0) {
        cse4589_print_and_log("[BROADCAST:SUCCESS]\n");
        cse4589_print_and_log("[BROADCAST:END]\n");
    } else if (strcmp(command, "SUCCESSUNBLOCK") == 0) {
        cse4589_print_and_log("[UNBLOCK:SUCCESS]\n");
        cse4589_print_and_log("[UNBLOCK:END]\n");
    } else if (strcmp(command, "SUCCESSBLOCK") == 0) {
        cse4589_print_and_log("[BLOCK:SUCCESS]\n");
        cse4589_print_and_log("[BLOCK:END]\n");
    } else if (strcmp(command, "ERRORUNBLOCK") == 0) {
        cse4589_print_and_log("[UNBLOCK:ERROR]\n");
        cse4589_print_and_log("[UNBLOCK:END]\n");
    } else if (strcmp(command, "ERRORBLOCK") == 0) {
        cse4589_print_and_log("[BLOCK:ERROR]\n");
        cse4589_print_and_log("[BLOCK:END]\n");
    } else if (strcmp(command, "SUCCESSSEND") == 0) {
        cse4589_print_and_log("[SEND:SUCCESS]\n");
        cse4589_print_and_log("[SEND:END]\n");
    } else if (strcmp(command, "LOGIN") == 0) { // takes two arguments server ip and server port
        char server_ip[500], server_port[500];
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
    } else if (strcmp(command, "REFRESHRESPONSE") == 0) {
        clientRefreshClientList(command);
    } else if (strcmp(command, "REFRESH") == 0) {
        if (myhost -> loggedIn) {
            sendCommand(server -> fd, "REFRESH\n");
        } else {
            cse4589_print_and_log("[REFRESH:ERROR]\n");
            cse4589_print_and_log("[REFRESH:END]\n");
        }
    } else if (strcmp(command, "SEND") == 0) {
        if (myhost -> loggedIn) {
            client__send(command);
        } else {
            cse4589_print_and_log("[SEND:ERROR]\n");
            cse4589_print_and_log("[SEND:END]\n");
        }
    } else if (strcmp(command, "RECEIVE") == 0) {
        char client_ip[500], message[500*200];
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
    } else if (strcmp(command, "BROADCAST") == 0) {
        if (myhost-> loggedIn) {
            sendCommand(server -> fd, command);
        } else {
            cse4589_print_and_log("[BROADCAST:ERROR]\n");
            cse4589_print_and_log("[BROADCAST:END]\n");
        }
    } else if (strcmp(command, "UNBLOCK") == 0) {
        if (myhost-> loggedIn) {
            client__block_or_unblock(command, false);
        } else {
            cse4589_print_and_log("[UNBLOCK:ERROR]\n");
            cse4589_print_and_log("[UNBLOCK:END]\n");
        }
    } else if (strcmp(command, "BLOCK") == 0) {
        if (myhost-> loggedIn) {
            client__block_or_unblock(command, true);
        } else {
            cse4589_print_and_log("[BLOCK:ERROR]\n");
            cse4589_print_and_log("[BLOCK:END]\n");
        }
    } else if (strcmp(command, "LOGOUT") == 0) {
        if (myhost-> loggedIn) {
            sendCommand(server -> fd, command);
        } else {
            cse4589_print_and_log("[LOGOUT:ERROR]\n");
            cse4589_print_and_log("[LOGOUT:END]\n");
        }
    }else if (strcmp(command, "EXIT") == 0) {
        exitClient();
    }
    fflush(stdout);
    return 1;
}