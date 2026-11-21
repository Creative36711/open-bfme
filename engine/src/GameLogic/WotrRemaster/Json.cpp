// OpenBFME. GPL-3.0.

#include "Json.h"

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace WotrRemaster {

namespace {

const char* skipWhitespace(const char* p, const char* end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
        ++p;
    }
    return p;
}

[[noreturn]] void failParse(const char* p, const char* start, const char* reason) {
    std::size_t offset = static_cast<std::size_t>(p - start);
    throw JsonParseError(std::string("json parse error at byte ") +
                         std::to_string(offset) + ": " + reason);
}

bool isDigit(char c) { return c >= '0' && c <= '9'; }

// Append one escape sequence to out. *p points at the character after backslash.
void appendEscape(const char** p, const char* end, std::string& out) {
    char c = **p;
    switch (c) {
        case '"': out.push_back('"'); break;
        case '\\': out.push_back('\\'); break;
        case '/': out.push_back('/'); break;
        case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        case 'u': {
            std::uint32_t cp = 0;
            for (int i = 0; i < 4; ++i) {
                ++*p;
                if (*p >= end) failParse(*p, end, "truncated unicode escape");
                char d = **p;
                cp <<= 4;
                if (d >= '0' && d <= '9') cp |= static_cast<std::uint32_t>(d - '0');
                else if (d >= 'a' && d <= 'f') cp |= static_cast<std::uint32_t>(d - 'a' + 10);
                else if (d >= 'A' && d <= 'F') cp |= static_cast<std::uint32_t>(d - 'A' + 10);
                else failParse(*p, end, "invalid hex digit in unicode escape");
            }
            ++*p;
            if (cp < 0x80u) {
                out.push_back(static_cast<char>(cp));
            } else if (cp < 0x800u) {
                out.push_back(static_cast<char>(0xc0 | (cp >> 6)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3fu)));
            } else {
                out.push_back(static_cast<char>(0xe0 | (cp >> 12)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3fu)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3fu)));
            }
            break;
        }
        default:
            failParse(*p, end, "invalid escape sequence");
    }
    ++*p;
}

JsonValue parseNumber(const char* start, const char** p, const char* end) {
    const char* q = *p;
    bool neg = false;
    if (q < end && *q == '-') { neg = true; ++q; }
    if (q >= end || !isDigit(*q)) failParse(q, start, "invalid number");
    if (*q == '0') {
        ++q;
        if (q < end && isDigit(*q)) failParse(q, start, "number with leading zero");
    } else {
        while (q < end && isDigit(*q)) ++q;
    }
    bool floating = false;
    if (q < end && (*q == '.' || *q == 'e' || *q == 'E')) {
        floating = true;
        if (*q == '.') {
            ++q;
            if (q >= end || !isDigit(*q)) failParse(q, start, "number requires digits after '.'");
            while (q < end && isDigit(*q)) ++q;
        }
        if (q < end && (*q == 'e' || *q == 'E')) {
            ++q;
            if (q < end && (*q == '+' || *q == '-')) ++q;
            if (q >= end || !isDigit(*q)) failParse(q, start, "exponent requires digits");
            while (q < end && isDigit(*q)) ++q;
        }
    }
    std::string text(start, q);
    *p = q;
    if (!floating) {
        errno = 0;
        char* endp = nullptr;
        std::strtoll(text.c_str(), &endp, 10);
        if (errno == 0 && endp == text.c_str() + text.size()) {
            JsonValue v;
            if (neg) {
                v.type = JsonValue::Type::Int;
                v.intValue = std::stoll(text);
            } else {
                errno = 0;
                endp = nullptr;
                std::uint64_t uv = std::strtoull(text.c_str(), &endp, 10);
                if (errno == 0 && endp == text.c_str() + text.size() &&
                    uv <= static_cast<std::uint64_t>(
                              std::numeric_limits<std::int64_t>::max())) {
                    v.type = JsonValue::Type::UInt;
                    v.uintValue = uv;
                } else {
                    v.type = JsonValue::Type::Double;
                    v.doubleValue = std::strtod(text.c_str(), nullptr);
                }
            }
            return v;
        }
    }
    JsonValue v;
    v.type = JsonValue::Type::Double;
    v.doubleValue = std::strtod(text.c_str(), nullptr);
    return v;
}

} // namespace

const JsonValue* JsonValue::get(const std::string& key) const {
    if (type != Type::Object) return nullptr;
    auto it = objectValue.find(key);
    return it == objectValue.end() ? nullptr : &it->second;
}

JsonValue JsonValue::parse(const std::string& text) {
    const char* p = text.data();
    const char* end = p + text.size();
    JsonValue v = parseValue(&p, end);
    p = skipWhitespace(p, end);
    if (p != end) failParse(p, end, "trailing data after json value");
    return v;
}

