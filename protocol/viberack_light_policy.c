#include "viberack_light_policy.h"

#include <errno.h>
#include <string.h>

static uint16_t command_timeout_s(const vbrk_light_command_t *command)
{
    uint16_t timeout = command->timeout_s_le;

    if (timeout == 0) {
        return 30;
    }
    if (timeout > 300) {
        return 300;
    }

    return timeout;
}

int vbrk_light_policy_resolve(const vbrk_light_command_t *command,
                              vbrk_light_policy_t *policy)
{
    uint16_t timeout_s;

    if (command == NULL || policy == NULL) {
        return -EINVAL;
    }

    memset(policy, 0, sizeof(*policy));
    if (command->mode > VBRK_LIGHT_FX) {
        return -EINVAL;
    }

    policy->mode = command->mode;
    if (command->mode == VBRK_LIGHT_OFF) {
        return 0;
    }

    timeout_s = command_timeout_s(command);
    if (command->mode == VBRK_LIGHT_FX && timeout_s > 10) {
        timeout_s = 10;
    }

    policy->timeout_s = timeout_s;
    policy->power_on = true;
    return 0;
}
