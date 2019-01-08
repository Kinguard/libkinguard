#include "IdentityManager.h"
#include "MailManager.h"

#include <ext/stdio_filebuf.h>

#include <libutils/Logger.h>
#include <libutils/String.h>
#include <libutils/Process.h>
#include <libutils/FileUtils.h>

#include <libopi/AuthServer.h>
#include <libopi/Secop.h>
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

bool IdentityManager::SetFqdn(const string &name, const string &domain)
{
	string oldFqdn = this->GetFqdnAsString();
	MailManager& mailmgr = MailManager::Instance();

	try
	{
		OPI::SysConfig cfg(true);
		this->hostname = name;
		this->domain = domain;
		cfg.PutKey("hostinfo","hostname", name);
		cfg.PutKey("hostinfo","domain", domain);

		// Update mail hostname
		mailmgr.SetHostname(name, domain);
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
		this->global_error = string("Failed change manilhostname: ") + err.what();
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

tuple<bool,string> IdentityManager::WriteCustomCertificate(string key, string cert)
{
	SysConfig sysconfig(true);


	string CustomCertFile,CustomKeyFile;
	string webcert = sysconfig.GetKeyAsString("webcertificate","activecert");
	string webkey = sysconfig.GetKeyAsString("webcertificate","activekey");


	// Generate new filenames
	logg << Logger::Debug << "Generating new custom cert paths" << lend;

	sprintf( this->tmpfilename,"/etc/opi/usercert/userCertXXXXXX.pem");

	// check that the path exists.
	if ( ! File::DirExists(File::GetPath(this->tmpfilename)) )
	{
		// create paths.
		try
		{
			File::MkPath(File::GetPath(this->tmpfilename),0700);
		}
		catch (Utils::ErrnoException& err)
		{
			logg << Logger::Error << "Failed to create path for usercerts:" << err.what() << lend;
			this->global_error = string("Failed to create path for usercerts:") + err.what();
			return make_tuple(false,"Failed to create path for usercerts:");
		}
	}

	int certfd = mkstemps(this->tmpfilename,4);

	if( certfd <0 )
	{
		return make_tuple(false,"Failed to create cert file");
	}
	else
	{
		CustomCertFile = string(this->tmpfilename);
		logg << Logger::Debug << "CustomCertFile:" << CustomCertFile<< lend;
	}
	// Write content to new files
	__gnu_cxx::stdio_filebuf<char> certfb( certfd, std::ios::out);
	ostream scert(&certfb);
	scert << cert;
	scert << flush;

	if ( key != "" ) {

		sprintf( this->tmpfilename,"/etc/opi/usercert/userKeyXXXXXX.pem");
		int keyfd = mkstemps(this->tmpfilename,4);

		if( keyfd <0 )
		{
			return make_tuple(false,"Failed to create key file");
		}
		else
		{
			CustomKeyFile = string(this->tmpfilename);
			logg << Logger::Debug << "CustomKeyFile:" << CustomKeyFile<< lend;
		}
		__gnu_cxx::stdio_filebuf<char> keyfb( keyfd, std::ios::out);
		ostream skey(&keyfb);
		skey << key;
		skey << flush;
	}
	else
	{
		CustomKeyFile = File::RealPath(webkey);
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
	int retval;
	string Message;

	tie(retval,Message)=Process::Exec( "nginx -t" );
	if ( retval )
	{
		try
		{
			if (SCFG.HasKey("webcertificate","backend") && (SCFG.GetKeyAsString("webcertificate","backend") == "CUSTOMCERT") )
			{
				// previous setup used Custom certificate, delete it
				logg << Logger::Error << "Removing old certs" << lend;
				File::Delete(curr_cert);
				if ( key != "")
				{
					// only remove it if it has been replaced
					File::Delete(curr_key);
				}
			}
			sysconfig.PutKey("webcertificate","backend","CUSTOMCERT");
		}
		catch (std::runtime_error& e)
		{
			logg << Logger::Error << "Failed to set config parameters" << lend;
			return make_tuple(false,"Failed to set config parameters");
		}

		return make_tuple(true,"");
	}
	else
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
}
bool IdentityManager::CreateCertificate()
{
	if ( SCFG.HasKey("webcertificate","backend") )
	{
		return this->CreateCertificate(true,SCFG.GetKeyAsString("webcertificate","backend"));
	}
	return false;
}

bool IdentityManager::CreateCertificate(bool forceProvider, string certtype)
{

	OPI::SysConfig cfg(true);
	string fqdn = this->GetHostname() + "." + this->GetDomain();
	string provider;

	cfg.PutKey("webcertificate","backend",certtype);
	if ( forceProvider )
	{
		try
		{
			// generate certificate for provider, or if no provider generate self signed certificate
			if ( this->HasDnsProvider() ) {
				provider = cfg.GetKeyAsString("dns","provider");
				if ( provider == "OpenProducts" )
				{
					provider = "OPI";  // legacy, provider shall be OPI for OpenProducts certificates.
				}

				logg << Logger::Debug << "Request certificate from '" << provider << "'"<<lend;
				if( !this->GetCertificate(fqdn, provider) )
				{
					logg << Logger::Error << "Failed to get certificate for device name: "<<fqdn<<lend;
					return false;
				}
			}
			else
			{

				if( ! OPI::CryptoHelper::MakeSelfSignedCert(
							cfg.GetKeyAsString("dns","dnsauthkey"),
							cfg.GetKeyAsString("hostinfo","syscert"),
							fqdn,
							cfg.GetKeyAsString("hostinfo","hostname")
							) )
				{
					return false;
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

	logg << Logger::Debug << "Get signed Certificate for '"<< fqdn <<"'"<<lend;
	if( ! this->GetSignedCert(fqdn) )
	{
		// The call forks a new process and can not really fail, but the generation of the certificate
		// can fail. But that can not be indicted here...
		logg << Logger::Notice << "Failed to get launch thread to get signed cert."<<lend;
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

bool IdentityManager::SetDNSProvider(string provider)
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
	string sysauthkey = SCFG.GetKeyAsString("hostinfo","sysauthkey");
	string syspubkey = SCFG.GetKeyAsString("hostinfo","syspubkey");
	string dnsauthkey = SCFG.GetKeyAsString("dns","dnsauthkey");
	string dnspubkey = SCFG.GetKeyAsString("dns","dnspubkey");
	try{

		// If OP device
		if( this->HasDnsProvider() )
		{
			AuthServer::Setup();

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
		string priv_path = File::GetPath( dnsauthkey );
		if( ! File::DirExists( priv_path ) )
		{
			File::MkPath( priv_path, 0755);
		}

		string pub_path = File::GetPath( dnspubkey );
		if( ! File::DirExists( pub_path ) )
		{
			File::MkPath( pub_path, 0755);
		}

		if( ! File::FileExists( dnsauthkey) || ! File::FileExists( dnspubkey ) )
		{
			RSAWrapper dns;
			dns.GenerateKeys();

			if ( File::FileExists(sysauthkey)) {
				string olddnsauthkey = File::GetContentAsString( dnsauthkey,true );
				File::Write(dnsauthkey+".old",olddnsauthkey, 0600 );
			}
			if ( File::FileExists(syspubkey)) {
				string olddnspubkey = File::GetContentAsString(dnspubkey,true );
				File::Write(dnspubkey+".old",olddnspubkey, 0644 );
			}

			File::Write(dnsauthkey, dns.PrivKeyAsPEM(), 0600 );
			File::Write(dnspubkey, dns.PubKeyAsPEM(), 0644 );
		}

	}
	catch( runtime_error& err)
	{
		logg << Logger::Notice << "Failed to register keys " << err.what() << lend;
		return false;
	}
	return true;
}

tuple<bool,string> IdentityManager::UploadKeys(string unitid,string mpwd)
{
	AuthServer s(unitid);
	int resultcode;
	string token;
	Json::Value ret;

	tie(resultcode, ret) = s.Login();
	logg << Logger::Debug << "Login resultcode from server: " << resultcode <<lend;

	if( resultcode != 200 && resultcode != 403 && resultcode != 503)
	{
		logg << Logger::Error << "Unexpected reply from server "<< resultcode <<lend;
		return make_tuple(false,"");
	}
	if( resultcode == 503 )
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
		if( resultcode != 200 && resultcode != 403 )
		{
			logg << Logger::Error << "Unexpected reply from server "<< resultcode <<lend;
			return make_tuple(false,"");
		}
	}

	if( resultcode == 403  || resultcode == 503 )
	{
		logg << Logger::Debug << "Send Secret"<<lend;

		if( ! ret.isMember("reply") || ! ret["reply"].isMember("challange")  )
		{
			logg << Logger::Error << "Missing argument from server "<< resultcode <<lend;
			return make_tuple(false,"");
		}

		// Got new challenge to encrypt with master
		string challenge = ret["reply"]["challange"].asString();

		RSAWrapperPtr c = AuthServer::GetKeysFromSecop();

		SecVector<byte> key = PBKDF2(SecString(mpwd.c_str(), mpwd.size() ), 32 );
		AESWrapper aes( key );

		string cryptchal = Base64Encode( aes.Encrypt( challenge ) );

		tie(resultcode, ret) = s.SendSecret(cryptchal, Base64Encode(c->PubKeyAsPEM()) );
		if( resultcode != 200 )
		{
			if( resultcode == 403)
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

		if( ret.isMember("token") && ret["token"].isString() )
		{
			token = ret["token"].asString();
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
		if( ret.isMember("token") && ret["token"].isString() )
		{
			token = ret["token"].asString();
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

IdentityManager::~IdentityManager()
{

}

bool IdentityManager::UploadDnsKey(string unitid, string token)
{
	logg << Logger::Error<< "Received token from server: " << token <<lend;

	// Try to upload dns-key
	stringstream pk;
	for( auto row: File::GetContent(SCFG.GetKeyAsString("dns","dnspubkey")) )
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

} // Namespace KGP

