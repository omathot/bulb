cmake_minimum_required(VERSION 4.0)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
project(bulb VERSION 1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

include(FetchContent)
FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.4.2
  SYSTEM
)
FetchContent_MakeAvailable(SDL3)

FetchContent_Declare(
  SDL_image
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
  GIT_TAG release-3.4.0
  SYSTEM
)
FetchContent_MakeAvailable(SDL_image)

FetchContent_Declare(
  SDL_ttf
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
  GIT_TAG release-3.2.2
  SYSTEM
)
FetchContent_MakeAvailable(SDL_ttf)

find_package(glm REQUIRED)

file(GLOB_RECURSE SRCS "src/*.cpp")
file(GLOB_RECURSE MODULES "src/*.cppm")

if(DEFINED ENV{SLANGC_EXECUTABLE})
    set(SLANGC_EXECUTABLE $ENV{SLANGC_EXECUTABLE})
else()
       message(FATAL_ERROR "slangc not found. Must set SLANGC_EXECUTABLE env variable")
endif()
set(SHADER_SLANG_SOURCES ${CMAKE_CURRENT_LIST_DIR}/shaders/shader.slang)
# slang compile function
function (add_slang_shader_target TARGET)
       cmake_parse_arguments ("SHADER" "" "" "SOURCES" ${ARGN})
       set (SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/shaders)
       set (ENTRY_POINTS -entry vertMain -entry fragMain)
       add_custom_command (
              OUTPUT ${SHADERS_DIR}
              COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
       )
       # very long -profile is to explicitly enable slangc features
       # not necessary slangc turns them on automatically when needed
       add_custom_command (
              OUTPUT  ${SHADERS_DIR}/slang.spv
              COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCES}
                     -target spirv
                     -profile spirv_1_6+SPV_GOOGLE_user_type+spvDerivativeControl+spvImageQuery+spvImageGatherExtended+spvSparseResidency+spvMinLod+spvFragmentFullyCoveredEXT
                     -emit-spirv-directly
                     -fvk-use-entrypoint-name ${ENTRY_POINTS}
                     -o shader.spv
              WORKING_DIRECTORY ${SHADERS_DIR}
              DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
              COMMENT "Compiling Slang Shaders"
              VERBATIM
       )
       add_custom_target (${TARGET} DEPENDS ${SHADERS_DIR}/slang.spv)
endfunction()
add_slang_shader_target(shaders SOURCES ${SHADER_SLANG_SOURCES})

add_executable(${PROJECT_NAME})
add_dependencies(${PROJECT_NAME} shaders)
target_sources(${PROJECT_NAME}
  PRIVATE ${SRCS}
  PRIVATE FILE_SET cxx_modules TYPE CXX_MODULES FILES ${MODULES}
)
set(SANITIZER_FLAGS -fsanitize=address -fsanitize=undefined)
target_compile_options(${PROJECT_NAME} PRIVATE
       -Wall -Werror -Wextra -Wshadow -Wconversion --coverage ${SANITIZER_FLAGS}
)
target_link_options(${PROJECT_NAME} PRIVATE
       --coverage ${SANITIZER_FLAGS}
)

# fix the .gcda not getting cleaned and showing as corrupted, even with --clean-first
add_custom_target(clean-gcda
  COMMAND ${CMAKE_COMMAND} -E rm -f
  ${CMAKE_BINARY_DIR}/CMakeFiles/bulb.dir/src/*.gcda
  COMMENT "Removing stale .gcda files"
)
add_dependencies(bulb clean-gcda)

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_MODULE_STD 1)
target_link_libraries(${PROJECT_NAME} PRIVATE
  SDL3::SDL3
  SDL3_image::SDL3_image
  SDL3_ttf::SDL3_ttf
  glm::glm
)


# clang-tidy
option(TIDY "Enable clang-tidy pass" OFF)
if (TIDY)
       find_program(CLANG_TIDY_EXE NAMES clang-tidy)
       if(CLANG_TIDY_EXE)
              set_target_properties(${PROJECT_NAME} PROPERTIES
                     CXX_CLANG_TIDY "${CLANG_TIDY_EXE}"
              )
              message(STATUS "clang-tidy enabled: Expect longer compile times")
       else()
              message(FATAL_ERROR "clang-tidy not found")
       endif()
endif()

