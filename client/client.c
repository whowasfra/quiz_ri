#include "client.h"

/**
 * Pulisce il buffer di input (stdin) rimuovendo tutti i caratteri fino al newline
 * Utile per prevenire buffer overflow e input corrotti
 * Deve essere chiamata dopo input troppo lunghi o quando si vuole ignorare input residuo
 */
void clear_input_buffer() {
    int c;
    // Legge e scarta tutti i caratteri fino al newline o EOF
    while ((c = getchar()) != '\n' && c != EOF);
}

// Gestori di segnali 
void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("\n\nChiusura del client richiesta dall'utente.\n");
        sleep(2);
        exit(0);
    } else if (sig == SIGPIPE) {
        printf("\nConnessione persa con il server.\n");
        exit(1);
    }
}

int main(int argc, char* argv[]){
    int port ;
    char hostname[16] = "127.0.0.1";
    int choice;

    // Registrazione gestori di segnali
    signal(SIGINT, signal_handler);
    signal(SIGPIPE, signal_handler);

    //Gestione parametri in ingresso
    if(argc > 1){
        port = atoi(argv[1]);
        if(port <= 0 || port > 65535){
            fprintf(stderr, "Mi dispiace, %s non è una porta valida.\n", argv[0]);
            exit(1);
        }
    }
    while(1){
        menu();
        
        char input_line[64];
        if(fgets(input_line, sizeof(input_line), stdin) == NULL) {
            continue;
        }
        
        // Se l'input è troppo lungo, pulisci il buffer
        if(strlen(input_line) > 0 && input_line[strlen(input_line)-1] != '\n') {
            clear_input_buffer();
            printf("Input troppo lungo. Riprova.\n");
            sleep(1);
            continue;
        }
        
        trim_newline(input_line);
        
        // Verifica che sia un numero
        if(!is_numeric(input_line)) {
            printf("Input non valido. Inserisci un numero.\n");
            sleep(1);
            continue;
        }
        
        choice = atoi(input_line);
    
        if(choice == 1){
            ClientStatus client = {0};
            //Connessione al server
            printf("Connessione al server %s sulla porta %d...\n", hostname, port);
            
            client.socket = connect_to_server(hostname, port); 
            if(client.socket < 0){
                fprintf(stderr, "Impossibile connettersi al server. \n");
                return 1;
            }
            printf("Connesso al server!\n");
            
            if(register_nickname(&client) == 0){
                int theme_result;
                int continue_session = 1;
                
                while (continue_session)
                {
                    theme_result = select_theme(&client);     
                    if(theme_result == 0){
                        play(&client);
                    } else if(theme_result == -1){
                        continue_session = 0;
                    } 
                }
            }
            
            printf("\nSessione terminata\n");
            close(client.socket);
        }
        else if(choice == 0){
            printf("\nArrivederci!\n");
            break;
        }
        else{
            printf("Scelta non valida. Riprova.\n");
        }
    }
    return 0;
}

/**
 * Connette il client al server specificato 
 * @param hostname L'indirizzo IP o hostname del server
 * @param port La porta su cui connettersi
 * @return Il socket connesso o -1 in caso di errore
 */
int connect_to_server(char* hostname, int port){
    struct sockaddr_in server_addr;

    //Crea il socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0){
        perror("Errore nella creazione del socket");
        return -1;
    }

    //Configura l'indirizzo del server
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if(inet_pton(AF_INET, hostname, &server_addr.sin_addr) <= 0){
        perror("Indirizzo non valido o non supportato");
        close(sock);
        return -1;
    }
    //Connetti al server
    if(connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("Errore nella connessione al server");
        close(sock);
        return -1;
    }
    return sock;
}
/// Mostra il menu principale del client
void menu(){
    system("clear");
    printf("=== TRIVIA QUIZ CLIENT ===\n");
    printf("1. Comincia una sessione\n");
    printf("0. Esci\n");
    printf("Scelta: ");
}

