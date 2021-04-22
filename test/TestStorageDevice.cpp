#include "TestStorageDevice.h"

#include "StorageDevice.h"

CPPUNIT_TEST_SUITE_REGISTRATION ( TestStorageDevice );

using namespace KGP;

void TestStorageDevice::setUp()
{
}

void TestStorageDevice::tearDown()
{
}
#if 1
static void dump(const StorageDevice& dev)
{
	cout << "Dev: "	<< dev.Model()
		 << " "		<< dev.DeviceName()
		 << " "		<< dev.DevicePath()
		 << " "		<< dev.SysPath();

	if( dev.Is(StorageDevice::Mounted) )
	{
		cout << " mounted at " << dev.MountPoint();
		if( dev.Is(StorageDevice::RootDevice) )
		{
			cout << " and this is the root device";
		}
	}
	else
	{
		cout <<  " not mounted";
	}
	cout << endl;
}
#endif

void TestStorageDevice::Test()
{
	list<StorageDevice> devs = StorageDevice::Devices();
#if 1
	cout << "Got " << devs.size() << " devices" << endl;

	for(const StorageDevice& dev: devs)
	{
		dump(dev);
		list<StorageDevice> parts = dev.Partitions();
		for(const auto& part: parts)
		{
			dump(part);
		}
	}
#endif
	CPPUNIT_ASSERT(devs.size() > 0 );

}
