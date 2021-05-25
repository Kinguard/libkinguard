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
	CPPUNIT_ASSERT_EQUAL( string(Model::Model::toName(typ)), typs );
}

void TestStorageConfig::Test()
{

	CPPUNIT_ASSERT_EQUAL( Model::Model::toType("static"),			Model::Static );
	CPPUNIT_ASSERT_EQUAL( Physical::Physical::toType("none"),			Physical::None );
	CPPUNIT_ASSERT( Logical::Logical::toType("lvm") !=				Logical::None );
	CPPUNIT_ASSERT_EQUAL( Logical::Logical::toType("lvm"),			Logical::LVM );
	CPPUNIT_ASSERT_EQUAL( Encryption::Encryption::toType("luks"),		Encryption::LUKS );
	CPPUNIT_ASSERT_EQUAL( Encryption::Encryption::toType("undefined"),	Encryption::Undefined );
	CPPUNIT_ASSERT_EQUAL( Encryption::Encryption::toType("unknown"),	Encryption::Unknown );

	testModel(Model::Undefined,	"undefined");
	testModel(Model::Static,	"static");
	testModel(Model::Dynamic,	"dynamic");
	testModel(Model::Unknown,	"unknown");

}
