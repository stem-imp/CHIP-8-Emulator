cmake_minimum_required(VERSION 3.10)

project(Chip8Interpreter VERSION 1.0)
message(STATUS "Project Name=${PROJECT_NAME}")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(Chip8)
add_subdirectory(SDL)

configure_file(chip8_interpreter_config.h.in chip8_interpreter_config.h)
message(STATUS "CMAKE_CURRENT_BINARY_DIR is: ${CMAKE_CURRENT_BINARY_DIR}")
message(STATUS "PROJECT_BINARY_DIR is: ${PROJECT_BINARY_DIR}")
message(STATUS "CMAKE_SOURCE_DIR is: ${CMAKE_SOURCE_DIR}")

add_executable(${PROJECT_NAME} WIN32 winmain.cpp winlayout.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE "${PROJECT_BINARY_DIR}")

get_target_property(srcincludes ${PROJECT_NAME} INCLUDE_DIRECTORIES)
message(STATUS "The current src include directory is: ${srcincludes}")
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/Chip8)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/SDL/include)

get_target_property(libincludes ${PROJECT_NAME} INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "The current lib include directory is: ${libincludes}")

target_include_directories(${PROJECT_NAME} PRIVATE
	$<$<CONFIG:Debug>:${srcincludes}/SDL/include-config-debug/SDL2>
	$<$<CONFIG:Release>:${srcincludes}/SDL/include-config-release/SDL2>
	$<$<CONFIG:MinSizeRel>:${srcincludes}/SDL/include-config-minsizerel/SDL2>
	$<$<CONFIG:RelWithDebInfo>:${srcincludes}/SDL/include-config-relwithdebinfo/SDL2>
)

add_dependencies(${PROJECT_NAME} sdl_headers_copy)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2)
target_link_libraries(${PROJECT_NAME} PRIVATE Chip8)

find_package(OpenGL REQUIRED COMPONENTS OpenGL)
message(STATUS "**** OpenGL_OpenGL_FOUND=${OpenGL_OpenGL_FOUND}, OPENGL_GLU_FOUND=${OPENGL_GLU_FOUND}")
message(STATUS "     Path to the OpenGL include directory=${OPENGL_INCLUDE_DIRS}")
message(STATUS "     Paths to the OpenGL library=${OPENGL_LIBRARIES}")

target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)

if(WIN32)
	target_link_libraries(${PROJECT_NAME} PRIVATE winmm comctl32)
	target_compile_definitions(${PROJECT_NAME} PRIVATE _UNICODE)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:SDL2::SDL2>" "$<TARGET_FILE_DIR:Chip8Interpreter>"
		COMMAND ${CMAKE_COMMAND} -E echo "Copy $<TARGET_FILE:SDL2::SDL2> to $<TARGET_FILE_DIR:Chip8Interpreter>"
	)
endif()