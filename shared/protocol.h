#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <signal.h>

// costanti del protocollo
#define MAX_NICKNAME_LEN 32
#define MAX_MSG_LEN 1024
#define MAX_THEMES 4
#define MAX_THEME_LEN 32
#define MAX_QUESTION_LEN 256
#define MAX_ANSWER_LEN 128
#define MAX_CLIENTS 20
#define QUIZ_QUESTIONS 5

// Tipi di messaggio del protocollo
#define MSG_NICK "NICK"
#define MSG_THEME "THEME"
#define MSG_THEMES "THEMES"
#define MSG_THEMES_LIST "THEMES_LIST"
#define MSG_QUIZ_START "QUIZ_START"
#define MSG_QUESTION "QUESTION"
#define MSG_ANSWER "ANSWER"
#define MSG_RESULT "RESULT"
#define MSG_SCORE "SCORE"
#define MSG_SCORELIST "SCORELIST"
#define MSG_END_SCORE "END_SCORE"  
#define MSG_END "END"
#define MSG_OK "OK"
#define MSG_ERROR "ERROR"

// Risposte del server
#define RESP_CORRECT "CORRECT"
#define RESP_WRONG "WRONG"
#define RESP_NICK_TAKEN "NICK_TAKEN"
#define RESP_INVALID_THEME "INVALID_THEME"
#define RESP_QUIZ_COMPLETE "QUIZ_COMPLETE"

typedef struct{
    char question[MAX_QUESTION_LEN];
    char correct_answer[MAX_ANSWER_LEN];
} Question;

typedef struct {
    Question questions[QUIZ_QUESTIONS];
    int count;
} Quiz;


extern char theme[MAX_THEMES][MAX_THEME_LEN];
extern int themes_count;

void print_formatted_list(const char* str, const char* title);
void trim_newline(char *str);
int valid_nickname(const char *nickname);
void clean_up_socket(int socket);
int is_numeric(const char* str);
int recv_msg(int socket, char* type, char* data);
int send_msg(int socket, const char* type, char* data);



#endif // PROTOCOL_H