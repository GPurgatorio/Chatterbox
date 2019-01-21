/*
 * Si dichiara che il contenuto di questo file è in ogni sua parte opera
 * originale dell'autore
 * 
 * Giulio Purgatorio - 516292
*/

#ifndef MY_WORKER_H
#define MY_WORKER_H

#include <pthread.h>
#include <sys/select.h>
#include <signal.h>
#include "config.h"
#include "stats.h"

extern pthread_mutex_t online_mtx;              //mutua esclusione per la lista degli utenti online
extern pthread_mutex_t stats_mtx;               //mutua esclusione per la struttura delle statistiche
extern online_users* onUsers;                   //la lista degli utenti online
extern void insert_online(char*, int);          //inserisce nella lista degli utenti online
extern void remove_online(char*);               //rimuove dalla lista degli utenti online
extern void freeOnline();                       //libera la lista degli utenti online
extern void printOnline();                      //stampa chi è presenta nella lista degli utenti online
extern void update_fd(char*, int);              //aggiorna il FD di un utente presente nella lista degli utenti online
extern char* reverse_search(int);               //trova, dal FD, il nickname dell'utente associato
extern int n_online;                            //numero di utenti online
extern int is_online(char *username);           //controlla se *username* è online
extern fd_set fdset;
extern int new_max;
extern pthread_mutex_t select_mtx;
extern struct statistics chattyStats;
extern volatile sig_atomic_t sigquit;

void *client_manager(void* par); 
void update_stats(stat_code code, int incr);    //aggiorna il campo *code* delle statistiche di *incr*
char *users_list();                             //ottiene la lista di persone online
int lock (char* username);                      //locka la lista di trabocco in cui *username* è nel database
int unlock (char* username);                    //unlocka la lista di trabocco in cui *username* è nel database 


#endif