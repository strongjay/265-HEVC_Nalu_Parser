cmake_minimum_required(VERSION 3.29)
project(ParseHEVCNALU)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)

# 查找 FFmpeg 库
find_package(PkgConfig REQUIRED)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavformat
    libavcodec
    libavutil
)
include_directories(${AVFORMAT_INCLUDE_DIRS})
include_directories(${AVCODEC_INCLUDE_DIRS})
include_directories(${AVUTIL_INCLUDE_DIRS})

link_directories(${AVFORMAT_LIBRARY_DIRS})
link_directories(${AVCODEC_LIBRARY_DIRS})
link_directories(${AVUTIL_LIBRARY_DIRS})
# 设置可执行文件的输出路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 添加源文件
add_executable(parse_hevc_nalu src/demo.cpp)

# 链接 FFmpeg 库
link_directories(/usr/lib/x86_64-linux-gnu)
target_link_libraries(parse_hevc_nalu PRIVATE
    ${AVFORMAT_LIBRARIES} 
    ${AVCODEC_LIBRARIES} 
    ${AVUTIL_LIBRARIES}
)

# 设置编译器标志
target_compile_options(parse_hevc_nalu PRIVATE
    -Wall
)
