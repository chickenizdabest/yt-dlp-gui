#include "webview/webview.h"
#include <windows.h>
#include <string>
#include <mutex>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <shlobj.h>
#include <vector>
#include <algorithm>
#include "httplib.h"
#include "resource.h"

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

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
std::string g_completionCallback = "";
std::mutex g_callbackMutex;

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

// Base64 encoding table
static const std::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

// Hàm encode Base64
std::string base64_encode(const std::string& input) {
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    size_t in_len = input.size();
    const unsigned char* bytes_to_encode = reinterpret_cast<const unsigned char*>(input.c_str());

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (j = 0; j < i + 1; j++)
            ret += base64_chars[char_array_4[j]];

        while (i++ < 3)
            ret += '=';
    }

    return ret;
}




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

// Hàm append text vào console HTML với Base64
void appendToConsole(const std::string& text, const std::string& type = "output") {
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

// Hàm xử lý message queue với Base64
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
            } else if (msg.type == "callback") {
                g_webview->eval(msg.text);
            } else {
                // Encode text thành Base64
                std::string encoded = base64_encode(msg.text);
                // Gọi hàm JS với base64 encoded string
                std::string js = "appendConsoleBase64(\"" + encoded + "\", \"" + msg.type + "\");";
                g_webview->eval(js);
            }
        }
    }
}
// Hàm thực thi lệnh Windows và capture output
void executeCommand(const std::string& command, const std::string& callbackId = "") {
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

    if (!callbackId.empty() && g_webview) {
        std::string jsCallback = "window.__commandComplete('" + callbackId + "', " +
                                std::to_string(exitCode) + ");";

        std::lock_guard<std::mutex> lock(g_queueMutex);
        g_messageQueue.push_back({jsCallback, "callback"});
    }

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

void openFolder(const std::string& path) {
    if (path.empty()) {
        appendToConsole("Error: Empty path", "error");
        return;
    }

    // Convert forward slashes to backslashes cho Windows
    std::string winPath = path;
    for (size_t i = 0; i < winPath.length(); i++) {
        if (winPath[i] == '/') {
            winPath[i] = '\\';
        }
    }

    std::cout << "Opening path: " << winPath << std::endl;

    // Mở folder với explorer
    std::string command = "explorer.exe \"" + winPath + "\"";
    int result = system(command.c_str());
}

std::string openFolderDialog() {
    std::string selectedPath = "";

    // Khởi tạo COM
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    // Sử dụng IFileDialog (modern, từ Vista trở lên)
    IFileDialog* pfd = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));

    if (SUCCEEDED(hr)) {
        // Set options để chỉ chọn thư mục
        DWORD dwOptions;
        hr = pfd->GetOptions(&dwOptions);
        if (SUCCEEDED(hr)) {
            pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);
        }

        // Set title
        pfd->SetTitle(L"Chọn thư mục");

        // Hiển thị dialog
        hr = pfd->Show(NULL);

        if (SUCCEEDED(hr)) {
            IShellItem* psi;
            hr = pfd->GetResult(&psi);

            if (SUCCEEDED(hr)) {
                PWSTR pszPath = nullptr;
                hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);

                if (SUCCEEDED(hr)) {
                    // Convert wide string to string
                    int size = WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, nullptr, 0, nullptr, nullptr);
                    if (size > 0) {
                        std::vector<char> buffer(size);
                        WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, buffer.data(), size, nullptr, nullptr);
                        selectedPath = std::string(buffer.data());
                    }
                    CoTaskMemFree(pszPath);
                }
                psi->Release();
            }
        }
        pfd->Release();
    }

    CoUninitialize();
    return selectedPath;
}

// Convert backslash thành forward slash cho JSON
std::string normalizePathForJson(const std::string& path) {
    std::string result = path;
    for (size_t i = 0; i < result.length(); i++) {
        if (result[i] == '\\') {
            result[i] = '/';
        }
    }
    return result;
}

