#include "webview/webview.h"
#include <windows.h>
#include <string>
#include <mutex>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include "httplib.h"
#include "resource.h"

// Title Console
void title() {
    std::string red   = "\033[1;31m";   // đỏ đậm
    std::string gray  = "\033[0;37m";   // xám nhạt
    std::string reset = "\033[0m";      // reset về màu mặc định
    std::cout << "=========================================================================================\n";
    std::cout << red;
    std::cout << " __     __         _         _          \n";
    std::cout << " \\ \\   / /        | |       | |         \n";
    std::cout << "  \\ \\_/ /__  _   _| |_ _   _| |__   ___ \n";
    std::cout << "   \\   / _ \\| | | | __| | | | '_ \\ / _ \\\n";
    std::cout << "    | | (_) | |_| | |_| |_| | |_) |  __/\n";
    std::cout << "    |_|\\___/ \\__,_|\\__|\\__,_|_.__/ \\___|\n";
    std::cout << gray;  // Phần "Downloader"
    std::cout << "                              _____                      _                 _                      \n";
    std::cout << "                             |  __ \\                    | |               | |                     \n";
    std::cout << "                             | |  | | _____      ___ __ | | ___   __ _  __| | ___ _ __            \n";
    std::cout << "                             | |  | |/ _ \\ \\ /\\ / / '_ \\| |/ _ \\ / _` |/ _` |/ _ \\ '__|           \n";
    std::cout << "                             | |__| | (_) \\ V  V /| | | | | (_) | (_| | (_| |  __/ |              \n";
    std::cout << "                             |_____/ \\___/ \\_/\\_/ |_| |_|_|\\___/ \\__,_|\\__,_|\\___|_|              \n";
    std::cout << "                                                                                                   \n";
    std::cout << reset;
    std::cout << "=========================================================================================\n";
}



// Global webview pointer để có thể gọi từ thread khác
webview::webview* g_webview = nullptr;
std::mutex g_mutex;

// Global process handle để có thể terminate
HANDLE g_currentProcess = NULL;
DWORD g_currentProcessId = 0;
std::mutex g_processMutex;

// Message queue để gửi messages từ worker thread về main thread
struct ConsoleMessage {
    std::string text;
    std::string type;
};
std::vector<ConsoleMessage> g_messageQueue;
std::mutex g_queueMutex;

// Hàm escape JSON string
std::string escapeJson(const std::string& input) {
    std::string output;
    for (char c : input) {
        switch (c) {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (c < 32) {
                    char buf[8];
                    sprintf(buf, "\\u%04x", c);
                    output += buf;
                } else {
                    output += c;
                }
        }
    }
    return output;
}

// Hàm append text vào console HTML
void appendToConsole(const std::string& text, const std::string& type = "output") {
    // Queue message thay vì gọi trực tiếp
    std::lock_guard<std::mutex> lock(g_queueMutex);
    g_messageQueue.push_back({text, type});
}

// Hàm enable/disable stop button
void setStopButtonEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(g_queueMutex);
    if (enabled) {
        g_messageQueue.push_back({"__ENABLE_STOP__", "control"});
    } else {
        g_messageQueue.push_back({"__DISABLE_STOP__", "control"});
    }
}

// Hàm xử lý message queue (gọi từ main thread)
void processMessageQueue() {
    std::vector<ConsoleMessage> messages;

    {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        messages = g_messageQueue;
        g_messageQueue.clear();
    }

    if (g_webview && !messages.empty()) {
        for (const auto& msg : messages) {
            if (msg.text == "__ENABLE_STOP__") {
                g_webview->eval("document.getElementById('stopBtn').disabled = false;");
            } else if (msg.text == "__DISABLE_STOP__") {
                g_webview->eval("document.getElementById('stopBtn').disabled = true;");
            } else {
                std::string escaped = escapeJson(msg.text);
                std::string js = "appendConsole(\"" + escaped + "\", \"" + msg.type + "\");";
                g_webview->eval(js);
            }
        }
    }
}