JsonValue JsonValue::parseValue(const char** pp, const char* end) {
    const char* p = skipWhitespace(*pp, end);
    if (p >= end) failParse(p, end, "unexpected end of json text");
    char c = *p;
    if (c == '{') {
        ++p;
        JsonValue v;
        v.type = Type::Object;
        p = skipWhitespace(p, end);
        if (p < end && *p == '}') {
            ++p;
            *pp = p;
            return v;
        }
        while (true) {
            p = skipWhitespace(p, end);
            if (p >= end || *p != '"') failParse(p, end, "expected object key");
            ++p; // opening quote
            std::string key;
            while (p < end && *p != '"') {
                if (*p == '\\') {
                    ++p;
                    if (p >= end) failParse(p, end, "unterminated string");
                    appendEscape(&p, end, key);
                } else if (static_cast<unsigned char>(*p) < 0x20) {
                    failParse(p, end, "raw control character in string");
                } else {
                    key.push_back(*p);
                    ++p;
                }
            }
            if (p >= end) failParse(p, end, "unterminated string");
            ++p; // closing quote
            p = skipWhitespace(p, end);
            if (p >= end || *p != ':') failParse(p, end, "expected ':' after object key");
            ++p;
            JsonValue value = parseValue(&p, end);
            v.objectValue.emplace(std::move(key), std::move(value));
            p = skipWhitespace(p, end);
            if (p >= end) failParse(p, end, "unterminated object");
            if (*p == '}') { ++p; *pp = p; return v; }
            if (*p != ',') failParse(p, end, "expected ',' or '}' in object");
            ++p;
        }
    }
    if (c == '[') {
        ++p;
        JsonValue v;
        v.type = Type::Array;
        p = skipWhitespace(p, end);
        if (p < end && *p == ']') { ++p; *pp = p; return v; }
        while (true) {
            JsonValue value = parseValue(&p, end);
            v.arrayValue.push_back(std::move(value));
            p = skipWhitespace(p, end);
            if (p >= end) failParse(p, end, "unterminated array");
            if (*p == ']') { ++p; *pp = p; return v; }
            if (*p != ',') failParse(p, end, "expected ',' or ']' in array");
            ++p;
        }
    }
    if (c == '"') {
        ++p;
        JsonValue v;
        v.type = Type::String;
        while (p < end && *p != '"') {
            if (*p == '\\') {
                ++p;
                if (p >= end) failParse(p, end, "unterminated string");
                appendEscape(&p, end, v.stringValue);
            } else if (static_cast<unsigned char>(*p) < 0x20) {
                failParse(p, end, "raw control character in string");
            } else {
                v.stringValue.push_back(*p);
                ++p;
            }
        }
        if (p >= end) failParse(p, end, "unterminated string");
        ++p;
        *pp = p;
        return v;
    }
    if (c == 't') {
        if (end - p >= 4 && p[1] == 'r' && p[2] == 'u' && p[3] == 'e') {
            JsonValue v;
            v.type = Type::Bool;
            v.boolean = true;
            *pp = p + 4;
            return v;
        }
        failParse(p, end, "invalid literal");
    }
    if (c == 'f') {
        if (end - p >= 5 && p[1] == 'a' && p[2] == 'l' && p[3] == 's' && p[4] == 'e') {
            JsonValue v;
            v.type = Type::Bool;
            v.boolean = false;
            *pp = p + 5;
            return v;
        }
        failParse(p, end, "invalid literal");
    }
    if (c == 'n') {
        if (end - p >= 4 && p[1] == 'u' && p[2] == 'l' && p[3] == 'l') {
            JsonValue v;
            v.type = Type::Null;
            *pp = p + 4;
            return v;
        }
        failParse(p, end, "invalid literal");
    }
    if (c == '-' || isDigit(c)) {
        JsonValue v = parseNumber(p, &p, end);
        *pp = p;
        return v;
    }
    failParse(p, end, "unexpected character in json value");
}

void JsonValue::escapeString(std::string& out, const std::string& s) {
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned>(static_cast<unsigned char>(c)));
                    out += buf;
                } else {
                    out.push_back(c);
                }
        }
    }
}

std::string JsonValue::canonical() const {
    std::string out;
    write(out, *this);
    return out;
}

void JsonValue::write(std::string& out, const JsonValue& v) {
    switch (v.type) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += v.boolean ? "true" : "false"; break;
        case Type::Int: out += std::to_string(v.intValue); break;
        case Type::UInt: out += std::to_string(v.uintValue); break;
        case Type::Double: {
            std::ostringstream os;
            os.precision(17);
            os << v.doubleValue;
            out += os.str();
            break;
        }
        case Type::String: {
            out.push_back('"');
            escapeString(out, v.stringValue);
            out.push_back('"');
            break;
        }
        case Type::Array: {
            out.push_back('[');
            for (std::size_t i = 0; i < v.arrayValue.size(); ++i) {
                if (i > 0) out.push_back(',');
                write(out, v.arrayValue[i]);
            }
            out.push_back(']');
            break;
        }
        case Type::Object: {
            out.push_back('{');
            std::size_t i = 0;
            for (const auto& kv : v.objectValue) {
                if (i > 0) out.push_back(',');
                out.push_back('"');
                escapeString(out, kv.first);
                out.push_back('"');
                out.push_back(':');
                write(out, kv.second);
                ++i;
            }
            out.push_back('}');
            break;
        }
    }
}

} // namespace WotrRemaster
