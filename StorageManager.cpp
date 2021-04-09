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

StorageManager::StorageManager(): device_new(false), initialized(false)
{
	SysConfig cfg;

	this->storagemount = cfg.GetKeyAsString("filesystem", "storagemount");
	this->luksdevice = cfg.GetKeyAsString("filesystem", "luksdevice");
	this->lvmdevice = cfg.GetKeyAsString("filesystem", "lvmdevice");
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

bool StorageManager::mountDevice(const string &destination)
{
	// Work out what to mount
	string source = StorageManager::DevicePath();

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
	DiskHelper::Umount( StorageManager::DevicePath() );
}

/*
 * TODO: This has to be reworked. To complicated and hackish
 */
bool StorageManager::Initialize(const string& password)
{
	logg << Logger::Debug << "Storagemanager initialize" << lend;

	if( ! this->checkDevice( sysinfo.StorageDevice() ) )
	{
		return false;
	}

	string curdevice = sysinfo.StorageDevicePath();

	if( ! this->initialized )
	{
		logg << Logger::Debug << "Device not initialized, starting initialization"<<lend;

		logg << Logger::Debug << "Check if " << sysinfo.StorageDevicePath() <<  " is mounted"  << lend;

		try
		{
			if( DiskHelper::IsMounted( sysinfo.StorageDevicePath() ) != "" )
			{
				logg << Logger::Notice << "Device" << sysinfo.StorageDevicePath() << " seems mounted, try umount" << lend;
				DiskHelper::Umount( sysinfo.StorageDevicePath() );
			}
		}
		catch (ErrnoException& err)
		{
			logg << Logger::Notice << "Failed to check device: " << err.what() << lend;
		}

		bool partition = true;
		if( SysInfo::useLVM() )
		{
			if( ! this->InitializeLVM( partition ) )
			{
				return false;
			}
			curdevice = this->lvmdevice;
			partition = false;
		}

		if( SysInfo::useLUKS() )
		{
			if( ! this->InitializeLUKS( curdevice, password, partition ) )
			{
				return false;
			}
			curdevice = this->luksdevice;
		}
		this->initialized = true;
	}
	else
	{
		logg << Logger::Debug << "Device initialized, skip to setup"<<lend;
		if( SysInfo::useLVM() )
		{
			curdevice = this->lvmdevice;
		}

		if( SysInfo::useLUKS() )
		{
			curdevice = this->luksdevice;
		}
	}
	return this->setupStorageArea( curdevice );
}

bool StorageManager::Open(const string& password)
{
	if( SysInfo::useLUKS() )
	{
		// Use lvm or raw blockdevice?
		string ld = SysInfo::useLVM() ? this->lvmdevice : sysinfo.StorageDevicePath();

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
	return SysInfo::useLUKS();
}

bool StorageManager::IsLocked()
{
	Luks l( StorageManager::DevicePath() );

	return ! l.Active( StorageManager::DevicePath() );
}

string StorageManager::DevicePath()
{
	string source = sysinfo.StorageDevicePath();

	if( SysInfo::useLUKS() )
	{
		source = SysConfig().GetKeyAsString("filesystem", "luksdevice");
	}
	else if( SysInfo::useLVM() )
	{
		source = SysConfig().GetKeyAsString("filesystem", "lvmdevice");
	}

	return source;
}

bool StorageManager::StorageAreaExists()
{
	logg << Logger::Debug << "Check if storage area exists"<<lend;
	try
	{
		// With no underlaying device there can't be any upper layer either
		if( ! DiskHelper::DeviceExists( sysinfo.StorageDevice() ) )
		{
			return false;
		}

		// We need storage space on underlaying device. I.e. an sd-card is available in slot
		if( DiskHelper::DeviceSize( sysinfo.StorageDevice() ) == 0 )
		{
			return false;
		}

		if( SysInfo::useLVM() && ! DiskHelper::DeviceExists( SysConfig().GetKeyAsString("filesystem", "lvmdevice") ) )
		{
			return false;
		}

		// If we have a luks-volume on underlaying storage we assume we have a correct setup
		if( SysInfo::useLUKS() )
		{
			if( SysInfo::useLVM() && ! Luks::isLuks( SysConfig().GetKeyAsString("filesystem", "lvmdevice") ) )
			{
				return false;
			}

			if( ! SysInfo::useLVM() && ! Luks::isLuks( sysinfo.StorageDevicePath() ) )
			{
				return false;
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
	try
	{
		logg << Logger::Debug << "Resolving device: " << sysinfo.StorageDevice() <<lend;

		string device = File::RealPath( sysinfo.StorageDevice() );

		logg << Logger::Debug << "Checking device " << device << lend;

		if( ! DiskHelper::DeviceExists( device ) )
		{
			return false;
		}

		if( DiskHelper::DeviceSize( device ) == 0 )
		{
			return false;
		}

	}catch( std::exception& e)
	{
		logg << Logger::Notice << "Failed to check device: " << sysinfo.StorageDevice()
			 << "(" << e.what() <<")" <<lend;
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

		DiskHelper::FormatPartition( this->luksdevice, "OPI");
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

bool StorageManager::InitializeLUKS(const string &device, const string& password, bool partition )
{
	logg << Logger::Debug << "Initialize LUKS on device " << device <<lend;
	if( ! Luks::isLuks( device ) )
	{
		logg << Logger::Notice << "No luks volume on device " << device << " creating" << lend;

		if( partition )
		{
			logg << Logger::Debug << "Partitioning " << sysinfo.StorageDevice() << lend;
			DiskHelper::PartitionDevice( sysinfo.StorageDevice() );
		}

		if( ! this->setupLUKS( device, password ) )
		{
			return false;
		}

		this->device_new = true;
	}

	if( ! this->unlockLUKS( device, password ) )
	{
		logg << Logger::Notice << "Unable to unlock device" << lend;
		return false;
	}

	return true;
}

bool StorageManager::setupStorageArea(const string &device)
{
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

void StorageManager::RemoveLUKS()
{
	logg << Logger::Debug << "Remove any active LUKS volumes on storage area" << lend;
	try
	{
		if( SysInfo::useLVM() && Luks::isLuks( this->lvmdevice ) )
		{
			logg << Logger::Debug << "Try closing device" << lend;
			Luks l( this->lvmdevice );

			l.Close("opi" );
		}
	}
	catch( std::exception& e)
	{
		logg << Logger::Notice << "Failed to close LUKS" << e.what() << lend;
	}
	logg << Logger::Debug << "Remove LUKS done"<< lend;
}

void StorageManager::RemoveLVM()
{
	logg << Logger::Debug << "Remove any present LVM volumes"<<lend;
	try
	{
		LVM l;

		list<VolumeGroupPtr> vgs = l.ListVolumeGroups();
		for( auto& vg: vgs)
		{
			if( vg == nullptr )
			{
				logg << Logger::Error << "Got nullptr vg from listvgs"<< lend;
				continue;
			}
			list<LogicalVolumePtr> lvs = vg->GetLogicalVolumes();
			for( auto& lv: lvs)
			{
				if( lv == nullptr )
				{
					logg << Logger::Error << "Got nullptr lv from listlvs"<< lend;
					continue;
				}
				vg->RemoveLogicalVolume(lv);
			}

			l.RemoveVolumeGroup(vg);
		}

		//TODO: remove all PVs
	}
	catch( std::exception& e)
	{
		logg << Logger::Error << "Failed to remove lvms: " << e.what()<<lend;
	}
	logg << Logger::Debug << "Remove done"<<lend;
}

bool StorageManager::CreateLVM()
{
	LVM lvm;
	PhysicalVolumePtr pv;
	try
	{
		pv = lvm.CreatePhysicalVolume( File::RealPath( sysinfo.StorageDevicePath() ) );

		VolumeGroupPtr vg = lvm.CreateVolumeGroup( SysConfig().GetKeyAsString("filesystem", "lvmvg"), {pv} );

		LogicalVolumePtr lv = vg->CreateLogicalVolume( SysConfig().GetKeyAsString("filesystem", "lvmlv") );
	}
	catch( ErrnoException& err )
	{
		logg << Logger::Notice << "Create LVM failed: " << err.what() << lend;
		return false;
	}


	return true;
}



bool StorageManager::InitializeLVM(bool partition)
{
	logg << Logger::Debug << "Initialize LVM on " << this->lvmdevice << lend;
	try
	{
		if( ! StorageManager::StorageAreaExists() )
		{
			logg << Logger::Notice << "No LVM on device "<< sysinfo.StorageDevicePath()<<", creating"<<lend;

			// Unfortunately we cant assume a clean slate, there could be a partial/full lvm here
			// try to remove
			if( SysInfo::useLUKS() )
			{
				this->RemoveLUKS();
			}
			this->RemoveLVM();

			if ( partition )
			{
				logg << Logger::Debug << "Partitioning " << sysinfo.StorageDevice() << lend;
				DiskHelper::PartitionDevice( sysinfo.StorageDevice() );
			}

			// We have a synchronization problem trying to figure out when partition is finalized
			// and u-dev have created /dev entries. Thus we back of twice to hopefully let udev
			// do its thing.
			logg << Logger::Debug << "Sleep and wait for udev"<< lend;
			sleep(3);

			int retries = 100;
			while( retries > 0 )
			{
				if( this->checkDevice( sysinfo.StorageDevicePath() ) )
				{
						break;
				}
				retries--;
			}

			if( retries == 0 )
			{
				logg << Logger::Error << "No such device avaliable (" << sysinfo.StorageDevicePath() << lend;
				return false;
			}

			sleep(3);

			// Setup device
			retries = 100;
			while( retries > 0)
			{
				if( this->CreateLVM() )
				{
					break;
				}
				retries--;
			}
			if( retries == 0 )
			{
				logg << Logger::Error << "Failed to create lvm"<<lend;
				return false;
			}
			this->device_new = true;
		}
		else
		{
			logg << Logger::Debug << "Storage area exists, not creating lvm"<<lend;
		}

	}catch( std::exception& e)
	{
		logg << Logger::Error << "Unable to setup lvm: " << e.what() << lend;
		return false;
	}

	// We should now have a valid LVM device to work with
	return true;
}
} // Namespace KGP
