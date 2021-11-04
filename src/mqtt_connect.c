
#include "mqtt_connect.h"

static void print_message(uint8_t *message, connection_request_t *result) {
    print_str_as_hex((char *) message, result->package_size - result->pointer_pos);
}

// Advance message pointer by a specif amount
static uint8_t * advanceReadingBy(uint8_t *message, connection_request_t *result, int size) {
    result->pointer_pos += size;
    return message + size;
}

static unsigned int getKeepAlive(uint8_t *message) {
    return combineBytes(message, 2);
}

// Assuming "message" already points to where the string size should be declared
static int isProtocolNameRight(uint8_t *message) {
    //Note: Two first bytes are the size of the protocol name. After it, the actual string
    uint8_t *ptr = message + 2;

    char protocol_name[5];
    memcpy(protocol_name, ptr, 4);
    protocol_name[4] = 0;

    if (strcmp(protocol_name, "MQTT") == 0)
        return 1;
    return 0;

}

static int isProtocolVersion5(uint8_t *message) {
    return (message[0] == 5);
}

static uint8_t *processConnectFlags(uint8_t *message, connection_request_t *result) {
    // Connect flags analisys
    // NOTE: Server does not support flags: User Name, Password and Will Flags
    // As this server does not support failures, there will not be a Will message to send

    if (CHECK_BIT(message[0], 0)) {
        result->reason_code = 0x81; // Malformed
        return message;
    }

    // Clean start flag
    if (CHECK_BIT(message[0], 1))
        result->connect_flags.clean_start = 1;
    else
        result->connect_flags.clean_start = 0;

    // As we are not worring about client failures, we will not support Will messages, so setting it
    // is a 0x83 error: Implementation specific error

    // Checking will flag
    if (CHECK_BIT(message[0], 2) != 0) {
        result->connect_flags.will_flag = 1;
        result->reason_code = 0x83; // Implementation specific error
        return message;
    }
    // Checking will QoS
    if (CHECK_BIT(message[0], 2) == 0 && getValFromBits(message[0], 3, 2) != 0) {
        result->connect_flags.will_qos = getValFromBits(message[0], 3, 2);
        result->reason_code = 0x81; // Malformed
        return message;
    }
    // Checking will retain
    if (CHECK_BIT(message[0], 2) == 0 && getValFromBits(message[0], 5, 1) != 0) {
        result->connect_flags.will_qos = getValFromBits(message[0], 5, 1);
        result->reason_code = 0x81; // Malformed
        return message;
    }
    // Checking auth
    if (getValFromBits(message[0], 6, 2) != 0) {
        result->reason_code = 0x8C; // Bad authentication method
        return message;
    }

    return advanceReadingBy(message, result, 1);
}

// Process fixed header that all packets have
static uint8_t * processFixedHeader(uint8_t *message, connection_request_t *result) {

    int code = codeMessageHeaderFlagsValid(message, CONNECT);
    if (code != 0) {
        result->reason_code = code;
        return message;
    }

    message = advanceReadingBy(message, result, 1);

    // Remaining Length analisys

    variable_byte_int_t remaining_length = decodeVariableByteIntVal(message);
    if (remaining_length != (result->package_size) - variableByteIntSizeFromMessage(message) - 1) {
        result->reason_code = 0x81;
        return message;
    }

    message = advanceReadingBy(message, result, variableByteIntSizeFromMessage(message));
    return message;

}

// Process each property that may be available on packet. If there's a Malformed Packet sign, end all analisys
// NOTE: This implementation also does not verify for multiple sets of the same field, which would be advised by RFC
static uint8_t * processIndividualProperties (uint8_t *message, connection_request_t *result) {

    uint8_t property_code = message[0];
    message = advanceReadingBy(message, result, 1); // Advance 1 byte (that was the property code)

    unsigned int num_bytes;

    switch (property_code) {
        case 0x11:// Session Expiry Interval
            num_bytes = 4;
            result->properties.session_expire_interval = combineBytes(message, num_bytes);
            break;

        case 0x21: // Receive Maximum
            num_bytes = 2;
            result->properties.receive_maximum = combineBytes(message, num_bytes);
            break;

        case 0x27: //Maximum Packet Size
            num_bytes = 4;
            result->properties.maximum_packet_size = combineBytes(message, num_bytes);
            break;

        case 0x22: //Topic Alias Maximum
            num_bytes = 2;
            result->properties.topic_alias_maximum = combineBytes(message, num_bytes);
            break;

        case 0x19: // Request Response Information
            num_bytes = 1;
            result->properties.request_response_information = message[0];
            break;

        case 0x17: //Request Problem Information
            num_bytes = 1;
            result->properties.request_problem_information = message[0];
            break;

        // TODO: this is not implemented yet! We are just skipping the verification
        case 0x26: //User Property
            {
                size_t str1_len = combineBytes(message, 2);
                uint8_t *message_at_next_str = message + 2 + str1_len;

                size_t str2_len = combineBytes(message_at_next_str, 2);
                num_bytes = str1_len + str2_len + 4;
                break;
            }


        // NOTE: Authenticaion not implemented! Will just ignore!
        case 0x15: // Authentication Method
            result->reason_code = 0x8C; //Bad authentication method
            return NULL;
        case 0x16: // Authentication Data
            result->reason_code = 0x8C; // Bad authentication method
            return NULL;
        default:
            result->reason_code = 0x81; // Malformed Packet
            return NULL;

    }

    return advanceReadingBy(message, result, num_bytes);
}

