cmake_minimum_required(VERSION 4.0)
project(CPP_GUI)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_RC_FLAGS "")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")


include(FetchContent)

FetchContent_Declare(
        webview
        GIT_REPOSITORY https://github.com/webview/webview
        GIT_TAG 0.12.0)
FetchContent_MakeAvailable(webview)
FetchContent_Declare(
        httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.15.3
)
FetchContent_MakeAvailable(httplib)

if (WIN32)
    set(WIN_ICON appicon.rc main.cpp)
endif()

add_executable(CPP_GUI main.cpp ${WIN_ICON}
        main.cpp)


target_link_libraries(CPP_GUI PRIVATE webview::core_static)
target_link_libraries(CPP_GUI PRIVATE httplib::httplib)
