#include "NetworkManager.h"

#include <libopi/SysInfo.h>
#include <libopi/NetworkConfig.h>

#include <libutils/Logger.h>

using namespace OPI;
using namespace Utils;

namespace KGP
{

NetworkManager::NetworkManager()
{
	logg << Logger::Debug << "NetworkManager starting up" << lend;

	this->net = make_shared<OPI::NetUtils::NullConfig>();

	// Check dependencies, a bit uggly concider refactor if more versions added
	SysInfo::OSType os = sysinfo.OS();

	if( os != SysInfo::OSType::OSDebian && os != SysInfo::OSType::OSRaspbian )
	{
		logg << Logger::Alert << "Network manager: unsupported platform: " << sysinfo.OSTypeText[os] << lend;
		return;
	}

	string version = sysinfo.OSVersion();

	if( version != "buster")
	{
		logg << Logger::Alert << "Unsupported raspbian/debian version: " << version << lend;
		return;
	}

	if( os == SysInfo::OSType::OSDebian )
	{
		logg << Logger::Debug << "Running on valid debian system" << lend;
		this->net = make_shared<OPI::NetUtils::DebianNetworkConfig>();
	}

	if( os == SysInfo::OSType::OSRaspbian )
	{
		logg << Logger::Debug << "Running on valid raspbian system" << lend;
		this->net = make_shared<OPI::NetUtils::RaspbianNetworkConfig>();
	}

}

NetworkManager &NetworkManager::Instance()
{
	static NetworkManager mgr;

	return mgr;
}

Json::Value NetworkManager::GetConfiguration(const string &interface)
{
	return this->net->GetInterface(interface);
}

bool NetworkManager::StaticConfiguration(const string &interface, const string &ip, const string &netmask, const string &gateway, const list<string> dns)
{
	this->net->SetStatic(interface,ip, netmask, gateway, dns);
	this->net->WriteConfig();

	return OPI::NetUtils::RestartInterface(interface);
}

bool NetworkManager::DynamicConfiguration(const string &interface)
{
	this->net->SetDHCP(interface);
	this->net->WriteConfig();

	return OPI::NetUtils::RestartInterface(interface);
}

} // KGP

