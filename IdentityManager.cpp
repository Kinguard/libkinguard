#include "IdentityManager.h"
#include "MailManager.h"

#include <ext/stdio_filebuf.h>

#include <libutils/FileUtils.h>
#include <libutils/Logger.h>
#include <libutils/String.h>
#include <libutils/Process.h>
#include <libutils/HttpStatusCodes.h>

#include <libopi/ServiceHelper.h>
#include <libopi/HostsConfig.h>
#include <libopi/AuthServer.h>
#include <libopi/DnsServer.h>
#include <libopi/SysConfig.h>
#include <libopi/Secop.h>
#include <unistd.h>

#include <utility>

using namespace Utils;
using namespace OPI;
using namespace Utils::HTTP;

#define SCFG	(OPI::SysConfig())

namespace KGP
{

IdentityManager::IdentityManager(): tmpfilename("")
{

}

IdentityManager &IdentityManager::Instance()
{
	static IdentityManager mgr;

	return mgr;
}


bool IdentityManager::SetFqdn(const string &name, const string &domain)
{
	string oldFqdn = this->GetFqdnAsString();

	MailManager& mailmgr = MailManager::Instance();

	try
	{
		if( sethostname(name.c_str(), name.length()) == -1)
		{
			this->global_error = string("Failed to set hostname:") + strerror(errno);
			return false;
		}

		if( setdomainname(domain.c_str(), domain.length()) == -1)
		{
			this->global_error = string("Failed to set domainname:") + strerror(errno);
			return false;
		}

		OPI::SysConfig cfg(true);
		this->hostname = name;
		this->domain = domain;
		cfg.PutKey("hostinfo","hostname", name);
		cfg.PutKey("hostinfo","domain", domain);

		// Update mail hostname
		if( ! mailmgr.SetHostname(name, domain) )
		{
			this->global_error = mailmgr.StrError();
			return false;
		}

		// Update /etc/hosts
		OPI::HostsConfig hosts;

		// Check if we have old host
		HostEntryPtr host = hosts.GetEntryByAddress("127.0.1.1");
		if( host )
		{
			hosts.DeleteEntry(host);
		}
		hosts.AddEntry("127.0.1.1", name+"."+domain, {name});

		hosts.WriteBack();

		// Update system hostname
		File::Write("/etc/hostname", this->hostname, File::UserRW | File::GroupRead | File::OtherRead);

	}
	catch (std::runtime_error& err)
	{
		logg << Logger::Error << "Failed to set hostname :" << err.what() << lend;
		this->global_error = string("Failed to set hostname :") + err.what();
		return false;
	}

	try
	{
		string newfqdn = this->GetFqdnAsString();

		if( oldFqdn == newfqdn )
		{
			logg << Logger::Debug << "New fqdn same as old one, not changing (" << newfqdn << ")" << lend;
		}
		else if( oldFqdn != "" )
		{
			logg << Logger::Debug << "Update MailConfig" << lend;

			mailmgr.ChangeDomain(oldFqdn, newfqdn);
		}
	}
	catch (std::runtime_error& err)
	{
		this->global_error = string("Failed change mailhostname: ") + err.what();
		logg << Logger::Error << this->global_error << lend;
		return false;
	}
	return true;
}

string IdentityManager::GetHostname()
{
	if( this->hostname == "" )
	{
		// Get hostname from sysconfig
		if( SCFG.HasKey("hostinfo", "hostname") )
		{
			this->hostname = SCFG.GetKeyAsString("hostinfo", "hostname");
		}
	}
	return this->hostname;
}

string IdentityManager::GetDomain()
{
	if( this->domain == "" )
	{
		// Get domain from sysconfig
		if( SCFG.HasKey("hostinfo", "domain") )
		{
			this->domain = SCFG.GetKeyAsString("hostinfo", "domain");
		}
	}
	return this->domain;
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

string IdentityManager::GetFqdnAsString()
{
	// GetHostname reads and sets this->hostname if necessary
	if (this->GetHostname() != "" && this->GetDomain() != "")
	{
		return this->hostname + "." + domain;
	}

	return "";

}

tuple<bool,string> IdentityManager::WriteCustomCertificate(const string &key, const string &cert)
{
	SysConfig sysconfig(true);


	string CustomCertFile,CustomKeyFile,CustomCertPath;
	string webcert = sysconfig.GetKeyAsString("webcertificate","activecert");
	string webkey = sysconfig.GetKeyAsString("webcertificate","activekey");


	// Generate new filenames
	logg << Logger::Debug << "Generating new custom cert paths" << lend;


	CustomCertPath = "/etc/opi/usercert/";
	CustomCertFile = CustomCertPath + "cert.pem";
	CustomKeyFile = CustomCertPath + "privkey.pem";
	// check that the path exists.
	if ( ! File::DirExists(CustomCertPath) )
	{
		// create paths.
		try
		{
			File::MkPath(CustomCertPath, File::UserRWX);
		}
		catch (Utils::ErrnoException& err)
		{
			logg << Logger::Error << "Failed to create path for usercerts:" << err.what() << lend;
			this->global_error = string("Failed to create path for usercerts:") + err.what();
			return make_tuple(false,"Failed to create path for usercerts:");
		}
	}

	try {
		// Write content to userCert files
		File::Write(CustomCertFile,cert, File::UserRW | File::GroupRead | File::OtherRead);

		if ( key != "" ) {
			File::Write(CustomKeyFile,key,File::UserRW | File::GroupRead | File::OtherRead);
		}
		else
		{
			// if no key provided, use the existing key
			// if just the device name is updated, this function is triggered anyway
			// but the "key" will be empty
			CustomKeyFile = File::RealPath(webkey);
		}
	}
	catch (std::runtime_error& err)
	{
		this->global_error = string("Filed to write custom certificate files: ") +err.what();
		return make_tuple(false, this->global_error);
	}

	// create a backup copy of the cert symlinks nginx uses
	string curr_key,curr_cert;
	curr_key = File::RealPath(webkey);
	curr_cert = File::RealPath(webcert);


	File::Delete(webcert);
	File::Delete(webkey);

	ignore = symlink(CustomCertFile.c_str(),webcert.c_str());
	ignore = symlink(CustomKeyFile.c_str(),webkey.c_str());

	// new links should now be in place, let nginx test the config
	int retval =0;
	string Message;

	tie(retval,Message)=Process::Exec( "nginx -t" );
	if ( ! retval )
	{
		// nginx config test failed, restore old links
		logg << Logger::Debug << "Nginx config test failed" << lend;
		File::Delete(webcert);
		File::Delete(webkey);

		ignore = symlink(curr_cert.c_str(),webcert.c_str());
		ignore = symlink(curr_key.c_str(),webkey.c_str());

		logg << Logger::Error << "Webserver config test failed with new certificates" << lend;
		return make_tuple(false,"Webserver config test failed with new certificates");

	}

	return make_tuple(true,"");
}

bool IdentityManager::CreateCertificate()
{
	if ( SCFG.HasKey("webcertificate","backend") )
	{
		return this->CreateCertificate(true,SCFG.GetKeyAsString("webcertificate","backend"));
	}
	return false;
}

bool IdentityManager::CreateCertificate(bool forceProvider, const string &certtype)
{

	OPI::SysConfig cfg(true);
	string fqdn = this->GetHostname() + "." + this->GetDomain();
	string provider;

	cfg.PutKey("webcertificate","backend",certtype);
	if ( forceProvider )
	{
		try
		{
			// Always genereate self signed web server certificate, used as long as no LE-cert or similar present
			if( ! OPI::CryptoHelper::MakeSelfSignedCert(
						cfg.GetKeyAsString("dns","dnsauthkey"),
						cfg.GetKeyAsString("webcertificate","defaultcert"),
						fqdn,
						cfg.GetKeyAsString("hostinfo","hostname")
						) )
			{
				logg << Logger::Error << "Failed to create self signed server certificate" << lend;
				return false;
			}

			// Also use this cert as the default OPI-cert. This will be replaced below if we have
			// a valid provider.
			string syscert = SCFG.GetKeyAsString("hostinfo","syscert");

			unlink( syscert.c_str() );

			if( symlink( cfg.GetKeyAsString("webcertificate","defaultcert").c_str(), syscert.c_str() ) < 0)
			{
				logg << Logger::Error << "Failed to symlink selfsigned cert" << lend;
			}

			// generate certificate for provider, or if no provider generate self signed certificate
			if ( this->HasDnsProvider() )
			{
				provider = cfg.GetKeyAsString("dns","provider");
				if ( provider == "OpenProducts" )
				{
					provider = "OPI";  // legacy, provider shall be OPI for OpenProducts certificates.
				}

				if( this->IsEnabledDNS() )
				{
					// Get a certificate for the supported DNS-provider
					list<string> domains = this->DnsAvailableDomains();

					if( std::find(domains.begin(), domains.end(), this->domain) != domains.end() )
					{
						if( !this->GetCertificate(fqdn, provider) )
						{
							logg << Logger::Error << "Failed to get certificate for device name: "<<fqdn<<lend;
							return false;
						}
					}
					else
					{
						logg << Logger::Debug << "Domain " << this->domain << " not supported in backend " <<lend;
					}
				}
				else
				{
					// We seem to have a custom domain but still use a mail service
					// TODO: This is a workaround, maybe should be implemented in mailmanager?
					logg << Logger::Debug << "Try get a mail-relay cert only" << lend;

					if( !this->GetCertificate(this->GetHostname()+".mailrelay", provider) )
					{
						logg << Logger::Error << "Failed to get certificate for device name: "<<fqdn<<lend;
						return false;
					}
				}

			}
		}
		catch (std::runtime_error& err)
		{
			logg << Logger::Error << "Failed to generate certificate:" << err.what() << lend;
			this->global_error = string("Failed to generate certificate:") + err.what();
			return false;
		}
	}

	if( certtype != "CUSTOMCERT" )
	{
		logg << Logger::Debug << "Get signed Certificate for '"<< fqdn <<"'"<<lend;
		if( ! this->GetSignedCert(fqdn) )
		{
			// The call forks a new process and can not really fail, but the generation of the certificate
			// can fail. But that can not be indicted here...
			logg << Logger::Notice << "Failed to get launch thread to get signed cert."<<lend;
		}
	}
	else
	{
		logg << Logger::Debug << "Custom certficate, not trying to sign" << lend;
	}

	return true;
}

bool IdentityManager::DnsNameAvailable(const string &hostname, const string &domain)
{
	OPI::DnsServer dns;
	int result_code = 0;
	json ret;
	tie(result_code, ret) = dns.CheckOPIName( hostname +"."+ domain );

	if( result_code != Status::Ok && result_code != Status::Forbidden )
	{
		logg << Logger::Notice << "Request for DNS check name failed" << lend;
		return false;
	}

	return result_code == Status::Ok;
}

bool IdentityManager::DnsDomainAvailable(const string &domain)
{
	if ( ! SCFG.HasKey("dns","availabledomains") )
	{
		return false;
	}
	list<string> domains = SCFG.GetKeyAsStringList("dns","availabledomains");

	for(const auto& d: domains)
	{
		if ( domain == d )
		{
			return true;
		}
	}
	return false;
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

	return true;
}

tuple<string, string> IdentityManager::GetCurrentDnsName()
{
	// Currently if we have a dns-name it is the same as the hostname.
	if( this->HasDnsProvider() )
	{
		return this->GetFqdn();
	}
	return make_tuple("","");
}

bool IdentityManager::HasDnsProvider()
{
	return ( SCFG.HasKey("dns", "provider") && SCFG.GetKeyAsString("dns", "provider") == "OpenProducts" );
}

bool IdentityManager::EnableDnsProvider(const string &provider)
{
	// Currently only OP supported
	if( provider != "OpenProducts" )
	{
		logg << Logger::Notice << "DNS provider " << provider << " not supported"<<lend;
		return false;
	}
	// We need a writable config
	OPI::SysConfig cfg(true);
	cfg.PutKey("dns", "provider", "OpenProducts");
	list<string> domains({"mykeep.net", "op-i.me"});
	cfg.PutKey("dns", "availabledomains", domains);

	return true;
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

bool IdentityManager::DisableDNS()
{
	try {
		OPI::SysConfig cfg(true);
		cfg.PutKey("dns","enabled", false);
	}
	catch (std::runtime_error& err)
	{
		logg << Logger::Error << "Failed to disable dns provider:" << err.what() << lend;
		this->global_error = string("Failed to disable dns provider:") + err.what();
		return false;
	}

	return true;
}

bool IdentityManager::IsEnabledDNS()
{
	OPI::SysConfig cfg;

	if( cfg.HasScope("dns") && cfg.HasKey("dns","enabled") )
	{
		return cfg.GetKeyAsBool("dns", "enabled");
	}
	return false;
}

bool IdentityManager::EnableDNS()
{
	if ( SCFG.HasKey("dns", "provider") )
	{
		return this->SetDNSProvider(SCFG.GetKeyAsString("dns", "provider"));
	}
	else
	{
		logg << Logger::Error << "Failed to enable dns provider (missing in config)" << lend;
		this->global_error = string("Failed to enable dns provider (missing in config)");
		return false;
	}
}

bool IdentityManager::SetDNSProvider(const string &provider)
{
	// set and enable dns provider
	try {
		OPI::SysConfig cfg(true);
		cfg.PutKey("dns","provider", provider);
		cfg.PutKey("dns","enabled", true);
	}
	catch (std::runtime_error& err)
	{
		logg << Logger::Error << "Failed to set dns provider:" << err.what() << lend;
		this->global_error = string("Failed to set dns provider:") + err.what();
		return false;
	}

	return true;
}

bool IdentityManager::RegisterKeys() {
	logg << Logger::Debug << "Register keys"<<lend;

	try{

		// If OP device
		if( this->HasDnsProvider() )
		{
			// Generate sysauthkeys and register with secop if not present
			AuthServer::Setup();

			string sysauthkey = SCFG.GetKeyAsString("hostinfo","sysauthkey");
			string syspubkey = SCFG.GetKeyAsString("hostinfo","syspubkey");

			logg << Logger::Debug << "Delete (if existing) old private key"<<lend;
			// sysauth keys shall no longer exist on file.
			if ( File::FileExists(sysauthkey)) {
				File::Delete(sysauthkey);
			}
			if ( File::FileExists(syspubkey)) {
				File::Delete(syspubkey);
			}
		}

		// Create "dns"-key
		logg << Logger::Debug << "Checking dns-key, possibly recreating" << lend;

		string dnsauthkey = SCFG.GetKeyAsString("dns","dnsauthkey");
		string dnspubkey = SCFG.GetKeyAsString("dns","dnspubkey");

		string priv_path = File::GetPath( dnsauthkey );
		if( ! File::DirExists( priv_path ) )
		{
			File::MkPath( priv_path, File::UserRWX | File::GroupRX | File::OtherRX);
		}

		string pub_path = File::GetPath( dnspubkey );
		if( ! File::DirExists( pub_path ) )
		{
			File::MkPath( pub_path, File::UserRWX | File::GroupRX | File::OtherRX);
		}

		if( ! File::FileExists( dnsauthkey) || ! File::FileExists( dnspubkey ) )
		{
			logg << Logger::Notice << "Recreating dns key" << lend;
			RSAWrapper dns;
			dns.GenerateKeys();

			if ( File::FileExists(dnsauthkey)) {
				string olddnsauthkey = File::GetContentAsString( dnsauthkey,true );
				File::Write(dnsauthkey+".old",olddnsauthkey, File::UserRW );
			}
			if ( File::FileExists(dnspubkey)) {
				string olddnspubkey = File::GetContentAsString(dnspubkey,true );
				File::Write(dnspubkey+".old",olddnspubkey, File::UserRW | File::GroupRead | File::OtherRead );
			}

			// Make sure we have no keys before writing new ones
			try
			{
				File::Delete( dnsauthkey );
			}
			catch (std::runtime_error &err)
			{
				// Ok if file does not exist
				logg << Logger::Debug << "Unable to delete: " << dnsauthkey << " : " << err.what()<< lend;
			}

			try
			{
				File::Delete( dnspubkey );
			}
			catch (std::runtime_error &err)
			{
				// Ok if file does not exist
				logg << Logger::Debug << "Unable to delete: " << dnspubkey << " : " << err.what()<< lend;
			}

			File::Write(dnsauthkey, dns.PrivKeyAsPEM(), File::UserRW );
			File::Write(dnspubkey, dns.PubKeyAsPEM(), File::UserRW | File::GroupRead | File::OtherRead );
		}

	}
	catch( runtime_error& err)
	{
		this->global_error = string("Failed to register keys: ") + err.what();
		logg << Logger::Notice << this->global_error << lend;
		return false;
	}
	return true;
}

tuple<bool,string> IdentityManager::UploadKeys(const string &unitid, const string &mpwd)
{
	AuthServer s(unitid);
	int resultcode = 0;
	string token;
	json ret;

	tie(resultcode, ret) = s.Login();
	logg << Logger::Debug << "Login resultcode from server: " << resultcode <<lend;

	if( resultcode != Status::Ok && resultcode != Status::Forbidden && resultcode != Status::ServiceUnavailable)
	{
		logg << Logger::Error << "Unexpected reply from server "<< resultcode <<lend;
		return make_tuple(false,"");
	}
	if( resultcode == Status::ServiceUnavailable )
	{
		// keys are missing in secop, register new keys in secop.
		if ( ! this->RegisterKeys() )
		{
			logg << Logger::Error << "Failed to register keys"<< resultcode <<lend;
			return make_tuple(false,"");
		}
		// try to login again
		tie(resultcode, ret) = s.Login();
		logg << Logger::Debug << "Retry Login resultcode from server: " << resultcode <<lend;
		if( resultcode != Status::Ok && resultcode != Status::Forbidden )
		{
			logg << Logger::Error << "Unexpected reply from server "<< resultcode <<lend;
			return make_tuple(false,"");
		}
	}

	if( resultcode == Status::Forbidden  || resultcode == Status::ServiceUnavailable )
	{
		logg << Logger::Debug << "Send Secret"<<lend;

		if( ! ret.contains("reply") || ! ret["reply"].contains("challange")  )
		{
			logg << Logger::Error << "Missing argument from server "<< resultcode <<lend;
			return make_tuple(false,"");
		}

		// Got new challenge to encrypt with master
		string challenge = ret["reply"]["challange"].get<string>();

		RSAWrapperPtr c = AuthServer::GetKeysFromSecop();

		SecVector<byte> key = PBKDF2(SecString(mpwd.c_str(), mpwd.size() ), 32 );
		AESWrapper aes( key );

		string cryptchal = Base64Encode( aes.Encrypt( challenge ) );

		tie(resultcode, ret) = s.SendSecret(cryptchal, Base64Encode(c->PubKeyAsPEM()) );
		if( resultcode != Status::Ok )
		{
			if( resultcode == Status::Forbidden)
			{
				logg << Logger::Debug << "Access denied to OP servers"<<lend;
				return make_tuple(false,"");
			}
			else
			{
				logg << Logger::Debug << "Failed to communicate with OP server"<<lend;
				return make_tuple(false,"");
			}
		}

		if( ret.contains("token") && ret["token"].is_string() )
		{
			token = ret["token"].get<string>();
			if (! this->UploadDnsKey(unitid,token))
			{
				logg << Logger::Error << "Failed to upload DNS key"<<lend;
				return make_tuple(false,"");
			}
		}
		else
		{
			logg << Logger::Error << "Missing argument in reply"<<lend;
			return make_tuple(false,"");
		}

	}
	else
	{
		if( ret.contains("token") && ret["token"].is_string() )
		{
			token = ret["token"].get<string>();
			if (! this->UploadDnsKey(unitid,token))
			{
				logg << Logger::Error << "Failed to upload DNS key"<<lend;
				return make_tuple(false,"");
			}
		}
		else
		{
			logg << Logger::Error << "Missing argument in reply"<<lend;
			return make_tuple(false,"");
		}
	}

	return make_tuple(true,token);
}

void IdentityManager::CleanUp()
{
	if( this->signerthread )
	{
		this->signerthread->Join();
	}
}


bool IdentityManager::UploadDnsKey(const string& unitid, const string& token)
{
	logg << Logger::Error<< "Received token from server: " << token <<lend;

	// Try to upload dns-key
	stringstream pk;
	for( const auto& row: File::GetContent(SCFG.GetKeyAsString("dns","dnspubkey")) )
	{
		pk << row << "\n";
	}
	DnsServer dns;
	string pubkey = Base64Encode( pk.str() );
	if(! dns.RegisterPublicKey(unitid, pubkey, token ))
	{
		logg << Logger::Error<< "Failed to upload DNS key" <<lend;
		return false;
	}
	return true;

}

bool IdentityManager::GetCertificate(const string &fqdn, const string &provider)
{

	logg << Logger::Debug << "Request certificate for '" << fqdn <<"' from '" << provider << "'"<<lend;

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

	int resultcode = 0;
	json ret;
	tie(resultcode, ret) = s.GetCertificate(csr,this->token );

	if( resultcode != Status::Ok )
	{
		logg << Logger::Error << "Failed to get csr "<<resultcode <<lend;
		this->global_error = "Failed to get certificate from OP servers";
		return false;
	}

	if( ! ret.contains("cert") || ! ret["cert"].is_string() )
	{
		logg << Logger::Error << "Malformed reply from server " <<lend;
		this->global_error = "Unexpected reply from OP server when retrieving certificate";
		return false;
	}

	// Make sure we have no symlinked tempcert in place
	unlink( syscert.c_str() );

	File::Write( syscert, ret["cert"].get<string>(), File::UserRW | File::GroupRead | File::OtherRead);

	// TODO: this should be a generic certificate event that opi-mail should react to
	if( ! OPI::ServiceHelper::Reload("postfix") )
	{
		logg << Logger::Error << "Failed to reload mailserver" << lend;
	}

	return true;
}


class SignerThread: public Utils::Thread
{
public:
	SignerThread(string  name): Thread(false), opiname(std::move(name)), result(true) {}

	void Run() override;
	bool Result();
	~SignerThread() override;
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

SignerThread::~SignerThread() = default;

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

	// Check again for unit-id that could have changed
	this->CheckUnitID();

	if( this->unitid.length() == 0 )
	{
		logg << Logger::Notice << "Missing UnitID when trying to do OP-Login" << lend;
		return false;
	}

	AuthServer s( this->unitid);
	int resultcode = 0;
	json ret;

	tie(resultcode, ret) = s.Login();

	if( resultcode != Status::Ok && resultcode != Status::Forbidden )
	{
		logg << Logger::Error << "Unexpected reply from server "<< resultcode <<lend;
		this->global_error ="Unexpected reply from OP server ("+ ret["desc"].get<string>()+")";
		return false;
	}

	if( resultcode == Status::Forbidden)
	{
		this->global_error ="Failed to authenticate with OP server. Wrong activation code or password.";
		return false;
	}

	if( ret.contains("token") && ret["token"].is_string() )
	{
		this->token = ret["token"].get<string>();
	}
	else
	{
		logg << Logger::Error << "Missing argument in reply"<<lend;
		this->global_error ="Failed to communicate with OP server (Missing argument)";
		return false;
	}

	return true;
}

} // Namespace KGP

