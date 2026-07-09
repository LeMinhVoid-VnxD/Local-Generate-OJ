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
    double time_limit = 1.0;
    int memory_limit = 256;
    std::vector<TestCase> test_cases;
    std::vector<Sample> samples;
    std::vector<Subtask> subtasks;

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

    HANDLE hStdInRd, hStdInWr, hStdOutRd, hStdOutWr;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    if (!CreatePipe(&hStdInRd, &hStdInWr, &sa, 0)) { error_msg = "CreatePipe failed"; return ""; }
    if (!CreatePipe(&hStdOutRd, &hStdOutWr, &sa, 0)) { CloseHandle(hStdInRd); CloseHandle(hStdInWr); error_msg = "CreatePipe failed"; return ""; }

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

    // Write input
    DWORD written;
    WriteFile(hStdInWr, input.c_str(), (DWORD)input.size(), &written, NULL);
    CloseHandle(hStdInWr);

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

    WaitForSingleObject(pi.hProcess, 100);
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
    // Keep internal newlines, just remove trailing whitespace
    std::string r = s.substr(0, end + 1);
    // Also normalize \r\n to \n
    std::string norm;
    for (size_t i = 0; i < r.size(); i++) {
        if (r[i] == '\r') continue;
        norm += r[i];
    }
    return norm;
}

JudgeResult judge_submission(const std::string& code, const Problem& problem, const std::string& work_dir) {
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

    // Compile
    std::string compile_cmd = "g++ -std=c++17 -O2 -o \"" + exe_path + "\" \"" + src_path + "\" 2>&1";
    std::string compile_out = exec_cmd(compile_cmd, 15000);

    if (!fs::exists(exe_path)) {
        result.verdict = "CE";
        result.compile_error = compile_out;
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
            std::string got = trim_trailing(output);
            std::string exp = trim_trailing(tr.expected);
            if (got == exp) {
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

    // Cleanup
    fs::remove(src_path);
    fs::remove(exe_path);

    return result;
}

// ---------- AI Client ----------
// WinHTTP AI Problem generator functions removed as they are no longer needed.

// ---------- HTTP Server ----------
class HttpServer {
    SOCKET listen_socket;
    int port;
    std::string static_dir;
    ProblemManager& problem_mgr;
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

        // POST /api/judge
        if (path == "/api/judge" && req.method == "POST") {
            JsonParser parser(req.body);
            auto j = parser.parse();

            std::string problem_id = j.get_string("problem_id");
            std::string code = j.get_string("code");
            std::string language = j.get_string("language", "cpp");

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

            auto judge_result = judge_submission(code, *problem, work_dir);

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

        // POST /api/reload
        if (path == "/api/reload" && req.method == "POST") {
            problem_mgr.load_all();
            JsonValue ok(JsonValue::Object);
            ok.set("status", JsonValue("success"));
            ok.set("count", JsonValue((double)problem_mgr.list().size()));
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
    HttpServer(int port, const std::string& static_dir, ProblemManager& pm)
        : port(port), static_dir(static_dir), problem_mgr(pm) {}

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

    // Check args
    if (__argc > 1) port = std::stoi(__argv[1]);
    if (__argc > 2) static_dir = __argv[2];
    if (__argc > 3) problems_dir = __argv[3];

    ProblemManager problem_mgr(problems_dir);
    HttpServer server(port, static_dir, problem_mgr);

    std::cout << "==========================================\n";
    std::cout << "  Local OJ Server\n";
    std::cout << "  Port: " << port << "\n";
    std::cout << "  Static: " << fs::absolute(static_dir).string() << "\n";
    std::cout << "  Problems: " << fs::absolute(problems_dir).string() << "\n";
    std::cout << "==========================================\n";

    server.start();

    return 0;
}
