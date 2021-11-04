#ifndef _MISC_H
#define _MISC_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

unsigned int getValFromBits(unsigned int data, unsigned int bitIndex, unsigned int fieldLength);
uint32_t combineBytes(uint8_t *bytes, size_t num_of_bytes);
void separateBytes(uint32_t data, uint8_t *split_data, size_t original_num_bytes);

void print_str_as_hex(char *string, size_t len);
uint8_t * getTestPacket();


#endif