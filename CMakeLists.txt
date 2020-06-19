cmake_minimum_required(VERSION 3.14)
# set the project name
project (pmem VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# include(CheckCXXCompilerFlag)
# CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
# if(COMPILER_SUPPORTS_MARCH_NATIVE)
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
# endif()

# default using release mode
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo) # RelWithDebInfo
endif()

# add Intel PCM library
add_custom_command(OUTPUT libPCM.a
                    COMMAND make lib
                    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/lib/pcm)
add_custom_target(pcm DEPENDS libPCM.a)

# add gflags library
add_subdirectory(lib/gflags)
list(APPEND THIRDPARTY_LIBS gflags)
include_directories(
    "${CMAKE_BINARY_DIR}/lib/gflags/include"
)

# Process Abseil's CMake build system
add_subdirectory(lib/abseil-cpp)
list(APPEND THIRDPARTY_LIBS absl::flat_hash_set)
include_directories(
    "${CMAKE_BINARY_DIR}/lib/abseil-cpp"
)

# add pthread support
find_package(Threads REQUIRED)
set(CMAKE_CXX_FLAGS "-pthread")

# add avx512 sse2 sse3 support
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -mavx512f -msse -msse2 -msse3 ")

# check existence of jemalloc
include(CheckIncludeFile)
check_include_file("jemalloc/jemalloc.h" HAVE_JEMALLOC)
if(HAVE_JEMALLOC)
  add_definitions(-DJEMALLOC)
  message("has jemalloc")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -ljemalloc ")
endif()

# In GCC, this will invoke the "-I" command
include_directories(
  "/usr/include"
  "/usr/local/include"
  "${PROJECT_SOURCE_DIR}/src"
  "${PROJECT_SOURCE_DIR}/lib/pcm"
  "${PROJECT_SOURCE_DIR}/lib"
)

# add system local lib
link_directories(/usr/lib)
link_directories(/usr/local/lib)
link_directories(${PROJECT_SOURCE_DIR}/lib/pcm)

list(APPEND THIRDPARTY_LIBS pmem vmem PCM)

# all the binary will be in the build folder
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# This will enable your add_test() under tests folder.
option(BUILD_TESTS "Build unit tests" ON)
enable_testing()
function(db_test test_file)
  get_filename_component(test_target_name "${test_file}" NAME_WE)
  set (case_name test_${test_target_name})
  add_executable("${case_name}" "${test_file}")
  target_link_libraries("${case_name}" ${PROJECT_LINK_LIBS} ${THIRDPARTY_LIBS} util)  # boost_thread boost_system
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
  target_link_libraries("${case_name}" ${PROJECT_LINK_LIBS} ${THIRDPARTY_LIBS} util)
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


# The add_subdirectory will add CMakeLists.txt under src and tests into your project.
add_subdirectory(src)