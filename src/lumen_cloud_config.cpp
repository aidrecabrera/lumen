#include "lumen_cloud_config.h"

#include <ArduinoJson.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lumen_system_utils.h"

namespace {
bool copyField(JsonVariantConst value, char* out, size_t out_len) {
    const char* text = value;
    return (text != nullptr) && (text[0] != '\0') && copyText(out, out_len, text);
}

bool isPem(const char* text, const char* label) {
    char begin[48] = {};
    char end[48] = {};
    snprintf(begin, sizeof(begin), "-----BEGIN %s-----", label);
    snprintf(end, sizeof(end), "-----END %s-----", label);
    return (strncmp(text, begin, strlen(begin)) == 0) && (strstr(text, end) != nullptr);
}

// device keys come as pkcs8, pkcs1 rsa or sec1 ec pem.
bool isPrivateKeyPem(const char* text) {
    return isPem(text, "PRIVATE KEY") || isPem(text, "RSA PRIVATE KEY") ||
           isPem(text, "EC PRIVATE KEY");
}

// filled in place: the config is ~6 kb and parsed on the 8 kb setup stack.
CloudConfigParse parseX509(JsonObjectConst auth, CloudConfig& out_config) {
    X509Auth& x509 = out_config.auth.emplace<X509Auth>();
    if (!copyField(auth["cert_pem"], x509.cert_pem, sizeof(x509.cert_pem)) ||
        !copyField(auth["key_pem"], x509.key_pem, sizeof(x509.key_pem))) {
        return CloudConfigParse::MISSING_FIELD;
    }

    if (!isPem(x509.cert_pem, "CERTIFICATE") || !isPrivateKeyPem(x509.key_pem)) {
        return CloudConfigParse::BAD_PEM;
    }

    return CloudConfigParse::OK;
}

CloudConfigParse parsePassword(JsonObjectConst auth, CloudConfig& out_config) {
    PasswordAuth& password = out_config.auth.emplace<PasswordAuth>();
    if (!copyField(auth["username"], password.username, sizeof(password.username)) ||
        !copyField(auth["password"], password.password, sizeof(password.password))) {
        return CloudConfigParse::MISSING_FIELD;
    }

    return CloudConfigParse::OK;
}
}  // namespace

CloudConfigParse parseCloudConfig(const char* text, size_t length, CloudConfig& out_config) {
    if (length > CLOUD_CONFIG_FILE_MAX_LEN) {
        return CloudConfigParse::TOO_LARGE;
    }

    JsonDocument doc;
    if (deserializeJson(doc, text, length)) {
        return CloudConfigParse::BAD_JSON;
    }

    out_config.ca_pem[0] = '\0';
    const uint32_t port = doc["port"] | 0U;
    JsonObjectConst auth = doc["auth"];
    if (!copyField(doc["endpoint"], out_config.endpoint, sizeof(out_config.endpoint)) ||
        port == 0U || port > UINT16_MAX || auth.isNull()) {
        return CloudConfigParse::MISSING_FIELD;
    }
    out_config.port = static_cast<uint16_t>(port);

    const bool plain_tcp = doc["tls"] == false;
    if (!plain_tcp) {
        if (!copyField(doc["ca_pem"], out_config.ca_pem, sizeof(out_config.ca_pem))) {
            return CloudConfigParse::MISSING_FIELD;
        }

        if (!isPem(out_config.ca_pem, "CERTIFICATE")) {
            return CloudConfigParse::BAD_PEM;
        }
    }

    const char* type = auth["type"] | "";
    if (strcmp(type, "x509") == 0) {
        return plain_tcp ? CloudConfigParse::PLAIN_TCP_NEEDS_PASSWORD : parseX509(auth, out_config);
    }

    if (strcmp(type, "password") == 0) {
        return parsePassword(auth, out_config);
    }

    return CloudConfigParse::UNKNOWN_AUTH;
}
