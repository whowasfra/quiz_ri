#include "../shared/protocol.h"
#include "quiz.h"
#include "server.h"
#include "logger.h"
#include <ctype.h>

// Gestore di segnali semplice per i processi client
static void client_signal_handler(int sig) {
    if (sig == SIGPIPE) {
        // Client disconnesso - termina silenziosamente
        exit(0);
    }
}

/**
 * Pulisce le risorse associate a un client e termina il processo figlio
 * Chiamata quando un client si disconnette o si verifica un errore
 * 
 * @param socket Il socket del client da chiudere
 * @param nickname Il nickname del client da rimuovere dalla memoria condivisa
 * @param registered Flag che indica se il client era registrato (1) o no (0)
 */
static void cleanup_and_exit(int socket, const char *nickname, int registered)
{
    // Chiude il socket del client
    clean_up_socket(socket);

    // Se il client era registrato, rimuovilo dalla lista dei giocatori attivi
    if (registered && nickname && strlen(nickname) > 0)
    {
        LOG_INFO("Chiusura connessione per il client %s", nickname);
        remove_player(nickname);
        
        // Aggiorna la visualizzazione dello stato dei giocatori sulla console del server
        print_players_status();
    }

    // Termina il processo figlio (non influenza il server principale)
    exit(0);
}

/**
 * Gestisce la comunicazione con un client
 * @param client_socket Il socket del client
 */
