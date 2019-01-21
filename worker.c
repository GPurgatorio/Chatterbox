/*
 * Si dichiara che il contenuto di questo file è in ogni sua parte opera
 * originale dell'autore
 * 
 * Giulio Purgatorio - 516292
*/

#include <fcntl.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "connections.h"
#include "hash.h"
#include "queue.h"
#include "stats.h"
#include "worker.h"

//#define DEBUG

void *client_manager(void* par) {
	
	while(TRUE) {

		pthread_mutex_lock(&fd_mtx);
		while(fd_head == NULL) {
			if(sigquit) {
				pthread_mutex_unlock(&fd_mtx);
				return NULL;
			}
			else 
				pthread_cond_wait(&fd_cond, &fd_mtx);
		}

		int fd_client = pop();
		pthread_mutex_unlock(&fd_mtx);

		message_t msg;
		msg.data.buf = NULL;	//test4
		
		op_t esito;
		bool_t closed = FALSE;

		#ifdef DEBUG
			printf("Worker: [%d]\n", fd_client);
		#endif

		int result = readMsg(fd_client, &msg);
		
		#ifdef DEBUG
			printf("Result [%d]: %d\n", fd_client, result);
		#endif

		if(result == 0) {
			esito = OP_END;
			closed = TRUE;
		}

		else if(result < 0) {
			esito = OP_FAIL;
			closed = TRUE;
		}

		else {		
			#ifdef DEBUG
				printf("Operazione [%d]: %d , Sender: %s\n", fd_client, msg.hdr.op, msg.hdr.sender);
			#endif	

			switch(msg.hdr.op) {

				case REGISTER_OP:
				case USRLIST_OP:
				case CONNECT_OP: {

					message_t reply;
					
					char *username = malloc(sizeof(char)*MAX_NAME_LENGTH);
					memset(username, 0, sizeof(char)*(MAX_NAME_LENGTH));	
					strcpy(username, msg.hdr.sender);

					op_t r;
					
					if(msg.hdr.op == REGISTER_OP) {

						r = insert_user(username, fd_client);
						
						if(r==OP_OK)
							insert_online(username, fd_client);
						else {

						}

						#ifdef DEBUG
							printDB();
						#endif
					}

					else if(msg.hdr.op == CONNECT_OP)  {
						
						//se un utente è già connesso e tenta di riconnettersi -> OP_NICK_ALREADY
						if(is_online(username) != FALSE) {
							esito = OP_NICK_ALREADY;
							update_stats(ERR,1);

							closed = TRUE;
							free(username);
							break;
						}
						if(lookup_user(username)) {
							insert_online(username, fd_client);
							r = update_state(username, TRUE);	
						}

						else {
							r = OP_NICK_UNKNOWN;
						}
					}
					
					else {					//3 casi, 2 gestiti -> USRLIST_OP 
						if(!lookup_user(username))
							r = OP_NICK_UNKNOWN;

						else if(r!=OP_NICK_UNKNOWN && !is_online(username))
							r = OP_FAIL;
					}

					if(r == OP_NICK_UNKNOWN || r == OP_NICK_ALREADY || r == OP_FAIL){

						setHeader(&msg.hdr, r, "");	

						lock(username);
						sendHeader(fd_client, &msg.hdr);
						unlock(username);

						update_stats(ERR,1);
						
						esito = r;
						closed = TRUE;
						free(username);
						break;
					}
					else {		//Ho trovato l'utente, preparo messaggio di risposta
						
						pthread_mutex_lock(&online_mtx);
						char *lista = users_list();
						pthread_mutex_unlock(&online_mtx);
						
						memset(reply.data.hdr.receiver, 0, sizeof(char)*(MAX_NAME_LENGTH+1));						
						memset(reply.hdr.sender, 0, sizeof(char)*(MAX_NAME_LENGTH+1));

						setHeader(&reply.hdr, OP_OK, "");
						setData(&reply.data, username, lista, n_online*(MAX_NAME_LENGTH+1));
						
						lock(username);
						sendRequest(fd_client, &reply);
						unlock(username);

						free(lista);
					}

					esito = r;

					if(esito == OP_OK) {
						if(msg.hdr.op == CONNECT_OP) 
							update_stats(ON, 1);
						else if(msg.hdr.op == REGISTER_OP){
							update_stats(REG, 1);
							update_stats(ON, 1);
						}
					}
					free(username);
					break;
				}

				case POSTFILE_OP:
				case POSTTXTALL_OP:
				case POSTTXT_OP:{
				
					message_t reply;
					char *username = malloc(sizeof(char)*MAX_NAME_LENGTH);
					memset(username, 0, sizeof(char)*(MAX_NAME_LENGTH));
					strcpy(username, msg.hdr.sender);
					
					op_t res;
					bool_t check = lookup_user(username);

					if(check) {
						if(is_online(username))
							res = OP_OK;
						else
							res = OP_FAIL;
					}

					else 
						res = OP_NICK_UNKNOWN;

					//mittente non valido
					if(res == OP_FAIL || res == OP_NICK_UNKNOWN) {
						setHeader(&reply.hdr, res, "");
						setData(&reply.data, "", "", 0);

						lock(username);
						sendHeader(fd_client, &reply.hdr);
						unlock(username);
						
						esito = res;		
						
						update_stats(ERR, 1);
						closed = TRUE;
						free(username);
						break;
					}

					if(msg.hdr.op == POSTTXT_OP || msg.hdr.op == POSTTXTALL_OP) {
						
						if(msg.data.hdr.len > MaxMsgSize) {
							
							setHeader(&reply.hdr, OP_MSG_TOOLONG, "");

							lock(username);
							sendHeader(fd_client, &reply.hdr);
							unlock(username);

							update_stats(ERR, 1);

							esito = OP_MSG_TOOLONG;
							closed = TRUE;
							free(username);
							break;
						}
					}
					
					//POSTTXT_OP: cerco il receiver
					if(msg.hdr.op == POSTTXT_OP || msg.hdr.op == POSTFILE_OP) {
						
						char *receiver = msg.data.hdr.receiver;
						int fd_rcv = FALSE;
						op_t res_rcv = OP_OK;
						
						//receiver !registrato
						if(!lookup_user(receiver))
							res_rcv = OP_NICK_UNKNOWN;

						if(res_rcv == OP_NICK_UNKNOWN) {
							
							setHeader(&reply.hdr, res_rcv, "");
							setData(&reply.data, "", "", 0);
							
							lock(username);
							sendHeader(fd_client, &reply.hdr);
							unlock(username);
							
							esito = res_rcv;		
							
							update_stats(ERR,1);
							closed = TRUE;
							free(username);
							break;
						}

						message_data_t file;
						char fileName[PATH_MAX];
						
						if(msg.hdr.op == POSTFILE_OP) {

							//concateno la directory
							strcpy(fileName, DirName);
							strcat(fileName,"/");
							//leggo il nome del file
							strcat(fileName, msg.data.buf);
							
							//leggo il file
							readData(fd_client, &file);
							
							if(file.hdr.len > MaxFileSize*1024) {
								
								setHeader(&reply.hdr, OP_MSG_TOOLONG, "");
								
								lock(username);
								sendHeader(fd_client, &reply.hdr);
								unlock(username);

								update_stats(ERR, 1);
								esito = OP_MSG_TOOLONG;
								closed = TRUE;
								free(username);
								free(file.buf);
								break;
							}
					
							//salvo file
							FILE *pFile = fopen(fileName, "wb");
							if(pFile != NULL) {
								fwrite(file.buf, file.hdr.len, 1, pFile);
								fclose(pFile);
							}
							else {
								perror("fopen");
							}
							free(file.buf);
						}

						//se arrivo qua -> receiver registrato, avviso il mittente
						setHeader(&reply.hdr, OP_OK, "");
						setData(&reply.data, "", "", 0);
						
						lock(username);
						sendHeader(fd_client, &reply.hdr);
						unlock(username);
						
						//creo messaggio per destinatario e lo inoltro
						message_t msg_sent;
						
						if(msg.hdr.op == POSTTXT_OP) {
							setHeader(&msg_sent.hdr, TXT_MESSAGE, username);
							setData(&msg_sent.data, receiver, msg.data.buf, msg.data.hdr.len);
						}

						else {		//POSTFILE
							setHeader(&msg_sent.hdr, FILE_MESSAGE, username);
							setData(&msg_sent.data, msg.data.hdr.receiver, fileName, msg.data.hdr.len);
						}
						
						fd_rcv = is_online(receiver);

						if(fd_rcv != FALSE){
							
							lock(receiver);
							sendRequest(fd_rcv, &msg_sent);
							unlock(receiver);
							
							esito = res_rcv;		
							
							if(msg.hdr.op == POSTTXT_OP)
								update_stats(DEL,1);
							else if(msg.hdr.op == POSTFILE_OP)
								update_stats(FILEDEL,1);
							free(username);
							break;
						}

						else {		//offline                  
							res_rcv = store_message(receiver, msg_sent);  
							esito = res_rcv;		
							if(msg.hdr.op == POSTTXT_OP)	
								update_stats(NDEL,1);
							if(msg.hdr.op == POSTFILE_OP) 
								update_stats(NFILEDEL,1);
						}
					}
					else {			//POSTTXTALL_OP
						setHeader(&reply.hdr, OP_OK, "");
						setData(&reply.data, "", "", 0);

						lock(username);
						sendHeader(fd_client, &reply.hdr);
						unlock(username);

						message_t new_msg;
						int cnt = 0;	//tengo un contatore del numero di messaggi per statistiche

						setHeader(&new_msg.hdr, TXT_MESSAGE, msg.hdr.sender);
						
						pthread_mutex_lock(&online_mtx);
						for(int i=0;i<MaxConnections;i++){
							if(onUsers[i].fd != -1){
								
								lock(onUsers[i].username);
								setData(&new_msg.data, onUsers[i].username, msg.data.buf, msg.data.hdr.len);
								sendRequest(onUsers[i].fd,&new_msg);
								unlock(onUsers[i].username);
								
								cnt++;
							}
							
						}
						pthread_mutex_unlock(&online_mtx);

						update_stats(DEL, cnt);		//messaggi inviati (online)
						
						cnt = sendAll(new_msg);
						update_stats(NDEL, cnt);	//messaggi inviati (offline)
						esito = OP_OK;			
					}
					free(username);
					break;
				}

				case GETPREVMSGS_OP: {

					message_t reply;
					char *username = malloc(sizeof(char)*MAX_NAME_LENGTH);
					memset(username, 0, sizeof(char)*(MAX_NAME_LENGTH));
					strcpy(username, msg.hdr.sender);
					
					op_t res;
					bool_t check = lookup_user(username);

					if(check) {
						if(is_online(username))
							res = OP_OK;
						else
							res = OP_FAIL;
					}

					else 
						res = OP_NICK_UNKNOWN;

					//mittente non valido
					if(res == OP_FAIL || res == OP_NICK_UNKNOWN) {
						setHeader(&reply.hdr, res, "");
						setData(&reply.data, "", "", 0);
						
						lock(username);
						sendHeader(fd_client, &reply.hdr);
						unlock(username);
						
						esito = res;		
						
						update_stats(ERR, 1);
						closed = TRUE;
						free(username);
						break;
					}
					
					message_t *cat = NULL;
					size_t count = 0;
					int oldest = 0;

					cat = get_messages(username, &oldest);
					setHeader(&reply.hdr, OP_OK, "");
					
					for(int i = 0; i < MaxHistMsgs; i++) {
						if(cat[i].data.hdr.len != 0)
							count++;
					}
					
					setData(&reply.data, username, (char*)&count, sizeof(size_t));		
					#ifdef DEBUG
						printf("Count: %ld\n", *count);
					#endif

					lock(username);
					sendRequest(fd_client, &reply);
					unlock(username);

					int counter = (int) count;
					
					while(counter != 0) {								
						if(cat[oldest].data.hdr.len != 0) {
							
							setHeader(&reply.hdr, cat[oldest].hdr.op, cat[oldest].hdr.sender);
							setData(&reply.data, username, cat[oldest].data.buf, cat[oldest].data.hdr.len);

							lock(username);
							sendRequest(fd_client, &reply);
							unlock(username);

							resetMessage(username, oldest);
						}
						
						oldest = (oldest + 1) % MaxHistMsgs;
						counter--;
					}
					
					resetIndexes(username);
					update_stats(NDEL, -counter);
					update_stats(DEL, counter);

					esito = OP_OK;
					free(username);
					break;
				}

				case UNREGISTER_OP: {
					
					message_t reply;
					op_t res = OP_OK;

					if(!lookup_user(msg.hdr.sender))
						res = OP_NICK_UNKNOWN;

					if(res == OP_NICK_UNKNOWN) {
						
						setHeader(&reply.hdr, res, "");
						setData(&reply.data, "", "", 0);
						
						lock(msg.hdr.sender);
						sendHeader(fd_client, &reply.hdr);
						unlock(msg.hdr.sender);
						
						esito = res;		
						
						update_stats(ERR,1);
						closed = TRUE;
						break;
					}

					setHeader(&reply.hdr, OP_OK, "");
					lock(msg.hdr.sender);
					sendHeader(fd_client, &reply.hdr);
					unlock(msg.hdr.sender);
					
					res = delete_user(msg.hdr.sender);
					update_stats(REG, -1);

					closed = TRUE;

					esito = OP_OK;
					break;
				}

				case GETFILE_OP: {
					
					message_t reply;
					char *username = malloc(sizeof(char)*MAX_NAME_LENGTH);
					memset(username, 0, sizeof(char)*(MAX_NAME_LENGTH));
					strcpy(username, msg.hdr.sender);
					
					op_t res;
					bool_t check = lookup_user(username);
					
					if(check) {
						if(is_online(username))
							res = OP_OK;
						else
							res = OP_FAIL;
					}
					
					else 
						res = OP_NICK_UNKNOWN;

					//mittente non valido
					if(res == OP_FAIL || res == OP_NICK_UNKNOWN) {
						
						setHeader(&reply.hdr, res, "");
						setData(&reply.data, "", "", 0);
						
						lock(username);
						sendHeader(fd_client, &reply.hdr);
						unlock(username);
						
						esito = res;		
						
						update_stats(ERR, 1);
						closed = TRUE;
						free(username);
						break;
					}

					struct stat st;
					char fileName[msg.data.hdr.len];
					
					strcpy(fileName, msg.data.buf);
					
					if (stat(fileName, &st)==-1) {
						
						setHeader(&reply.hdr, OP_NO_SUCH_FILE, "");
						setData(&reply.data, "", "", 0);
						
						lock(username);
						sendHeader(fd_client, &reply.hdr);
						unlock(username);
						
						esito = res;		
						
						update_stats(ERR, 1);
						closed = TRUE;
						free(username);
						break;
					}

					char *fileBuf;
					
					int fdFile = open(fileName, O_RDONLY);

					fileBuf = mmap(NULL, msg.data.hdr.len, PROT_READ, MAP_PRIVATE, fdFile, 0);
					close(fdFile);

					setHeader(&reply.hdr, OP_OK, "");
					setData(&reply.data, msg.data.hdr.receiver, fileBuf, st.st_size);
					
					lock(username);
					sendRequest(fd_client, &reply);
					unlock(username);

					esito = OP_OK;
					update_stats(FILEDEL,1);
					update_stats(NFILEDEL, -1);
					free(username);
					break;
				}
				
				default: {
					closed = TRUE;
					break;
				}
			}
		}
		
		if(msg.data.buf) {
			free(msg.data.buf);
		}

		if(!closed) {
			#ifdef DEBUG
				printf("!closed: [%d]\n", fd_client);
			#endif
			//inserire nella select
			pthread_mutex_lock(&select_mtx);
			FD_SET(fd_client, &fdset);
			if(new_max < fd_client)
				new_max = fd_client;
			pthread_mutex_unlock(&select_mtx);
		}

		else {				//disconnessione 
			
			char *tmp = reverse_search(fd_client);
			
			if(tmp != NULL) {
				if(is_online(tmp)) {
					update_stats(ON, -1);
					remove_online(tmp);
					update_state(tmp,FALSE);
				}
				lock(tmp);
				close(fd_client);
				unlock(tmp);
				
				free(tmp);
			}
			else {
				close(fd_client);
			}

			
			#ifdef DEBUG
				printf("Closed: %d\n", esito);
			#endif
		}
	}
    return NULL;
}

