
#include "message_queue.h"

void inititializeMessageQueue(message_queue_t *queue) {
    queue->start = NULL;
    queue->end = NULL;
    queue->size = 0;
}

void enqueueMessage(message_queue_t *queue, message_t *message) {

    message_node_t *new_entry = malloc(sizeof(message_node_t));

    if (queue->size == 0) {
        queue->start = queue->end = new_entry;
    }

    else {
        queue->end->next = new_entry;
        queue->end = new_entry;
    }
    queue->end->message = message;
    queue->size++;
}

message_t *dequeueMessage(message_queue_t *queue) {

    if (queue->size == 0) {
        return NULL;
    }

    message_node_t *next_start = queue->start->next;
    message_t *removed_message = queue->start->message;

    free(queue->start);
    queue->start = next_start;
    queue->size--;

    return removed_message;
}

int getMessageQueueSize(message_queue_t *queue) {
    return queue->size;
}