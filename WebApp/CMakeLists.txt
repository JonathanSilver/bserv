add_executable(
	WebApp
	
	handlers.cpp
	rendering.cpp
	WebApp.cpp
)

target_include_directories(
	WebApp PUBLIC
	
	../dependencies/inja/include
	../dependencies/inja/third_party/include
)

target_link_libraries(
	WebApp PUBLIC
	
	bserv
)
