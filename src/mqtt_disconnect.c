
#include "mqtt_disconnect.h"

uint8_t getReasonCodeFromMessage(uint8_t *message) {
    uint8_t *ptr = message;

    ptr += 1;

    if (decodeVariableByteIntVal(ptr) == 0) {
        return 0;
    }

    ptr += variableByteIntSizeFromMessage(ptr);

    return ptr[0];
}

uint8_t *getDisconnectFromCode(uint8_t reason_code, int *packet_size) {
    // This is not accounting for the fact that 'remain length' is a variable byte integer,
    // but since this message is short enough, we can hardcode it.
    uint8_t *message = malloc(3);
    message[0] = 0xe0;
    message[1] = 1;
    message[2] = reason_code;

    *packet_size = 3;

    return message;
}