extern void handle_client(int client_socket)
{
    char type[64], data[MAX_MSG_LEN];
    char nickname[MAX_NICKNAME_LEN] = {0};
    int score[MAX_THEMES] = {0};
    int current_question = 0;
    int count = 0;
    int quiz_active = 1;
    Quiz quiz = {0};

    // Registra gestore segnali per questo processo client
    signal(SIGPIPE, client_signal_handler);

    // Per gestire la pulizia quando il client si disconnette
    int client_registered = 0;

    LOG_INFO("Gestione client iniziata");

    // Registrazione nickname
    while (1)
    {
        if (recv_msg(client_socket, type, data) < 0)
        {
            LOG_WARNING("Client disconnesso durante la registrazione");
            cleanup_and_exit(client_socket, NULL, 0);
        }

        print_players_status();

        if (strcmp(type, MSG_NICK) == 0)
        {
            if (!valid_nickname(data))
            {
                send_msg(client_socket, MSG_ERROR, "Nickname non valido");
                continue;
            }
            if (taken_nickname(data))
            {
                send_msg(client_socket, MSG_ERROR, RESP_NICK_TAKEN);
                continue;
            }

            strcpy(nickname, data);
            send_msg(client_socket, MSG_OK, "Nickname registrato con successo.");
            LOG_INFO("Nickname registrato: %s", nickname);
            break;
        }
    }

    if (init_player(nickname) != 0)
    {
        LOG_ERROR("Errore nella registrazione del giocatore: %s", nickname);
        exit(0);
    }

    // Segna il client come registrato
    client_registered = 1;

    while (quiz_active)
    {
        // Invio lista temi
        if (themes_count <= 0)
        {
            LOG_ERROR("Errore nel recupero dei temi");
            send_msg(client_socket, MSG_ERROR, "Nessun tema disponibile");
            cleanup_and_exit(client_socket, nickname, client_registered);
        }

        if (recv_msg(client_socket, type, data) < 0)
        {
            LOG_WARNING("Client %s disconnesso durante la richiesta dei temi", nickname);
            cleanup_and_exit(client_socket, nickname, client_registered);
        }

        print_players_status();

        if (strcmp(type, MSG_THEMES) != 0)
        {
            printf("Errore: richiesta temi non valida da %s\n", nickname);
            continue; 
        }

        // invia il numero di temi disponibili
        sprintf(data, "%d", themes_count);
        send_msg(client_socket, MSG_OK, data);

        if(recv_msg(client_socket, type, data)<0){
            LOG_WARNING("Client %s disconnesso durante la selezione del tema", nickname);
            cleanup_and_exit(client_socket, nickname, client_registered);
        }
        if(strcmp(type, MSG_OK) != 0){
            // printf("Errore: Lista temi non ricevuta\n");
            continue;
        }

        // Prepara la lista dei temi da inviare al client
        // Ogni tema viene marcato come [COMPLETATO] se il giocatore l'ha già finito
        char themes_list[MAX_MSG_LEN] = {0};
        int remaining = MAX_MSG_LEN - 1;     // Spazio rimanente nel buffer
        char *current = themes_list;         // Puntatore alla posizione corrente nel buffer

        for (count = 0; count < themes_count; count++)
        {
            int written;

            // Controlla se il giocatore ha completato questo tema
            if (has_completed_quiz(nickname, count))
            {
                written = snprintf(current, remaining, "%d. %s [COMPLETATO]\\n", count, theme[count]);
            }
            else
            {
                written = snprintf(current, remaining, "%d. %s\\n", count, theme[count]);
            }

            // Verifica che ci sia ancora spazio nel buffer
            if (written >= 0 && written < remaining)
            {
                current += written;      // Sposta il puntatore
                remaining -= written;    // Aggiorna lo spazio rimanente
            }
            else
            {
                // Buffer pieno, tronca la lista
                LOG_WARNING("Lista temi troppo lunga, troncata");
                break;
            }
        }
        // invia la lista dei temi
        send_msg(client_socket, MSG_THEMES_LIST, themes_list);

        // Riceve il tema scelto o un comando speciale
        if (recv_msg(client_socket, type, data) < 0)
        {
            LOG_WARNING("Client %s disconnesso durante la selezione del tema", nickname);
            cleanup_and_exit(client_socket, nickname, client_registered);
        }

        print_players_status();

        // Gestione comando di visualizzazione classifica durante selezione tema
        // Il client può richiedere di vedere le classifiche senza selezionare un tema
        if (strcmp(type, MSG_SCORE) == 0)
        {

            LOG_INFO("Client %s ha richiesto la classifica", nickname);

            // Invia le classifiche per ogni tema disponibile
            for (int i = 0; i < themes_count; i++)
            {
                // Recupera la classifica per questo tema
                get_leaderboard(i, data);
                
                if (strlen(data) > 0)
                {
                    // Invia la classifica al client
                    send_msg(client_socket, MSG_SCORELIST, data);
                }
                else
                {
                    // Nessun punteggio disponibile, invia classifica vuota con solo il numero del tema
                    char empty_score[16];
                    snprintf(empty_score, sizeof(empty_score), "%d", i);
                    send_msg(client_socket, MSG_SCORELIST, empty_score);
                }
                
                // Attendi conferma di ricezione dal client prima di inviare la prossima
                if (recv_msg(client_socket, type, data) < 0)
                {
                    LOG_WARNING("Client %s disconnesso durante la ricezione della classifica", nickname);
                    cleanup_and_exit(client_socket, nickname, client_registered);
                }
                if (strcmp(type, MSG_OK) != 0)
                {
                    LOG_WARNING("Client %s ha inviato un messaggio inatteso durante la ricezione della classifica", nickname);
                }
            }
            
            // Invia un messaggio finale per indicare che tutte le classifiche sono state inviate
            send_msg(client_socket, MSG_END_SCORE, "");

            continue; // Torna alla selezione del tema
        }
        
        // Gestione comando di terminazione durante selezione tema
        if (strcmp(type, MSG_END) == 0)
        {
            LOG_INFO("Client %s ha richiesto di terminare la sessione durante la selezione tema", nickname);
            cleanup_and_exit(client_socket, nickname, client_registered);
        }

        if (strcmp(type, MSG_THEME) != 0)
        {
            continue;
        }

        int choice = atoi(data);
        LOG_INFO("Cliente %s ha scelto il tema numero: %d", nickname, choice);
        
        if (choice < 0 || choice >= themes_count)
        {
            send_msg(client_socket, MSG_ERROR, RESP_INVALID_THEME);
            continue;
        }

        if (has_completed_quiz(nickname, choice))
        {
            send_msg(client_socket, MSG_ERROR, "Questo quiz è già stato completato. Scegli un altro tema.");
            continue;
        }

        char filename[MAX_THEME_LEN + 8];
        snprintf(filename, sizeof(filename), "src/%s.txt", theme[choice]);
        LOG_INFO("Cliente %s ha scelto il tema: %s", nickname, theme[choice]);

        if (load_quiz(filename, &quiz) < 0)
        {
            send_msg(client_socket, MSG_ERROR, RESP_INVALID_THEME);
            continue;
        }
        send_msg(client_socket, MSG_OK, "");
        Question *q;
        current_question = 0;

        // QUIZ - Ciclo principale che gestisce tutte le domande del quiz
        while (current_question < quiz.count)
        {
            if (recv_msg(client_socket, type, data) < 0)
            {
                LOG_WARNING("Client %s disconnesso durante il quiz alla domanda %d", nickname, current_question + 1);
                cleanup_and_exit(client_socket, nickname, client_registered);
            }

            print_players_status();
            
            if (strcmp(type, MSG_QUIZ_START) == 0)
            {
                // Il client richiede la prossima domanda (o la stessa se ha chiesto la classifica)
                q = get_question(&quiz, current_question);
                if (!q)
                    break;

                send_msg(client_socket, MSG_QUESTION, q->question);
            }
            else if (strcmp(type, MSG_ANSWER) == 0)
            {
                // Il client ha inviato una risposta, verificala
                int correct = check_answer(q, data);

                if (correct)
                {
                    score[choice]++;
                    send_msg(client_socket, MSG_RESULT, RESP_CORRECT);
                    LOG_INFO("Client %s ha risposto alla domanda CORRETTAMENTE. ", nickname);
                }
                else
                {
                    send_msg(client_socket, MSG_RESULT, RESP_WRONG);
                    LOG_INFO("Client %s ha risposto in modo ERRATO alla domanda. ", nickname);
                }
                
                current_question++;
                
                // Verifica se il quiz è stato completato (tutte le domande risposte)
                int quiz_completed = (current_question >= quiz.count);
                
                // Salva il punteggio nella memoria condivisa
                // Se quiz_completed=1, il tema viene marcato come completato
                save_score(choice, nickname, score[choice], quiz_completed);
            }
            else if (strcmp(type, MSG_SCORE) == 0)
            {
                // Il client ha richiesto la classifica durante il quiz
                LOG_INFO("Client %s ha richiesto la classifica", nickname);

                // Invia le classifiche per ogni tema
                for (int i = 0; i < themes_count; i++)
                {
                    get_leaderboard(i, data);
                    if (strlen(data) > 0)
                    {
                        send_msg(client_socket, MSG_SCORELIST, data);
                    }
                    if (recv_msg(client_socket, type, data) < 0)
                    {
                        LOG_WARNING("Client %s disconnesso durante la ricezione della classifica", nickname);
                        cleanup_and_exit(client_socket, nickname, client_registered);
                    }
                    if (strcmp(type, MSG_OK) != 0)
                    {
                        LOG_WARNING("Client %s ha inviato un messaggio inatteso durante la ricezione della classifica", nickname);
                        cleanup_and_exit(client_socket, nickname, client_registered);
                    }
                }
                // Invia un messaggio finale per indicare che tutte le classifiche sono state inviate
                send_msg(client_socket, MSG_END_SCORE, "");
            }
            else if (strcmp(type, MSG_END) == 0)
            {
                // Il client ha scelto di terminare il quiz prematuramente
                LOG_INFO("Il client %s ha voluto chiudere il quiz", nickname);
                quiz_active = 0;
                break;
            }
        }

        if(quiz_active && current_question >= quiz.count){
            // Quiz completato: tutte le domande sono state risposte
            send_msg(client_socket, MSG_RESULT, RESP_QUIZ_COMPLETE);
        }
        // Fine del quiz, reset stato
        current_question = 0;
        memset(&quiz, 0, sizeof(Quiz));
        memset(score, 0, sizeof(score));
        
        // Aggiorna lo stato e mostra la lista giocatori aggiornata
        print_players_status();
        
        // Log per indicare che il client è pronto per una nuova selezione tema
        if (quiz_active) {
            LOG_INFO("Cliente %s pronto per una nuova selezione tema", nickname);
        }
    }

    // Pulisci e termina il processo client
    cleanup_and_exit(client_socket, nickname, client_registered);
}
