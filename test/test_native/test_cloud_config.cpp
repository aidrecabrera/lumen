#include <string.h>
#include <unity.h>

#include <string>
#include <variant>

#include "lumen_cloud_config.h"

namespace {
const char* const CA = "-----BEGIN CERTIFICATE-----\\nMIIB\\n-----END CERTIFICATE-----\\n";
const char* const CERT = "-----BEGIN CERTIFICATE-----\\nMIIC\\n-----END CERTIFICATE-----\\n";
const char* const KEY = "-----BEGIN RSA PRIVATE KEY-----\\nMIIE\\n-----END RSA PRIVATE KEY-----\\n";

CloudConfig config;

CloudConfigParse parse(const std::string& json) {
    return parseCloudConfig(json.c_str(), json.size(), config);
}

std::string x509Json(const char* cert = CERT) {
    return std::string(R"({"endpoint":"abc-ats.iot.eu-west-1.amazonaws.com","port":8883,"ca_pem":")") +
           CA + R"(","auth":{"type":"x509","cert_pem":")" + cert + R"(","key_pem":")" + KEY + "\"}}";
}

std::string passwordJson(const char* extra = "") {
    return std::string(R"({"endpoint":"abc.emqxsl.com","port":8883,"ca_pem":")") + CA +
           R"(","auth":{"type":"password","username":"node-1","password":"secret"})" + extra + "}";
}

void assertParse(CloudConfigParse expected, CloudConfigParse actual) {
    TEST_ASSERT_EQUAL(static_cast<int>(expected), static_cast<int>(actual));
}
}  // namespace

void test_cloud_config_parses_x509() {
    assertParse(CloudConfigParse::OK, parse(x509Json()));
    TEST_ASSERT_EQUAL_STRING("abc-ats.iot.eu-west-1.amazonaws.com", config.endpoint);
    TEST_ASSERT_EQUAL_UINT16(8883, config.port);
    TEST_ASSERT_TRUE(std::holds_alternative<X509Auth>(config.auth));
    TEST_ASSERT_NOT_NULL(strstr(std::get<X509Auth>(config.auth).key_pem, "RSA PRIVATE KEY"));
}

void test_cloud_config_parses_password() {
    assertParse(CloudConfigParse::OK, parse(passwordJson()));
    TEST_ASSERT_EQUAL_STRING("node-1", std::get<PasswordAuth>(config.auth).username);
    TEST_ASSERT_EQUAL_STRING("secret", std::get<PasswordAuth>(config.auth).password);
}

void test_cloud_config_allows_plain_tcp_for_password_only() {
    const std::string plain =
        R"({"endpoint":"192.168.1.50","port":1883,"tls":false,"auth":{"type":"password","username":"n","password":"p"}})";
    assertParse(CloudConfigParse::OK, parse(plain));
    TEST_ASSERT_EQUAL_STRING("", config.ca_pem);

    const std::string plain_x509 = R"({"endpoint":"h","port":1883,"tls":false,"auth":{"type":"x509"}})";
    assertParse(CloudConfigParse::PLAIN_TCP_NEEDS_PASSWORD, parse(plain_x509));
}

void test_cloud_config_rejects_unknown_auth_type() {
    std::string json = passwordJson();
    json.replace(json.find("\"password\",\"username\""), 10, "\"token\"");
    assertParse(CloudConfigParse::UNKNOWN_AUTH, parse(json));
}

void test_cloud_config_rejects_missing_field() {
    assertParse(CloudConfigParse::MISSING_FIELD, parse(R"({"port":8883,"auth":{"type":"password"}})"));
    assertParse(
        CloudConfigParse::MISSING_FIELD,
        parse(R"({"endpoint":"h","port":8883,"auth":{"type":"password","username":"n","password":"p"}})")
    );    std::string out_of_range = passwordJson();
    out_of_range.replace(out_of_range.find("8883"), 4, "70000");
    assertParse(CloudConfigParse::MISSING_FIELD, parse(out_of_range));
}

void test_cloud_config_rejects_bad_pem_marker() {
    assertParse(CloudConfigParse::BAD_PEM, parse(x509Json("not a certificate")));
}

void test_cloud_config_rejects_oversized_and_bad_json() {
    const std::string huge(CLOUD_CONFIG_FILE_MAX_LEN + 1, ' ');
    assertParse(CloudConfigParse::TOO_LARGE, parse(huge));
    assertParse(CloudConfigParse::BAD_JSON, parse("{\"endpoint\":"));
}
