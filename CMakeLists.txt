cmake_minimum_required(VERSION 3.25.2)

project(vulkan-fractal)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)

file(MAKE_DIRECTORY ${SHADER_BINARY_DIR})

function(add_shader PATH BINARY_NAME)
  if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(GLSLC_OPT -O)
  elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    set(GLSLC_OPT -Os)
  endif()
  add_custom_command(
    OUTPUT ${SHADER_BINARY_DIR}/${BINARY_NAME}
    COMMAND Vulkan::glslc
    ARGS --target-env=vulkan1.0 -o ${SHADER_BINARY_DIR}/${BINARY_NAME} ${GLSLC_OPT} ${PATH}
    DEPENDS ${SHADER_BINARY_DIR} ${PATH} Vulkan::glslc
  )
endfunction()

add_shader(${SHADER_SOURCE_DIR}/shader.vert vert.spv)
add_shader(${SHADER_SOURCE_DIR}/shader.frag frag.spv)

add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)

add_executable(${PROJECT_NAME} main.c)



target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan glfw m)

add_custom_target(${PROJECT_NAME}-with-shaders
    DEPENDS ${PROJECT_NAME} ${SHADER_BINARY_DIR}/vert.spv ${SHADER_BINARY_DIR}/frag.spv
)
