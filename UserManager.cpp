#include "UserManager.h"
#include "MailManager.h"

#include <algorithm>

#include <libutils/FileUtils.h>
#include <libutils/Logger.h>
#include <libutils/Process.h>
#include <libopi/SysConfig.h>
#include <libopi/Secop.h>

#define SCFG	(OPI::SysConfig())


using namespace OPI;
using namespace Utils;
using namespace std;

namespace KGP {

UserManager::UserManager(SecopPtr authdb):authdb(authdb)
{
	if( ! this->authdb )
	{
		this->authdb = SecopPtr(new Secop());
		this->authdb->SockAuth();
	}
}

KGP::UserManager &KGP::UserManager::Instance()
{
	static UserManager um;

	return um;
}

bool UserManager::AddUser(const string &username, const string &password, const string &displayname, bool isAdmin)
{

	if( ! this->authdb->CreateUser(username, password, displayname) )
	{
		this->global_error = "Failed to create user (User exists?)";
		return false;
	}

	MailManager &mmgr = MailManager::Instance();

	// Set local mail address
	if( ! mmgr.SetLocalAddress( username ) )
	{
		this->global_error = mmgr.StrError();
		return false;
	}

	if( isAdmin )
	{
		if( ! this->authdb->AddGroupMember("admin", username) )
		{
			this->global_error = "Failed to make user admin";
			return false;
		}

		// Add this user as receiver of administrative mail
		if( !mmgr.AddToAdmin(username))
		{
			this->global_error = mmgr.StrError();
			return false;
		}

		if( !mmgr.Synchronize()	)
		{
			this->global_error = mmgr.StrError();
			return false;
		}

	}

	return true;

}

bool UserManager::DeleteUser(const string &user)
{

	if( ! this->authdb->RemoveUser( user ) )
	{
		this->global_error = "Failed to remove user: " + user;
		logg << Logger::Error << this->global_error << lend;
		return false;
	}

	// Remove all mail
	MailManager& mmgr = MailManager::Instance();
	mmgr.DeleteUser( user );
	mmgr.Synchronize();

	// delete the users files and mail
	logg << Logger::Debug << "Deleting files for user: " << user << lend;
	try {
		string storage = SCFG.GetKeyAsString("filesystem","storagemount");
		string dir = storage + "/mail/data/" + user;
		if( File::DirExists(dir.c_str()))
		{
			Process::Exec("rm -rf "+ dir);
		}
		dir = storage + "/nextcloud/data/" + user;
		if( File::DirExists(dir.c_str()))
		{
			Process::Exec("rm -rf "+ dir);
		}
	}
	catch (std::runtime_error& e)
	{
		logg << Logger::Error << "Failed to delete user files" << e.what() << lend;
		return false;
	}

	return true;
}

UserManager::~UserManager()
{

}

} // Namespace KGP
