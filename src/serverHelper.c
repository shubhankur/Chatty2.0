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

/***  PRINT STATISTICS ***/
void server__print_statistics() {
    cse4589_print_and_log("[STATISTICS:SUCCESS]\n");

    struct host * temp = clients;
    int id = 1;
    while (temp != NULL) {
        cse4589_print_and_log("%-5d%-35s%-8d%-8d%-8s\n", id, temp -> hostname, temp -> sentMsgCount, temp -> recvMsgCount, temp -> loggedIn ? "logged-in" : "logged-out");
        id = id + 1;
        temp = temp -> next_host;
    }

    cse4589_print_and_log("[STATISTICS:END]\n");
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
        requesting_client -> loggedIn = true;
    }

    temp = clients;
    while (temp != NULL) {
        if (temp -> loggedIn) {
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
        requesting_client -> recvMsgCount++;
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
//server handling the request to refresh the client
void serverHandleRefresh(int requesting_client_fd) {
    char clientListString[dataSizeMaxBg] = "REFRESHRESPONSE NOTFIRST\n";
    struct host * temp = clients;
    while (temp != NULL) {
        if (temp -> loggedIn) {
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

    from_client -> sentMsgCount++;
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
            struct message * temp_message = to_client -> queued_messages;
            while (temp_message -> next_message != NULL) {
                temp_message = temp_message -> next_message;
            }
            temp_message -> next_message = new_message;
        }
    }

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
    from_client -> sentMsgCount++;
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
    if (result!=0 && temp) {
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
            temp -> loggedIn = false;
            break;
        }
        temp = temp -> next_host;
    }
    if (temp == NULL) {
        sendCommand(requesting_client_fd, "ERRORLOGOUT\n");
    }
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