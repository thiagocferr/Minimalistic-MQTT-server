#ifndef _MQTT_PUBLISH_H
#define _MQTT_PUBLISH_H

#include "mqtt.h"
#include "misc.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>

/**
 * NOTE: This package is treated a little differently: As being a packet type without a need for an
 *  acknolegdement message when sending messages of QoS 0 (which is all messages on this server) and
 *  seeing as the received packets of this kind will be almost entirely passed to other Clients
 *  mostly unaltered, the inner working of this package processing will be different from the CONNECT and
 *  SUBSCRIBE packets.
 */

// typedef struct {
//     unsigned int retain: 1;
//     unsigned int qos: 2;
//     unsigned int dup: 1;
// } publish_header_flags_t;

typedef struct {
    int is_message_expiry_interval_defined;
    uint32_t message_expiry_interval;
    // As there's no Wildcard support (hardocoded on CONNACK response), this packet should contain only
    // zero or one Subscription Identifier
    variable_byte_int_t subscription_identifier;

    // Other flags don't need to be present here

} publish_request_properties_t;

typedef struct {

    // This struct is only used for the code to know the current position of the message during the reading
    // THIS IS NOT INFORMATION FROM THE MESSAGE ITSELF
    unsigned int pointer_pos;
    unsigned int package_size;
    unsigned int reason_code;

    // A pointer to the original message. Will be useful to just send message to subscribers, so that we don't need to reconstruct all message (almost no fields will be modified anyway)
    uint8_t *ptr_to_msg;

    // As there's nothing to process on header except checking its values, there's no need to store them
    // on this struct
    //publish_header_flags_t header_flags;

    uint16_t topic_name_len;
    char *topic_name; //! Important: this is null-terminated

    // Will only be present on PUBLISH packet where QoS > 0, which is not the case
    //uint16_t packet_id;

    publish_request_properties_t properties;

} publish_request_t;

typedef struct {
    char *topic;
    uint8_t *publish_packet;
    size_t publish_packet_size;
} cli_to_man_pub_t;

// Created when a PUBLISH package shall be distributed between client threads that subscribed to the related topic.
// The responsibility of keeping track the amount of subscribers that already wrote to their respective sockets is
// distributed among the subscribers, with the last one to finish writing freeing the packet allocated in memory.
typedef struct {
    uint8_t *publish_packet;
    unsigned int packet_size;
    int counter; //! NOTE: MUST BE BIGGER THAN 0
    pthread_mutex_t counter_lock;
} man_to_cli_pub_t;

void freePublishMessageStruct(publish_request_t *pub);

// ATTENTION: do not free the message when receiving a PUBLISH packet. Just send it to the manager thread.
publish_request_t *getPublishFromMessage(uint8_t *message, size_t message_size);

#endif