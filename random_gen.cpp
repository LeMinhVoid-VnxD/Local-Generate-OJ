#include <bits/stdc++.h>
using namespace std;
using namespace std::filesystem;
using ll = long long;
using ull = unsigned long long;
using ld = long double;

static mt19937 rng(chrono::system_clock::now().time_since_epoch().count());

template<typename T>
T randint(T l, T r) {
    return uniform_int_distribution<T>(l, r)(rng);
}

ll randll(ll l, ll r) {
    return uniform_int_distribution<ll>(l, r)(rng);
}

static string json_escape(const string& s) {
    string out;
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
}

struct TestCase {
    string input, expected;
    int subtask = 0;
    TestCase() = default;
    TestCase(string inp, string exp, int sub = 0) : input(move(inp)), expected(move(exp)), subtask(sub) {}
};

struct Sample {
    string input, expected, explanation;
};

struct SubtaskDef {
    int id, points;
    string constraints;
};

struct Problem {
    string id_str, title, category, difficulty;
    string description, input_format, output_format, constraints_str;
    double time_limit = 1.0;
    int memory_limit = 256;
    int cf_rating = 800;
    vector<SubtaskDef> subtasks;
    vector<Sample> samples;
    vector<TestCase> test_cases;

    static int difficulty_to_rating(const string& diff) {
        if (diff == "easy") return randint(800, 999);
        if (diff == "medium") return randint(1000, 1499);
        if (diff == "hard") return randint(1500, 1999);
        return 800;
    }

    static string rating_to_label(int r) {
        if (r < 1000) return "Easy";
        if (r < 1500) return "Medium";
        if (r < 2000) return "Hard";
        if (r < 2500) return "Hardcore";
        if (r < 3000) return "IOI-ICPC";
        return "Only AI";
    }

    string to_json() const {
        ostringstream out;
        out << "{\n";
        out << "  \"title\": \"" << json_escape(title) << "\",\n";
        out << "  \"category\": \"" << json_escape(category) << "\",\n";
        out << "  \"difficulty\": \"" << json_escape(difficulty) << "\",\n";
        out << "  \"cf_rating\": " << cf_rating << ",\n";
        out << "  \"time_limit\": " << fixed << setprecision(1) << time_limit << ",\n";
        out << "  \"memory_limit\": " << memory_limit << ",\n";
        out << "  \"description\": \"" << json_escape(description) << "\",\n";
        out << "  \"input_format\": \"" << json_escape(input_format) << "\",\n";
        out << "  \"output_format\": \"" << json_escape(output_format) << "\",\n";
        out << "  \"constraints\": \"" << json_escape(constraints_str) << "\"";

        if (!subtasks.empty()) {
            out << ",\n  \"subtasks\": [\n";
            for (size_t i = 0; i < subtasks.size(); i++) {
                if (i > 0) out << ",\n";
                out << "    {\n";
                out << "      \"id\": " << subtasks[i].id << ",\n";
                out << "      \"points\": " << subtasks[i].points << ",\n";
                out << "      \"constraints\": \"" << json_escape(subtasks[i].constraints) << "\"\n";
                out << "    }";
            }
            out << "\n  ]";
        }

        out << ",\n  \"samples\": [\n";
        for (size_t i = 0; i < samples.size(); i++) {
            if (i > 0) out << ",\n";
            out << "    {\n";
            out << "      \"input\": \"" << json_escape(samples[i].input) << "\",\n";
            out << "      \"output\": \"" << json_escape(samples[i].expected) << "\",\n";
            out << "      \"explanation\": \"" << json_escape(samples[i].explanation) << "\"\n";
            out << "    }";
        }
        out << "\n  ],\n  \"test_cases\": [\n";
        for (size_t i = 0; i < test_cases.size(); i++) {
            if (i > 0) out << ",\n";
            out << "    {\n";
            out << "      \"input\": \"" << json_escape(test_cases[i].input) << "\",\n";
            out << "      \"output\": \"" << json_escape(test_cases[i].expected) << "\"";
            if (test_cases[i].subtask > 0) {
                out << ",\n      \"subtask\": " << test_cases[i].subtask;
            }
            out << "\n    }";
        }
        out << "\n  ]\n}\n";
        return out.str();
    }

    bool save(const string& path) const {
        ofstream f(path);
        if (!f) return false;
        f << to_json();
        return true;
    }
};

static string make_id(const string& prefix) {
    auto now = chrono::system_clock::now();
    auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()).count();
    return prefix + "_" + to_string(ms) + "_" + to_string(randint(1000, 9999));
}

static string pick(const vector<string>& vs) {
    return vs[randint(0, (int)vs.size() - 1)];
}

static void randomize_subtask_points(vector<SubtaskDef>& subtasks) {
    int n = (int)subtasks.size();
    if (n < 2) return;
    vector<int> pts(n);
    int remaining = 100;
    for (int i = 0; i < n - 1; i++) {
        int min_pt = max(5, 10 / n);
        int max_pt = remaining - (n - i - 1) * min_pt;
        if (max_pt <= min_pt) { pts[i] = min_pt; }
        else { pts[i] = randint(min_pt, max_pt); }
        remaining -= pts[i];
    }
    pts[n - 1] = remaining;
    for (int i = 0; i < n; i++) subtasks[i].points = pts[i];
}