// Process packet properties fields
static uint8_t *processProperties (uint8_t *message, connection_request_t *result) {

    result->properties.properties_len = decodeVariableByteIntVal(message);
    message = advanceReadingBy(message, result, variableByteIntSizeFromMessage(message));

    if (result->properties.properties_len == 0)
        return message;

    unsigned int initial_pointer_pos = result->pointer_pos;
    unsigned int properties_len = result->properties.properties_len;

    while (result->pointer_pos < initial_pointer_pos + properties_len) {
        message = processIndividualProperties(message, result);

        // If there was a malformation on the properties, abort all processing
        if (message == NULL)
            return NULL;
    }

    return message;
}


// Process packet variable header fields
static uint8_t * processVariableHeader(uint8_t *message, connection_request_t *result) {

    //  Protocol Field analisys

    if (!isProtocolNameRight(message)) {
        result->reason_code = 0x81; // Malformed
        return message;
    }
    message = advanceReadingBy(message, result, 2 + 4);

    // Protocol version analisys

    if (!isProtocolVersion5(message)) {
        result->reason_code = 0x84; // Unsupported version
        return message;
    }
    result->version = message[0];
    message = advanceReadingBy(message, result, 1);

    message = processConnectFlags(message, result);

    // Keep Alive analisys

    result->keep_alive = getKeepAlive(message);
    message = advanceReadingBy(message, result, 2);

    // Before Properties Analisys (standart values)

    result->properties.session_expire_interval = 0;
    result->properties.topic_alias_maximum = 0;
    result->properties.receive_maximum = 65535;
    result->properties.maximum_packet_size = MAX_PACKAGE_SIZE;
    result->properties.topic_alias_maximum = 0;
    result->properties.request_response_information = 0;
    result->properties.request_problem_information = 1;

    // Properties Analisys

    message = processProperties(message, result);

    return message;

}



// For now, payload will consist only of the ClientID.
//! Attention: the client_id registered on main struct is NOT null terminated
// (although internal broker representation is)
static uint8_t *processPayload(uint8_t *message, connection_request_t *result) {

    uint16_t client_id_len = combineBytes(message, 2);
    result->client_id_len = client_id_len;

    message = advanceReadingBy(message, result, 2);

    if (result->client_id_len > 0) {
        result->client_id = malloc(client_id_len * sizeof (char));
        result->isIdSetByServer = 0;
    } else {
        result->client_id = malloc(MAX_CLIENT_ID_SIZE * sizeof (char));
        result->client_id_len = MAX_CLIENT_ID_SIZE;

        generateRandomID(result->client_id, MAX_CLIENT_ID_SIZE);
        result->isIdSetByServer = 1;
    }

    return message;
}

static connection_request_t *allocateConnectMessageStruct() {
    connection_request_t *conn = malloc(sizeof (connection_request_t));
    if (conn == NULL)
        return NULL;

    // Need to be set after we know the actual size of the client ID
    conn->client_id = NULL;
    return conn;
}

// Generate a random ID fo size 'size' and put it on array of MINIMUM SIZE 'str'
void generateRandomID(char *str, size_t size) {
    const char char_set[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

    for (int i = 0; i < (int) size; i++) {
        str[i] = char_set[rand() % (int) (sizeof (char_set) - 1)];
    }
}

void freeConnectMessageStruct(connection_request_t *conn) {
    if (conn != NULL) {
        if (conn->client_id != NULL) {
            free(conn->client_id);
        }
        free(conn);
    }
}

connection_request_t *getConnectionFromMessage(uint8_t *message, size_t message_size) {


    connection_request_t *result = allocateConnectMessageStruct();

    result->pointer_pos = 0;
    result->package_size = message_size;
    result->reason_code = 0;

    uint8_t *ptr = message;

    // Fixed header analisys

    ptr = processFixedHeader(ptr, result);

    // Variable header analisys

    ptr = processVariableHeader(ptr, result);

    // Payload analisys

    ptr = processPayload(ptr, result);

    return result;

}