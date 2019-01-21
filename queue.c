/*
 * Si dichiara che il contenuto di questo file è in ogni sua parte opera
 * originale dell'autore
 * 
 * Giulio Purgatorio - 516292
*/

#include <stdlib.h>
#include "queue.h"

pthread_mutex_t fd_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t fd_cond = PTHREAD_COND_INITIALIZER;

void push(int fd) {
    list *new = malloc(sizeof(list));
    new -> fd = fd;
    if(fd_head == NULL) { 
        new -> next = NULL;
        fd_head = new;
        fd_tail = new;
    }
    else {
        fd_tail -> next = new;
        fd_tail = new;
        new -> next = NULL;
    }
}

int pop() {
    if(fd_head == NULL)
        return -1;
    list *aux = fd_head;
    fd_head = fd_head -> next;
    
    int fd = aux -> fd;
    free(aux);
    return fd;
}