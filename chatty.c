/*
 * membox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 * Si dichiara che il contenuto di questo file è in ogni sua parte opera
 * originale dell'autore
 * 
 * Giulio Purgatorio - 516292
 * 
 */
/*
 * @file chatty.c
 * @brief File principale del server chatterbox
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/select.h>
#include <unistd.h>

#include "config.h"
#include "connections.h"
#include "hash.h"
#include "message.h"
#include "queue.h"
#include "stats.h"
#include "worker.h"

//#define DEBUG

// struttura che memorizza le statistiche del server, struct statistics è definita in stats.h
struct statistics  chattyStats = { 0,0,0,0,0,0,0 };
pthread_mutex_t stats_mtx = PTHREAD_MUTEX_INITIALIZER;

volatile sig_atomic_t sigquit = 0; //1 -> quit
volatile sig_atomic_t sigUSR1 = 0; //1 -> print

//dalle slides, per aggiornare il parametro della select
static inline int updatemax(fd_set set, int fdmax) {
    for(int i=(fdmax-1);i>=0;--i)
	if (FD_ISSET(i, &set)) return i;
    return -1;
}

static void usage(const char *progname) {
    fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
    fprintf(stderr, "  %s -f conffile\n", progname);
}

static void handler(int signum) {
    sigquit = 1;
}

static void handlerUSR1(int signum) {
    sigUSR1 = 1;
}

pthread_mutex_t online_mtx = PTHREAD_MUTEX_INITIALIZER;	//gestisce la mutua esclusione su online_users, last_pos e n_online
online_users *onUsers;                                  //lista degli utenti online
pthread_mutex_t select_mtx = PTHREAD_MUTEX_INITIALIZER;
fd_set fdset;
int last_pos = 0, n_online = 0;
int new_max;

inline void remove_online(char *username) {
    bool_t flag = FALSE;
    int i = 0;
    
    pthread_mutex_lock(&online_mtx);
    while(!flag){
        if(strcmp(onUsers[i].username, username)==0) {
            flag = TRUE;
            n_online--;
            last_pos = i;
            strcpy(onUsers[i].username,"");
            onUsers[i].fd = -1;
        }
        i = (i+1)% MaxConnections;
    }
    pthread_mutex_unlock(&online_mtx);
}

inline void insert_online(char *username, int fd) {
	
    pthread_mutex_lock(&online_mtx);
    strcpy(onUsers[last_pos].username, username);
    onUsers[last_pos].fd = fd;
	n_online++;
	
	int i=(last_pos+1) % MaxConnections;
    int first = i;
	while(onUsers[i].fd != -1) {
		i =(i+1) % MaxConnections;
        if(i == first) break;
	}
	last_pos = i;
    pthread_mutex_unlock(&online_mtx);
}

inline int is_online(char *username) {
    int fd;
    pthread_mutex_lock(&online_mtx);
    for(int i=0;i<MaxConnections;i++) {
        if(onUsers[i].fd!=-1 && (strcmp(username, onUsers[i].username)==0)) {
            fd = onUsers[i].fd;
            pthread_mutex_unlock(&online_mtx);
            return fd;
        }
    }
    pthread_mutex_unlock(&online_mtx);
    return FALSE;
}

inline char* reverse_search(int fd) {
    char *buf = calloc(sizeof(char),MAX_NAME_LENGTH+1);
    pthread_mutex_lock(&online_mtx);
    for(int i=0;i<MaxConnections;i++) {
        if(onUsers[i].fd == fd) {
            strcpy(buf, onUsers[i].username);
            pthread_mutex_unlock(&online_mtx);
            return buf;
        }
    }
    pthread_mutex_unlock(&online_mtx);
    return NULL;
}

inline void update_fd(char *username, int newfd) {
    bool_t flag = FALSE;
    int i = 0;
    
    pthread_mutex_lock(&online_mtx);
    while(!flag){
        if(strcmp(onUsers[i].username, username)==0) {
            flag = TRUE;
            onUsers[i].fd = newfd;
        }
        i++;
    }
    pthread_mutex_unlock(&online_mtx);
}

inline void printOnline(){
    pthread_mutex_lock(&online_mtx);
    int i=0;
    while(i<MaxConnections) {
        printf("%d: %s - %d --> ", i, onUsers[i].username, onUsers[i].fd);
        i++;
    }
    pthread_mutex_unlock(&online_mtx);
}

inline void freeOnline() {
    int i=0;
    while(i < MaxConnections) {
        if(onUsers[i].fd != -1) {
            onUsers[i].fd = -1;
            free(onUsers[i].username);
        }
        i++;
    }
    free(onUsers);
}


int main(int argc, char *argv[]) {

    //semplice controllo degli argomenti
    if(argc < 3 || strcmp(argv[1], "-f") != 0) {
        usage(argv[0]);
        return -1;
    }

    int res;
    //parsing del file di configurazione
    res = parse_config(argv[2], DirName, UnixPath, StatFileName, &ThreadsInPool, &MaxConnections, &MaxMsgSize, &MaxFileSize, &MaxHistMsgs);

    if(res == -1) {
        perror("parser");
        return -1;
    }
    
    #ifdef DEBUG
        printf("%s %s %s %d %d %d %d %d\n", UnixPath, DirName, StatFileName, MaxConnections, ThreadsInPool, MaxMsgSize, MaxFileSize, MaxHistMsgs);
    #endif
    
    //creazione ed inizializzazione della tabella hash
    init_server();

    if(unlink(UnixPath) < 0) {      //cancella la path relativa a lui -> posso creare una nuova socket
        if(errno == ENOENT) 
            errno = 0;
        else {
            perror("unlink UnixPath");
            exit(EXIT_FAILURE);
        }
    }
    
    if(unlink(StatFileName) < 0) {
        if(errno == ENOENT) 
            errno = 0;
        else {
            perror("unlink StatFile");
            exit(EXIT_FAILURE);
        }
    }
    
    //registro segnali [...]
    struct sigaction s;
    memset(&s, 0, sizeof(s));
    
    //[...] per terminare il server
    s.sa_handler = handler;

    if(sigaction(SIGTERM,&s,NULL) == -1) {
        perror("sigaction");
        return -1;
    }
    if(sigaction(SIGQUIT,&s,NULL) == -1) {
        perror("sigaction");
        return -1;
    }
    if(sigaction(SIGINT,&s,NULL) == -1) {
        perror("sigaction");
        return -1;
    }
    
    //[...] per il segnale SIGUSR1
    s.sa_handler = handlerUSR1;

    if(sigaction(SIGUSR1,&s,NULL) == -1) {
        perror("sigaction");
        return -1;
    }

    //[...] ignorando SIGPIPE (per evitare terminazione in caso di scrittura su un socket chiuso)
    s.sa_handler = SIG_IGN;

    if(sigaction(SIGPIPE,&s,NULL) == -1) {
        perror("sigaction");
        return -1;
    }

    struct sockaddr_un sa;
    strncpy(sa.sun_path, UnixPath, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    //creo la socket
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    //assegno indirizzo locale al socket e metto in attesa di richieste
    if(bind(server_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("bind");
        return -1;
    }

    if((listen(server_fd, SOMAXCONN) < 0)) {
        perror("listen");
        return -1;
    }

    int maxfd = server_fd;
    new_max = server_fd;

    //set dei fd da ascoltare
    fd_set rdset;
    FD_ZERO(&fdset);
    FD_ZERO(&rdset);FD_SET(server_fd, &fdset);
    FD_SET(server_fd, &fdset);
    FD_SET(server_fd, &rdset);

    //creo il threadpool
    pthread_t *pool = malloc(sizeof(pthread_t) * ThreadsInPool);
    for(int i = 0; i < ThreadsInPool; i++) {
        int r = pthread_create(&pool[i], NULL, client_manager, NULL);
        if(r != 0) {
            #ifdef DEBUG
                printf("Errore thread_create %d", i);
            #endif
            perror("pthread_create");
        }
    }

    //timeout della select, settato poco dopo
    struct timeval timeout;
    bool_t flag = TRUE;
    int client_fd = -1;

    while(flag) {
    	pthread_mutex_lock(&select_mtx);
        rdset = fdset;
        maxfd = new_max;
        pthread_mutex_unlock(&select_mtx);

        timeout.tv_sec = 0;
		timeout.tv_usec = 1000;

        do {
            //gestione errore
            if(errno == 0 || errno == EINTR) {  //c'è un problema -> guarda i segnali
                if(sigquit) {
                    //graceful shutdown
                    flag = FALSE;
                }

                if(sigUSR1) {
                    FILE *stat = fopen(StatFileName, "a+");
                    if(stat != NULL) {
                        pthread_mutex_lock(&stats_mtx);
                        printStats(stat);
                        pthread_mutex_unlock(&stats_mtx);
                    }
                    sigUSR1 = 0;
                }
            }
            else {
                //graceful shutdown
                flag = FALSE;
            }
        }
        while(select(maxfd+1, &rdset, NULL, NULL, &timeout) < 0);
        
        if(!flag) {
            break;
        }

        for(int i = 3; i < maxfd+1; i++) {
            if(FD_ISSET(i, &rdset)) {
                if(i == server_fd) {
                    client_fd = accept(server_fd, NULL, NULL);

                    #ifdef DEBUG
                        printf("Accept [%d]\n", client_fd);
                    #endif
                    
                    pthread_mutex_lock(&stats_mtx);
                    if(chattyStats.nonline >= MaxConnections){
                        pthread_mutex_unlock(&stats_mtx);
                        close(client_fd);
                        #ifdef DEBUG
                            printf("Refused_MaxConnections: [%d]\n", client_fd);
                        #endif
                        continue;
                    }
                    pthread_mutex_unlock(&stats_mtx);
                    
                    pthread_mutex_lock(&select_mtx);
                    FD_SET(client_fd, &fdset);
                    if(client_fd > new_max)  
                        new_max = client_fd;
                    pthread_mutex_unlock(&select_mtx);
                }
                else {      //un client ha scritto qualcosa o è morto
                    
                    #ifdef DEBUG
                        printf("Select: [%d]\n", i);
                    #endif

                    pthread_mutex_lock(&select_mtx);
                    FD_CLR(i, &fdset);  //toglie dalla lista di ascolto
                    if(i == new_max)
                        new_max = updatemax(fdset, new_max+1);
					pthread_mutex_unlock(&select_mtx);

                    pthread_mutex_lock(&fd_mtx);
                    push(i);        //pusha nella coda
                    pthread_cond_signal(&fd_cond);
                    pthread_mutex_unlock(&fd_mtx);
                }
            }
        }
    }

    freeOnline();
    freeDB();

    pthread_mutex_lock(&fd_mtx);
    pthread_cond_broadcast(&fd_cond);
    pthread_mutex_unlock(&fd_mtx);

    for(int i = 0; i < ThreadsInPool; i++)
        pthread_join(pool[i],NULL);

    free(pool);

    return 0;
}

