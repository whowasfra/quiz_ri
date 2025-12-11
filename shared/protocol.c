#include "../shared/protocol.h"

char theme[MAX_THEMES] [MAX_THEME_LEN]= {0};
int themes_count = 0;

/**
 * Formatta e stampa una stringa che contiene sequenze di escape \n
 * Utile per visualizzare elenchi e liste formattate ricevute dal server
 * 
 * @param str La stringa da formattare e stampare
 * @param title Titolo da mostrare prima dell'elenco (può essere NULL)
 */
void print_formatted_list(const char* str, const char* title) {
    if (str == NULL) {
        return;
    }
    
    // Stampa il titolo se fornito
    if (title != NULL) {
        printf("%s\n", title);
    }
    
    // Crea una copia della stringa per modificarla
    char formatted[MAX_MSG_LEN];
    strncpy(formatted, str, MAX_MSG_LEN - 1);
    formatted[MAX_MSG_LEN - 1] = '\0';  // Assicura terminazione
    
    // Sostituisce le sequenze di escape \n con veri newline
    for (int i = 0; formatted[i]; i++) {
        if (formatted[i] == '\\' && formatted[i + 1] == 'n') {
            formatted[i] = '\n';
            // Shifta la stringa rimuovendo il carattere 'n'
            memmove(&formatted[i + 1], &formatted[i + 2], strlen(&formatted[i + 2]) + 1);
        }
    }
    
    // Stampa la stringa formattata
    printf("\n%s\n", formatted);
}

/**
 * Rimuove il carattere newline finale da una stringa, se presente
 * @param str La stringa da modificare
 */
void trim_newline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && str[len-1] == '\n') {
        str[len-1] = '\0';
    }
}

/**
 * Verifica se un nickname è valido (alfanumerico e underscore, lunghezza valida)
 * @param nickname Il nickname da verificare
 * @return 1 se il nickname è valido, 0 altrimenti
 */
int valid_nickname(const char *nickname){
    if(!nickname || strlen(nickname) == 0 || strlen(nickname) >= MAX_NICKNAME_LEN){
        return 0;
    }

    //Verifico se alfanumerico e underscore
    for(int i = 0; nickname[i]; i++){
        if(!((nickname[i] >= 'a' && nickname[i] <= 'z') ||
             (nickname[i] >= 'A' && nickname[i] <= 'Z') ||
             (nickname[i] >= '0' && nickname[i] <= '9') ||
             nickname[i] == '_')){
            return 0;
        }
    }
    return 1;
}

/**
 * Verifica se una stringa rappresenta un numero intero positivo
 * 
 * @param str La stringa da verificare
 * @return 1 se la stringa è numerica, 0 altrimenti
 */
int is_numeric(const char* str) {
    if (str == NULL || *str == '\0') {
        return 0;
    }
    
    // Salta eventuali spazi iniziali
    while (*str == ' ' || *str == '\t') {
        str++;
    }
    
    // Controlla se tutti i caratteri sono cifre
    while (*str) {
        if (*str < '0' || *str > '9') {
            // Ignora spazi finali e newline
            if (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
                str++;
                continue;
            }
            return 0;
        }
        str++;
    }
    
    return 1;
}

/**
 * Invia un messaggio formattato tramite socket secondo il protocollo TIPO|LUNGHEZZA|DATI\n
 * Gestisce l'invio parziale richiamando send() finché tutti i byte sono inviati
 * 
 * @param socket Il socket su cui inviare il messaggio
 * @param type Il tipo di messaggio (es: MSG_NICK, MSG_QUESTION, ecc.)
 * @param data I dati del messaggio (può essere NULL per messaggi senza dati)
 * @return 0 se l'invio ha successo, -1 in caso di errore
 */
int send_msg (int socket, const char* type, char* data){
    if (type == NULL) {
        fprintf(stderr, "Errore: tipo messaggio NULL\n");
        return -1;
    }
    
    char message[MAX_MSG_LEN];
    int len = data ? strlen(data) : 0;  // Alcuni messaggi hanno dati vuoti
    
    // Formatta il messaggio: TIPO|LUNGHEZZA|DATI\n
    snprintf(message, sizeof(message), "%s|%d|%s\n", type, len, data ? data : "");

    int full_len = strlen(message);
    int sent = 0;

    // Invia tutto il messaggio, gestendo invii parziali
    while(sent < full_len){
        int res = send(socket, message + sent, full_len - sent, 0);
        if(res < 0){
            perror ("Errore invio messaggio");
            return -1;
        }
        sent += res;
    }
    // printf("Inviato: %s", message);  // DEBUG
    return 0;
}

/**
 * Riceve un messaggio formattato tramite socket e lo parse secondo il protocollo
 * Formato atteso: TIPO|LUNGHEZZA|DATI\n
 * 
 * @param socket Il socket da cui ricevere il messaggio
 * @param type Buffer per il tipo di messaggio ricevuto (output)
 * @param data Buffer per i dati del messaggio ricevuto (output)
 * @return 0 se la ricezione ha successo, -1 in caso di errore
 */
int recv_msg (int socket, char* type, char* data){
    char message[MAX_MSG_LEN];
    ssize_t received;
    char *first, *second, *newline; 

    // Ricevi i dati dal socket
    received = recv(socket, message, MAX_MSG_LEN - 1, 0);
    if(received < 0 ){
        perror("Errore nella ricezione del messaggio");
        return -1;
    }
    
    // Termina la stringa ricevuta
    message[received] = '\0';
    
    // printf("Ricevuto: %s", message); // DEBUG

    // Parse del messaggio: cerca i delimitatori '|' e '\n'
    
    // Trova il primo '|' (separa TIPO da LUNGHEZZA)
    first = strchr(message, '|');
    if(!first){
        return -1;
    }

    // Trova il secondo '|' (separa LUNGHEZZA da DATI)
    second = strchr(first + 1, '|');
    if(!second){
        return -1;
    }

    // Trova il newline finale
    newline = strchr(second, '\n');
    if(!newline){
        return -1;
    }

    // Estrae il TIPO (prima del primo '|')
    *first = '\0';
    if (type != NULL) {
        strncpy(type, message, MAX_MSG_LEN);
        type[MAX_MSG_LEN - 1] = '\0';  // Assicura terminazione
    }
    
    // Ignora la LUNGHEZZA (non rilevante per il parsing)
    *second = '\0';

    // Estrae i DATI (dopo il secondo '|', prima del '\n')
    *newline = '\0';

    if (data != NULL) {
        strncpy(data, second + 1, MAX_MSG_LEN);
        data[MAX_MSG_LEN - 1] = '\0';  // Assicura terminazione
    }
    
    return 0;
}

/**
 * Chiude in modo sicuro un socket
 * @param socket Il socket da chiudere
 */
void clean_up_socket(int socket){
    if(socket >= 0){
        close(socket);
    }
}
