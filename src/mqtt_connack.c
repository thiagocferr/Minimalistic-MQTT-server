
#include "mqtt_connack.h"

// Set flags and return number of bytes they occupy
static size_t setFlags(connection_ack_t *ack, connection_request_t *conn_req) {

    size_t num_bytes;

    if (ack->connect_reason_code != 0) {
        ack->flags.session_present = 0;
    }

    // NOTE: check would be need here if we implemented Session control
    else if (conn_req->connect_flags.clean_start) {
        ack->flags.session_present = 0;
    }

    // This else block should set flag to 1. As we currently do not implement session persistence, it is set to 0
    else {
        ack->flags.session_present = 0;
    }

    num_bytes = 1;

    return num_bytes;
}

// Set properties and return number of bytes they occupy.
// NOTE: if using some Server-set value, they will be defined as macros on the mqtt_conf.h file
static size_t setProperties(connection_ack_t *ack, connection_request_t *conn_req) {

    size_t num_bytes = 0;

    // Session Expiry Interval
    // Condition to be set: Server is using another value for Session Expiry Interval, potencially
    //  different from the one requested by the Client.
    // As there's no reason to change it, it will not be set differently from the request by the Client


    // Receive Maximum
    // Same as above


    // Maximum QoS
    ack->properties.maximum_qos = MAX_QOS;
    ack->properties_switch.maximum_qos = 1;
    num_bytes += 2;


    // Retain Available
    // As we do not support retained messages, we set a reason code of 0x9A in case the CONNECT
    //  message specified for it
    ack->properties.retain_available = 0;
    ack->properties_switch.retain_available = 1;

    if (conn_req->connect_flags.will_retain != 0)
        ack->connect_reason_code = 0x9A; // Retain not supported
    num_bytes += 2;


    // Maximum Packet Size
    // We can set this field with the macro MAX_PACKAGE_SIZE. It SHOULD contain the maximum size allowed for
    //  the protocol (128^4), but this is decided here

    ack->properties.maximum_packet_size = MAX_PACKAGE_SIZE;
    ack->properties_switch.maximum_packet_size = 1;
    num_bytes += 5;


    //! Assigned Client Identifier
    // If no ID was provided, generate one
    if (conn_req->isIdSetByServer) {

        ack->properties_switch.assigned_client_identifier = 1;
        ack->properties.assigned_client_identifier = conn_req->client_id;

        ack->properties.assigned_client_identifier_size = conn_req->client_id_len;
        num_bytes += 1 + 2 + conn_req->client_id_len; // Identifier + string size + string

    } else {
        ack->properties_switch.assigned_client_identifier = 0;
    }

    // Topic Alias Maximum
    // As the absence of value indicates a value of 0, which is what we want, no value is defined here


    // Reason String
    // Not implemented


    // Reason String
    // Not implemented


    // Wildcard Subscription Available
    // As the absence of specification here indicates support for Wildcards, disable here
    ack->properties.wildcard_subscription_available = 0;
    ack->properties_switch.wildcard_subscription_available = 1;
    num_bytes += 2;

    // Subscription Identifiers Available
    // As we probably want to keep this setting active, let it go unassigned here


    // Shared Subscription Available
    // Disable this property


    // Server Keep Alive
    // Let's not overwrite the CONNECT packet's 'keep alive' attribute for now...

    return num_bytes;
}


// Populate connection_ack_t struct and return total number of bytes that a message will occupy
static size_t prepareConnackStruct(connection_ack_t *ack, connection_request_t *conn_req) {

    // As "Session Present" depends directly on the Reason Code, check it first
    ack->connect_reason_code = conn_req->reason_code;
    ack->remaining_length += 1;

    size_t flags_len = setFlags(ack, conn_req);
    ack->remaining_length += flags_len;

    size_t properties_len = setProperties(ack, conn_req);
    ack->properties_length = properties_len;
    ack->remaining_length += properties_len + variableByteIntSizeFromNumber(properties_len);

    // 1 byte for package type + some bytes for the 'remaining_length' info + the length of flags
    // + some bytes for the 'properties_length' info + the length of properties info (with property
    // identifiers and ocasional size before strings)
    return 1 + variableByteIntSizeFromNumber(ack->remaining_length) + ack->remaining_length;



}

