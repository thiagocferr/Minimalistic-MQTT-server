#ifndef _MQTT_CONNECT_H
#define _MQTT_CONNECT_H

#include "mqtt.h"
#include "misc.h"
#include "message_queue.h"


#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

typedef struct {
    unsigned int clean_start: 1;
    unsigned int will_flag: 1;
    unsigned int will_qos: 2;
    unsigned int will_retain: 1;
} connect_flags_t;

typedef struct {

    variable_byte_int_t properties_len;
    uint32_t session_expire_interval;
    uint16_t receive_maximum;
    uint32_t maximum_packet_size;
    uint16_t topic_alias_maximum;
    uint8_t request_response_information;
    uint8_t request_problem_information;
    //utf_str_pair_t user_property[MAX_NUM_USER_PROPERTIES];
    //char authentication_method[MAX_STR_SIZE];
    uint16_t authentication_data;

} connect_properties_t;

typedef struct {

    // This struct is only used for the code to know the current position of the message during the reading
    // THIS IS NOT INFORMATION FROM THE MESSAGE ITSELF
    unsigned int pointer_pos;
    unsigned int reason_code;
    unsigned int package_size;

    // MQTT version (number)
    uint8_t version;

    // A number of optional (and not so optional) flags. Check RFC for more details
    connect_flags_t connect_flags;

    // Time, in seconds, representing max time interval between client and server communication
    uint16_t keep_alive;

    connect_properties_t properties;

    uint16_t client_id_len;
    char *client_id; //! IMPORTANT: THIS IS NOT NULL TERMINATED

    int isIdSetByServer;

} connection_request_t;

// Information about the thread client's structure
typedef struct {

    // Used so that just one thread can use client_message_queue at the time
    // or change its liveness status
    pthread_mutex_t *client_message_queue_lock;
    message_queue_t *client_message_queue;
    int *client_alert_pipe;

    char *client_id; //! IMPORTANT: THIS IS NULL-TERMINATED
} cli_to_man_connect_t;

//connection_request_t * allocateConnectMessageStruct(uint8_t *message, size_t message_size);
void freeConnectMessageStruct(connection_request_t *conn);

connection_request_t *getConnectionFromMessage(uint8_t *message, size_t message_size);

void generateRandomID(char *str, size_t size);

#endif