// ============================================================
// GENERATOR 1: Tổng hai số (Sum of Two Numbers)
// ============================================================
Problem gen_sum_two() {
    Problem p;
    p.id_str = make_id("sum_two");
    p.title = pick({"Tổng hai số", "Tính A + B", "Phép cộng cơ bản", "Cộng hai số nguyên"});
    p.category = "basic";
    p.difficulty = pick({"easy", "easy", "medium"});
    p.description = pick({
        "Cho hai số nguyên A và B. Hãy tính và in ra tổng của chúng.",
        "Nhập vào hai số nguyên A và B, nhiệm vụ của bạn là tính tổng A + B.",
        "Hãy viết chương trình tính tổng hai số nguyên A và B được nhập từ bàn phím."
    });
    p.input_format = "Một dòng duy nhất chứa hai số nguyên A và B, cách nhau bởi dấu cách.";
    p.output_format = "In ra một số nguyên duy nhất là tổng A + B.";
    p.time_limit = 1.0;
    p.memory_limit = 256;

    p.subtasks = {
        {1, 30, "|A|, |B| <= 1000"},
        {2, 70, "|A|, |B| <= 10^9"}
    };

    auto make_tc = [](ll a, ll b, int sub) -> TestCase {
        return {to_string(a) + " " + to_string(b) + "\n", to_string(a + b) + "\n", sub};
    };

    p.constraints_str = "|A|, |B| <= 10^9";

    p.samples = {
        {"3 5\n", "8\n", "3 + 5 = 8"},
        {"-2 7\n", "5\n", "-2 + 7 = 5"},
        {"0 0\n", "0\n", "0 + 0 = 0"}
    };

    // Sub 1: small
    p.test_cases.push_back(make_tc(0, 0, 1));
    p.test_cases.push_back(make_tc(1, 2, 1));
    p.test_cases.push_back(make_tc(randint(-1000, 1000), randint(-1000, 1000), 1));
    p.test_cases.push_back(make_tc(randint(-500, 500), randint(-500, 500), 1));
    if (randint(0, 1)) p.test_cases.push_back(make_tc(randint(-1000, 1000), randint(-1000, 1000), 1));

    // Sub 2: large
    p.test_cases.push_back(make_tc(randint(-1000000000, 1000000000), randint(-1000000000, 1000000000), 2));
    p.test_cases.push_back(make_tc(randint(-1000000000, 1000000000), randint(-1000000000, 1000000000), 2));
    p.test_cases.push_back(make_tc(randint(-1000000000, 1000000000), randint(-1000000000, 1000000000), 2));
    for (int i = 0; i < randint(2, 5); i++)
        p.test_cases.push_back(make_tc(randint(-1000000000, 1000000000), randint(-1000000000, 1000000000), 2));
    p.test_cases.push_back(make_tc(0, 1000000000, 2));
    p.test_cases.push_back(make_tc(-1000000000, 0, 2));
    return p;
}

// ============================================================
// GENERATOR 2: Tổng dãy số (Sum of Array)
// ============================================================
Problem gen_array_sum() {
    Problem p;
    p.id_str = make_id("array_sum");
    p.title = pick({"Tổng dãy số", "Tính tổng mảng", "Sum of Array", "Tổng các phần tử"});
    p.category = "array";
    p.difficulty = pick({"easy", "easy", "medium"});
    p.description = pick({
        "Cho một dãy số nguyên A gồm N phần tử. Hãy tính tổng tất cả các phần tử trong dãy.",
        "Cho mảng A có N số nguyên. Hãy tính tổng các giá trị trong mảng.",
        "Viết chương trình tính tổng các số nguyên trong một dãy số cho trước."
    });
    p.input_format = "Dòng đầu tiên chứa số nguyên N (1 <= N <= 10^5).\nDòng thứ hai chứa N số nguyên A_1, A_2, ..., A_N (-10^6 <= A_i <= 10^6), cách nhau bởi dấu cách.";
    p.output_format = "In ra một số nguyên duy nhất là tổng của dãy.";
    p.constraints_str = "1 <= N <= 10^5, -10^6 <= A_i <= 10^6";
    p.time_limit = 1.0;

    p.subtasks = {
        {1, 30, "N <= 100, |A_i| <= 100"},
        {2, 70, "N <= 10^5, |A_i| <= 10^6"}
    };

    auto make_arr_str = [](const vector<ll>& v) {
        ostringstream os;
        for (size_t i = 0; i < v.size(); i++) {
            if (i > 0) os << " ";
            os << v[i];
        }
        os << "\n";
        return os.str();
    };

    auto make_tc = [&](int n, ll lo, ll hi, int sub) -> TestCase {
        vector<ll> arr(n);
        ll sum = 0;
        for (int i = 0; i < n; i++) {
            arr[i] = randll(lo, hi);
            sum += arr[i];
        }
        return {to_string(n) + "\n" + make_arr_str(arr), to_string(sum) + "\n", sub};
    };

    p.samples = {
        {"5\n1 2 3 4 5\n", "15\n", "1 + 2 + 3 + 4 + 5 = 15"},
        {"3\n-5 0 5\n", "0\n", "-5 + 0 + 5 = 0"}
    };

    // Sub 1: small
    p.test_cases.push_back({"1\n5\n", "5\n", 1});
    p.test_cases.push_back({"2\n-100 100\n", "0\n", 1});
    p.test_cases.push_back({"5\n0 0 0 0 0\n", "0\n", 1});
    for (int i = 0; i < randint(1, 4); i++)
        p.test_cases.push_back(make_tc(randint(10, 100), -100, 100, 1));

    // Sub 2: large
    for (int i = 0; i < randint(3, 8); i++)
        p.test_cases.push_back(make_tc(randint(1000, 100000), randint(-1000000, -1), randint(1, 1000000), 2));
    p.test_cases.push_back(make_tc(randint(100, 1000), 0, 0, 2));
    p.test_cases.push_back(make_tc(randint(50000, 100000), 1, 1000000, 2));
    p.test_cases.push_back(make_tc(randint(50000, 100000), -1000000, -1, 2));
    return p;
}

