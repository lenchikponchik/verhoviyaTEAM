#include "json.h"

#include <iomanip>

namespace web_agent {

namespace {

class Parser {
public:
    explicit Parser(const std::string &text) : text_(text) {}

    JsonValue parse() {
        skip_ws();
        JsonValue value = parse_value();
        skip_ws();
        if (!eof()) {
            throw std::runtime_error("unexpected trailing JSON data");
        }
        return value;
    }

private:
    JsonValue parse_value() {
        if (eof()) {
            throw std::runtime_error("unexpected end of JSON");
        }

        const char ch = peek();
        if (ch == '{') {
            return parse_object();
        }
        if (ch == '[') {
            return parse_array();
        }
        if (ch == '"') {
            return JsonValue(parse_string());
        }
        if (ch == 't') {
            consume_literal("true");
            return JsonValue(true);
        }
        if (ch == 'f') {
            consume_literal("false");
            return JsonValue(false);
        }
        if (ch == 'n') {
            consume_literal("null");
            return JsonValue();
        }
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            return JsonValue(parse_number());
        }

        throw std::runtime_error("unsupported JSON token");
    }

    JsonValue::object parse_object() {
        expect('{');
        skip_ws();

        JsonValue::object result;
        if (match('}')) {
            return result;
        }

        while (true) {
            skip_ws();
            const std::string key = parse_string();
            skip_ws();
            expect(':');
            skip_ws();
            result.emplace(key, parse_value());
            skip_ws();
            if (match('}')) {
                break;
            }
            expect(',');
        }

        return result;
    }

    JsonValue::array parse_array() {
        expect('[');
        skip_ws();

        JsonValue::array result;
        if (match(']')) {
            return result;
        }

        while (true) {
            skip_ws();
            result.push_back(parse_value());
            skip_ws();
            if (match(']')) {
                break;
            }
            expect(',');
        }

        return result;
    }

    std::string parse_string() {
        expect('"');
        std::string out;

        while (!eof()) {
            const char ch = get();
            if (ch == '"') {
                return out;
            }
            if (ch != '\\') {
                out.push_back(ch);
                continue;
            }

            if (eof()) {
                throw std::runtime_error("invalid escape sequence");
            }

            switch (const char esc = get()) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    // The current API payloads are ASCII/UTF-8 strings; preserve raw codepoints as '?'.
                    for (int i = 0; i < 4; ++i) {
                        if (eof() || std::isxdigit(static_cast<unsigned char>(peek())) == 0) {
                            throw std::runtime_error("invalid unicode escape");
                        }
                        get();
                    }
                    out.push_back('?');
                    break;
                }
                default:
                    throw std::runtime_error("unsupported escape sequence");
            }
        }

        throw std::runtime_error("unterminated JSON string");
    }

    double parse_number() {
        const std::size_t start = pos_;
        if (match('-')) {
            // handled by match
        }
        consume_digits();
        if (match('.')) {
            consume_digits();
        }
        if (match('e') || match('E')) {
            match('+');
            match('-');
            consume_digits();
        }

        return std::stod(text_.substr(start, pos_ - start));
    }

    void consume_digits() {
        if (eof() || std::isdigit(static_cast<unsigned char>(peek())) == 0) {
            throw std::runtime_error("expected digit");
        }
        while (!eof() && std::isdigit(static_cast<unsigned char>(peek())) != 0) {
            ++pos_;
        }
    }

    void consume_literal(const char *literal) {
        while (*literal != '\0') {
            expect(*literal++);
        }
    }

    void skip_ws() {
        while (!eof() && std::isspace(static_cast<unsigned char>(peek())) != 0) {
            ++pos_;
        }
    }

    bool match(char expected) {
        if (!eof() && peek() == expected) {
            ++pos_;
            return true;
        }
        return false;
    }

    void expect(char expected) {
        if (eof() || get() != expected) {
            throw std::runtime_error("unexpected JSON character");
        }
    }

    [[nodiscard]] bool eof() const { return pos_ >= text_.size(); }
    [[nodiscard]] char peek() const { return text_[pos_]; }
    char get() { return text_[pos_++]; }

    const std::string &text_;
    std::size_t pos_ = 0;
};

std::string escape_json(const std::string &value) {
    std::ostringstream out;
    for (const char ch : value) {
        switch (ch) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(ch)) << std::dec;
                } else {
                    out << ch;
                }
        }
    }
    return out.str();
}

} // namespace

bool JsonValue::as_bool(bool fallback) const {
    return is_bool() ? std::get<bool>(value_) : fallback;
}

double JsonValue::as_number(double fallback) const {
    return is_number() ? std::get<double>(value_) : fallback;
}

int JsonValue::as_int(int fallback) const {
    return is_number() ? static_cast<int>(std::get<double>(value_)) : fallback;
}

const std::string &JsonValue::as_string() const {
    if (!is_string()) {
        throw std::runtime_error("JSON value is not a string");
    }
    return std::get<std::string>(value_);
}

const JsonValue::array &JsonValue::as_array() const {
    if (!is_array()) {
        throw std::runtime_error("JSON value is not an array");
    }
    return std::get<array>(value_);
}

const JsonValue::object &JsonValue::as_object() const {
    if (!is_object()) {
        throw std::runtime_error("JSON value is not an object");
    }
    return std::get<object>(value_);
}

const JsonValue *JsonValue::find(const std::string &key) const {
    if (!is_object()) {
        return nullptr;
    }
    const auto &obj = std::get<object>(value_);
    const auto it = obj.find(key);
    return it == obj.end() ? nullptr : &it->second;
}

std::string JsonValue::dump() const {
    if (is_null()) {
        return "null";
    }
    if (is_bool()) {
        return std::get<bool>(value_) ? "true" : "false";
    }
    if (is_number()) {
        std::ostringstream out;
        out << std::get<double>(value_);
        return out.str();
    }
    if (is_string()) {
        return "\"" + escape_json(std::get<std::string>(value_)) + "\"";
    }
    if (is_array()) {
        std::ostringstream out;
        out << "[";
        bool first = true;
        for (const auto &item : std::get<array>(value_)) {
            if (!first) {
                out << ",";
            }
            first = false;
            out << item.dump();
        }
        out << "]";
        return out.str();
    }

    std::ostringstream out;
    out << "{";
    bool first = true;
    for (const auto &item : std::get<object>(value_)) {
        if (!first) {
            out << ",";
        }
        first = false;
        out << "\"" << escape_json(item.first) << "\":" << item.second.dump();
    }
    out << "}";
    return out.str();
}

JsonValue JsonValue::parse(const std::string &text) {
    return Parser(text).parse();
}

} // namespace web_agent
