cmake_minimum_required(VERSION 3.21)
project(WebProxyServer LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(/utf-8)

file(GLOB APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/WebProxyServer.cpp"
)

add_executable(WebProxyServer ${APP_SOURCES})

target_link_libraries(WebProxyServer ws2_32)