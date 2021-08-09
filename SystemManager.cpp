#include "SystemManager.h"

#include <json/json.h>

#include <libutils/FileUtils.h>
#include <libutils/Process.h>
#include <libutils/Logger.h>

#include <libopi/ServiceHelper.h>
#include <libopi/SysConfig.h>

#define SLOG	ScopedLog l(__func__)

using namespace Utils;


SystemManager::SystemManager(): providers({"OpenProducts"})
{
	logg << Logger::Debug << "SystemManager created" << lend;
	this->shellaccess = make_unique<ShellAccess>();
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
			Process::Spawn("/usr/share/opi-updates/kgp-update", {"-f"});
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

/*
 *
 * Shell access implementation
 *
 */

class ShellAccess
{
public:
	ShellAccess()
	{
		this->accessavailable = ! OPI::ServiceHelper::IsAvailable("ssh");

		if( this->accessavailable )
		{
			this->dropbearinstalled = OPI::ServiceHelper::IsAvailable("dropbear");
		}

		if( this->dropbearinstalled )
		{
			this->shellenabled = OPI::ServiceHelper::IsRunning("dropbear");
		}

		logg << Logger::Debug
			 << "ShellAccess:"
			 << " Available: " << this->accessavailable
			 << " Installed " << this->dropbearinstalled
			 << " Enabled " << this->shellenabled << lend;

	}

	ShellAccess(const ShellAccess& shell) = default;
	ShellAccess(ShellAccess&& shell ) = default;

	ShellAccess& operator=(const ShellAccess& shell) = default;
	ShellAccess& operator=(ShellAccess&& shell) = default;

	bool Available()
	{
		return this->accessavailable;
	}

	bool Enabled()
	{
		return this->shellenabled;
	}

	bool Enable()
	{
		logg << Logger::Debug << "ShellAccess: Enable" << lend;

		bool ret = false;
		if( this->accessavailable && ! this->shellenabled )
		{
			tie(ret, std::ignore) = Process::Exec("/usr/share/kgp-assets/kgp-shell/enable_shell.sh");

			if( ! ret )
			{
				logg << Logger::Error << "Failed to install dropbear ssh server" << lend;
			}

			this->shellenabled = ret;
			this->dropbearinstalled = ret;
		}
		return ret;
	}

	bool Disable()
	{
		logg << Logger::Debug << "ShellAccess: Disable" << lend;

		bool ret = false;

		if( ! this->dropbearinstalled )
		{
			logg << Logger::Warning << "Trying to disable ssh but dropbear not installed!" << lend;
			return false;
		}

		tie(ret, std::ignore) = Process::Exec( "/usr/share/kgp-assets/kgp-shell/disable_shell.sh" );

		if( ! ret )
		{
			logg << Logger::Error << "Failed to remove dropbear ssh server" << lend;
		}
		else
		{
			// Succesfully removed
			this->shellenabled = false;
			this->dropbearinstalled = false;
		}

		return ret;
	}

	~ShellAccess() = default;
private:

	bool accessavailable{false};	/// Is shell access supported (I.e. no openssh installed)
	// TODO: Below most likely redundant. Should be enough with one of them,
	// There currently are no scenarion with dropbear installed but not running(?)
	bool shellenabled{false};		/// Is ssh server running?
	bool dropbearinstalled{false};	/// Drobbear currently installed?
};

// We currently support shell access on all arch as long as openssh is not installed
// We should revise this and give finer control over this
bool SystemManager::ShellAccessAvailable()
{
	return this->shellaccess->Available();
}

bool SystemManager::ShellAccessEnabled()
{
	return this->shellaccess->Enabled();
}

bool SystemManager::ShellAccessEnable()
{
	if( ! this->shellaccess->Enable() )
	{
		this->global_error = "Failed to enable shell access";
		return false;
	}
	return true;
}

bool SystemManager::ShellAccessDisable()
{
	if( !this->shellaccess->Disable())
	{
		this->global_error = "Failed to disable shell access";
		return false;
	}
	return true;
}

SystemManager::~SystemManager() = default;
