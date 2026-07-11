#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <mutex>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <functional>
#include <filesystem>
#include <random>

#pragma comment(lib, "ws2_32.lib")
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

namespace fs = std::filesystem;

// ---------- utilities ----------
std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

std::string html_encode(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '<') r += "&lt;";
        else if (c == '>') r += "&gt;";
        else if (c == '&') r += "&amp;";
        else if (c == '"') r += "&quot;";
        else r += c;
    }
    return r;
}

std::string url_decode(const std::string& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int v;
            sscanf(s.substr(i+1,2).c_str(), "%x", &v);
            r += (char)v;
            i += 2;
        } else if (s[i] == '+') {
            r += ' ';
        } else {
            r += s[i];
        }
    }
    return r;
}

std::string current_time() {
    time_t now = time(nullptr);
    char buf[64];
    ctime_s(buf, sizeof(buf), &now);
    std::string s = buf;
    if (!s.empty() && s.back() == '\n') s.pop_back();
    return s;
}

std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool ends_with(const std::string& s, const std::string& suffix) {
    if (suffix.size() > s.size()) return false;
    return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool write_file(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << content;
    return true;
}

// ---------- JSON mini ----------
std::string json_escape(const std::string& s) {
    std::string r;
    r.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"': r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n"; break;
            case '\r': r += "\\r"; break;
            case '\t': r += "\\t"; break;
            default: r += c;
        }
    }
    return r;
}

std::string json_string(const std::string& s) {
    return "\"" + json_escape(s) + "\"";
}

class JsonValue {
public:
    enum Type { Null, String, Number, Object, Array, Bool };
    Type type = Null;
    std::string str_val;
    double num_val = 0;
    bool bool_val = false;
    std::vector<std::pair<std::string, JsonValue>> obj_val;
    std::vector<JsonValue> arr_val;

    JsonValue() : type(Null) {}
    JsonValue(const std::string& s) : type(String), str_val(s) {}
    JsonValue(const char* s) : type(String), str_val(s) {}
    JsonValue(double n) : type(Number), num_val(n) {}
    JsonValue(bool b) : type(Bool), bool_val(b) {}
    JsonValue(Type t) : type(t) {}

    std::string to_string() const {
        switch (type) {
            case Null: return "null";
            case String: return json_string(str_val);
            case Number: {
                char buf[64];
                snprintf(buf, sizeof(buf), "%g", num_val);
                return buf;
            }
            case Bool: return bool_val ? "true" : "false";
            case Object: {
                std::string r = "{";
                for (size_t i = 0; i < obj_val.size(); i++) {
                    if (i) r += ",";
                    r += json_string(obj_val[i].first) + ":" + obj_val[i].second.to_string();
                }
                return r + "}";
            }
            case Array: {
                std::string r = "[";
                for (size_t i = 0; i < arr_val.size(); i++) {
                    if (i) r += ",";
                    r += arr_val[i].to_string();
                }
                return r + "]";
            }
        }
        return "null";
    }

    const JsonValue* get(const std::string& key) const {
        if (type != Object) return nullptr;
        for (const auto& [k, v] : obj_val)
            if (k == key) return &v;
        return nullptr;
    }

    JsonValue* get_mut(const std::string& key) {
        if (type != Object) return nullptr;
        for (auto& [k, v] : obj_val)
            if (k == key) return &v;
        return nullptr;
    }

    void set(const std::string& key, const JsonValue& val) {
        if (type != Object) {
            type = Object;
            obj_val.clear();
        }
        for (auto& [k, v] : obj_val) {
            if (k == key) { v = val; return; }
        }
        obj_val.emplace_back(key, val);
    }

    void push(const JsonValue& val) {
        if (type != Array) { type = Array; arr_val.clear(); }
        arr_val.push_back(val);
    }

    std::string get_string(const std::string& key, const std::string& def = "") const {
        auto* v = get(key);
        return (v && v->type == String) ? v->str_val : def;
    }

    double get_number(const std::string& key, double def = 0) const {
        auto* v = get(key);
        return (v && v->type == Number) ? v->num_val : def;
    }

    bool get_bool(const std::string& key, bool def = false) const {
        auto* v = get(key);
        return (v && v->type == Bool) ? v->bool_val : def;
    }

    int get_int(const std::string& key, int def = 0) const {
        return (int)get_number(key, def);
    }
};

class JsonParser {
    const std::string& s;
    size_t pos = 0;

    void skip_ws() {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r'))
            pos++;
    }

    std::string parse_string() {
        if (pos >= s.size() || s[pos] != '"') return "";
        pos++; // skip opening quote
        std::string r;
        while (pos < s.size() && s[pos] != '"') {
            if (s[pos] == '\\') {
                pos++;
                if (pos >= s.size()) break;
                switch (s[pos]) {
                    case '"': r += '"'; break;
                    case '\\': r += '\\'; break;
                    case '/': r += '/'; break;
                    case 'n': r += '\n'; break;
                    case 'r': r += '\r'; break;
                    case 't': r += '\t'; break;
            case 'u': {
                    // decode \uXXXX
                    if (pos + 4 < s.size()) {
                        std::string hex = s.substr(pos+1, 4);
                        char* end;
                        long cp = strtol(hex.c_str(), &end, 16);
                        if (cp >= 0 && cp <= 127) {
                            r += (char)cp;
                        } else {
                            r += (char)cp; // best effort for ASCII-range
                        }
                        pos += 4;
                    }
                    break;
                }
                    default: r += s[pos];
                }
            } else {
                r += s[pos];
            }
            pos++;
        }
        if (pos < s.size()) pos++; // skip closing quote
        return r;
    }

    JsonValue parse_value() {
        skip_ws();
        if (pos >= s.size()) return JsonValue();
        if (s[pos] == '"') return JsonValue(parse_string());
        if (s[pos] == '{') return parse_object();
        if (s[pos] == '[') return parse_array();
        if (s[pos] == 't' && s.substr(pos, 4) == "true") { pos += 4; return JsonValue(true); }
        if (s[pos] == 'f' && s.substr(pos, 5) == "false") { pos += 5; return JsonValue(false); }
        if (s[pos] == 'n' && s.substr(pos, 4) == "null") { pos += 4; return JsonValue(JsonValue::Null); }
        // number
        size_t start = pos;
        if (s[pos] == '-') pos++;
        while (pos < s.size() && (isdigit(s[pos]) || s[pos] == '.' || s[pos] == 'e' || s[pos] == 'E' || s[pos] == '+' || s[pos] == '-'))
            pos++;
        if (pos > start) {
            return JsonValue(std::stod(s.substr(start, pos - start)));
        }
        return JsonValue();
    }

    JsonValue parse_object() {
        JsonValue obj(JsonValue::Object);
        pos++; // skip {
        skip_ws();
        if (pos < s.size() && s[pos] == '}') { pos++; return obj; }
        while (true) {
            skip_ws();
            if (pos >= s.size()) break;
            if (s[pos] != '"') break;
            std::string key = parse_string();
            skip_ws();
            if (pos < s.size() && s[pos] == ':') pos++;
            skip_ws();
            obj.set(key, parse_value());
            skip_ws();
            if (pos < s.size() && s[pos] == ',') pos++;
            else if (pos < s.size() && s[pos] == '}') { pos++; break; }
            else break;
        }
        return obj;
    }

    JsonValue parse_array() {
        JsonValue arr(JsonValue::Array);
        pos++; // skip [
        skip_ws();
        if (pos < s.size() && s[pos] == ']') { pos++; return arr; }
        while (true) {
            skip_ws();
            if (pos >= s.size()) break;
            arr.push(parse_value());
            skip_ws();
            if (pos < s.size() && s[pos] == ',') pos++;
            else if (pos < s.size() && s[pos] == ']') { pos++; break; }
            else break;
        }
        return arr;
    }

public:
    JsonParser(const std::string& str) : s(str) {}
    JsonValue parse() { return parse_value(); }
};

// ---------- Problem & Test data ----------
struct TestCase {
    std::string input;
    std::string expected;
    int subtask = 0;
};

struct Subtask {
    int id = 0;
    int points = 0;
    std::string constraints;
};

struct Sample {
    std::string input;
    std::string expected;
    std::string explanation;
};

struct Problem {
    std::string id;
    std::string title;
    std::string category;
    std::string description;
    std::string input_format;
    std::string output_format;
    std::string constraints;
    std::string difficulty;
    int cf_rating = 800;
    double time_limit = 1.0;
    int memory_limit = 256;
    std::vector<TestCase> test_cases;
    std::vector<Sample> samples;
    std::vector<Subtask> subtasks;

    static std::string rating_to_label(int r) {
        if (r < 1000) return "Easy";
        if (r < 1500) return "Medium";
        if (r < 2000) return "Hard";
        if (r < 2500) return "Hardcore";
        if (r < 3000) return "IOI-ICPC";
        return "Only AI";
    }

    JsonValue to_json() const {
        JsonValue j(JsonValue::Object);
        j.set("id", JsonValue(id));
        j.set("title", JsonValue(title));
        j.set("category", JsonValue(category));
        j.set("description", JsonValue(description));
        j.set("input_format", JsonValue(input_format));
        j.set("output_format", JsonValue(output_format));
        j.set("constraints", JsonValue(constraints));
        j.set("difficulty", JsonValue(difficulty));
        j.set("cf_rating", JsonValue((double)cf_rating));
        j.set("time_limit", JsonValue(time_limit));
        j.set("memory_limit", JsonValue((double)memory_limit));
        
        JsonValue subtasks_arr(JsonValue::Array);
        for (const auto& sub : subtasks) {
            JsonValue subj(JsonValue::Object);
            subj.set("id", JsonValue((double)sub.id));
            subj.set("points", JsonValue((double)sub.points));
            subj.set("constraints", JsonValue(sub.constraints));
            subtasks_arr.push(subj);
        }
        j.set("subtasks", subtasks_arr);

        JsonValue samples_arr(JsonValue::Array);
        for (const auto& s : samples) {
            JsonValue sj(JsonValue::Object);
            sj.set("input", JsonValue(s.input));
            sj.set("output", JsonValue(s.expected));
            sj.set("explanation", JsonValue(s.explanation));
            samples_arr.push(sj);
        }
        j.set("samples", samples_arr);

        JsonValue tc_arr(JsonValue::Array);
        for (const auto& tc : test_cases) {
            JsonValue tj(JsonValue::Object);
            tj.set("input", JsonValue(tc.input));
            tj.set("output", JsonValue(tc.expected));
            tj.set("subtask", JsonValue((double)tc.subtask));
            tc_arr.push(tj);
        }
        j.set("test_cases", tc_arr);
        return j;
    }

