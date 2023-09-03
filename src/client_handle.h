#ifndef CLIENT_HANDLE_H
#define CLIENT_HANDLE_H
#include<stdbool.h>
#include<sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>

int create_client(char* timeout, size_t timeout_size);
bool authorize_connect(int client, char* SERVER_KEY);
bool handle_connection (int client, struct fd_set* all_set, int max_fd, char* SERVER_PING, char* SERVER_KEY);

#endif //CLIENT_HANDLE_H