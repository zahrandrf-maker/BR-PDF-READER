cmake_minimum_required(VERSION 3.20)
project(BR_PDF_READER VERSION 1.4.1 LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "BR PDF READER targets Windows 10/11 x64.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/ffgl/CMakeLists.txt")
  message(FATAL_ERROR "Missing external/ffgl/CMakeLists.txt. GitHub Actions should clone Resolume FFGL before Configure.")
endif()

find_package(GLEW REQUIRED)

set(FFGL_BUILD_EXAMPLE_PLUGINS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/ffgl ffgl-build EXCLUDE_FROM_ALL)

add_library(BR_PDF_READER MODULE
  src/BRPdfReader.cpp
  src/BRPdfReader.h
  src/PdfRenderer.cpp
  src/PdfRenderer.h
)

target_compile_definitions(BR_PDF_READER PRIVATE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
  _WIN32_WINNT=0x0A00
  WINVER=0x0A00
  GLEW_STATIC
  BR_PDF_READER_VERSION="1.4.1"
)

target_link_libraries(BR_PDF_READER PRIVATE
  ffgl::sdk
  GLEW::GLEW
  windowsapp
  runtimeobject
  ole32
  shell32
  windowscodecs
  opengl32
)

if(MSVC)
  target_compile_options(BR_PDF_READER PRIVATE /W4 /EHsc /permissive-)
endif()

set_target_properties(BR_PDF_READER PROPERTIES
  PREFIX ""
  OUTPUT_NAME "BR_PDF_READER_FFGL"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