/**
 * Registra il nickname del client presso il server
 * @param client Puntatore alla struttura ClientStatus del client
 * @return 0 se la registrazione ha successo, -1 in caso di errore
 */
int register_nickname(ClientStatus *client){
    char type[64], data[MAX_MSG_LEN];

    system("clear");
    while (1)
    {
        printf("=== REGISTRAZIONE NICKNAME ===\n");
        printf("Inserisci il tuo nickname (solo lettere e numeri, max %d caratteri): \n", MAX_NICKNAME_LEN - 1);
        if(fgets(client->nickname, sizeof(client->nickname), stdin) == NULL){
            return -1;
        }
        
        // Se l'input è troppo lungo, pulisci il buffer
        if(strlen(client->nickname) > 0 && client->nickname[strlen(client->nickname)-1] != '\n') {
            clear_input_buffer();
            printf("Nickname troppo lungo. Riprova.\n");
            sleep(1);
            continue;
        }
        
        trim_newline(client->nickname);

        if(!valid_nickname(client->nickname)){
            printf("Nickname non valido. Riprova.\n");
            continue;
        }

        //invia nickname al server
        if(send_msg(client->socket, MSG_NICK, client->nickname) < 0){
            printf("Errore nell'invio del nickname.\n");
            return -1;
        }

        if(recv_msg(client->socket, type,data) < 0){
            printf("Errore nella ricezione della risposta.\n");
            return -1;
        }

        if(strcmp(type, MSG_OK) == 0){
            printf("Nickname '%s' registrato con successo!\n", client->nickname);
            return 0;
        } else if(strcmp(type, MSG_ERROR) == 0){
            if(strcmp(data, RESP_NICK_TAKEN) == 0){
                printf("Nickname già in uso. Sceglierne uno differente.\n");
            } else{
                printf("Errore: %s \n", data);
            }
        }
    }
}

/**
 * Mostra il menu di selezione del tema e gestisce la scelta
 * @param client Puntatore alla struttura ClientStatus del client
 * @return 0 se un tema è stato selezionato con successo, -1 per terminare la sessione, 1 per ripetere la selezione
 */
