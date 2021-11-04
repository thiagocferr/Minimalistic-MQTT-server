#ifndef _MESSAGE_QUEUE_H
#define _MESSAGE_QUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "mqtt_conf.h"
#include "mqtt.h"

// Message from client threads to manager thread or vice-versa
typedef struct {
    message_type_t message_type;
    void *info;

    // Special field used when we need to send a reference to some field to the manager and
    // back to the client with no changes
    void *pigback_struct;
} message_t;


typedef struct message_node {
    message_t *message;
    struct message_node *next;
} message_node_t;

typedef struct {
    message_node_t *start;
    message_node_t *end;
    int size;
} message_queue_t;

void inititializeMessageQueue(message_queue_t *queue);
void enqueueMessage(message_queue_t *queue, message_t *message);
message_t *dequeueMessage(message_queue_t *queue);

int getMessageQueueSize(message_queue_t *queue);

#endif
