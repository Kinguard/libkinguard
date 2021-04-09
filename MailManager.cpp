#include "MailManager.h"

#include <libopi/FetchmailConfig.h>
#include <libopi/ServiceHelper.h>
#include <libopi/MailConfig.h>

#include <libutils/UserGroups.h>
#include <libutils/Process.h>
#include <libutils/Logger.h>

using namespace OPI;

//TODO: move to sysconfig or better, move storage to secop
#define FETCHMAILRC	"/var/opi/etc/fetchmailrc"

namespace KGP
{

MailManager::MailManager(): fetchmailupdated(false), postfixupdated(false)
{
	logg << Logger::Notice << "Mailmanager initialized" << lend;
}

MailManager &MailManager::Instance()
{
	static MailManager mgr;

	return mgr;
}

void MailManager::DeleteUser(const string &user)
{
	// Remove any fetchmail accounts
	list<map<string,string>> accounts = this->GetRemoteAccounts( user );

	for(map<string,string>& account: accounts)
	{
		this->DeleteRemoteAccount( account["host"], account["identity"]);
	}

	// Delete all incoming addresses
	this->DeleteAddresses( user );

	// Delete user from localmail
	this->RemoveLocalAddress(user);

	// If in aliases, remove as well
	this->RemoveUserAliases( user );
}

bool MailManager::AddToAdmin(const string &user)
{
	if( ! this->AddUserAlias("/^postmaster@/",user+"@localdomain") ||
			! this->AddUserAlias("/^root@/",user+"@localdomain"))
	{
		return false;
	}

	return true;
}

bool MailManager::RemoveFromAdmin(const string &user)
{
	if( ! this->RemoveUserAlias("/^postmaster@/",user+"@localdomain") ||
			! this->RemoveUserAlias("/^root@/",user+"@localdomain") )
	{
		return false;
	}

	return true;
}

bool MailManager::SetHostname(const string &name, const string& domain)
{
	try
	{
		File::Write("/etc/mailname", name + "."+ domain, File::UserRW | File::GroupRead | File::OtherRead);

		bool res = false;
		stringstream cmd;
		cmd << "/usr/sbin/postconf -e \"myhostname=" << name << "." << domain <<"\"";
		tie(res, std::ignore) = Process::Exec(cmd.str() );

		if( ! res )
		{
			this->global_error = "Failed to update postfix hostname";
			logg << Logger::Error << this->global_error << lend;
			return false;
		}

	}
	catch (std::runtime_error& err)
	{
		this->global_error = string("Failed to set mail hostname: ") + err.what();
		logg << Logger::Error << this->global_error << lend;
		return false;
	}

	return true;
}

list<string> MailManager::GetDomains()
{
	return MailConfig().GetDomains();
}

void MailManager::AddDomain(const string &domain)
{
	MailConfig mc;

	mc.AddDomain(domain);
	mc.WriteConfig();
	this->postfixupdated = true;
}

void MailManager::DeleteDomain(const string &domain)
{
	MailConfig mc;
	mc.DeleteDomain( domain );
	mc.WriteConfig();
	this->postfixupdated = true;
}

void MailManager::SetAddress(const string &domain, const string &address, const string &user)
{
	MailConfig mc;
	mc.SetAddress(domain, address, user);
	mc.WriteConfig();
	this->postfixupdated = true;
}

void MailManager::DeleteAddress(const string &domain, const string &address)
{
	MailConfig mc;
	mc.DeleteAddress(domain, address);
	mc.WriteConfig();
	this->postfixupdated = true;
}

void MailManager::DeleteAddresses(const string &user)
{
	MailConfig mc;

	list<string> doms = mc.GetDomains();

	for( const auto& domain: doms)
	{
		list<tuple<string,string>> addrs = mc.GetAddresses(domain);

		for( const tuple<string,string> &addr: addrs)
		{
			string localpart, auser;
			tie(localpart, auser) = addr;

			if( auser == user )
			{
				mc.DeleteAddress(domain,localpart);
			}
		}

	}
	mc.WriteConfig();
	this->postfixupdated = true;

}

list<tuple<string, string> > MailManager::GetAddresses(const string &domain)
{
	MailConfig mc;
	return mc.GetAddresses(domain);
}

void MailManager::ChangeDomain(const string &from, const string &to)
{
	MailConfig mc;
	mc.ChangeDomain(from, to);
	mc.WriteConfig();
	this->postfixupdated = true;
}

tuple<string, string> MailManager::GetAddress(const string &domain, const string &address)
{
	return MailConfig().GetAddress(domain, address);
}

bool MailManager::hasDomain(const string &domain)
{
	return MailConfig().hasDomain(domain);
}

bool MailManager::hasAddress(const string &domain, const string &address)
{
	return MailConfig().hasAddress(domain, address);
}

bool MailManager::SetLocalAddress(const string &user)
{
	const string localmail(SCFG.GetKeyAsString("filesystem", "storagemount")+SCFG.GetKeyAsString("mail", "localmail"));
	try
	{
		// Add user to localdomain mailboxfile

		OPI::MailMapFile mmf( localmail );
		mmf.ReadConfig();
		mmf.SetAddress("localdomain", user, user);
		mmf.WriteConfig();

		if( chown( localmail.c_str(), User::UserToUID("postfix"), Group::GroupToGID("postfix") ) != 0 )
		{
			this->global_error = string("Failed to chown localmail (")+strerror(errno)+")";
			logg << Logger::Error << this->global_error << lend;
			return false;
		}
	}
	catch( runtime_error& err)
	{
		this->global_error = string("Failed to add local mail address (")+err.what()+")";
		return false;
	}

	this->postfixupdated = true;
	return true;
}

bool MailManager::RemoveLocalAddress(const string &user)
{
	const string localmail(SCFG.GetKeyAsString("filesystem", "storagemount")+SCFG.GetKeyAsString("mail", "localmail"));
	try
	{
		// Remove user from localdomain mailboxfile

		OPI::MailMapFile mmf( localmail );
		mmf.ReadConfig();
		mmf.DeleteAddress("localdomain", user);
		mmf.WriteConfig();
	}
	catch( runtime_error& err)
	{
		this->global_error = string("Failed to remove local mail address (")+err.what()+")";
		logg << Logger::Error << this->global_error << lend;
		return false;
	}

	this->postfixupdated = true;
	return true;
}

list<map<string, string> > MailManager::GetRemoteAccounts(const string &user)
{
	FetchmailConfig fc( FETCHMAILRC );

	return fc.GetAccounts( user );
}

map<string, string> MailManager::GetRemoteAccount(const string &hostname, const string &identity)
{
	FetchmailConfig fc( FETCHMAILRC );

	return fc.GetAccount( hostname, identity);
}

void MailManager::AddRemoteAccount(const string &email, const string &host, const string &identity, const string &password, const string &user, bool ssl)
{
	FetchmailConfig fc( FETCHMAILRC);

	fc.AddAccount(email, host, identity, password, user, ssl );
	fc.WriteConfig();

	this->fetchmailupdated = true;
}

void MailManager::UpdateRemoteAccount(const string &email, const string &host, const string &identity, const string &password, const string &user, bool ssl)
{
	FetchmailConfig fc( FETCHMAILRC );

	fc.UpdateAccount(email, host, identity, password, user, ssl );
	fc.WriteConfig();

	this->fetchmailupdated = true;
}

void MailManager::DeleteRemoteAccount(const string &hostname, const string &identity)
{
	FetchmailConfig fc( FETCHMAILRC );

	fc.DeleteAccount(hostname, identity);
	fc.WriteConfig();

	this->fetchmailupdated = true;
}

list<string> MailManager::GetAliases()
{
	list<string> ret;
	const string virtual_aliases(SCFG.GetKeyAsString("filesystem", "storagemount") + SCFG.GetKeyAsString("mail","virtualalias"));
	try
	{
		OPI::MailAliasFile mf( virtual_aliases );

		ret = mf.GetAliases();
	}
	catch( runtime_error& err)
	{
		this->global_error = string("Failed to retrieve aliases (")+err.what()+")";
	}
	return ret;
}

list<string> MailManager::GetAliasUsers(const string &alias)
{
	list<string> ret;
	const string virtual_aliases(SCFG.GetKeyAsString("filesystem", "storagemount") + SCFG.GetKeyAsString("mail","virtualalias"));
	try
	{
		OPI::MailAliasFile mf( virtual_aliases );

		ret = mf.GetUsers( alias );
	}
	catch( runtime_error& err)
	{
		this->global_error = string("Failed to retrieve aliases (")+err.what()+")";
	}
	return ret;
}

bool MailManager::AddUserAlias(const string &alias, const string &user)
{
	const string virtual_aliases(SCFG.GetKeyAsString("filesystem", "storagemount") + SCFG.GetKeyAsString("mail","virtualalias"));
	try
	{
		OPI::MailAliasFile mf( virtual_aliases );

		mf.AddUser(alias,user);

		mf.WriteConfig();
	}
	catch( runtime_error& err)
	{
		this->global_error = string("Failed to add user alias (")+err.what()+")";
		return false;
	}

	if ( chown( virtual_aliases.c_str(), User::UserToUID("postfix"), Group::GroupToGID("postfix") ) != 0 )
	{
		this->global_error = strerror(errno);
		logg << Logger::Error << "Failed to chown aliases file: " << this->global_error << lend;
		return false;
	}

	this->postfixupdated = true;
	return true;
}

bool MailManager::RemoveUserAlias(const string &alias, const string &user)
{
	const string virtual_aliases(SCFG.GetKeyAsString("filesystem", "storagemount") + SCFG.GetKeyAsString("mail","virtualalias"));
	try
	{
		OPI::MailAliasFile mf( virtual_aliases );

		mf.RemoveUser(alias, user);

		mf.WriteConfig();
	}
	catch( runtime_error& err)
	{
		this->global_error = string("Failed to remove user alias (")+err.what()+")";
		logg << Logger::Error << this->global_error << lend;
		return false;
	}

	return true;
}

bool MailManager::RemoveUserAliases(const string &user)
{
	list<string> aliases = this->GetAliases();

	for( const string& alias: aliases)
	{

		list<string> users = this->GetAliasUsers( alias );

		for( const string& auser: users)
		{
			if( auser == user )
			{
				if( ! this->RemoveUserAlias(alias, user) )
				{
					return false;
				}
			}
		}

	}
	return true;
}


// From OPI-B
// Merge of update_postfix and reload fetchmail
bool MailManager::Synchronize(bool force)
{
	bool p_ret = true; // Postfixreturn
	bool f_ret = true; // Fetchmailreturn
	SysConfig sysconfig;

	if( this->postfixupdated || force )
	{
		bool status = false;
		string aliases = sysconfig.GetKeyAsString("filesystem","storagemount") + "/" + sysconfig.GetKeyAsString("mail","vmailbox");
		tie(status, std::ignore) = Utils::Process::Exec( "/usr/sbin/postmap " + aliases );
		if( !status )
		{
			this->global_error = "Falied to process aliases file";
			logg << Logger::Error << this->global_error  << lend;
			p_ret = false;
		}

		string saslpwd = sysconfig.GetKeyAsString("filesystem","storagemount") + "/" + sysconfig.GetKeyAsString("mail","saslpasswd");
		tie(status, std::ignore) = Utils::Process::Exec( "/usr/sbin/postmap " + saslpwd );
		if( !status )
		{
			this->global_error = "Falied to process sasl password file";
			logg << Logger::Error << this->global_error << lend;
			p_ret = false;
		}

		string localmail = sysconfig.GetKeyAsString("filesystem","storagemount") + "/" + sysconfig.GetKeyAsString("mail","localmail");
		tie(status, std::ignore) = Utils::Process::Exec( "/usr/sbin/postmap " + localmail );
		if( !status )
		{
			this->global_error = "Falied to process local mail file";
			logg << Logger::Error << this->global_error << lend;
			p_ret = false;
		}

		status = ServiceHelper::Reload("postfix");
		if( !status )
		{
			this->global_error = "Falied to reload postfix";
			logg << Logger::Error << this->global_error << lend;
			p_ret = false;
		}

		if( p_ret )
		{
			this->postfixupdated = false;
		}
	}

	if ( this->fetchmailupdated || force )
	{
		f_ret = ServiceHelper::Stop( "fetchmail" );
		f_ret &= ServiceHelper::Start( "fetchmail" );

		if( f_ret)
		{
			this->fetchmailupdated = false;
		}
		else
		{
			this->global_error = "Falied to restart fetchmail";
			logg << Logger::Error << this->global_error << lend;
		}
	}


	return p_ret && f_ret;
}


// From opi-b postfix_fixpaths
void MailManager::SetupEnvironment()
{
	SysConfig sysconfig;

	string aliases = sysconfig.GetKeyAsString("filesystem","storagemount") + sysconfig.GetKeyAsString("mail","vmailbox");
	if( ! File::FileExists( aliases ) )
	{
		File::Write( aliases, "", File::UserRW);
	}

	string saslpwd = sysconfig.GetKeyAsString("filesystem","storagemount")  + sysconfig.GetKeyAsString("mail","saslpasswd");
	if( ! File::FileExists( saslpwd ) )
	{
		File::Write( saslpwd, "", File::UserRW);
	}

	string domains = sysconfig.GetKeyAsString("filesystem","storagemount") + sysconfig.GetKeyAsString("mail","vdomains");
	if( ! File::FileExists( domains ) )
	{
		File::Write( domains, "", File::UserRW);
	}

	string localmail = sysconfig.GetKeyAsString("filesystem","storagemount") + sysconfig.GetKeyAsString("mail","localmail");
	if( ! File::FileExists( localmail ) )
	{
		File::Write( localmail, "", File::UserRW);
	}

	if( chown( aliases.c_str(), User::UserToUID("postfix"), Group::GroupToGID("postfix") ) != 0)
	{
		logg << Logger::Error << "Failed to change owner on aliases file"<<lend;
	}

	if( chown( saslpwd.c_str(), User::UserToUID("postfix"), Group::GroupToGID("postfix") ) != 0)
	{
		logg << Logger::Error << "Failed to change owner on saslpasswd file"<<lend;
	}

	if( chown( domains.c_str(), User::UserToUID("postfix"), Group::GroupToGID("postfix") ) != 0)
	{
		logg << Logger::Error << "Failed to change owner on domain file"<<lend;
	}

	if( chown( File::GetPath(domains).c_str(), User::UserToUID("postfix"), Group::GroupToGID("postfix") ) != 0)
	{
		logg << Logger::Error << "Failed to change owner on config directory"<<lend;
	}

	if( chmod( File::GetPath(domains).c_str(), File::UserRWX ) != 0)
	{
		logg << Logger::Error << "Failed to change mode on config directory"<<lend;
	}

}

MailManager::~MailManager()
{
	logg << Logger::Notice << "Mailmanager destroyed" << lend;
}

} // Namespace KGP
