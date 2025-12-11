#ifndef CLIENT_H
#define CLIENT_H

#include "../shared/protocol.h"

typedef struct 
{
    int socket;
    char nickname[MAX_NICKNAME_LEN];
    int theme;
    int score;
    int quiz_active;
} ClientStatus;

int connect_to_server(char* hostname, int port);
void menu();
int register_nickname(ClientStatus *client);
int select_theme(ClientStatus* client);
int play(ClientStatus* client);
int show_result(const char* result);

#endif