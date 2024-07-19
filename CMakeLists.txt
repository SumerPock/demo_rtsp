cmake_minimum_required(VERSION 3.0.0)
project(demo_rtsp)

#检查构建类型是否为Debug，并添加一个预处理器定义
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE}")
    add_definitions(-DDEBUG)
endif()

# 给对方的时候，这样会优先使用当前目录下的库
# set(CMAKE_SKIP_BUILD_RPATH TRUE)
# set(CMAKE_CXX_FLAGS   "-Wl,-z,origin,-rpath,$ORIGIN")

#查找并包含opencv包
find_package(OpenCV REQUIRED)


# set(CMAKE_CXX_FLAGS_RELEASE -Ofast)
 
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3  -Wall")
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3  -Wall")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --pg")

#定义ffmedia库的路径
set(FFMEDIA_PATH /home/firefly/ffmedia_release)

#定义yaml-cpp库的路径
#set(YAML_CPP_PATH ${PROJECT_SOURCE_DIR}/3rdparty/yaml-cpp)
# set(SPDLOG_PATH ${PROJECT_SOURCE_DIR}/3rdparty/spdlog)
set(YAML_CPP_PATH /home/firefly/yaml-cpp)
#定义项目包含的目录
set(
    include_libs
    ${OpenCV_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/include
    # /usr/include/eigen3
    # /usr/include/rockchip
    ${YAML_CPP_PATH}/include
    ${FFMEDIA_PATH}/include
)

#定义链接目录
include_directories(${include_libs})
LINK_DIRECTORIES(${PROJECT_SOURCE_DIR}/libs)
LINK_DIRECTORIES(${YAML_CPP_PATH}/lib)
LINK_DIRECTORIES(${FFMEDIA_PATH}/lib)
LINK_DIRECTORIES(/usr/local/lib)

#定义可执行文件并链接所需要的库
add_executable(demo_rtsp demo_rtsp.cpp)
target_link_libraries(demo_rtsp yaml-cpp ${OpenCV_LIBS} ff_media)