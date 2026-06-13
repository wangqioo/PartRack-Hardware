#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "viberack_light_state.h"

static vbrk_light_command_t make_command(uint8_t mode, uint16_t timeout_s)
{
    vbrk_light_command_t command;

    memset(&command, 0, sizeof(command));
    command.mode = mode;
    command.timeout_s_le = timeout_s;
    return command;
}

static void test_apply_sets_mode_and_remaining(void)
{
    vbrk_light_state_t state;
    vbrk_light_policy_t policy;
    vbrk_light_command_t command = make_command(VBRK_LIGHT_PICK, 5);

    vbrk_light_state_init(&state);
    assert(vbrk_light_state_apply(&state, &command, 1000, &policy) == 0);
    assert(policy.mode == VBRK_LIGHT_PICK);
    assert(policy.timeout_s == 5);
    assert(vbrk_light_state_mode(&state) == VBRK_LIGHT_PICK);
    assert(vbrk_light_state_is_active(&state));
    assert(vbrk_light_state_remaining_s(&state, 1000) == 5);
    assert(vbrk_light_state_remaining_s(&state, 5001) == 1);
}

static void test_repeated_command_refreshes_timeout(void)
{
    vbrk_light_state_t state;
    vbrk_light_command_t command = make_command(VBRK_LIGHT_FIND, 5);

    vbrk_light_state_init(&state);
    assert(vbrk_light_state_apply(&state, &command, 0, NULL) == 0);
    assert(vbrk_light_state_remaining_s(&state, 4000) == 1);
    assert(vbrk_light_state_apply(&state, &command, 4000, NULL) == 0);
    assert(vbrk_light_state_remaining_s(&state, 4000) == 5);
    assert(vbrk_light_state_remaining_s(&state, 8999) == 1);
}

static void test_tick_expires_once(void)
{
    vbrk_light_state_t state;
    vbrk_light_command_t command = make_command(VBRK_LIGHT_SORT, 2);

    vbrk_light_state_init(&state);
    assert(vbrk_light_state_apply(&state, &command, 1000, NULL) == 0);
    assert(!vbrk_light_state_tick(&state, 2999));
    assert(vbrk_light_state_tick(&state, 3000));
    assert(vbrk_light_state_mode(&state) == VBRK_LIGHT_OFF);
    assert(!vbrk_light_state_is_active(&state));
    assert(vbrk_light_state_remaining_s(&state, 3000) == 0);
    assert(!vbrk_light_state_tick(&state, 4000));
}

static void test_off_command_clears_state(void)
{
    vbrk_light_state_t state;
    vbrk_light_policy_t policy;
    vbrk_light_command_t on = make_command(VBRK_LIGHT_STOCK_IN, 30);
    vbrk_light_command_t off = make_command(VBRK_LIGHT_OFF, 30);

    vbrk_light_state_init(&state);
    assert(vbrk_light_state_apply(&state, &on, 0, NULL) == 0);
    assert(vbrk_light_state_apply(&state, &off, 1000, &policy) == 0);
    assert(policy.mode == VBRK_LIGHT_OFF);
    assert(!policy.power_on);
    assert(vbrk_light_state_mode(&state) == VBRK_LIGHT_OFF);
    assert(vbrk_light_state_remaining_s(&state, 1000) == 0);
}

int main(void)
{
    test_apply_sets_mode_and_remaining();
    test_repeated_command_refreshes_timeout();
    test_tick_expires_once();
    test_off_command_clears_state();
    return 0;
}
