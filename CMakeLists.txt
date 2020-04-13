cmake_minimum_required(VERSION 3.10)
# set the project name
project (pmem VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# default using release mode
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# For the static library:
include_directories(/usr/local/include)
include_directories(/usr/include)

# In GCC, this will invoke the "-I" command
include_directories(
  "${PROJECT_SOURCE_DIR}/src"
)

# add system local lib
link_directories(/usr/lib)
link_directories(/usr/local/lib)

# add pthread support
find_package(Threads REQUIRED)

# Check if the compiler has AVX512 support.
set(CMAKE_CXX_FLAGS "-pthread -mavx512f -msse4.1 -msse2 -mavx")

# check existence of jemalloc
include(CheckIncludeFile)
check_include_file("jemalloc/jemalloc.h" HAVE_JEMALLOC)
if(HAVE_JEMALLOC)
  add_definitions(-DJEMALLOC)
  message("has jemalloc")
endif()

# all the binary will be in the build folder
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# This will enable your add_test() under tests folder.
option(BUILD_TESTS "Build unit tests" ON)
enable_testing()
function(db_test test_file)
  get_filename_component(test_target_name "${test_file}" NAME_WE)
  set (case_name test_${test_target_name})
  add_executable("${case_name}" "${test_file}")
  target_link_libraries("${case_name}" ${PROJECT_LINK_LIBS} ${THIRDPARTY_LIBS} util pmem vmem)  # boost_thread boost_system
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
  target_link_libraries("${case_name}" ${PROJECT_LINK_LIBS} ${THIRDPARTY_LIBS} util pmem vmem)
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