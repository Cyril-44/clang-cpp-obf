cmake_minimum_required(VERSION 3.13)
project(cppobf LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM include dirs: ${LLVM_INCLUDE_DIRS}")

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# 强制 Release 模式使用 -O2
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -Wall -fsanitize=address")

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -O3 -Ofast")

set(CMAKE_EXE_LINKER_FLAGS "")

add_executable(cppobf main.cpp)

target_include_directories(cppobf PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
# 链接 Clang/LLVM 组件 —— 包含 clangIndex
target_link_libraries(cppobf
  PRIVATE
    clangTooling
    clangBasic
    clangASTMatchers
    clangRewrite
    clangIndex
)
# 有时需要显式 link LLVM 库列表（可选，CMake 的 find_package 通常处理）
# llvm_map_components_to_libnames(llvm_libs support core irreader)
# target_link_libraries(cppobf PRIVATE ${llvm_libs})
