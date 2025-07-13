cmake_minimum_required(VERSION 3.10)
project(purity)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Найти Boost
find_package(Boost REQUIRED COMPONENTS system)

# Найти PostgreSQL клиент (libpq)
find_package(PostgreSQL REQUIRED)

# Найти libpqxx
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)

# Автоматический поиск исходников
file(GLOB_RECURSE COMMON_SOURCES CONFIGURE_DEPENDS src/common/*.cpp)
file(GLOB_RECURSE CLIENT_SOURCES CONFIGURE_DEPENDS src/client/*.cpp)
file(GLOB_RECURSE SERVER_SOURCES CONFIGURE_DEPENDS src/server/*.cpp)

# Общие include директории
set(COMMON_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/src/common
        ${CMAKE_SOURCE_DIR}/src/common/database
)

# Клиент
add_executable(client
        src/main_client.cpp
        ${CLIENT_SOURCES}
)
target_include_directories(client PRIVATE
        ${CLIENT_SOURCES}
        ${COMMON_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}
        ${Boost_INCLUDE_DIRS}
)
target_link_libraries(client PRIVATE
        ${Boost_LIBRARIES}
)

# Сервер
add_executable(server
        src/main_server.cpp
        ${SERVER_SOURCES}
        ${COMMON_INCLUDE_DIRS}
)
target_include_directories(server PRIVATE
        ${SERVER_SOURCES}
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
)
