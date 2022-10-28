int connectClientServer(char server_ip[], char server_port[]);
void loginClient(char server_ip[], char server_port[]);
void clientRefreshClientList(char clientListString[]);
void client__send(char command[]);
void client__handle_receive(char client_ip[], char msg[]);
void client__block_or_unblock(char command[], bool is_a_block);
void exitClient();