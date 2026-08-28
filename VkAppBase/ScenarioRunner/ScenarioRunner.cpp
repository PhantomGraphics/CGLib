#include "ScenarioRunner.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string_view>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

// ============================================================
//  Minimal JSON parser (scenario files only)
// ============================================================

namespace {

static bool tryFloat(const std::string& s, float& out) {
    const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc{};
}

// Walks up from the running executable's directory until it finds
// "Phantom2026.sln", so scenario JSON files can reference repo-relative
// paths via ${repo_root} instead of hardcoding a machine-specific absolute
// path (mirrors the same sentinel-file lookup the run_*_scenarios.ps1
// runners use).
static std::string detectRepoRoot() {
    std::filesystem::path dir;
#ifdef _WIN32
    wchar_t buf[MAX_PATH]{};
    if (::GetModuleFileNameW(nullptr, buf, MAX_PATH) > 0)
        dir = std::filesystem::path(buf).parent_path();
#endif
    if (dir.empty()) {
        std::error_code ec;
        dir = std::filesystem::current_path(ec);
    }

    std::error_code ec;
    for (std::filesystem::path cur = dir; !cur.empty(); ) {
        if (std::filesystem::exists(cur / "Phantom2026.sln", ec))
            return cur.generic_string();
        std::filesystem::path parent = cur.parent_path();
        if (parent == cur) break;
        cur = parent;
    }
    return {};
}

struct JVal {
    enum Kind { Null, Str, Bool, Arr, Obj } kind = Null;
    std::string                           str;
    bool                                  b   = false;
    std::vector<JVal>                     arr;
    std::map<std::string, JVal>           obj;

    const std::string& asStr()  const { return str; }
    bool               asBool() const { return b;   }
    const JVal* get(const std::string& key) const {
        auto it = obj.find(key);
        return (it != obj.end()) ? &it->second : nullptr;
    }
    std::string strOf(const std::string& key, const std::string& def = {}) const {
        const JVal* v = get(key);
        return (v && v->kind == Str) ? v->str : def;
    }
    int numOf(const std::string& key, int def = 0) const {
        const JVal* v = get(key);
        if (!v || v->kind != Str || v->str.empty()) return def;
        int result = 0;
        bool neg = false;
        const char* p = v->str.c_str();
        if (*p == '-') { neg = true; ++p; }
        while (*p >= '0' && *p <= '9') { result = result * 10 + (*p - '0'); ++p; }
        return neg ? -result : result;
    }
};

class JParser {
    const char* p_;
    const char* end_;
public:
    JParser(const char* data, size_t len) : p_(data), end_(data + len) {}
    JVal parse() { ws(); return value(); }

private:
    void ws() {
        while (p_ < end_ && (*p_ == ' ' || *p_ == '\t' || *p_ == '\r' || *p_ == '\n'))
            ++p_;
    }
    bool eat(char c) {
        ws();
        if (p_ < end_ && *p_ == c) { ++p_; return true; }
        return false;
    }

    std::string str() {
        if (!eat('"')) return {};
        std::string s;
        while (p_ < end_ && *p_ != '"') {
            if (*p_ == '\\') {
                ++p_;
                if (p_ < end_) {
                    char c = *p_++;
                    switch (c) {
                    case '"':  s += '"';  break;
                    case '\\': s += '\\'; break;
                    case '/':  s += '/';  break;
                    case 'n':  s += '\n'; break;
                    case 'r':  s += '\r'; break;
                    case 't':  s += '\t'; break;
                    default:   s += c;    break;
                    }
                }
            } else {
                s += *p_++;
            }
        }
        eat('"');
        return s;
    }

    JVal value() {
        ws();
        if (p_ >= end_) return {};
        if (*p_ == '{') return object();
        if (*p_ == '[') return array();
        if (*p_ == '"') { JVal v; v.kind = JVal::Str; v.str = str(); return v; }
        if (p_ + 4 <= end_ && std::string_view(p_, 4) == "true")  { JVal v; v.kind = JVal::Bool; v.b = true;  p_ += 4; return v; }
        if (p_ + 5 <= end_ && std::string_view(p_, 5) == "false") { JVal v; v.kind = JVal::Bool; v.b = false; p_ += 5; return v; }
        if (p_ + 4 <= end_ && std::string_view(p_, 4) == "null")  { p_ += 4; return {}; }
        if (*p_ == '-' || (*p_ >= '0' && *p_ <= '9')) return number();
        return {};
    }

    JVal number() {
        std::string s;
        if (p_ < end_ && *p_ == '-') s += *p_++;
        while (p_ < end_ && *p_ >= '0' && *p_ <= '9') s += *p_++;
        if (p_ < end_ && *p_ == '.') {
            s += *p_++;
            while (p_ < end_ && *p_ >= '0' && *p_ <= '9') s += *p_++;
        }
        JVal v; v.kind = JVal::Str; v.str = std::move(s);
        return v;
    }

