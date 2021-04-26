#include "StorageConfig.h"

#include <libutils/Logger.h>

#include <libopi/SysInfo.h>
#include <libopi/DiskHelper.h>

using namespace Utils;
using namespace OPI;

#include <iostream>
using namespace std;

namespace KGP
{
namespace Storage
{
namespace Model
{
	Type asType(const char* name)
	{
		static const map<const string, Type> t =
		{
			{ "undefined",	Undefined },
			{ "static",		Static },
			{ "dynamic",	Dynamic },
			{ "unknown",	Unknown },
		};

		return t.at(name);
	}

	static constexpr array<const char*,4> typenames=
	{
		"undefined",
		"static",
		"dynamic",
		"unknown",
	};

	const char *asString(Storage::Model::Type type)
	{
		return typenames.at(type);
	}
} // NS Model

namespace Physical
{
	Type asType(const char* name)
	{
		static const map<const string, Type> t =
		{
			{ "undefined", Undefined },
			{ "none", None },
			{ "partition", Partition },
			{ "block", Block },
			{ "unknown", Unknown },
		};
		return t.at(name);
	}

	static constexpr array<const char*,5> typenames=
	{
		"undefined",
		"none",
		"partition",
		"block",
		"unknown"
	};

	constexpr const char *asString(Storage::Model::Type type)
	{
		return typenames.at(type);
	}
} // NS Phys


namespace Logical
{

	Type asType(const char* name)
	{
		static const map<const string, Type> t =
		{
			{ "undefined", Undefined },
			{ "none", None },
			{ "lvm", LVM },
			{ "unknown", Unknown },
		};
		return t.at(name);
	}

	static constexpr array<const char*,4> typenames=
	{
		"undefined",
		"none",
		"lvm",
		"unknown"
	};

	constexpr const char *asString(Storage::Model::Type type)
	{
		return typenames.at(type);
	}
} // NS Logical

namespace Encryption
{
	Type asType(const char* name)
	{
		static const map<const string, Type> t =
		{
			{ "undefined", Undefined },
			{ "none", None },
			{ "luks", LUKS },
			{ "unknown", Unknown },
		};
		return t.at(name);
	}

