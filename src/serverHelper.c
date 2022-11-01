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
#include "../include/send.h"

void serverPrintStatistics() {
    cse4589_print_and_log("[STATISTICS:SUCCESS]\n");
    int id = 1;
    for (struct host * tmp = clients;tmp != NULL;tmp = tmp -> next_host) {
        cse4589_print_and_log("%-5d%-35s%-8d%-8d%-8s\n", id, tmp -> hostname, tmp -> sentMsgCount, tmp -> recvMsgCount, tmp -> loggedIn ? "logged-in" : "logged-out");
        id = id + 1;
    }
    cse4589_print_and_log("[STATISTICS:END]\n");
}

void loginHandleServer(char client_ip[], char client_port[], char client_hostname[], int requesting_client_fd) {
    char returnMsg[500*200] = "REFRESHRESPONSE FIRST\n";
    int is_new = 1;
    struct host * requesting_client = malloc(sizeof(struct host));

    for (struct host * tmp = clients;tmp != NULL;tmp = tmp -> next_host) {
        if (tmp -> fd == requesting_client_fd) {
            requesting_client = tmp;
            is_new = 0;
            break;
        }
    }

    if (is_new==1) {
        memcpy(clientNew -> hostname, client_hostname, sizeof(clientNew -> hostname));
        memcpy(clientNew -> port, client_port, sizeof(clientNew -> port));
        requesting_client = clientNew;
        int client_port_value = atoi(client_port);
        if (clients == NULL) {
            clients = malloc(sizeof(struct host));
            clients = clientNew;
        } 
        else if (client_port_value < atoi(clients -> port)) {
            clientNew -> next_host = clients;
            clients = clientNew;
        } 
        else {
            struct host * tmp = clients;
            for (; tmp -> next_host != NULL && atoi(tmp -> next_host -> port) < client_port_value;tmp = tmp -> next_host) {
                
            }
            clientNew -> next_host = tmp -> next_host;
            tmp -> next_host = clientNew;
        }

    } else {
        requesting_client -> loggedIn = true;
    }
    for (struct host * tmp = clients;tmp != NULL;tmp = tmp -> next_host) {
        if (tmp -> loggedIn) {
            char clientString[2000];
            sprintf(clientString, "%s %s %s\n", tmp -> ip, tmp -> port, tmp -> hostname);
            strcat(returnMsg, clientString);
        }
    }

    strcat(returnMsg, "ENDREFRESH\n");
    struct message * tmp_message = requesting_client -> queued_messages;
    char receive[1500];

    for (;tmp_message != NULL;tmp_message = tmp_message -> next_message) {
        requesting_client -> recvMsgCount++;
        sprintf(receive, "RECEIVE %s %s    ", tmp_message -> from_client -> ip, tmp_message -> text);
        strcat(returnMsg, receive);
        if (!tmp_message -> is_broadcast) {
            cse4589_print_and_log("[RELAYED:SUCCESS]\n");
            cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", tmp_message -> from_client -> ip, requesting_client -> ip, tmp_message -> text);
            cse4589_print_and_log("[RELAYED:END]\n");
        }
    }
    sendCommand(requesting_client_fd, returnMsg);
    requesting_client -> queued_messages = tmp_message;
}
//server handling the request to refresh the client
void serverHandleRefresh(int requesting_client_fd) {
    char clientListString[500*200] = "REFRESHRESPONSE NOTFIRST\n";
    for (struct host * tmp = clients;tmp != NULL;tmp = tmp -> next_host) {
        if (tmp -> loggedIn) {
            char clientString[500 * 4];
            sprintf(clientString, "%s %s %s\n", tmp -> ip, tmp -> port, tmp -> hostname);
            strcat(clientListString, clientString);
        }
    }
    strcat(clientListString, "ENDREFRESH\n");
    sendCommand(requesting_client_fd, clientListString);
}
/** SERVER HANDLE SEND REQUEST FROM CLIENTS **/
void server__handle_send(char client_ip[], char msg[], int requesting_client_fd) {
    char receive[2000];
    struct host * from_client = malloc(sizeof(struct host)), * to_client = malloc(sizeof(struct host));;
    for (struct host * tmp = clients;tmp != NULL;tmp = tmp -> next_host) {
        if (strstr(client_ip, tmp -> ip) != NULL) {
            to_client = tmp;
        }
        if (requesting_client_fd == tmp -> fd) {
            from_client = tmp;
        }
    }
    if (to_client == NULL || from_client == NULL) {
        cse4589_print_and_log("[RELAYED:ERROR]\n");
        cse4589_print_and_log("[RELAYED:END]\n");

        return;
    }

    from_client -> sentMsgCount++;
    // CHECK IF SENDER IS BLOCKED (FROM IS BLOCKED BY TO)
    bool is_blocked = false;
    for (struct host * tmp = to_client -> blocked;tmp != NULL;tmp = tmp -> next_host) {
        if (strstr(from_client -> ip, tmp -> ip) != NULL) {
            is_blocked = true;
            break;
        }
    }
    sendCommand(from_client -> fd, "SUCCESSSEND\n");
    if (is_blocked) {
        cse4589_print_and_log("[RELAYED:SUCCESS]\n");
        cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", from_client -> ip, to_client -> ip, msg);
        cse4589_print_and_log("[RELAYED:END]\n");
        sendCommand(from_client -> fd, "SUCCESSSEND\n");
        return;
    }

    if (to_client -> loggedIn) {
        to_client -> recvMsgCount++;
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
            struct message * tmp_message = to_client -> queued_messages;
            for (;tmp_message -> next_message != NULL;tmp_message -> next_message = new_message) {
                tmp_message = tmp_message -> next_message;
            }
            tmp_message -> next_message = new_message; 
        }
    }

}
/** SERVER HANDLE BROADCAST REQUEST FROM CLIENT **/
void server__handle_broadcast(char msg[], int requesting_client_fd) {
    struct host * from_client = malloc(sizeof(struct host));
    for (struct host * tmp = clients;tmp != NULL;tmp = tmp -> next_host) {
        if (requesting_client_fd == tmp -> fd) {
            from_client = tmp;
        }
    }
    from_client -> sentMsgCount++;
    for (struct host * to_client = clients;to_client != NULL;to_client = to_client -> next_host) {
        if (to_client -> fd == requesting_client_fd) {
            continue;
        }
        bool is_blocked = false;
        for (struct host * tmp_blocked = to_client -> blocked;tmp_blocked != NULL;tmp_blocked = tmp_blocked -> next_host) {
            if (tmp_blocked -> fd == requesting_client_fd) {
                is_blocked = true;
                break;
            }
        }
        if (is_blocked) {
            continue;
        }
        char receive[500 * 4];
        if (to_client -> loggedIn) {
            to_client -> recvMsgCount++;
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
                struct message * tmp_message = to_client -> queued_messages;
                for (;tmp_message -> next_message != NULL;tmp_message = tmp_message -> next_message) {
                }
                tmp_message -> next_message = new_message;
            }
        }
    }

    cse4589_print_and_log("[RELAYED:SUCCESS]\n");
    cse4589_print_and_log("msg from:%s, to:255.255.255.255\n[msg]:%s\n", from_client ->ip, msg);
    cse4589_print_and_log("[RELAYED:END]\n");
    sendCommand(from_client -> fd, "SUCCESSBROADCAST\n");
}

