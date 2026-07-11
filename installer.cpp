// ============================================================
// Compiler/IDE Auto-Downloader
// Compile 3 versions:
//   g++ -std=c++17 -O2 -o download_msys2.exe installer.cpp -lwinhttp -static -DINSTALL_MSYS2
//   g++ -std=c++17 -O2 -o download_devcpp.exe installer.cpp -lwinhttp -static -DINSTALL_DEVCPP
//   g++ -std=c++17 -O2 -o download_codeblocks.exe installer.cpp -lwinhttp -static -DINSTALL_CODEBLOCKS
// ============================================================

#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <chrono>

#pragma comment(lib, "winhttp.lib")

static std::string download_file(const std::string& host, int port, const std::string& path,
                                  const std::string& extra_headers, int timeout_ms = 60000) {
    HINTERNET hSession = WinHttpOpen(L"Downloader/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
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

    // Get content length
    DWORD content_len = 0;
    DWORD size = sizeof(content_len);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                        nullptr, &content_len, &size, nullptr);

    std::string result;
    char buf[65536];
    DWORD read = 0;
    DWORD total_read = 0;
    int last_pct = -1;

    while (WinHttpReadData(hRequest, buf, sizeof(buf), &read) && read > 0) {
        result.append(buf, read);
        total_read += read;
        if (content_len > 0) {
            int pct = (int)(total_read * 100 / content_len);
            if (pct != last_pct) {
                std::cout << "\r  Downloading... " << pct << "% (" << total_read / 1024 << " KB / " << content_len / 1024 << " KB)   " << std::flush;
                last_pct = pct;
            }
        } else {
            std::cout << "\r  Downloaded " << total_read / 1024 << " KB   " << std::flush;
        }
        read = 0;
    }
    std::cout << "\n";

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

static bool save_file(const std::string& path, const std::string& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(data.data(), data.size());
    return true;
}

static std::string get_filename() {
#if defined(INSTALL_MSYS2)
    return "msys2-x86_64.exe";
#elif defined(INSTALL_DEVCPP)
    return "Dev-Cpp_Setup.exe";
#elif defined(INSTALL_CODEBLOCKS)
    return "codeblocks_setup.exe";
#else
    return "download.exe";
#endif
}

static std::string get_display_name() {
#if defined(INSTALL_MSYS2)
    return "MSYS2 (MinGW-w64 GCC)";
#elif defined(INSTALL_DEVCPP)
    return "Embarcadero Dev-C++";
#elif defined(INSTALL_CODEBLOCKS)
    return "Code::Blocks (MinGW)";
#else
    return "Unknown";
#endif
}

struct DownloadTarget {
    std::string host;
    int port;
    std::string path;
    std::string headers;
};

static DownloadTarget get_target() {
#if defined(INSTALL_MSYS2)
    // MSYS2 latest from GitHub
    return {"github.com", 443,
        "/msys2/msys2-installer/releases/latest/download/msys2-x86_64.exe",
        "User-Agent: OJ-Downloader/1.0\r\n"};
#elif defined(INSTALL_DEVCPP)
    // Embarcadero Dev-C++ latest from GitHub
    return {"github.com", 443,
        "/Embarcadero/Dev-Cpp/releases/latest/download/Embarcadero_Dev-Cpp_6.3_TDM-GCC_9.2_Setup.exe",
        "User-Agent: OJ-Downloader/1.0\r\n"};
#elif defined(INSTALL_CODEBLOCKS)
    // CodeBlocks with MinGW from SourceForge
    return {"downloads.sourceforge.net", 443,
        "/project/codeblocks/Binaries/25.03/Windows/codeblocks-25.03mingw_fortran_setup.exe",
        "User-Agent: OJ-Downloader/1.0\r\n"};
#else
    return {"", 0, "", ""};
#endif
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::string name = get_display_name();
    std::string filename = get_filename();

    std::cout << "========================================\n";
    std::cout << "  Auto Download: " << name << "\n";
    std::cout << "========================================\n\n";

    DownloadTarget target = get_target();
    if (target.host.empty()) {
        std::cerr << "ERROR: No target defined. Recompile with a -D flag.\n";
        std::cerr << "  -DINSTALL_MSYS2\n  -DINSTALL_DEVCPP\n  -DINSTALL_CODEBLOCKS\n";
        return 1;
    }

    std::cout << "[1] Downloading " << name << " installer...\n";
    std::cout << "    URL: " << target.host << target.path << "\n\n";

    auto start = std::chrono::steady_clock::now();
    std::string data = download_file(target.host, target.port, target.path, target.headers);
    if (data.empty()) {
        std::cerr << "\n[ERROR] Download failed! Kiem tra ket noi mang.\n";
        std::cerr << "  Co the truy cap thu cong tai:\n";
        std::cerr << "  https://" << target.host << target.path << "\n";
        system("pause");
        return 1;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start).count();

    std::cout << "\n[2] Download hoan tat! " << data.size() / 1024 / 1024 << " MB (" << elapsed << "s)\n";
    std::cout << "[3] Luu file: " << filename << " ... ";

    if (!save_file(filename, data)) {
        std::cerr << "LOI! Khong the ghi file.\n";
        system("pause");
        return 1;
    }
    std::cout << "OK\n\n";

    std::cout << "[4] Mo trinh cai dat? (y/N): ";
    std::string answer;
    std::getline(std::cin, answer);

    if (!answer.empty() && (answer[0] == 'y' || answer[0] == 'Y')) {
        std::cout << "  Dang mo " << filename << " ...\n";
        ShellExecuteA(nullptr, "open", filename.c_str(), nullptr, nullptr, SW_SHOW);
    }

#if defined(INSTALL_MSYS2)
    std::cout << "\n--- Sau khi cai dat MSYS2 ---\n";
    std::cout << "1. Mo MSYS2 UCRT64 tu Start Menu\n";
    std::cout << "2. Chay: pacman -S mingw-w64-ucrt-x86_64-gcc --noconfirm\n";
    std::cout << "3. Them vao PATH:\n";
    std::cout << "   C:\\msys64\\ucrt64\\bin\n";
    std::cout << "4. Kiem tra: g++ --version\n";
    std::cout << "5. Chay OJ Server: oj_server.exe\n";
#elif defined(INSTALL_DEVCPP)
    std::cout << "\n--- Sau khi cai dat Dev-C++ ---\n";
    std::cout << "Duong dan compiler (dung trong Settings OJ):\n";
    std::cout << "  C:\\Program Files (x86)\\Dev-Cpp\\MinGW64\\bin\\g++.exe\n";
    std::cout << "Hoac:\n";
    std::cout << "  C:\\Program Files\\Embarcadero\\Dev-Cpp\\MinGW64\\bin\\g++.exe\n";
#elif defined(INSTALL_CODEBLOCKS)
    std::cout << "\n--- Sau khi cai dat Code::Blocks ---\n";
    std::cout << "Duong dan compiler (dung trong Settings OJ):\n";
    std::cout << "  C:\\Program Files\\CodeBlocks\\MinGW\\bin\\g++.exe\n";
#endif

    std::cout << "\nNhan Enter de thoat...";
    std::cin.get();
    return 0;
}
