#pragma once

#include <string>
#include <vector>

namespace Phantom::Editor::CommandCore {

enum class ArgumentType { String, Integer, Number, Boolean, Vector3, Color };

constexpr double kNoMin = -1.0e100;
constexpr double kNoMax = 1.0e100;

struct ArgumentSpec {
    const char* name;
    ArgumentType type;
    const char* defaultValue;
    double minValue;
    double maxValue;
};

struct CommandSchema {
    const char* name;
    const char* aliases; // comma-separated
    const char* category;
    const char* summary;
    bool takesTarget;
    bool mutates;
    std::vector<ArgumentSpec> arguments;
};

// Declarative command schema. help / describe / reference / option validation are all
// generated from it, so every app (and the MCP server) sees the same command surface.
// Not copyable: find() hands out pointers into the stored schemas.
class CommandRegistry {
public:
    explicit CommandRegistry(std::vector<CommandSchema> commands);
    CommandRegistry(const CommandRegistry&) = delete;
    CommandRegistry& operator=(const CommandRegistry&) = delete;

    const std::vector<CommandSchema>& commands() const { return commands_; }
    const CommandSchema* find(const std::string& name) const; // name or alias, case-insensitive
    static std::string describe(const CommandSchema& command);
    std::string describeAll() const;
    std::string help(const CommandSchema* command) const;
    std::string referenceMarkdown(const std::string& title) const;
    static bool validateOption(const CommandSchema& command, const std::string& key,
        const std::string& value, std::string& error);

private:
    std::vector<CommandSchema> commands_;
};

} // namespace Phantom::Editor::CommandCore
