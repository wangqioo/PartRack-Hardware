#ifndef VIBERACK_LIGHT_STATE_H
#define VIBERACK_LIGHT_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "viberack_light_policy.h"
#include "viberack_protocol.h"

typedef struct {
    uint8_t mode;
    int64_t expires_at_ms;
} vbrk_light_state_t;

void vbrk_light_state_init(vbrk_light_state_t *state);
void vbrk_light_state_force_off(vbrk_light_state_t *state);
int vbrk_light_state_apply(vbrk_light_state_t *state,
                           const vbrk_light_command_t *command,
                           int64_t now_ms,
                           vbrk_light_policy_t *policy);
bool vbrk_light_state_tick(vbrk_light_state_t *state, int64_t now_ms);
uint8_t vbrk_light_state_mode(const vbrk_light_state_t *state);
bool vbrk_light_state_is_active(const vbrk_light_state_t *state);
uint16_t vbrk_light_state_remaining_s(const vbrk_light_state_t *state,
                                      int64_t now_ms);

#endif
