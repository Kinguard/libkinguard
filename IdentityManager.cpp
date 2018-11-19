#include "IdentityManager.h"

#include <libutils/Logger.h>
#include <libutils/String.h>
#include <libutils/Process.h>
#include <libutils/FileUtils.h>

#include <libopi/AuthServer.h>
#include <libopi/SysConfig.h>
#include <libopi/DnsServer.h>

using namespace Utils;
using namespace OPI;

#define SCFG	(OPI::SysConfig())

namespace KGP
{

IdentityManager::IdentityManager()
{

}

IdentityManager &IdentityManager::Instance()
{
	static IdentityManager mgr;

	return mgr;
}

bool IdentityManager::SetHostname(const string &name)
{
	try {
		OPI::SysConfig cfg(true);
		cfg.PutKey("hostinfo","hostname", name);
	}
	catch (std::runtime_error& err)
	{
		logg << Logger::Error << "Failed to set hostname :" << err.what() << lend;
		this->global_error = string("Failed to set hostname :") + err.what();
		return false;
	}

	return true;
}

string IdentityManager::GetHostname()
{
	string ret;
	try
	{
		ret = SCFG.GetKeyAsString("hostinfo", "hostname");
	}
	catch (std::runtime_error& err)
	{
		this->global_error = string("Failed to retrieve hostname") + err.what();
		logg << Logger::Error << this->global_error << lend;
	}
	return ret;
}

bool IdentityManager::SetDomain(const string &domain)
{
	try {
		OPI::SysConfig cfg(true);
		cfg.PutKey("hostinfo","domain", domain);
	}
	catch (std::runtime_error& err)
	{
		logg << Logger::Error << "Failed to set domain:" << err.what() << lend;
		this->global_error = string("Failed to set domain:") + err.what();
		return false;
	}

	return true;

}

string IdentityManager::GetDomain()
{
	string ret;
	try
	{
		ret = SCFG.GetKeyAsString("hostinfo", "domain");
	}
	catch (std::runtime_error& err)
	{
		this->global_error = string("Failed to retrieve domain") + err.what();
		logg << Logger::Error << this->global_error << lend;
	}
	return ret;
}

tuple<string, string> IdentityManager::GetFqdn()
{
	OPI::SysConfig cfg;
	string host, domain;

	if( cfg.HasKey("hostinfo", "hostname") && cfg.HasKey("hostinfo", "domain") )
	{
		host = cfg.GetKeyAsString("hostinfo", "hostname");
		domain = cfg.GetKeyAsString("hostinfo", "domain");
	}

	return make_tuple(host, domain);

}

bool IdentityManager::CreateCertificate()
{
	try
	{
		OPI::SysConfig cfg;

		if( ! OPI::CryptoHelper::MakeSelfSignedCert(
					cfg.GetKeyAsString("dns","dnsauthkey"),
					cfg.GetKeyAsString("hostinfo","syscert"),
					cfg.GetKeyAsString("hostinfo","hostname") + "." + cfg.GetKeyAsString("hostinfo","domain"),
					cfg.GetKeyAsString("dns","provider")
					) )
		{
			return false;
		}

	}
	catch (std::runtime_error& err)
	{
		logg << Logger::Error << "Failed to generate certificate:" << err.what() << lend;
		this->global_error = string("Failed to generate certificate:") + err.what();
		return false;
	}
	return true;
}

bool IdentityManager::DnsNameAvailable(const string &hostname, const string &domain)
{
	OPI::DnsServer dns;
	int result_code;
	Json::Value ret;
	tie(result_code, ret) = dns.CheckOPIName( hostname +"."+ domain );

	if( result_code != 200 && result_code != 403 )
	{
		logg << Logger::Notice << "Request for DNS check name failed" << lend;
		return false;
	}

	return result_code == 200;
}

bool IdentityManager::AddDnsName(const string &hostname, const string &domain)
{
	logg << Logger::Debug << "Add DNS name" << lend;

	if( ! this->CheckUnitID() )
	{
		logg << Logger::Error << "No unitid provided" << lend;
		this->global_error = "No unitid provided to IdentityManager";
		return false;
	}

	string fqdn = hostname +"."+domain;
	DnsServer dns;
	if( ! dns.UpdateDynDNS(this->unitid, fqdn) )
	{
		logg << Logger::Error << "Failed to update Dyndns ("<< this->unitid << ") ("<< fqdn <<")"<<lend;
		this->global_error = "Failed to update DynDNS";
		return false;
	}

	logg << Logger::Debug << "Request OP certificate: "<<this->global_error<<lend;
	if( !this->GetCertificate(fqdn, "OPI") )
	{
		logg << Logger::Error << "Failed to get certificate for device name: "<<this->global_error<<lend;
		return false;
	}

	logg << Logger::Debug << "Get signed Certificate for '"<< fqdn <<"'"<<lend;
	if( ! this->GetSignedCert(fqdn) )
	{
		// This can fail if portforwards does not work, then the above cert will be used.
		logg << Logger::Notice << "Failed to get signed Certificate for device name: "<< fqdn <<lend;
	}

	if( ! this->SetHostname( hostname ) || ! this->SetDomain( domain ) )
	{
		logg << Logger::Error << "Failed to update hostname" << lend;
		return false;
	}

	try
	{
		OPI::SysConfig sysconfig(true);
		sysconfig.PutKey("dns", "enabled", true);
	}
	catch (std::runtime_error& err)
	{
		logg << Logger::Error << "Failed set dns flag: " << err.what() << lend;
		return false;
	}

	return true;
}

tuple<string, string> IdentityManager::GetCurrentDnsName()
{
	string hostname,domain;

	return make_tuple(hostname,domain);
}

bool IdentityManager::HasDnsProvider(void)
{
	return ( SCFG.HasKey("dns", "provider") && SCFG.GetKeyAsString("dns", "provider") == "OpenProducts" );
}

list<string> IdentityManager::DnsAvailableDomains()
{
	list<string> domains;

	if( SCFG.HasKey("dns","availabledomains") )
	{
		domains = SCFG.GetKeyAsStringList("dns","availabledomains");
	}

	return domains;
}

void IdentityManager::CleanUp()
{
	if( this->signerthread )
	{
		this->signerthread->Join();
	}
}

IdentityManager::~IdentityManager()
{

}

bool IdentityManager::GetCertificate(const string &fqdn, const string &provider)
{

	/*
	 *
	 * This is a workaround for a bug in the authserver that loses our
	 * credentials when we login with dns-key
	 *
	 */
	if( ! this->OPLogin() )
	{
		return false;
	}

	string syscert = SCFG.GetKeyAsString("hostinfo","syscert");
	string dnsauthkey = SCFG.GetKeyAsString("dns","dnsauthkey");

	string csrfile = File::GetPath( syscert )+"/"+String::Split(fqdn,".",2).front()+".csr";

	if( ! CryptoHelper::MakeCSR(dnsauthkey, csrfile, fqdn, provider) )
	{
		this->global_error = "Failed to make certificate signing request";
		return false;
	}

	string csr = File::GetContentAsString(csrfile, true);

	AuthServer s(this->unitid);

	int resultcode;
	Json::Value ret;
	tie(resultcode, ret) = s.GetCertificate(csr,this->token );

	if( resultcode != 200 )
	{
		logg << Logger::Error << "Failed to get csr "<<resultcode <<lend;
		this->global_error = "Failed to get certificate from OP servers";
		return false;
	}

	if( ! ret.isMember("cert") || ! ret["cert"].isString() )
	{
		logg << Logger::Error << "Malformed reply from server " <<lend;
		this->global_error = "Unexpected reply from OP server when retrieving certificate";
		return false;
	}

	// Make sure we have no symlinked tempcert in place
	unlink( syscert.c_str() );

	File::Write( syscert, ret["cert"].asString(), 0644);

	return true;
}


class SignerThread: public Utils::Thread
{
public:
	SignerThread(const string& name): Thread(false), opiname(name) {}

