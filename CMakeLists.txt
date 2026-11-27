cmake_minimum_required(VERSION 3.16)
project(CAIR LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(CAIR_ENABLE_SANITIZERS "Build C-AIR and its tests with address and undefined behavior sanitizers" OFF)
if(CAIR_ENABLE_SANITIZERS)
	if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
		add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
		add_link_options(-fsanitize=address,undefined)
	else()
		message(FATAL_ERROR "CAIR_ENABLE_SANITIZERS requires GCC or Clang")
	endif()
endif()

find_package(raven CONFIG REQUIRED)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ghostty-vt")
find_package(ghostty-vt CONFIG REQUIRED)

add_library(cair_ui STATIC
	src/app.c
	src/model/project_tree.c
	src/model/terminal_model.c
	src/model/terminal_process.c
	src/widgets/project_explorer.c
	src/widgets/breadcrumb_bar.c
	src/widgets/editor_pane.c
	src/widgets/editor_tabs.c
	src/widgets/main_menu.c
	src/widgets/find_replace_bar.c
	src/widgets/status_bar.c
	src/widgets/terminal_panel.c
	src/widgets/bottom_panel.c
	src/search/find_replace.c
)
target_include_directories(cair_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(cair_ui PUBLIC raven::raven uuid ghostty-vt::ghostty-vt-static)

add_executable(cair WIN32 main.c)
target_link_libraries(cair PRIVATE cair_ui)
set(CAIR_OUTPUT_NAME "C-AIR" CACHE STRING "Name of the built C-AIR executable")
set_target_properties(cair PROPERTIES
	OUTPUT_NAME "${CAIR_OUTPUT_NAME}"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

function(cair_enable_warnings target)
	if(MINGW)
		target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
	endif()
endfunction()

cair_enable_warnings(cair_ui)
cair_enable_warnings(cair)

include(CTest)
if(BUILD_TESTING)
	add_executable(cair_unit_tests tests/test_project_tree.c src/model/project_tree.c)
	target_include_directories(cair_unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
	cair_enable_warnings(cair_unit_tests)
	add_test(NAME unit_project_tree COMMAND cair_unit_tests)
	add_executable(cair_search_tests tests/test_find_replace.c src/search/find_replace.c)
	target_include_directories(cair_search_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
	cair_enable_warnings(cair_search_tests)
	add_test(NAME unit_find_replace COMMAND cair_search_tests)

	add_executable(cair_terminal_model_tests tests/test_terminal_model.c src/model/terminal_model.c)
	target_include_directories(cair_terminal_model_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
	target_link_libraries(cair_terminal_model_tests PRIVATE ghostty-vt::ghostty-vt-static)
	cair_enable_warnings(cair_terminal_model_tests)
	add_test(NAME unit_terminal_model COMMAND cair_terminal_model_tests)

	add_executable(cair_terminal_process_tests tests/test_terminal_process.c src/model/terminal_process.c)
	target_include_directories(cair_terminal_process_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
	cair_enable_warnings(cair_terminal_process_tests)
	add_test(NAME unit_terminal_process COMMAND cair_terminal_process_tests)
	add_executable(cair_widget_tests tests/test_widgets.c)
	target_link_libraries(cair_widget_tests PRIVATE cair_ui)
	cair_enable_warnings(cair_widget_tests)
	add_test(NAME integration_widgets COMMAND cair_widget_tests)
	add_executable(cair_terminal_widget_input_tests tests/test_terminal_widget_input.c)
	target_link_libraries(cair_terminal_widget_input_tests PRIVATE cair_ui)
	cair_enable_warnings(cair_terminal_widget_input_tests)
	add_test(NAME integration_terminal_widget_input COMMAND cair_terminal_widget_input_tests)

	add_executable(cair_terminal_widget_render_tests tests/test_terminal_widget_render.c)
	target_link_libraries(cair_terminal_widget_render_tests PRIVATE cair_ui)
	cair_enable_warnings(cair_terminal_widget_render_tests)
	add_test(NAME integration_terminal_widget_render COMMAND cair_terminal_widget_render_tests)

	add_executable(cair_smoke main.c)
	target_link_libraries(cair_smoke PRIVATE cair_ui)
	cair_enable_warnings(cair_smoke)
	add_test(NAME e2e_app_startup COMMAND cair_smoke --self-test)
endif()
