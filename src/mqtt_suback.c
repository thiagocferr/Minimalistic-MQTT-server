#include "mqtt_suback.h"

static uint8_t *advancePointerBy(uint8_t *ptr, size_t size) {
    return ptr + size;
}

static uint8_t *generateMessageFromStruct(subscriber_ack_t *ack, size_t total_len) {

    uint8_t *message = malloc(total_len);
    uint8_t *ptr = message;

    // Fixed header flag
    *ptr = 0x90;
    ptr = advancePointerBy(ptr, 1);

    // Write the remaining length of the message encoded on th Variable Byte Int type
    encodeVariableByteIntVal(ack->remaining_length, ptr);
    ptr = advancePointerBy(ptr, variableByteIntSizeFromNumber(ack->remaining_length));

    // Write Message ID
    separateBytes(ack->packet_id, ptr, 2);
    ptr = advancePointerBy(ptr, 2);

    // Write properties_len
    encodeVariableByteIntVal(ack->properties_len, ptr);
    ptr = advancePointerBy(ptr, variableByteIntSizeFromNumber(ack->properties_len));

    // As we are not supporting properties, skip their check

    // List of Reason codes
    for (int i = 0; i < (int) ack->num_reason_codes; i++) {
        *ptr = ack->reason_code_list[i];
        ptr++;
    }
    // Don't forget do free memory allocted for this field!
    free(ack->reason_code_list);

    return message;
}

static size_t prepareSubackStruct(subscriber_ack_t *ack, subscribe_request_t *sub_req) {
    ack->packet_id = sub_req->packet_id;
    ack->remaining_length += 2;

    // Properties length. As we do not support
    ack->properties_len = 0;
    ack->remaining_length += 1;

    // Reason codes
    ack->num_reason_codes = sub_req->num_of_topics;
    ack->reason_code_list = malloc(sub_req->num_of_topics);

    for (int i = 0; i < (int) sub_req->num_of_topics; i++) {
        if (sub_req->topics[i].reason_code != 0) {
            ack->reason_code_list[i] = sub_req->topics[i].reason_code;
        } else {
            // As we only support QoS 0, hardcoded here
            ack->reason_code_list[i] = 0x00;
        }
    }
    ack->remaining_length += sub_req->num_of_topics;

    return 1 + variableByteIntSizeFromNumber(ack->remaining_length) + ack->remaining_length;
}

uint8_t *generateMessageFromSub(subscribe_request_t *sub_req, size_t *result_message_size) {

    subscriber_ack_t ack;
    ack.remaining_length = 0;
    ack.reason_code_list = NULL;

    size_t message_len = prepareSubackStruct(&ack, sub_req);
    *result_message_size = message_len;

    uint8_t *generated_message = generateMessageFromStruct(&ack, message_len);

    return generated_message;

}