// ============================================================
// GENERATOR 3: Số lớn nhất (Maximum Element)
// ============================================================
Problem gen_max_element() {
    Problem p;
    p.id_str = make_id("max_element");
    p.title = pick({"Số lớn nhất", "Tìm max", "Maximum Element", "Giá trị lớn nhất"});
    p.category = "array";
    p.difficulty = pick({"easy", "easy", "medium"});
    p.description = pick({
        "Cho một dãy số nguyên A gồm N phần tử. Hãy tìm giá trị lớn nhất trong dãy.",
        "Tìm phần tử có giá trị lớn nhất trong mảng số nguyên.",
        "Viết chương trình tìm số lớn nhất trong một dãy số."
    });
    p.input_format = "Dòng đầu tiên chứa số nguyên N (1 <= N <= 10^5).\nDòng thứ hai chứa N số nguyên A_i (-10^9 <= A_i <= 10^9), cách nhau bởi dấu cách.";
    p.output_format = "In ra một số nguyên duy nhất là giá trị lớn nhất trong dãy.";
    p.constraints_str = "1 <= N <= 10^5, -10^9 <= A_i <= 10^9";
    p.time_limit = 1.0;

    p.subtasks = {
        {1, 30, "N <= 100, |A_i| <= 1000"},
        {2, 70, "N <= 10^5, |A_i| <= 10^9"}
    };

    auto make_arr_str = [](const vector<ll>& v) {
        ostringstream os;
        for (size_t i = 0; i < v.size(); i++) {
            if (i > 0) os << " ";
            os << v[i];
        }
        os << "\n";
        return os.str();
    };

    auto make_tc = [&](int n, ll lo, ll hi, int sub) -> TestCase {
        vector<ll> arr(n);
        for (int i = 0; i < n; i++) arr[i] = randll(lo, hi);
        ll mx = *max_element(arr.begin(), arr.end());
        return {to_string(n) + "\n" + make_arr_str(arr), to_string(mx) + "\n", sub};
    };

    p.samples = {
        {"5\n3 7 1 9 4\n", "9\n", "Số lớn nhất là 9."},
        {"3\n-5 -2 -8\n", "-2\n", "Số lớn nhất là -2."}
    };

    // Sub 1
    p.test_cases.push_back({"1\n42\n", "42\n", 1});
    p.test_cases.push_back({"2\n-1000 1000\n", "1000\n", 1});
    p.test_cases.push_back({"5\n0 0 0 0 0\n", "0\n", 1});
    for (int i = 0; i < randint(1, 3); i++)
        p.test_cases.push_back(make_tc(randint(10, 100), -1000, 1000, 1));

    // Sub 2
    for (int i = 0; i < randint(3, 7); i++)
        p.test_cases.push_back(make_tc(randint(1000, 100000), -1000000000, 1000000000, 2));
    p.test_cases.push_back(make_tc(randint(50000, 100000), 1, 1000000000, 2));
    p.test_cases.push_back(make_tc(randint(50000, 100000), -1000000000, -1, 2));
    return p;
}

// ============================================================
// GENERATOR 4: Đếm số chẵn (Count Even Numbers)
// ============================================================
Problem gen_count_even() {
    Problem p;
    p.id_str = make_id("count_even");
    p.title = pick({"Đếm số chẵn", "Số lượng số chẵn", "Count Even", "Đếm chẵn lẻ"});
    p.category = "array";
    p.difficulty = pick({"easy", "easy", "medium"});
    p.description = pick({
        "Cho một dãy số nguyên A gồm N phần tử. Hãy đếm số lượng số chẵn trong dãy.",
        "Đếm xem trong mảng số nguyên có bao nhiêu số chẵn.",
        "Viết chương trình đếm các số chẵn trong một dãy số."
    });
    p.input_format = "Dòng đầu tiên chứa số nguyên N (1 <= N <= 10^5).\nDòng thứ hai chứa N số nguyên A_i (-10^9 <= A_i <= 10^9), cách nhau bởi dấu cách.";
    p.output_format = "In ra một số nguyên duy nhất là số lượng số chẵn trong dãy.";
    p.constraints_str = "1 <= N <= 10^5, -10^9 <= A_i <= 10^9";
    p.time_limit = 1.0;

    p.subtasks = {
        {1, 30, "N <= 100, |A_i| <= 1000"},
        {2, 70, "N <= 10^5, |A_i| <= 10^9"}
    };

    auto make_arr_str = [](const vector<ll>& v) {
        ostringstream os;
        for (size_t i = 0; i < v.size(); i++) {
            if (i > 0) os << " ";
            os << v[i];
        }
        os << "\n";
        return os.str();
    };

    auto make_tc = [&](int n, ll lo, ll hi, int sub) -> TestCase {
        vector<ll> arr(n);
        int cnt = 0;
        for (int i = 0; i < n; i++) {
            arr[i] = randll(lo, hi);
            if (arr[i] % 2 == 0) cnt++;
        }
        return {to_string(n) + "\n" + make_arr_str(arr), to_string(cnt) + "\n", sub};
    };

    p.samples = {
        {"5\n1 2 3 4 5\n", "2\n", "Các số chẵn là 2 và 4."},
        {"3\n0 -1 2\n", "2\n", "0 và 2 là số chẵn."}
    };

    // Sub 1
    p.test_cases.push_back({"1\n7\n", "0\n", 1});
    p.test_cases.push_back({"1\n8\n", "1\n", 1});
    p.test_cases.push_back({"5\n0 0 0 0 0\n", "5\n", 1});
    p.test_cases.push_back({"4\n-2 -1 0 1\n", "2\n", 1});
    for (int i = 0; i < randint(1, 3); i++)
        p.test_cases.push_back(make_tc(randint(10, 100), -100, 100, 1));

    // Sub 2
    for (int i = 0; i < randint(3, 7); i++)
        p.test_cases.push_back(make_tc(randint(1000, 100000), -1000000000, 1000000000, 2));
    p.test_cases.push_back(make_tc(randint(1000, 50000), 1, 1000000000, 2));
    p.test_cases.push_back(make_tc(randint(1000, 50000), -1000000000, -1, 2));
    return p;
}

