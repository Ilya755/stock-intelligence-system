cmake_minimum_required(VERSION 3.20)

project(StockIntelligenceSystem
    VERSION 1.0.0
    DESCRIPTION "High Performance Stock Intelligence System"
    LANGUAGES CXX
)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(Boost REQUIRED) 

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

FetchContent_Declare(
    njson
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.2
)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(njson spdlog)

file(GLOB_RECURSE PROJECT_SOURCES "src/*.cpp")
file(GLOB_RECURSE PROJECT_HEADERS "src/*.hpp")
list(REMOVE_ITEM PROJECT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_library(stock_lib STATIC
    ${PROJECT_SOURCES}
    ${PROJECT_HEADERS}
)

target_include_directories(stock_lib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(stock_lib PUBLIC 
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    PostgreSQL::PostgreSQL
    Threads::Threads
    Boost::boost
    OpenSSL::SSL
    OpenSSL::Crypto
)

add_executable(stock_app src/main.cpp)
target_link_libraries(stock_app PRIVATE stock_lib)