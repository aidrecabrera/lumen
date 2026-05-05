#include "lumen_system_utils.h"

#include <string.h>

#if defined(ESP_PLATFORM)
#include <esp_timer.h>
#else
#include <chrono>
#endif

bool copyText(char* dest, size_t dest_len, const char* src) {
    if (dest == nullptr || dest_len == 0U) {
        return false;
    }

    if (src == nullptr) {
        dest[0] = '\0';
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

uint64_t getNowMs() {
#if defined(ESP_PLATFORM)
    return static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
#else
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count()
    );
#endif
}
