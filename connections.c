/*
 * Si dichiara che il contenuto di questo file è in ogni sua parte opera
 * originale dell'autore
 * 
 * Giulio Purgatorio - 516292
*/

#include "connections.h"

int openConnection(char* path, unsigned int ntimes, unsigned int secs) {
    if(path == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    int fd; 
    if ((fd=socket(AF_UNIX, SOCK_STREAM, 0)) == -1) 
        return -1;

    struct sockaddr_un sa;
    strncpy(sa.sun_path, path, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    if (ntimes > MAX_RETRIES) 
        ntimes = MAX_RETRIES;
    if (secs > MAX_SLEEPING) 
        secs = MAX_SLEEPING;
    
    while (ntimes-- && connect(fd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
        if (errno == ENOENT)    //No such file or directory
            sleep(secs);
        else
            return -1;
    }

    return fd;
}

/**
 * @function readHeader
 * @brief Legge l'header del messaggio
 *
 * @param fd     descrittore della connessione
 * @param hdr    puntatore all'header del messaggio da ricevere
 *
 * @return <=0 se c'e' stato un errore 
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa) 
 */
int readHeader(long connfd, message_hdr_t *hdr) {
    if(hdr == NULL) {
        errno = EINVAL;
        return -1; 
    }

    int letti = 0;
    letti = readn(connfd, &(hdr->op), sizeof(hdr->op));

    if(letti == 0) return 0;
    if(letti < 0) return -1; 

    letti = readn(connfd, hdr->sender, sizeof(hdr->sender));

    if(letti == 0) return 0;
    if(letti < 0) return -1;

    return 1;
}

/**
 * @function readData
 * @brief Legge il body del messaggio
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al body del messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa) 
 */
int readData(long fd, message_data_t *data) {
    if(data == NULL) {
        errno = EINVAL; 
        return -1; 
    }
    int letti = 0;

    letti = readn(fd, data->hdr.receiver, sizeof(data->hdr.receiver));
    if(letti < 0) return -1; 
    if(letti == 0) return 0;


    letti = readn(fd, &(data->hdr.len), sizeof(data->hdr.len));
    if(letti < 0) return -1; 
    if(letti == 0) return 0;
    
    data->buf = NULL;   //test4
    if(data->hdr.len > 0) {
        data -> buf = malloc(data->hdr.len * sizeof(char));

        letti = readn(fd, data->buf, data->hdr.len);
        if(letti < 0) return -1;
        if(letti == 0) return 0;
    }
    
    return 1;
}

/**
 * @function readMsg
 * @brief Legge l'intero messaggio
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa) 
 */
int readMsg(long fd, message_t *msg) {
    if(msg == NULL) {
        errno = EINVAL; 
        return -1; 
    }
    int esito = 0;

    esito = readHeader(fd, &(msg->hdr));
    if(esito < 0) return -1; 
    if(esito == 0) return 0;

    esito = readData(fd, &(msg->data));
    if(esito < 0) return -1; 
    if(esito == 0) return 0;

    return 1;
}

int sendHeader(long fd, message_hdr_t *msg) {
    if(msg == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    int scritti = 0;

    scritti = writen(fd, &(msg->op), sizeof(msg->op));
    if(scritti < 0) return -1; 
    if(scritti == 0) return 0;

    scritti = writen(fd, msg->sender, sizeof(msg->sender));
    if(scritti < 0) return -1; 
    if(scritti == 0) return 0;

    return 1;
}

int sendData(long fd, message_data_t *msg) {
    if(msg == NULL) {
        errno = EINVAL;
        return -1;
    }

    int scritti = writen(fd, msg->hdr.receiver, sizeof(msg->hdr.receiver));
    if(scritti < 0) return -1; 
    if(scritti == 0) return 0;

    scritti = writen(fd, &(msg->hdr.len), sizeof(msg->hdr.len));
    if(scritti < 0) return -1; 
    if(scritti == 0) return 0;

    if(msg->hdr.len > 0)
        return writen(fd, msg->buf, msg->hdr.len);
    else 
        return 1;

}

/**
 * @function sendRequest
 * @brief Invia un messaggio di richiesta al server 
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 */
int sendRequest(long fd, message_t *msg) {
    if(msg == NULL) {
        errno = EINVAL; 
        return -1;
    }

    int scritti = 0;

    scritti = sendHeader(fd, &msg->hdr);

    if(scritti <=0) return scritti;

    scritti = sendData(fd, &msg->data);
    
    if(scritti <=0) return scritti;

    return 1;
}

