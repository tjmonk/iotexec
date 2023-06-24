cmake_minimum_required(VERSION 3.10)

include(GNUInstallDirs)

project(iotexec
	VERSION 0.1
    DESCRIPTION "Service to receive and process cloud-to-device commands"
)

set(CMAKE_C_STANDARD 99)

add_executable( ${PROJECT_NAME}
	src/iotexec.c
)

target_include_directories( ${PROJECT_NAME}
	PRIVATE inc
)

target_link_libraries( ${PROJECT_NAME}
	iotclient
)

install(TARGETS ${PROJECT_NAME}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
