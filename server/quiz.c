#include "quiz.h"
#include "logger.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/sem.h>

/**
 * Verifica se un nickname è già stato preso
 * @param nickname Il nickname da verificare
 * @return 1 se il nickname è già preso, 0 altrimenti
 */
int taken_nickname(const char *nickname){
    lock_shared_state();
    
    for(int i = 0; i < shared_state->player_count; i++){
        if(strcmp(shared_state->players[i].nickname, nickname) == 0){
            unlock_shared_state();
            return 1;
        }
    }
    
    unlock_shared_state();
    return 0;
}

/**
 * Carica un quiz da un file
 * @param filename Il nome del file del quiz
 * @param quiz Puntatore alla struttura Quiz da riempire
 * @return Numero di domande caricate, -1 in caso di errore
 */
int load_quiz(char *filename, Quiz* quiz){
    FILE* file  = fopen(filename, "r");
    if(!file){
        LOG_ERROR("Errore nell'apertura del file %s!", filename);
        return -1;
    }

    char line[MAX_QUESTION_LEN];
    quiz->count = 0;

    while(quiz->count < QUIZ_QUESTIONS && fgets(line, sizeof(line), file)){
        trim_newline(line);

        if(strlen(line) == 0 || strcmp(line, "---") == 0){
            continue;
        }

        Question* q = &quiz->questions[quiz->count];

        strcpy(q->question, line);

        if(fgets(line, sizeof(line), file)){
            trim_newline(line);
            strcpy(q->correct_answer, line);
        }

        quiz->count++;
    }

    fclose(file);
    return quiz->count;
}

/**
 * Recupera una domanda dal quiz
 * @param quiz Puntatore alla struttura Quiz
 * @param index Indice della domanda da recuperare
 * @return Puntatore alla struttura Question, o NULL se l'indice è invalido
 */
Question* get_question(Quiz* quiz, int index){
    if(index < 0 || index >= quiz->count){
        return NULL;
    }
    return &quiz->questions[index];
}

/**
 * Verifica la risposta data dall'utente
 * @param question Puntatore alla struttura Question
 * @param answer La risposta fornita dall'utente
 * @return 1 se la risposta è corretta, 0 altrimenti
 */
int check_answer (Question* question, const char* answer ){
    if(!question || !answer){ return 0;}

    char user_answer[MAX_ANSWER_LEN];
    char correct_answer[MAX_ANSWER_LEN];

    strcpy(user_answer, answer);
    strcpy(correct_answer, question->correct_answer);

    // Converte entrambe le stringhe in minuscolo per confronto case-insensitive
    for(int i = 0; user_answer[i]; i++){
        user_answer[i] = tolower(user_answer[i]);
    }

    for(int i = 0; correct_answer[i]; i++){
        correct_answer[i] = tolower(correct_answer[i]);
    }

    // Rimuove gli spazi iniziali e finali dalla risposta utente
    // Questo permette risposte come "  roma  " equivalenti a "roma"
    char* start = user_answer;

    // Salta gli spazi iniziali
    while(*start == ' ') {start++;}

    // Trova la fine della stringa
    char* end = start + strlen(start) - 1;

    // Rimuove gli spazi finali
    while(end > start && *end == ' '){ end--;}

    // Termina la stringa nel punto corretto
    *(end + 1) = '\0';

    // Confronta le stringhe processate
    return(strcmp(start, correct_answer) == 0);
}

/**
 * Recupera la classifica per un tema specifico
 * @param theme_num Il numero del tema
 * @param leaderboard Buffer dove salvare la classifica formattata
 */
