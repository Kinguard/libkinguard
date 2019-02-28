project( kinguard )

cmake_minimum_required(VERSION 2.6)

include( FindPkgConfig )
pkg_check_modules ( LIBOPI REQUIRED libopi>=1.6.49 )

set (VERSION_MAJOR 1)
set (VERSION_MINOR 0)
set (VERSION_PATCH 14)
set (VERSION "${VERSION_MAJOR}.${VERSION_MINOR}")
set (VERSION_FULL "${VERSION}.${VERSION_PATCH}")
set (APP_NAME "lib${PROJECT_NAME}")

configure_file(
	"${PROJECT_SOURCE_DIR}/Config.h.in"
	"${PROJECT_BINARY_DIR}/Config.h"
)

configure_file(
	"${PROJECT_SOURCE_DIR}/lib${PROJECT_NAME}.pc.in"
	"${PROJECT_BINARY_DIR}/lib${PROJECT_NAME}.pc"
	@ONLY
)

include_directories(
	"${PROJECT_BINARY_DIR}"
	"${LIBOPI_INCLUDE_DIRS}"
	)

set( headers
	BackupManager.h
	BaseManager.h
	IdentityManager.h
	MailManager.h
	StorageManager.h
	UserManager.h
	"${PROJECT_BINARY_DIR}/Config.h"
	)

set( src
	BackupManager.cpp
	BaseManager.cpp
	IdentityManager.cpp
	MailManager.cpp
	StorageManager.cpp
	UserManager.cpp
	)

add_definitions( -std=c++11 -Wall )

add_library( ${PROJECT_NAME}_static STATIC ${src} )
add_library( ${PROJECT_NAME} SHARED ${src} )

target_link_libraries(  ${PROJECT_NAME}
	${LIBOPI_LDFLAGS}
	)

set_target_properties( ${PROJECT_NAME} PROPERTIES
		VERSION ${VERSION_FULL}
		SOVERSION ${VERSION_MAJOR}
	)

set_target_properties( ${PROJECT_NAME}_static PROPERTIES
		OUTPUT_NAME ${PROJECT_NAME}
	)

#add_subdirectory( test )
#enable_testing()
#add_test( NAME TestLibKinguard COMMAND testapp )

install( FILES ${headers} DESTINATION include/${PROJECT_NAME} )
install(FILES "${PROJECT_BINARY_DIR}/lib${PROJECT_NAME}.pc" DESTINATION lib/${CMAKE_LIBRARY_ARCHITECTURE}/pkgconfig)
install(FILES "${PROJECT_BINARY_DIR}/lib${PROJECT_NAME}.pc" DESTINATION share/pkgconfig)
install( TARGETS ${PROJECT_NAME} ${PROJECT_NAME}_static DESTINATION lib/${CMAKE_LIBRARY_ARCHITECTURE} )
