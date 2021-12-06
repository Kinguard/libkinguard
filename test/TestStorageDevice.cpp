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
		 << " name "		<< dev.DeviceName()
		 << " path "		<< dev.DevicePath()
		 << " sysp "		<< dev.SysPath()
		 << " is boot "		<< dev.Is(StorageDevice::BootDevice)
			;
#if 0
	if( dev.Is(StorageDevice::Mounted) )
	{
		list<string> mps = dev.MountPoint();
		for( const auto& mp : mps)
		{
			cout << " mounted at " << mp;
			if( dev.Is(StorageDevice::RootDevice) )
			{
				cout << " and this is the root device";
			}
		}
	}
	else
	{
		cout <<  " not mounted";
	}
#endif
	cout << endl;
}
#endif

void TestStorageDevice::Test()
{
	if( true )
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

	// We should always have one boot device and one root partition
	bool isBoot = false;
	bool isRoot = false;

	for(const StorageDevice& dev: devs)
	{
		if( dev.Is(StorageDevice::BootDevice) )
		{
			isBoot = true;
		}
		list<StorageDevice> parts = dev.Partitions();
		for(const auto& part: parts)
		{
			if( part.Is(StorageDevice::RootDevice) )
			{
				isRoot = true;
			}
		}
	}

	CPPUNIT_ASSERT_MESSAGE( "Missing boot device", isBoot);
	CPPUNIT_ASSERT_MESSAGE( "Missing root device", isRoot);
	}
}
