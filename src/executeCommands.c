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
    char command1[len];
    memcpy(command1, command, len);
    if(command1[len-1]='\n'){
        command1[len-1]=0;
    }
    if (strstr(command1, "AUTHOR") != NULL) {
        printAuthor("skumar45");
    } else if (strstr(command1, "IP") != NULL) {
        displayIp(myhost->ip);
    } else if (strstr(command1, "PORT") != NULL) {
        displayPort(myhost -> port);
    }
    fflush(stdout);
}

//executing server commands
int exCommandServer(char command[], int requesting_client_fd) {
    int len = strlen(command);
    char command1[len];
    memcpy(command1, command, len);
    if(command1[len-1]='\n'){
        command1[len-1]=0;
    }
    if (strstr(command1, "STATISTICS") != NULL) {
        serverPrintStatistics();
    } else if (strstr(command, "BLOCKED") != NULL) {
        //command[len-1] = '\n';
        char client_ip[500];
        sscanf(command, "BLOCKED %s", client_ip);
        server__print_blocked(client_ip);
    } else if (strstr(command1, "LOGIN") != NULL) {
        //command[len-1] = '\n';
        char client_hostname[500], client_port[500], client_ip[500];
        sscanf(command, "LOGIN %s %s %s", client_ip, client_port, client_hostname);
        loginHandleServer(client_ip, client_port, client_hostname, requesting_client_fd);
    } else if (strstr(command1, "BROADCAST") != NULL) {
        //command[len-1] = '\n';
        char message[500*200];
        int msgi = 0;
        for (int cmdi = 10;command[cmdi] != '\0';cmdi+=1) {
            message[msgi] = command[cmdi];
            msgi += 1;
        }
        message[msgi - 1] = '\0';
        server__handle_broadcast(message, requesting_client_fd);
    } else if (strstr(command1, "REFRESH") != NULL) {
        //command[len-1] = '\n';
        serverHandleRefresh(requesting_client_fd);
    } else if (strstr(command1, "SEND") != NULL) {
        //command[len-1] = '\n';
        char client_ip[500];
        char message[500];
        int ipi = 0;
        int cmdi = 5;
        for (;command[cmdi] != ' ';cmdi+=1) {
            client_ip[ipi] = command[cmdi];
            ipi += 1;
        }
        client_ip[ipi] = '\0';
        cmdi++;
        int msgi = 0;
        for (;command[cmdi] != '\0';cmdi+=1) {
            message[msgi] = command[cmdi];
            msgi += 1;
        }
        message[msgi - 1] = '\0'; // Remove new line
        server__handle_send(client_ip, message, requesting_client_fd);
    } else if (strstr(command1, "UNBLOCK") != NULL) {
        server__block_or_unblock(command, false, requesting_client_fd);
    } else if (strstr(command1, "BLOCK") != NULL) {
        server__block_or_unblock(command, true, requesting_client_fd);
    } else if (strstr(command1, "LOGOUT") != NULL) {
        server__handle_logout(requesting_client_fd);
    }else if (strstr(command1, "EXIT") != NULL) {
        exitServer(requesting_client_fd);
    }
    fflush(stdout);
    return 1;
}

