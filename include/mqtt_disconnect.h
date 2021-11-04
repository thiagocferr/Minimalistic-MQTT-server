# ifndef _MQTT_DISCONNECT_H
#define _MQTT_DISCONNECT_H

#include "mqtt.h"
#include "misc.h"

#include <stdint.h>

typedef struct {
    char *client_id;
} cli_to_man_disconnect_t;

uint8_t getReasonCodeFromMessage(uint8_t *message);
uint8_t *getDisconnectFromCode(uint8_t reason_code, int *packet_size);

#endif