# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(SRCS ${app_sources}
                       REQUIRES driver)

target_add_binary_data(${COMPONENT_TARGET} "../webui/dist/main.css.gz" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "../webui/dist/main.js.gz" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "../webui/dist/index.html.gz" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "../webui/dist/favicon.ico.gz" BINARY)
