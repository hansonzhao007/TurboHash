cmake_minimum_required(VERSION 3.11)

# THIS HAS TO COME BEFORE THE PROJECT LINE
# set(CMAKE_C_COMPILER "clang")
# set(CMAKE_CXX_COMPILER "clang++")
# THIS HAS TO COME BEFORE THE PROJECT LINE

# set the project name
project (turbohash VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# default using release mode
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE release) # RelWithDebInfo
  message(STATUS "build type set to release")
endif()
string(TOLOWER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE)
message(STATUS "build type: ${CMAKE_BUILD_TYPE}")

option(AVX512 "Enable use of the Advanced Vector Extensions 512 (AVX512) instruction set" ON)

# add Intel PCM library
execute_process(  COMMAND make lib
                  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/lib/pcm)
link_directories(${PROJECT_SOURCE_DIR}/lib/pcm)
list(APPEND THIRDPARTY_LIBS PCM)
include_directories(
  "${PROJECT_SOURCE_DIR}/lib/pcm"
)

# add liballoc.a library: https://github.com/urcs-sync/ralloc
execute_process(  COMMAND make libralloc.a -j8
                  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/lib/ralloc/test
                  )
link_directories(${PROJECT_SOURCE_DIR}/lib/ralloc/test)
list(APPEND THIRDPARTY_LIBS ralloc)
include_directories(
  "${PROJECT_SOURCE_DIR}/lib/ralloc/src"
)
add_definitions(-DRALLOC)

# add gflags library
add_subdirectory(lib/gflags)
list(APPEND THIRDPARTY_LIBS gflags)
include_directories(
    "${CMAKE_BINARY_DIR}/lib/gflags/include"
)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
if(COMPILER_SUPPORTS_MARCH_NATIVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -Wall -Wextra")
endif()

# tbb
find_library(TbbLib tbb)
list(APPEND THIRDPARTY_LIBS  ${TbbLib})

# add pthread support
find_package(Threads REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wno-unused-parameter -Wno-ignored-qualifiers -msse -msse2")

if(AVX512)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__AVX512__ -mavx -mavx512f")
endif(AVX512)

# check existence of jemalloc
include(CheckIncludeFile)
check_include_file("jemalloc/jemalloc.h" HAVE_JEMALLOC)
if(HAVE_JEMALLOC)
  add_definitions(-DJEMALLOC)
  list(APPEND THIRDPARTY_LIBS jemalloc)
endif()

# In GCC, this will invoke the "-I" command
include_directories(
  "/usr/include"
  "/usr/local/include"
  "${PROJECT_SOURCE_DIR}/src"  
  "${PROJECT_SOURCE_DIR}/lib"
)

# add system local lib
link_directories(/usr/lib)
link_directories(/usr/local/lib)

list(APPEND THIRDPARTY_LIBS pmem vmem pmemobj)

# all the binary will be in the build folder
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# This will enable your add_test() under tests folder.
option(BUILD_TESTS "Build unit tests" ON)
enable_testing()
function(db_test test_file)
  get_filename_component(test_target_name "${test_file}" NAME_WE)
  set (case_name test_${test_target_name})
  add_executable("${case_name}" "${test_file}")
  target_link_libraries("${case_name}" ${PROJECT_LINK_LIBS} ${THIRDPARTY_LIBS})  # boost_thread boost_system
  target_sources("${case_name}"
      PRIVATE
      "${test_file}"
  )
  add_test(NAME "${case_name}" COMMAND "${case_name}")
endfunction(db_test)

function(db_exe src_file)
  get_filename_component(target_name "${src_file}" NAME_WE)
  set (case_name ${target_name})
  add_executable("${case_name}" "${src_file}")
  target_link_libraries("${case_name}" ${PROJECT_LINK_LIBS} ${THIRDPARTY_LIBS})
  target_sources("${case_name}"
      PRIVATE
      "${src_file}"
  )
endfunction(db_exe)

# print all include directories and lib directories
get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
foreach(dir ${dirs})
  message(STATUS "include dir='${dir}'")
endforeach()
get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY LINK_DIRECTORIES)
foreach(dir ${dirs})
  message(STATUS "lib dir='${dir}'")
endforeach()

foreach(tlib ${THIRDPARTY_LIBS})
  message(STATUS "lib name='${tlib}'")
endforeach()

# The add_subdirectory will add CMakeLists.txt under src and tests into your project.
add_subdirectory(src)