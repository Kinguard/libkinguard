#include "StorageDevice.h"

#include <libopi/DiskHelper.h>
#include <libutils/FileUtils.h>

#include <utility>

namespace KGP
{

StorageDevice::StorageDevice(const string &devicename)
{
	using namespace Utils;
	if( File::DirExists("/sys/class/block/"s + devicename) )
	{
		this->device = OPI::DiskHelper::StorageDevice(devicename);
	}
	else
	{
		this->device = File::GetFileName(File::RealPath(devicename));
	}

	if( this->device.is_null() )
	{
		throw std::runtime_error("Unable to locate storage device: "s + devicename);
	}
}

list<StorageDevice> StorageDevice::Devices()
{
	list<StorageDevice> devices;

	json jdevs = OPI::DiskHelper::StorageDevices();

	for(const auto& jdev: jdevs)
	{
		devices.emplace_back( StorageDevice(jdev) );
	}

	return devices;
}

string StorageDevice::DeviceName() const
{
	return this->device["devname"].get<string>();
}

string StorageDevice::SysPath() const
{
	return this->device["syspath"].get<string>();
}

string StorageDevice::DevicePath() const
{
	return this->device["devpath"].get<string>();
}

string StorageDevice::LVMPath() const
{
	return this->device["dm-path"].get<string>();
}

string StorageDevice::LUKSPath() const
{
	return this->LVMPath();
}

string StorageDevice::Model() const
{
	return this->device["model"].get<string>();
}

list<string> StorageDevice::MountPoint() const
{
	list<string> mps;

	for( const auto& mp : this->device["mountpoint"])
	{
		mps.emplace_back(mp.get<string>());
	}
	return mps;
}

uint64_t StorageDevice::Blocks() const
{
	return this->device["blocks"];
}

uint64_t StorageDevice::Size() const
{
	return this->device["size"];
}

list<StorageDevice> StorageDevice::Partitions() const
{
	list<StorageDevice> parts;

	if( ! this->Is(StorageDevice::Partition) )
	{
		for(const auto& part: this->device["partitions"])
		{
			parts.emplace_back(StorageDevice(part));
		}
	}
	return parts;
}

bool StorageDevice::Is(StorageDevice::Characteristic c) const
{
	switch (c)
	{
	case Mounted:
		return this->device["mounted"];
		break;
	case Partition:
		return this->device["partition"];
		break;
	case Physical:
		return this->device["isphysical"];
		break;
	case ReadOnly:
		return this->device["readonly"];
		break;
	case Removable:
		return this->device["removable"];
		break;
	case RootDevice:
	{
		if( ! this->device["mounted"] )
		{
			return false;
		}
		for( const auto& mp : this->device["mountpoint"] )
		{
			if( mp.get<string>() == "/")
			{
				return true;
			}
		}
		return false;
		break;
	}
	case BootDevice:
	{
		if( this->device.contains("partitions") )
		{
			for(const auto& part: this->device["partitions"])
			{
				for(const auto& mp : part["mountpoint"])
				{
					if( mp.get<string>() == "/")
					{
						return true;
					}
				}
			}
		}
		return false;
		break;
	}
	case DeviceMapper:
		return this->device["dm"];
		break;
	case LVMDevice:
		return  this->device["dm-type"].get<string>() == "lvm";
		break;
	case LUKSDevice:
		return  this->device["dm-type"].get<string>() == "luks";
		break;
	}
	// Should never get here
	return false;
}

StorageDevice::StorageDevice(json dev): device(std::move(dev))
{

}

}
