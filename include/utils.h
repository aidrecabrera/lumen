#pragma once

#include <stddef.h>
#include <stdint.h>

bool copyText(char* dest, size_t dest_len, const char* src);
bool endsWith(const char* text, const char* suffix);
uint64_t getNowMs();