// Escape string để trả về JSON an toàn
std::string escapeJsonString(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

// Callback function cho bind - dùng C API
void selectFolderCallback(const char* seq, const char* req, void* arg) {
    webview_t w = (webview_t)arg;

    std::string folderPath = openFolderDialog();
    std::string result;

    if (!folderPath.empty()) {
        // Normalize path (convert \ thành /)
        std::string normalizedPath = normalizePathForJson(folderPath);

        // Build JSON response
        std::ostringstream json;
        json << "{\"success\": true, \"path\": \""
             << escapeJsonString(normalizedPath)
             << "\", \"originalPath\": \""
             << escapeJsonString(folderPath) << "\"}";

        result = json.str();
    } else {
        result = "{\"success\": false, \"path\": \"\", \"originalPath\": \"\"}";
    }

    // Return result to JavaScript
    webview_return(w, seq, 0, result.c_str());
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

    w.bind("openFolder", [](std::string seq, std::string req, void* arg) {
     auto w = static_cast<webview::webview*>(arg);

     std::cout << "Raw request: " << req << std::endl;

     std::string path;

     // Parse JSON array: ["C:/path/to/folder"]
     // Tìm chuỗi giữa dấu ngoặc kép đầu tiên
     size_t firstQuote = req.find("\"");
     if (firstQuote != std::string::npos) {
         size_t secondQuote = req.find("\"", firstQuote + 1);
         if (secondQuote != std::string::npos) {
             path = req.substr(firstQuote + 1, secondQuote - firstQuote - 1);

             // Unescape các ký tự đặc biệt từ JSON
             std::string unescaped;
             for (size_t i = 0; i < path.length(); i++) {
                 if (path[i] == '\\' && i + 1 < path.length()) {
                     char next = path[i + 1];
                     if (next == '\\' || next == '/' || next == '"') {
                         unescaped += next;
                         i++; // Skip next character
                         continue;
                     }
                 }
                 unescaped += path[i];
             }
             path = unescaped;
         }
     }

     std::cout << "Parsed path: " << path << std::endl;

     if (!path.empty()) {
         openFolder(path);
         w->resolve(seq, 0, "true");
     } else {
         w->resolve(seq, 1, "\"Invalid path\"");
         appendToConsole("Error: Cannot parse folder path from: " + req, "error");
     }
 }, &w);

    w.bind("selectFolder", [](const std::string& args) -> std::string {
        std::string folderPath = openFolderDialog();

        if (!folderPath.empty()) {
            std::string normalizedPath = normalizePathForJson(folderPath);

            // Trả về plain object (webview sẽ tự serialize)
            // Chỉ cần escape các ký tự đặc biệt trong string
            std::ostringstream result;
            result << "{"
                   << "\"success\":true,"
                   << "\"path\":\"" << escapeJsonString(normalizedPath) << "\","
                   << "\"originalPath\":\"" << escapeJsonString(folderPath) << "\""
                   << "}";
            return result.str();
        } else {
            return "{\"success\":false,\"path\":\"\",\"originalPath\":\"\"}";
        }
    });



    // Bind callback từ JavaScript
    w.bind("runCommand", [](std::string jsonArgs) -> std::string {
    std::string command;
    std::string callbackId;

    // Parse JSON để lấy command và callbackId
    size_t start = jsonArgs.find("\"");
    if (start != std::string::npos) {
        size_t end = jsonArgs.find("\"", start + 1);
        if (end != std::string::npos) {
            command = jsonArgs.substr(start + 1, end - start - 1);

            // Tìm callbackId (tham số thứ 2)
            start = jsonArgs.find("\"", end + 1);
            if (start != std::string::npos) {
                end = jsonArgs.find("\"", start + 1);
                if (end != std::string::npos) {
                    callbackId = jsonArgs.substr(start + 1, end - start - 1);
                }
            }
        }
    }

    if (command.empty()) {
        appendToConsole("Error: Cannot parse command", "error");
        return "{\"status\":\"error\"}";
    }

    std::thread([command, callbackId]() {
        executeCommand(command, callbackId);
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