void get_leaderboard(int theme_num, char* leaderboard){
    char entry[MAX_MSG_LEN];
    size_t remaining = MAX_MSG_LEN - 2; // Riserva spazio per l'indice del tema e il terminatore
    
    // Aggiunge il numero del tema all'inizio del messaggio
    leaderboard[0] = theme_num + '0';
    leaderboard[1] = '\0';
    
    // Copia locale dei dati dei giocatori per minimizzare il tempo di lock
    // Questo evita di tenere bloccata la memoria condivisa durante l'ordinamento
    Player local_players[MAX_CLIENTS];
    int local_player_count;
    
    // SEZIONE CRITICA: acquisisce lock, copia dati rapidamente, rilascia lock
    lock_shared_state();
    local_player_count = shared_state->player_count;
    for (int i = 0; i < local_player_count; i++) {
        local_players[i] = shared_state->players[i];
    }
    unlock_shared_state();
    
    // Ora lavoriamo sui dati locali senza bisogno di lock
    
    // Crea un array temporaneo per ordinare i giocatori
    Player sorted_players[MAX_CLIENTS];
    int valid_players = 0;
    
    // Copia solo i giocatori che hanno un punteggio valido per questo tema
    // (score != -1 indica che hanno giocato almeno una volta)
    for (int i = 0; i < local_player_count; i++) {
        if (local_players[i].score[theme_num] != -1) {
            sorted_players[valid_players++] = local_players[i];
        }
    }
    
    // Bubble sort per punteggio decrescente (migliore punteggio prima)
    Player temp;
    for (int i = 0; i < valid_players - 1; i++) {
        for (int j = 0; j < valid_players - i - 1; j++) {
            if (sorted_players[j].score[theme_num] < sorted_players[j + 1].score[theme_num]) {
                temp = sorted_players[j];
                sorted_players[j] = sorted_players[j + 1];
                sorted_players[j + 1] = temp;
            }
        }
    }

    // Verifica se ci sono giocatori validi
    if (valid_players == 0) {
        // Se non ci sono giocatori, aggiungi un messaggio dopo l'indice del tema
        strncat(leaderboard, "Nessun giocatore. \\n", remaining);
    } else {
        // Costruisci la stringa della classifica
        for (int i = 0; i < valid_players && remaining > 0; i++) {
            int written = snprintf(entry, sizeof(entry), "%d. %s: %d punti%s\\n", 
                    i + 1,
                    sorted_players[i].nickname, 
                    sorted_players[i].score[theme_num],
                    sorted_players[i].completed[theme_num] ? " (completato)" : "");
            
            if (written > 0 && (size_t)written < remaining) {
                strcat(leaderboard, entry);
                remaining -= written;
            } else {
                strncat(leaderboard, "...", remaining);
                break;
            }
        }
    }
    
    return;
}

/**
 * Inizializza un nuovo giocatore nella memoria condivisa
 * @param nickname Il nickname del giocatore
 * @return 0 se il giocatore è stato aggiunto con successo, -1 se non è stato aggiunto
 */
int init_player(const char* nickname){
    lock_shared_state();
    
    if(shared_state->player_count < MAX_CLIENTS){
        strcpy(shared_state->players[shared_state->player_count].nickname, nickname);
        for(int i = 0; i < MAX_THEMES; i++){
            shared_state->players[shared_state->player_count].score[i] = -1;
            shared_state->players[shared_state->player_count].completed[i] = 0;
        }
        shared_state->player_count++;
        unlock_shared_state();
        return 0;
    }
    
    unlock_shared_state();
    return -1;
}

/**
 * Rimuove un giocatore dalla memoria condivisa
 * Compatta l'array spostando tutti i giocatori successivi di una posizione indietro
 * 
 * @param nickname Il nickname del giocatore da rimuovere
 */
void remove_player(const char* nickname){
    lock_shared_state();
    
    int found = -1;
    
    // Trova l'indice del giocatore nell'array
    for(int i = 0; i < shared_state->player_count; i++){
        if(strcmp(shared_state->players[i].nickname, nickname) == 0){
            found = i;
            break;
        }
    }
    
    // Se il giocatore è stato trovato, rimuovilo compattando l'array
    if(found >= 0){
        // Sposta tutti i giocatori successivi di una posizione indietro
        // Questo riempie il "buco" lasciato dal giocatore rimosso
        for(int i = found; i < shared_state->player_count - 1; i++){
            strcpy(shared_state->players[i].nickname, shared_state->players[i + 1].nickname);
            for(int j = 0; j < MAX_THEMES; j++){
                shared_state->players[i].score[j] = shared_state->players[i + 1].score[j];
                shared_state->players[i].completed[j] = shared_state->players[i + 1].completed[j];
            }
        }
        
        // Decrementa il contatore dei giocatori
        shared_state->player_count--;
        
        LOG_INFO("Giocatore %s rimosso. Giocatori attivi: %d", nickname, shared_state->player_count);
    }
    
    unlock_shared_state();
}

/**
 * Salva il punteggio di un giocatore per un tema specifico
 * @param theme_num Il numero del tema
 * @param nickname Il nickname del giocatore
 * @param score Il punteggio da salvare
 * @param completed Indica se il quiz è stato completato (1) o no (0)
 */
