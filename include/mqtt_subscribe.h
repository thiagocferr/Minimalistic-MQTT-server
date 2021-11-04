#ifndef _MQTT_SUBSCRIBE_H
#define _MQTT_SUBSCRIBE_H

#include "mqtt.h"
#include "misc.h"
#include "mqtt_connect.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    int isSubscribeIdDefined;
    variable_byte_int_t subscription_id;
    //utf_str_pair_t user_property; // Not supported
} subscribe_request_properties_t;

typedef struct {
    unsigned int qos: 2;
    unsigned int nl: 1;
    unsigned int rap: 1;
    unsigned int retain_handling: 2;
} subscription_options_t;

typedef struct {
    uint16_t topic_name_size;
    char *topic_name;
    subscription_options_t options;
    uint8_t reason_code;
} subscribe_request_payload_t;

typedef struct {

    // This struct is only used for the code to know the current position of the message during the reading
    // THIS IS NOT INFORMATION FROM THE MESSAGE ITSELF
    unsigned int pointer_pos;
    unsigned int package_size;
    unsigned int reason_code;

    uint16_t packet_id;
    subscribe_request_properties_t properties;

    size_t num_of_topics;
    subscribe_request_payload_t *topics;

} subscribe_request_t;

// Information sent from client to manager thread about a SIGLE subscription
typedef struct {
    char *client_id; // Client info
    char **topics; // Topics to subscribe into
    int num_of_topics;
} cli_to_man_sub_t;

void freeSubscribeMessageStruct(subscribe_request_t *sub);

subscribe_request_t *getSubscribeFromMessage(uint8_t *message, size_t message_size);


#endif