// ============================================================
// GENERATOR 5: Tổng chữ số (Sum of Digits)
// ============================================================
Problem gen_digit_sum() {
    Problem p;
    p.id_str = make_id("digit_sum");
    p.title = pick({"Tổng chữ số", "Sum of Digits", "Digit Sum", "Tổng các chữ số"});
    p.category = "math";
    p.difficulty = pick({"easy", "easy", "medium"});
    p.description = pick({
        "Cho một số nguyên không âm N. Hãy tính tổng các chữ số của N.",
        "Tính tổng tất cả các chữ số của một số nguyên dương.",
        "Viết chương trình tính tổng các chữ số của số N."
    });
    p.input_format = "Một dòng duy nhất chứa số nguyên không âm N (0 <= N <= 10^18).";
    p.output_format = "In ra một số nguyên duy nhất là tổng các chữ số của N.";
    p.constraints_str = "0 <= N <= 10^18";
    p.time_limit = 1.0;

    p.subtasks = {
        {1, 30, "N <= 10^6"},
        {2, 70, "N <= 10^18"}
    };

    auto digit_sum = [](ll n) -> int {
        int s = 0;
        if (n == 0) return 0;
        while (n > 0) { s += n % 10; n /= 10; }
        return s;
    };

    auto make_tc = [&](ll n, int sub) -> TestCase {
        return {to_string(n) + "\n", to_string(digit_sum(n)) + "\n", sub};
    };

    p.samples = {
        {"123\n", "6\n", "1 + 2 + 3 = 6"},
        {"0\n", "0\n", "0"}
    };

    // Sub 1
    p.test_cases.push_back(make_tc(0, 1));
    p.test_cases.push_back(make_tc(randint(1, 9), 1));
    p.test_cases.push_back(make_tc(randint(10, 99), 1));
    p.test_cases.push_back(make_tc(randint(100, 999), 1));
    p.test_cases.push_back(make_tc(randint(1000, 9999), 1));
    p.test_cases.push_back(make_tc(randint(100000, 999999), 1));

    // Sub 2
    for (int i = 0; i < randint(3, 5); i++)
        p.test_cases.push_back(make_tc(randll(1000000000LL, 999999999999999999LL), 2));
    p.test_cases.push_back(make_tc(0, 2));
    p.test_cases.push_back(make_tc(1000000000000000000LL, 2));
    return p;
}

// ============================================================
// GENERATOR 6: GCD (Ước chung lớn nhất)
// ============================================================
Problem gen_gcd() {
    Problem p;
    p.id_str = make_id("gcd");
    p.title = pick({"Ước chung lớn nhất", "GCD", "Greatest Common Divisor", "Tìm GCD"});
    p.category = "math";
    p.difficulty = pick({"easy", "easy", "medium"});
    p.description = pick({
        "Cho hai số nguyên dương A và B. Hãy tìm ước chung lớn nhất của chúng.",
        "Tìm số lớn nhất chia hết cả A và B.",
        "Viết chương trình tìm GCD của hai số nguyên dương."
    });
    p.input_format = "Một dòng duy nhất chứa hai số nguyên dương A và B (1 <= A, B <= 10^12), cách nhau bởi dấu cách.";
    p.output_format = "In ra một số nguyên duy nhất là GCD(A, B).";
    p.constraints_str = "1 <= A, B <= 10^12";
    p.time_limit = 1.0;

    p.subtasks = {
        {1, 30, "A, B <= 1000"},
        {2, 70, "A, B <= 10^12"}
    };

    auto my_gcd = [](ll a, ll b) -> ll {
        while (b) { ll t = b; b = a % b; a = t; }
        return a;
    };

    auto make_tc = [&](ll a, ll b, int sub) -> TestCase {
        return {to_string(a) + " " + to_string(b) + "\n", to_string(my_gcd(a, b)) + "\n", sub};
    };

    p.samples = {
        {"12 8\n", "4\n", "GCD(12, 8) = 4"},
        {"7 13\n", "1\n", "7 và 13 là hai số nguyên tố cùng nhau."}
    };

    // Sub 1
    p.test_cases.push_back(make_tc(1, 1, 1));
    p.test_cases.push_back(make_tc(randint(1, 1000), randint(1, 1000), 1));
    p.test_cases.push_back(make_tc(randint(1, 1000), randint(1, 1000), 1));
    p.test_cases.push_back(make_tc(randint(1, 1000), randint(1, 1000), 1));
    for (int i = 0; i < randint(1, 3); i++)
        p.test_cases.push_back(make_tc(randint(1, 1000), randint(1, 1000), 1));

    // Sub 2
    for (int i = 0; i < randint(3, 6); i++)
        p.test_cases.push_back(make_tc(randll(1, 1000000000000LL), randll(1, 1000000000000LL), 2));
    p.test_cases.push_back(make_tc(1000000000000LL, 1, 2));
    p.test_cases.push_back(make_tc(1, 1000000000000LL, 2));
    return p;
}

// ============================================================
// GENERATOR 7: Kiểm tra nguyên tố (Prime Check)
// ============================================================
Problem gen_prime_check() {
    Problem p;
    p.id_str = make_id("prime_check");
    p.title = pick({"Kiểm tra số nguyên tố", "Prime Check", "Số nguyên tố", "Prime Number"});
    p.category = "math";
    p.difficulty = pick({"medium", "medium", "hard"});
    p.description = pick({
        "Cho một số nguyên dương N. Hãy kiểm tra xem N có phải là số nguyên tố hay không.",
        "Xác định xem số N có là số nguyên tố không.",
        "Viết chương trình kiểm tra tính nguyên tố của một số."
    });
    p.input_format = "Một dòng duy nhất chứa số nguyên dương N (1 <= N <= 10^7).";
    p.output_format = "In ra \"YES\" nếu N là số nguyên tố, ngược lại in ra \"NO\".";
    p.constraints_str = "1 <= N <= 10^7";
    p.time_limit = 1.0;

    p.subtasks = {
        {1, 30, "N <= 100"},
        {2, 70, "N <= 10^7"}
    };

    auto is_prime = [](int n) -> bool {
        if (n < 2) return false;
        if (n == 2) return true;
        if (n % 2 == 0) return false;
        for (int i = 3; i * i <= n; i += 2)
            if (n % i == 0) return false;
        return true;
    };

    auto make_tc = [&](int n, int sub) -> TestCase {
        return {to_string(n) + "\n", is_prime(n) ? "YES\n" : "NO\n", sub};
    };

    p.samples = {
        {"7\n", "YES\n", "7 là số nguyên tố."},
        {"10\n", "NO\n", "10 = 2 * 5, không phải số nguyên tố."}
    };

    // Sub 1
    p.test_cases.push_back(make_tc(1, 1));
    p.test_cases.push_back(make_tc(2, 1));
    for (int i = 0; i < randint(2, 4); i++)
        p.test_cases.push_back(make_tc(randint(3, 100), 1));

    // Sub 2
    for (int i = 0; i < randint(3, 6); i++)
        p.test_cases.push_back(make_tc(randint(101, 10000000), 2));
    p.test_cases.push_back(make_tc(2, 2));
    p.test_cases.push_back(make_tc(randint(1000000, 10000000), 2));
    p.test_cases.push_back(make_tc(randint(1000000, 10000000), 2));
    return p;
}

