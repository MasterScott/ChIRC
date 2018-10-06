target_sources(cathook PRIVATE
	"${CMAKE_CURRENT_LIST_DIR}/src/ChIRC.cpp")

target_include_directories(cathook PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
target_sources(cathook PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src/ChIRC.hpp")

add_subdirectory(IRCClient)
