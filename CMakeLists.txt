cmake_minimum_required(VERSION 3.10)
project(purity)

set(CMAKE_CXX_STANDARD 17)

find_package(Boost REQUIRED COMPONENTS system)

# Клиент
add_executable(client
        src/client_main.cpp
        src/async_client.cpp
)
target_include_directories(client PRIVATE include)
target_link_libraries(client PRIVATE Boost::system pthread)

# Сервер
add_executable(server
        src/server_main.cpp
        src/async_server.cpp
)
target_compile_options(server PRIVATE -pthread)
target_include_directories(server PRIVATE include)
target_link_libraries(server PRIVATE Boost::system pthread)