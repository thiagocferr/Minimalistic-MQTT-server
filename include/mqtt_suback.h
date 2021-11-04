#ifndef _MQTT_SUBACK_H
#define _MQTT_SUBACK_H

#include "mqtt.h"
#include "mqtt_subscribe.h"
#include "misc.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {

    variable_byte_int_t remaining_length;
    uint16_t packet_id;

    // For now, properties will NOT be supported
    variable_byte_int_t properties_len;

    size_t num_reason_codes;
    uint8_t *reason_code_list;

} subscriber_ack_t;

typedef struct {
    // If there has happened something that could trigger a new reason code do be sent,
    // this will be different than 0
    uint8_t *new_reason_codes;
    int num_of_topics;
} man_to_cli_suback_t;

uint8_t *generateMessageFromSub(subscribe_request_t *sub_req, size_t *result_message_size);

#endif