// ============================================================
// GENERATOR 8: Căn bậc hai nguyên (Integer Square Root)
// ============================================================
Problem gen_int_sqrt() {
    Problem p;
    p.id_str = make_id("int_sqrt");
    p.title = pick({"Căn bậc hai nguyên", "Integer Square Root", "Căn nguyên", "Tìm căn bậc 2"});
    p.category = "binary_search";
    p.difficulty = pick({"easy", "medium", "medium"});
    p.description = pick({
        "Cho số nguyên không âm N. Hãy tìm số nguyên x lớn nhất sao cho x * x <= N.",
        "Tìm phần nguyên của căn bậc hai của N.",
        "Viết chương trình tìm số nguyên lớn nhất không vượt quá căn bậc hai của N."
    });
    p.input_format = "Một dòng duy nhất chứa số nguyên không âm N (0 <= N <= 10^18).";
    p.output_format = "In ra một số nguyên duy nhất là kết quả x.";
    p.constraints_str = "0 <= N <= 10^18";
    p.time_limit = 1.0;

    p.subtasks = {
        {1, 30, "N <= 10^6"},
        {2, 70, "N <= 10^18"}
    };

    auto isqrt = [](ll n) -> ll {
        if (n < 0) return -1;
        ll x = (ll)sqrt((ld)n);
        while ((x + 1) * (x + 1) <= n) x++;
        while (x * x > n) x--;
        return x;
    };

    auto make_tc = [&](ll n, int sub) -> TestCase {
        return {to_string(n) + "\n", to_string(isqrt(n)) + "\n", sub};
    };

    p.samples = {
        {"10\n", "3\n", "3 * 3 = 9 <= 10, nhung 4 * 4 = 16 > 10."},
        {"16\n", "4\n", "4 * 4 = 16 <= 16."}
    };

    // Sub 1
    p.test_cases.push_back(make_tc(0, 1));
    p.test_cases.push_back(make_tc(1, 1));
    for (int i = 0; i < randint(3, 5); i++)
        p.test_cases.push_back(make_tc(randint(2, 1000000), 1));

    // Sub 2
    p.test_cases.push_back(make_tc(0, 2));
    for (int i = 0; i < randint(3, 6); i++)
        p.test_cases.push_back(make_tc(randll(1000001, 1000000000000000000LL), 2));
    return p;
}

// ============================================================
// GENERATOR 9: Tìm kiếm nhị phân (Binary Search - Lower Bound)
// ============================================================
Problem gen_binary_search() {
    Problem p;
    p.id_str = make_id("binary_search");
    p.title = pick({"Tìm kiếm nhị phân", "Binary Search", "Lower Bound", "Cận dưới"});
    p.category = "binary_search";
    p.difficulty = pick({"medium", "medium", "hard"});
    p.description = pick({
        "Cho một mảng A gồm N số nguyên đã được sắp xếp không giảm và Q truy vấn. Với mỗi truy vấn k, hãy tìm vị trí (1-indexed) của phần tử đầu tiên trong mảng A có giá trị lớn hơn hoặc bằng k. Nếu không có phần tử nào thỏa mãn, in ra -1.",
        "Cho mảng đã sắp xếp, tìm vị trí đầu tiên có giá trị >= k cho mỗi truy vấn.",
        "Thực hiện Q truy vấn tìm kiếm trên mảng đã sắp xếp, trả về vị trí đầu tiên >= k."
    });
    p.input_format = "Dòng đầu tiên chứa hai số nguyên N và Q (1 <= N, Q <= 10^5).\nDòng thứ hai chứa N số nguyên A_1, A_2, ..., A_N (-10^9 <= A_i <= 10^9) được sắp xếp không giảm.\nQ dòng tiếp theo, mỗi dòng chứa một số nguyên k (-10^9 <= k <= 10^9).";
    p.output_format = "In ra Q dòng, mỗi dòng là kết quả của truy vấn tương ứng (vị trí 1-indexed hoặc -1).";
    p.constraints_str = "1 <= N, Q <= 10^5, -10^9 <= A_i, k <= 10^9";
    p.time_limit = 1.0;

    p.subtasks = {
        {1, 30, "N, Q <= 100"},
        {2, 30, "N, Q <= 5000"},
        {3, 40, "N, Q <= 10^5"}
    };

    auto make_arr_str = [](const vector<ll>& v) {
        ostringstream os;
        for (size_t i = 0; i < v.size(); i++) {
            if (i > 0) os << " ";
            os << v[i];
        }
        return os.str();
    };

    auto lower_bound_pos = [](const vector<ll>& a, ll k) -> int {
        auto it = lower_bound(a.begin(), a.end(), k);
        if (it == a.end()) return -1;
        return (int)(it - a.begin()) + 1;
    };

    auto make_queries = [&](int qcnt, ll qlo, ll qhi) -> vector<ll> {
        vector<ll> q;
        set<ll> unique_q;
        int target = min(qcnt, 10);
        while ((int)unique_q.size() < target) {
            unique_q.insert(randll(qlo, qhi));
        }
        q.assign(unique_q.begin(), unique_q.end());
        return q;
    };

    auto make_tc = [&](int n, const vector<ll>& queries, int sub) -> TestCase {
        vector<ll> arr(n);
        ll lo = -1e9, hi = 1e9;
        if (sub == 1) { lo = -100; hi = 100; }
        else if (sub == 2) { lo = -10000; hi = 10000; }
        for (int i = 0; i < n; i++) arr[i] = randll(lo, hi);
        sort(arr.begin(), arr.end());

        ostringstream in;
        in << n << " " << queries.size() << "\n";
        in << make_arr_str(arr) << "\n";
        ostringstream out;
        for (ll k : queries) {
            in << k << "\n";
            out << lower_bound_pos(arr, k) << "\n";
        }
        return {in.str(), out.str(), sub};
    };

    p.samples = {
        {"5 3\n1 3 3 5 8\n3\n4\n10", "2\n4\n-1",
         "k=3 -> A[2]=3; k=4 -> A[4]=5; k=10 -> khong co."}
    };

    // Sub 1
    p.test_cases.push_back({"3 2\n10 20 30\n5\n35", "1\n-1\n", 1});
    p.test_cases.push_back(make_tc(randint(20, 100), make_queries(randint(3, 5), -100, 100), 1));
    p.test_cases.push_back(make_tc(randint(20, 100), make_queries(randint(3, 5), -100, 100), 1));

    // Sub 2
    p.test_cases.push_back(make_tc(randint(500, 5000), make_queries(randint(3, 6), -10000, 10000), 2));
    p.test_cases.push_back(make_tc(randint(500, 5000), make_queries(randint(3, 6), -10000, 10000), 2));

    // Sub 3
    for (int i = 0; i < randint(2, 4); i++)
        p.test_cases.push_back(make_tc(randint(10000, 100000), make_queries(randint(3, 6), -1000000000, 1000000000), 3));
    return p;
}

