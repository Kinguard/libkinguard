#include "BackupManager.h"
#include "Config.h"

#include <libutils/Logger.h>
#include <libutils/FileUtils.h>
#include <libutils/Process.h>

#include <libopi/SysConfig.h>
#include <libopi/AuthServer.h>

#include <sys/file.h>

#include "StorageManager.h"

// Convenience defines
#define SCFG	(OPI::SysConfig())

using namespace Utils;
using namespace OPI;
using namespace OPI::CryptoHelper;

namespace KGP
{

BackupManager::BackupManager(): opprovider(false),hasunitid(false)
{
	this->opprovider = ( SCFG.HasKey("dns", "provider") && SCFG.GetKeyAsString("dns", "provider") == "OpenProducts" );

	if( SCFG.HasKey("hostinfo", "unitid") )
	{
		this->hasunitid = true;
		this->unitid = SCFG.GetKeyAsString("hostinfo", "unitid");
	}

	logg << Logger::Notice << "Backupmanager initialized" << lend;
}

BackupManager &BackupManager::Instance()
{
	static BackupManager mgr;

	return mgr;
}


static string GetBackupPassword(const string& password)
{
	SecString spass(password.c_str(), password.size() );
	SecVector<byte> key = PBKDF2( spass, 20);
	vector<byte> ukey(key.begin(), key.end());

	return Base64Encode( ukey );
}

static AESWrapperPtr GetBackupCrypto(const string& password)
{
	SecVector<byte> key = PBKDF2(SecString(password.c_str(), password.size() ), 32 );
	return AESWrapperPtr(new AESWrapper(key) );
}

void BackupManager::Configure(const Json::Value &cfg)
{
	ScopedLog l("Configure");

	if( !cfg.isMember("password") || !cfg["password"].isString() )
	{
		throw std::runtime_error("Missing password in backup config");
	}
	BackupManager::Instance().SetConfig(cfg);
}

Json::Value BackupManager::GetBackups()
{
	ScopedLog l("Get backups");
	if( ! this->backuphelper )
	{
		// Make sure we have no leftovers from earlier attempts
		this->CleanupRestoreEnv();

		if( ! this->SetupRestoreEnv() )
		{
			logg << Logger::Error << "Failed to set up restore environment"<<lend;
			return Json::nullValue;
		}
		this->backuphelper = BackupHelperPtr( new BackupHelper( this->backuppassword ) );
	}
	else
	{
		// Entered password might have been changed
		this->backuphelper->SetPassword( this->backuppassword );
	}

	Json::Value retval;
	bool hasdata = false;
	// Check local
	if( this->backuphelper->MountLocal() )
	{
		list<string> local = this->backuphelper->GetLocalBackups();
		for( const auto& val: local)
		{
			hasdata = true;
			retval["local"].append(val);
		}

		this->backuphelper->UmountLocal();
	}
	else
	{
		logg << Logger::Debug << "Mount local failed" << lend;
	}

	// Check remote if OP unit
	if( this->opprovider )
	{
		if( this->backuphelper->MountRemote() )
		{
			list<string> remote = this->backuphelper->GetRemoteBackups();
			for( const auto& val: remote)
			{
				hasdata = true;
				retval["remote"].append(val);
			}
			this->backuphelper->UmountRemote();
		}
		else
		{
			logg << Logger::Debug << "Mount remote failed" << lend;
		}
	}
	else
	{
		logg << Logger::Debug << "Not checking remote backup since none OP unit" << lend;
	}

	if( ! hasdata )
	{
		logg << Logger::Debug << "Clean up restore env since no data available"<<lend;
		this->CleanupRestoreEnv();
	}

	return hasdata ? retval : Json::nullValue ;
}

bool BackupManager::RestoreBackup(const string &backup, const string &targetpath)
{
	// Currently unused
	(void) targetpath;
	StorageManager& mgr=StorageManager::Instance();
	if( ! mgr.mountDevice( TMP_MOUNT ) )
	{
		logg << Logger::Error << "Failed to mount SD for backup: "<< mgr.Error()<<lend;
		this->global_error = "Restore backup - Failed to access SD card";
		return false;
	}

// Temp workaround to figure out if this is a local or remote backup
// Todo: Refactor in libopi
#define LOCALBACKUP	"/tmp/localbackup"
#define REMOTEBACKUP "/tmp/remotebackup"

	if( backup.substr(0,strlen(LOCALBACKUP) ) == LOCALBACKUP )
	{
		logg << Logger::Debug << "Do restore from local backup "<< backup << lend;
		if( ! this->backuphelper->MountLocal() )
		{
			logg << Logger::Error << "Failed to (re)mount local backup" << lend;
			this->global_error = "Restore backup - failed to retrieve local backup";
			return false;
		}
	}
	else if( backup.substr(0, strlen(REMOTEBACKUP)) == REMOTEBACKUP )
	{
		logg << Logger::Debug << "Do restore from remote backup "<< backup << lend;
		if( ! this->backuphelper->MountRemote() )
		{
			logg << Logger::Error << "Failed to (re)mount remote backup" << lend;
			this->global_error = "Restore backup - failed to retrieve remote backup";
			return false;
		}
	}
	else
	{
		logg << Logger::Error << "Malformed restore path: " << backup << lend;
		this->global_error = "Restore backup - Malformed source path" ;
		return false;
	}

	if( !this->backuphelper->RestoreBackup( backup ) )
	{
		StorageManager::umountDevice();
		this->global_error = "Restore Backup - restore failed";
		return false;
	}

	try
	{
		StorageManager::umountDevice();
	}
	catch( ErrnoException& err)
	{
		logg << Logger::Error << "Failed to umount SD after backup: "<< err.what()<<lend;
		this->global_error = "Restore backup - Failed to remove SD card";
		return false;
	}

	logg << Logger::Debug << "Restore completed sucessfully"<<lend;

	return true;
}

bool BackupManager::StartBackup()
{
	ScopedLog l("StartBackup");

	try
	{

		if( BackupManager::InProgress() )
		{
			logg << Logger::Notice << "Backup already in progress, don't start" << lend;
			return false;
		}

		Process::Spawn("/usr/share/opi-backup/opi_backup.sh");
	}
	catch (ErrnoException& err)
	{
		logg << Logger::Notice << "Failed to start backup: " << err.what() << lend;
		return false;
	}
	return true;
}

bool BackupManager::InProgress()
{
	ScopedLog l("BackupInProgress");
	bool ret = false;
#define LOCKFILE  "/var/run/lock/kgp-backup.lock"

	if( ! File::FileExists(LOCKFILE) )
	{
		return ret;
	}

	int fd = open(LOCKFILE, O_RDONLY);
	if (fd < 0 )
	{
		throw ErrnoException("Failed to open lockfile");
	}

	int lockstatus = flock(fd, LOCK_EX | LOCK_NB);

	if( lockstatus < 0 && errno == EWOULDBLOCK )
	{
		ret = true;
	}

	close(fd);

	return ret;
}

BackupManager::~BackupManager()
{
	this->CleanupRestoreEnv();
}

void BackupManager::CleanupRestoreEnv()
{
	ScopedLog l("CleanupRestoreEnv");

	if( ! this->opprovider )
	{
		logg << Logger::Notice << "None OP system, nothing to do" << lend;
		return;
	}

	if( this->backuphelper )
	{
		this->backuphelper->UmountLocal();
		this->backuphelper->UmountRemote();
	}

	// Remove any temporarily added keys
	if( File::FileExists(SCFG.GetKeyAsString("hostinfo","syspubkey")))
	{
		File::Delete(SCFG.GetKeyAsString("hostinfo","syspubkey"));
	}

	if( File::FileExists(SCFG.GetKeyAsString("hostinfo","sysauthkey")))
	{
		File::Delete(SCFG.GetKeyAsString("hostinfo","sysauthkey"));
	}
}

bool BackupManager::SetupRestoreEnv()
{
	ScopedLog l("SetupRestoreEnv");

	logg << Logger::Debug << "Setting up environment for restore"<<lend;

	if( ! this->opprovider )
	{
		logg << Logger::Notice << "None OP system, nothing to do" << lend;
		return true;
	}

	// Make sure we have environment to work from.
	// TODO: Lot of duplicated code here :(
	// Generate temporary keys to use
	RSAWrapper ob;
	ob.GenerateKeys();

	File::Write(SCFG.GetKeyAsString("hostinfo","syspubkey"), ob.PubKeyAsPEM(), 0644);
	File::Write(SCFG.GetKeyAsString("hostinfo","sysauthkey"), ob.PrivKeyAsPEM(), 0600);

	AuthServer s(this->unitid);

	string challenge;
	int resultcode;
	tie(resultcode,challenge) = s.GetChallenge();

	if( resultcode != 200 )
	{
		logg << Logger::Notice << "Failed to get challenge " << resultcode <<lend;
		return false;
	}

	string signedchal = CryptoHelper::Base64Encode( ob.SignMessage( challenge ) );
	Json::Value ret;
	tie(resultcode, ret) = s.SendSignedChallenge( signedchal );

	if( resultcode != 403 )
	{
		logg << Logger::Notice << "Failed to send challenge " << resultcode <<lend;
		return false;
	}

	challenge = ret["challange"].asString();
	string cryptchal = Base64Encode( this->backupkey->Encrypt( challenge ) );

	tie(resultcode, ret) = s.SendSecret(cryptchal, Base64Encode( ob.PubKeyAsPEM() ) );

	if( resultcode != 200 )
	{
		logg << Logger::Notice << "Failed to send secret ("
			 << resultcode
			 << ") '" << ret["Message"].asString()<<"'"
			 <<lend;
		logg << "Response : "<< ret.toStyledString()<<lend;
		return false;
	}

	return true;

}

void BackupManager::SetConfig(const Json::Value &cfg)
{
	this->backuppassword = GetBackupPassword( cfg["password"].asString() );
	this->backupkey = GetBackupCrypto( cfg["password"].asString() );

	this->WriteConfig();
}

void BackupManager::WriteConfig()
{
	string authfile = SCFG.GetKeyAsString("backup","authfile");
	string path = File::GetPath( authfile );

	if( ! File::DirExists( path ) )
	{
		File::MkPath( path ,0755);
	}

	stringstream ss;
	ss << "[s3op]\n"
		<< "storage-url: s3op://\n"
		<< "backend-login: NotUsed\n"
		<< "backend-password: NotUsed\n"
		<< "fs-passphrase: " << this->backuppassword <<"\n\n"

		<< "[local]\n"
		<< "storage-url: local://\n"
		<< "fs-passphrase: " << this->backuppassword <<endl

		<< "[s3]\n"
		<< "storage-url: s3://\n"
		<< "fs-passphrase: " << this->backuppassword <<endl;


	File::Write(authfile, ss.str(), 0600 );
}
} // Namespace KGP
