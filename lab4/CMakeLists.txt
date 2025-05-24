cmake_minimum_required(VERSION 3.31)
project(lab4)

set(CMAKE_CXX_STANDARD 20)

add_executable(server "server.cpp")
add_executable(client "client.cpp")
#add_executable(auto_client "auto_client.cpp")
#add_executable(study "study.cpp")