// ============================================================
// GENERATOR 10: Số Palindrome (Palindrome Number)
// ============================================================
Problem gen_palindrome() {
    Problem p;
    p.id_str = make_id("palindrome");
    p.title = pick({"Số Palindrome", "Palindrome Number", "Số đối xứng", "Palindrome"});
    p.category = "basic";
    p.difficulty = pick({"easy", "easy", "medium"});
    p.description = pick({
        "Cho một số nguyên không âm N. Hãy kiểm tra xem N có phải là số Palindrome hay không (số Palindrome là số đọc xuôi hay đọc ngược đều giống nhau).",
        "Kiểm tra một số có đọc xuôi ngược đều giống nhau không.",
        "Xác định xem N có là số đối xứng (Palindrome) hay không."
    });
    p.input_format = "Một dòng duy nhất chứa số nguyên không âm N (0 <= N <= 10^9).";
    p.output_format = "In ra \"YES\" nếu N là số Palindrome, ngược lại in ra \"NO\".";
    p.constraints_str = "0 <= N <= 10^9";
    p.time_limit = 1.0;

    p.subtasks = {
        {1, 30, "N <= 1000"},
        {2, 70, "N <= 10^9"}
    };

    auto is_pal = [](ll n) -> bool {
        if (n < 0) return false;
        string s = to_string(n);
        int l = 0, r = (int)s.size() - 1;
        while (l < r) if (s[l++] != s[r--]) return false;
        return true;
    };

    auto make_tc = [&](ll n, int sub) -> TestCase {
        return {to_string(n) + "\n", is_pal(n) ? "YES\n" : "NO\n", sub};
    };

    p.samples = {
        {"1221\n", "YES\n", "1221 đọc ngược là 1221."},
        {"123\n", "NO\n", "123 đọc ngược là 321, khác nhau."}
    };

    // Sub 1
    p.test_cases.push_back(make_tc(0, 1));
    p.test_cases.push_back(make_tc(1, 1));
    p.test_cases.push_back(make_tc(randint(2, 1000), 1));
    p.test_cases.push_back(make_tc(randint(2, 1000), 1));
    p.test_cases.push_back(make_tc(randint(2, 1000), 1));

    // Sub 2
    for (int i = 0; i < randint(3, 6); i++)
        p.test_cases.push_back(make_tc(randll(1001, 1000000000), 2));
    return p;
}

// ============================================================
// GENERATOR 11: Fibonacci (Số Fibonacci thứ N)
// ============================================================
Problem gen_fibonacci() {
    Problem p;
    p.id_str = make_id("fibonacci");
    p.title = pick({"Fibonacci", "Số Fibonacci thứ N", "Fibonacci modulo", "Tính Fibonacci"});
    p.category = "math";
    p.difficulty = pick({"medium", "medium", "hard"});
    p.description = pick({
        "Dãy Fibonacci được định nghĩa: F(0) = 0, F(1) = 1, F(n) = F(n-1) + F(n-2) với n >= 2. Cho số nguyên không âm N, hãy tính F(N) modulo 10^9 + 7.",
        "Tìm số Fibonacci thứ N modulo 10^9+7.",
        "Viết chương trình tính F(N) % (10^9+7) của dãy Fibonacci."
    });
    p.input_format = "Một dòng duy nhất chứa số nguyên không âm N (0 <= N <= 10^6).";
    p.output_format = "In ra một số nguyên duy nhất là F(N) % (10^9 + 7).";
    p.constraints_str = "0 <= N <= 10^6";
    p.time_limit = 1.0;
    const int MOD = 1000000007;

    p.subtasks = {
        {1, 30, "N <= 30"},
        {2, 70, "N <= 10^6"}
    };

    auto fib_mod = [&](int n) -> int {
        if (n == 0) return 0;
        int a = 0, b = 1;
        for (int i = 2; i <= n; i++) {
            int c = (a + b) % MOD;
            a = b; b = c;
        }
        return b;
    };

    auto make_tc = [&](int n, int sub) -> TestCase {
        return {to_string(n) + "\n", to_string(fib_mod(n)) + "\n", sub};
    };

    p.samples = {
        {"10\n", "55\n", "F(10) = 55."},
        {"0\n", "0\n", "F(0) = 0."}
    };

    // Sub 1
    p.test_cases.push_back(make_tc(0, 1));
    p.test_cases.push_back(make_tc(1, 1));
    for (int i = 0; i < randint(2, 5); i++)
        p.test_cases.push_back(make_tc(randint(2, 30), 1));

    // Sub 2
    for (int i = 0; i < randint(3, 6); i++)
        p.test_cases.push_back(make_tc(randint(100, 1000000), 2));
    p.test_cases.push_back(make_tc(1000000, 2));
    return p;
}

