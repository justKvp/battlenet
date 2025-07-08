# purity

Сборка (релиз / дебаг)

### 4.1. Release

mkdir -p build/Release && cd build/Release

conan install ../.. --build=missing -s build_type=Release

cmake ../.. -DCMAKE_TOOLCHAIN_FILE=generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release

cmake --build .