cmake_minimum_required(VERSION 3.0)

project(thermik_challenge)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-long-long -pedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -fsanitize=address")

find_package(units REQUIRED)

file(GLOB_RECURSE libsources
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
add_library(lib STATIC ${libsources})
target_link_libraries(lib units::units)

include_directories(include)
add_executable(thermik_challenge bin/main.cpp)
target_link_libraries(thermik_challenge lib)

install(TARGETS thermik_challenge RUNTIME DESTINATION bin)
