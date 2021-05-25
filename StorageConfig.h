#ifndef STORAGECONFIG_H
#define STORAGECONFIG_H

#include <tuple>
#include <cstring>

#include <libopi/SysConfig.h>

#include "StorageDevice.h"

namespace KGP
{

namespace Storage
{

	template<class Type> struct TypeEntry
	{
		const char*	name;				/**< Identifying name (For use in config files etc) */
		const char* description;		/**< Human readable description of type */
		Type type;						/**< Type identifier */
	};

	/**
	 * @brief base object for storage objects
	 *
	 */
	template<class Der, class T> class Base
	{
	private:

		T type;						/**< Which type */
		int priority{};				/**< Prio, for sorting */
		const char *name{};			/**< Type name */
		const char *description{};	/**< Human readable description */

		/**
		 * @brief setMembers populate members from type table
		 * @param t type to use for initialization
		 */
		void setMembers(T t)
		{
			for(const auto& e: Der::TypeEntries())
			{
				if( e.type == t )
				{
					this->name = e.name;
					this->description = e.description;
					return;
				}
			}
			throw std::out_of_range("No such type");
		}

	public:

		/**
		 * @brief Base construct object upon given type
		 * @param t
		 */
		Base(T t):
			type(t),
			priority(t)
		{
			this->setMembers(t);
		}

		/**
		 * @brief Name get name of this storage type
		 * @return string with machine version of type
		 */
		[[nodiscard]] const char* Name() const { return this->name; }

		/**
		 * @brief Description get human readable description of storage type
		 * @return string with description
		 */
		[[nodiscard]] const char* Description() const { return this->description;}

		/**
		 * @brief Type get type identifier for storage type
		 * @return type
		 */
		[[nodiscard]] T Type() const { return this->type; }

		/**
		 * @brief operator < used for sorting items
		 * @param obj
		 * @return true if this object is less than provided object
		 */
		bool operator<(const Base<Der,T>& obj){ return priority < obj.priority;}

		/**
		 * @brief operator == used for equal comparison
		 * @param obj
		 * @return true if objects equal
		 */
		bool operator==(const Base<Der,T>& obj){ return priority == obj.priority;}

		/**
		 * @brief fromType construct list of storage objects
		 * @param typelist list with types
		 * @return list with objects
		 */
		static std::list<Der> fromType(const std::list<T>& typelist)
		{
			std::list<Der> ret;
			for(const auto& type: typelist)
			{
				ret.emplace_back(Der(type));
			}
			return ret;
		}

		/**
		 * @brief toName retrieve machine descriptive name of type
		 * @param type
		 * @return machine descriptive text
		 */
		static const char* toName(T type)
		{
			for(const auto& entry: Der::TypeEntries())
			{
				if( entry.type == type )
				{
					return entry.name;
				}
			}
			throw std::out_of_range("Type not found");
		}

		/**
		 * @brief toType retrieve type from machine name of type
		 * @param name name of type
		 * @return type of object
		 */
		static T toType(const char* name)
		{
			for(const auto& entry: Der::TypeEntries())
			{
				if( strcmp(entry.name, name) == 0)
				{
					return entry.type;
				}
			}
			throw std::out_of_range("Element "s + name + " not found"s);
		}

		/**
		 * @brief fromName construct an object from a descriptive name
		 * @param name name of object
		 * @return object
		 */
		static Der fromName(const char* name)
		{
			for(const auto& entry: Der::TypeEntries())
			{
				if( strcmp(entry.name, name) == 0)
				{
					return Der(entry.type);
				}
			}
			throw std::out_of_range("Element "s + name + " not found"s);
		}

	};

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

		class Model: public Base<Model,Type>
		{
		public:
			Model(enum Type type):Base<Model,enum Type>(type){}

			static const vector<TypeEntry<enum Type>>& TypeEntries();

		};
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

		class Physical: public Base<Physical,Type>
		{
		public:
			Physical(enum Type type):Base<Physical,enum Type>(type){}

			static const vector<TypeEntry<enum Type>>& TypeEntries();

		};
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

		class Logical: public Base<Logical,Type>
		{
		public:
			Logical(enum Type type):Base<Logical,enum Type>(type){}

			static const vector<TypeEntry<enum Type>>& TypeEntries();

		};

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

		class Encryption: public Base<Encryption,Type>
		{
		public:
			Encryption(enum Type type):Base<Encryption,enum Type>(type){}

			static const vector<TypeEntry<enum Type>>& TypeEntries();

		};

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
	Storage::Physical::Physical PhysicalStorage();

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
	Storage::Logical::Logical LogicalStorage();

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
	Storage::Encryption::Encryption EncryptionStorage();

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

	Storage::Model::Model			model;
	Storage::Physical::Physical		physical;
	Storage::Logical::Logical		logical;
	Storage::Encryption::Encryption	encryption;

	OPI::SysConfig syscfg;
};

} // NS KGP

#endif // STORAGECONFIG_H
