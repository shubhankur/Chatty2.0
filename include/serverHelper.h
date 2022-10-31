void loginHandleServer(char client_ip[], char client_port[], char client_hostname[], int requesting_client_fd);
void serverHandleRefresh(int requesting_client_fd);
void server__handle_send(char client_ip[], char msg[], int requesting_client_fd);
void server__handle_broadcast(char msg[], int requesting_client_fd);
void server__block_or_unblock(char command[], bool is_a_block, int requesting_client_fd);
void server__print_blocked(char blocker_ip_addr[]);
void server__handle_logout(int requesting_client_fd);
void exitServer(int requesting_client_fd);
void serverPrintStatistics();