	virtual void Run();
	bool Result();
	virtual ~SignerThread();
private:
	string opiname;
	bool result;
};

void SignerThread::Run()
	{
		tie(this->result, ignore) = Process::Exec("/usr/share/kinguard-certhandler/letsencrypt.sh -ac");
	}

bool SignerThread::Result()
	{
		// Only valid upon completed run
		return this->result;
	}

SignerThread::~SignerThread()
{
}

bool IdentityManager::GetSignedCert(const string &fqdn)
{
	try
	{
		logg << Logger::Debug << "Launching detached signer thread" << lend;
		this->signerthread = ThreadPtr( new SignerThread(fqdn) );
		this->signerthread->Start();
	}
	catch( std::runtime_error& err)
	{
		logg << Logger::Error << "Failed to launch signer thread: " << err.what() << lend;
		return false;
	}
	return true;
}

bool IdentityManager::CheckUnitID()
{
	if( this->unitid != "" )
	{
		// We have a unitid retrieved earlier
		return true;
	}

	if( SCFG.HasKey("hostinfo", "unitid") )
	{
		this->unitid = SCFG.GetKeyAsString("hostinfo", "unitid");
	}

	return this->unitid != "";
}

bool IdentityManager::OPLogin()
{
	logg << Logger::Debug << "Do OP login" << lend;

	AuthServer s( this->unitid);
	int resultcode;
	Json::Value ret;

	tie(resultcode, ret) = s.Login();

	if( resultcode != 200 && resultcode != 403 )
	{
		logg << Logger::Error << "Unexpected reply from server "<< resultcode <<lend;
		this->global_error ="Unexpected reply from OP server ("+ ret["desc"].asString()+")";
		return false;
	}

	if( resultcode == 403)
	{
		this->global_error ="Failed to authenticate with OP server. Wrong activation code or password.";
		return false;
	}

	if( ret.isMember("token") && ret["token"].isString() )
	{
		this->token = ret["token"].asString();
	}
	else
	{
		logg << Logger::Error << "Missing argument in reply"<<lend;
		this->global_error ="Failed to communicate with OP server (Missing argument)";
		return false;
	}

	return true;
}

} // Namespace OPI

