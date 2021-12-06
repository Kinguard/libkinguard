#ifndef USERMANAGER_H
#define USERMANAGER_H

#include "BaseManager.h"

#include <libopi/Secop.h>

#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <map>

using namespace std;
using json = nlohmann::json;
namespace KGP {

class UserManager;
typedef shared_ptr<UserManager>  UserManagerPtr;

/**
 * @brief The User class
 *
 * Holds user attributes about a user
 *
 * Currently supports
 * UserName - non updateable attribute that uniquely identifies user in system
 * DisplayName - User readable version of name
 *
 * Custom attributes, key value string pairs
 *
 * Note that user password is not part of user attributes and is handled
 * separately
 *
 */
class User
{
public:
	/**
	 * @brief User creat new user with username and displayname
	 * @param username
	 * @param displayname
	 */
	User(string  username, string  displayname, map<string,string> attrs = {});

	/**
	 * @brief User Creates new user from userdata
	 * @param userdata json value with attributes
	 *        must contain at least username
	 */
	User(const json& userdata);

	/**
	 * @brief GetUsername
	 * @return objects username
	 */
	string GetUsername(void);

	/**
	 * @brief GetDisplayname
	 * @return user displayname
	 */
	string GetDisplayname(void);

	/**
	 * @brief AddAttribute add attribute to user
	 *
	 * @param attr
	 * @param value
	 *
	 * @note currently not synched to backend
	 */
	void AddAttribute(const string& attr, const string &value);

	/**
	 * @brief GetAttribute retrieve user attribute
	 * @param attr
	 * @return
	 */
	string GetAttribute(const string& attr);

	/**
	 * @brief GetAttributes retrieve all user attributes
	 *        excluding username and displayname
	 * @return key value map of attributes
	 */
	map<string,string> GetAttributes(void);

	/**
	 * @brief ToJson serialize user to json object
	 *
	 * @return json object with one level key value attributes
	 */
	json ToJson(void);

	virtual ~User() = default;
private:
	string username;
	string displayname;
	map<string, string> attributes;
};

typedef shared_ptr<User> UserPtr;

class UserManager : public BaseManager
{
private:
	UserManager(OPI::SecopPtr authdb=nullptr);

public:

	/**
	 * @brief Instance, retrieve a new usermanager instance
	 * @param authdb authdb to use or nullptr to use default
	 * @return shared pointer to usagemanager or nullptr
	 */
	static UserManagerPtr Instance(OPI::SecopPtr authdb = nullptr);

	/**
	 * @brief UserExists check if user exists in db
	 * @param username
	 * @return true if user exists
	 */
	bool UserExists(const string& username);

	/**
	 * @brief AddUser add new user to system
	 *			User is added to the system and basic email is setup
	 * @param username
	 * @param password
	 * @param displayname Full name of new user
	 * @param isAdmin If true user is given admin rights, notifications etc
	 * @param attributes optional key value map with further attributes
	 * @return true upon success false otherwise
	 */
	bool AddUser(const string& username, const string& password, const string& displayname, bool isAdmin, const map<string,string>& attributes = {});

	/**
	 * @brief AddUser add new user to system
	 * @param user userobject of user to add
	 * @param password users password
	 * @param isAdmin is this user an admin user?
	 * @return true upon success
	 */
	bool AddUser(const UserPtr &user, const string& password, bool isAdmin);

	/**
	 * @brief GetUser retrieve userinfo on user
	 * @param username
	 * @return shared pointer to user or nullptr
	 */
	UserPtr GetUser(const string& username);

	/**
	 * @brief UpdateUser Update a system user
	 * @param user userobject to use for update, username must exist
	 * @return true upon success
	 */
	bool UpdateUser(const UserPtr& user);

	/**
	 * @brief DeleteUser delete user from system
	 * @param user
	 * @return true upon success
	 */
	bool DeleteUser(const string& user);

	/**
	 * @brief DeleteUser delete user from system
	 * @param user
	 * @return true upon success
	 */
	bool DeleteUser(const UserPtr& user);

	/**
	 * @brief UpdateUserPassword
	 * @param user User for which password should be updated
	 * @param new_pass New password for user
	 * @param old_pass Optional old password for user, if provided must
	 *        match stored old password or operation will fail
	 *
	 * @return true upon success
	 */
	bool UpdateUserPassword(const string& user, const string& new_pass, const string& old_pass = "");

	/**
	 * @brief GetUsers get all users in system
	 * @return list of shared pointers to user objects
	 */
	list<UserPtr> GetUsers(void);

	/*
	 *
	 * Group management
	 *
	 */

	/**
	 * @brief GetGroups
	 * @return list of group names on system
	 */
	list<string> GetGroups(void);

	/**
	 * @brief GetUserGroups
	 * @param user
	 * @return list of group names that user belongs to
	 */
	list<string> GetUserGroups(const string& user);


	/**
	 * @brief AddGroup create new group
	 * @param groupname
	 * @return true upon success
	 */
	bool AddGroup(const string& groupname);


	/**
	 * @brief DeleteGroup delete group
	 * @param groupname
	 * @return true upon success
	 */
	bool DeleteGroup(const string& groupname);

	/**
	 * @brief AddGroupMember add new member to group
	 * @param group
	 * @param member
	 * @return true upon success
	 */
	bool AddGroupMember(const string& group, const string& member);

	/**
	 * @brief DeleteGroupMembar remove member from group
	 * @param group
	 * @param member
	 * @return true upon success
	 */
	bool DeleteGroupMember(const string& group, const string& member);

	/**
	 * @brief GetGroupMembers get all members of group
	 * @param group
	 * @return list of members
	 */
	list<string> GetGroupMembers(const string& group);


	virtual ~UserManager() = default;
private:
	OPI::SecopPtr authdb;
};

} // Namespace KGP
#endif // USERMANAGER_H
