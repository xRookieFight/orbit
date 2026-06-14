#pragma once
#include <stdint.h>

void panic_exception(const char* message, uint64_t int_no, uint64_t err_code, uint64_t rip);
