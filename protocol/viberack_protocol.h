#ifndef VIBERACK_PROTOCOL_H
#define VIBERACK_PROTOCOL_H

#include <stdint.h>

#define VBRK_PROTO_VER 0x01
#define VBRK_SLOT_COUNT 25
#define VBRK_SLOT_RECORD_SIZE 16
#define VBRK_LIGHT_COMMAND_SIZE 17
#define VBRK_TABLE_INFO_SIZE 7
#define VBRK_ADV_MSD_SIZE 11
#define VBRK_READ_ALL_END_MARKER 0xFF
#define VBRK_DEV_COMPANY_ID 0xFFFF
#define VBRK_FACTORY_RESET_MAGIC 0x5A5AA5A5u

typedef enum {
    VBRK_STATUS_OK = 0x00,
    VBRK_STATUS_ERR_PARAM = 0x01,
    VBRK_STATUS_ERR_FULL = 0x02,
    VBRK_STATUS_ERR_FLASH_BUSY = 0x03,
    VBRK_STATUS_ERR_CRC = 0x04,
} vbrk_status_t;

typedef enum {
    VBRK_OP_READ_ONE = 0x01,
    VBRK_OP_READ_ALL = 0x02,
    VBRK_OP_WRITE_ONE = 0x10,
    VBRK_OP_CLEAR_ONE = 0x11,
    VBRK_OP_INSERT_AT = 0x20,
    VBRK_OP_REMOVE_AT = 0x21,
    VBRK_OP_MOVE_BLOCK = 0x22,
    VBRK_OP_SET_QTY = 0x30,
    VBRK_OP_FACTORY_RESET = 0xF0,
} vbrk_binding_op_t;

typedef enum {
    VBRK_LIGHT_OFF = 0x00,
    VBRK_LIGHT_FIND = 0x01,
    VBRK_LIGHT_PICK = 0x02,
    VBRK_LIGHT_SORT = 0x03,
    VBRK_LIGHT_STOCK_IN = 0x04,
    VBRK_LIGHT_FX = 0x05,
} vbrk_light_mode_t;

typedef enum {
    VBRK_ADV_LOW_BATTERY = 1 << 0,
    VBRK_ADV_HAS_UNBOUND_SLOT = 1 << 1,
    VBRK_ADV_LIGHT_ACTIVE = 1 << 2,
    VBRK_ADV_FAULT = 1 << 3,
} vbrk_adv_flags_t;

typedef enum {
    VBRK_SLOT_FLAG_MSD = 1 << 0,
    VBRK_SLOT_FLAG_LOW_STOCK = 1 << 1,
    VBRK_SLOT_FLAG_CUSTOM_PART = 1 << 2,
} vbrk_slot_flags_t;

typedef struct __attribute__((packed)) {
    uint8_t slot;
    char part_id[10];
    uint16_t qty_le;
    uint8_t flags;
    uint8_t reserved;
    uint8_t crc8;
} vbrk_slot_record_t;

typedef struct __attribute__((packed)) {
    uint8_t mode;
    uint32_t mask_a_le;
    uint32_t mask_b_le;
    uint8_t color_a[3];
    uint8_t color_b[3];
    uint16_t timeout_s_le;
} vbrk_light_command_t;

typedef struct __attribute__((packed)) {
    uint32_t table_seq_le;
    uint16_t crc16_le;
    uint8_t slot_count;
} vbrk_table_info_t;

uint8_t vbrk_crc8_maxim(const uint8_t *data, uint32_t len);
uint16_t vbrk_crc16_ccitt_false(const uint8_t *data, uint32_t len);
uint32_t vbrk_slot_mask(uint8_t slot);

#endif