    static Problem from_json(const std::string& id, const JsonValue& j) {
        Problem p;
        p.id = id;
        p.title = j.get_string("title");
        p.category = j.get_string("category");
        p.description = j.get_string("description");
        p.input_format = j.get_string("input_format");
        p.output_format = j.get_string("output_format");
        p.constraints = j.get_string("constraints");
        p.difficulty = j.get_string("difficulty", "medium");
        p.cf_rating = j.get_int("cf_rating", 800);
        p.time_limit = j.get_number("time_limit", 1.0);
        p.memory_limit = j.get_int("memory_limit", 256);

        auto* subtasks = j.get("subtasks");
        if (subtasks && subtasks->type == JsonValue::Array) {
            for (const auto& subj : subtasks->arr_val) {
                Subtask sub;
                sub.id = subj.get_int("id");
                sub.points = subj.get_int("points");
                sub.constraints = subj.get_string("constraints");
                p.subtasks.push_back(sub);
            }
        }

        auto* samples = j.get("samples");
        if (samples && samples->type == JsonValue::Array) {
            for (const auto& sj : samples->arr_val) {
                Sample s;
                s.input = sj.get_string("input");
                s.expected = sj.get_string("output");
                s.explanation = sj.get_string("explanation");
                p.samples.push_back(s);
            }
        }

        auto* tcs = j.get("test_cases");
        if (tcs && tcs->type == JsonValue::Array) {
            for (const auto& tj : tcs->arr_val) {
                TestCase tc;
                tc.input = tj.get_string("input");
                tc.expected = tj.get_string("output");
                tc.subtask = tj.get_int("subtask", 0);
                p.test_cases.push_back(tc);
            }
        }
        return p;
    }
};

struct ContestProblem {
    std::string problem_id;
    int points = 0;
};

struct Contest {
    std::string id;
    std::string title;
    std::string description;
    int time_limit_minutes = 180;
    int total_points = 20;
    std::vector<ContestProblem> problems;

    JsonValue to_json() const {
        JsonValue j(JsonValue::Object);
        j.set("id", JsonValue(id));
        j.set("title", JsonValue(title));
        j.set("description", JsonValue(description));
        j.set("time_limit_minutes", JsonValue((double)time_limit_minutes));
        j.set("total_points", JsonValue((double)total_points));
        
        JsonValue probs_arr(JsonValue::Array);
        for (const auto& p : problems) {
            JsonValue pj(JsonValue::Object);
            pj.set("id", JsonValue(p.problem_id));
            pj.set("points", JsonValue((double)p.points));
            probs_arr.push(pj);
        }
        j.set("problems", probs_arr);
        return j;
    }

    static Contest from_json(const std::string& id, const JsonValue& j) {
        Contest c;
        c.id = id;
        c.title = j.get_string("title");
        c.description = j.get_string("description");
        c.time_limit_minutes = j.get_int("time_limit_minutes", 180);
        c.total_points = j.get_int("total_points", 20);

        auto* probs = j.get("problems");
        if (probs && probs->type == JsonValue::Array) {
            for (const auto& pj : probs->arr_val) {
                ContestProblem cp;
                cp.problem_id = pj.get_string("id");
                cp.points = pj.get_int("points");
                c.problems.push_back(cp);
            }
        }
        return c;
    }
};

class ContestManager {
    std::map<std::string, Contest> contests;
    std::string data_dir;
    mutable std::mutex mtx;

public:
    ContestManager(const std::string& dir) : data_dir(dir) {
        fs::create_directories(dir);
        load_all();
    }

    void load_all() {
        std::lock_guard<std::mutex> lock(mtx);
        contests.clear();
        if (!fs::exists(data_dir)) return;
        for (const auto& entry : fs::directory_iterator(data_dir)) {
            if (entry.path().extension() == ".json") {
                auto content = read_file(entry.path().string());
                if (content.empty()) continue;
                JsonParser parser(content);
                auto j = parser.parse();
                if (j.type != JsonValue::Object) continue;
                std::string id = entry.path().stem().string();
                contests[id] = Contest::from_json(id, j);
            }
        }
    }

    std::vector<Contest> list() const {
        std::lock_guard<std::mutex> lock(mtx);
        std::vector<Contest> result;
        for (const auto& [_, c] : contests) result.push_back(c);
        return result;
    }

    const Contest* get(const std::string& id) const {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = contests.find(id);
        return (it != contests.end()) ? &it->second : nullptr;
    }

    void save(const Contest& c) {
        std::lock_guard<std::mutex> lock(mtx);
        contests[c.id] = c;
        std::string path = data_dir + "/" + c.id + ".json";
        write_file(path, c.to_json().to_string());
    }
};

// ---------- Problem Manager ----------
class ProblemManager {
    std::map<std::string, Problem> problems;
    std::string data_dir;
    mutable std::mutex mtx;

public:
    ProblemManager(const std::string& dir) : data_dir(dir) {
        fs::create_directories(dir);
        load_all();
    }

    void load_all() {
        std::lock_guard<std::mutex> lock(mtx);
        problems.clear();
        for (const auto& entry : fs::directory_iterator(data_dir)) {
            if (entry.path().extension() == ".json") {
                auto content = read_file(entry.path().string());
                if (content.empty()) continue;
                JsonParser parser(content);
                auto j = parser.parse();
                if (j.type != JsonValue::Object) continue;
                std::string id = entry.path().stem().string();
                problems[id] = Problem::from_json(id, j);
            }
        }
    }

    std::vector<Problem> list() const {
        std::lock_guard<std::mutex> lock(mtx);
        std::vector<Problem> result;
        for (const auto& [_, p] : problems) result.push_back(p);
        return result;
    }

    const Problem* get(const std::string& id) const {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = problems.find(id);
        return (it != problems.end()) ? &it->second : nullptr;
    }

    void save(const Problem& p) {
        std::lock_guard<std::mutex> lock(mtx);
        problems[p.id] = p;
        std::string path = data_dir + "/" + p.id + ".json";
        write_file(path, p.to_json().to_string());
    }

    std::string generate_id() const {
        std::mt19937 rng(std::random_device{}());
        const char* chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        while (true) {
            std::string id = "prob_";
            for (int i = 0; i < 6; i++) id += chars[rng() % 36];
            if (problems.find(id) == problems.end()) return id;
        }
    }
};

// ---------- Judge Engine ----------
struct TestResult {
    int index;
    std::string status; // AC, WA, TLE, RE
    std::string input;
    std::string expected;
    std::string output;
    std::string error;
    double time = 0;
};

struct SubtaskResult {
    int id;
    int points;
    int max_points;
    int passed;
    int total;
    std::string status; // AC, WA, TLE, RE
};

struct JudgeResult {
    std::string verdict;
    int passed = 0;
    int total = 0;
    int score = 0;
    int max_score = 0;
    std::string compile_error;
    std::vector<TestResult> test_cases;
    std::map<int, SubtaskResult> subtask_results;
};

std::string exec_cmd(const std::string& cmd, int timeout_ms = 5000) {
    std::string result;
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (!pipe) return "";
    char buf[4096];
    auto start = std::chrono::steady_clock::now();
    while (fgets(buf, sizeof(buf), pipe)) {
        result += buf;
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeout_ms) {
            break;
        }
    }
    _pclose(pipe);
    return result;
}

std::string exec_cmd_with_input(const std::string& cmd, const std::string& input, int timeout_ms, double& elapsed_sec, std::string& error_msg) {
    std::string result;
    error_msg = "";
    elapsed_sec = 0;

    const DWORD PIPE_BUF = 4 * 1024 * 1024; // 4MB pipe buffer
    HANDLE hStdInRd, hStdInWr, hStdOutRd, hStdOutWr;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    if (!CreatePipe(&hStdInRd, &hStdInWr, &sa, PIPE_BUF)) { error_msg = "CreatePipe failed"; return ""; }
    if (!CreatePipe(&hStdOutRd, &hStdOutWr, &sa, PIPE_BUF)) { CloseHandle(hStdInRd); CloseHandle(hStdInWr); error_msg = "CreatePipe failed"; return ""; }

    SetHandleInformation(hStdInWr, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdOutRd, HANDLE_FLAG_INHERIT, 0);

    PROCESS_INFORMATION pi = {0};
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    si.hStdError = hStdOutWr;
    si.hStdOutput = hStdOutWr;
    si.hStdInput = hStdInRd;
    si.dwFlags |= STARTF_USESTDHANDLES;

    bool created = CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, TRUE,
                                  CREATE_NO_WINDOW, NULL, NULL, &si, &pi);

    CloseHandle(hStdOutWr);
    CloseHandle(hStdInRd);

    if (!created) {
        CloseHandle(hStdInWr);
        CloseHandle(hStdOutRd);
        error_msg = "CreateProcess failed";
        return "";
    }

    // Write input in a separate thread to prevent pipe deadlock
    HANDLE hStdInWrCopy = hStdInWr;
    std::thread writer([hStdInWrCopy, &input]() {
        const char* data = input.c_str();
        DWORD remaining = (DWORD)input.size();
        DWORD offset = 0;
        while (remaining > 0) {
            DWORD to_write = (remaining > 65536) ? 65536 : remaining;
            DWORD written = 0;
            if (!WriteFile(hStdInWrCopy, data + offset, to_write, &written, NULL)) break;
            offset += written;
            remaining -= written;
        }
        CloseHandle(hStdInWrCopy);
    });
    writer.detach();

    // Read output with timeout
    char buf[65536];
    auto start = std::chrono::steady_clock::now();
    DWORD read;
    while (true) {
        if (ReadFile(hStdOutRd, buf, sizeof(buf) - 1, &read, NULL) && read > 0) {
            buf[read] = 0;
            result += buf;
        } else {
            break;
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeout_ms) {
            error_msg = "TLE";
            TerminateProcess(pi.hProcess, 1);
            break;
        }
    }

    elapsed_sec = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - start).count();

    WaitForSingleObject(pi.hProcess, 500);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hStdOutRd);


    if (exit_code != 0 && error_msg.empty()) {
        error_msg = "RE (exit code: " + std::to_string(exit_code) + ")";
    }

    return result;
}

