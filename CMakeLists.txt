cmake_minimum_required(VERSION 3.0)
project(FaceAuthServer)

# C++11 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置模块路径
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# 构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 查找包
find_package(OpenCV 3.4 REQUIRED)
find_package(MySQL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(JSONCPP REQUIRED)
find_package(Threads REQUIRED)

# 显示OpenCV版本
message(STATUS "OpenCV库版本: ${OpenCV_VERSION}")
message(STATUS "OpenCV库路径: ${OpenCV_LIBRARIES}")

# 头文件路径
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
    ${MYSQL_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
    ${JSONCPP_INCLUDE_DIRS}
)

# 主服务器源文件
file(GLOB SERVER_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/auth_server.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/db_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/face_detector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/face_recognizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tcp_server.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)

# 主服务器可执行文件
add_executable(face_auth_server ${SERVER_SOURCES})

# 链接库
target_link_libraries(face_auth_server
    ${OpenCV_LIBS}
    /usr/local/lib64/libopencv_face.so
    /usr/local/lib64/libopencv_core.so.3.4
    /usr/local/lib64/libopencv_imgproc.so.3.4
    /usr/local/lib64/libopencv_objdetect.so.3.4
    /usr/local/lib64/libopencv_video.so.3.4
    ${MYSQL_LIBRARY}
    ${OPENSSL_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

# 安装规则
install(TARGETS face_auth_server DESTINATION bin) 