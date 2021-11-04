
#include "misc.h"


// Gets "fieldLength" bits from "data" staringt at the bit "bitIndex", from the least to most siginificant bits
// Obs: bitIndex starts at 0
// This solution was described at https://stackoverflow.com/a/27592777
unsigned int getValFromBits(unsigned int data, unsigned int bitIndex, unsigned int fieldLength) {

    //bitIndex = bitIndex + 1;
    char mask = ((1 << fieldLength) - 1) << (bitIndex);

    return (data & mask) >> bitIndex;

}

uint32_t combineBytes(uint8_t *bytes, size_t num_of_bytes) {

    // As the output can only have up to 4 bytes, that's the maximum we can do
    if (num_of_bytes > 4 || num_of_bytes < 2) {
        abort();
    }

    switch (num_of_bytes)
    {
        case 2:
            return ((int) bytes[0] << 8) | bytes[1];
        case 3:
            return ((int) bytes[0] << 16) | ((int) bytes[1] << 8) | bytes[2];
        case 4:
            return (((int) bytes[0] << 24) | (int) bytes[1] << 16) | ((int) bytes[2] << 8) | bytes[3];
        default:
            abort();
    }
}

// Receive a number and store its bytes on an array 'split_data', where index 0 is occupied by the
// most significant byte (if the data passed to this function is less than 32 bits, it disconsiders
// the padding by checking parameter 'original_num_bytes', which gives the original data size)
void separateBytes(uint32_t data, uint8_t *split_data, size_t original_num_bytes) {

    // Based on https://stackoverflow.com/a/3784478
    for (int i = 0; i < (int) original_num_bytes; i++) {
        split_data[i] = (data >> (8 * ((original_num_bytes - i) - 1))) & 0xFF;
    }
}

void print_str_as_hex(char *string, size_t len) {
    for (int i = 0; i < (int) len; i ++) {
        printf("%02x ", (unsigned)string[i]);
    }
    putchar('\n');
}

// Test packet of CONNECT
uint8_t * getTestPacket() {
    // Test package has 18 bytes
    uint8_t *connect_package = malloc(18);

    connect_package[0] = 0x10;
    connect_package[1] = 0x10;
    connect_package[2] = 0x00;
    connect_package[3] = 0x04;
    connect_package[4] = 0x4d;
    connect_package[5] = 0x51;
    connect_package[6] = 0x54;
    connect_package[7] = 0x54;
    connect_package[8] = 0x05;
    connect_package[9] = 0x02;
    connect_package[10] = 0x00;
    connect_package[11] = 0x3c;
    connect_package[12] = 0x03;
    connect_package[13] = 0x21;
    connect_package[14] = 0x00;
    connect_package[15] = 0x14;
    connect_package[16] = 0x00;
    connect_package[17] = 0x00;

    print_str_as_hex((char *)connect_package, 18);

    return connect_package;

}