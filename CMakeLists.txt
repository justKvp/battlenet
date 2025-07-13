cmake_minimum_required(VERSION 3.10)
project(BattleNet)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === Boost ===
find_package(Boost REQUIRED COMPONENTS system)

# === PostgreSQL libpq ===
find_package(PostgreSQL REQUIRED)

# === libpqxx ===
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)

# === spdlog ===
set(SPDLOG_DIR ${CMAKE_SOURCE_DIR}/deps/spdlog-1.15.3)
add_subdirectory(${SPDLOG_DIR})

# === Исходники ===
file(GLOB_RECURSE COMMON_SOURCES CONFIGURE_DEPENDS src/common/*.cpp)
file(GLOB_RECURSE CLIENT_SOURCES CONFIGURE_DEPENDS src/client/*.cpp)
file(GLOB_RECURSE SERVER_SOURCES CONFIGURE_DEPENDS src/server/*.cpp)

# === Include dirs ===
set(COMMON_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/src/common
        ${CMAKE_SOURCE_DIR}/src/common/database
)

# === Клиент ===
add_executable(client
        src/main_client.cpp
        ${CLIENT_SOURCES}
        ${COMMON_SOURCES}
)

target_include_directories(client PRIVATE
        ${COMMON_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}
        ${Boost_INCLUDE_DIRS}
)

target_link_libraries(client PRIVATE
        ${Boost_LIBRARIES}
        spdlog::spdlog
)

# === Сервер ===
add_executable(server
        src/main_server.cpp
        ${SERVER_SOURCES}
        ${COMMON_SOURCES}
)

target_include_directories(server PRIVATE
        ${COMMON_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}
        ${Boost_INCLUDE_DIRS}
        ${PostgreSQL_INCLUDE_DIRS}
        ${PQXX_INCLUDE_DIRS}
)

target_link_libraries(server PRIVATE
        ${Boost_LIBRARIES}
        ${PostgreSQL_LIBRARIES}
        ${PQXX_LIBRARIES}
        spdlog::spdlog
)
