#include <sys/shm.h>
#include <sys/ipc.h>
#include <errno.h>
#include "server.h"

extern void print_players_status(void);

// Variabili globali per la memoria condivisa
ServerState* shared_state = NULL;
int shm_id = -1;
int sem_id = -1;  // Semaforo per sincronizzazione
int server_socket = -1;

/**
 * Gestore di segnali per il processo server principale
 * SIGINT: Termina il server in modo ordinato
 * SIGPIPE: Ignorato per evitare crash quando un client si disconnette improvvisamente
 */
void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("\n\nTerminazione del server richiesta dall'utente.\n");
        
        // Chiudi il socket del server per fermare nuove connessioni
        if (server_socket >= 0) {
            close(server_socket);
        }
        // Le risorse IPC (semafori e memoria condivisa) vengono rilasciate automaticamente alla terminazione
        sleep(2); // Attendi brevemente
        exit(0);
    } else if (sig == SIGPIPE) {
        // Ignora SIGPIPE - la disconnessione improvvisa dei client viene gestita
        // dai singoli processi figli tramite controllo dei valori di ritorno di send/recv
        return;
    }
}

/**
 * Acquisisce il lock sulla memoria condivisa usando un semaforo POSIX
 * Implementa l'operazione P (wait) su un semaforo binario
 * Blocca il processo chiamante finché il semaforo non è disponibile
 */
void lock_shared_state() {
    struct sembuf sem_op;
    sem_op.sem_num = 0;    // Numero del semaforo (usiamo il primo del set)
    sem_op.sem_op = -1;    // Operazione P (wait/lock): decrementa il semaforo
    sem_op.sem_flg = 0;    // Nessun flag speciale (operazione bloccante)
    
    if (semop(sem_id, &sem_op, 1) == -1) {
        perror("Errore lock semaforo");
    }
}

/**
 * Rilascia il lock sulla memoria condivisa
 * Implementa l'operazione V (signal) su un semaforo binario
 * Consente ad altri processi in attesa di acquisire il lock
 */
void unlock_shared_state() {
    struct sembuf sem_op;
    sem_op.sem_num = 0;    // Numero del semaforo
    sem_op.sem_op = 1;     // Operazione V (signal/unlock): incrementa il semaforo
    sem_op.sem_flg = 0;    // Nessun flag speciale
    
    if (semop(sem_id, &sem_op, 1) == -1) {
        perror("Errore unlock semaforo");
    }
}

/**
 * Inizializza il semaforo per la sincronizzazione
 * @return 0 se successo, -1 in caso di errore
 */
int init_semaphore() {
    // Crea il semaforo
    sem_id = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (sem_id == -1) {
        perror("Errore creazione semaforo");
        return -1;
    }
    
    // Inizializza il semaforo a 1 (mutex libero)
    if (semctl(sem_id, 0, SETVAL, 1) == -1) {
        perror("Errore inizializzazione semaforo");
        return -1;
    }
    
    LOG_INFO("Semaforo inizializzato con ID: %d", sem_id);
    return 0;
}
/**
 * Rimuove il semaforo dal sistema
 * Deve essere chiamato solo dal processo server principale alla terminazione
 * Tutti i processi che usano questo semaforo devono essere terminati prima
 */
void cleanup_semaphore() {
    if (sem_id != -1) {
        // IPC_RMID rimuove il semaforo dal sistema operativo
        if (semctl(sem_id, 0, IPC_RMID) == -1) {
            perror("Errore rimozione semaforo");
        } else {
            LOG_INFO("Semaforo rimosso con successo");
        }
    }
}  

/**
 * Funzione di pulizia del server
 * @param status Stato di uscita del server
 */
void cleanup_server(int status) {
    // Rimuovi il semaforo
    cleanup_semaphore();
    
    // Detach e rimuovi la memoria condivisa
    if (shared_state != NULL) {
        shmdt(shared_state);
    }
    
    if (shm_id != -1) {
        shmctl(shm_id, IPC_RMID, NULL);
    }
    
    LOG_INFO("Memoria condivisa rimossa. Server terminato con stato %d", status);
}

