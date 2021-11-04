
#include "mqtt_publish.h"

static uint8_t * advanceReadingBy(uint8_t *message, publish_request_t *result, int size) {
    result->pointer_pos += size;
    return message + size;
}

static uint8_t * processFixedHeader(uint8_t *ptr, publish_request_t *result) {

    // Control packet type
    int code = codeMessageHeaderFlagsValid(ptr, PUBLISH);
    if (code != 0) {
        result->reason_code = code;
        return ptr;
    }

    ptr = advanceReadingBy(ptr, result, 1);

    // Remaining length - SKIP
    ptr = advanceReadingBy(ptr, result, variableByteIntSizeFromMessage(ptr));

    return ptr;
}

static uint8_t * processIndividualProperties (uint8_t *ptr, publish_request_t *result) {
    uint8_t property_code = ptr[0];
    ptr = advanceReadingBy(ptr, result, 1);

    unsigned int num_bytes;

    switch (property_code) {
        case 0x02: //Message Expiry Interval
            num_bytes = 4;

            result->properties.is_message_expiry_interval_defined = 1;
            result->properties.message_expiry_interval = combineBytes(ptr, num_bytes);

            break;
        case 0x0B: // Subscription Identifier
            num_bytes = variableByteIntSizeFromMessage(ptr);
            result->properties.subscription_identifier = decodeVariableByteIntVal(ptr);

            break;
        // Will not register other properties no check for malformed packets
        default:
            break;
    }

    return advanceReadingBy(ptr, result, num_bytes);
}

static uint8_t *processVariableHeader(uint8_t *ptr, publish_request_t *result) {

    // Topic name
    result->topic_name_len = combineBytes(ptr, 2);
    ptr = advanceReadingBy(ptr, result, 2);

    // We save topic name as a readable string on the struct
    result->topic_name = malloc((result->topic_name_len + 1) * sizeof (char));
    memcpy(result->topic_name, ptr, result->topic_name_len);
    result->topic_name[result->topic_name_len] = '\0';
    ptr = advanceReadingBy(ptr, result, result->topic_name_len);

    // Packet ID - SKIP
    ptr = advanceReadingBy(ptr, result, 2);

    // PROPERTIES

    // Properties Length
    variable_byte_int_t properties_len = decodeVariableByteIntVal(ptr);
    ptr = advanceReadingBy(ptr, result, variableByteIntSizeFromMessage(ptr));

    // If there are properties, get them
    if (properties_len > 0) {
        unsigned int initial_pointer_pos = result->pointer_pos;
        while (result->pointer_pos < initial_pointer_pos + properties_len) {
            ptr = processIndividualProperties(ptr, result);
            if (ptr == NULL)
                return NULL;
        }
    }

    return ptr;
}

void freePublishMessageStruct(publish_request_t *pub) {
    // This will need to be used when client thread goes passing the PUBLISH message to the manager,
    // so it should be freed there
    free(pub);
}

static publish_request_t *allocatePublishStruct() {
    publish_request_t *pub = malloc(sizeof(publish_request_t));
    if (pub == NULL) {
        return NULL;
    }

    return pub;
}

publish_request_t *getPublishFromMessage(uint8_t *message, size_t message_size) {

    publish_request_t *result = allocatePublishStruct();
    result->pointer_pos = 0;
    result->package_size = message_size;
    result->reason_code = 0;
    result->properties.is_message_expiry_interval_defined = 0;

    result->ptr_to_msg = message;
    uint8_t *ptr = message;

    ptr = processFixedHeader(ptr, result);
    ptr = processVariableHeader(ptr, result);
    // No need to process payload

    return result;

}