	constexpr const char *asString(Storage::Model::Type type)
	{
		constexpr const array<const char*,4> typenames=
		{
			"undefined",
			"none",
			"luks",
			"unknown"
		};

		return typenames.at(type);
	}
} // NS Encryption

} // NS Storage

static void initStorageConfig()
{
	logg << Logger::Notice << "Migrate/create sysconfig entries to use storage section" << lend;

	SysConfig cfg(true);

	if( cfg.HasKey("dns", "provider") && cfg.GetKeyAsString("dns", "provider") == "OpenProducts" )
	{
		// OP-Device

		logg << Logger::Debug << "Migrating OP Device" << lend;

		cfg.PutKey("storage", "model", "static" );
		cfg.PutKey("storage", "physical", "block" );

		cfg.PutKey("storage", "block_devices", list<string>{ sysinfo.StorageDevice() } );

		if( SysInfo::isOpi() )
		{
			cfg.PutKey("storage", "logical", "none" );
		}
		else
		{
			cfg.PutKey("storage", "logical", "lvm" );

			cfg.PutKey("storage", "lvm_device",	cfg.GetKeyAsString("filesystem", "lvmdevice") );
			cfg.PutKey("storage", "lvm_lv",		cfg.GetKeyAsString("filesystem", "lvmlv") );
			cfg.PutKey("storage", "lvm_vg",		cfg.GetKeyAsString("filesystem", "lvmvg"));
		}

		cfg.PutKey("storage", "encryption", "luks" );
		cfg.PutKey("storage", "luks_device", cfg.GetKeyAsString("filesystem", "luksdevice") );
	}
	else
	{
		logg << Logger::Debug << "Migrating/creating storage config for none OP Device" << lend;

		cfg.PutKey("storage", "model", "dynamic" );
		cfg.PutKey("storage", "physical", "undefined" );
		cfg.PutKey("storage", "logical", "undefined" );
		cfg.PutKey("storage", "encryption", "undefined" );
	}


	logg << Logger::Notice << "Migration completed" << lend;
}

StorageConfig::StorageConfig():
	model(Storage::Model::Undefined),
	physical(Storage::Physical::Undefined),
	logical(Storage::Logical::Undefined),
	encryption(Storage::Encryption::Undefined)

{

	if( !this->syscfg.HasScope("storage") )
	{
		// Migrate config, might belong in a ccheck script but fix here for now
		initStorageConfig();
	}

	this->parseConfig();
}

bool StorageConfig::isStatic()
{
	return this->model == Storage::Model::Static;
}

list<Storage::Physical::Type> StorageConfig::QueryPhysicalStorage()
{
	using namespace Storage::Physical;
	if( SysInfo::isOpi() || SysInfo::isArmada() )
	{
		return { Block };
	}

	return { None, Partition, Block };
}

Storage::Physical::Type StorageConfig::PhysicalStorage()
{
	return this->physical;
}

bool StorageConfig::UsePhysicalStorage(Storage::Physical::Type type)
{
	return this->physical == type;
}

list<string> StorageConfig::PhysicalDevices()
{
	if( this->physical == Storage::Physical::Partition )
	{
		return { this->syscfg.GetKeyAsString("storage","partition_path") };
	}

	if( this->physical == Storage::Physical::Block )
	{
		return this->syscfg.GetKeyAsStringList("storage","block_devices");
	}

	return {};
}

list<Storage::Logical::Type> StorageConfig::QueryLogicalStorage(Storage::Physical::Type type)
{
	using namespace Storage;
	using namespace Storage::Logical;
	if( SysInfo::isOpi() )
	{
		return {None};
	}

	if( SysInfo::isArmada() )
	{
		return {LVM};
	}

	switch (type) {
	case Physical::Undefined:
	case Physical::Unknown:
	case Physical::None:
		return {};
		break;
	case Physical::Partition:
	case Physical::Block:
		return {None, LVM};
		break;
	}
	// Should never get here
	return {};
}

Storage::Logical::Type StorageConfig::LogicalStorage()
{
	return this->logical;
}

bool StorageConfig::UseLogicalStorage(Storage::Logical::Type type)
{
	return this->logical == type;
}

list<string> StorageConfig::LogicalDevices()
{
	if( this->logical == Storage::Logical::LVM )
	{
		return { this->syscfg.GetKeyAsString("storage","lvm_device") };
	}
	return {};
}

list<Storage::Encryption::Type> StorageConfig::QueryEncryptionStorage(Storage::Physical::Type phys, Storage::Logical::Type logical)
{
	using namespace Storage;
	using namespace Storage::Encryption;

	// OPI & Keep, static config
	if( SysInfo::isOpi() || SysInfo::isArmada() )
	{
		return { LUKS };
	}

	// Currently need some block storage to be able to encrypt
	if( phys != Physical::Partition && phys != Physical::Block )
	{
		return { None };
	}

	if( logical == Logical::Undefined || logical == Logical::Unknown )
	{
		return { None };
	}

	return {None, LUKS};
}

Storage::Encryption::Type StorageConfig::EncryptionStorage()
{
	return this->encryption;
}

bool StorageConfig::UseEncryption(Storage::Encryption::Type type)
{
	return this->encryption == type;
}

list<string> StorageConfig::EncryptionDevices()
{
	if( this->encryption == Storage::Encryption::LUKS )
	{
		return {this->syscfg.GetKeyAsString("storage","luks_device") };
	}
	return {};
}

string StorageConfig::StorageDevice()
{

	// If we use encryption, that is top device.
	if( this->UseEncryption(Storage::Encryption::LUKS) )
	{
		return this->syscfg.GetKeyAsString("storage","luks_device");
	}

	//If no encryption, logical device could be top device
	if( this->UseLogicalStorage(Storage::Logical::LVM) )
	{
		return this->syscfg.GetKeyAsString("storage","lvm_device");
	}

	// Use unencrypted external drive? Can only manage one device!
	// This device is expected to have one partition that should be used
	if( this->UsePhysicalStorage( Storage::Physical::Block ) )
	{
		list<string> devs = this->syscfg.GetKeyAsStringList("storage", "block_devices");
		if( devs.size() != 1 )
		{
			logg << Logger::Error << "Unable to deterimine storage device. Got " << devs.size() << " disks" << lend;
			//TODO: Throw exception? This is a faulty config
			return "";
		}
		return DiskHelper::PartitionName(devs.front());
	}

	//Uses separate partition?
	if( this->UsePhysicalStorage( Storage::Physical::Partition ) )
	{
		return this->syscfg.GetKeyAsString("storage","partition_path");
	}

	logg << Logger::Notice << "Unable to determine final storage device path" << lend;

	return "";
}

void StorageConfig::parseConfig()
{
	using namespace Storage;

	this->model =		Model::asType( this->syscfg.GetKeyAsString("storage","model").c_str() );
	this->physical =	Physical::asType( this->syscfg.GetKeyAsString("storage","physical").c_str() );
	this->logical =		Logical::asType( this->syscfg.GetKeyAsString("storage","logical").c_str() );
	this->encryption =	Encryption::asType( this->syscfg.GetKeyAsString("storage","encryption").c_str() );

}
} // NS KGP
