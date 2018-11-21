#include "UserManager.h"
#include "MailManager.h"
#include "IdentityManager.h"

#include <algorithm>

#include <libutils/FileUtils.h>
#include <libopi/JsonHelper.h>
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

UserManagerPtr KGP::UserManager::Instance(SecopPtr authdb)
{
	return UserManagerPtr(new UserManager(authdb) );
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
	}

	// Add default email
	IdentityManager &idmgr = IdentityManager::Instance();

	if( idmgr.HasDnsProvider() )
	{
		string host, domain;
		tie(host, domain) = idmgr.GetCurrentDnsName();
		if( host!="" && domain != "" )
		{
			mmgr.SetAddress(host+"."+domain, username, username);
		}
	}

	if( !mmgr.Synchronize()	)
	{
		this->global_error = mmgr.StrError();
		return false;
	}

	return true;
}

bool UserManager::AddUser(const UserPtr user, const string &password, bool isAdmin)
{
	if( user == nullptr )
	{
		return false;
	}

	return this->AddUser( user->GetUsername(), password, user->GetDisplayname(), isAdmin);
}

UserPtr UserManager::GetUser(const string &username)
{
	vector<string> users = this->authdb->GetUsers();

	if( std::find(users.begin(), users.end(), username) == users.end() )
	{
		this->global_error = "User not found";
		return nullptr;
	}

	string displayname;
	try {
		displayname = this->authdb->GetAttribute(username, "displayname");
	} catch (std::runtime_error& err) {
		logg << Logger::Info << "Missing displayname for "<< username << " ("<< err.what()<<")"<<lend;
	}

	return UserPtr(new User(username, displayname) );
}

bool UserManager::UpdateUser(const UserPtr user)
{
	if( user == nullptr || user->GetUsername() == "" )
	{
		this->global_error = "Missing user object or username";
		return false;
	}

	if( ! this->authdb->AddAttribute( user->GetUsername(), "displayname", user->GetDisplayname() ) )
	{
		this->global_error = "Failed to update user";
		return false;
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

bool UserManager::DeleteUser(const UserPtr user)
{
	if( user == nullptr || user->GetUsername() == "")
	{
		this->global_error = "No user object provided or empty user";
		return false;
	}

	return this->DeleteUser( user->GetUsername() );
}

bool UserManager::UpdateUserPassword(const string &user, const string &new_pass, const string &old_pass)
{
	list<map<string,string>>  ids = this->authdb->GetIdentifiers( user, "opiuser");
	if(ids.size() == 0 )
	{
		this->global_error = "Database error";
		return false;
	}

	map<string,string> id = ids.front();
	if( id.find("password") == id.end() )
	{
		this->global_error = "Database error";
		return false;
	}

	if( old_pass != "" )
	{
		// We should check old password against db
		if( old_pass != id["password"] )
		{
			this->global_error = "Bad request";
			return false;
		}
	}

	if( ! this->authdb->UpdateUserPassword( user, new_pass) )
	{
		this->global_error = "Operation failed";
		return false;
	}

	return true;
}

list<UserPtr> UserManager::GetUsers()
{
	list<UserPtr> users;

	vector<string> usernames = this->authdb->GetUsers();
	for(const auto& user: usernames)
	{
		users.push_back(this->GetUser( user ) );
	}

	return users;
}

list<string> UserManager::GetGroups()
{
	vector<string> groups = this->authdb->GetGroups();

	return list<string>(groups.begin(), groups.end());
}

list<string> UserManager::GetUserGroups(const string &user)
{
	vector<string> groups = this->authdb->GetUserGroups( user );

	return list<string>(groups.begin(), groups.end());
}

bool UserManager::AddGroup(const string &groupname)
{
	if( ! this->authdb->AddGroup( groupname) )
	{
		this->global_error = "Failed to add new group";
		return false;
	}
	return true;
}

bool UserManager::DeleteGroup(const string &groupname)
{
	if( ! this->authdb->RemoveGroup( groupname) )
	{
		this->global_error = "Failed to delete group";
		return false;
	}
	return true;
}

bool UserManager::AddGroupMember(const string &group, const string &member)
{
	if( ! this->authdb->AddGroupMember( group, member) )
	{
		this->global_error = "Failed to add member to group";
		return false;
	}
	return true;
}

bool UserManager::DeleteGroupMembar(const string &group, const string &member)
{
	if( ! this->authdb->RemoveGroupMember(group, member) )
	{
		this->global_error = "Failed to remove member from group";
		return false;
	}

	if( group == "admin" )
	{
		// User left admin role, remove from admin mail as well
		MailManager& mmgr = MailManager::Instance();

		if( ! mmgr.RemoveFromAdmin( member ) )
		{
			this->global_error ="Failed to remove user from admin mail";
			return false;
		}
	}
	return true;
}

list<string> UserManager::GetGroupMembers(const string &group)
{
	vector<string> members = this->authdb->GetGroupMembers(group);
	return list<string>(members.begin(), members.end());
}

UserManager::~UserManager()
{

}

/*
 *
 *  User implementation
 *
 */

User::User(const string &username, const string &displayname): username(username), displayname(displayname)
{

}

User::User(const Json::Value &userdata)
{
	JsonHelper::TypeChecker tc(	{
									{0x01, "username", JsonHelper::TypeChecker::STRING},
									{0x02, "displayname", JsonHelper::TypeChecker::STRING}
								} );

	if( ! tc.Verify( 0x01|0x02, userdata ) )
	{
		throw std::runtime_error("Missing parameters in user constructor");
	}

	this->username = userdata["username"].asString();
	this->displayname = userdata["displayname"].asString();
}

string User::GetUsername()
{
	return this->username;
}

string User::GetDisplayname()
{
	return this->displayname;
}

void User::AddAttribute(const string &attr, const string& value)
{
	this->attributes[attr] = value;
}

string User::GetAttribute(const string &attr)
{
	if( this->attributes.find(attr) == this->attributes.end() )
	{
		throw std::runtime_error("Unable to find attribute");
	}
	return this->attributes[attr];
}

Json::Value User::ToJson()
{
	Json::Value ret;

	ret["username"] = this->username;
	ret["displayname"] = this->displayname;

	return ret;
}

User::~User()
{

}

} // Namespace KGP
