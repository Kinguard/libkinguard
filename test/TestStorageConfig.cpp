#include "TestStorageConfig.h"

#include "StorageConfig.h"

CPPUNIT_TEST_SUITE_REGISTRATION ( TestStorageConfig );

using namespace KGP;
using namespace KGP::Storage;

void TestStorageConfig::setUp()
{
}

void TestStorageConfig::tearDown()
{
}

static void testModel(Model::Type typ, const string& typs)
{
	CPPUNIT_ASSERT_EQUAL( string(Model::asString(typ)), typs );
}

void TestStorageConfig::Test()
{

	CPPUNIT_ASSERT_EQUAL( Model::asType("static"),			Model::Static );
	CPPUNIT_ASSERT_EQUAL( Physical::asType("none"),			Physical::None );
	CPPUNIT_ASSERT( Logical::asType("lvm") !=				Logical::None );
	CPPUNIT_ASSERT_EQUAL( Logical::asType("lvm"),			Logical::LVM );
	CPPUNIT_ASSERT_EQUAL( Encryption::asType("luks"),		Encryption::LUKS );
	CPPUNIT_ASSERT_EQUAL( Encryption::asType("undefined"),	Encryption::Undefined );
	CPPUNIT_ASSERT_EQUAL( Encryption::asType("unknown"),	Encryption::Unknown );

	testModel(Model::Undefined,	"undefined");
	testModel(Model::Static,	"static");
	testModel(Model::Dynamic,	"dynamic");
	testModel(Model::Unknown,	"unknown");

}
