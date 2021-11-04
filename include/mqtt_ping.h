#ifndef _MQTT_PING_H
#define _MQTT_PING_H

#include "misc.h"
#include "mqtt.h"
#include <stdint.h>

// Get a message of type PINGREQ and change it to a PINGRESP
void transformPing(uint8_t *ping_req);

#endif