// Hàm thực thi lệnh Windows và capture output
void executeCommand(const std::string& command) {
    appendToConsole("=== Starting command execution ===", "info");
    appendToConsole("Command: " + command, "info");

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hStdOutRead, hStdOutWrite;

    // Tạo pipe cho stdout
    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0)) {
        appendToConsole("Error: Cannot create stdout pipe - Code: " + std::to_string(GetLastError()), "error");
        return;
    }

    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

    PROCESS_INFORMATION pi;
    STARTUPINFOA si;
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&si, sizeof(STARTUPINFOA));
    si.cb = sizeof(STARTUPINFOA);
    si.hStdError = hStdOutWrite;  // Redirect stderr to stdout
    si.hStdOutput = hStdOutWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    // Tạo command line với cmd.exe
    std::string cmdLine = "cmd.exe /c " + command;
    char* cmdLineBuf = new char[cmdLine.length() + 1];
    strcpy(cmdLineBuf, cmdLine.c_str());

    appendToConsole("$ " + command, "command");

    // Tạo process
    BOOL success = CreateProcessA(
        NULL,
        cmdLineBuf,
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP,  // Thêm CREATE_NEW_PROCESS_GROUP để có thể gửi Ctrl+C
        NULL,
        NULL,
        &si,
        &pi
    );

    delete[] cmdLineBuf;
    CloseHandle(hStdOutWrite);  // Đóng write end ngay sau khi tạo process

    if (!success) {
        DWORD error = GetLastError();
        appendToConsole("Error: Cannot create process - Code: " + std::to_string(error), "error");
        CloseHandle(hStdOutRead);
        return;
    }

    // Lưu process handle để có thể terminate
    {
        std::lock_guard<std::mutex> lock(g_processMutex);
        g_currentProcess = pi.hProcess;
        g_currentProcessId = pi.dwProcessId;
    }

    appendToConsole("Process created successfully, PID: " + std::to_string(pi.dwProcessId), "info");

    // Enable stop button
    setStopButtonEnabled(true);

    // Đọc output tuần tự (blocking)
    char buffer[4096];
    DWORD bytesRead;
    std::string output;

    while (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        output += std::string(buffer);
        // Append từng chunk để hiển thị real-time
        appendToConsole(std::string(buffer), "output");
    }

    CloseHandle(hStdOutRead);

    // Đợi process kết thúc
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    // Clear process handle
    {
        std::lock_guard<std::mutex> lock(g_processMutex);
        g_currentProcess = NULL;
        g_currentProcessId = 0;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Disable stop button
    setStopButtonEnabled(false);

    std::stringstream ss;
    ss << "Process exited with code: " + std::to_string(exitCode);
    appendToConsole(ss.str(), "info");
    appendToConsole("=== Command execution finished ===", "info");
}

// Hàm terminate process đang chạy
void stopCurrentProcess() {
    std::lock_guard<std::mutex> lock(g_processMutex);

    if (g_currentProcess != NULL && g_currentProcessId != 0) {
        appendToConsole("Stopping process tree (PID: " + std::to_string(g_currentProcessId) + ")...", "info");

        // Dùng taskkill để kill toàn bộ process tree bao gồm child processes
        std::string killCmd = "taskkill /F /T /PID " + std::to_string(g_currentProcessId);
        system(killCmd.c_str());

        // Đợi process kết thúc
        WaitForSingleObject(g_currentProcess, 2000);
        appendToConsole("Process tree terminated.", "info");
    } else {
        appendToConsole("No running process to stop.", "info");
    }
}


std::string readFile(const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Khong the mo file: " << path << std::endl;
        return "<h1>Loi: khong the mo file HTML!</h1>";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}


