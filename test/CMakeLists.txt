cmake_minimum_required(VERSION 3.14)

project(zxormTests LANGUAGES CXX)

include(../cmake/project-is-top-level.cmake)
include(../cmake/folders.cmake)
include(../cmake/windows-set-path.cmake)

# ---- Dependencies ----

# GTEST
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

if(PROJECT_IS_TOP_LEVEL)
  # find_package(zxorm REQUIRED)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../" zxorm)
  enable_testing()
endif()

# ---- Tests ----

set(test_sources "")
file(GLOB_RECURSE test_sources "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")
add_executable(zxorm_test ${test_sources})
target_link_libraries(zxorm_test pthread zxorm::zxorm GTest::gtest_main)
target_compile_features(zxorm_test PRIVATE cxx_std_20)

add_test(NAME zxorm_test COMMAND zxorm_test)

# ---- End-of-file commands ----

add_folders(Test)