// ============================================================
// GENERATOR 12: Số chính phương (Perfect Square Check)
// ============================================================
Problem gen_perfect_square() {
    Problem p;
    p.id_str = make_id("perfect_square");
    p.title = pick({"Số chính phương", "Perfect Square", "Chính phương", "Kiểm tra chính phương"});
    p.category = "math";
    p.difficulty = pick({"easy", "easy", "medium"});
    p.description = pick({
        "Cho một số nguyên không âm N. Hãy kiểm tra xem N có phải là số chính phương hay không.",
        "Xác định xem N có là bình phương của một số nguyên hay không.",
        "Kiểm tra số chính phương: N có bằng k*k với k nguyên không?"
    });
    p.input_format = "Một dòng duy nhất chứa số nguyên không âm N (0 <= N <= 10^12).";
    p.output_format = "In ra \"YES\" nếu N là số chính phương, ngược lại in ra \"NO\".";
    p.constraints_str = "0 <= N <= 10^12";
    p.time_limit = 1.0;

    p.subtasks = {
        {1, 30, "N <= 10^6"},
        {2, 70, "N <= 10^12"}
    };

    auto is_perfect = [](ll n) -> bool {
        if (n < 0) return false;
        ll x = (ll)sqrt((ld)n);
        while ((x + 1) * (x + 1) <= n) x++;
        return x * x == n;
    };

    auto make_tc = [&](ll n, int sub) -> TestCase {
        return {to_string(n) + "\n", is_perfect(n) ? "YES\n" : "NO\n", sub};
    };

    p.samples = {
        {"16\n", "YES\n", "16 = 4^2."},
        {"15\n", "NO\n", "15 khong phai so chinh phuong."}
    };

    // Sub 1
    p.test_cases.push_back(make_tc(0, 1));
    p.test_cases.push_back(make_tc(1, 1));
    for (int i = 0; i < randint(2, 5); i++)
        p.test_cases.push_back(make_tc(randll(2, 1000000), 1));

    // Sub 2
    for (int i = 0; i < randint(3, 5); i++)
        p.test_cases.push_back(make_tc(randll(1000001, 1000000000000LL), 2));
    p.test_cases.push_back(make_tc(1000000000000LL, 2));
    return p;
}

// ============================================================
// GENERATOR 13: Đếm số lần xuất hiện (Count Occurrences)
// ============================================================
Problem gen_count_occurrences() {
    Problem p;
    p.id_str = make_id("count_occurrences");
    p.title = pick({"Đếm số lần xuất hiện", "Tần suất", "Count Occurrences", "Frequency"});
    p.category = "array";
    p.difficulty = pick({"medium", "medium", "hard"});
    p.description = pick({
        "Cho một mảng A gồm N số nguyên và Q truy vấn. Mỗi truy vấn yêu cầu đếm số lần xuất hiện của một số x trong mảng.",
        "Đếm tần suất xuất hiện của mỗi giá trị trong mảng theo yêu cầu.",
        "Cho mảng và các truy vấn, hãy cho biết mỗi số xuất hiện bao nhiêu lần."
    });
    p.input_format = "Dòng đầu tiên chứa hai số nguyên N và Q (1 <= N, Q <= 10^5).\nDòng thứ hai chứa N số nguyên A_i (-10^9 <= A_i <= 10^9).\nQ dòng tiếp theo, mỗi dòng chứa một số nguyên x.";
    p.output_format = "In ra Q dòng, mỗi dòng là số lần xuất hiện của x trong mảng.";
    p.constraints_str = "1 <= N, Q <= 10^5, -10^9 <= A_i, x <= 10^9";
    p.time_limit = 1.0;

    p.subtasks = {
        {1, 30, "N, Q <= 100, |A_i|, |x| <= 100"},
        {2, 70, "N, Q <= 10^5, |A_i|, |x| <= 10^9"}
    };

    auto make_arr_str = [](const vector<ll>& v) {
        ostringstream os;
        for (size_t i = 0; i < v.size(); i++) {
            if (i > 0) os << " ";
            os << v[i];
        }
        return os.str();
    };

    auto make_queries = [&](int qcnt, ll qlo, ll qhi, const unordered_map<ll, int>& cnt) -> vector<ll> {
        vector<ll> q;
        set<ll> unique_q;
        int target = min(qcnt, 8);
        while ((int)unique_q.size() < target) {
            ll v = randll(qlo, qhi);
            unique_q.insert(v);
        }
        q.assign(unique_q.begin(), unique_q.end());
        return q;
    };

    auto make_tc = [&](int n, const vector<ll>& queries, int sub) -> TestCase {
        vector<ll> arr(n);
        ll lo = -1e9, hi = 1e9;
        if (sub == 1) { lo = -100; hi = 100; }
        for (int i = 0; i < n; i++) arr[i] = randll(lo, hi);
        unordered_map<ll, int> cnt;
        for (ll v : arr) cnt[v]++;

        ostringstream in;
        in << n << " " << queries.size() << "\n";
        in << make_arr_str(arr) << "\n";
        ostringstream out;
        for (ll x : queries) {
            in << x << "\n";
            auto it = cnt.find(x);
            out << (it != cnt.end() ? it->second : 0) << "\n";
        }
        return {in.str(), out.str(), sub};
    };

    p.samples = {
        {"5 3\n1 2 2 3 2\n2\n1\n5", "3\n1\n0\n",
         "So 2 xuat hien 3 lan, so 1 xuat hien 1 lan, so 5 khong xuat hien."}
    };

    // Sub 1
    p.test_cases.push_back({"3 2\n1 1 1\n1\n0", "3\n0\n", 1});
    p.test_cases.push_back({"1 1\n42\n42", "1\n", 1});
    p.test_cases.push_back({"5 3\n0 0 0 0 0\n0\n1\n-1", "5\n0\n0\n", 1});
    p.test_cases.push_back(make_tc(randint(20, 100), {-50, 0, 50, randint(-100, 100)}, 1));

    // Sub 2
    for (int i = 0; i < randint(2, 4); i++)
        p.test_cases.push_back(make_tc(randint(1000, 100000),
            {randll(-1000000000, 1000000000), 0, randll(-1000000000, 1000000000)}, 2));
    return p;
}