    JVal object() {
        eat('{');
        JVal v; v.kind = JVal::Obj;
        ws();
        while (p_ < end_ && *p_ != '}') {
            ws();
            std::string key = str();
            eat(':');
            v.obj.emplace(std::move(key), value());
            ws();
            if (!eat(',')) break;
        }
        eat('}');
        return v;
    }

    JVal array() {
        eat('[');
        JVal v; v.kind = JVal::Arr;
        ws();
        while (p_ < end_ && *p_ != ']') {
            v.arr.push_back(value());
            ws();
            if (!eat(',')) break;
        }
        eat(']');
        return v;
    }
};

} // anonymous namespace

// ============================================================
//  ScenarioRunner
// ============================================================

bool ScenarioRunner::load(const std::string& jsonPath) {
    std::ifstream f(jsonPath, std::ios::binary);
    if (!f) {
        fprintf(stderr, "[Scenario] Cannot open: %s\n", jsonPath.c_str());
        return false;
    }

    std::ostringstream ss;
    ss << f.rdbuf();
    const std::string src = ss.str();

    JParser parser(src.data(), src.size());
    JVal root = parser.parse();

    const JVal* stepsNode = root.get("steps");
    if (!stepsNode || stepsNode->kind != JVal::Arr) {
        fprintf(stderr, "[Scenario] Missing 'steps' array in %s\n", jsonPath.c_str());
        return false;
    }

    steps_.clear();
    postAssert_.clear();
    current_  = 0;
    waiting_  = false;
    finished_ = false;
    failed_   = false;
    failMsg_.clear();
    vars_.clear();

    const std::string repoRoot = detectRepoRoot();
    if (!repoRoot.empty())
        vars_["repo_root"] = repoRoot;

    for (const JVal& s : stepsNode->arr) {
        if (s.kind != JVal::Obj) continue;
        Step step;
        step.label            = s.strOf("label");
        step.command          = s.strOf("command");
        step.expect           = s.strOf("expect");
        step.expectNot        = s.strOf("expect_not");
        step.expectPrefix     = s.strOf("expect_prefix");
        step.expectNotPrefix  = s.strOf("expect_not_prefix");
        step.expectRange      = s.strOf("expect_range");
        step.storeAs          = s.strOf("store_as");
        step.stripPrefix      = s.strOf("strip_prefix");
        if (!step.command.empty()) {
            const int repeat = std::max(1, s.numOf("repeat", 1));
            for (int r = 0; r < repeat; ++r)
                steps_.push_back(step);
        }
    }

    postAssert_ = root.strOf("post_assert");

    const std::string name = root.strOf("name", jsonPath);
    fprintf(stdout, "[Scenario] Loaded '%s' (%zu steps)\n", name.c_str(), steps_.size());
    return !steps_.empty();
}

// ---- variable expansion --------------------------------------------------

std::string ScenarioRunner::expandVars(const std::string& s) const {
    std::string result;
    result.reserve(s.size());
    for (size_t i = 0; i < s.size(); ) {
        if (s[i] == '$' && i + 1 < s.size() && s[i + 1] == '{') {
            size_t close = s.find('}', i + 2);
            if (close != std::string::npos) {
                const std::string key = s.substr(i + 2, close - (i + 2));
                auto it = vars_.find(key);
                result += (it != vars_.end()) ? it->second : s.substr(i, close - i + 1);
                i = close + 1;
                continue;
            }
        }
        result += s[i++];
    }
    return result;
}

// ---- response validation -------------------------------------------------

bool ScenarioRunner::checkResponse(const Step& step, const std::string& resp) {
    if (!step.expect.empty()) {
        const std::string expected = expandVars(step.expect);
        if (resp != expected) {
            failMsg_ = "expected '" + expected + "' got '" + resp + "'";
            return false;
        }
    }
    if (!step.expectNot.empty()) {
        const std::string unexpected = expandVars(step.expectNot);
        if (resp == unexpected) {
            failMsg_ = "unexpected exact value '" + unexpected + "'";
            return false;
        }
    }
    if (!step.expectPrefix.empty()) {
        if (resp.rfind(step.expectPrefix, 0) != 0) {
            failMsg_ = "expected prefix '" + step.expectPrefix + "' got '" + resp + "'";
            return false;
        }
    }
    if (!step.expectNotPrefix.empty()) {
        if (resp.rfind(step.expectNotPrefix, 0) == 0) {
            failMsg_ = "unexpected prefix '" + step.expectNotPrefix + "' in '" + resp + "'";
            return false;
        }
    }
    if (!step.expectRange.empty()) {
        auto p = step.expectRange.find(',');
        if (p == std::string::npos) {
            failMsg_ = "invalid expect_range format '" + step.expectRange + "'";
            return false;
        }

        float minV = 0.f, maxV = 0.f, actual = 0.f;
        if (!tryFloat(step.expectRange.substr(0, p), minV) ||
            !tryFloat(step.expectRange.substr(p + 1), maxV)) {
            failMsg_ = "invalid expect_range values '" + step.expectRange + "'";
            return false;
        }
        if (!tryFloat(resp, actual)) {
            failMsg_ = "response is not numeric for expect_range: '" + resp + "'";
            return false;
        }
        // NaN compares false against both < and >, so "actual < minV || actual > maxV"
        // silently passed for any NaN response (e.g. a diverged simulation) without this
        // explicit check -- exactly the case expect_range exists to catch.
        if (std::isnan(actual)) {
            failMsg_ = "response is NaN for expect_range: '" + resp + "'";
            return false;
        }
        if (actual < minV || actual > maxV) {
            failMsg_ = "expected range [" + std::to_string(minV) + "," + std::to_string(maxV) +
                       "] got " + std::to_string(actual);
            return false;
        }
    }
    return true;
}

