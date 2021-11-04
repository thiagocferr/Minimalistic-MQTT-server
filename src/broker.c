
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>

#include <stdint.h>
#include <poll.h>
#include <pthread.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#include "misc.h"
#include "mqtt_connect.h"
#include "mqtt_connack.h"
#include "mqtt_subscribe.h"
#include "mqtt_suback.h"
#include "mqtt_publish.h"
#include "mqtt_ping.h"
#include "mqtt_disconnect.h"

#include "message_queue.h"
#include "packet_queue.h"
#include "mqtt_conf.h"

#include "mqtt.h"
#include "broker_manager.h"
#include "broker_client.h"

#define LISTENQ 1

int getListeningSocket(int port_num) {
    int listenfd;
    struct sockaddr_in servaddr;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket :(\n");
        exit(2);
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(port_num);
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("bind :(\n");
        exit(3);
    }

    if (listen(listenfd, LISTENQ) == -1) {
        perror("listen :(\n");
        exit(4);
    }

    return listenfd;
}

int main (int argc, char **argv) {
    /* Os sockets. Um que será o socket que vai escutar pelas conexões
     * e o outro que vai ser o socket específico de cada conexão */
    int listenfd;
    pthread_t manager_thread;
    srand(time(0));

    // This needs to be here, since, from the beggining, both thread types make use of these values
    manager_message_queue = malloc(sizeof(message_queue_t));
    manager_message_queue_lock = malloc(sizeof(pthread_mutex_t));
    message_size_cond = malloc(sizeof(pthread_cond_t));

    // Initialize static queues, locks and conditions
    inititializeMessageQueue(manager_message_queue);
    pthread_mutex_init(manager_message_queue_lock, NULL);
    pthread_cond_init(message_size_cond, NULL);


    if (argc != 2) {
        fprintf(stderr,"Uso: %s <Porta>\n",argv[0]);
        fprintf(stderr,"Vai rodar um servidor de echo na porta <Porta> TCP\n");
        exit(1);
    }

    listenfd = getListeningSocket(atoi(argv[1]));

    if (pthread_create(&manager_thread, NULL, manager_routine, NULL) != 0) {
        perror("Could not inicialize server: could not create manager thread");
        exit(5);
    }


    printf("[Servidor no ar. Aguardando conexões na porta %s]\n",argv[1]);
    printf("[Para finalizar, pressione CTRL+c ou rode um kill ou killall]\n");

    pthread_t client_thread;
    while (1) {

        int *connfd = malloc(sizeof(int));
        if ((*connfd = accept(listenfd, (struct sockaddr *) NULL, NULL)) == -1 ) {
            perror("accept :(\n");
            exit(6);
        }

        pthread_create(&client_thread, NULL, client_connection_routine, (void*)connfd);
    }

    pthread_join(manager_thread, NULL);

    pthread_mutex_destroy(manager_message_queue_lock);
    pthread_cond_destroy(message_size_cond);

    free(manager_message_queue);
    free(manager_message_queue_lock);
    free(message_size_cond);

    exit(0);
}

