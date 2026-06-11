#include "viberack_light_frame.h"

#include <errno.h>
#include <string.h>

static void set_rgb(vbrk_rgb_t *rgb, const uint8_t color[3])
{
    rgb->r = color[0];
    rgb->g = color[1];
    rgb->b = color[2];
}

int vbrk_light_frame_build(const vbrk_light_command_t *command,
                           vbrk_light_frame_t *frame)
{
    if (command == NULL || frame == NULL) {
        return -EINVAL;
    }

    memset(frame, 0, sizeof(*frame));
    if (command->mode == VBRK_LIGHT_OFF) {
        return 0;
    }

    for (uint8_t slot = 1; slot <= VBRK_SLOT_COUNT; slot++) {
        uint32_t mask = vbrk_slot_mask(slot);

        if ((command->mask_a_le & mask) != 0) {
            set_rgb(&frame->slots[slot - 1], command->color_a);
        }
        if ((command->mask_b_le & mask) != 0) {
            set_rgb(&frame->slots[slot - 1], command->color_b);
        }
    }

    return 0;
}

int vbrk_light_frame_copy_pixels(const vbrk_light_frame_t *frame,
                                 vbrk_rgb_t *pixels,
                                 size_t pixel_count,
                                 uint8_t *active_slots)
{
    uint8_t active = 0;

    if (frame == NULL || pixels == NULL) {
        return -EINVAL;
    }
    if (pixel_count < VBRK_SLOT_COUNT) {
        return -ENOSPC;
    }

    for (uint8_t i = 0; i < VBRK_SLOT_COUNT; i++) {
        pixels[i] = frame->slots[i];
        if (pixels[i].r != 0 || pixels[i].g != 0 || pixels[i].b != 0) {
            active++;
        }
    }

    if (active_slots != NULL) {
        *active_slots = active;
    }
    return 0;
}
