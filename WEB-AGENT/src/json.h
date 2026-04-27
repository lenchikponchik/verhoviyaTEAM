#pragma once

#include <cctype>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace web_agent {

class JsonValue {
public:
    using array = std::vector<JsonValue>;
    using object = std::map<std::string, JsonValue>;

    JsonValue() : value_(nullptr) {}
    JsonValue(std::nullptr_t) : value_(nullptr) {}
    JsonValue(bool value) : value_(value) {}
    JsonValue(double value) : value_(value) {}
    JsonValue(int value) : value_(static_cast<double>(value)) {}
    JsonValue(std::string value) : value_(std::move(value)) {}
    JsonValue(const char *value) : value_(std::string(value)) {}
    JsonValue(array value) : value_(std::move(value)) {}
    JsonValue(object value) : value_(std::move(value)) {}

    [[nodiscard]] bool is_null() const { return std::holds_alternative<std::nullptr_t>(value_); }
    [[nodiscard]] bool is_bool() const { return std::holds_alternative<bool>(value_); }
    [[nodiscard]] bool is_number() const { return std::holds_alternative<double>(value_); }
    [[nodiscard]] bool is_string() const { return std::holds_alternative<std::string>(value_); }
    [[nodiscard]] bool is_array() const { return std::holds_alternative<array>(value_); }
    [[nodiscard]] bool is_object() const { return std::holds_alternative<object>(value_); }

    [[nodiscard]] bool as_bool(bool fallback = false) const;
    [[nodiscard]] double as_number(double fallback = 0.0) const;
    [[nodiscard]] int as_int(int fallback = 0) const;
    [[nodiscard]] const std::string &as_string() const;
    [[nodiscard]] const array &as_array() const;
    [[nodiscard]] const object &as_object() const;
    [[nodiscard]] std::string dump() const;

    [[nodiscard]] const JsonValue *find(const std::string &key) const;

    static JsonValue parse(const std::string &text);

private:
    std::variant<std::nullptr_t, bool, double, std::string, array, object> value_;
};

} // namespace web_agent
