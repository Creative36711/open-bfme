// OpenBFME. GPL-3.0.
// WotrRemaster deterministic strategic core: canonical JSON.
// A minimal strict JSON object model whose objects use sorted keys, so the
// canonical byte sequence never depends on source property order. Numbers
// keep their kind (integer vs floating point) and integers stay exact. Used
// for pack manifests, campaign state, battle DTOs and save envelopes.

#pragma once

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace WotrRemaster {

// Thrown by JsonValue::parse on any syntax error; callers convert to error
// codes at the pack/state boundary.
struct JsonParseError : std::runtime_error {
    explicit JsonParseError(std::string what) : std::runtime_error(std::move(what)) {}
};

class JsonValue {
public:
    enum class Type { Null, Bool, Int, UInt, Double, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    std::int64_t intValue = 0;
    std::uint64_t uintValue = 0;
    double doubleValue = 0.0;
    std::string stringValue;
    std::vector<JsonValue> arrayValue;
    // std::map keeps keys sorted: canonical serialization and equality never
    // depend on the order properties appeared in the source text.
    std::map<std::string, JsonValue> objectValue;

    bool isNull() const { return type == Type::Null; }
    bool isObject() const { return type == Type::Object; }
    bool isArray() const { return type == Type::Array; }
    bool isString() const { return type == Type::String; }
    bool isNumber() const {
        return type == Type::Int || type == Type::UInt || type == Type::Double;
    }

    // Absent or not-an-object yields nullptr.
    const JsonValue* get(const std::string& key) const;

    // Strict parse. Throws JsonParseError on the first syntax error.
    static JsonValue parse(const std::string& text);

    // Deterministic serialization: sorted object keys, no whitespace,
    // integers as integers, floating point with enough digits to round-trip.
    std::string canonical() const;

private:
    static JsonValue parseValue(const char** p, const char* end);
    static void write(std::string& out, const JsonValue& v);
    static void escapeString(std::string& out, const std::string& s);
};

} // namespace WotrRemaster