int select_theme(ClientStatus* client){
    char type[64], data[MAX_MSG_LEN], theme_choice[16];
    int choice;
        
    if(send_msg(client->socket, MSG_THEMES, "") < 0){
        //printf("Errore nella richiesta temi.\n");
        return -1;
    }

    //riceve quanti temi ci sono
    
    if(recv_msg(client->socket, type, data) < 0 ){
        //printf("Errore nella ricezione dei temi.\n");
        return -1;
    }

    if(strcmp(type, MSG_OK) != 0){
        // printf("Errore: Lista temi non ricevuta\n");
        return 1;
    }

    
    themes_count = atoi(data);
    if(themes_count <= 0){
        printf("Nessun tema disponibile.\n");
        send_msg(client->socket, MSG_END, "");
        return -1;
    }
    send_msg(client->socket, MSG_OK, "");

    //riceve la lista dei temi
    if(recv_msg(client->socket, type, data) < 0 ){
        // printf("Errore nella ricezione dei temi.\n");
        return 1;
    }
    printf("\n=== SELEZIONE TEMA ===\n");
    print_formatted_list(data, "Temi disponibili:");

    printf("Scegli un tema (numero), 'show score' per classifica o 'endquiz' per uscire: ");
    
    char input[64];
    if(fgets(input, sizeof(input), stdin) == NULL) {
        return 1;
    }
    
    // Se l'input è troppo lungo, pulisci il buffer
    if(strlen(input) > 0 && input[strlen(input)-1] != '\n') {
        clear_input_buffer();
        printf("Input troppo lungo. Riprova.\n");
        send_msg(client->socket, MSG_THEMES, "");
        return 1;
    }
    
    trim_newline(input);
    
    // Gestione comandi speciali
    if(strcmp(input, "show score") == 0) {
        // Richiedi classifica al server
        if(send_msg(client->socket, MSG_SCORE, "") < 0)
        {
            printf("Errore nell'invio richiesta classifica.\n");
            return 1;
        }

        // Ricevi e visualizza le classifiche per tutti i temi
        while(1){            
            // Ricevi il prossimo messaggio dal server
            if(recv_msg(client->socket, type, data) < 0) {
                printf("Errore ricezione classifica.\n");
                return 1;
            }
            
            if(strcmp(type, MSG_SCORELIST) == 0) {
                // Messaggio contiene una classifica
                if (strlen(data) > 0) {
                    // Primo carattere: numero del tema
                    int t = data[0] - '0';
                    char title[64];
                    snprintf(title, sizeof(title), "=== CLASSIFICA TEMA %d ===", t);
                    
                    if (strlen(data) > 1) {
                        // C'è una classifica da mostrare (rimuovi il carattere del tema)
                        memmove(data, data + 1, strlen(data));
                        print_formatted_list(data, title);
                    } else {
                        // Classifica vuota per questo tema
                        printf("%s\n", title);
                        printf("Nessun punteggio disponibile per questo tema.\n\n");
                    }
                }
                // Conferma ricezione al server
                send_msg(client->socket, MSG_OK, "");
            }
            else if (strcmp(type, MSG_END_SCORE) == 0)
            {
                // Tutte le classifiche sono state ricevute
                printf("\nPremi Invio per continuare...");
                clear_input_buffer();
                return 1; // Torna al menu selezione tema
            }
        }   
    }
    if(strcmp(input, "endquiz") == 0) {
        // Invia comando di terminazione al server
        if(send_msg(client->socket, MSG_END, "") < 0) {
            printf("Errore nell'invio comando terminazione.\n");
        }
        printf("Uscita dalla sessione richiesta.\n");
        sleep(1); // Pausa per permettere di leggere l'output
        return -1; // Termina la sessione
    }
    
    // Conversione input numerico
    if(!is_numeric(input)){
        printf("Input non valido. Riprova.\n");
        send_msg(client->socket, MSG_THEMES, ""); // Richiedi nuovamente la lista temi
        return 1;
    }

    choice = atoi(input);
    if(choice < 0 || choice >= themes_count){
        printf("Scelta non valida. Riprova.\n");
        send_msg(client->socket, MSG_THEMES, ""); // Richiedi nuovamente la lista temi
        return 1;
    }

    client->theme = choice;

    snprintf(theme_choice, sizeof(theme_choice), "%d", choice);
    
    //invia tema scelto
    if(send_msg(client->socket, MSG_THEME, theme_choice) < 0 ){
        printf ("Errore nell'invio del tema\n");
        return 1;
    }

    //riceve conferma dal server
    if(recv_msg(client->socket, type, data) < 0){
        printf("Errore ricezione conferma\n");
        return 1;
    } 

    if(strcmp(type, MSG_OK) == 0){
        printf("Tema %d selezionato correttamente\n", client->theme);
        return 0;
    } else{
        printf("Errore nella selezione del tema: %s\n", data);
        return 1;
    }
}

/**
 * Gestisce il quiz per il client
 * @param client Puntatore alla struttura ClientStatus del client
 * @return 0 se il quiz è completato con successo, -1 in caso di errore
 */