int main() {
    title();
    httplib::Server server;

    // Route: trả về nội dung index.html
    server.Get("/", [](const httplib::Request &, httplib::Response &res) {
        std::string html = readFile("ui/index.html");
        res.set_content(html, "text/html; charset=UTF-8");
    });

    // Route chung cho file tĩnh (CSS, JS, ảnh,…)
    server.Get(R"(/(.*))", [](const httplib::Request &req, httplib::Response &res) {
        std::string path = "ui/" + req.matches[1].str();

        std::string content = readFile(path);
        if (content.empty()) {
            res.status = 404;
            res.set_content("404 Not Found", "text/plain");
            return;
        }

        // Đoán loại MIME
        std::string mime = "text/plain";
        if (path.ends_with(".html")) mime = "text/html";
        else if (path.ends_with(".css")) mime = "text/css";
        else if (path.ends_with(".js")) mime = "application/javascript";
        else if (path.ends_with(".png")) mime = "image/png";
        else if (path.ends_with(".jpg") || path.ends_with(".jpeg")) mime = "image/jpeg";
        else if (path.ends_with(".ico")) mime = "image/x-icon";
        else if (path.ends_with(".otf")) mime = "font/otf";
        else if (path.ends_with(".woff")) mime = "font/woff";
        else if (path.ends_with(".woff2")) mime = "font/woff2";

        // Thêm CORS header
        res.set_header("Access-Control-Allow-Origin", "*");

        res.set_content(content, mime.c_str());
    });

    // Chạy server trên luồng riêng
    std::thread server_thread([&server]() {
        std::cout << "Server dang chay tai http://localhost:8080\n";
        server.listen("localhost", 8080);
    });

    std::cout << "\nDung tat cua so nay nha :D\n";

    webview::webview w(true, nullptr);
    g_webview = &w;  // <-- SET g_webview NGAY SAU KHI TẠO

    w.set_title("YouTube Downloader");
    w.set_size(1600, 900, WEBVIEW_HINT_NONE);

    // Bind callback từ JavaScript
    w.bind("runCommand", [](std::string jsonArgs) -> std::string {
        // Parse JSON array - webview gửi arguments dưới dạng ["command"]
        std::string command;

        // Simple JSON parsing để lấy string trong array
        size_t start = jsonArgs.find("\"");
        if (start != std::string::npos) {
            size_t end = jsonArgs.find("\"", start + 1);
            if (end != std::string::npos) {
                command = jsonArgs.substr(start + 1, end - start - 1);
            }
        }

        if (command.empty()) {
            appendToConsole("Error: Cannot parse command from: " + jsonArgs, "error");
            return "{\"status\":\"error\",\"message\":\"Invalid command\"}";
        }

        appendToConsole("Parsed command: " + command, "info");

        // Chạy trong thread riêng để không block UI
        std::thread([command]() {
            executeCommand(command);
        }).detach();

        return "{\"status\":\"ok\"}";
    });

    w.bind("stopCurrentProcess", [](std::string) -> std::string {
        stopCurrentProcess();
        return "{\"status\":\"ok\"}";
    });

    // Init script để debug
    w.init(R"(
        console.log('Webview initialized');
        console.log('runCommand type:', typeof runCommand);
    )");

    // Dispatch loop để xử lý message queue
    std::thread dispatchThread([&w]() {
        while (g_webview) {
            w.dispatch([&w]() {
                processMessageQueue();
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    w.navigate("http://localhost:8080");

    auto hwnd_result = w.window();
    if (hwnd_result.has_value()) {
        HWND hwnd = static_cast<HWND>(hwnd_result.value());

        HICON hIconBig   = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
        HICON hIconSmall = (HICON)LoadImage(GetModuleHandle(NULL),
                                            MAKEINTRESOURCE(IDI_ICON1),
                                            IMAGE_ICON,
                                            16, 16,
                                            LR_DEFAULTCOLOR);

        SendMessage(hwnd, WM_SETICON, ICON_BIG,   (LPARAM)hIconBig);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
    }

    w.run();

    g_webview = nullptr;
    dispatchThread.join();

    server.stop();
    server_thread.join();

    return 0;
}