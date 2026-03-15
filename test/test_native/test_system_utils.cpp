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

void test_endsWith_exact_suffix_match() {
    TEST_ASSERT_TRUE(endsWith("lumen/status", "status"));
}

void test_endsWith_no_match() {
    TEST_ASSERT_FALSE(endsWith("lumen/status", "energy"));
}

void test_endsWith_suffix_longer_than_text() {
    TEST_ASSERT_FALSE(endsWith("abc", "abcd"));
}

void test_endsWith_empty_suffix() {
    TEST_ASSERT_TRUE(endsWith("abc", ""));
}

void test_endsWith_null_text() {
    TEST_ASSERT_FALSE(endsWith(nullptr, "abc"));
}

void test_endsWith_null_suffix() {
    TEST_ASSERT_FALSE(endsWith("abc", nullptr));
}

void test_getNowMs_monotonic_non_decreasing() {
    const uint64_t t1 = getNowMs();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    const uint64_t t2 = getNowMs();
    TEST_ASSERT_TRUE(t2 >= t1);
}