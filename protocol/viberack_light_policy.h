#ifndef VIBERACK_LIGHT_POLICY_H
#define VIBERACK_LIGHT_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#include "viberack_protocol.h"

typedef struct {
    uint8_t mode;
    uint16_t timeout_s;
    bool power_on;
} vbrk_light_policy_t;

int vbrk_light_policy_resolve(const vbrk_light_command_t *command,
                              vbrk_light_policy_t *policy);

#endif
