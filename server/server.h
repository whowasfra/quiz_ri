#ifndef SERVER_H
#define SERVER_H

#include "../shared/protocol.h"
#include "logger.h"
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <signal.h>

#define SERVER_PORT 8080
#define LOG_FILE_PATH "server.log"
#define SHM_KEY 12345 // Chiave per la memoria condivisa
#define SEM_KEY 54321 // Chiave per il semaforo

typedef struct{
    char nickname[MAX_NICKNAME_LEN];
    int score [MAX_THEMES]; // tiene traccia del punteggio per ogni tema, -1 se non iniziato
    int completed [MAX_THEMES]; // 0 se non completato, 1 completato
} Player;

// Struttura per la memoria condivisa
typedef struct {
    Player players[MAX_CLIENTS];
    int player_count;
    int server_running;
} ServerState;

// Variabili globali
extern ServerState* shared_state;
extern int shm_id;
extern int sem_id;

// Funzioni per sincronizzazione
void lock_shared_state();
void unlock_shared_state();
int init_semaphore();
void cleanup_semaphore();

// Funzioni
int create_server_socket();
void init_themes();
void handle_client(int client_socket);
int accept_client(int server_socket);
void cleanup_server(int status);

#endif