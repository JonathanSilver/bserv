add_library(
	bserv
	
	pch.cpp
	bserv.cpp
	client.cpp
	database.cpp
	session.cpp
	utils.cpp
)

target_include_directories(
	bserv PUBLIC
	
	../dependencies
	../dependencies/libpqxx/include
	include
)

target_link_libraries(
	bserv PUBLIC
	
	pthread
	boost_thread
	boost_context
	boost_coroutine
	boost_system
	boost_filesystem
	boost_log
	boost_log_setup
	boost_json
	"${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/cryptopp/libcryptopp.a"
	"${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/libpqxx/src/.libs/libpqxx.a"
	pq
)
