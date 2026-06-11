#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "viberack_light_policy.h"
#include "viberack_protocol.h"

static vbrk_light_command_t make_command(uint8_t mode, uint16_t timeout_s)
{
    vbrk_light_command_t command;

    memset(&command, 0, sizeof(command));
    command.mode = mode;
    command.timeout_s_le = timeout_s;
    return command;
}

static void test_off_command_turns_power_off(void)
{
    vbrk_light_command_t command = make_command(VBRK_LIGHT_OFF, 120);
    vbrk_light_policy_t policy;

    assert(vbrk_light_policy_resolve(&command, &policy) == 0);
    assert(policy.mode == VBRK_LIGHT_OFF);
    assert(policy.timeout_s == 0);
    assert(policy.power_on == false);
}

static void test_non_off_command_turns_power_on_with_default_timeout(void)
{
    vbrk_light_command_t command = make_command(VBRK_LIGHT_FIND, 0);
    vbrk_light_policy_t policy;

    assert(vbrk_light_policy_resolve(&command, &policy) == 0);
    assert(policy.mode == VBRK_LIGHT_FIND);
    assert(policy.timeout_s == 30);
    assert(policy.power_on == true);
}

static void test_timeout_is_capped_at_five_minutes(void)
{
    vbrk_light_command_t command = make_command(VBRK_LIGHT_PICK, 301);
    vbrk_light_policy_t policy;

    assert(vbrk_light_policy_resolve(&command, &policy) == 0);
    assert(policy.timeout_s == 300);
    assert(policy.power_on == true);
}

static void test_fx_timeout_is_capped_at_ten_seconds(void)
{
    vbrk_light_command_t command = make_command(VBRK_LIGHT_FX, 120);
    vbrk_light_policy_t policy;

    assert(vbrk_light_policy_resolve(&command, &policy) == 0);
    assert(policy.timeout_s == 10);
    assert(policy.power_on == true);
}

static void test_invalid_mode_is_rejected(void)
{
    vbrk_light_command_t command = make_command(VBRK_LIGHT_FX + 1, 30);
    vbrk_light_policy_t policy;

    assert(vbrk_light_policy_resolve(&command, &policy) == -EINVAL);
}

int main(void)
{
    test_off_command_turns_power_off();
    test_non_off_command_turns_power_on_with_default_timeout();
    test_timeout_is_capped_at_five_minutes();
    test_fx_timeout_is_capped_at_ten_seconds();
    test_invalid_mode_is_rejected();
    return 0;
}
