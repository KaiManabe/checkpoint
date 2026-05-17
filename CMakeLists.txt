cmake_minimum_required(VERSION 3.20)

project(checkpoint LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(checkpoint
    src/arguments.cpp
    src/checkpoint_hash.cpp
    src/checkpoint_ignore.cpp
    src/checkpoint_layout.cpp
    src/checkpoint_repository.cpp
    src/checkpoint_snapshot.cpp
    src/main.cpp
)

target_include_directories(checkpoint PRIVATE include)

if(MSVC)
    target_compile_options(checkpoint PRIVATE /W4)
    set_property(TARGET checkpoint PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
else()
    target_compile_options(checkpoint PRIVATE -Wall -Wextra)
endif()

set_target_properties(checkpoint PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/dist"
)

install(TARGETS checkpoint RUNTIME DESTINATION bin)
