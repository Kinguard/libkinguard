#include "StorageManager.h"

#include "Config.h"

#include <libutils/FileUtils.h>
#include <libutils/Logger.h>

#include <libopi/LVM.h>
#include <libopi/Luks.h>
#include <libopi/SysInfo.h>
#include <libopi/SysConfig.h>
#include <libopi/DiskHelper.h>

using namespace Utils;
using namespace OPI;

namespace KGP
{


StorageManager &StorageManager::Instance()
{
	static StorageManager mgr;

	return mgr;
}

StorageManager::StorageManager():
	device_new(false),
	initialized(false)
{
}

/**
 * @brief checkDevice check if device is available, wait a while and retry if
 *        currently not available.
 * @param path Path to device to check
 * @return
 */
bool StorageManager::checkDevice(const string& path)
{
	logg << Logger::Debug << "Check device " << path << lend;
	int retries = 50;
	bool done=false;
	do
	{
		try
		{
			logg << Logger::Debug << "Checking device"<<lend;
			done = DiskHelper::DeviceExists( Utils::File::RealPath( path ) );
		}
		catch(std::runtime_error& err)
		{
			logg << Logger::Debug << "Unable to probe device: "<< err.what() << lend;
		}
		if( !done && retries > 0 )
		{
			logg << Logger::Debug << "Device not yet available, waiting" << lend;
			usleep(5000);
		}
	}while( !done && retries-- > 0);

	if ( ! done )
	{
		logg << Logger::Notice << "Unable to locate device, aborting" << lend;
		this->global_error = "Unable to locate storage device";
		return false;
	}

	logg << Logger::Debug << "Device " << path << " avaliable" << lend;
	return true;
}

bool StorageManager::partitionDisks(const list<string>& devs)
{
	try
	{
		for( const auto& pv : devs)
		{
			if( !DiskHelper::DeviceExists(pv) )
			{
				logg << Logger::Error << "Device doesn't exist: " << pv << lend;
				return false;
			}

			logg << Logger::Debug << "Partition: " << pv << lend;
			DiskHelper::PartitionDevice(File::RealPath(pv));
		}

	}
	catch (std::runtime_error& err)
	{
		logg << Logger::Error << "Failed to partition disk: " << err.what() << lend;
		return false;
	}

	// Check proper setup
	for( const auto& pv : devs)
	{
		if( ! this->checkDevice( DiskHelper::PartitionName(pv) ) )
		{
			logg << Logger::Error << "Device partition missing!" << lend;
			return false;
		}
	}

	return true;
}

bool StorageManager::mountDevice(const string &destination)
{
	// Work out what to mount
	string source = this->DevicePath();

	logg << Logger::Debug << "Mount "<< source << " device at " << destination << lend;

	try
	{
		// Make sure device is not mounted (Should not happen)
		if( DiskHelper::IsMounted( source ) != "" )
		{
			DiskHelper::Umount( source );
		}

	}
	catch ( ErrnoException& err)
	{
		logg << Logger::Error << "Failed to make sure storage not mounted: " << err.what() << lend;
		return false;
	}

	try
	{
		DiskHelper::Mount( source , destination );
	}
	catch( ErrnoException& err)
	{
		logg << Logger::Error << "Failed to mount storage device: " << err.what() << lend;

		// Sometimes pclose failes waiting on process, in these cases operation is most likely
		// successful and we should not error out.
		if( errno == ECHILD )
		{
			if( DiskHelper::IsMounted( source ) != "" )
			{
				logg << Logger::Notice << "Storage is mounted, ignore previous error" << lend;
				return true;
			}
		}
		return false;
	}

	return true;
}

void StorageManager::umountDevice()
{
	DiskHelper::Umount( this->DevicePath() );
}

bool StorageManager::Initialize(const string& password)
{
	using namespace Storage;

	logg << Logger::Debug << "Storagemanager initialize storage" << lend;


	if( this->storageConfig.UsePhysicalStorage(Physical::None) )
	{
		logg << Logger::Notice << "Device dont use separate physical backing store, skip storage initialization" << lend;
		return true;
	}

	this->encryptionpassword = password;

	if ( ! this->initialized )
	{
		logg << Logger::Debug << "Device not initialized, starting initialization"<<lend;

		const map<StorageType, std::function<bool()>> smap =
		{
			{ {Physical::Partition,	Logical::None,	Encryption::None}, [this](){ return this->InitPNNHandler();} },
			{ {Physical::Partition,	Logical::LVM,	Encryption::None}, [this](){ return this->InitPLNHandler();} },
			{ {Physical::Partition,	Logical::None,	Encryption::LUKS}, [this](){ return this->InitPNLHandler();} },
			{ {Physical::Partition,	Logical::LVM,	Encryption::LUKS}, [this](){ return this->InitPLLHandler();} },
			{ {Physical::Block,		Logical::None,	Encryption::None}, [this](){ return this->InitBNNHandler();} },
			{ {Physical::Block,		Logical::LVM,	Encryption::None}, [this](){ return this->InitBLNHandler();} },
			{ {Physical::Block,		Logical::None,	Encryption::LUKS}, [this](){ return this->InitBNLHandler();} },
			{ {Physical::Block,		Logical::LVM,	Encryption::LUKS}, [this](){ return this->InitBLLHandler();} },
		};

		// Workout setup scenario
		Storage::StorageType stype = make_tuple(
					this->storageConfig.PhysicalStorage(),
					this->storageConfig.LogicalStorage(),
					this->storageConfig.EncryptionStorage());

		const auto& setupselection = smap.find(stype);

		if( setupselection == smap.end() )
		{
			logg << Logger::Emerg << "Undefined setup configurartion" << lend;
			return false;
		}

		try {
			if( ! setupselection->second() )
			{
				logg << Logger::Error << "Failed to setup storage" << lend;
				return false;
			}
		}
		catch ( std::runtime_error& err)
		{
			logg << Logger::Error << "Storage setup failed with exception: " << err.what() << lend;
			return false;
		}

		this->initialized = true;
	}

	return this->setupStorageArea();
}

bool StorageManager::Open(const string& password)
{
	if( this->UseLocking() )
	{
		// Use lvm or raw blockdevice?
		string ld = this->UseLogicalStorage() ? this->getLogicalDevice() : this->getPysicalDevice();

		Luks l( ld );

		if( ! l.Active("opi") )
		{
			logg << Logger::Debug << "Activating LUKS volume"<<lend;
			if ( !l.Open("opi", password) )
			{
				logg << Logger::Debug << "Failed to openLUKS volume on "<<sysinfo.StorageDevicePath()<< lend;
				this->global_error = "Unable to unlock crypto storage. (Wrong password?)";
				return false;
			}
		}

	}
	return true;
}

bool StorageManager::UseLocking()
{
	return this->storageConfig.UseEncryption(Storage::Encryption::LUKS);
}

bool StorageManager::UseLogicalStorage()
{
	// Currently only use LVM
	return this->storageConfig.UseLogicalStorage(Storage::Logical::LVM);
}

bool StorageManager::IsLocked()
{

	if( ! this->UseLocking() )
	{
		return false;
	}

	;
	Luks l( this->DevicePath() );

	return ! l.Active( this->DevicePath() );
}

string StorageManager::DevicePath()
{
	return this->storageConfig.StorageDevice();
}

bool StorageManager::StorageAreaExists()
{
	logg << Logger::Debug << "Check if storage area exists"<<lend;
	try
	{

		list<string> pdevs = this->storageConfig.PhysicalDevices();

		if( pdevs.size() == 0 )
		{
			logg << Logger::Error << "Missing physical devices in config" << lend;
			return false;
		}

		// Need physical storage
		for( const auto& pdev: pdevs )
		{
			if( ! DiskHelper::DeviceExists( pdev ) )
			{
				logg << Logger::Notice << "Device " << pdev << " doesnt exist" << lend;
				return false;
			}

			// We need storage space on underlaying device. I.e. an sd-card is available in slot
			if( DiskHelper::DeviceSize( pdev ) == 0 )
			{
				logg << Logger::Notice << "Device " << pdev << " has no space" << lend;
				return false;
			}
		}

		if( this->storageConfig.UseLogicalStorage( Storage::Logical::LVM) )
		{
			list<string> ldevs = this->storageConfig.LogicalDevices();

			if( ldevs.size() == 0 )
			{
				logg << Logger::Error << "Logical storage selected but no device specified" << lend;
				return false;
			}

			for(const auto& ldev: ldevs)
			{
				if( !DiskHelper::DeviceExists( ldev ) )
				{
					logg << Logger::Debug << "Logical device " << ldev <<" not created" << lend;
					return false;
				}
			}

		}

		if( this->storageConfig.UseEncryption(Storage::Encryption::LUKS) )
		{
			list<string> devs;
			if( this->storageConfig.UseLogicalStorage( Storage::Logical::LVM) )
			{
				devs = this->storageConfig.LogicalDevices();
			}
			else
			{
				devs = this->storageConfig.PhysicalDevices();
			}

			for(const auto& dev: devs)
			{
				if( ! Luks::isLuks( dev ) )
				{
					logg << Logger::Debug << "No LUKS on device " << dev << lend;
					return false;
				}
			}

		}
	}
	catch( std::exception& e)
	{
		logg << Logger::Notice << "Failed to check" << StorageManager::DevicePath()<< ": " << e.what() <<lend;
		return false;
	}

	return true;
}

bool StorageManager::DeviceExists()
{

	list<string> devs = this->storageConfig.PhysicalDevices();

	try
	{

		for( const auto& dev: devs)
		{
			logg << Logger::Debug << "Resolving device: " << dev <<lend;

			string device = File::RealPath( dev );

			logg << Logger::Debug << "Checking device " << device << lend;

			if( ! DiskHelper::DeviceExists( device ) )
			{
				return false;
			}

			if( DiskHelper::DeviceSize( device ) == 0 )
			{
				return false;
			}

		}

	}catch( std::exception& e)
	{
		logg << Logger::Notice << "Failed to check device. (" << e.what() <<")" <<lend;
		return false;
	}
	return true;
}

size_t StorageManager::Size()
{
	return DiskHelper::DeviceSize( sysinfo.StorageDevicePath() );
}

string StorageManager::Error()
{
	return this->global_error;
}

StorageManager::~StorageManager() = default;

bool StorageManager::setupLUKS(const string &path, const string& password)
{
	try
	{
		Luks l( Utils::File::RealPath( path ) );
		l.Format( password );

		if( ! l.Open("opi", password ) )
		{
			this->global_error = "Wrong password";
			return false;
		}
	}
	catch( std::runtime_error& err)
	{
		logg << Logger::Notice << "Failed to format device: "<<err.what()<<lend;
		return false;
	}

	return true;
}

bool StorageManager::unlockLUKS(const string &path, const string& password)
{
	Luks l( path );

	if( ! l.Active("opi") )
	{
		logg << Logger::Debug << "Activating LUKS volume"<<lend;
		if ( !l.Open("opi", password ) )
		{
			this->global_error = "Wrong password";
			return false;
		}
	}

	return true;
}

/*
 * Initialize Luks on lower level block device i.e. lvm or physical device
 */
bool StorageManager::InitializeLUKS(const string& device )
{

	logg << Logger::Debug << "Initialize LUKS on device " << device <<lend;
	if( ! Luks::isLuks( device ) )
	{
		if( ! this->setupLUKS( device, this->encryptionpassword ) )
		{
			return false;
		}

		this->device_new = true;
	}

	if( ! this->unlockLUKS( device, this->encryptionpassword ) )
	{
		logg << Logger::Notice << "Unable to unlock device" << lend;
		return false;
	}

	return true;
}

bool StorageManager::setupStorageArea()
{
	string device = this->DevicePath();
	try
	{
		const string mountpoint = SysConfig().GetKeyAsString("filesystem", "storagemount");
		// Make sure device is not mounted (Should not happen)
		if( DiskHelper::IsMounted( device ) != "" )
		{
			DiskHelper::Umount( device );
		}

		if( this->device_new )
		{
			logg << Logger::Debug << "Sync template data to storage device " << device <<lend;
			// Sync data from root to storage
			DiskHelper::Mount( device , TMP_MOUNT );

			DiskHelper::SyncPaths(mountpoint, TMP_MOUNT);

			DiskHelper::Umount(device);

			this->device_new = false;
		}

		// Mount in final place
		DiskHelper::Mount( device, mountpoint );
	}
	catch( ErrnoException& err)
	{
		logg << Logger::Error << "Finalize unlock failed: " << err.what() << lend;
		this->global_error = "Unable to access storage device";
		return false;
	}

	return true;
}

bool StorageManager::CreateLVM(const list<string>& physdevs)
{
	LVM lvm;
	list<PhysicalVolumePtr> pvs;
	try
	{
		for(const auto& pdev : physdevs)
		{
			logg << Logger::Debug << "Adding " << pdev << " to volumegroup" << lend;
			pvs.emplace_back(lvm.CreatePhysicalVolume( File::RealPath( pdev  ) ) );
		}

		VolumeGroupPtr vg = lvm.CreateVolumeGroup( SysConfig().GetKeyAsString("storage", "lvm_vg"), pvs );

		LogicalVolumePtr lv = vg->CreateLogicalVolume( SysConfig().GetKeyAsString("storage", "lvm_lv") );
	}
	catch( ErrnoException& err )
	{
		logg << Logger::Notice << "Create LVM failed: " << err.what() << lend;
		return false;
	}

	return true;
}

bool StorageManager::InitPNNHandler()
{
	ScopedLog log("Init Partition|None|None");

	DiskHelper::FormatPartition(this->getPysicalDevice(), Storage::PartitionName );

	return true;
}

bool StorageManager::InitPLNHandler()
{
	ScopedLog log("Init Partition|LVM|None");

	if( !this->CreateLVM({ this->getPysicalDevice() }) )
	{
		return false;
	}

	DiskHelper::FormatPartition(this->getLogicalDevice(), Storage::PartitionName );

	return true;
}

bool StorageManager::InitPNLHandler()
{
	ScopedLog log("Init Partition|None|LUKS");

	if( ! this->InitializeLUKS( this->getPysicalDevice() ) )
	{
		return false;
	}

	DiskHelper::FormatPartition( this->getEncryptionDevice(), Storage::PartitionName );

	return true;
}

bool StorageManager::InitPLLHandler()
{
	ScopedLog log("Init Partition|LVM|LUKS");

	if( !this->CreateLVM({ this->getPysicalDevice() }) )
	{
		return false;
	}

	if( ! this->InitializeLUKS( this->getLogicalDevice() ) )
	{
		return false;
	}

	DiskHelper::FormatPartition( this->getEncryptionDevice(), Storage::PartitionName );

	return true;
}

bool StorageManager::InitBNNHandler()
{
	ScopedLog log("Init Block|None|None");

	if( !this->partitionDisks({ this->getPysicalDevice() } ) )
	{
		return false;
	}

	DiskHelper::FormatPartition( DiskHelper::PartitionName( this->getPysicalDevice() ), Storage::PartitionName);

	return true;
}

bool StorageManager::InitBLNHandler()
{
	ScopedLog log("Init Block|LVM|None");

	if( ! this->partitionDisks( this->storageConfig.PhysicalDevices() ) )
	{
		return false;
	}

	list<string> parts;
	for(const auto& dev: this->storageConfig.PhysicalDevices() )
	{
		parts.emplace_back(DiskHelper::PartitionName(dev));
	}

	if( ! this->CreateLVM( parts ) )
	{
		return false;
	}

	DiskHelper::FormatPartition( this->getLogicalDevice(), Storage::PartitionName );

	return true;
}

bool StorageManager::InitBNLHandler()
{
	ScopedLog log("Init Block|None|LUKS");

	if( !this->partitionDisks({ this->getPysicalDevice() } ) )
	{
		return false;
	}

	if( this->InitializeLUKS( DiskHelper::PartitionName( this->getPysicalDevice())) )
	{
		return false;
	}

	DiskHelper::FormatPartition( this->getEncryptionDevice(), Storage::PartitionName );

	return true;
}

bool StorageManager::InitBLLHandler()
{
	ScopedLog log("Init Block|LVM|LUKS");

	if( ! this->partitionDisks( this->storageConfig.PhysicalDevices() ) )
	{
		return false;
	}

	list<string> parts;
	for(const auto& dev: this->storageConfig.PhysicalDevices() )
	{
		parts.emplace_back(DiskHelper::PartitionName(dev));
	}

	if( ! this->CreateLVM( parts ) )
	{
		return false;
	}

	if( this->InitializeLUKS( this->getLogicalDevice() ) )
	{
		return false;
	}

	DiskHelper::FormatPartition( this->getEncryptionDevice(), Storage::PartitionName );

	return true;
}

string StorageManager::getLogicalDevice()
{
	list<string> ldevs = this->storageConfig.LogicalDevices();
	if( ldevs.size() != 1 )
	{
		logg << Logger::Notice << "Wrong amount of logical devices got:" << ldevs.size() << " assumed 1" << lend;
		return "";
	}
	return ldevs.front();
}

string StorageManager::getPysicalDevice()
{
	list<string> pdevs = this->storageConfig.PhysicalDevices();
	if( pdevs.size() != 1 )
	{
		logg << Logger::Notice << "Wrong amount of physical devices got:" << pdevs.size() << " assumed 1" << lend;
		return "";
	}
	return pdevs.front();
}

string StorageManager::getEncryptionDevice()
{
	list<string> edevs = this->storageConfig.EncryptionDevices();
	if( edevs.size() != 1 )
	{
		logg << Logger::Notice << "Wrong amount of encryption devices got:" << edevs.size() << " assumed 1" << lend;
		return "";
	}
	return edevs.front();
}

} // Namespace KGP