std::string trim_trailing(const std::string& s) {
    size_t end = s.find_last_not_of(" \t\n\r");
    if (end == std::string::npos) return "";
    return s.substr(0, end + 1);
}

bool compare_outputs(const std::string& got, const std::string& exp) {
    auto get_clean_view = [](const std::string& s) {
        size_t end = s.find_last_not_of(" \t\n\r");
        if (end == std::string::npos) return std::string("");
        std::string res;
        res.reserve(end + 1);
        for (size_t i = 0; i <= end; ++i) {
            if (s[i] != '\r') res += s[i];
        }
        return res;
    };
    return get_clean_view(got) == get_clean_view(exp);
}

static std::string g_compiler_path = "g++";

JudgeResult judge_submission(const std::string& code, const Problem& problem, const std::string& work_dir, const std::string& compiler_path = "") {
    std::string gxx = compiler_path.empty() ? g_compiler_path : compiler_path;
    JudgeResult result;
    result.total = (int)problem.test_cases.size();

    // Initialize subtasks
    if (problem.subtasks.empty()) {
        SubtaskResult sr;
        sr.id = 0;
        sr.max_points = 100;
        sr.points = 0;
        sr.passed = 0;
        sr.total = 0;
        sr.status = "AC";
        result.subtask_results[0] = sr;
        result.max_score = 100;
    } else {
        result.max_score = 0;
        for (const auto& sub : problem.subtasks) {
            SubtaskResult sr;
            sr.id = sub.id;
            sr.max_points = sub.points;
            sr.points = 0;
            sr.passed = 0;
            sr.total = 0;
            sr.status = "AC";
            result.subtask_results[sub.id] = sr;
            result.max_score += sub.points;
        }
    }

    fs::create_directories(work_dir);
    std::string src_path = work_dir + "/submission.cpp";
    std::string exe_path = work_dir + "/submission.exe";

    // Write source
    write_file(src_path, code);

    // Compile with fallback if -std=c++17 not supported (old MinGW)
    std::string compile_out;
    std::vector<std::string> std_flags = {"-std=c++17", "-std=c++11", ""};
    bool compiled = false;
    for (const auto& flag : std_flags) {
        std::string compile_cmd = gxx;
        if (!flag.empty()) compile_cmd += " " + flag;
        compile_cmd += " -O2 -o \"" + exe_path + "\" \"" + src_path + "\" 2>&1";
        compile_out = exec_cmd(compile_cmd, 15000);
        if (fs::exists(exe_path)) { compiled = true; break; }
    }

    if (!compiled) {
        result.verdict = "CE";
        // Include the attempted command and last output in error
        std::string last_cmd = gxx + " -std=c++17 -O2 -o \"" + exe_path + "\" \"" + src_path + "\"";
        result.compile_error = "Command: " + last_cmd + "\n\nError:\n" + compile_out;
        return result;
    }

    // Run test cases
    for (size_t i = 0; i < problem.test_cases.size(); i++) {
        TestResult tr;
        tr.index = (int)i + 1;
        tr.input = problem.test_cases[i].input;
        tr.expected = problem.test_cases[i].expected;

        double elapsed = 0;
        std::string error;
        std::string output;
        int timeout_ms = (int)(problem.time_limit * 1000) + 500;

        try {
            output = exec_cmd_with_input("\"" + exe_path + "\"", tr.input, timeout_ms, elapsed, error);
        } catch (...) {
            error = "RE (exception)";
        }

        tr.time = elapsed;
        tr.output = output;

        if (!error.empty() && error.find("TLE") != std::string::npos) {
            tr.status = "TLE";
        } else if (!error.empty()) {
            tr.status = "RE";
            tr.error = error;
        } else {
            if (compare_outputs(output, tr.expected)) {
                tr.status = "AC";
                result.passed++;
            } else {
                tr.status = "WA";
            }
        }

        // Map to subtask
        int sub_id = problem.test_cases[i].subtask;
        if (result.subtask_results.find(sub_id) == result.subtask_results.end()) {
            if (!result.subtask_results.empty()) {
                sub_id = result.subtask_results.begin()->first;
            } else {
                sub_id = 0;
            }
        }

        result.subtask_results[sub_id].total++;
        if (tr.status == "AC") {
            result.subtask_results[sub_id].passed++;
        } else {
            if (result.subtask_results[sub_id].status == "AC") {
                result.subtask_results[sub_id].status = tr.status;
            }
        }

        result.test_cases.push_back(tr);
    }

    // Compute subtask points
    result.score = 0;
    for (auto& [id, sub] : result.subtask_results) {
        if (sub.passed == sub.total && sub.total > 0) {
            sub.points = sub.max_points;
            sub.status = "AC";
        } else {
            sub.points = 0;
            if (sub.status == "AC") sub.status = "WA";
        }
        result.score += sub.points;
    }

    // Determine overall verdict
    if (result.passed == result.total) {
        result.verdict = "AC";
    } else {
        for (const auto& tc : result.test_cases) {
            if (tc.status != "AC") {
                result.verdict = tc.status;
                break;
            }
        }
        if (result.verdict.empty()) result.verdict = "WA";
    }

    // Save copy to archives
    std::string archive_dir = "submissions/archives";
    fs::create_directories(archive_dir);
    std::string archive_path = archive_dir + "/" + problem.id + "_" + std::to_string(time(nullptr)) + ".cpp";
    write_file(archive_path, code);

    // Cleanup temporary files
    fs::remove(src_path);
    fs::remove(exe_path);
    fs::remove_all(work_dir);

    return result;
}

// ---------- AI Client ----------

struct AIRequest {
    std::string provider;
    std::string model;
    std::string api_key;
    std::string category;
    std::string sub_type;
    int subtask_count;
    std::string rating; // "easy", "medium", "hard", "hardcore", "ioi", "only-ai", or ""
};

