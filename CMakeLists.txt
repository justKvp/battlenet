cmake_minimum_required(VERSION 3.10)
project(purity)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Указываем Boost
find_package(Boost REQUIRED COMPONENTS system)
include_directories(${Boost_INCLUDE_DIRS})

# Клиент
add_executable(client src/client/Client.cpp)
target_include_directories(client PRIVATE
        src/common
        src/client)
target_link_libraries(client ${Boost_LIBRARIES})

# Сервер
add_executable(server src/server/Server.cpp)
target_include_directories(server PRIVATE
        src/common
        src/server)
target_link_libraries(server ${Boost_LIBRARIES})