int main(){
    
    // Registrazione gestori di segnali
    signal(SIGINT, signal_handler);
    signal(SIGPIPE, signal_handler);
    
    system("clear");
    printf("=== TRIVIA QUIZ SERVER ===\n");
    printf("Inizializzazione del server...\n");
    
    // Crea segmento di memoria condivisa
    shm_id = shmget(SHM_KEY, sizeof(ServerState), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("shmget");
        exit(1);
    }
    
    // Collega il segmento di memoria condivisa
    shared_state = (ServerState*)shmat(shm_id, NULL, 0);
    if (shared_state == (ServerState*)-1) {
        perror("shmat");
        cleanup_server(1);
        exit(1);
    }
    
    // Inizializza la memoria condivisa
    memset(shared_state, 0, sizeof(ServerState));
    shared_state->server_running = 1;
    shared_state->player_count = 0;
    
    // Inizializza il semaforo per la sincronizzazione
    if (init_semaphore() < 0) {
        printf("Errore: impossibile inizializzare il semaforo\n");
        LOG_ERROR("Impossibile inizializzare il semaforo");
        cleanup_server(1);
        exit(1);
    }

    // Inizializza il logger
    init_logger(LOG_FILE_PATH);

    printf("=== TRIVIA QUIZ SERVER ===\n");
    printf("Avvio server sulla porta %d...\n", SERVER_PORT);
    LOG_INFO("Avvio server sulla porta %d...", SERVER_PORT);
    
    printf("Caricamento temi...\n");
    
    server_socket = create_server_socket(SERVER_PORT);
    if(server_socket < 0){
        printf("Errore: impossibile avviare server\n");
        LOG_ERROR("Impossibile avviare il server");
        close_logger();
        return 1;
    }

    printf("Server avviato con successo!\n");
    printf("In attesa di connessioni...\n");
    printf("Temi disponibili:\n");
    LOG_INFO("Server avviato con successo");
    
    init_themes();
    
    printf("In attesa di connessioni...\n");
    printf("Premi Ctrl+C per terminare\n\n");
    LOG_INFO("Server in ascolto in attesa di connessioni");

    // Loop principale del server
    while (shared_state->server_running)
    {
        int client_socket = accept_client(server_socket);
        if(client_socket < 0){
            if(shared_state->server_running){
                LOG_WARNING("Errore accept, continuazione...");
            }
            continue;
        }
        
        LOG_INFO("Nuova connessione accettata");

        // Stampa la lista aggiornata dei giocatori e le classifiche
        //print_players_status();

        // Fork: crea un processo figlio per gestire questo client
        // Il processo padre continua ad accettare nuove connessioni
        // Il processo figlio gestisce la comunicazione con il singolo client
        pid_t pid = fork();

        if(pid == 0){
            // Processo figlio: gestisce il client
            // Chiudi il socket del server (non necessario nel figlio)
            close(server_socket);
            
            // Gestisce tutta la comunicazione con il client
            // Questa funzione non ritorna finché il client non si disconnette
            handle_client(client_socket);
        } else if (pid > 0){
            // Processo padre: torna ad accettare nuove connessioni
            // Chiudi il socket del client (gestito dal processo figlio)
            close(client_socket);
        } else {
            // Errore nella fork
            perror("Errore fork");
            LOG_ERROR("Errore nella creazione del processo figlio");
            close(client_socket);
        }
    }

    printf("Chiusura server...\n");
    LOG_INFO("Chiusura server in corso");
    close(server_socket);

    printf("Server terminato.\n");
    LOG_INFO("Server terminato");
    close_logger();
    
    // Pulizia finale
    cleanup_server(0);
    return 0;    
}

/**
 * Crea e configura il socket del server
 * @return Il socket del server o -1 in caso di errore
 */
int create_server_socket(){
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket < 0){
        perror("Errore creazione socket");
        LOG_ERROR("Errore creazione socket");
        return -1;
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    if(bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("Errore bind");
        LOG_ERROR("Errore bind socket");
        close(server_socket);
        return -1;
    }

    if(listen(server_socket, MAX_CLIENTS) < 0){
        perror("Errore listen");
        LOG_ERROR("Errore listen socket");
        close(server_socket);
        return -1;
    }
    LOG_INFO("Socket server creato e in ascolto");
    return server_socket;
}

/**
 * Inizializza i temi caricandoli dal file temi.txt
 */
void init_themes(){
    FILE *fp;
    char buffer[MAX_THEME_LEN];
    int count = 0;

    fp = fopen("./src/temi.txt", "r");
    if (fp == NULL) {
        printf("Errore: impossibile aprire il file dei temi\n");
    } else {
        while (fgets(buffer, MAX_THEME_LEN, fp) != NULL && count < MAX_THEMES) {
            // Rimuovi il carattere newline se presente
            size_t len = strlen(buffer);
            if (len > 0 && buffer[len-1] == '\n')
                buffer[len-1] = '\0';
            
            // Salva il tema nell'array globale
            strncpy(theme[count], buffer, MAX_THEME_LEN);
            count++;
        }
        fclose(fp);
        
        if (count == 0) {
            printf("  Nessun tema disponibile\n");
        } else {
            printf("Trovati %d temi\n", count);
            // Stampa i temi disponibili
            for (int i = 0; i < count; i++) {
                printf("  - %s\n", theme[i]);
            }
        }
        
        // Aggiorna il numero totale di temi
        themes_count = count;
    }
}

/**
 * Accetta una nuova connessione client
 * @param server_socket Il socket del server
 * @return Il socket del client accettato o -1 in caso di errore
 */
int accept_client(int server_socket){
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);

    if(client_socket < 0){
        if (errno == EINTR) {
            // Accettazione interrotta da un segnale (come SIGALRM)
            LOG_INFO("Accept interrotto da segnale");
            return -1;
        }
        LOG_WARNING("Errore nella accept");
        return -1;
    }

    printf("Nuovo client connesso: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    return client_socket;
}