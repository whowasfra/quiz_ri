#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

static FILE* log_file = NULL;

void init_logger(const char* filename) {
    // Chiudi un eventuale file già aperto
    if (log_file != NULL) {
        fclose(log_file);
    }
    
    // Apri il file di log in modalità append
    log_file = fopen(filename, "a");
    if (log_file == NULL) {
        fprintf(stderr, "ERRORE: Impossibile aprire il file di log %s\n", filename);
        return;
    }
    
    // Scrivi intestazione all'avvio
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[64];
    
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
    fprintf(log_file, "\n[%s] === SERVER AVVIATO ===\n", timestamp);
    fflush(log_file);
}

void close_logger(void) {
    if (log_file != NULL) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char timestamp[64];
        
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
        fprintf(log_file, "[%s] === SERVER TERMINATO ===\n", timestamp);
        fclose(log_file);
        log_file = NULL;
    }
}

void log_message(LogLevel level, const char* format, ...) {
    if (log_file == NULL) {
        return;
    }
    
    // Ottieni timestamp
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
    
    // Prefisso basato sul livello
    const char* level_str;
    switch(level) {
        case LOG_INFO:
            level_str = "INFO";
            break;
        case LOG_WARNING:
            level_str = "WARNING";
            break;
        case LOG_ERROR:
            level_str = "ERROR";
            break;
        default:
            level_str = "UNKNOWN";
    }
    
    // Scrivi l'intestazione del messaggio
    fprintf(log_file, "[%s] [%s] ", timestamp, level_str);
    
    // Scrivi il messaggio formattato
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    
    // Aggiungi newline se necessario
    if (format[strlen(format) - 1] != '\n') {
        fprintf(log_file, "\n");
    }
    
    // Flush immediato per assicurarci che il messaggio venga scritto
    fflush(log_file);
}