#include "SystemManager.h"

#include <json/json.h>

#include <libutils/FileUtils.h>
#include <libutils/Process.h>
#include <libutils/Logger.h>

#include <libopi/SysConfig.h>

#define SLOG	ScopedLog l(__func__)

using namespace Utils;


SystemManager::SystemManager(): providers({"OpenProducts"})
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

	bool retval = false;
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

	if( ! val.isMember("status") || !val["status"].isBool() )
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

bool SystemManager::IsConfigured()
{
	return OPI::SysConfig().HasKey("hostinfo","hostname");
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
		this->global_error = "Launch of upgrade script failed: "s + err.what();
	}
}

void SystemManager::StartUpdate()
{
	SLOG;
	try
	{
		if( File::FileExists("/usr/share/opi-updates/kgp-update") )
		{
			logg << Logger::Debug << "Try executing /usr/share/opi-updates/kgp-update"<<lend;
			Process::Spawn("/usr/share/opi-updates/kgp-update -f");
		}
		else
		{
			logg << Logger::Debug << "Try executing /usr/share/opi-updates/opi-dist-upgrade.sh"<<lend;
			Process::Spawn("/usr/share/opi-updates/opi-dist-upgrade.sh");
		}
	}
	catch (ErrnoException& err)
	{
		logg << Logger::Notice << "Failed to start update ("<< err.what()<<")" << lend;
		this->global_error = "Failed to start update ("s + err.what();
	}
}

bool SystemManager::HasProviders()
{
	SLOG;

	return this->providers.size() > 0;
}

const list<string> &SystemManager::Providers()
{
	return this->providers;
}

SystemManager::~SystemManager() = default;
