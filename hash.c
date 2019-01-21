/*
 * Si dichiara che il contenuto di questo file è in ogni sua parte opera
 * originale dell'autore
 * 
 * Giulio Purgatorio - 516292
*/

#include <stdlib.h>
#include <stdio.h>
#include "hash.h"

// from stackoverflow
unsigned long hash(unsigned char *str) {
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}


op_t insert_user(char* nickname, int fd){        //check #utenti? chatty_stats etc
    
    if(lookup_user(nickname)==TRUE)
        return OP_NICK_ALREADY;
    
    int key = hash((unsigned char*)nickname)%chatty_users.size;
    user *new=malloc(sizeof(user));
    new->nickname=calloc(sizeof(char)*MAX_NAME_LENGTH+1, sizeof(char));
    strcpy(new->nickname, nickname);
    new->history = malloc(sizeof(message_t)*MaxHistMsgs);
    memset(new->history, 0, sizeof(message_t)*(MaxHistMsgs));   //test4
    for(int i=0;i<MaxHistMsgs;i++) {
        new->history->data.hdr.len = 0;
    }
    new->oldest_msg = -1;
    new->last_msg = 0;
    new->online = TRUE;

    pthread_mutex_lock(&chatty_users.locks[key]);
    new->next=chatty_users.users[key];
    chatty_users.users[key]=new;
    pthread_mutex_unlock(&chatty_users.locks[key]);

    return OP_OK;
}

bool_t lookup_user(char* nickname){
    
    int key = hash((unsigned char*)nickname)%chatty_users.size;        
    bool_t flag = FALSE;

    pthread_mutex_lock(&chatty_users.locks[key]);
    if(chatty_users.users[key]!=NULL){
        user *a = chatty_users.users[key];
        while(a != NULL){
            if(strcmp(a->nickname,nickname)==0){
                flag = TRUE;
                break;
            }
        a=a->next;
        }
    }
    pthread_mutex_unlock(&chatty_users.locks[key]);

    return flag;
}
/*
//verifica che un utente sia registrato E online
op_t authorized_user(char *nickname, int *fd){
    
    int key = hash(nickname)%chatty_users.size;

    pthread_mutex_lock(&chatty_users.locks[key]);
    if(chatty_users.users[key]!=NULL) {
        user *corr = chatty_users.users[key], *prec = corr;
        op_t res = OP_NICK_UNKNOWN;
        while(corr != NULL && res == OP_NICK_UNKNOWN){
            if(strcmp(corr->nickname,nickname)==0){
                if(corr->online == TRUE) {
                    res = OP_OK;
                    break;
                }
                else
                    res = OP_FAIL;	
            }
            corr=corr->next;
        }
        if(res == OP_OK && fd != NULL) 
            *fd = corr->fd;
        pthread_mutex_unlock(&chatty_users.locks[key]);
        return res;
    }   
    pthread_mutex_unlock(&chatty_users.locks[key]);
    return OP_NICK_UNKNOWN;
}
*/

op_t delete_user(char* nickname) {

    int key = hash((unsigned char*)nickname) % chatty_users.size;

    pthread_mutex_lock(&chatty_users.locks[key]);
    if(chatty_users.users[key]!=NULL){
        user *corr = chatty_users.users[key], *prec = corr;
        while(corr!=NULL){
            if(strcmp(corr->nickname,nickname)==0){
                if(prec==corr){
                    chatty_users.users[key]=corr->next;
                    for(int i = 0; i< MaxHistMsgs; i++) {
                        if(corr->history[i].data.hdr.len != 0)
                            free(corr->history[i].data.buf);
                    }
                    free(corr);
                    pthread_mutex_unlock(&chatty_users.locks[key]);
                    return OP_OK;
                }
                prec->next=corr->next;
                free(corr);
                pthread_mutex_unlock(&chatty_users.locks[key]);
                return OP_OK;
            }
            corr = corr->next;
        }
    }

    pthread_mutex_unlock(&chatty_users.locks[key]);
    return OP_NICK_UNKNOWN;
}

op_t store_message_unsafe(char* nickname, message_t mess){

    int key = hash((unsigned char*)nickname)%chatty_users.size;

    if(chatty_users.users[key]!=NULL){
        user *a = chatty_users.users[key];
        int index = (a->last_msg) %MaxHistMsgs;         //punta alla posizione disponibile
        if(a->oldest_msg == -1)                         //punta al messaggio più vecchio
            a->oldest_msg = 0;
        else if(a->last_msg == a->oldest_msg)
            a->oldest_msg = (a->oldest_msg + 1) %MaxHistMsgs;
        while(a!=NULL){
            if(strcmp(a->nickname, nickname) == 0) {
                if(a->history[index].data.hdr.len != 0) {
                    free(a->history[index].data.buf);
                    a->history[index].data.buf = NULL;
                }
                a->history[index] = mess;
                if(mess.data.hdr.len != 0) {
                    a->history[index].data.buf = calloc(sizeof(char), mess.data.hdr.len);
                    memcpy(a->history[index].data.buf, mess.data.buf, sizeof(char)*mess.data.hdr.len);
                    a->last_msg++;
                }
                return OP_OK;
            }
            a=a->next;
        }
    }
    return OP_NICK_UNKNOWN;
}

