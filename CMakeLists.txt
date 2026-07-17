cmake_minimum_required(VERSION 3.22...3.30)
project(PS2RichPresence LANGUAGES C)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND UNIX)
	set(CMAKE_INSTALL_PREFIX "/opt/PS2RichPresence" CACHE PATH "Install path" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PS2RP_TITLE_DB "${PROJECT_SOURCE_DIR}/data/ps2_titles.tsv")
set(PS2RP_RUNTIME_TITLE_DB "${PROJECT_SOURCE_DIR}/bin/ps2_titles.tsv")
set(PS2RP_DEPENDENCY_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/include")
set(PS2RP_DEPENDENCY_LIBRARY_DIRS "${PROJECT_SOURCE_DIR}/lib")
if(APPLE)
	set(PS2RP_INSTALL_RPATH "@loader_path")
else()
	set(PS2RP_INSTALL_RPATH "$ORIGIN")
endif()

if(NOT EXISTS "${PS2RP_TITLE_DB}")
	message(FATAL_ERROR "Missing title database: ${PS2RP_TITLE_DB}")
endif()

add_executable(ps2rpc
	pc/src/TitleDatabase.c
	pc/src/main.c
	pc/src/TitleDatabase.h
	protocol/ps2rp_protocol.h
)
target_compile_features(ps2rpc PRIVATE c_std_11)
set_target_properties(ps2rpc PROPERTIES
	C_EXTENSIONS OFF
	INSTALL_RPATH "${PS2RP_INSTALL_RPATH}"
	MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
	RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${PROJECT_SOURCE_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${PROJECT_SOURCE_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PROJECT_SOURCE_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PROJECT_SOURCE_DIR}/bin"
)
if(UNIX)
	target_compile_definitions(ps2rpc PRIVATE _XOPEN_SOURCE=700) # POSIX APIs
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
	target_compile_options(ps2rpc PRIVATE -pedantic-errors)
endif()

# Add Apple ARM brew include/library paths
if(APPLE)
	list(APPEND PS2RP_DEPENDENCY_INCLUDE_DIRS /opt/homebrew/include)
	list(APPEND PS2RP_DEPENDENCY_LIBRARY_DIRS /opt/homebrew/lib)
	# _XOPEN_SOURCE alone hides too much of the BSD side of Apple's headers.
	target_compile_definitions(ps2rpc PRIVATE _DARWIN_C_SOURCE)
endif()

find_path(DISCORD_RPC_INCLUDE_DIR discord_rpc.h
	REQUIRED
	PATHS ${PS2RP_DEPENDENCY_INCLUDE_DIRS}
	NO_DEFAULT_PATH
	NO_CACHE
)
target_include_directories(ps2rpc PRIVATE
	"${DISCORD_RPC_INCLUDE_DIR}"
	"${PROJECT_SOURCE_DIR}/protocol"
)

find_library(DISCORD_RPC_LIBRARY discord-rpc
	REQUIRED
	PATHS ${PS2RP_DEPENDENCY_LIBRARY_DIRS}
	NO_DEFAULT_PATH
	NO_CACHE
)
if(WIN32)
	find_file(DISCORD_RPC_RUNTIME_LIBRARY
		NAMES discord-rpc.dll libdiscord-rpc.dll
		REQUIRED
		PATHS "${PROJECT_SOURCE_DIR}/bin"
		NO_DEFAULT_PATH
		NO_CACHE
	)
	target_compile_definitions(ps2rpc PRIVATE DISCORD_DYNAMIC_LIB)
	target_link_libraries(ps2rpc PRIVATE "${DISCORD_RPC_LIBRARY}" ws2_32)
else()
	set(DISCORD_RPC_RUNTIME_LIBRARY "${DISCORD_RPC_LIBRARY}")
	target_link_libraries(ps2rpc PRIVATE "${DISCORD_RPC_LIBRARY}")
endif()

add_custom_command(
	OUTPUT "${PS2RP_RUNTIME_TITLE_DB}"
	COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:ps2rpc>"
	COMMAND "${CMAKE_COMMAND}" -E copy
		"${PS2RP_TITLE_DB}"
		"${PS2RP_RUNTIME_TITLE_DB}"
	DEPENDS "${PS2RP_TITLE_DB}"
	VERBATIM
)
add_custom_target(ps2rp_runtime_data ALL
	DEPENDS "${PS2RP_RUNTIME_TITLE_DB}"
)
add_dependencies(ps2rpc ps2rp_runtime_data)

install(TARGETS ps2rpc
	RUNTIME
		DESTINATION "."
)
install(FILES
	"${DISCORD_RPC_RUNTIME_LIBRARY}"
	"${PS2RP_TITLE_DB}"
	"${PROJECT_SOURCE_DIR}/LICENSE.txt"
	DESTINATION "."
)
install(FILES "${PROJECT_SOURCE_DIR}/bin/ps2rp.elf"
	DESTINATION "."
	OPTIONAL
)
