#ifndef _MQTT_H
#define _MQTT_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "misc.h"
#include "mqtt_conf.h"

// If bit is 0, return 0. Otherwise, return non-zero value
#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define NULL ((void *)0)


// In reality, its size is variable, but its maximum size can be contained on an int. Check for "Variable Byte Integer"
// on MQTT RFC for the encoding and decoding algorithm
typedef unsigned int variable_byte_int_t;


// A pair of strings representing "name" and "value"
typedef struct {
    char name[MAX_STR_SIZE];
    char value[MAX_STR_SIZE];
} utf_str_pair_t;

typedef enum {
    RESERVED,
    CONNECT,
    CONNACK,
    PUBLISH,
    PUBACK,
    PUBREC,
    PUBREL,
    PUBCOMP,
    SUBSCRIBE,
    SUBACK,
    UNSUBSCRIBE,
    UNSUBACK,
    PINGREQ,
    PINGRESP,
    DISCONNECT,
    AUTH
} message_type_t;

// Get what kind of message this package represents
message_type_t getMessageType(uint8_t *message);

// Get a code representing the Header Flags check. 0 if all is ok, non-zero for error codes
// an extra check needs to be done on the PUBLISH on another function
unsigned int codeMessageHeaderFlagsValid(uint8_t *message, message_type_t message_type);

unsigned int variableByteIntSizeFromNumber(uint32_t number);
void encodeVariableByteIntVal(uint32_t number, uint8_t *message);

// Get the number of bytes occupied by a Variable Byte Integer (type specified on RFC)
unsigned int variableByteIntSizeFromMessage(uint8_t *message);

// Get the "Remaining Length" information from the fixed header of all packages
unsigned int decodeVariableByteIntVal(uint8_t *message);




#endif