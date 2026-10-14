#include <chrono>
#include <cstring>
#include <thread>
#include <unity.h>

#include "lumen_system_utils.h"

void test_copyText_normal_copy() {
    char dest[8];
    TEST_ASSERT_TRUE(copyText(dest, sizeof(dest), "abc"));
    TEST_ASSERT_EQUAL_STRING("abc", dest);
}

void test_copyText_exact_fit_copy() {
    char dest[4];
    TEST_ASSERT_TRUE(copyText(dest, sizeof(dest), "abc"));
    TEST_ASSERT_EQUAL_STRING("abc", dest);
}

void test_copyText_source_too_long() {
    char dest[4] = {'x', 'x', 'x', '\0'};
    TEST_ASSERT_FALSE(copyText(dest, sizeof(dest), "abcd"));
    TEST_ASSERT_EQUAL('\0', dest[0]);
}

void test_copyText_null_source() {
    char dest[8] = "alive";
    TEST_ASSERT_FALSE(copyText(dest, sizeof(dest), nullptr));
    TEST_ASSERT_EQUAL('\0', dest[0]);
}

void test_copyText_null_dest() {
    TEST_ASSERT_FALSE(copyText(nullptr, 8, "abc"));
}

void test_copyText_zero_dest_length() {
    char dest[4] = "abc";
    TEST_ASSERT_FALSE(copyText(dest, 0, "a"));
    TEST_ASSERT_EQUAL_STRING("abc", dest);
}

void test_copyText_null_termination_on_success() {
    char dest[8];
    std::memset(dest, 'x', sizeof(dest));
    TEST_ASSERT_TRUE(copyText(dest, sizeof(dest), "abc"));
    TEST_ASSERT_EQUAL('\0', dest[3]);
    TEST_ASSERT_EQUAL_STRING("abc", dest);
}

void test_copyText_dest_cleared_on_overflow() {
    char dest[5] = "init";
    TEST_ASSERT_FALSE(copyText(dest, sizeof(dest), "12345"));
    TEST_ASSERT_EQUAL('\0', dest[0]);
}

void test_getNowMs_monotonic_non_decreasing() {
    const uint64_t t1 = getNowMs();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    const uint64_t t2 = getNowMs();
    TEST_ASSERT_TRUE(t2 >= t1);
}
void test_nextBackoffMs_doubles_below_max() {
    TEST_ASSERT_EQUAL_UINT32(2000U, nextBackoffMs(1000U, 30000U));
}

void test_nextBackoffMs_caps_at_max() {
    TEST_ASSERT_EQUAL_UINT32(30000U, nextBackoffMs(16000U, 30000U));
    TEST_ASSERT_EQUAL_UINT32(30000U, nextBackoffMs(30000U, 30000U));
    TEST_ASSERT_EQUAL_UINT32(30000U, nextBackoffMs(40000U, 30000U));
}

void test_backoffWithJitterMs_stays_in_window() {
    TEST_ASSERT_EQUAL_UINT32(1000U, backoffWithJitterMs(1000U, 0U));
    TEST_ASSERT_EQUAL_UINT32(1250U, backoffWithJitterMs(1000U, 250U));
    TEST_ASSERT_EQUAL_UINT32(1000U, backoffWithJitterMs(1000U, 251U));
}

void test_backoffWithJitterMs_no_jitter_for_tiny_backoff() {
    TEST_ASSERT_EQUAL_UINT32(3U, backoffWithJitterMs(3U, 12345U));
}
