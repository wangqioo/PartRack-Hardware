#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "viberack_light_frame.h"
#include "viberack_protocol.h"

static vbrk_light_command_t make_command(uint8_t mode)
{
    vbrk_light_command_t command;

    memset(&command, 0, sizeof(command));
    command.mode = mode;
    command.color_a[0] = 0x10;
    command.color_a[1] = 0x20;
    command.color_a[2] = 0x30;
    command.color_b[0] = 0xA0;
    command.color_b[1] = 0xB0;
    command.color_b[2] = 0xC0;
    return command;
}

static void assert_rgb(const vbrk_rgb_t *rgb, uint8_t r, uint8_t g, uint8_t b)
{
    assert(rgb->r == r);
    assert(rgb->g == g);
    assert(rgb->b == b);
}

static void test_off_clears_all_slots(void)
{
    vbrk_light_command_t command = make_command(VBRK_LIGHT_OFF);
    vbrk_light_frame_t frame;

    memset(&frame, 0xFF, sizeof(frame));
    assert(vbrk_light_frame_build(&command, &frame) == 0);

    for (uint8_t i = 0; i < VBRK_SLOT_COUNT; i++) {
        assert_rgb(&frame.slots[i], 0, 0, 0);
    }
}

static void test_mask_a_sets_color_a_on_selected_slots(void)
{
    vbrk_light_command_t command = make_command(VBRK_LIGHT_FIND);
    vbrk_light_frame_t frame;

    command.mask_a_le = vbrk_slot_mask(1) | vbrk_slot_mask(25);
    assert(vbrk_light_frame_build(&command, &frame) == 0);

    assert_rgb(&frame.slots[0], 0x10, 0x20, 0x30);
    assert_rgb(&frame.slots[24], 0x10, 0x20, 0x30);
    assert_rgb(&frame.slots[1], 0, 0, 0);
}

static void test_mask_b_overrides_mask_a_on_overlap(void)
{
    vbrk_light_command_t command = make_command(VBRK_LIGHT_PICK);
    vbrk_light_frame_t frame;

    command.mask_a_le = vbrk_slot_mask(2) | vbrk_slot_mask(3);
    command.mask_b_le = vbrk_slot_mask(3) | vbrk_slot_mask(4);
    assert(vbrk_light_frame_build(&command, &frame) == 0);

    assert_rgb(&frame.slots[1], 0x10, 0x20, 0x30);
    assert_rgb(&frame.slots[2], 0xA0, 0xB0, 0xC0);
    assert_rgb(&frame.slots[3], 0xA0, 0xB0, 0xC0);
}

static void test_bits_outside_slot_count_are_ignored(void)
{
    vbrk_light_command_t command = make_command(VBRK_LIGHT_STOCK_IN);
    vbrk_light_frame_t frame;

    command.mask_a_le = 0xFFFFFFFFu;
    command.mask_b_le = 0;
    assert(vbrk_light_frame_build(&command, &frame) == 0);

    for (uint8_t i = 0; i < VBRK_SLOT_COUNT; i++) {
        assert_rgb(&frame.slots[i], 0x10, 0x20, 0x30);
    }
}

static void test_invalid_arguments_are_rejected(void)
{
    vbrk_light_command_t command = make_command(VBRK_LIGHT_FIND);
    vbrk_light_frame_t frame;

    assert(vbrk_light_frame_build(NULL, &frame) == -EINVAL);
    assert(vbrk_light_frame_build(&command, NULL) == -EINVAL);
}

int main(void)
{
    test_off_clears_all_slots();
    test_mask_a_sets_color_a_on_selected_slots();
    test_mask_b_overrides_mask_a_on_overlap();
    test_bits_outside_slot_count_are_ignored();
    test_invalid_arguments_are_rejected();
    return 0;
}
