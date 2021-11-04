#ifndef _PACKET_QUEUE_H
#define _PACKET_QUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>

typedef struct {
    uint8_t *packet;
    size_t packet_size;
} packet_t;


typedef struct packet_node {
    packet_t *packet;
    struct packet_node *next;
} packet_node_t;

typedef struct {
    packet_node_t *start;
    packet_node_t *end;
    int size;
} packet_queue_t;

void inititializePacketQueue(packet_queue_t *queue);
void enqueuePacket(packet_queue_t *queue, packet_t *packet);
packet_t *dequeuePacket(packet_queue_t *queue);

int getPacketQueueSize(packet_queue_t *queue);

#endif