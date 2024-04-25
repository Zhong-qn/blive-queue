set(CMAKE_LEGACY_CYGWIN_WIN32 0)
cmake_minimum_required(VERSION 3.7)

project(blive_queue C)

set(MAJOR_VERSION  0)
set(MINOR_VERSION  0)
set(PATCH_VERSION  1)

# set(CMAKE_CXX_COMPILER g++)
# set(CMAKE_C_COMPILER gcc)

# 创建项目工作路径
if(DEFINED BLIVE_QUEUE_DIR)
message("already difined work directory: ${BLIVE_QUEUE_DIR}")
else()
message("not difined work directory, using default:  ${CMAKE_CURRENT_SOURCE_DIR}")
set(BLIVE_QUEUE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# 创建编译路径
if(DEFINED BLIVE_QUEUE_BUILD_DIR)
message("already difined build directory: ${BLIVE_QUEUE_BUILD_DIR}")
else()
message("not difined build directory, using default:  ${BLIVE_QUEUE_DIR}/build/")
set(BLIVE_QUEUE_BUILD_DIR ${BLIVE_QUEUE_DIR}/build)
endif()
set(BLIVE_API_BUILD_DIR ${BLIVE_QUEUE_BUILD_DIR}/blive-api-c/build)

# 创建lib输出路径
if(DEFINED LIB_OUTPUT_DIR)
message("already difined libarary output directory: ${LIB_OUTPUT_DIR}")
else()
message("not difined libarary output directory, using default:  ${BLIVE_QUEUE_BUILD_DIR}/lib/")
set(LIB_OUTPUT_DIR ${BLIVE_QUEUE_BUILD_DIR}/output/lib)
endif()
set(LIBRARY_OUTPUT_PATH ${LIB_OUTPUT_DIR})

# 创建头文件输出路径
if(DEFINED INCLUDE_OUTPUT_DIR)
message("already difined include file output directory: ${INCLUDE_OUTPUT_DIR}")
else()
message("not difined include file output directory, using default:  ${BLIVE_QUEUE_BUILD_DIR}/include/")
set(INCLUDE_OUTPUT_DIR ${BLIVE_QUEUE_DIR}/include)
endif()

# 创建cmake临时文件路径
if(DEFINED TMP_FILES_DIR)
message("already difined cache file directory: ${TMP_FILES_DIR}")
else()
message("not difined cache file directory, using default:  ${BLIVE_QUEUE_DIR}/CMakeCaches/")
set(TMP_FILES_DIR ${BLIVE_QUEUE_DIR}/CMakeFiles/blive_queue)
endif()

# 创建可执行文件路径
if (DEFINED BLIVE_QUEUE_EXEC_OUT_PATH)
set(EXECUTABLE_OUTPUT_PATH ${BLIVE_QUEUE_EXEC_OUT_PATH})
message("already difined executable file directory: ${BLIVE_QUEUE_EXEC_OUT_PATH}")
else()
message("not difined executable file directory, using default:  ${BLIVE_QUEUE_DIR}/CMakeCaches/")
set(EXECUTABLE_OUTPUT_PATH ${BLIVE_QUEUE_BUILD_DIR}/output/)
endif()

include_directories(${BLIVE_QUEUE_DIR}/include
                    ${BLIVE_QUEUE_DIR}/source
                    ${BLIVE_QUEUE_DIR}/source/utils)

link_directories(./build/output/lib/)


set(BLIVE_QUEUE_SRC     ${BLIVE_QUEUE_DIR}/source/main.c
                        ${BLIVE_QUEUE_DIR}/source/callbacks.c
                        ${BLIVE_QUEUE_DIR}/source/utils/qlist.c
                        ${BLIVE_QUEUE_DIR}/source/utils/hash.c
                        ${BLIVE_QUEUE_DIR}/source/utils/pri_queue.c
                        ${BLIVE_QUEUE_DIR}/source/utils/select.c
                        ${BLIVE_QUEUE_DIR}/source/utils/httpd.c
                        )


file(COPY  ${BLIVE_QUEUE_DIR}/config/pdjcfg.json   DESTINATION ${EXECUTABLE_OUTPUT_PATH}/config)

add_compile_options(-g -Wall -fPIC)

add_subdirectory(supports/blive-api-c ${BLIVE_QUEUE_DIR}/CMakeFiles/blive_api)

add_executable(blive_queue ${BLIVE_QUEUE_SRC})
add_definitions(-DBLIVE_API_DEBUG_DEBUG)

target_link_libraries(blive_queue pthread  blive_api_s)
if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries(blive_queue ws2_32)
endif()