// ---- post_assert (variable comparison after all steps pass) -------------

bool ScenarioRunner::evaluatePostAssert() {
    if (postAssert_.empty()) return true;

    struct OpSpec { const char* token; size_t len; };
    static const OpSpec ops[] = {
        {"==", 2}, {"!=", 2}, {">=", 2}, {"<=", 2}, {">", 1}, {"<", 1},
    };

    size_t opPos = std::string::npos;
    std::string opTok;
    for (const auto& o : ops) {
        const size_t pos = postAssert_.find(o.token);
        if (pos != std::string::npos && (opPos == std::string::npos || pos < opPos)) {
            opPos = pos;
            opTok = o.token;
        }
    }
    if (opPos == std::string::npos) {
        failMsg_ = "post_assert: invalid expression (expected >, <, ==, !=, >= or <=): '" + postAssert_ + "'";
        return false;
    }

    auto trim = [](const std::string& s) {
        const size_t b = s.find_first_not_of(" \t");
        if (b == std::string::npos) return std::string{};
        const size_t e = s.find_last_not_of(" \t");
        return s.substr(b, e - b + 1);
    };

    const std::string lhsTok = trim(postAssert_.substr(0, opPos));
    const std::string rhsTok = trim(postAssert_.substr(opPos + opTok.size()));

    auto resolve = [this](const std::string& tok, float& out) -> bool {
        auto it = vars_.find(tok);
        const std::string& valStr = (it != vars_.end()) ? it->second : tok;
        return tryFloat(valStr, out);
    };

    float lhs = 0.f, rhs = 0.f;
    if (!resolve(lhsTok, lhs)) {
        failMsg_ = "post_assert: cannot resolve numeric value for '" + lhsTok + "'";
        return false;
    }
    if (!resolve(rhsTok, rhs)) {
        failMsg_ = "post_assert: cannot resolve numeric value for '" + rhsTok + "'";
        return false;
    }

    bool ok = false;
    if      (opTok == "==") ok = (lhs == rhs);
    else if (opTok == "!=") ok = (lhs != rhs);
    else if (opTok == ">=") ok = (lhs >= rhs);
    else if (opTok == "<=") ok = (lhs <= rhs);
    else if (opTok == ">")  ok = (lhs > rhs);
    else if (opTok == "<")  ok = (lhs < rhs);

    if (!ok) {
        failMsg_ = "post_assert failed: '" + postAssert_ + "' (" + lhsTok + "=" + std::to_string(lhs) +
                   ", " + rhsTok + "=" + std::to_string(rhs) + ")";
    }
    return ok;
}

// ---- tick (one step per frame) ------------------------------------------

bool ScenarioRunner::tick(IScenarioDispatcher& dispatcher,
                          const std::vector<std::string>& responses)
{
    if (finished_ || steps_.empty()) return finished_;

    if (!waiting_) {
        const Step& step = steps_[current_];
        const std::string cmd = expandVars(step.command);
        fprintf(stdout, "[Scenario] [%zu/%zu] %s\n",
                current_ + 1, steps_.size(), step.label.c_str());
        fprintf(stdout, "[Scenario]   > %s\n", cmd.c_str());
        dispatcher.dispatch(cmd);
        waiting_ = true;
        return false;
    }

    if (responses.empty()) return false;

    const Step& step = steps_[current_];
    const std::string& resp = responses[0];
    fprintf(stdout, "[Scenario]   < %s\n", resp.c_str());

    if (!checkResponse(step, resp)) {
        fprintf(stderr, "[Scenario] FAIL [%zu/%zu] %s\n  %s\n",
                current_ + 1, steps_.size(), step.label.c_str(), failMsg_.c_str());
        failed_   = true;
        finished_ = true;
        return true;
    }

    if (!step.storeAs.empty()) {
        std::string val = resp;
        if (!step.stripPrefix.empty() && val.rfind(step.stripPrefix, 0) == 0)
            val = val.substr(step.stripPrefix.size());
        vars_[step.storeAs] = val;
        fprintf(stdout, "[Scenario]   stored %s = '%s'\n",
                step.storeAs.c_str(), val.c_str());
    }

    ++current_;
    waiting_ = false;

    if (current_ == steps_.size()) {
        if (!evaluatePostAssert()) {
            fprintf(stderr, "[Scenario] FAIL (post_assert)\n  %s\n", failMsg_.c_str());
            failed_ = true;
        }
        finished_ = true;
        return true;
    }
    return false;
}
