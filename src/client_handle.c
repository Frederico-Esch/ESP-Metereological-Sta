#include "client_handle.h"

int create_client(char* timeout, size_t timeout_size) {
    int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, timeout, timeout_size);
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, timeout, timeout_size);
    return client;
}

bool authorize_connect(int client, char* SERVER_KEY) {
    char msg[100];
    ssize_t bytes = send(client, SERVER_KEY, sizeof(char)*(strlen(SERVER_KEY)+1), 0);
    if (bytes < 0) return false;

    memset(msg, 0, sizeof(msg)*sizeof(char));
    bytes = recv(client, msg, sizeof(char)*sizeof(msg), 0);
    if (bytes < 0) return false;
    puts(msg);
    
    return true;
}

bool handle_connection(int client, struct fd_set* all_set, int max_fd, char* SERVER_PING, char* SERVER_KEY) {
    struct fd_set read_set, error_set;
    int amount;
    struct timeval timeout_select = (struct timeval) {.tv_sec = 30, .tv_usec = 0};
    char msg[100];

    do {
        FD_COPY(all_set, &read_set);
        FD_COPY(all_set, &error_set);
        //int is_pressed = !gpio_get_level(BUTTON_PIN);

        amount = select(max_fd+1, &read_set, NULL, &error_set, &timeout_select);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    } while(amount < 0 && errno == EINTR);

    
    if (amount < 0) {
        printf("SELECT ERROR errno %d\n %s\n", errno, strerror(errno));
        return false;
    }
    else if (amount == 0) { /* SERVERS PING, CLIENT DO NOT */ puts("timeout"); }
    else {
        for(int i = 0; i < max_fd+1; i++) {
            //DISCONNECTS
            if (FD_ISSET(i, &error_set)) { puts("ERROR SET"); return false; }
            //READ EITHER SERVER PING OR SENSOR READ
            if (FD_ISSET(i, &read_set)) {
                if (i == client) {
                    memset(msg, 0, sizeof(char)*sizeof(char));
                    ssize_t bytes = recv(client, msg, sizeof(msg)*sizeof(char), 0);
                    if (bytes < 0) {puts("RECV ERROR"); return false;} //ERROR

                    bytes = strncmp(msg, SERVER_PING, strlen(SERVER_PING));
                    if(bytes) {
                        puts(msg);

                        bytes = send(client, SERVER_KEY, (strlen(SERVER_KEY)+1)*sizeof(char), 0);

                        if (bytes < 0) {puts("SEND ERROR"); return false;}
                        puts("SERVER PINGED");
                    }
                }
            }
        }
    }

    return true;
}