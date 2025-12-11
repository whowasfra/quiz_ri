#ifndef LOGGER_H
#define LOGGER_H

// Livelli di log
typedef enum {
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} LogLevel;

// Inizializza il logger
void init_logger(const char* log_file);

// Chiude il logger
void close_logger(void);

// Funzioni di logging
void log_message(LogLevel level, const char* format, ...);

// Macro helper per semplificare l'uso
#define LOG_INFO(format, ...) log_message(LOG_INFO, format, ##__VA_ARGS__)
#define LOG_WARNING(format, ...) log_message(LOG_WARNING, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) log_message(LOG_ERROR, format, ##__VA_ARGS__)

#endif /* LOGGER_H */