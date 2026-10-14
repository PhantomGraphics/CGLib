#pragma once

#include "json.hpp"

#include <string>
#include <utility>

namespace PG::CommandCore {

// Every command answers with {"ok":true,"result":...} or {"ok":false,"error":{"code":...,"message":...}}.
using Json = nlohmann::json;

namespace ErrorCode {
constexpr const char* ParseError = "parse_error";
constexpr const char* UnknownCommand = "unknown_command";
constexpr const char* InvalidArgument = "invalid_argument";
constexpr const char* NotFound = "not_found";
constexpr const char* NoTarget = "no_target";
constexpr const char* IoError = "io_error";
constexpr const char* Unsupported = "unsupported";
constexpr const char* Busy = "busy";
}

inline Json success(Json result = Json::object()) { return {{"ok", true}, {"result", std::move(result)}}; }

inline Json failure(const char* code, const std::string& message) {
    return {{"ok", false}, {"error", {{"code", code}, {"message", message}}}};
}

inline bool good(const Json& response) { return response.value("ok", false); }

} // namespace PG::CommandCore
