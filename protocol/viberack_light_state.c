#include "viberack_light_state.h"

#include <errno.h>
#include <stddef.h>

#define MSEC_PER_SEC_MODEL 1000

void vbrk_light_state_init(vbrk_light_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->mode = VBRK_LIGHT_OFF;
    state->expires_at_ms = 0;
}

void vbrk_light_state_force_off(vbrk_light_state_t *state)
{
    vbrk_light_state_init(state);
}

int vbrk_light_state_apply(vbrk_light_state_t *state,
                           const vbrk_light_command_t *command,
                           int64_t now_ms,
                           vbrk_light_policy_t *policy)
{
    vbrk_light_policy_t resolved;
    int err;

    if (state == NULL) {
        return -EINVAL;
    }

    err = vbrk_light_policy_resolve(command, &resolved);
    if (err != 0) {
        return err;
    }

    if (!resolved.power_on) {
        vbrk_light_state_force_off(state);
    } else {
        state->mode = resolved.mode;
        state->expires_at_ms = now_ms + ((int64_t)resolved.timeout_s * MSEC_PER_SEC_MODEL);
    }

    if (policy != NULL) {
        *policy = resolved;
    }

    return 0;
}

bool vbrk_light_state_tick(vbrk_light_state_t *state, int64_t now_ms)
{
    if (state == NULL || state->mode == VBRK_LIGHT_OFF || state->expires_at_ms == 0) {
        return false;
    }

    if (now_ms < state->expires_at_ms) {
        return false;
    }

    vbrk_light_state_force_off(state);
    return true;
}

uint8_t vbrk_light_state_mode(const vbrk_light_state_t *state)
{
    if (state == NULL) {
        return VBRK_LIGHT_OFF;
    }

    return state->mode;
}

bool vbrk_light_state_is_active(const vbrk_light_state_t *state)
{
    return state != NULL && state->mode != VBRK_LIGHT_OFF && state->expires_at_ms != 0;
}

uint16_t vbrk_light_state_remaining_s(const vbrk_light_state_t *state,
                                      int64_t now_ms)
{
    int64_t remaining_ms;

    if (!vbrk_light_state_is_active(state)) {
        return 0;
    }

    remaining_ms = state->expires_at_ms - now_ms;
    if (remaining_ms <= 0) {
        return 0;
    }

    return (uint16_t)((remaining_ms + MSEC_PER_SEC_MODEL - 1) / MSEC_PER_SEC_MODEL);
}