static std::string winhttp_post_json(const std::string& host, int port, const std::string& path,
                                      const std::string& extra_headers, const std::string& body,
                                      int timeout_ms = 120000) {
    HINTERNET hSession = WinHttpOpen(L"OJ-Agent/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      nullptr, nullptr, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, std::wstring(host.begin(), host.end()).c_str(), port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    DWORD flags = port == 443 ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
        std::wstring(path.begin(), path.end()).c_str(), nullptr, nullptr, nullptr, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    // Set timeouts
    WinHttpSetTimeouts(hRequest, timeout_ms, timeout_ms, timeout_ms, timeout_ms);

    // Set security flags for self-signed etc
    if (port == 443) {
        DWORD sec_flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE |
                          SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &sec_flags, sizeof(sec_flags));
    }

    // Headers
    std::wstring wheaders(extra_headers.begin(), extra_headers.end());
    WinHttpAddRequestHeaders(hRequest, wheaders.c_str(), wheaders.size(), WINHTTP_ADDREQ_FLAG_ADD);

    // Send
    if (!WinHttpSendRequest(hRequest, nullptr, 0, (void*)body.data(), body.size(), body.size(), 0)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return "";
    }
    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return "";
    }

    std::string result;
    char buf[8192];
    DWORD read = 0;
    while (WinHttpReadData(hRequest, buf, sizeof(buf) - 1, &read) && read > 0) {
        buf[read] = 0;
        result += buf;
        read = 0;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

static std::string winhttp_get(const std::string& host, int port, const std::string& path,
                                const std::string& extra_headers = "",
                                int timeout_ms = 30000) {
    HINTERNET hSession = WinHttpOpen(L"OJ-Agent/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      nullptr, nullptr, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, std::wstring(host.begin(), host.end()).c_str(), port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    DWORD flags = port == 443 ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET",
        std::wstring(path.begin(), path.end()).c_str(), nullptr, nullptr, nullptr, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    WinHttpSetTimeouts(hRequest, timeout_ms, timeout_ms, timeout_ms, timeout_ms);

    if (port == 443) {
        DWORD sec_flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE |
                          SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &sec_flags, sizeof(sec_flags));
    }

    if (!extra_headers.empty()) {
        std::wstring wheaders(extra_headers.begin(), extra_headers.end());
        WinHttpAddRequestHeaders(hRequest, wheaders.c_str(), wheaders.size(), WINHTTP_ADDREQ_FLAG_ADD);
    }

    if (!WinHttpSendRequest(hRequest, nullptr, 0, nullptr, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return "";
    }
    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return "";
    }

    std::string result;
    char buf[8192];
    DWORD read = 0;
    while (WinHttpReadData(hRequest, buf, sizeof(buf) - 1, &read) && read > 0) {
        buf[read] = 0;
        result += buf;
        read = 0;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

static bool sync_problems_from_github(const std::string& repo, const std::string& local_dir) {
    // repo format: "owner/repo"
    // Uses GitHub API: GET /repos/{owner}/{repo}/contents/problems
    // Then downloads each .json file via its download_url
    std::string api_path = "/repos/" + repo + "/contents/problems";
    std::string raw = winhttp_get("api.github.com", 443, api_path,
                                   "Accept: application/vnd.github.v3+json\r\n"
                                   "User-Agent: OJ-Sync/1.0\r\n", 15000);
    if (raw.empty()) {
        std::cerr << "[GitHub Sync] Failed to contact GitHub API\n";
        return false;
    }

    JsonParser parser(raw);
    auto j = parser.parse();
    if (j.type == JsonValue::Null) {
        std::cerr << "[GitHub Sync] Invalid response from GitHub API\n";
        return false;
    }
    if (j.type == JsonValue::Object && !j.get_string("message").empty()) {
        std::cerr << "[GitHub Sync] API error: " << j.get_string("message") << "\n";
        return false;
    }
    if (j.type != JsonValue::Array) {
        std::cerr << "[GitHub Sync] Unexpected response format\n";
        return false;
    }

    fs::create_directories(local_dir);
    int count = 0;
    for (const auto& item : j.arr_val) {
        std::string name = item.get_string("name");
        std::string download_url = item.get_string("download_url");
        if (name.size() < 5 || name.substr(name.size() - 5) != ".json") continue;
        if (download_url.empty()) continue;

        std::string content = winhttp_get("raw.githubusercontent.com", 443,
                                           "/" + repo + "/main/problems/" + name,
                                           "User-Agent: OJ-Sync/1.0\r\n", 10000);
        if (content.empty()) {
            std::cerr << "[GitHub Sync] Failed to download: " << name << "\n";
            continue;
        }

        std::string local_path = local_dir + "/" + name;
        write_file(local_path, content);
        count++;
        std::cout << "[GitHub Sync] Downloaded: " << name << "\n";
    }
    std::cout << "[GitHub Sync] Downloaded " << count << " problems from " << repo << "\n";
    return true;
}

static int fast_rand() {
    static unsigned int seed = (unsigned int)std::chrono::system_clock::now().time_since_epoch().count();
    seed = seed * 1103515245 + 12345;
    return (seed >> 16) & 0x7FFF;
}

static std::string build_system_prompt() {
    std::vector<std::string> intros = {
        "Bạn là người ra đề thi lập trình thi đấu (competitive programming). ",
        "Bạn là chuyên gia ra đề thi Olympic Tin học. ",
        "Bạn là giáo viên dạy lập trình, hãy tạo một bài tập lập trình. ",
        "Bạn là người tạo đề thi Codeforces, hãy ra một bài toán lập trình. "
    };
    std::vector<std::string> closings = {
        "Trả về CHỈ DUY NHẤT JSON, không kèm markdown hay giải thích gì thêm.",
        "Chỉ trả về JSON, KHÔNG có bất kỳ văn bản nào khác ngoài JSON.",
        "Tuyệt đối chỉ xuất ra JSON, không thêm bất kỳ chú thích hay giải thích nào."
    };

    // Randomize test case count requirement
    int min_tc = 5 + fast_rand() % 10;
    std::string tc_req = "test_cases phải có ít nhất " + std::to_string(min_tc) +
        " test case, bao gồm cả edge cases, test nhỏ, test ngẫu nhiên, test lớn.";

    std::string prompt =
        intros[fast_rand() % intros.size()] +
        "Hãy tạo một bài toán theo đúng định dạng JSON sau đây, KHÔNG thêm bất kỳ văn bản nào khác:\n"
        "{\n"
        "  \"title\": \"Tên bài toán bằng tiếng Việt\",\n"
        "  \"difficulty\": \"easy|medium|hard\",\n"
        "  \"cf_rating\": <số nguyên 800-3500 theo Codeforces rating>,\n"
        "  \"description\": \"Mô tả bài toán bằng tiếng Việt\",\n"
        "  \"input_format\": \"Định dạng đầu vào bằng tiếng Việt\",\n"
        "  \"output_format\": \"Định dạng đầu ra bằng tiếng Việt\",\n"
        "  \"constraints\": \"Ràng buộc tổng thể\",\n"
        "  \"subtasks\": [\n"
        "    { \"id\": 1, \"points\": <số điểm>, \"constraints\": \"Ràng buộc subtask 1\" },\n"
        "    { \"id\": 2, \"points\": <số điểm>, \"constraints\": \"Ràng buộc subtask 2\" },\n"
        "    ...\n"
        "  ],\n"
        "  \"samples\": [\n"
        "    { \"input\": \"input mẫu\\n\", \"output\": \"output mẫu\\n\", \"explanation\": \"Giải thích\" }\n"
        "  ],\n"
        "  \"test_cases\": [\n"
        "    { \"input\": \"test input\\n\", \"output\": \"test output\\n\", \"subtask\": <id subtask> },\n"
        "    ...\n"
        "  ]\n"
        "}\n\n"
        "Yêu cầu:\n"
        "1. Tổng điểm các subtask phải bằng 100.\n"
        "2. " + tc_req + "\n"
        "3. input và output trong test_cases phải dùng \\n để xuống dòng.\n"
        "4. Dữ liệu test phải đa dạng, bao phủ mọi trường hợp.\n"
        "5. cf_rating: Easy(<1000), Medium(1000-1499), Hard(1500-1999), Hardcore(2000-2499), IOI-ICPC(2500-3000), Only AI(>3000). Chọn rating phù hợp với độ khó.\n"
        "6. " + closings[fast_rand() % closings.size()];
    return prompt;
}

static std::string rating_prompt(const std::string& rating) {
    if (rating == "easy") return "Độ khó mục tiêu: Easy (CF Rating < 1000). Bài toán dành cho người mới bắt đầu.";
    if (rating == "medium") return "Độ khó mục tiêu: Medium (CF Rating 1000-1499). Bài toán cơ bản.";
    if (rating == "hard") return "Độ khó mục tiêu: Hard (CF Rating 1500-1999). Bài toán yêu cầu tư duy.";
    if (rating == "hardcore") return "Độ khó mục tiêu: Hardcore (CF Rating 2000-2499). Bài toán khó.";
    if (rating == "ioi") return "Độ khó mục tiêu: IOI-ICPC (CF Rating 2500-3000). Bài toán rất khó, cấp độ Olympic.";
    if (rating == "only-ai") return "Độ khó mục tiêu: Only AI (CF Rating > 3000). Bài toán siêu khó, chỉ AI mới giải được.";
    return "";
}

static std::string build_user_prompt(const AIRequest& req) {
    std::string rating_info = rating_prompt(req.rating);
    std::string rating_line = rating_info.empty() ? "" : "\n" + rating_info + "\n";

    std::vector<std::string> templates = {
        "Hãy tạo một bài toán lập trình với các thông tin sau:\n"
        "- Thể loại: %s\n"
        "- Dạng cụ thể: %s\n"
        "- Số subtask: %d\n%s"
        "\nHãy tạo một bài toán phù hợp với thể loại và dạng đã chọn. "
        "Bài toán nên có độ khó phù hợp, test cases đầy đủ và chính xác. "
        "Các test case phải có input và output đúng, bao gồm các trường hợp biên.",

        "Tạo bài tập lập trình chủ đề \"%s\" - dạng \"%s\" với %d subtask.%s\n"
        "Bài toán cần có mô tả rõ ràng, input/output cụ thể. "
        "Test case phải bao phủ đủ các trường hợp đặc biệt và giới hạn.",

        "Yêu cầu: Tạo một bài toán thuộc thể loại \"%s\", kiểu \"%s\".\n"
        "Số subtask: %d.%s\n"
        "Viết mô tả, input/output, ràng buộc, và test cases chi tiết. "
        "Chú ý các edge cases và test ngẫu nhiên kích thước lớn.",

        "Hãy ra đề:\n"
        "  - Thể loại: %s\n"
        "  - Dạng bài: %s\n"
        "  - Số subtask: %d%s\n\n"
        "Soạn thảo bài toán hoàn chỉnh với test cases đầy đủ, dữ liệu chính xác."
    };

    char buf[2048];
    int idx = fast_rand() % (int)templates.size();
    snprintf(buf, sizeof(buf), templates[idx].c_str(),
             req.category.c_str(), req.sub_type.c_str(), req.subtask_count, rating_line.c_str());
    return std::string(buf);
}

static std::string extract_json(const std::string& response) {
    // Try to find JSON between ```json ... ```
    size_t start_marker = response.find("```json");
    if (start_marker != std::string::npos) {
        start_marker += 7;
        size_t end_marker = response.find("```", start_marker);
        if (end_marker != std::string::npos) {
            return response.substr(start_marker, end_marker - start_marker);
        }
    }
    // Try ``` ... ```
    start_marker = response.find("```");
    if (start_marker != std::string::npos) {
        start_marker += 3;
        // Skip language identifier if any
        size_t nl = response.find('\n', start_marker);
        if (nl != std::string::npos && nl - start_marker < 20) start_marker = nl + 1;
        size_t end_marker = response.find("```", start_marker);
        if (end_marker != std::string::npos) {
            return response.substr(start_marker, end_marker - start_marker);
        }
    }
    // Fallback: find first { and last }
    size_t first = response.find('{');
    size_t last = response.rfind('}');
    if (first != std::string::npos && last != std::string::npos && last > first) {
        return response.substr(first, last - first + 1);
    }
    return response;
}

static std::string call_openai_like(const std::string& host, const std::string& path,
                                     const std::string& api_key, const std::string& model,
                                     const std::string& system_prompt, const std::string& user_prompt,
                                     const std::string& auth_header_prefix = "Bearer ") {
    JsonValue body(JsonValue::Object);
    body.set("model", JsonValue(model));

    JsonValue msgs(JsonValue::Array);
    JsonValue sys(JsonValue::Object);
    sys.set("role", JsonValue("system"));
    sys.set("content", JsonValue(system_prompt));
    msgs.push(sys);

    JsonValue usr(JsonValue::Object);
    usr.set("role", JsonValue("user"));
    usr.set("content", JsonValue(user_prompt));
    msgs.push(usr);

    body.set("messages", msgs);
    double temp = 0.5 + (fast_rand() % 50) / 100.0;
    body.set("temperature", JsonValue(temp));
    body.set("max_tokens", JsonValue(4000.0));

    std::string json_body = body.to_string();
    std::string headers = "Content-Type: application/json\r\nAuthorization: " + auth_header_prefix + api_key;
    std::string raw = winhttp_post_json(host, 443, path, headers, json_body);

    if (raw.empty()) return "";

    JsonParser parser(raw);
    auto resp = parser.parse();
    auto* choices = resp.get("choices");
    if (choices && choices->type == JsonValue::Array && !choices->arr_val.empty()) {
        auto* msg = choices->arr_val[0].get("message");
        if (msg) {
            return msg->get_string("content");
        }
    }
    auto* err = resp.get("error");
    if (err) {
        return "__ERROR__:" + err->get_string("message");
    }
    return "";
}

static std::string call_openai(const std::string& api_key, const std::string& model,
                                const std::string& system_prompt, const std::string& user_prompt) {
    return call_openai_like("api.openai.com", "/v1/chat/completions",
                            api_key, model, system_prompt, user_prompt);
}

static std::string call_deepseek(const std::string& api_key, const std::string& model,
                                  const std::string& system_prompt, const std::string& user_prompt) {
    return call_openai_like("api.deepseek.com", "/v1/chat/completions",
                            api_key, model, system_prompt, user_prompt);
}

static std::string call_claude(const std::string& api_key, const std::string& model,
                                const std::string& system_prompt, const std::string& user_prompt) {
    JsonValue body(JsonValue::Object);
    body.set("model", JsonValue(model));
    body.set("max_tokens", JsonValue(4000.0));

    JsonValue msgs(JsonValue::Array);
    JsonValue usr(JsonValue::Object);
    usr.set("role", JsonValue("user"));
    usr.set("content", JsonValue(user_prompt));
    msgs.push(usr);
    body.set("messages", msgs);

    body.set("system", JsonValue(system_prompt));

    std::string json_body = body.to_string();
    std::string headers = "Content-Type: application/json\r\nx-api-key: " + api_key +
                          "\r\nanthropic-version: 2023-06-01";
    std::string raw = winhttp_post_json("api.anthropic.com", 443, "/v1/messages", headers, json_body);

    if (raw.empty()) return "";

    JsonParser parser(raw);
    auto resp = parser.parse();
    auto* content = resp.get("content");
    if (content && content->type == JsonValue::Array && !content->arr_val.empty()) {
        return content->arr_val[0].get_string("text");
    }
    auto* err = resp.get("error");
    if (err) {
        return "__ERROR__:" + err->get_string("message");
    }
    return "";
}

static std::string call_gemini(const std::string& api_key, const std::string& model,
                                const std::string& system_prompt, const std::string& user_prompt) {
    JsonValue body(JsonValue::Object);

    // system instruction
    JsonValue sys_inst(JsonValue::Object);
    JsonValue sys_parts(JsonValue::Array);
    JsonValue sys_text(JsonValue::Object);
    sys_text.set("text", JsonValue(system_prompt));
    sys_parts.push(sys_text);
    sys_inst.set("parts", sys_parts);
    body.set("system_instruction", sys_inst);

    // user content
    JsonValue contents(JsonValue::Array);
    JsonValue content_obj(JsonValue::Object);
    JsonValue parts(JsonValue::Array);
    JsonValue text_part(JsonValue::Object);
    text_part.set("text", JsonValue(user_prompt));
    parts.push(text_part);
    content_obj.set("parts", parts);
    contents.push(content_obj);
    body.set("contents", contents);

    // generation config
    JsonValue gen_config(JsonValue::Object);
    double temp = 0.5 + (fast_rand() % 50) / 100.0;
    gen_config.set("temperature", JsonValue(temp));
    gen_config.set("maxOutputTokens", JsonValue(4000.0));
    body.set("generationConfig", gen_config);

    std::string json_body = body.to_string();
    std::string path = "/v1beta/models/" + model + ":generateContent?key=" + api_key;
    std::string headers = "Content-Type: application/json";
    std::string raw = winhttp_post_json("generativelanguage.googleapis.com", 443, path, headers, json_body);

    if (raw.empty()) return "";

    JsonParser parser(raw);
    auto resp = parser.parse();
    auto* candidates = resp.get("candidates");
    if (candidates && candidates->type == JsonValue::Array && !candidates->arr_val.empty()) {
        auto* content = candidates->arr_val[0].get("content");
        if (content) {
            auto* parts = content->get("parts");
            if (parts && parts->type == JsonValue::Array && !parts->arr_val.empty()) {
                return parts->arr_val[0].get_string("text");
            }
        }
    }
    auto* err = resp.get("error");
    if (err) {
        return "__ERROR__:" + err->get_string("message");
    }
    return "";
}

static std::string default_model_for(const std::string& provider) {
    if (provider == "claude") return "claude-sonnet-4-6";
    if (provider == "gemini") return "gemini-3.5-flash";
    if (provider == "deepseek") return "deepseek-chat";
    return "gpt-4o";
}

static Problem generate_problem_ai(const AIRequest& req, std::string& error) {
    Problem prob;
    std::string system_prompt = build_system_prompt();
    std::string user_prompt = build_user_prompt(req);
    std::string raw_response;
    std::string model = req.model.empty() ? default_model_for(req.provider) : req.model;

    if (req.provider == "claude") {
        raw_response = call_claude(req.api_key, model, system_prompt, user_prompt);
    } else if (req.provider == "gemini") {
        raw_response = call_gemini(req.api_key, model, system_prompt, user_prompt);
    } else if (req.provider == "deepseek") {
        raw_response = call_deepseek(req.api_key, model, system_prompt, user_prompt);
    } else {
        raw_response = call_openai(req.api_key, model, system_prompt, user_prompt);
    }

    if (raw_response.empty()) {
        error = "Không nhận được phản hồi từ AI. Vui lòng kiểm tra kết nối mạng.";
        return prob;
    }
    if (raw_response.find("__ERROR__:") == 0) {
        error = "Lỗi AI: " + raw_response.substr(10);
        return prob;
    }

    std::string json_str = extract_json(raw_response);
    if (json_str.empty()) {
        error = "AI không trả về JSON hợp lệ.";
        return prob;
    }

    JsonParser parser(json_str);
    auto j = parser.parse();
    if (j.type == JsonValue::Null) {
        error = "Không thể phân tích JSON từ phản hồi của AI.";
        return prob;
    }

    try {
        prob.title = j.get_string("title", "Untitled");
        prob.category = req.category;
        prob.description = j.get_string("description", "");
        prob.input_format = j.get_string("input_format", "");
        prob.output_format = j.get_string("output_format", "");
        prob.constraints = j.get_string("constraints", "");
        prob.difficulty = j.get_string("difficulty", "medium");
        prob.cf_rating = j.get_int("cf_rating", 800);
        prob.time_limit = j.get_number("time_limit", 1.0);
        prob.memory_limit = j.get_int("memory_limit", 256);

        // Subtasks
        auto* subtasks = j.get("subtasks");
        if (subtasks && subtasks->type == JsonValue::Array) {
            for (const auto& subj : subtasks->arr_val) {
                Subtask sub;
                sub.id = subj.get_int("id");
                sub.points = subj.get_int("points");
                sub.constraints = subj.get_string("constraints");
                prob.subtasks.push_back(sub);
            }
        }

        // Samples
        auto* samples = j.get("samples");
        if (samples && samples->type == JsonValue::Array) {
            for (const auto& sj : samples->arr_val) {
                Sample s;
                s.input = sj.get_string("input");
                s.expected = sj.get_string("output");
                s.explanation = sj.get_string("explanation", "");
                prob.samples.push_back(s);
            }
        }

        // Test cases
        auto* tcs = j.get("test_cases");
        if (tcs && tcs->type == JsonValue::Array) {
            for (const auto& tj : tcs->arr_val) {
                TestCase tc;
                tc.input = tj.get_string("input");
                tc.expected = tj.get_string("output");
                tc.subtask = tj.get_int("subtask", 0);
                prob.test_cases.push_back(tc);
            }
        }

        if (prob.description.empty()) {
            error = "Thiếu trường description trong phản hồi AI.";
            return prob;
        }
        if (prob.test_cases.empty()) {
            error = "Không có test case nào được tạo.";
            return prob;
        }

        // Generate ID
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        std::string safe_cat = req.category;
        for (auto& c : safe_cat) if (c == ' ') c = '_';
        prob.id = "ai_" + safe_cat + "_" + std::to_string(ms % 100000000);

    } catch (const std::exception& e) {
        error = std::string("Lỗi xử lý JSON: ") + e.what();
        return prob;
    }

    return prob;
}

// ---------- Solution Help ----------

static std::string build_solution_hint_prompt(const Problem& prob) {
    std::string prompt =
        "Bạn là người hướng dẫn lập trình. Dựa vào đề bài sau, hãy giải thích cách giải bài toán. "
        "Trả về CHỈ DUY NHẤT JSON, không kèm markdown hay giải thích gì thêm.\n\n"
        "Đề bài:\n"
        "Tiêu đề: " + prob.title + "\n"
        "Mô tả: " + prob.description + "\n"
        "Định dạng đầu vào: " + prob.input_format + "\n"
        "Định dạng đầu ra: " + prob.output_format + "\n"
        "Ràng buộc: " + prob.constraints + "\n\n";

    if (!prob.subtasks.empty()) {
        prompt += "Subtasks:\n";
        for (const auto& sub : prob.subtasks) {
            prompt += "  Subtask " + std::to_string(sub.id) + " (" + std::to_string(sub.points) + " điểm): "
                      + sub.constraints + "\n";
        }
    }

    prompt += "\nVí dụ:\n";
    for (const auto& s : prob.samples) {
        prompt += "  Input: " + s.input + "\n  Output: " + s.expected + "\n";
    }

    prompt +=
        "\nHãy trả về JSON theo định dạng:\n"
        "{\n"
        "  \"approach\": \"Giải thích thuật toán tổng quan, tư duy giải bài\",\n"
        "  \"algorithm\": \"Tên thuật toán cụ thể cần dùng\",\n"
        "  \"complexity\": \"Độ phức tạp thời gian và bộ nhớ\",\n"
        "  \"steps\": [\"Bước 1\", \"Bước 2\", ...],\n"
        "  \"key_insight\": \"Điểm mấu chốt để giải bài\"\n"
        "}\n\n"
        "Sử dụng $...$ cho công thức toán học. QUAN TRỌNG: Chỉ trả về JSON, không thêm gì khác.";
    return prompt;
}

static std::string build_solution_code_prompt(const Problem& prob) {
    std::string prompt =
        "Bạn là lập trình viên C++ thi đấu. Dựa vào đề bài sau, hãy viết code C++ giải bài toán. "
        "Code cần tối ưu, xử lý đúng mọi ràng buộc và subtask.\n"
        "Trả về CHỈ DUY NHẤT JSON, không kèm markdown hay giải thích gì thêm.\n\n"
        "Đề bài:\n"
        "Tiêu đề: " + prob.title + "\n"
        "Mô tả: " + prob.description + "\n"
        "Định dạng đầu vào: " + prob.input_format + "\n"
        "Định dạng đầu ra: " + prob.output_format + "\n"
        "Ràng buộc: " + prob.constraints + "\n";

    if (!prob.subtasks.empty()) {
        prompt += "Subtasks:\n";
        for (const auto& sub : prob.subtasks) {
            prompt += "  Subtask " + std::to_string(sub.id) + " (" + std::to_string(sub.points) + " điểm): "
                      + sub.constraints + "\n";
        }
    }

    prompt += "\nVí dụ:\n";
    for (const auto& s : prob.samples) {
        prompt += "  Input: " + s.input + "\n  Output: " + s.expected + "\n";
    }

    prompt +=
        "\nHãy trả về JSON theo định dạng:\n"
        "{\n"
        "  \"language\": \"C++\",\n"
        "  \"std\": \"C++17\",\n"
        "  \"code\": \"Code C++ hoàn chỉnh, dùng #include <bits/stdc++.h> và using namespace std;\",\n"
        "  \"explanation\": \"Giải thích ngắn code hoạt động thế nào\",\n"
        "  \"complexity\": \"Độ phức tạp\"\n"
        "}\n\n"
        "Sử dụng $...$ cho công thức toán học. QUAN TRỌNG: Chỉ trả về JSON, không thêm gì khác. "
        "Code phải đọc từ stdin và in ra stdout.";
    return prompt;
}

static std::string call_ai_provider(const std::string& provider, const std::string& api_key,
                                     const std::string& model, const std::string& system_prompt,
                                     const std::string& user_prompt) {
    std::string m = model.empty() ? default_model_for(provider) : model;
    if (provider == "claude") return call_claude(api_key, m, system_prompt, user_prompt);
    if (provider == "gemini") return call_gemini(api_key, m, system_prompt, user_prompt);
    if (provider == "deepseek") return call_deepseek(api_key, m, system_prompt, user_prompt);
    return call_openai(api_key, m, system_prompt, user_prompt);
}

static std::string build_chat_system_prompt(const Problem* problem) {
    std::string prompt = "Bạn là trợ lý AI hỗ trợ giải bài tập lập trình. ";
    if (problem) {
        prompt += "Dưới đây là đề bài đang được hỏi:\n\n"
                  "Tiêu đề: " + problem->title + "\n"
                  "Mô tả: " + problem->description + "\n"
                  "Định dạng đầu vào: " + problem->input_format + "\n"
                  "Định dạng đầu ra: " + problem->output_format + "\n"
                  "Ràng buộc: " + problem->constraints + "\n\n";
        if (!problem->subtasks.empty()) {
            prompt += "Subtasks:\n";
            for (const auto& sub : problem->subtasks) {
                prompt += "  Subtask " + std::to_string(sub.id) + " (" + std::to_string(sub.points) + " điểm): "
                          + sub.constraints + "\n";
            }
        }
        prompt += "\nVí dụ:\n";
        for (const auto& s : problem->samples) {
            prompt += "  Input: " + s.input + "\n  Output: " + s.expected + "\n";
        }
    }
    prompt += "\nTrả lời câu hỏi của người dùng về bài tập này. Sử dụng $...$ cho công thức toán học. "
              "Giải thích rõ ràng, dễ hiểu. Nếu được hỏi về code, hãy đưa ra code C++.";
    return prompt;
}

// ---------- HTTP Server ----------
class HttpServer {
    SOCKET listen_socket;
    int port;
    std::string static_dir;
    ProblemManager& problem_mgr;
    ContestManager& contest_mgr;
    bool running = false;

    struct Request {
        std::string method;
        std::string path;
        std::map<std::string, std::string> headers;
        std::string body;
    };

    struct Response {
        int status = 200;
        std::string status_text = "OK";
        std::string content_type = "text/plain";
        std::string body;
        std::map<std::string, std::string> extra_headers;

        std::string to_string() const {
            std::ostringstream ss;
            ss << "HTTP/1.1 " << status << " " << status_text << "\r\n";
            ss << "Content-Type: " << content_type << "\r\n";
            ss << "Content-Length: " << body.size() << "\r\n";
            ss << "Access-Control-Allow-Origin: *\r\n";
            ss << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
            ss << "Access-Control-Allow-Headers: Content-Type\r\n";
            ss << "Connection: close\r\n";
            for (const auto& [k, v] : extra_headers) {
                ss << k << ": " << v << "\r\n";
            }
            ss << "\r\n";
            ss << body;
            return ss.str();
        }
    };

    std::string mime_type(const std::string& path) const {
        if (ends_with(path, ".html")) return "text/html; charset=utf-8";
        if (ends_with(path, ".css")) return "text/css; charset=utf-8";
        if (ends_with(path, ".js")) return "application/javascript; charset=utf-8";
        if (ends_with(path, ".png")) return "image/png";
        if (ends_with(path, ".jpg") || ends_with(path, ".jpeg")) return "image/jpeg";
        if (ends_with(path, ".svg")) return "image/svg+xml";
        if (ends_with(path, ".ico")) return "image/x-icon";
        return "text/plain; charset=utf-8";
    }

    Request parse_request(const std::string& raw) const {
        Request req;
        std::istringstream ss(raw);
        std::string line;

        // Request line
        std::getline(ss, line);
        line = trim(line);
        auto space1 = line.find(' ');
        if (space1 != std::string::npos) {
            req.method = line.substr(0, space1);
            auto space2 = line.find(' ', space1 + 1);
            if (space2 != std::string::npos) {
                req.path = line.substr(space1 + 1, space2 - space1 - 1);
            }
        }

        // Headers
        while (std::getline(ss, line)) {
            line = trim(line);
            if (line.empty()) break;
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = trim(line.substr(0, colon));
                std::string val = trim(line.substr(colon + 1));
                for (auto& c : key) c = tolower(c);
                req.headers[key] = val;
            }
        }

        // Body
        std::string remaining;
        while (std::getline(ss, line)) {
            remaining += line + "\n";
        }
        req.body = trim(remaining);

        // Try to get proper body from content-length
        auto it = req.headers.find("content-length");
        if (it != req.headers.end()) {
            int len = std::stoi(it->second);
            if ((int)req.body.size() > len) {
                req.body = req.body.substr(0, len);
            }
        }

        return req;
    }

    std::string url_path_decode(const std::string& path) const {
        // Get path without query string and decode
        size_t q = path.find('?');
        std::string p = (q == std::string::npos) ? path : path.substr(0, q);
        return url_decode(p);
    }

    Response serve_static(const std::string& path) const {
        Response resp;
        std::string file_path = static_dir + path;
        if (path == "/" || path == "") file_path = static_dir + "/index.html";

        std::string content = read_file(file_path);
        if (content.empty()) {
            resp.status = 404;
            resp.status_text = "Not Found";
            resp.content_type = "text/plain; charset=utf-8";
            resp.body = "404 Not Found";
        } else {
            resp.content_type = mime_type(file_path);
            resp.body = content;
        }
        return resp;
    }

    Response handle_api(const Request& req) const {
        Response resp;
        resp.content_type = "application/json; charset=utf-8";

        std::string path = req.path;

        // CORS preflight
        if (req.method == "OPTIONS") {
            resp.status = 204;
            resp.status_text = "No Content";
            return resp;
        }

        // GET /api/problems
        if (path == "/api/problems" && req.method == "GET") {
            auto probs = problem_mgr.list();
            JsonValue arr(JsonValue::Array);
            for (const auto& p : probs) {
                JsonValue j(JsonValue::Object);
                j.set("id", JsonValue(p.id));
                j.set("title", JsonValue(p.title));
                j.set("difficulty", JsonValue(p.difficulty));
                j.set("cf_rating", JsonValue((double)p.cf_rating));
                j.set("category", JsonValue(p.category));
                j.set("test_count", JsonValue((double)p.test_cases.size()));
                j.set("subtask_count", JsonValue((double)p.subtasks.size()));
                arr.push(j);
            }
            resp.body = arr.to_string();
            return resp;
        }

        // GET /api/problem/{id}
        if (path.find("/api/problem/") == 0 && req.method == "GET") {
            std::string id = path.substr(13);
            id = url_decode(id);
            const Problem* p = problem_mgr.get(id);
            if (!p) {
                resp.status = 404;
                resp.status_text = "Not Found";
                JsonValue err(JsonValue::Object);
                err.set("error", JsonValue("Problem not found"));
                resp.body = err.to_string();
            } else {
                resp.body = p->to_json().to_string();
            }
            return resp;
        }

        // GET /api/contests
        if (path == "/api/contests" && req.method == "GET") {
            auto contests = contest_mgr.list();
            JsonValue arr(JsonValue::Array);
            for (const auto& c : contests) {
                JsonValue j(JsonValue::Object);
                j.set("id", JsonValue(c.id));
                j.set("title", JsonValue(c.title));
                j.set("description", JsonValue(c.description));
                j.set("time_limit_minutes", JsonValue((double)c.time_limit_minutes));
                j.set("total_points", JsonValue((double)c.total_points));
                j.set("problem_count", JsonValue((double)c.problems.size()));
                arr.push(j);
            }
            resp.body = arr.to_string();
            return resp;
        }

        // GET /api/contest/{id}
        if (path.find("/api/contest/") == 0 && req.method == "GET") {
            std::string id = path.substr(13);
            id = url_decode(id);
            const Contest* c = contest_mgr.get(id);
            if (!c) {
                resp.status = 404;
                resp.status_text = "Not Found";
                JsonValue err(JsonValue::Object);
                err.set("error", JsonValue("Contest not found"));
                resp.body = err.to_string();
            } else {
                JsonValue cj(JsonValue::Object);
                cj.set("id", JsonValue(c->id));
                cj.set("title", JsonValue(c->title));
                cj.set("description", JsonValue(c->description));
                cj.set("time_limit_minutes", JsonValue((double)c->time_limit_minutes));
                cj.set("total_points", JsonValue((double)c->total_points));
                
                JsonValue probs_arr(JsonValue::Array);
                for (const auto& cp : c->problems) {
                    JsonValue pj(JsonValue::Object);
                    pj.set("id", JsonValue(cp.problem_id));
                    pj.set("points", JsonValue((double)cp.points));
                    
                    const Problem* p = problem_mgr.get(cp.problem_id);
                    if (p) {
                        pj.set("title", JsonValue(p->title));
                        pj.set("difficulty", JsonValue(p->difficulty));
                        pj.set("cf_rating", JsonValue((double)p->cf_rating));
                        pj.set("time_limit", JsonValue(p->time_limit));
                        pj.set("memory_limit", JsonValue((double)p->memory_limit));
                        pj.set("description", JsonValue(p->description));
                        pj.set("input_format", JsonValue(p->input_format));
                        pj.set("output_format", JsonValue(p->output_format));
                        pj.set("constraints", JsonValue(p->constraints));
                        
                        JsonValue subtasks_arr(JsonValue::Array);
                        for (const auto& sub : p->subtasks) {
                            JsonValue subj(JsonValue::Object);
                            subj.set("id", JsonValue((double)sub.id));
                            subj.set("points", JsonValue((double)sub.points));
                            subj.set("constraints", JsonValue(sub.constraints));
                            subtasks_arr.push(subj);
                        }
                        pj.set("subtasks", subtasks_arr);

                        JsonValue samples_arr(JsonValue::Array);
                        for (const auto& s : p->samples) {
                            JsonValue sj(JsonValue::Object);
                            sj.set("input", JsonValue(s.input));
                            sj.set("output", JsonValue(s.expected));
                            sj.set("explanation", JsonValue(s.explanation));
                            samples_arr.push(sj);
                        }
                        pj.set("samples", samples_arr);
                    }
                    probs_arr.push(pj);
                }
                cj.set("problems", probs_arr);
                resp.body = cj.to_string();
            }
            return resp;
        }

        // POST /api/judge
        if (path == "/api/judge" && req.method == "POST") {
            JsonParser parser(req.body);
            auto j = parser.parse();

            std::string problem_id = j.get_string("problem_id");
            std::string code = j.get_string("code");
            std::string language = j.get_string("language", "cpp");
            std::string compiler_path = j.get_string("compiler_path", "");

            if (problem_id.empty() || code.empty()) {
                JsonValue err(JsonValue::Object);
                err.set("error", JsonValue("Missing problem_id or code"));
                resp.body = err.to_string();
                return resp;
            }

            if (language != "cpp") {
                JsonValue err(JsonValue::Object);
                err.set("error", JsonValue("Only C++ supported currently"));
                resp.body = err.to_string();
                return resp;
            }

            const Problem* problem = problem_mgr.get(problem_id);
            if (!problem) {
                JsonValue err(JsonValue::Object);
                err.set("error", JsonValue("Problem not found"));
                resp.body = err.to_string();
                return resp;
            }

            // Generate unique work dir
            std::string work_dir = "submissions/" + problem_id + "_" + std::to_string(GetCurrentProcessId());

            auto judge_result = judge_submission(code, *problem, work_dir, compiler_path);

            JsonValue jr(JsonValue::Object);
            jr.set("verdict", JsonValue(judge_result.verdict));
            jr.set("passed", JsonValue((double)judge_result.passed));
            jr.set("total", JsonValue((double)judge_result.total));
            jr.set("score", JsonValue((double)judge_result.score));
            jr.set("max_score", JsonValue((double)judge_result.max_score));
            jr.set("compile_error", JsonValue(judge_result.compile_error));

            JsonValue sub_arr(JsonValue::Array);
            for (const auto& [sub_id, sub] : judge_result.subtask_results) {
                JsonValue subj(JsonValue::Object);
                subj.set("id", JsonValue((double)sub.id));
                subj.set("points", JsonValue((double)sub.points));
                subj.set("max_points", JsonValue((double)sub.max_points));
                subj.set("passed", JsonValue((double)sub.passed));
                subj.set("total", JsonValue((double)sub.total));
                subj.set("status", JsonValue(sub.status));
                sub_arr.push(subj);
            }
            jr.set("subtask_results", sub_arr);

            JsonValue tc_arr(JsonValue::Array);
            for (const auto& tc : judge_result.test_cases) {
                JsonValue tj(JsonValue::Object);
                tj.set("index", JsonValue((double)tc.index));
                tj.set("status", JsonValue(tc.status));
                tj.set("input", JsonValue(tc.input));
                tj.set("expected", JsonValue(tc.expected));
                tj.set("output", JsonValue(tc.output));
                tj.set("error", JsonValue(tc.error));
                tj.set("time", JsonValue(tc.time));
                int orig_idx = tc.index - 1;
                int sub_id = (orig_idx >= 0 && orig_idx < (int)problem->test_cases.size()) ? problem->test_cases[orig_idx].subtask : 0;
                tj.set("subtask", JsonValue((double)sub_id));
                tc_arr.push(tj);
            }
            jr.set("test_cases", tc_arr);
            resp.body = jr.to_string();

            // Cleanup work dir
            fs::remove_all(work_dir);

            return resp;
        }

        // POST /api/generate
        if (path == "/api/generate" && req.method == "POST") {
            JsonParser gen_parser(req.body);
            auto gen_j = gen_parser.parse();

            AIRequest ai_req;
            ai_req.provider = gen_j.get_string("provider", "openai");
            ai_req.model = gen_j.get_string("model", "");
            ai_req.api_key = gen_j.get_string("api_key", "");
            ai_req.category = gen_j.get_string("category", "");
            ai_req.sub_type = gen_j.get_string("sub_type", "");
            ai_req.subtask_count = gen_j.get_int("subtask_count", 3);
            ai_req.rating = gen_j.get_string("rating", "");

            if (ai_req.api_key.empty()) {
                JsonValue err(JsonValue::Object);
                err.set("error", JsonValue("Vui lòng nhập API key."));
                resp.body = err.to_string();
                return resp;
            }
            if (ai_req.category.empty() || ai_req.sub_type.empty()) {
                JsonValue err(JsonValue::Object);
                err.set("error", JsonValue("Vui lòng chọn thể loại và dạng bài."));
                resp.body = err.to_string();
                return resp;
            }

            std::string error;
            Problem prob = generate_problem_ai(ai_req, error);

            if (!error.empty()) {
                JsonValue err(JsonValue::Object);
                err.set("error", JsonValue(error));
                resp.body = err.to_string();
                return resp;
            }

            // Save to file
            fs::create_directories("problems");
            std::string filename = "problems/" + prob.id + ".json";
            std::string json_out = prob.to_json().to_string();
            std::ofstream f(filename);
            if (!f) {
                JsonValue err(JsonValue::Object);
                err.set("error", JsonValue("Không thể ghi file bài tập."));
                resp.body = err.to_string();
                return resp;
            }
            f << json_out;
            f.close();

            // Try to auto-reload
            problem_mgr.load_all();

            JsonValue ok(JsonValue::Object);
            ok.set("status", JsonValue("success"));
            ok.set("problem_id", JsonValue(prob.id));
            ok.set("title", JsonValue(prob.title));
            ok.set("test_count", JsonValue((double)prob.test_cases.size()));
            ok.set("subtask_count", JsonValue((double)prob.subtasks.size()));
            resp.body = ok.to_string();
            return resp;
        }

        // POST /api/solution/hint
        if (path == "/api/solution/hint" && req.method == "POST") {
            JsonParser parser(req.body);
            auto j = parser.parse();

            std::string problem_id = j.get_string("problem_id");
            std::string api_key = j.get_string("api_key", "");
            std::string provider = j.get_string("provider", "openai");
            std::string model = j.get_string("model", "");

            if (api_key.empty()) {
                JsonValue err(JsonValue::Object);
                err.set("error", JsonValue("Vui lòng nhập API key."));
                resp.body = err.to_string();
                return resp;
            }
            if (problem_id.empty()) {
                JsonValue err(JsonValue::Object);
                err.set("error", JsonValue("Thiếu problem_id."));
                resp.body = err.to_string();
                return resp;
            }

            const Problem* problem = problem_mgr.get(problem_id);
            if (!problem) {
                JsonValue err(JsonValue::Object);
                err.set("error", JsonValue("Không tìm thấy bài tập."));
                resp.body = err.to_string();
                return resp;
            }

            std::string system_prompt = "Bạn là chuyên gia hướng dẫn lập trình thi đấu. Sử dụng $...$ cho công thức toán học.";
            std::string user_prompt = build_solution_hint_prompt(*problem);
            std::string raw = call_ai_provider(provider, api_key, model, system_prompt, user_prompt);

            if (raw.empty()) {
                JsonValue err(JsonValue::Object);
                err.set("error", JsonValue("Không nhận được phản hồi từ AI."));
                resp.body = err.to_string();
                return resp;
            }
            if (raw.find("__ERROR__:") == 0) {
                JsonValue err(JsonValue::Object);
                err.set("error", JsonValue("Lỗi AI: " + raw.substr(10)));
                resp.body = err.to_string();
                return resp;
            }

            // Try to extract JSON from response
            std::string json_str = extract_json(raw);
            if (!json_str.empty()) {
                resp.body = json_str;
            } else {
                // Return raw text wrapped in JSON
                JsonValue ok(JsonValue::Object);
                ok.set("approach", JsonValue(raw));
                resp.body = ok.to_string();
            }
            return resp;
        }

        // POST /api/solution/code
        if (path == "/api/solution/code" && req.method == "POST") {
            JsonParser parser(req.body);
            auto j = parser.parse();

            std::string problem_id = j.get_string("problem_id");
            std::string api_key = j.get_string("api_key", "");
            std::string provider = j.get_string("provider", "openai");
            std::string model = j.get_string("model", "");

            if (api_key.empty()) {
                JsonValue err(JsonValue::Object);
                err.set("error", JsonValue("Vui lòng nhập API key."));
                resp.body = err.to_string();
                return resp;
            }
            if (problem_id.empty()) {
                JsonValue err(JsonValue::Object);
                err.set("error", JsonValue("Thiếu problem_id."));
                resp.body = err.to_string();
                return resp;
            }

            const Problem* problem = problem_mgr.get(problem_id);
            if (!problem) {
                JsonValue err(JsonValue::Object);
                err.set("error", JsonValue("Không tìm thấy bài tập."));
                resp.body = err.to_string();
                return resp;
            }

            std::string system_prompt = "Bạn là lập trình viên C++ thi đấu chuyên nghiệp. Sử dụng $...$ cho công thức toán học.";
            std::string user_prompt = build_solution_code_prompt(*problem);
            std::string raw = call_ai_provider(provider, api_key, model, system_prompt, user_prompt);

            if (raw.empty()) {
                JsonValue err(JsonValue::Object);
                err.set("error", JsonValue("Không nhận được phản hồi từ AI."));
                resp.body = err.to_string();
                return resp;
            }
            if (raw.find("__ERROR__:") == 0) {
                JsonValue err(JsonValue::Object);
                err.set("error", JsonValue("Lỗi AI: " + raw.substr(10)));
                resp.body = err.to_string();
                return resp;
            }

            std::string json_str = extract_json(raw);
            if (!json_str.empty()) {
                resp.body = json_str;
            } else {
                JsonValue ok(JsonValue::Object);
                ok.set("code", JsonValue(raw));
                resp.body = ok.to_string();
            }
            return resp;
        }

        // POST /api/chat
        if (path == "/api/chat" && req.method == "POST") {
            JsonParser parser(req.body);
            auto j = parser.parse();

            std::string problem_id = j.get_string("problem_id", "");
            std::string message = j.get_string("message", "");
            std::string api_key = j.get_string("api_key", "");
            std::string provider = j.get_string("provider", "openai");

            if (api_key.empty()) {
                JsonValue err(JsonValue::Object);
                err.set("error", JsonValue("Vui lòng nhập API key."));
                resp.body = err.to_string();
                return resp;
            }
            if (message.empty()) {
                JsonValue err(JsonValue::Object);
                err.set("error", JsonValue("Vui lòng nhập câu hỏi."));
                resp.body = err.to_string();
                return resp;
            }

            const Problem* problem = nullptr;
            if (!problem_id.empty()) {
                problem = problem_mgr.get(problem_id);
            }

            std::string system_prompt = build_chat_system_prompt(problem);
            std::string raw = call_ai_provider(provider, api_key, "", system_prompt, message);

            if (raw.empty()) {
                JsonValue err(JsonValue::Object);
                err.set("error", JsonValue("Không nhận được phản hồi từ AI."));
                resp.body = err.to_string();
                return resp;
            }
            if (raw.find("__ERROR__:") == 0) {
                JsonValue err(JsonValue::Object);
                err.set("error", JsonValue("Lỗi AI: " + raw.substr(10)));
                resp.body = err.to_string();
                return resp;
            }

            JsonValue ok(JsonValue::Object);
            ok.set("reply", JsonValue(raw));
            resp.body = ok.to_string();
            return resp;
        }

        // POST /api/reload
        if (path == "/api/reload" && req.method == "POST") {
            problem_mgr.load_all();
            contest_mgr.load_all();
            JsonValue ok(JsonValue::Object);
            ok.set("status", JsonValue("success"));
            ok.set("count", JsonValue((double)problem_mgr.list().size()));
            ok.set("contest_count", JsonValue((double)contest_mgr.list().size()));
            resp.body = ok.to_string();
            return resp;
        }

        // 404
        resp.status = 404;
        resp.status_text = "Not Found";
        JsonValue err(JsonValue::Object);
        err.set("error", JsonValue("API endpoint not found"));
        resp.body = err.to_string();
        return resp;
    }

    void handle_client(SOCKET client) {
        std::string raw;
        char buf[65536];
        int n;

        // Read request
        while ((n = recv(client, buf, sizeof(buf) - 1, 0)) > 0) {
            buf[n] = 0;
            raw += buf;
            if (raw.find("\r\n\r\n") != std::string::npos) {
                // Got headers; check if there's a body
                size_t body_start = raw.find("\r\n\r\n") + 4;
                // Try to read content-length more bytes
                std::string headers = raw.substr(0, body_start - 4);
                auto cl_pos = headers.find("Content-Length:");
                if (cl_pos == std::string::npos) cl_pos = headers.find("content-length:");
                if (cl_pos != std::string::npos) {
                    size_t val_start = headers.find(':', cl_pos) + 1;
                    size_t val_end = headers.find("\r\n", val_start);
                    int content_len = std::stoi(trim(headers.substr(val_start, val_end - val_start)));
                    while ((int)raw.size() - (int)body_start < content_len) {
                        n = recv(client, buf, sizeof(buf) - 1, 0);
                        if (n <= 0) break;
                        buf[n] = 0;
                        raw += buf;
                    }
                }
                break;
            }
        }

        if (raw.empty()) {
            closesocket(client);
            return;
        }

        Request req = parse_request(raw);
        Response resp;

        if (req.method == "OPTIONS") {
            resp = handle_api(req);
        } else if (req.path.find("/api/") == 0) {
            resp = handle_api(req);
        } else {
            resp = serve_static(url_path_decode(req.path));
        }

        std::string resp_str = resp.to_string();
        send(client, resp_str.c_str(), (int)resp_str.size(), 0);
        closesocket(client);
    }

public:
    HttpServer(int port, const std::string& static_dir, ProblemManager& pm, ContestManager& cm)
        : port(port), static_dir(static_dir), problem_mgr(pm), contest_mgr(cm) {}

    bool start() {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            std::cerr << "WSAStartup failed\n";
            return false;
        }

        listen_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_socket == INVALID_SOCKET) {
            std::cerr << "socket failed: " << WSAGetLastError() << "\n";
            WSACleanup();
            return false;
        }

        int opt = 1;
        setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(listen_socket, (sockaddr*)&addr, sizeof(addr)) != 0) {
            std::cerr << "bind failed on port " << port << ": " << WSAGetLastError() << "\n";
            closesocket(listen_socket);
            WSACleanup();
            return false;
        }

        if (listen(listen_socket, SOMAXCONN) != 0) {
            std::cerr << "listen failed: " << WSAGetLastError() << "\n";
            closesocket(listen_socket);
            WSACleanup();
            return false;
        }

        running = true;
        std::cout << "Local OJ Server running on http://localhost:" << port << "\n";
        std::cout << "Press Ctrl+C to stop\n";

        while (running) {
            sockaddr_in client_addr;
            int addr_len = sizeof(client_addr);
            SOCKET client = accept(listen_socket, (sockaddr*)&client_addr, &addr_len);
            if (client == INVALID_SOCKET) {
                if (running) {
                    std::cerr << "accept failed: " << WSAGetLastError() << "\n";
                }
                continue;
            }
            std::thread(&HttpServer::handle_client, this, client).detach();
        }

        closesocket(listen_socket);
        WSACleanup();
        return true;
    }

    void stop() {
        running = false;
    }
};

