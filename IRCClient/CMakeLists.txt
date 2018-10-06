target_sources(cathook PRIVATE
	"${CMAKE_CURRENT_LIST_DIR}/src/IRCClient.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/src/IRCSocket.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/src/Thread.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/src/IRCHandler.cpp")

target_include_directories(cathook PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")

