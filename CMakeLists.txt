cmake_minimum_required(VERSION 3.16)
project(clang14-obfuscator LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)
find_package(LLVM REQUIRED CONFIG 
    COMPONENTS 
        Support
        Core
        Tooling
        Frontend
        AST
        Rewrite
)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Found Clang ${Clang_PACKAGE_VERSION}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

set(SRC main.cpp)

add_executable(obfuscator ${SRC})

target_include_directories(obfuscator PRIVATE ${CLANG_INCLUDE_DIRS})
include_directories(${LLVM_INCLUDE_DIRS})
target_link_libraries(obfuscator
  PRIVATE
    ${LLVM_LIBRARIES}
    clangTooling
    clangBasic
    clangASTMatchers
    clangRewrite
    clangFrontend
    clangAST
    clangLex
    LLVM
)

add_compile_options(-Ofast)
