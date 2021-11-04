
#include "mqtt_ping.h"

// Transform a PINGREQ in a PINGRESP. It does NOT create a new message, it just modifies receiving message
void transformPing(uint8_t *ping_req) {

    if (getMessageType(ping_req) == PINGREQ) {
        ping_req[0] = 0xd0;
    }
}