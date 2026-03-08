cmake_minimum_required(VERSION 3.16)

project(hoymiles-cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile database for clang-tidy / clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Prefer modern protobuf config package
find_package(Protobuf CONFIG REQUIRED)

# Bring in protobuf_generate_cpp helper macro
include(${Protobuf_DIR}/protobuf-module.cmake OPTIONAL)
if(NOT COMMAND protobuf_generate_cpp)
    include(FindProtobuf)
endif()

# Find clang tools
find_program(CLANG_FORMAT_EXE NAMES clang-format
    HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin)

find_program(CLANG_TIDY_EXE NAMES clang-tidy
    HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin)

# Collect proto files
file(GLOB_RECURSE PROTO_FILES
     "${CMAKE_CURRENT_SOURCE_DIR}/protobuf/*.proto")

# Generate .pb.cc/.pb.h
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Collect project source/header files to format/lint
file(GLOB_RECURSE PROJECT_FORMAT_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

file(GLOB_RECURSE PROJECT_TIDY_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# Build executable
add_executable(hoymiles_cpp
    src/hoymiles_cpp.cpp
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

# Link protobuf (this propagates Abseil correctly)
target_link_libraries(hoymiles_cpp PRIVATE protobuf::libprotobuf)

# Include generated headers
target_include_directories(hoymiles_cpp PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

# format: rewrite files in place
if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND "${CLANG_FORMAT_EXE}" -i --style=file ${PROJECT_FORMAT_FILES}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Formatting src/*.cpp and src/*.hpp"
        VERBATIM
    )

    add_custom_target(check-format
        COMMAND "${CLANG_FORMAT_EXE}" --dry-run --Werror --style=file ${PROJECT_FORMAT_FILES}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Checking formatting of src/*.cpp and src/*.hpp"
        VERBATIM
    )
else()
    message(STATUS "clang-format not found: format targets will not be available")
endif()

# lint: run clang-tidy over translation units in src
if(CLANG_TIDY_EXE)
    add_custom_target(lint
        COMMAND "${CLANG_TIDY_EXE}"
                -p "${CMAKE_BINARY_DIR}"
                --config-file="${CMAKE_SOURCE_DIR}/.clang-tidy"
                ${PROJECT_TIDY_FILES}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Running clang-tidy on src/*.cpp"
        VERBATIM
    )
else()
    message(STATUS "clang-tidy not found: lint target will not be available")
endif()