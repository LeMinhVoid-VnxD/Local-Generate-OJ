#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>

namespace fs = std::filesystem;

std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
    if (!pipe) {
        return "Error: Failed to run command";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

bool check_gpp() {
    std::string output = exec("g++ --version 2>&1");
    return (output.find("g++") != std::string::npos);
}

bool check_dir(const std::string& dir) {
    return fs::exists(dir) && fs::is_directory(dir);
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::cout << "==========================================\n";
    std::cout << "   KIEM TRA DIEU KIEN CHAY LOCAL OJ\n";
    std::cout << "==========================================\n\n";

    // 1. Check g++
    std::cout << "[1] Kiem tra bo dich g++ (MSYS2)... ";
    if (check_gpp()) {
        std::cout << "OK ✅\n";
    } else {
        std::cout << "LOI ❌ (Vui long cai dat MSYS2 va add g++ vao PATH)\n";
    }

    // 2. Check folders
    std::cout << "[2] Kiem tra thu muc du lieu... ";
    bool folders_ok = true;
    if (!check_dir("static")) {
        std::cout << "THIEU 'static/' ";
        folders_ok = false;
    }
    if (!check_dir("problems")) {
        std::cout << "THIEU 'problems/' ";
        folders_ok = false;
    }
    if (folders_ok) std::cout << "OK ✅\n"; else std::cout << "LOI ❌\n";

    // 3. Check Internet (for AI)
    std::cout << "[3] Kiem tra ket noi Internet (cho AI)... ";
    // Try to ping a common server or just check if a simple network call works
    // Using a simple ping to google.com as a proxy for internet check
    std::string ping = exec("ping -n 1 google.com");
    if (ping.find("TTL=") != std::string::npos || ping.find("Reply from") != std::string::npos) {
        std::cout << "OK ✅\n";
    } else {
        std::cout << "KHONG CO INTERNET ⚠️ (AI se khong hoat dong)\n";
    }

    std::cout << "\n==========================================\n";
    if (check_gpp() && folders_ok) {
        std::cout << "KẾT LUẬN: Hệ thống đủ điều kiện để chạy! 🚀\n";
    } else {
        std::cout << "KẾT LUẬN: Bạn cần khắc phục các lỗi trên để chạy OJ.\n";
    }
    std::cout << "==========================================\n";
    
    std::cout << "\nNhan Enter de thoat...";
    std::cin.get();

    return 0;
}