char *users_list() {
	char *s = calloc(sizeof(char)*n_online * (MAX_NAME_LENGTH+1), sizeof(char));
	char* aux = s;
	//char *tmp = malloc(sizeof(char)*(MAX_NAME_LENGTH+1));
	
	for(int i=0; i<n_online; i++) {
		if(onUsers[i].fd!=-1) {
			//sprintf(tmp,"%-*s",(MAX_NAME_LENGTH+1),online_users[i]);
			strcat(aux,onUsers[i].username);
			aux += MAX_NAME_LENGTH+1;
		}
	}
	return s;
}

int lock (char* username){
	int key = hash((unsigned char*)username) % chatty_users.size;
	return pthread_mutex_lock(&chatty_users.locks[key]);	
}

int unlock (char* username){
	int key = hash((unsigned char*)username) % chatty_users.size;
	return pthread_mutex_unlock(&chatty_users.locks[key]);
}

void update_stats(stat_code code, int incr){
	switch(code) {
		case REG: {
			pthread_mutex_lock(&stats_mtx);
			chattyStats.nusers+=incr;
			pthread_mutex_unlock(&stats_mtx);
			break;
		}
		case ON:{
			pthread_mutex_lock(&stats_mtx);
			chattyStats.nonline+=incr;
			#ifdef DEBUG
				printf("ON: %ld\n", chattyStats.nonline);
			#endif
			pthread_mutex_unlock(&stats_mtx);
			break;
		}
		case DEL:{
			pthread_mutex_lock(&stats_mtx);
			chattyStats.ndelivered+=incr;
			pthread_mutex_unlock(&stats_mtx);
			break;
		}
		case NDEL:{
			pthread_mutex_lock(&stats_mtx);
			chattyStats.nnotdelivered+=incr;
			pthread_mutex_unlock(&stats_mtx);
			break;
		}
		case FILEDEL:{
			pthread_mutex_lock(&stats_mtx);
			chattyStats.nfiledelivered+=incr;
			pthread_mutex_unlock(&stats_mtx);
			break;
		}
		case NFILEDEL:{
			pthread_mutex_lock(&stats_mtx);
			chattyStats.nfilenotdelivered+=incr;
			pthread_mutex_unlock(&stats_mtx);
			break;
		}
		case ERR:{
			pthread_mutex_lock(&stats_mtx);
			chattyStats.nerrors+=incr;
			pthread_mutex_unlock(&stats_mtx);
			break;
		}
	}
}
