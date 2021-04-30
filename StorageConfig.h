#ifndef STORAGECONFIG_H
#define STORAGECONFIG_H

#include <tuple>

#include <libopi/SysConfig.h>

#include "StorageDevice.h"

namespace KGP
{

namespace Storage
{
	namespace Model {
		/**
		 * @brief The Type enum, enumerates storage types
		 */
		enum Type
		{
			Undefined,	/**< Not known atm */
			Static,		/**< Fixed none configurable storage. */
			Dynamic,	/**< User configurable storage */
			Unknown,	/**< Unable to determine atm */
		};

		Type asType(const char* name);
		const char* asString(enum Type type);
	}

	namespace Physical
	{
		/**
		 * @brief The Type enum, enumerates known physical storage types
		 */
		enum Type
		{
			Undefined,	/**< Not known atm */
			None,		/**< No separate physical storage, use OS preconfigured storage */
			Partition,	/**< Use provided partition as backing storage */
			Block,		/**< Use separate block devices */
			Unknown,	/**< Unable to determine atm */
		};

		Type asType(const char* name);
		const char* asString(enum Type type);
	}

	namespace Logical
	{
		/**
		 * @brief The Type enum, enumerates known logical storage types
		 */
		enum Type
		{
			Undefined,	/**< Not known atm */
			None,		/**< No logical storage */
			LVM,		/**< LVM logical storage */
			Unknown,	/**< Unable to determine atm */
		};

		Type asType(const char* name);
		const char* asString(enum Type type);

		constexpr const char* DefaultLVMDevice = "/dev/pool/data";
		constexpr const char* DefaultLV = "data";
		constexpr const char* DefaultVG = "pool";

	}

	namespace Encryption
	{
		/**
		 * @brief The Type enum, enumerate known encryption types
		 */
		enum Type
		{
			Undefined,	/**< Not known atm */
			None,		/**< No encryption */
			LUKS,		/**< LUKS encryption */
			Unknown,	/**< Unable to deterimine atm */
		};

		Type asType(const char* name);
		const char* asString(enum Type type);

		constexpr const char* DefaultEncryptionDevice = "/dev/mapper/opi";
	}

	using  StorageType = std::tuple<Storage::Physical::Type, Storage::Logical::Type, Storage::Encryption::Type>;

	constexpr const char* PartitionName = "KGP";
}


/**
 * @brief The StorageConfig class
 *
 *        This class wraps the underlaying sysconfig storage configuration.
 */
class StorageConfig
{
public:

	StorageConfig();

	/**
	 * @brief isStatic, does this device have a static storage mapping?
	 *        if so, no storageconfiguration possible
	 * @return true if mapping is static
	 */
	bool isStatic();


	/**
	 * @brief isValid check if configuration is usable and valid
	 * @return true if valid
	 */
	bool isValid();

	/**
	 * @brief QueryPhysicalStorage Get possible physical storage types for device
	 * @return list of physical storage types
	 */
	list<Storage::Physical::Type> QueryPhysicalStorage();

	/**
	 * @brief PhysicalStorage, get current physical storage type
	 * @return Physical storage type
	 */
	Storage::Physical::Type PhysicalStorage();

	/**
	 * @brief PhysicalStorage set physical storage type
	 * @param type type to use
	 */
	void PhysicalStorage(Storage::Physical::Type type);

	/**
	 * @brief UsePhysicalStorage check if backing store uses storage type
	 * @param type
	 * @return true if backing store uses this type
	 */
	bool UsePhysicalStorage(Storage::Physical::Type type);

	/**
	 * @brief PhysicalDevices get physical devices used by storage
	 * @return list with strings describing each device
	 */
	list<string> PhysicalDevices();

	/**
	 * @brief PhysicalStorage set partition to use when physical type is partition
	 *        if type is not partition an exception will be thrown
	 *
	 * @param partition path to partition to use
	 */
	void PhysicalStorage(const string& partition);

	/**
	 * @brief PhysicalStorage set devices to use when physical type is Block
	 *        If type is not block an exception will be thrown
	 * @param devices
	 */
	void PhysicalStorage(const list<string>& devices);

	/**
	 * @brief QueryLogicalStorage Get possible logical storage types for device.
	 * @param type physical type to retrieve information on
	 * @return  list of logical storage types
	 */
	list<Storage::Logical::Type> QueryLogicalStorage(Storage::Physical::Type type);

	/**
	 * @brief LogicalStorage get current logical storage type
	 * @return Logical storage type
	 */
	Storage::Logical::Type LogicalStorage();

	/**
	 * @brief LogicalStorage set logical storage type
	 * @param type
	 */
	void LogicalStorage(Storage::Logical::Type type);

	/**
	 * @brief UseLogicalStorage check if storage uses this type
	 * @param type
	 * @return true if storage uses type
	 */
	bool UseLogicalStorage(Storage::Logical::Type type);

	/**
	 * @brief LogicalDevices get logical devices used by storage
	 *        currently only one device is supported
	 * @return list of devices
	 */
	list<string> LogicalDevices();

	/**
	 * @brief LogicalDevices set logical devices to use
	 * @param devices
	 */
	void LogicalDevices(const list<string>& devices);

	/**
	 * @brief LogicalDefaults set default values for logical storage
	 *        as defined in Storage::Logical
	 */
	void LogicalDefaults();


	/**
	 * @brief QueryEncryptionStorage, Get possible encryption methods for storage
	 *        using underlaing storage type
	 * @param phys Physical storage type for query
	 * @param logical Logical storage type for query
	 * @return List of encryption types
	 */
	list<Storage::Encryption::Type> QueryEncryptionStorage(Storage::Physical::Type phys, Storage::Logical::Type logical);

	/**
	 * @brief EncryptionStorage, get currently set encryption type
	 * @return Encryption type
	 */
	Storage::Encryption::Type EncryptionStorage();

	/**
	 * @brief EncryptionStorage set encryption type
	 * @param type
	 */
	void EncryptionStorage(Storage::Encryption::Type type);

	/**
	 * @brief UseEncryption check if storage uses this type
	 * @param type
	 * @return true if storage uses encryption type
	 */
	bool UseEncryption(Storage::Encryption::Type type);

	/**
	 * @brief EncryptionDevices get encryption devices used by device
	 *        currently only one device is supported
	 * @return list of encryption devices
	 */
	list<string> EncryptionDevices();

	/**
	 * @brief EncryptionDevices set encryption devices to use
	 *        currently only _one_ device is supported
	 * @param devices
	 */
	void EncryptionDevices(const list<string>& devices);


	/**
	 * @brief EncryptionDefaults setup device with default encryption
	 */
	void EncryptionDefaults();

	/**
	 * @brief StorageDevice get top storage device depending upon config
	 * @return device path to top storage device or empty string if unable
	 *         to determine.
	 */
	string StorageDevice();


	virtual ~StorageConfig() = default;

private:
	void parseConfig();

	bool encryptionValid();
	bool logicalValid();
	bool physicalValid();

	Storage::Model::Type		model;
	Storage::Physical::Type		physical;
	Storage::Logical::Type		logical;
	Storage::Encryption::Type	encryption;

	OPI::SysConfig syscfg;
};

} // NS KGP

#endif // STORAGECONFIG_H
