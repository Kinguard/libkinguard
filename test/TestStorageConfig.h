#ifndef TESTSTORAGECONFIG_H_
#define TESTSTORAGECONFIG_H_

#include <cppunit/extensions/HelperMacros.h>

class TestStorageConfig: public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE( TestStorageConfig );
	CPPUNIT_TEST( Test );
	CPPUNIT_TEST_SUITE_END();
public:
	void setUp();
	void tearDown();
	void Test();
};

#endif /* TESTSTORAGECONFIG_H_ */
