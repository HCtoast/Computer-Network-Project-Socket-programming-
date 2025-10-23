cmake_minimum_required(VERSION 3.21)
project(Server LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB APP_SOURCES CONFIGURE_DEPENDS
	# "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
	# "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
	# "${CMAKE_CURRENT_SOURCE_DIR}/include/*.cpp"
	# "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Server.cpp"
)

add_executable(Server ${APP_SOURCES})
