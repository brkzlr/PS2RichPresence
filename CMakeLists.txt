cmake_minimum_required(VERSION 3.22...3.30)
project(PS2RichPresence LANGUAGES C)

file(GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/src/*.c")
add_executable(ps2rpc ${SOURCES})
set_target_properties(ps2rpc PROPERTIES
	C_STANDARD 11
	C_STANDARD_REQUIRED ON
	C_EXTENSIONS OFF
	EXPORT_COMPILE_COMMANDS ON
	RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
	INSTALL_RPATH "$ORIGIN"
)

target_include_directories(ps2rpc PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_directories(ps2rpc PRIVATE "${PROJECT_SOURCE_DIR}/lib")
target_compile_definitions(ps2rpc PRIVATE _XOPEN_SOURCE) # For strptime
target_compile_options(ps2rpc PRIVATE -pedantic-errors)

# Add Apple ARM brew include/library paths
if(APPLE)
	target_include_directories(ps2rpc PRIVATE /opt/homebrew/include)
	target_link_directories(ps2rpc PRIVATE /opt/homebrew/lib)
endif()

# Find JSON-C and Discord-RPC libraries
get_target_property(link_directories ps2rpc LINK_DIRECTORIES)
if(APPLE)
	set(jsonc_soname_ext ".5.dylib")
	set(discordrpc_soname_ext ".dylib")
else()
	set(jsonc_soname_ext ".so.5")
	set(discordrpc_soname_ext ".so")
endif()

find_library(json_c_path json-c
	REQUIRED
	PATHS ${link_directories}
)
add_library(jsonC SHARED IMPORTED)
file(REAL_PATH ${json_c_path} json_c_path) # To resolve symlink and grab real library
set_target_properties(jsonC PROPERTIES
	IMPORTED_LOCATION "${json_c_path}"
	IMPORTED_SONAME "libjson-c${jsonc_soname_ext}"
)

find_library(discord_rpc_path discord-rpc
	REQUIRED
	PATHS ${link_directories}
)
add_library(discordRpc SHARED IMPORTED)
set_target_properties(discordRpc PROPERTIES
	IMPORTED_LOCATION "${discord_rpc_path}"
	IMPORTED_SONAME "libdiscord-rpc${discordrpc_soname_ext}"
)

target_link_libraries(ps2rpc PRIVATE discordRpc jsonC)

install(TARGETS ps2rpc
	RUNTIME
		DESTINATION "/opt/PS2RichPresence"
)
install(IMPORTED_RUNTIME_ARTIFACTS discordRpc jsonC
	LIBRARY
		DESTINATION "/opt/PS2RichPresence"
)
