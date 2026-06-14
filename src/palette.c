#include "palette.h"

static const uint32_t colors[16] = {
    0xFF14161E, 0xFF3465A4, 0xFF4E9A06, 0xFF06989A,
    0xFFCC0000, 0xFF75507B, 0xFFC4A000, 0xFFD3D7CF,
    0xFF555761, 0xFF729FCF, 0xFF8AE234, 0xFF34E2E2,
    0xFFEF2929, 0xFFAD7FA8, 0xFFFCE94F, 0xFFFFFFFF
};

uint32_t palette_rgb(uint8_t index)
{
    return colors[index & 15];
}
