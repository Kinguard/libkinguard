#ifndef STORAGEDEVICE_H
#define STORAGEDEVICE_H

#include <json/json.h>
#include <string>
#include <list>

using namespace std;

namespace KGP
{

/**
 * @brief The StorageDevice class a thin wrapper around info from DiskHelper::StorageDevice
 */
class StorageDevice
{
public:

	enum Characteristic
	{
		Mounted,		/**< Partition is mounted				*/
		Partition,		/**< Device is a partition				*/
		Physical,		/**< Device is physical					*/
		ReadOnly,		/**< Device is a read only device		*/
		Removable,		/**< Device is removable				*/
		RootDevice,		/**< This is the root partition			*/
		DeviceMapper,	/**< This is a DM-device				*/
		LVMDevice,		/**< This is a logical volume device	*/
		LUKSDevice,		/**< This is an encrypted LUKS device	*/
		BootDevice,		/**< This is the system/boot device		*/
	};

	/**
	 * @brief StorageDevice create a storage device from devicename
	 * @param devicename as listed under /sys/class/block or complete
	 *        path to device under /dev
	 */
	StorageDevice(const string& devicename);

	/**
	 * @brief Devices, retreive all known devices in system
	 * @return list of devices
	 */
	static list<StorageDevice> Devices();

	/**
	 * @brief DeviceName short form device name. I.e. sda loop0
	 * @return device name
	 */
	string DeviceName() const;

	/**
	 * @brief SysPath get syspath of device
	 * @return path to device under /sys/class/block
	 */
	string SysPath() const;

	/**
	 * @brief DevicePath get device path of device
	 * @return path to device under /dev
	 */
	string DevicePath() const;

	/**
	 * @brief LVMPath if device is lvm device get path under /dev/pool
	 * @return string with path if lvm device empty string otherwise
	 */
	string LVMPath() const;

	/**
	 * @brief LUKSPath if device is a luks device get path under /dev/mapper
	 * @return string with path if luks device empty string otherwise
	 */
	string LUKSPath() const;


	/**
	 * @brief Model human readable name of device
	 * @return name of device
	 */
	string Model() const;

	/**
	 * @brief MountPoints mount point of mounted device
	 * @return list of mount paths
	 */
	list<string> MountPoint() const;

	/**
	 * @brief Blocks amount of 512B blocks
	 * @return amount of blocks
	 */
	uint64_t Blocks() const;

	/**
	 * @brief Size size of device in bytes
	 * @return size
	 */
	uint64_t Size() const;

	/**
	 * @brief Partitions list of partitions of device
	 * @return list of partitions
	 */
	list<StorageDevice> Partitions() const;

	/**
	 * @brief Is query device feature
	 * @param f
	 * @return true if device has characteristic
	 */
	bool Is(Characteristic c) const;


	virtual ~StorageDevice() = default;
private:
	StorageDevice(Json::Value  dev);
	Json::Value device;
};

} // NS KGP

#endif // STORAGEDEVICE_H
