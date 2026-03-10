#include "utils.h"

#include <esp_timer.h>
#include <string.h>

bool copyText(char* dest, size_t dest_len, const char* src) {
    if (dest == nullptr || dest_len == 0U || src == nullptr) {
        return false;
    }

    const size_t src_len = strlen(src);
    if (src_len >= dest_len) {
        dest[0] = '\0';
        return false;
    }

    memcpy(dest, src, src_len);
    dest[src_len] = '\0';
    return true;
}

bool endsWith(const char* text, const char* suffix) {
    if (text == nullptr || suffix == nullptr) {
        return false;
    }

    const size_t text_len = strlen(text);
    const size_t suffix_len = strlen(suffix);

    if (text_len < suffix_len) {
        return false;
    }

    return strcmp(text + text_len - suffix_len, suffix) == 0;
}

uint64_t getNowMs() { return static_cast<uint64_t>(esp_timer_get_time() / 1000ULL); }