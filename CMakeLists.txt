cmake_minimum_required(VERSION 3.16)
# C 与 C++ 都要启用：LLVM 的 CMake 配置脚本（FindFFI / FindTerminfo）会试编译
# 一小段代码来探测可选依赖，构建目录里只要有 CMAKE_C_COMPILER（IDE 按 kit
# 配置时会传入，并留在缓存里）它就走 C 分支——项目未启用 C 时报
# “check_source_compiles: C: needs to be enabled before use”，配置失败。
project(cppobf LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 使用 LLVM/Clang 18（构建时指定：
#   cmake -B build -DLLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm \
#                  -DClang_DIR=/usr/lib/cmake/clang-18 ）
# 链接动态库 libLLVM-18 / libclang-cpp，避免静态库对 zstd 等的依赖
set(LLVM_LINK_LLVM_DYLIB ON)
set(CLANG_LINK_CLANG_DYLIB ON)
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM include dirs: ${LLVM_INCLUDE_DIRS}")

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
include_directories(SYSTEM ${CLANG_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

add_executable(cppobf
    main.cpp
    obf/Support.hpp
    obf/RenamePass.hpp
    obf/MacroObfuscator.hpp
    obf/Passes.hpp
)

target_link_libraries(cppobf
    PRIVATE
        clang-cpp
        LLVM
)
