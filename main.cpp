#include "webview/webview.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include "httplib.h"
#include "resource.h"


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
    httplib::Server server;

    // Route: trả về nội dung index.html
    server.Get("/", [](const httplib::Request &, httplib::Response &res) {
        std::string html = readFile("ui/index.html");
        res.set_content(html, "text/html; charset=UTF-8");
    });

    // Route chung cho file tĩnh (CSS, JS, ảnh,…) (bỏ đi nếu nhúng css và js vào html)
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



    webview::webview w(true, nullptr);
    w.set_title("YouTube Downloader");
    w.set_size(1600, 900, WEBVIEW_HINT_NONE);
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

    server.stop();
    server_thread.join();

    return 0;
}