static uint8_t *advancePointerBy(uint8_t *ptr, connection_ack_t *ack, size_t size) {
    ack->pointer_pos += size;
    return ptr + size;
}

// Write properties on the message. As not all the properties are defined / given a value on this file,
// some of the will be skipped. More specifically, only the properties actually set on the 'setProperties'
// function will bre treated here. It could be setup for checking the existance of other setting using the
// 'properties_switch' struct.

static uint8_t *generatePropertiesOnMessage(uint8_t *message, connection_ack_t *ack) {

    encodeVariableByteIntVal(ack->properties_length, message);
    message = advancePointerBy(message, ack, variableByteIntSizeFromNumber(ack->properties_length));

    if (ack->properties_switch.maximum_qos) {
        message[0] = 0x24; // Identifier
        message[1] = ack->properties.maximum_qos;
        message = advancePointerBy(message, ack, 2);
    }

    if (ack->properties_switch.retain_available) {
        message[0] = 0x25; // Identifier
        message[1] = ack->properties.retain_available;
        message = advancePointerBy(message, ack, 2);
    }

    if (ack->properties_switch.maximum_packet_size) {
        message[0] = 0x27; // Identifier
        message = advancePointerBy(message, ack, 1);

        // 4 bytes
        size_t property_size = 4;
        separateBytes(ack->properties.maximum_packet_size, message, property_size);

        message = advancePointerBy(message, ack, 4);
    }

    if (ack->properties_switch.assigned_client_identifier) {

        // Writing flag identifies
        message[0] = 0x12;
        message = advancePointerBy(message, ack, 1);

        // Writing prefix
        unsigned int prefix_val = ack->properties.assigned_client_identifier_size;

        separateBytes(prefix_val, message, 2);
        message = advancePointerBy(message, ack, 2);

        // Writing info itself
        memcpy(message, ack->properties.assigned_client_identifier, prefix_val);
        message = advancePointerBy(message, ack, prefix_val);
    }

    if (ack->properties_switch.wildcard_subscription_available) {
        message[0] = 0x28;
        message[1] = ack->properties.wildcard_subscription_available;

        message = advancePointerBy(message, ack, 2);
    }

    return message;
}

static uint8_t *generateMessageFromStruct(connection_ack_t *ack, size_t total_len) {

    uint8_t *message = malloc(total_len);
    uint8_t *ptr = message;

    // Fixed header flag
    *ptr = 0x20;
    ptr = advancePointerBy(ptr, ack, 1);

    // Write the remaining length of the message encoded on th Variable Byte Int type
    encodeVariableByteIntVal(ack->remaining_length, ptr);
    ptr = advancePointerBy(ptr, ack, variableByteIntSizeFromNumber(ack->remaining_length));

    // Write acknowlefge flags
    *ptr = ack->flags.session_present;
    ptr = advancePointerBy(ptr, ack, 1);

    // Write Reason code
    *ptr = ack->connect_reason_code;
    ptr = advancePointerBy(ptr, ack, 1);

    ptr = generatePropertiesOnMessage(ptr, ack);

    return message;


}

// Allocates and populates a messag array of type CONNACK
uint8_t *generateMessageFromConnection(connection_request_t *conn_req, size_t *result_message_size) {

    connection_ack_t ack;

    ack.remaining_length = 0;
    ack.pointer_pos = 0;

    size_t total_length = prepareConnackStruct(&ack, conn_req);
    *result_message_size = total_length;

    uint8_t *generated_message = generateMessageFromStruct(&ack, total_length);

    return generated_message;

}