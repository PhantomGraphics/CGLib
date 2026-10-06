#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace VolumeView {

// Command-text parsing for VolumeView's CommandDispatcher. Pure std (no Vulkan/ImGui/renderer
// types), so unknown / malformed commands can be checked without a GPU; the dispatcher keeps
// the execution side.

// Numeric parse of the leading number (trailing text is ignored, as before the split out of
// CommandDispatcher); false if no number can be read.
bool parseFloat(const std::string& s, float& out);
bool parseInt(const std::string& s, int& out);

// Splits on every `delim`, keeping empty fields ("a::b" -> {"a","","b"}; "" -> {""}).
std::vector<std::string> splitBy(const std::string& s, char delim);

// Parses parts[first .. first+count) as floats / ints into `out`. False if parts is too short
// or any element fails.
bool parseFloats(const std::vector<std::string>& parts, size_t first, size_t count, float* out);
bool parseInts(const std::vector<std::string>& parts, size_t first, size_t count, int* out);

// "Name:value" commands that set one PBVR parameter. Value kinds:
//   Float / Int : must parse, otherwise "Error:bad <Name> value".
//   Flag        : any text; false only for exactly "0".
enum class ScalarArg { Float, Int, Flag };

struct ScalarCommandSpec {
    const char* name;
    ScalarArg   kind;
};

struct ScalarValue {
    float f    = 0.f;
    int   i    = 0;
    bool  flag = false;
};

// All single-value PBVR setters the dispatcher routes through the table.
const std::vector<ScalarCommandSpec>& pbvrScalarCommands();

// The spec for `parts` = {name, value} (exactly two fields, name in the table), else nullptr.
const ScalarCommandSpec* findScalarCommand(const std::vector<std::string>& parts);

// Parses `text` according to spec.kind. False for an unparsable Float/Int; Flag never fails.
bool parseScalarValue(const ScalarCommandSpec& spec, const std::string& text, ScalarValue& out);

} // namespace VolumeView
