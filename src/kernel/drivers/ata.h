#pragma once
#include <stdint.h>

int ata_read(uint32_t lba, uint32_t count, void* buf);
