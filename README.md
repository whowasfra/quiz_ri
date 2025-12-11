# Quiz Server Client-Server 🎯

Un sistema di quiz interattivo multi-client implementato in C, sviluppato come progetto universitario per il corso di Reti Informatiche. Il sistema utilizza socket TCP/IP per la comunicazione client-server e meccanismi IPC (Inter-Process Communication) per la gestione concorrente di più utenti.

## 📋 Descrizione del Progetto

Il progetto implementa un'architettura client-server che permette a più utenti di partecipare simultaneamente a quiz su diversi temi. Il server gestisce le sessioni di gioco, mantiene uno stato condiviso tra i processi e traccia i punteggi dei giocatori.

### Caratteristiche Principali

- **Architettura Multi-Client**: Gestione concorrente di fino a 20 client tramite processi figli
- **IPC con Memoria Condivisa**: Utilizzo di shared memory System V per lo stato globale del server
- **Sincronizzazione**: Semafori POSIX per garantire l'accesso thread-safe alle risorse condivise
- **Protocollo Custom**: Comunicazione strutturata tramite messaggi con tipo e payload
- **Sistema di Logging**: Tracciamento dettagliato delle operazioni del server
- **Classifica Globale**: Mantenimento dei punteggi per tema con salvataggio persistente

## 🏗️ Architettura

### Componenti Server

- **server.c**: Core del server con gestione socket e processi
- **client_handler.c**: Logica di gestione delle sessioni client
- **quiz.c**: Gestione domande, risposte e punteggi
- **logger.c**: Sistema di logging con timestamp

### Componenti Client

- **client.c**: Interfaccia utente e logica di comunicazione
- **client.h**: Definizioni e prototipi

### Modulo Condiviso

- **protocol.c/h**: Definizioni del protocollo di comunicazione e utility

## 🎮 Funzionalità

### Lato Server

- Ascolto su porta configurabile (default: 8080)
- Gestione nickname univoci per sessione
- Caricamento dinamico dei quiz da file
- Validazione delle risposte (case-insensitive)
- Generazione classifica in tempo reale
- Prevenzione di quiz duplicati per utente
- Terminazione pulita con rilascio risorse IPC

### Lato Client

- Connessione a server remoto/locale
- Registrazione nickname
- Selezione tema da lista disponibile
- Interfaccia Q&A interattiva
- Visualizzazione punteggio e classifica
- Gestione errori di connessione

## 🛠️ Tecnologie e Concetti

- **Linguaggio**: C (C99)
- **Networking**: Berkeley sockets (TCP/IP)
- **IPC**: System V Shared Memory e Semaphores
- **Concorrenza**: Fork() per gestione multi-client
- **Signal Handling**: SIGINT, SIGPIPE per terminazione ordinata
- **File I/O**: Parsing quiz e persistenza punteggi

## 📂 Struttura File

```
.
├── Makefile              # Build automation
├── client/
│   ├── client.c         # Implementazione client
│   └── client.h         # Header client
├── server/
│   ├── server.c         # Server principale
│   ├── server.h         # Header server
│   ├── client_handler.c # Gestione sessioni
│   ├── quiz.c           # Logica quiz
│   ├── quiz.h           # Header quiz
│   ├── logger.c         # Sistema logging
│   └── logger.h         # Header logger
├── shared/
│   ├── protocol.c       # Utility protocollo
│   └── protocol.h       # Definizioni protocollo
└── src/
    ├── temi.txt         # Lista temi disponibili
    ├── Calabria.txt     # Quiz Calabria
    └── AGG.txt          # Quiz Aldo, Giovanni e Giacomo
```

## 🚀 Compilazione ed Esecuzione

### Prerequisiti

- GCC (GNU Compiler Collection)
- Sistema operativo POSIX-compliant (Linux/Unix)
- Make

### Compilazione

```bash
# Compila sia client che server
make

# Compila solo il client
make client_bin

# Compila solo il server
make server_bin
```

### Esecuzione

**Server:**
```bash
# Avvia il server sulla porta di default (8080)
./server_bin

# Oppure usa il target make
make run_server
```

**Client:**
```bash
# Connetti al server locale sulla porta 8080
./client_bin 8080

# Oppure usa il target make
make run_client
```

### Pulizia

```bash
make clean
```

## 📝 Formato Quiz

I quiz sono file di testo con il seguente formato:

```
Domanda numero uno?
Risposta corretta
---

Domanda numero due?
Risposta corretta
---
```

- Ogni quiz deve contenere almeno 5 domande
- Il separatore `---` delimita le coppie domanda-risposta
- I file devono essere salvati in `src/` con estensione `.txt`
- I temi disponibili sono listati in `src/temi.txt`

## 🔒 Sicurezza e Robustezza

- Validazione input utente per prevenire buffer overflow
- Controllo lunghezza nickname e messaggi
- Gestione errori di rete con retry
- Cleanup risorse in caso di interruzione
- Protezione accessi concorrenti con semafori

## 📊 Protocollo di Comunicazione

Il sistema utilizza un protocollo basato su messaggi con struttura:

```
TIPO|PAYLOAD
```

### Tipi di Messaggio

- `NICK`: Registrazione nickname
- `THEMES`: Richiesta lista temi
- `THEME`: Selezione tema
- `ANSWER`: Invio risposta
- `RESULT`: Esito risposta
- `SCORE`: Punteggio finale
- `SCORELIST`: Classifica

## 🎓 Obiettivi Didattici

Questo progetto dimostra la comprensione di:

- Programmazione di rete con socket TCP
- Gestione della concorrenza con processi
- Sincronizzazione tramite semafori
- Inter-Process Communication (IPC)
- Gestione memoria condivisa
- Parsing e validazione dati
- Architetture client-server

## 👤 Autore

**Francesco (@whowasfra)**  
Progetto universitario - Corso di Reti Informatiche  
Università, 2025

## 📄 Licenza

Questo progetto è stato sviluppato a scopo educativo per il corso di Reti Informatiche.