int exCommandClient(char command[]) {
    int len = strlen(command);
    char command1[len];
    memcpy(command1, command, len);
    if(command1[len-1]='\n'){
        command1[len-1]=0;
    }
    if (strstr(command1, "SUCCESSLOGIN") != NULL) {
        cse4589_print_and_log("[LOGIN:SUCCESS]\n");
        cse4589_print_and_log("[LOGIN:END]\n");
    } else if (strstr(command1, "ERRORLOGIN") != NULL) {
        cse4589_print_and_log("[LOGIN:ERROR]\n");
        cse4589_print_and_log("[LOGIN:END]\n");
    }  else if (strstr(command1, "SUCCESSLOGOUT") != NULL) {
        myhost-> loggedIn = false;
        cse4589_print_and_log("[LOGOUT:SUCCESS]\n");
        cse4589_print_and_log("[LOGOUT:END]\n");
    } else if (strstr(command1, "ERRORLOGOUT") != NULL) {
        cse4589_print_and_log("[LOGOUT:ERROR]\n");
        cse4589_print_and_log("[LOGOUT:END]\n");
    } else if (strstr(command1, "SUCCESSBROADCAST") != NULL) {
        cse4589_print_and_log("[BROADCAST:SUCCESS]\n");
        cse4589_print_and_log("[BROADCAST:END]\n");
    } else if (strstr(command1, "SUCCESSUNBLOCK") != NULL) {
        cse4589_print_and_log("[UNBLOCK:SUCCESS]\n");
        cse4589_print_and_log("[UNBLOCK:END]\n");
    } else if (strstr(command1, "SUCCESSBLOCK") != NULL) {
        cse4589_print_and_log("[BLOCK:SUCCESS]\n");
        cse4589_print_and_log("[BLOCK:END]\n");
    } else if (strstr(command1, "ERRORUNBLOCK") != NULL) {
        cse4589_print_and_log("[UNBLOCK:ERROR]\n");
        cse4589_print_and_log("[UNBLOCK:END]\n");
    } else if (strstr(command1, "ERRORBLOCK") != NULL) {
        cse4589_print_and_log("[BLOCK:ERROR]\n");
        cse4589_print_and_log("[BLOCK:END]\n");
    } else if (strstr(command1, "SUCCESSSEND") != NULL) {
        cse4589_print_and_log("[SEND:SUCCESS]\n");
        cse4589_print_and_log("[SEND:END]\n");
    } else if (strstr(command1, "LOGIN") != NULL) { // takes two arguments server ip and server port
        //command[len-1] = '\n';
        char server_ip[500], server_port[500];
        int cmdi = 6;
        int ipi = 0;
        for (;command[cmdi] != ' ' && ipi < 256;cmdi+=1) {
            server_ip[ipi] = command[cmdi];
            ipi += 1;
        }
        server_ip[ipi] = '\0';
        cmdi += 1;
        int pi = 0;
        for (;command[cmdi] != '\0';cmdi+=1) {
            server_port[pi] = command[cmdi];
            pi += 1;
        }
        server_port[pi - 1] = '\0'; // REMOVE THE NEW LINE
        loginClient(server_ip, server_port);
    } else if (strstr(command1, "REFRESHRESPONSE") != NULL) {
        //command[len-1] = '\n';
        clientRefreshClientList(command);
    } else if (strstr(command1, "REFRESH") != NULL) {
        //command[len-1] = '\n';
        if (myhost -> loggedIn) {
            sendCommand(server -> fd, "REFRESH\n");
        } else {
            cse4589_print_and_log("[REFRESH:ERROR]\n");
            cse4589_print_and_log("[REFRESH:END]\n");
        }
    } else if (strstr(command1, "SEND") != NULL) {
        //command[len-1] = '\n';
        if (myhost -> loggedIn) {
            client__send(command);
        } else {
            cse4589_print_and_log("[SEND:ERROR]\n");
            cse4589_print_and_log("[SEND:END]\n");
        }
    } else if (strstr(command1, "RECEIVE") != NULL) {
        //command[len-1] = '\n';
        char client_ip[500], message[500*200];
        int cmdi = 8;
        int ipi = 0;
        for (;command[cmdi] != ' ' && ipi < 256;cmdi+=1) {
            client_ip[ipi] = command[cmdi];
            ipi += 1;
        }
        client_ip[ipi] = '\0';
        cmdi += 1;
        int msgi = 0;
        for (;command[cmdi] != '\0';cmdi+=1) {
            message[msgi] = command[cmdi];
            msgi += 1;
        }
        message[msgi - 1] = '\0'; // REMOVE THE NEW LINE
        client__handle_receive(client_ip, message);
    } else if (strstr(command1, "BROADCAST") != NULL) {
        //command[len-1] = '\n';
        if (myhost-> loggedIn) {
            sendCommand(server -> fd, command);
        } else {
            cse4589_print_and_log("[BROADCAST:ERROR]\n");
            cse4589_print_and_log("[BROADCAST:END]\n");
        }
    } else if (strstr(command1, "UNBLOCK") != NULL) {
        //command[len-1] = '\n';
        if (myhost-> loggedIn) {
            client__block_or_unblock(command, false);
        } else {
            cse4589_print_and_log("[UNBLOCK:ERROR]\n");
            cse4589_print_and_log("[UNBLOCK:END]\n");
        }
    } else if (strstr(command1, "BLOCK") != NULL) {
        //command[len-1] = '\n';
        if (myhost-> loggedIn) {
            client__block_or_unblock(command, true);
        } else {
            cse4589_print_and_log("[BLOCK:ERROR]\n");
            cse4589_print_and_log("[BLOCK:END]\n");
        }
    } else if (strstr(command1, "LOGOUT") != NULL) {
        //command[len-1] = '\n';
        if (myhost-> loggedIn) {
            sendCommand(server -> fd, command);
        } else {
            cse4589_print_and_log("[LOGOUT:ERROR]\n");
            cse4589_print_and_log("[LOGOUT:END]\n");
        }
    }else if (strstr(command1, "EXIT") != NULL) {
        exitClient();
    }
    fflush(stdout);
    return 1;
}