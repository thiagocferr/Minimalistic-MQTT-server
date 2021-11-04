#ifndef _MQTT_CONNACK_H
#define _MQTT_CONNACK_H

#include "mqtt.h"
#include "mqtt_connect.h"
#include "misc.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    unsigned int session_present: 1;
} connack_flags_t;

typedef struct {

    uint32_t session_expiry_interval;
    uint16_t receive_maximum;
    uint8_t maximum_qos;
    uint8_t retain_available;
    uint32_t maximum_packet_size;
    size_t assigned_client_identifier_size;
    char *assigned_client_identifier;
    uint16_t topic_alias_maximum;
    //char *reason_string;
    //utf_str_pair_t *user_property;
    uint8_t wildcard_subscription_available;
    uint8_t subscription_identifiers_available;
    uint8_t shared_subscription_available;
    uint16_t server_keep_alive;
    //char *response_information;
    //char *server_reference;
    //char *authentication_method;
    //uint16_t authentication_data;

} connack_properties_t;

typedef struct {
    unsigned int  session_expiry_interval : 1;
    unsigned int  receive_maximum: 1;
    unsigned int  maximum_qos: 1;
    unsigned int  retain_available: 1;
    unsigned int  maximum_packet_size: 1;
    unsigned int  assigned_client_identifier: 1;
    unsigned int  topic_alias_maximum: 1;
    unsigned int  reason_string: 1;
    //unsigned int  user_property : 1;
    unsigned int  wildcard_subscription_available: 1;
    unsigned int  subscription_identifiers_available: 1;
    unsigned int  shared_subscription_available: 1;
    unsigned int  server_keep_alive: 1;
    //unsigned int  response_information: 1;
    //unsigned int  server_reference: 1;
    //unsigned int  authentication_method: 1;
    //unsigned int  authentication_data: 1;
} connack_properties_switch_t;


typedef struct {

    // This struct is only used for the code to know the current position of the message during the reading
    // THIS IS NOT INFORMATION FROM THE MESSAGE ITSELF
    unsigned int pointer_pos;

    variable_byte_int_t remaining_length;

    connack_flags_t flags;

    uint8_t connect_reason_code;

    variable_byte_int_t properties_length;
    connack_properties_t properties;
    connack_properties_switch_t properties_switch;

} connection_ack_t;

// Allocates and returns message generated from CONNECT pos-processed information.
// Get message size from parameter 'result_message_size'
uint8_t *generateMessageFromConnection(connection_request_t *conn_req, size_t *result_message_size);

#endif