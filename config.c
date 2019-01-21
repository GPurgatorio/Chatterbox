/*
 * Si dichiara che il contenuto di questo file è in ogni sua parte opera
 * originale dell'autore
 * 
 * Giulio Purgatorio - 516292
*/

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#include "config.h"
#include "hash.h"

#define SIZE 367

extern users_DB chatty_users;
extern online_users *onUsers;
extern int MaxConnections;

void init_server(){

    chatty_users.size = SIZE;
	chatty_users.users = malloc(sizeof(user*)*SIZE);
	for(int i=0; i < SIZE; i++)
		chatty_users.users[i] = NULL;

	chatty_users.locks = malloc(sizeof(pthread_mutex_t)*SIZE);
	onUsers = malloc(sizeof(online_users)*MaxConnections);
	
	int i;
	for(i=0; i<SIZE; i++){
		if(i<MaxConnections) 
			onUsers[i].fd = -1;
		pthread_mutex_init(&(chatty_users.locks[i]), NULL);
	}
}

char *get_value(char *s) {
	
	int i = 0;
	while (s[i] != '=')
		i++;
	
	s = (s+i+2);
	i = strlen(s);
	
	while((s[i]=='\0') || (s[i]=='\n') || (s[i]==' ') || (s[i]=='	'))
		i--;

	s[i+1] = '\0';
	
	char *str = NULL;
	int len = strlen(s)+1;
	str = (char *)malloc(sizeof(char) * len);
	
	str[0] = '\0';
	str[len-1] = '\0';
	strncpy(str, s, len-1);
	
	return str;
}

int parse_config(char* file, char *dir, char *path, char *statFile, int *poolSz, int *maxConn, int *msgSz, int *fileSz, int *histSz ) {

	FILE *conf = fopen(file, "r");
	
	if ((conf == NULL) || (errno == EINVAL))
		return -1;

	int res = fseek(conf, 0, SEEK_END);
	
	if (res != 0)
		return -1;

	long l = 0;
	l = ftell(conf);

	if (errno != 0)
		return -1;

	rewind(conf);

	char *str = NULL, *aux;
	str = (char *)malloc(sizeof(char) * l);

	bool_t flag[8];
	for(int i=0;i<8;i++)
		flag[i] = FALSE;

	while (fgets(str, l, conf)) {
		
		if (str == NULL)
			return -1;

		if (str[0] != '#') {
			
			if (strncmp(str, "DirName", 7) == 0) {
				aux = get_value((str + 7));
				strcpy(dir, aux);
				flag[0] = TRUE;	
				free(aux);
				aux = NULL;
			}

			else if (strncmp(str, "UnixPath", 8) == 0) {
				aux = get_value((str + 8));
				strcpy(path, aux);
				flag[1] = TRUE;
				free(aux);
				aux = NULL;
			}
			
			else if (strncmp(str, "StatFileName", 12) == 0) {
				aux = get_value((str + 12));
				strcpy(statFile, aux);
				flag[2] = TRUE;
				free(aux);
				aux = NULL;
			}
			
			else if (strncmp(str, "ThreadsInPool", 13) == 0) {
				aux = get_value((str + 13));
				*poolSz = (int)strtol(aux, NULL, 10);
				flag[3] = TRUE;
				free(aux);
				aux = NULL;
			}
			
			else if (strncmp(str, "MaxConnections", 14) == 0) {
				aux = get_value((str + 14));
				*maxConn = (int)strtol(aux, NULL, 10);
				flag[4] = TRUE;
				free(aux);
				aux = NULL;
			}
			
			else if (strncmp(str, "MaxMsgSize", 10) == 0) {
				aux = get_value((str + 10));
				*msgSz = (int)strtol(aux, NULL, 10);
				flag[5] = TRUE;
				free(aux);
				aux = NULL;
			}
			
			else if (strncmp(str, "MaxFileSize", 11) == 0) {
				aux = get_value((str + 11));
				*fileSz = (int)strtol(aux, NULL, 10);
				flag[6] = TRUE;
				free(aux);
				aux = NULL;
			}
			
			else if (strncmp(str, "MaxHistMsgs", 8) == 0) {
				aux = get_value((str + 8));
				*histSz = (int)strtol(aux, NULL, 10);
				flag[7] = TRUE;
				free(aux);
				aux = NULL;
			}
		}

		str[0] = '\0';
	}

	fclose(conf);
	
	if(str != NULL) {
		free(str);
		str = NULL;
	}
	
	if(aux != NULL) {
		free(aux);
		aux = NULL;
	}
	
	for(int i = 0;i<8;i++) {
		if(flag[i] == FALSE)
			return -1;
	}

	return 1;
}
