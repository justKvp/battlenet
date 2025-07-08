cmake_minimum_required(VERSION 3.15)
project(purity)

# Установка типа сборки по умолчанию, если не указан
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    message(STATUS "Build type not specified, defaulting to Release")
endif()

# Настройка стандартов C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Автоматический поиск исходных файлов
file(GLOB_RECURSE SOURCE_FILES "src/*.cpp")
file(GLOB_RECURSE HEADER_FILES "src/*.hpp")
# Поиск Boost
find_package(Boost REQUIRED COMPONENTS system)

# Общие исходные файлы
set(COMMON_HEADERS src/common/common.hpp)

# Клиент
add_executable(client
        src/client/client.cpp
        ${COMMON_HEADERS}
)
target_include_directories(client PRIVATE src/common)
target_link_libraries(client
        PRIVATE
        Boost::boost
        Boost::system
        pthread
)
target_compile_options(client PRIVATE -Wall -Wextra -Wpedantic)

# Сервер
add_executable(server
        src/server/server.cpp
        ${COMMON_HEADERS}
)
target_include_directories(server PRIVATE src/common)
target_link_libraries(server
        PRIVATE
        Boost::boost
        Boost::system
        pthread
)
target_compile_options(server PRIVATE -Wall -Wextra -Wpedantic)

# Установка в bin/
install(TARGETS client server DESTINATION bin)