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
 */
/**
 * @file config.h
 * @brief File contenente alcune define con valori massimi utilizzabili
 */

#if !defined(CONFIG_H_)
#define CONFIG_H_

#define MAX_NAME_LENGTH 32

#include <limits.h>
/* aggiungere altre define qui */
#include <linux/limits.h>

typedef enum { TRUE = 1, FALSE = 0 } bool_t;
int MaxHistMsgs;
int MaxConnections;
char UnixPath[PATH_MAX];
char DirName[PATH_MAX];
char StatFileName[PATH_MAX];
int ThreadsInPool;
int MaxMsgSize;
int MaxFileSize;


// to avoid warnings like "ISO C forbids an empty translation unit"
typedef struct {
    char username[MAX_NAME_LENGTH+1];
    int fd;
} online_users;

char *get_value(char *s);
int parse_config(char* file, char *dir, char *path, char *statFile, int *poolSz, int *maxConn, int *msgSz, int *fileSz, int *histSz);
void init_server();

#endif /* CONFIG_H_ */
