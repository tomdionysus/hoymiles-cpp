cmake_minimum_required(VERSION 3.16)

project(hoymiles-cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Prefer modern protobuf config package
find_package(Protobuf CONFIG REQUIRED)

# Bring in protobuf_generate_cpp helper macro
include(${Protobuf_DIR}/protobuf-module.cmake OPTIONAL)
if(NOT COMMAND protobuf_generate_cpp)
    include(FindProtobuf)
endif()

# Collect proto files
file(GLOB_RECURSE PROTO_FILES
     "${CMAKE_CURRENT_SOURCE_DIR}/protobuf/*.proto")

# Generate .pb.cc/.pb.h
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

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