void save_score(int theme_num, const char* nickname, int score, int completed){
    lock_shared_state();
    
    for(int i = 0; i < shared_state->player_count; i++){
        if(strcmp(shared_state->players[i].nickname, nickname) == 0){
            shared_state->players[i].score[theme_num] = score;
            shared_state->players[i].completed[theme_num] = completed;
            
            unlock_shared_state();
            // Mostra la classifica aggiornata dopo ogni aggiornamento di punteggio
            print_players_status();
            return;
        }
    }
    
    unlock_shared_state();
}

/**
 * Stampa lo stato aggiornato dei giocatori con sezioni formattate
 * Mostra: giocatori attivi, punteggi per ogni tema, quiz completati
 * Usa una copia locale dei dati per minimizzare il tempo di lock
 */
void print_players_status() {
    // Copia locale dei dati per evitare lock prolungato durante la visualizzazione
    Player local_players[MAX_CLIENTS];
    int local_player_count;
    
    // SEZIONE CRITICA: copia rapida dei dati dalla memoria condivisa
    lock_shared_state();
    local_player_count = shared_state->player_count;
    for (int i = 0; i < local_player_count; i++) {
        local_players[i] = shared_state->players[i];
    }
    unlock_shared_state();
    
    // Pulisce il terminale per una visualizzazione aggiornata
    system("clear");
    
    // Ora lavoriamo sui dati locali senza bisogno di lock
    printf("\n\n===== STATO ATTUALE DEL SERVER =====\n\n");
    
    // 1. Lista di tutti i giocatori attivi
    printf("GIOCATORI ATTIVI (%d):\n", local_player_count);
    if (local_player_count == 0) {
        printf("  Nessun giocatore attivo.\n");
    } else {
        for (int i = 0; i < local_player_count; i++) {
            printf("  - %s\n", local_players[i].nickname);
        }
    }
    printf("\n");

    // 2. Sezione punteggio per ogni tema
    for (int t = 0; t < themes_count; t++) {
        printf("PUNTEGGIO: TEMA '%s'\n", theme[t]);
        
        // Ordina i giocatori per punteggio decrescente
        Player sorted_players[MAX_CLIENTS];
        int valid_players = 0;
        
        // Copia solo i giocatori che hanno un punteggio valido per questo tema
        for (int i = 0; i < local_player_count; i++) {
            if (local_players[i].score[t] != -1) {
                sorted_players[valid_players++] = local_players[i];
            }
        }
        
        // Bubble sort per punteggio decrescente
        Player temp;
        for (int i = 0; i < valid_players - 1; i++) {
            for (int j = 0; j < valid_players - i - 1; j++) {
                if (sorted_players[j].score[t] < sorted_players[j + 1].score[t]) {
                    temp = sorted_players[j];
                    sorted_players[j] = sorted_players[j + 1];
                    sorted_players[j + 1] = temp;
                }
            }
        }
        
        // Stampa la classifica ordinata
        if (valid_players == 0) {
            printf("  Nessun giocatore ha ancora partecipato a questo tema.\n");
        } else {
            for (int i = 0; i < valid_players; i++) {
                printf("  %d. %s: %d punti%s\n", 
                       i + 1, 
                       sorted_players[i].nickname, 
                       sorted_players[i].score[t],
                       sorted_players[i].completed[t] ? " (completato)" : "");
            }
        }
        printf("\n");
    }
    
    // 3. Sezione giocatori che hanno completato i quiz per ogni tema
    printf("GIOCATORI CHE HANNO COMPLETATO I QUIZ:\n");
    
    for (int t = 0; t < themes_count; t++) {
        int completions = 0;
        printf("  Tema '%s':\n", theme[t]);
        
        for (int i = 0; i < shared_state->player_count; i++) {
            if (shared_state->players[i].completed[t]) {
                printf("    - %s (punteggio: %d)\n", 
                       shared_state->players[i].nickname, shared_state->players[i].score[t]);
                completions++;

            }
        }
        
        if (completions == 0) {
            printf("    Nessun giocatore ha completato questo quiz.\n");
        }
    }
    
    printf("\n=====================================\n\n");
}

/**
 * Verifica se un giocatore ha completato il quiz per un tema specifico
 * @param nickname Il nickname del giocatore
 * @param theme_index L'indice del tema
 * @return 1 se il quiz è stato completato, 0 altrimenti
 */
int has_completed_quiz(const char* nickname, int theme_index) {
    lock_shared_state();
    
    for (int i = 0; i < shared_state->player_count; i++) {
        if (strcmp(shared_state->players[i].nickname, nickname) == 0) {
            int completed = shared_state->players[i].completed[theme_index];
            unlock_shared_state();
            return completed;
        }
    }
    
    unlock_shared_state();
    return 0; // Giocatore non trovato o non completato
}