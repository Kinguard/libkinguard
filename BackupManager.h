#ifndef BACKUPMANAGER_H
#define BACKUPMANAGER_H

#include "BaseManager.h"

#include <libopi/BackupHelper.h>
#include <libutils/ClassTools.h>

#include <libopi/CryptoHelper.h>

#include <string>

#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

namespace KGP
{

class BackupManager: public KGP::BaseManager
{
private:
	BackupManager();
public:
	static BackupManager& Instance();

	/**
	 * @brief Configure, set current configuration for backup
	 * @param cfg json value with configuration.
	 *
	 * @example currently master password is required
	 *
	 * { "password" : "secret master password" }
	 *
	 */
	static void Configure(const json& cfg);

	/**
	 * @brief GetBackups
	 * @return Json:object with keys as sources and each key with list of backups
	 *
	 * @example { "local": ("2012-02-13","2012-04-05"), "remote" : ("date1", "date2")
	 */
	json GetBackups();

	/**
	 * @brief RestoreBackup restore a backup to target path
	 * @param backup Which backup to restore
	 * @param targetpath Where to restore backup (Currently unused, implied)
	 * @return status of operation
	 */
	bool RestoreBackup(const string& backup, const string& targetpath = "");

	/**
	 * @brief StartBackup immediately try start a backup in the background
	 * @return true if command issued ok
	 */
	static bool StartBackup(void);

	/**
	 * @brief InProgress  check if backup currently running
	 * @return true if backup is in progress, false otherwise
	 */
	static bool InProgress(void);

	virtual ~BackupManager();

private:
	void CleanupRestoreEnv();

	bool SetupRestoreEnv();

	/**
	 * @brief SetConfig
	 * @param cfg configuration to use from now on
	 */
	void SetConfig(const json& cfg);

	/**
	 * @brief WriteConfig writes config to disk
	 */
	void WriteConfig();

	bool opprovider;
	bool hasunitid;
	string unitid;

	string backuppassword;
	OPI::CryptoHelper::AESWrapperPtr backupkey;
	OPI::BackupHelperPtr backuphelper;
};

} // Namespace KGP

#endif // BACKUPMANAGER_H
