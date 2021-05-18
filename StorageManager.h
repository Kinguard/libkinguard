#ifndef STORAGEMANAGER_H
#define STORAGEMANAGER_H

#include <string>
#include <list>


#include "BaseManager.h"
#include "StorageConfig.h"

using namespace std;

namespace KGP
{

class StorageManager: public BaseManager
{
private:
	StorageManager();
public:

	static StorageManager& Instance();

	/**
	 * @brief Initialize setup storagedevice and mount it
	 * @param password to use if device requires locking
	 * @return true upon success
	 */
	bool Initialize(const string &password);

	/**
	 * @brief Open unlock device if it uses locking
	 * @param password
	 * @return true upon success
	 */
	bool Open(const string &password);

	bool mountDevice(const string& destination);
	void umountDevice();

	/**
	 * @brief UseLock tells if device needs some form of unlock
	 * @return true if unlock needed, false otherwise
	 */
	bool UseLocking();

	/**
	 * @brief UseLogicalStorage tells if device uses a logical storage
	 *        device or directly uses physical storage
	 * @return true if device uses logical storage
	 */
	bool UseLogicalStorage();

	/**
	 * @brief IsLocked tells us if device is locked
	 * @return true if locked
	 */
	bool IsLocked();

	/**
	 * @brief DevicePath get path to top device, ie that what should be mounted
	 * @return path to top device
	 */
	string DevicePath();

	/**
	 * @brief StorageAreaExists check if storage area is existant
	 *        I.e. all used components available lvm, luks etc
	 * @return true if area exists
	 */
	bool StorageAreaExists();

	/**
	 * @brief DeviceExists check if underlaying block device exists
	 * @return true if exists
	 */
	bool DeviceExists();

	/**
	 * @brief QueryPhysical for this device which physical storage
	 *        types are available?
	 *
	 *        The difference between these and the ones in StorageConfig
	 *        is that these return what is possible on _this_ sytem
	 *        while StorageConfig says what combinations is possible.
	 *
	 * @return list with physical storage types
	 */
	list<Storage::Physical::Type> QueryPhysical();

	/**
	 * @brief QueryLogical, for this device which logical storage
	 *        types are available?
	 * @return list with logical storage types
	 */
	list<Storage::Logical::Type> QueryLogical(Storage::Physical::Type types);

	/**
	 * @brief QueryEncryption, for this device which encryption storage
	 *        types are available?
	 * @return list whit encryption storage types
	 */
	list<Storage::Encryption::Type> QueryEncryption(Storage::Physical::Type phys, Storage::Logical::Type log);


	/**
	 * @brief QueryStorageDevices get all suitable physical storage devices
	 *
	 *        This method retrieves all disks suitable for an install
	 *
	 * @return list with devices
	 */
	list<StorageDevice> QueryStorageDevices();

	/**
	 * @brief QueryStoragePartitions get all suitable partitions of system disk
	 *        Get all partitions on system disk suitable for storage
	 *
	 * @return list with partition devices
	 */
	list<StorageDevice> QueryStoragePartitions();

	/**
	 * @brief Size get size of raw device
	 * @return size of device
	 */
	// TODO: revise this since it is a bit ambigous
	static size_t Size();

	string Error();

	virtual ~StorageManager();
private:

	bool checkDevice(const string& path);

	bool partitionDisks(const list<string>& devs);

	bool setupLUKS(const string& path, const string &password);
	bool unlockLUKS(const string& path, const string &password);
	bool InitializeLUKS(const string &device);

	bool setupStorageArea();

	bool CreateLVM(const list<string>& physdevs);

	/*
	 * Functions to handle the different setup options/scenarios
	 */
	bool InitPNNHandler();
	bool InitPLNHandler();
	bool InitPNLHandler();
	bool InitPLLHandler();

	bool InitBNNHandler();
	bool InitBLNHandler();
	bool InitBNLHandler();
	bool InitBLLHandler();


	/**
	 * @brief getLogicalDevice try get unique logical device
	 * @return device path to device or empty string ""
	 */
	string getLogicalDevice();

	/**
	 * @brief getPysicalDevice try get unique physical device if available
	 * @return string to device or empty string ""
	 */
	string getPysicalDevice();

	/**
	 * @brief getEncryptionDevice try get unique encryption device if available
	 * @return string to device or empty string ""
	 */
	string getEncryptionDevice();

	bool dosyncstorage;
	bool initialized;

	string encryptionpassword;

	StorageConfig storageConfig;
};
} // Namespace KGP
#endif // STORAGEMANAGER_H
