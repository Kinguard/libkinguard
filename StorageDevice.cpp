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

	if( this->device == Json::nullValue )
	{
		throw std::runtime_error("Unable to locate storage device: "s + devicename);
	}
}

list<StorageDevice> StorageDevice::Devices()
{
	list<StorageDevice> devices;

	Json::Value jdevs = OPI::DiskHelper::StorageDevices();

	for(const auto& jdev: jdevs)
	{
		devices.emplace_back( StorageDevice(jdev) );
	}

	return devices;
}

string StorageDevice::DeviceName() const
{
	return this->device["devname"].asString();
}

string StorageDevice::SysPath() const
{
	return this->device["syspath"].asString();
}

string StorageDevice::DevicePath() const
{
	return this->device["devpath"].asString();
}

string StorageDevice::LVMPath() const
{
	return this->device["dm-path"].asString();
}

string StorageDevice::LUKSPath() const
{
	return this->LVMPath();
}

string StorageDevice::Model() const
{
	return this->device["model"].asString();
}

string StorageDevice::MountPoint() const
{
	return this->device["mountpoint"].asString();
}

uint64_t StorageDevice::Blocks() const
{
	return this->device["blocks"].asUInt64();
}

uint64_t StorageDevice::Size() const
{
	return this->device["size"].asUInt64();
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
		return this->device["mounted"].asBool();
		break;
	case Partition:
		return this->device["partition"].asBool();
		break;
	case Physical:
		return this->device["isphysical"].asBool();
		break;
	case ReadOnly:
		return this->device["readonly"].asBool();
		break;
	case Removable:
		return this->device["removable"].asBool();
		break;
	case RootDevice:
		return this->device["mounted"].asBool() && this->device["mountpoint"].asString() == "/";
		break;
	case DeviceMapper:
		return this->device["dm"].asBool();
		break;
	case LVMDevice:
		return  this->device["dm-type"].asString() == "lvm";
		break;
	case LUKSDevice:
		return  this->device["dm-type"].asString() == "luks";
		break;
	}
	// Should never get here
	return false;
}

StorageDevice::StorageDevice(Json::Value dev): device(std::move(dev))
{

}

}
