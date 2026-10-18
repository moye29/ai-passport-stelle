#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
bool chat_wire_hex16(const char *text, uint8_t out[16]);
void chat_wire_header(uint8_t out[16], const uint8_t nonce[16], uint16_t size, uint32_t sequence);
bool chat_wire_accept(const uint8_t *packet, size_t bytes, uint32_t previous, uint32_t *sequence);
