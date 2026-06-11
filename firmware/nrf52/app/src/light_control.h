#ifndef LIGHT_CONTROL_H
#define LIGHT_CONTROL_H

#include <stdint.h>

#include "viberack_protocol.h"

int light_control_init(void);
int light_control_apply(const vbrk_light_command_t *command);
uint8_t light_control_mode(void);
uint16_t light_control_remaining_s(void);

#endif
