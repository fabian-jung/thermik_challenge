cmake_minimum_required(VERSION 3.0)

project(thermik_challenge)

add_executable(thermik_challenge main.cpp)

install(TARGETS thermik_challenge RUNTIME DESTINATION bin)