// ---------- Main ----------
int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    int port = 8080;
    std::string static_dir = "static";
    std::string problems_dir = "problems";
    std::string github_repo = "";

    bool force_sync = false;

    // Parse args
    for (int i = 1; i < __argc; i++) {
        std::string arg = __argv[i];
        if (arg == "--repo" && i + 1 < __argc) {
            github_repo = __argv[++i];
        } else if (arg == "--force") {
            force_sync = true;
        } else if (arg == "--g++" && i + 1 < __argc) {
            g_compiler_path = __argv[++i];
        } else if (arg == "--port" && i + 1 < __argc) {
            port = std::stoi(__argv[++i]);
        } else if (arg == "--static" && i + 1 < __argc) {
            static_dir = __argv[++i];
        } else if (arg == "--problems" && i + 1 < __argc) {
            problems_dir = __argv[++i];
        } else if (arg[0] != '-') {
            // positional: port
            port = std::stoi(arg);
        }
    }

    std::string contests_dir = "contests";

    // Sync problems from GitHub if --repo is given
    if (!github_repo.empty()) {
        bool has_local = fs::exists(problems_dir) && !fs::is_empty(problems_dir);
        if (force_sync || !has_local) {
            std::cout << "Syncing problems from GitHub: " << github_repo << "\n";
            sync_problems_from_github(github_repo, problems_dir);
        } else {
            std::cout << "Problems directory exists locally, skipping GitHub sync.\n";
            std::cout << "Use --repo --force to re-download.\n";
        }
    }

    ProblemManager problem_mgr(problems_dir);
    ContestManager contest_mgr(contests_dir);
    HttpServer server(port, static_dir, problem_mgr, contest_mgr);

    std::cout << "==========================================\n";
    std::cout << "  Local OJ Server\n";
    std::cout << "  Port: " << port << "\n";
    std::cout << "  Static: " << fs::absolute(static_dir).string() << "\n";
    std::cout << "  Problems: " << fs::absolute(problems_dir).string() << "\n";
    std::cout << "  Compiler: " << g_compiler_path << "\n";
    std::cout << "==========================================\n";

    server.start();

    return 0;
}