int play(ClientStatus *client){
    char type[64], data[MAX_MSG_LEN];
    char input[64];
    int question_num = 1;

    system("clear");
    printf("====== QUIZ: 'Tema %d'======\n", client->theme);
    printf("Rispondi ai quesiti che ti verranno posti.\n");
    printf("Comandi: 'show score' per vedere la classifica, 'endquiz' per uscire \n");

    client->quiz_active = 1;

    if (send_msg(client->socket, MSG_QUIZ_START, "") < 0)
    {
        printf("Errore nell'avvio del quiz.\n");
        return -1;
    }

    while (client->quiz_active){

        
        if(recv_msg(client->socket, type, data) < 0){
            printf("Errore nella connessione.\n");
            break;
        }

        if(strcmp(type, MSG_QUESTION) == 0){
            printf("---- Domanda %d -----\n", question_num);
            printf("%s\n", data);
            // printf("La tua risposta / show score / endquiz \n");

            if(fgets(input, sizeof(input), stdin)){
                // Se l'input è troppo lungo, pulisci il buffer
                if(strlen(input) > 0 && input[strlen(input)-1] != '\n') {
                    clear_input_buffer();
                    printf("Input troppo lungo. Riprova.\n");
                    continue; // Richiedi nuovamente la stessa domanda
                }
                
                trim_newline(input);

                if(strcmp(input, "show score") == 0){
                    send_msg(client->socket, MSG_SCORE, "");
                } else if(strcmp(input, "endquiz") == 0){
                    send_msg(client->socket, MSG_END, "");
                    sleep(1); // Pausa per permettere di leggere l'output
                    client->quiz_active = 0;
                } else{
                    // invio della risposta 
                    send_msg(client->socket, MSG_ANSWER, input);
                    question_num++;
                }
            }
        } 
        else if(strcmp(type, MSG_RESULT) == 0){
            int result = show_result(data);
            if(result == 1){ // Quiz completato
                // Il quiz è completato, usciamo dal ciclo interno
                printf("\nTornando al menu di selezione dei temi...\n");
                client->quiz_active = 0;
                return 0; // Torniamo alla funzione chiamante (main) che gestirà una nuova selezione
            }
            send_msg(client->socket, MSG_OK, "");
            // Altrimenti invia richiesta per la prossima domanda
            send_msg(client->socket, MSG_QUIZ_START, "");
        }
        else if (strcmp(type, MSG_SCORELIST) == 0)
        {   
            // Ricevuta una classifica durante il quiz
            if (strlen(data) > 0) {
                // Estrai il numero del tema (primo carattere)
                int t = data[0] - '0';
                // Rimuovi il carattere del tema dai dati
                memmove(data, data + 1, strlen(data));
                
                // Formatta e mostra la classifica
                char title[64];
                snprintf(title, sizeof(title), "=== CLASSIFICA TEMA %d ===", t);
                print_formatted_list(data, title);
            }
            // Conferma ricezione al server
            send_msg(client->socket, MSG_OK, "");
        }
        else if (strcmp(type, MSG_END_SCORE) == 0)
        {
            // Tutte le classifiche sono state visualizzate
            printf("\nPremi Invio per continuare...");
            getchar();
            // Richiedi la prossima domanda del quiz
            send_msg(client->socket, MSG_QUIZ_START, "");
        }
        else if(strcmp(type, MSG_END) == 0){
            printf("Il server si è disconesso. Quiz terminato dal server.\n");
            sleep(1); // Pausa per permettere di leggere l'output
            break;
        }
        else if (strcmp(type, MSG_ERROR) == 0)
        {
            printf("Errore del server: %s\n", data);
            break;
        }
    }
    return 0;
}

/**
 * Mostra il risultato della risposta del client
 * @param result La stringa di risultato ricevuta dal server
 * @return 1 se il quiz è completato, 0 altrimenti
 */
int show_result(const char* result){
    if(strcmp(result, RESP_CORRECT) == 0){
        printf("Risposta esatta!\n");
        sleep(1); // Pausa per leggere il feedback
        return 0;
    }
    else if(strcmp(result, RESP_WRONG) == 0){
        printf("Risposta errata.\n");
        sleep(1); // Pausa per leggere il feedback
        return 0;   
    }
    else if(strcmp(result, RESP_QUIZ_COMPLETE) == 0){
        printf("Hai risposto a tutte le domande sul tema! Congratulazioni! \n");
        printf("Il quiz è terminato.\n");
        sleep(1); // Pausa più lunga per messaggio di completamento
        return 1;
    } 
    else{
        printf("Risultato sconosciuto: %s\n", result);
        return 0;
    }
}