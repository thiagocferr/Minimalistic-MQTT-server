#include "mqtt.h"
#include "misc.h"

// Get what kind of message this package represents
message_type_t getMessageType(uint8_t *message) {
    // Message types are in the first 4 more significat bits
    return (message_type_t) message[0] >> 4;
}

// Return 0 if Message header is valid or a non-zero Reason Code for Malformed packages and protocol errors
// NOTE: For messages of type PUBLISH, there should be a way to get the Session QoS, but since this server does
// not support other QoS, this function will not used any of that
unsigned int codeMessageHeaderFlagsValid(uint8_t *message, message_type_t message_type) {

    uint8_t header_flags;

    //print_str_as_hex(message, 1);

    // If fixed header flags are not 0 (the most common one), verify individual cases
    if ((header_flags = getValFromBits(message[0], 0, 4)) != 0) {

        // Grouping message types with same header flags value (in this case, 0x10)
        if ((message_type == PUBREL ||
            message_type == SUBSCRIBE ||
            message_type == UNSUBSCRIBE) &&
            header_flags == 0x02) {
                return 0;
        }

        // PUBLISH packages are treated a little different. Check RFC
        // NOTE: on this server, we only support QoS level 0, so all flags related to it will be setting
        // accordingly (DUP and QoS will be 0)
        if (message_type == PUBLISH) {

            // Checking RETAIN flag (not supported) (Should get this from connection ack)
            if (getValFromBits(header_flags, 0, 1) != 0)
                return 0x9A;

            // Checking QoS levels
            switch (getValFromBits(header_flags, 1, 2))
            {
            case 3: // Malformed package
                return 0x81;
            case 0: // QoS level 0 (only one supported at the moment)
                break;
            default: // Other QoS (QoS not supported) (Should get this from connection ack)
                return 0x9B; // Implementation-specific error
            }

            // If Message QoS is 0, the DUP bit needs to be 0. Otherwise, package is malformed
            if (getValFromBits(header_flags, 3, 1) != 0)
                return 0x81;

            // TODO: Check the connection requested to see if a retain flag was set
            return 0;

        }

        // Besides the package types described above, all header flags should be 0. If code is inside this loop, it
        // means that's not the case and so the package is malformed
        return 0x81;
    }

    return 0;
}

// Get number of bytes occupied by a Variable Byte Integer. If more than four (specification limit)
//  return 0
unsigned int variableByteIntSizeFromMessage(uint8_t *message) {

    int num_bytes = 1;

    // Check how many bytes the length occupies
    for (int i = 0; num_bytes < 4; i++) {

        if (CHECK_BIT(message[i], 7))
            num_bytes++;
        else
            return num_bytes;
    }

    return num_bytes;
}

unsigned int variableByteIntSizeFromNumber(uint32_t number) {
    int n;

    if (number < 127)
        n = 1;
    else if (number < (128*128) - 1)
        n = 2;
    else if (number < (128*128*128) - 1)
        n = 3;
    else if (number < 128*128*128*128)
        n = 4;
    else // Above upper limit for the VAriable Byte Int size
        abort();

    return n;
}

// Given an array of MINIMUM SIZE of 1 and MAXIMUM SIZE of 4, fill it with the encoded version of 'number'
// Obs: FIRST CALL METHOD 'variableByteIntSizeFromNumber' to know how much space this will occupy on message
void encodeVariableByteIntVal(uint32_t number, uint8_t *message) {

    uint8_t encode_byte;

    // If number is 0, at least make sure the first byte is 0 (may not be cleaned before)
    if (number == 0) {
        message[0] = 0;
        return;
    }

    // Above upper limit for the Variable Byte Int size
    if (number > 128*128*128*128) {
        abort();
    }

    for (int i = 0; number > 0; i++) {
        encode_byte = number % 128;
        number = number / 128;
        if (number > 0)
            encode_byte = encode_byte | 128;
        message[i] = encode_byte;
    }

}

// Get number of bytes present after the fixed header
unsigned int decodeVariableByteIntVal(uint8_t *message) {

    uint8_t encodedByte;
    int num_bytes = variableByteIntSizeFromMessage(message);

    // This algorithm is referenced on the specification RFC
    int multiplier  = 1;
    variable_byte_int_t package_size = 0;
    for (int i = 0; i < num_bytes; i++) {
        encodedByte = message[i];
        package_size += (encodedByte & 127) * multiplier;
        multiplier *= 128;
    }

    return package_size;

}
