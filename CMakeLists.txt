target_sources(${CMAKE_PROJECT_NAME} PRIVATE
	"${CMAKE_CURRENT_LIST_DIR}/src/ChIRC.cpp")

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
target_sources(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src/ChIRC.hpp")

add_subdirectory(IRCClient)
