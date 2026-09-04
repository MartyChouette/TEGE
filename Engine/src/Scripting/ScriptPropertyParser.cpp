#include "Enjin/Scripting/ScriptPropertyParser.h"
#include "Enjin/Logging/Log.h"
#include <regex>
#include <sstream>

namespace Enjin {
namespace Scripting {

static ECS::ScriptPropertyType TypeNameToPropertyType(const std::string& typeName) {
    if (typeName == "int" || typeName == "int32") return ECS::ScriptPropertyType::Int;
    if (typeName == "float") return ECS::ScriptPropertyType::Float;
    if (typeName == "bool") return ECS::ScriptPropertyType::Bool;
    if (typeName == "string") return ECS::ScriptPropertyType::String;
    if (typeName == "Vector2") return ECS::ScriptPropertyType::Vector2;
    if (typeName == "Vector3") return ECS::ScriptPropertyType::Vector3;
    if (typeName == "Vector4") return ECS::ScriptPropertyType::Vector4;
    if (typeName == "Entity" || typeName == "uint64") return ECS::ScriptPropertyType::Entity;
    if (typeName == "array<uint64>" || typeName == "array<Entity>")
        return ECS::ScriptPropertyType::EntityArray;
    return ECS::ScriptPropertyType::Float; // Default fallback
}

static void ParseRangeAttribute(const std::string& attrs, ParsedProperty& prop) {
    // Match Range(min, max)
    std::regex rangeRegex(R"(Range\s*\(\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*\))");
    std::smatch match;
    if (std::regex_search(attrs, match, rangeRegex)) {
        try {
            prop.hasRange = true;
            prop.rangeMin = std::stof(match[1].str());
            prop.rangeMax = std::stof(match[2].str());
        } catch (const std::exception&) {
            prop.hasRange = false;
        }
    }
}

static void ParseTooltipAttribute(const std::string& attrs, ParsedProperty& prop) {
    // Match Tooltip("text")
    std::regex tooltipRegex(R"re(Tooltip\s*\(\s*"([^"]*)"\s*\))re");
    std::smatch match;
    if (std::regex_search(attrs, match, tooltipRegex)) {
        prop.tooltip = match[1].str();
    }
}

static void ParseHeaderAttribute(const std::string& attrs, ParsedProperty& prop) {
    // Match Header("Section Name")
    std::regex headerRegex(R"re(Header\s*\(\s*"([^"]*)"\s*\))re");
    std::smatch match;
    if (std::regex_search(attrs, match, headerRegex)) {
        prop.header = match[1].str();
    }
}

static ECS::ScriptPropertyValue ParseDefaultValue(const std::string& literal, ECS::ScriptPropertyType type) {
    ECS::ScriptPropertyValue val;
    if (literal.empty()) return val;

    try {
        switch (type) {
            case ECS::ScriptPropertyType::EntityArray:
                // A list of entity references has no literal default in script
                // source; it is filled by dragging entities onto it. Listed so a
                // new property type cannot slip through here unnoticed.
                break;
            case ECS::ScriptPropertyType::Int:
            case ECS::ScriptPropertyType::Enum:
                val.intVal = std::stoi(literal);
                break;
            case ECS::ScriptPropertyType::Float: {
                std::string cleaned = literal;
                // Remove trailing 'f' suffix
                if (!cleaned.empty() && (cleaned.back() == 'f' || cleaned.back() == 'F')) {
                    cleaned.pop_back();
                }
                val.floatVal = std::stof(cleaned);
                break;
            }
            case ECS::ScriptPropertyType::Bool:
                val.boolVal = (literal == "true" || literal == "1");
                break;
            case ECS::ScriptPropertyType::String: {
                // Strip surrounding quotes
                std::string s = literal;
                if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
                    s = s.substr(1, s.size() - 2);
                }
                val.stringVal = s;
                break;
            }
            case ECS::ScriptPropertyType::Vector2:
            case ECS::ScriptPropertyType::Vector3:
            case ECS::ScriptPropertyType::Vector4:
                // Vector defaults like Vector3(1, 2, 3) — parse the numbers
                {
                    std::regex numRegex(R"(-?[\d.]+f?)");
                    auto begin = std::sregex_iterator(literal.begin(), literal.end(), numRegex);
                    auto end = std::sregex_iterator();
                    std::vector<f32> vals;
                    for (auto it = begin; it != end; ++it) {
                        std::string ns = (*it).str();
                        if (!ns.empty() && (ns.back() == 'f' || ns.back() == 'F')) ns.pop_back();
                        vals.push_back(std::stof(ns));
                    }
                    if (vals.size() >= 2) { val.vec2Val.x = vals[0]; val.vec2Val.y = vals[1]; }
                    if (vals.size() >= 3) { val.vec3Val = Math::Vector3(vals[0], vals[1], vals[2]); }
                    if (vals.size() >= 4) { val.vec4Val = Math::Vector4(vals[0], vals[1], vals[2], vals[3]); }
                }
                break;
            case ECS::ScriptPropertyType::Entity:
                if (literal != "0" && !literal.empty()) {
                    val.entityVal = std::stoull(literal);
                }
                break;
        }
    } catch (const std::exception& e) {
        ENJIN_LOG_WARN(Script, "Failed to parse default value '%s': %s", literal.c_str(), e.what());
    }

    return val;
}

std::vector<ParsedProperty> ParseProperties(const std::string& source) {
    std::vector<ParsedProperty> results;

    // Pattern matches lines like:
    //   [Property] float moveSpeed = 5.0f;
    //   [Property, Range(0, 100)] int health = 100;
    //   [Property] Vector3 position;
    // Captures: attributes, type, name, optional default value
    std::regex propRegex(
        R"(\[\s*Property\s*([^\]]*)\]\s*)"          // [Property, attrs]
        R"(([\w:]+(?:<[\w:]+>)?)\s+)"                 // type (incl. array<uint64>)
        R"((\w+))"                                    // name
        R"(\s*(?:=\s*([^;]+))?\s*;)"                  // optional = default;
    );

    auto begin = std::sregex_iterator(source.begin(), source.end(), propRegex);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        ParsedProperty prop;
        std::string attrs = (*it)[1].str();
        prop.typeName = (*it)[2].str();
        prop.name = (*it)[3].str();
        prop.defaultLiteral = (*it)[4].str();

        // Trim whitespace from default
        while (!prop.defaultLiteral.empty() && prop.defaultLiteral.back() == ' ')
            prop.defaultLiteral.pop_back();

        prop.type = TypeNameToPropertyType(prop.typeName);
        ParseRangeAttribute(attrs, prop);
        ParseTooltipAttribute(attrs, prop);
        ParseHeaderAttribute(attrs, prop);

        results.push_back(prop);
    }

    return results;
}

std::string StripPropertyAnnotations(const std::string& source) {
    // Replace [Property...] with empty string, leaving the member declaration intact
    std::regex annotationRegex(R"(\[\s*Property\s*[^\]]*\]\s*)");
    return std::regex_replace(source, annotationRegex, "");
}

ECS::ScriptProperty ToScriptProperty(const ParsedProperty& parsed) {
    ECS::ScriptProperty prop;
    prop.name = parsed.name;
    prop.type = parsed.type;
    prop.hasRange = parsed.hasRange;
    prop.rangeMin = parsed.rangeMin;
    prop.rangeMax = parsed.rangeMax;
    prop.tooltip = parsed.tooltip;
    prop.header = parsed.header;
    prop.defaultValue = ParseDefaultValue(parsed.defaultLiteral, parsed.type);
    prop.instanceValue = prop.defaultValue;
    prop.isOverridden = false;
    return prop;
}

} // namespace Scripting
} // namespace Enjin