int sendAll(message_t msg){

    int c = 0;
    for(int i = 0;i<chatty_users.size;i++) {

        pthread_mutex_lock(&chatty_users.locks[i]);
        user *corr = chatty_users.users[i];
        
        while(corr!=NULL){
            
            message_t msg_sent;
            setHeader(&msg_sent.hdr, TXT_MESSAGE, corr->nickname);
            setData(&msg_sent.data, corr->nickname, msg.data.buf, msg.data.hdr.len);
            if(!corr->online) {
                char* nick_of = corr -> nickname;
                store_message_unsafe(nick_of, msg_sent);   
                c++;
            }
            corr = corr -> next;
        }
        pthread_mutex_unlock(&chatty_users.locks[i]);
    }	
    return c;
}

op_t store_message(char* nickname, message_t mess){

    int key = hash((unsigned char*)nickname)%chatty_users.size;
    pthread_mutex_lock(&chatty_users.locks[key]);
    op_t res = store_message_unsafe(nickname, mess);
    pthread_mutex_unlock(&chatty_users.locks[key]);
    return res;
}



//imposta stato online/offline di un utente
op_t update_state(char* nickname, bool_t state){    //DONE CHECK:,int fd_new

    int key = hash((unsigned char*)nickname)%chatty_users.size;

    pthread_mutex_lock(&chatty_users.locks[key]);
    if(chatty_users.users[key]!=NULL){
        user *a = chatty_users.users[key];
        while(a != NULL) {
            if(strcmp(a->nickname, nickname) == 0) {
                a->online = state;
                pthread_mutex_unlock(&chatty_users.locks[key]);
                return OP_OK;
            }
            a = a->next;
        }
    }

    pthread_mutex_unlock(&chatty_users.locks[key]);
    return OP_NICK_UNKNOWN;
}

void printDB(){
  user *corr = NULL;
  for(int i=0;i < chatty_users.size;i++){
    corr = chatty_users.users[i];
    while(corr!=NULL){
      printf("Username %s lista %d\n",corr->nickname,i);
      corr=corr->next;
    }
  }
}

message_t *get_messages(char* nickname , int *oldest){
    int key = hash((unsigned char*)nickname) % chatty_users.size;
    pthread_mutex_lock(&chatty_users.locks[key]);
    if(chatty_users.users[key]!=NULL){
        user *a = chatty_users.users[key];
        while(a!=NULL){
            if(strcmp(a->nickname, nickname)==0) {
                *oldest = a->oldest_msg;
                pthread_mutex_unlock(&chatty_users.locks[key]);
                return a->history;
            }
            a=a->next;
        }
    }
    pthread_mutex_unlock(&chatty_users.locks[key]);
    return NULL;    //non c'è il tizio
}

void resetMessage(char *nickname, int oldest) {
    int key = hash((unsigned char*)nickname) % chatty_users.size;
    pthread_mutex_lock(&chatty_users.locks[key]);
    if(chatty_users.users[key] != NULL) {
        user *a = chatty_users.users[key];
        while(a != NULL) {
            if(strcmp(a->nickname, nickname) == 0) {
                a->history[oldest].data.hdr.len = 0;
                free(a->history[oldest].data.buf);
                a->history[oldest].data.buf = NULL;
            }
            a=a->next;
        }
    }
    pthread_mutex_unlock(&chatty_users.locks[key]);
}

void resetIndexes(char *nickname) {
    int key = hash((unsigned char*)nickname) % chatty_users.size;
    pthread_mutex_lock(&chatty_users.locks[key]);
    if(chatty_users.users[key] != NULL) {
        user *a = chatty_users.users[key];
        while(a != NULL) {
            if(strcmp(a->nickname, nickname) == 0) {
                a->oldest_msg = -1;
                a->last_msg = 0;
            }
            a=a->next;
        }
    }
    pthread_mutex_unlock(&chatty_users.locks[key]);
}

void freeDB(){
    for(int i = 0;i<chatty_users.size;i++){

        pthread_mutex_lock(&chatty_users.locks[i]);
        user *corr = chatty_users.users[i];
        user *prec = corr;

        while(corr!=NULL){
            corr=corr->next;
            free(prec->nickname);
            for(int j = 0; j < MaxHistMsgs; j++) {
                if(prec->history[j].data.hdr.len != 0)
                    free(prec->history[j].data.buf);
            }
            free(prec->history);
            free(prec);
            prec=corr;
        }
        pthread_mutex_unlock(&chatty_users.locks[i]);
    }
}