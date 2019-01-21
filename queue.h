/*
 * Si dichiara che il contenuto di questo file è in ogni sua parte opera
 * originale dell'autore
 * 
 * Giulio Purgatorio - 516292
*/

#ifndef MY_QUEUE_H
#define MY_QUEUE_H

#include <pthread.h>

typedef struct lista {
    int fd;
    struct lista *next;
} list;

pthread_mutex_t fd_mtx;
pthread_cond_t fd_cond;

list *fd_head;
list *fd_tail;

void push(int fd);

int pop();

#endif