// ============================================================
// GENERATOR 14: Số hoàn hảo (Perfect Number Check)
// ============================================================
Problem gen_perfect_number() {
    Problem p;
    p.id_str = make_id("perfect_number");
    p.title = pick({"Số hoàn hảo", "Perfect Number", "Số hoàn thiện", "Perfect Number Check"});
    p.category = "math";
    p.difficulty = pick({"medium", "medium", "hard"});
    p.description = pick({
        "Số hoàn hảo là số nguyên dương có tổng các ước số dương (không kể chính nó) bằng chính nó. Ví dụ: 6 = 1 + 2 + 3. Cho số nguyên dương N, hãy kiểm tra xem N có phải là số hoàn hảo hay không.",
        "Kiểm tra xem N có phải là số hoàn hảo (tổng ước bằng chính nó) không.",
        "Một số được gọi là hoàn hảo nếu tổng các ước thực sự của nó bằng chính nó. Hãy kiểm tra N."
    });
    p.input_format = "Một dòng duy nhất chứa số nguyên dương N (1 <= N <= 10^6).";
    p.output_format = "In ra \"YES\" nếu N là số hoàn hảo, ngược lại in ra \"NO\".";
    p.constraints_str = "1 <= N <= 10^6";
    p.time_limit = 1.0;

    p.subtasks = {
        {1, 40, "N <= 1000"},
        {2, 60, "N <= 10^6"}
    };

    auto is_perfect = [](int n) -> bool {
        if (n < 1) return false;
        int sum = 0;
        for (int i = 1; i * i <= n; i++) {
            if (n % i == 0) {
                if (i != n) sum += i;
                int j = n / i;
                if (j != i && j != n) sum += j;
            }
        }
        return sum == n;
    };

    auto make_tc = [&](int n, int sub) -> TestCase {
        return {to_string(n) + "\n", is_perfect(n) ? "YES\n" : "NO\n", sub};
    };

    p.samples = {
        {"6\n", "YES\n", "1 + 2 + 3 = 6, la so hoan hao."},
        {"8\n", "NO\n", "1 + 2 + 4 = 7 != 8, khong phai so hoan hao."}
    };

    // Sub 1
    p.test_cases.push_back(make_tc(1, 1));
    p.test_cases.push_back(make_tc(2, 1));
    p.test_cases.push_back(make_tc(6, 1));
    for (int i = 0; i < randint(2, 4); i++)
        p.test_cases.push_back(make_tc(randint(7, 1000), 1));

    // Sub 2
    for (int i = 0; i < randint(3, 5); i++)
        p.test_cases.push_back(make_tc(randint(1001, 1000000), 2));
    p.test_cases.push_back(make_tc(8128, 2));
    return p;
}

// ============================================================
// GENERATOR 15: Giai thừa có modulo (Factorial modulo)
// ============================================================
Problem gen_factorial_mod() {
    Problem p;
    p.id_str = make_id("factorial_mod");
    p.title = pick({"Giai thừa", "Tính N!", "Factorial", "Giai thừa modulo"});
    p.category = "math";
    p.difficulty = pick({"medium", "medium", "hard"});
    p.description = pick({
        "Cho số nguyên không âm N. Hãy tính N! modulo 10^9 + 7. Biết rằng 0! = 1.",
        "Tính giai thừa của N modulo 10^9+7.",
        "Viết chương trình tính N! % (10^9+7)."
    });
    p.input_format = "Một dòng duy nhất chứa số nguyên không âm N (0 <= N <= 10^6).";
    p.output_format = "In ra một số nguyên duy nhất là N! % (10^9 + 7).";
    p.constraints_str = "0 <= N <= 10^6";
    p.time_limit = 1.0;
    const int MOD = 1000000007;

    p.subtasks = {
        {1, 30, "N <= 10"},
        {2, 70, "N <= 10^6"}
    };

    auto fact_mod = [&](int n) -> int {
        ll res = 1;
        for (int i = 2; i <= n; i++) res = (res * i) % MOD;
        return (int)res;
    };

    auto make_tc = [&](int n, int sub) -> TestCase {
        return {to_string(n) + "\n", to_string(fact_mod(n)) + "\n", sub};
    };

    p.samples = {
        {"5\n", "120\n", "1 * 2 * 3 * 4 * 5 = 120."},
        {"0\n", "1\n", "0! = 1."}
    };

    // Sub 1
    p.test_cases.push_back(make_tc(0, 1));
    p.test_cases.push_back(make_tc(1, 1));
    for (int i = 0; i < randint(2, 5); i++)
        p.test_cases.push_back(make_tc(randint(2, 10), 1));

    // Sub 2
    for (int i = 0; i < randint(3, 6); i++)
        p.test_cases.push_back(make_tc(randint(100, 1000000), 2));
    p.test_cases.push_back(make_tc(1000000, 2));
    return p;
}

// ============================================================
// MAIN: Continuous random problem generation
// ============================================================
int main(int argc, char* argv[]) {
    bool single_mode = (argc > 1 && string(argv[1]) == "1");
    create_directory("problems");

    vector<pair<string, function<Problem()>>> generators = {
        {"Tổng hai số", gen_sum_two},
        {"Tổng dãy số", gen_array_sum},
        {"Số lớn nhất", gen_max_element},
        {"Đếm số chẵn", gen_count_even},
        {"Tổng chữ số", gen_digit_sum},
        {"GCD", gen_gcd},
        {"Kiểm tra nguyên tố", gen_prime_check},
        {"Căn bậc hai nguyên", gen_int_sqrt},
        {"Tìm kiếm nhị phân", gen_binary_search},
        {"Số Palindrome", gen_palindrome},
        {"Fibonacci", gen_fibonacci},
        {"Số chính phương", gen_perfect_square},
        {"Đếm số lần xuất hiện", gen_count_occurrences},
        {"Số hoàn hảo", gen_perfect_number},
        {"Giai thừa", gen_factorial_mod}
    };

    cout << "=== RANDOM PROBLEM GENERATOR ===" << endl;
    cout << "Generating problems continuously..." << endl;
    cout << "Press Ctrl+C to stop." << endl;
    cout << endl;

    int count = 0;
    while (true) {
        int idx = randint(0, (int)generators.size() - 1);
        auto& gen = generators[idx];

        cout << "[" << (count + 1) << "] Generating: " << gen.first << "... " << flush;

        Problem prob = gen.second();
        randomize_subtask_points(prob.subtasks);
        prob.cf_rating = Problem::difficulty_to_rating(prob.difficulty);
        string filename = "problems/" + prob.id_str + ".json";
        if (prob.save(filename)) {
            cout << "OK -> " << filename << endl;
            count++;
        } else {
            cout << "FAILED!" << endl;
        }

        if (single_mode) break;

        this_thread::sleep_for(chrono::milliseconds(1500));
    }

    cout << "\nDone. Generated " << count << " problems." << endl;
    return 0;
}
