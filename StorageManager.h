#ifndef STORAGEMANAGER_H
#define STORAGEMANAGER_H

#include <string>

#include "BaseManager.h"

using namespace std;

namespace KGP
{

class StorageManager: public BaseManager
{
private:
	StorageManager();
public:

	static StorageManager& Instance();

	bool Initialize(const string &password);

	bool Open(const string &password);

	static bool mountDevice(const string& destination);
	static void umountDevice();

	/**
	 * @brief UseLock tells if device needs some form of unlock
	 * @return true if unlock needed, false otherwise
	 */
	static bool UseLocking();

	/**
	 * @brief IsLocked tells us if device is locked
	 * @return true if locked
	 */
	static bool IsLocked();

	/**
	 * @brief DevicePath get path to top device, ie that what should be monted
	 * @return path to top device
	 */
	static string DevicePath();

	/**
	 * @brief StorageAreaExists check if storage area is existant
	 *        I.e. all used components available lvm, luks etc
	 * @return true if area exists
	 */
	static bool StorageAreaExists();

	/**
	 * @brief DeviceExists check if underlaying block device exists
	 * @return true if exists
	 */
	static bool DeviceExists();

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

	bool setupLUKS(const string& path, const string &password);
	bool unlockLUKS(const string& path, const string &password);
	bool InitializeLUKS(const string& device, const string &password, bool partition);

	bool setupStorageArea(const string& device);

	void RemoveLUKS();
	void RemoveLVM();
	bool CreateLVM();
	bool InitializeLVM(bool partition);

	bool device_new;
	bool initialized;

	// parameters from sysconfig
	string storagemount;	// MOUNTPOINT
	string luksdevice;		// LUKSDEVICE
	string lvmdevice;		// LVMDEVICE

};
} // Namespace KGP
#endif // STORAGEMANAGER_H
