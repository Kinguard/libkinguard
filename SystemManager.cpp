#include "SystemManager.h"

#include <json/json.h>

#include <libutils/FileUtils.h>
#include <libutils/Process.h>
#include <libutils/Logger.h>

#define SLOG	ScopedLog l(__func__)

using namespace Utils;

SystemManager::SystemManager()
{

}

SystemManager &SystemManager::Instance()
{
	static SystemManager mgr;

	return mgr;
}

tuple<bool, string> SystemManager::UpgradeAvailable()
{
	SLOG;

	if( ! File::FileExists("/usr/share/opi-updates/kgp-checkupgrade") )
	{
		logg << Logger::Debug << "No upgradescript available" << lend;
		return make_tuple(false,"");
	}

	bool retval;
	string retoutput;
	tie(retval, retoutput) = Process::Exec("/usr/share/opi-updates/kgp-checkupgrade");

	if( ! retval )
	{
		logg << Logger::Debug << "Failed to execute checker script" << lend;
		return make_tuple(false,"");
	}

	Json::Reader r;
	Json::Value val;
	if( !r.parse(retoutput, val) )
	{
		logg << Logger::Debug << "Unable to parse output of checker script" << lend;
		return make_tuple(false,"");
	}

	if( ! val.isMember("status") || !val["member"].isBool() )
	{
		logg << Logger::Debug << "Missing status from checker script" << lend;
		return make_tuple(false,"");
	}

	if( ! val.isMember("description") || !val["description"].isString() )
	{
		logg << Logger::Debug << "Missing description from checker script" << lend;
		return make_tuple(false,"");
	}

	return make_tuple(val["status"].asBool(),val["description"].asString());
}

void SystemManager::StartUpgrade()
{
	SLOG;
	try
	{
		if( !File::FileExists("/usr/share/opi-updates/kgp-distupgrade") )
		{
			logg << Logger::Debug  << "Upgrade script not available" << lend;
			return;
		}

		Process::Spawn("/usr/share/opi-updates/kgp-distupgrade");

	}
	catch (ErrnoException& err)
	{
		logg << Logger::Notice << "Launch of upgrade script failed: " << err.what() << lend;
	}
}

void SystemManager::StartUpdate()
{
	SLOG;
	try
	{
		if( File::FileExists("/usr/share/opi-updates/kgp-update") )
		{
			Process::Spawn("/usr/share/opi-updates/kgp-update");
		}
		else
		{
			Process::Spawn("/usr/share/opi-updates/opi-dist-upgrade.sh");
		}
	}
	catch (ErrnoException& err)
	{
		logg << Logger::Notice << "Failed to start update ("<< err.what()<<")" << lend;
	}
}
