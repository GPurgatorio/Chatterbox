/*
 * Si dichiara che il contenuto di questo file è in ogni sua parte opera
 * originale dell'autore
 * 
 * Giulio Purgatorio - 516292
*/

#ifndef HASH_H
#define HASH_H

#include <pthread.h>
#include "config.h"
#include "connections.h"
#include "message.h"

typedef struct utente {
    char *nickname;             //nick avrà una dimensione fissa, in base al file di configurazione.
    int last_msg;               //ultimo messaggio
    int oldest_msg;             //messaggio più vecchio
    int online;                 //1 se utente online, 0 se offline
    message_t *history;         //history avrà una dimensione fissa, in base al file di configurazione.
    struct utente *next;
} user;

typedef struct tabella {
    struct utente **users;                              //array di puntatori a strutture utente
    pthread_mutex_t *locks;                             //array di locks, una per ogni lista di trabocco
    int size;
} users_DB;

users_DB chatty_users;                                  //chatty_users è il database degli utenti

unsigned long hash(unsigned char *str);                 //calcola il valore hash per la chatty_users

op_t insert_user(char* nickname,  int fd);              //inserisce in chatty_users *nickname, fd, etc.*

bool_t lookup_user(char* nickname);                     //controlla se esiste in chatty_users *nickname*

op_t delete_user(char* nickname);                       //cancella da chatty_users *nickname*

int sendAll(message_t msg);                             //invia a tutti gli utenti offline *msg*

op_t store_message(char* nickname,  message_t mess);    //salva un messaggio
op_t store_message_unsafe(char *nickname, message_t mess);

op_t update_state(char* nickname,  bool_t state);       //imposta stato utente = state (offline/online)

message_t *get_messages(char* nickname, int *oldest);   //ottiene i messaggi pendenti (ricevuti mentre era offline)

void printDB();                                         //funzione di debug

void resetMessage(char *nickname, int oldest);          //libera il messaggio più vecchio       

void resetIndexes(char *nickname);                      //resetta gli indici last_msg e oldest_msg 

void freeDB();                                          //libera chatty_users

#endif 
