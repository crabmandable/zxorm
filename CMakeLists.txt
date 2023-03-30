cmake_minimum_required(VERSION 3.14)

include(cmake/prelude.cmake)

project(
    zxorm
    VERSION 0.1.1
    DESCRIPTION "Zach's SQLite ORM"
    HOMEPAGE_URL "https://github.com/crabmandable/zxorm"
    LANGUAGES NONE
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

# ---- Declare library ----

add_library(zxorm_zxorm INTERFACE)
add_library(zxorm::zxorm ALIAS zxorm_zxorm)

set_property(
    TARGET zxorm_zxorm PROPERTY
    EXPORT_NAME zxorm
)

target_include_directories(
    zxorm_zxorm ${warning_guard}
    INTERFACE
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

target_compile_features(zxorm_zxorm INTERFACE cxx_std_20)

find_package(SQLite3 REQUIRED)
target_link_libraries(zxorm_zxorm INTERFACE SQLite::SQLite3)

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

# ---- Developer mode ----

if(NOT zxorm_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of zxorm"
  )
endif()

include(cmake/dev-mode.cmake)