void server__block_or_unblock(char command[], bool is_a_block, int requesting_client_fd) {
    char client_ip[500], client_port[500];;
    if (is_a_block) {
        sscanf(command, "BLOCK %s %s\n", client_ip, client_port);
    } else {
        sscanf(command, "UNBLOCK %s %s\n", client_ip, client_port);
    }
    struct host * requesting_client = malloc(sizeof(struct host));
    struct host * blocked_client = malloc(sizeof(struct host));

    for (struct host * tmp = clients;tmp != NULL;tmp = tmp -> next_host) {
        if (tmp -> fd == requesting_client_fd) {
            requesting_client = tmp;
        }
        if (strstr(client_ip, tmp -> ip) != NULL) {
            blocked_client = tmp;
        }
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
                struct host * tmp = requesting_client -> blocked;
                for (;tmp -> next_host != NULL && atoi(tmp -> next_host -> port) < new_blocked_client_port_value;tmp = tmp -> next_host) {
                }
                new_blocked_client -> next_host = tmp -> next_host;
                tmp -> next_host = new_blocked_client;
            }

            sendCommand(requesting_client_fd, "SUCCESSBLOCK\n");
        } else {
            struct host * tmp_blocked = requesting_client -> blocked;
            if (strstr(tmp_blocked ->ip, blocked_client ->ip) != NULL) {
                requesting_client -> blocked = requesting_client -> blocked -> next_host;
            } else {
                struct host * previous = tmp_blocked;
                for (;tmp_blocked != NULL;tmp_blocked = tmp_blocked -> next_host) {
                    if (strstr(tmp_blocked ->ip, blocked_client ->ip) != NULL) {
                        previous -> next_host = tmp_blocked -> next_host;
                        break;
                    }
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
    struct host * tmp = clients;
    for (;tmp != NULL;tmp = tmp -> next_host) {
        if (strstr(blocker_ip_addr, tmp ->ip) != NULL) {
            break;
        }
    }
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, blocker_ip_addr, & (sa.sin_addr));
    if (result!=0 && tmp) {
        cse4589_print_and_log("[BLOCKED:SUCCESS]\n");
        struct host * tmp_blocked = clients;
        tmp_blocked = tmp -> blocked;
        int id = 1;
        for (;tmp_blocked != NULL;tmp_blocked = tmp_blocked -> next_host) {
            cse4589_print_and_log("%-5d%-35s%-20s%-8d\n", id, tmp_blocked -> hostname, tmp_blocked ->ip, atoi(tmp_blocked -> port));
            id = id + 1;
        }
    } else {
        cse4589_print_and_log("[BLOCKED:ERROR]\n");
    }

    cse4589_print_and_log("[BLOCKED:END]\n");
}

/** SERVER HANDLE LOGOUT REQUEST FROM CLIENT **/
void server__handle_logout(int requesting_client_fd) {
    struct host * tmp = clients;
    for (;tmp != NULL;tmp = tmp -> next_host) {
        if (tmp -> fd == requesting_client_fd) {
            sendCommand(requesting_client_fd, "SUCCESSLOGOUT\n");
            tmp -> loggedIn = false;
            break;
        }
    }
    if (tmp == NULL) {
        sendCommand(requesting_client_fd, "ERRORLOGOUT\n");
    }
}

//server handling the request too exit the client
void exitServer(int requesting_client_fd) {
    struct host * tmp = clients;
    if (tmp -> fd == requesting_client_fd) {
        clients = clients -> next_host;
    } else {
        struct host * previous = tmp;
        for (;tmp != NULL;tmp = tmp -> next_host) {
            if (tmp -> fd == requesting_client_fd) {
                previous -> next_host = tmp -> next_host;
                tmp = tmp -> next_host;
                break;
            }
        }
    }
}