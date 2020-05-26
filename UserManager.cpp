#include "UserManager.h"
#include "MailManager.h"
#include "IdentityManager.h"

#include <algorithm>
#include <memory>
#include <utility>

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

UserManager::UserManager(SecopPtr authdb):authdb(std::move(authdb))
{
	if( ! this->authdb )
	{
		this->authdb = std::make_shared<Secop>();
		this->authdb->SockAuth();
	}
}

UserManagerPtr KGP::UserManager::Instance(SecopPtr authdb)
{
	return UserManagerPtr(new UserManager(std::move(authdb)) );
}

bool UserManager::UserExists(const string &username)
{
	vector<string> users = this->authdb->GetUsers();

	return  std::find(users.begin(), users.end(), username) != users.end();
}

bool UserManager::AddUser(const string &username, const string &password, const string &displayname, bool isAdmin, const map<string, string> &attributes)
{

	if( ! this->authdb->CreateUser(username, password, displayname) )
	{
		this->global_error = "Failed to create user (User exists?)";
		return false;
	}

	for( const pair<const string, string>& attr: attributes )
	{
		this->authdb->AddAttribute( username, attr.first, attr.second );
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

	string host, domain;
	if( idmgr.HasDnsProvider() )
	{
		tie(host, domain) = idmgr.GetCurrentDnsName();
	}
	else
	{
		logg << Logger::Notice << "No dns-provider available, assigning local mail address" << lend;
		tie(host,domain) = idmgr.GetFqdn();
	}

	if( host!="" && domain != "" )
	{
		logg << Logger::Debug << "Adding address " << username << "@"<< host << "."<<domain << " to " << username << lend;
		mmgr.SetAddress(host+"."+domain, username, username);
	}
	else
	{
		logg << Logger::Notice << "No valid hostname, not adding email address to user " << username << lend;
	}

	if( !mmgr.Synchronize()	)
	{
		this->global_error = mmgr.StrError();
		return false;
	}

	return true;
}

bool UserManager::AddUser(const UserPtr& user, const string &password, bool isAdmin)
{
	if( user == nullptr )
	{
		return false;
	}

	return this->AddUser( user->GetUsername(), password, user->GetDisplayname(), isAdmin, user->GetAttributes());
}

UserPtr UserManager::GetUser(const string &username)
{

	if( ! this->UserExists( username ) )
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

	UserPtr user = std::make_shared<User>(username, displayname );

	vector<string> attrs = this->authdb->GetAttributes( username );
	for(const string& attr: attrs)
	{
		if( attr != "displayname" )
		{
			user->AddAttribute(attr, this->authdb->GetAttribute(username, attr) );
		}
	}

	return user;
}

bool UserManager::UpdateUser(const UserPtr& user)
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

	for( const pair<const string,string>& attr: user->GetAttributes() )
	{
		this->authdb->AddAttribute( user->GetUsername(), attr.first, attr.second);
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

bool UserManager::DeleteUser(const UserPtr& user)
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

bool UserManager::DeleteGroupMember(const string &group, const string &member)
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


/*
 *
 *  User implementation
 *
 */

User::User(string username, string displayname, map<string, string> attrs): username(std::move(username)), displayname(std::move(displayname)),attributes(std::move(attrs))
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

	vector<string> members = userdata.getMemberNames();
	for( const string& member: members)
	{
		if( member != "username" && member != "displayname" )
		{
			if( userdata[member].isString() )
			{
				this->AddAttribute(member, userdata[member].asString() );
			}
			else
			{
				logg << Logger::Debug << "Malformed attribute to create user " << member<< lend;
			}
		}
	}

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

map<string, string> User::GetAttributes()
{
	return this->attributes;
}

Json::Value User::ToJson()
{
	Json::Value ret;

	ret["username"] = this->username;
	ret["displayname"] = this->displayname;

	return ret;
}

} // Namespace KGP
