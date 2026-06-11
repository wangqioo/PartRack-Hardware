#include "viberack_protocol.h"

uint8_t vbrk_crc8_maxim(const uint8_t *data, uint32_t len) {
    uint8_t crc = 0x00;

    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if ((crc & 0x01) != 0) {
                crc = (uint8_t)((crc >> 1) ^ 0x8C);
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

uint16_t vbrk_crc16_ccitt_false(const uint8_t *data, uint32_t len) {
    uint16_t crc = 0xFFFF;

    for (uint32_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if ((crc & 0x8000u) != 0) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

uint32_t vbrk_slot_mask(uint8_t slot) {
    if (slot == 0 || slot > VBRK_SLOT_COUNT) {
        return 0;
    }

    return 1u << (slot - 1u);
}
