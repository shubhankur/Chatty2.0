#include <stdbool.h>

struct message {
    char text[500];
    struct host * from_client;
    struct message * next_message;
    bool is_broadcast;
};

struct host {
    char hostname[500];
    char ip[500];
    char port[500];
    char status[500];
    int sentMsgCount;
    int recvMsgCount;
    int fd;
    struct host * blocked;
    struct host * next_host;
    bool loggedIn;
    bool checkServer;
    struct message * queued_messages;
};