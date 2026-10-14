#pragma once

#include <stddef.h>
#include <stdint.h>

bool copyText(char* dest, size_t dest_len, const char* src);
bool endsWith(const char* text, const char* suffix);
uint64_t getNowMs();

// Doubles the reconnect backoff, capped at max_backoff_ms.
uint32_t nextBackoffMs(uint32_t current_backoff_ms, uint32_t max_backoff_ms);

// Returns backoff_ms plus a jitter in [0, backoff_ms / 4] picked from random_value.
uint32_t backoffWithJitterMs(uint32_t backoff_ms, uint32_t random_value);
