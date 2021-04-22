#ifndef TESTSTORAGEDEVICE_H_
#define TESTSTORAGEDEVICE_H_

#include <cppunit/extensions/HelperMacros.h>

class TestStorageDevice: public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE( TestStorageDevice );
	CPPUNIT_TEST( Test );
	CPPUNIT_TEST_SUITE_END();
public:
	void setUp();
	void tearDown();
	void Test();
};

#endif /* TESTSTORAGEDEVICE_H_ */
