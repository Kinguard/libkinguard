#ifndef USERMANAGER_H
#define USERMANAGER_H

#include "BaseManager.h"

#include <libopi/Secop.h>

#include <string>

using namespace std;

namespace KGP {


class UserManager : public BaseManager
{
private:
	UserManager(OPI::SecopPtr authdb=nullptr);

public:

	static UserManager& Instance();

	/**
	 * @brief AddUser add new user to system
	 * @param username
	 * @param password
	 * @param displayname Full name of new user
	 * @param isAdmin If true user is given admin rights, notifications etc
	 * @return true upon success false otherwise
	 */
	bool AddUser(const string& username, const string& password, const string& displayname, bool isAdmin);

	/**
	 * @brief DeleteUser delete user from system
	 * @param user
	 * @return true upon success
	 */
	bool DeleteUser(const string& user);

	virtual ~UserManager();
private:
	OPI::SecopPtr authdb;
};

} // Namespace KGP
#endif // USERMANAGER_H
