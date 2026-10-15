#pragma once

#include <stddef.h>
#include <stdint.h>

#include <variant>

static constexpr size_t CLOUD_ENDPOINT_MAX_LEN = 128U;
static constexpr size_t PEM_MAX_LEN = 2048U;
static constexpr size_t CLOUD_USERNAME_MAX_LEN = 64U;
static constexpr size_t CLOUD_PASSWORD_MAX_LEN = 128U;
static constexpr size_t CLOUD_CONFIG_FILE_MAX_LEN = 8192U;

struct X509Auth {
    char cert_pem[PEM_MAX_LEN];
    char key_pem[PEM_MAX_LEN];
};

struct PasswordAuth {
    char username[CLOUD_USERNAME_MAX_LEN];
    char password[CLOUD_PASSWORD_MAX_LEN];
};

using CloudAuth = std::variant<X509Auth, PasswordAuth>;

// an empty ca_pem means plain tcp, allowed only with PasswordAuth for lan development.
struct CloudConfig {
    char endpoint[CLOUD_ENDPOINT_MAX_LEN];
    uint16_t port;
    char ca_pem[PEM_MAX_LEN];
    CloudAuth auth;
};

enum class CloudConfigParse : uint8_t {
    OK,
    TOO_LARGE,
    BAD_JSON,
    MISSING_FIELD,
    UNKNOWN_AUTH,
    BAD_PEM,
    PLAIN_TCP_NEEDS_PASSWORD,
};

// out_config is complete only when the result is OK.
CloudConfigParse parseCloudConfig(const char* text, size_t length, CloudConfig& out_config);
