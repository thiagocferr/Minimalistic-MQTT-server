
#include "packet_queue.h"

void inititializePacketQueue(packet_queue_t *queue) {
    queue->start = NULL;
    queue->end = NULL;
    queue->size = 0;
}

void enqueuePacket(packet_queue_t *queue, packet_t *packet) {

    packet_node_t *new_entry = malloc(sizeof(packet_node_t));

    if (queue->size == 0) {
        queue->start = queue->end = new_entry;
    }

    else {
        queue->end->next = new_entry;
        queue->end = new_entry;
    }
    queue->end->packet = packet;
    queue->size++;
}

packet_t *dequeuePacket(packet_queue_t *queue) {

    if (queue->size == 0) {
        return NULL;
    }

    packet_node_t *next_start = queue->start->next;
    packet_t *removed_message = queue->start->packet;

    free(queue->start);
    queue->start = next_start;
    queue->size--;

    return removed_message;
}

int getPacketQueueSize(packet_queue_t *queue) {
    return queue->size;
}