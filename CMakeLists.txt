cmake_minimum_required(VERSION 3.16)
project(vulkan_test VERSION 1.0 DESCRIPTION "Test" LANGUAGES C)

find_package(Vulkan COMPONENTS glslc)
find_program(glslc_executable NAMES glslc)
set(CMAKE_BUILD_TYPE Debug)
function(compile_shader target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "ENV;FORMAT" "SOURCES")
    foreach(source ${arg_SOURCES})
	get_filename_component(source_name ${source} NAME)
	set(output "${CMAKE_CURRENT_BINARY_DIR}/${source_name}.${arg_FORMAT}")
        add_custom_command(
		OUTPUT  ${output}      	
		DEPENDS ${source}
        	DEPFILE ${source}.d
        	COMMAND
			${glslc_executable}
                	$<$<BOOL:${arg_ENV}>:--target-env=${arg_ENV}>
			#$<$<BOOL:${arg_FORMAT}>:-mfmt=${arg_FORMAT}>
                	-MD -MF ${source}.d
			-o ${output}
                	${source}
        )
        target_sources(${target} PRIVATE ${output})
    endforeach()
endfunction()

add_compile_definitions(DEBUG)

set(CMAKE_EXPORT_COMPILE_COMMANDS true)

set(INC "${PROJECT_SOURCE_DIR}/inc")
set(SHADERS "${PROJECT_SOURCE_DIR}/shaders")
include_directories("${CMAKE_SOURCE_DIR}/inc")

add_subdirectory(src)
add_subdirectory(app)
add_subdirectory(shaders)
