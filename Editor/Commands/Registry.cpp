#include "Registry.h"

#include "Tokenizer.h"
#include "json.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace PG::CommandCore {
using Phantom::Editor::toLower;
namespace {
using Json = nlohmann::json;

const char* typeName(ArgumentType type) {
    switch (type) {
    case ArgumentType::String: return "string";
    case ArgumentType::Integer: return "integer";
    case ArgumentType::Number: return "number";
    case ArgumentType::Boolean: return "boolean";
    case ArgumentType::Vector3: return "vector3";
    case ArgumentType::Color: return "color";
    }
    return "string";
}

Json schemaJson(const CommandSchema& command) {
    Json j = {{"name", command.name}, {"category", command.category},
        {"summary", command.summary}, {"takesTarget", command.takesTarget},
        {"mutates", command.mutates}, {"aliases", Json::array()},
        {"arguments", Json::array()}};
    std::istringstream aliases(command.aliases);
    std::string alias;
    while (std::getline(aliases, alias, ',')) if (!alias.empty()) j["aliases"].push_back(alias);
    for (const auto& arg : command.arguments) {
        Json a = {{"name", arg.name}, {"type", typeName(arg.type)}};
        if (arg.defaultValue && *arg.defaultValue) a["default"] = arg.defaultValue;
        if (arg.minValue != kNoMin) a["min"] = arg.minValue;
        if (arg.maxValue != kNoMax) a["max"] = arg.maxValue;
        j["arguments"].push_back(std::move(a));
    }
    return j;
}

bool number(const std::string& value, double& out) {
    char* end = nullptr;
    out = std::strtod(value.c_str(), &end);
    return end != value.c_str() && *end == '\0' && std::isfinite(out);
}

bool triplet(const std::string& value, double out[3]) {
    size_t start = 0;
    for (int i = 0; i < 3; ++i) {
        const size_t comma = value.find(',', start);
        if ((i < 2 && comma == std::string::npos) || (i == 2 && comma != std::string::npos)) return false;
        if (!number(value.substr(start, comma - start), out[i])) return false;
        start = comma + 1;
    }
    return true;
}

std::string formatNumber(double n) {
    std::ostringstream out;
    out << n;
    return out.str();
}
}

CommandRegistry::CommandRegistry(std::vector<CommandSchema> commands) : commands_(std::move(commands)) {}

const CommandSchema* CommandRegistry::find(const std::string& name) const {
    const auto query = toLower(name);
    for (const auto& command : commands_) {
        if (query == command.name) return &command;
        std::istringstream stream(command.aliases);
        std::string alias;
        while (std::getline(stream, alias, ',')) if (query == alias) return &command;
    }
    return nullptr;
}

std::string CommandRegistry::describe(const CommandSchema& command) { return schemaJson(command).dump(); }

std::string CommandRegistry::describeAll() const {
    Json out = Json::array();
    for (const auto& command : commands_) out.push_back(schemaJson(command));
    return out.dump();
}

std::string CommandRegistry::help(const CommandSchema* command) const {
    if (!command) {
        std::string result;
        for (const auto& entry : commands_)
            result += std::string(entry.name) + " - " + entry.summary + "\n";
        return result;
    }
    std::string result = std::string(command->name) + " - " + command->summary;
    for (const auto& arg : command->arguments)
        result += "\n  " + std::string(arg.name) + "=" + typeName(arg.type);
    return result;
}

std::string CommandRegistry::referenceMarkdown(const std::string& title) const {
    std::string result = "# " + title + "\n\n"
        "Generated from `CommandRegistry`; edit the schema and regenerate this file.\n\n";
    for (const auto& command : commands_) {
        result += "## `" + std::string(command.name) + "`\n\n";
        result += std::string(command.summary) + "\n\n";
        result += "Category: `" + std::string(command.category) + "`. ";
        result += command.takesTarget ? "Accepts object targets. " : "";
        result += command.mutates ? "Changes state.\n\n" : "Reads state or writes output.\n\n";
        if (*command.aliases) result += "Aliases: `" + std::string(command.aliases) + "`.\n\n";
        if (!command.arguments.empty()) {
            result += "| Option | Type | Default | Range |\n|---|---|---|---|\n";
            for (const auto& arg : command.arguments) {
                result += "| `" + std::string(arg.name) + "` | " + typeName(arg.type) + " | ";
                result += arg.defaultValue && *arg.defaultValue ? std::string(arg.defaultValue) : "—";
                result += " | ";
                if (arg.minValue == kNoMin && arg.maxValue == kNoMax) result += "—";
                else {
                    if (arg.minValue != kNoMin) result += formatNumber(arg.minValue);
                    result += "…";
                    if (arg.maxValue != kNoMax) result += formatNumber(arg.maxValue);
                }
                result += " |\n";
            }
            result += "\n";
        }
    }
    return result;
}

bool CommandRegistry::validateOption(const CommandSchema& command, const std::string& key,
    const std::string& value, std::string& error)
{
    const auto it = std::find_if(command.arguments.begin(), command.arguments.end(),
        [&](const ArgumentSpec& arg) { return key == arg.name; });
    if (it == command.arguments.end()) { error = "unknown option " + key; return false; }
    double n = 0.0, triple[3] = {};
    bool valid = true;
    switch (it->type) {
    case ArgumentType::String: valid = !value.empty(); break;
    case ArgumentType::Integer:
        valid = number(value, n) && std::floor(n) == n; break;
    case ArgumentType::Number: valid = number(value, n); break;
    case ArgumentType::Boolean: valid = value == "0" || value == "1" || toLower(value) == "true" || toLower(value) == "false"; break;
    case ArgumentType::Vector3: valid = number(value, n) || triplet(value, triple); break;
    case ArgumentType::Color:
        if (value.size() == 7 && value[0] == '#')
            valid = std::all_of(value.begin() + 1, value.end(), [](unsigned char c) { return std::isxdigit(c) != 0; });
        else valid = triplet(value, triple) && std::all_of(triple, triple + 3,
            [](double v) { return v >= 0.0 && v <= 1.0; });
        break;
    }
    if (valid && (it->type == ArgumentType::Integer || it->type == ArgumentType::Number))
        valid = n >= it->minValue && n <= it->maxValue;
    if (!valid) error = "invalid " + key + " (expected " + typeName(it->type) + ")";
    return valid;
}

} // namespace PG::CommandCore
