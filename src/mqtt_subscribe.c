
#include "mqtt_subscribe.h"

// Advance message pointer by a specif amount
static uint8_t * advanceReadingBy(uint8_t *message, subscribe_request_t *result, int size) {
    result->pointer_pos += size;
    return message + size;
}

static uint8_t * processFixedHeader(uint8_t *message, subscribe_request_t *result) {

    // Control packet type
    int code = codeMessageHeaderFlagsValid(message, SUBSCRIBE);
    if (code != 0) {
        result->reason_code = code;
        return message;
    }
    message = advanceReadingBy(message, result, 1);

    // Remaining length
    variable_byte_int_t remaining_length = decodeVariableByteIntVal(message);
    if (remaining_length != (result->package_size) - variableByteIntSizeFromMessage(message) - 1) {
        result->reason_code = 0x81;
        return message;
    }
    message = advanceReadingBy(message, result, variableByteIntSizeFromMessage(message));

    return message;
}

static uint8_t * processIndividualProperties (uint8_t *message, subscribe_request_t *result) {
    uint8_t property_code = message[0];
    message = advanceReadingBy(message, result, 1);

    unsigned int num_bytes;

    switch (property_code) {
        case 0x0B: // Subscription Identifier
            result->properties.isSubscribeIdDefined = 1;
            result->properties.subscription_id = decodeVariableByteIntVal(message);

            num_bytes = variableByteIntSizeFromMessage(message);
            break;
        case 0x26: // User Property
            result->reason_code = 0x83; // Implementation specific error
            return NULL;
        default:
            result->reason_code = 0x81; // Malformed Packet
            return NULL;

    }

    return advanceReadingBy(message, result, num_bytes);
}

static uint8_t * processVariableHeader(uint8_t *message, subscribe_request_t *result) {

    result->packet_id = combineBytes(message, 2);
    message = advanceReadingBy(message, result, 2);

    // Properties length field
    variable_byte_int_t properties_len = decodeVariableByteIntVal(message);
    message = advanceReadingBy(message, result, variableByteIntSizeFromMessage(message));

    // If there are properties, get them
    if (properties_len > 0) {
        unsigned int initial_pointer_pos = result->pointer_pos;
        while (result->pointer_pos < initial_pointer_pos + properties_len) {
            message = processIndividualProperties(message, result);
            if (message == NULL)
                return NULL;
        }
    }

    return message;
}

static size_t getNumberOfTopics(uint8_t *message, subscribe_request_t *result) {
    size_t num_of_topics = 0;

    uint8_t *ptr = message;
    unsigned int ptr_pos = result->pointer_pos;

    while (ptr_pos < result->package_size) {
        num_of_topics++;

        uint16_t topic_size = combineBytes(ptr, 2);
        // 2 bytes from topic_size field + 'topic_size' bytes + 1 options byte
        size_t num_bytes_skipped = 2 + topic_size + 1;
        ptr_pos += num_bytes_skipped;

        // If we let pointer go beyond packet size, SEGFAULT
        if (ptr_pos < result->package_size)
            ptr += num_bytes_skipped;
    }

    return num_of_topics;

}

static uint8_t *getTopics(uint8_t *message, subscribe_request_t *result) {

    // Which topic we are seeing now
    int i = 0;

    // In the begginin of every iteration, message will point to first byte of the topic length
    while (result->pointer_pos < result->package_size - 1) {

        // If we are getting more than one topic, last loop left
        // message pointer on previous topic's options. Jump one to
        // current topic block
        if (i > 0) {
            message += 1;
            result->pointer_pos += 1;
        }

        uint16_t topic_size = combineBytes(message, 2);
        message += 2;
        result->pointer_pos += 2;

        subscribe_request_payload_t *curr_payload = (result->topics + i);
        curr_payload->topic_name_size = topic_size;
        curr_payload->topic_name = malloc((topic_size + 1) * sizeof(char));
        memcpy(curr_payload->topic_name, message, topic_size);
        curr_payload->topic_name[topic_size] = '\0';

        // Initiate all reason codes as 0
        curr_payload->reason_code = 0;

        message += topic_size;
        result->pointer_pos += topic_size;

        // NOTE: although we are getting all possible values from flags here,
        // that doesn't mean this will actually take place. For example, as we do
        // not support Retained messages, althoug 'retain_handling' flag might be 1
        // here doesn't mean the messages sent to this subscription will have this behavior
        curr_payload->options.qos = getValFromBits(message[0], 0, 2);
        curr_payload->options.nl = getValFromBits(message[0], 2, 1);
        curr_payload->options.rap = getValFromBits(message[0], 3, 1);
        curr_payload->options.retain_handling = getValFromBits(message[0], 4, 2);

        i++;
    }

    return message;
}

static uint8_t *processPayload(uint8_t *message, subscribe_request_t *result) {
    size_t num_of_topics = getNumberOfTopics(message, result);
    result->num_of_topics = num_of_topics;

    result->topics = malloc(num_of_topics * sizeof(subscribe_request_payload_t));

    message = getTopics(message, result);

    return message;
}

void freeSubscribeMessageStruct(subscribe_request_t *sub) {

    // To leave the freeing of topic names here or to tha manager thread...
    free(sub);
}

// Allocates main fields Subscribe struct. Obs: array of payload entries must
// be allocated separately.
static subscribe_request_t *allocateSubscribeStruct() {

    subscribe_request_t *sub = malloc(sizeof(subscribe_request_t));
    if (sub == NULL)
        return NULL;

    return sub;

}

subscribe_request_t *getSubscribeFromMessage(uint8_t *message, size_t message_size) {

    subscribe_request_t *result = allocateSubscribeStruct(message);
    result->pointer_pos = 0;
    result->package_size = message_size;
    result->reason_code = 0;

    uint8_t *ptr = message;

    ptr = processFixedHeader(ptr, result);
    ptr = processVariableHeader(ptr, result);
    ptr = processPayload(ptr, result);

    return result;
}