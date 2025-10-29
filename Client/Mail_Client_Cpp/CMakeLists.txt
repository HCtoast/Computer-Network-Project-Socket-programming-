cmake_minimum_required(VERSION 3.21)
project(Mail_Client LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(/utf-8)     # 소스·실행 문자셋을 UTF-8로 고정

file(GLOB APP_SOURCES CONFIGURE_DEPENDS
	# "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
	# "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
	# "${CMAKE_CURRENT_SOURCE_DIR}/include/*.cpp"
	# "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Mail_Client.cpp"
)

add_executable(Server ${APP_SOURCES})
