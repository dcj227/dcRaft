#CMakeLists.txt

cmake_minimum_required(VERSION 2.8)

set(CMAKE_VERBOSE_MAKEFILE OFF)

set(thirdparty "${PROJECT_SOURCE_DIR}/thirdparty/")
set(pro_inc "${PROJECT_SOURCE_DIR}/include/")
set(pro_src "${PROJECT_SOURCE_DIR}/src/")

include_directories(
    ${pro_inc}/core/
    ${thirdparty}/gtest/include/
)

# 注意用绝对路径
link_directories(
    ${thirdparty}/gtest/lib/
)

link_libraries(
    libgtest.a     
)

add_library(dcraft STATIC
    ${pro_src}/core/epoll_event.cpp
    ${pro_src}/core/socket_event.cpp
)

# 添加编译选项
ADD_DEFINITIONS(
    -O0 -g -W -Wall -pipe -D_GNU_SOURCE -rdynamic
)

#add_subdirectory(example)
#add_subdirectory(test)
