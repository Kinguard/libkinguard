pkg_check_modules( CPPUNIT cppunit>=1.12.1 )

set( testapp_src
	test.cpp
	TestStorageDevice.cpp
	)


include_directories(
	"${PROJECT_SOURCE_DIR}"
)

add_definitions( -Wall )
add_executable( testapp ${testapp_src} )

target_link_libraries( testapp kinguard ${CPPUNIT_LDFLAGS} ${LIBUTILS_LDFLAGS} )

