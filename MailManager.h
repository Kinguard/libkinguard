#ifndef MAILMANAGER_H
#define MAILMANAGER_H

#include "BaseManager.h"

#include <string>
#include <list>
#include <map>

using namespace std;

namespace KGP
{

/**
 * @brief The MailManager class the central mail management class of KGP
 *
 * KGP have four different entities regarding email management
 *
 * Public, external addresses that tells the system to which addresses it
 * should expect to accept incomming emails from.
 *
 * Local internal mail, each user gets a local address in the form
 * username@localdomain which is used to receive, local, mail directly
 * when no obvious external address is available.
 *
 * This address is currently only used for mail originating for admin/root.
 *
 * Remote mail accounts where the system fetches mail from other providers and
 * deliver them to local users.
 *
 * Finally we have aliases which is currently only used to group receivers for
 * email originating from internal services to admin users.
 *
 */

class MailManager : public BaseManager
{
private:
	MailManager();
public:
	static MailManager& Instance();

	// Global management

	/**
	 * @brief DeleteUser completely remove local user from mailsystem
	 * @param user
	 */
	void DeleteUser(const string& user);

	/**
	 * @brief AddToAdmin add user to receivelist of admin mail
	 * @param user
	 * @return true upon success
	 */
	bool AddToAdmin(const string& user);

	/**
	 * @brief RemoveFromAdmin remove user as recipient for admin mail
	 * @param user
	 * @return true upon success
	 */
	bool RemoveFromAdmin(const string& user);

	// Domain management
	/**
	 * @brief GetDomains Retrieve domains that we accept mail for
	 * @return list of domains
	 */
	list<string> GetDomains();

	/**
	 * @brief AddDomain add new domain to handle mail for
	 * @param domain
	 */
	void AddDomain(const string& domain);

	/**
	 * @brief DeleteDomain Remove domain from domains that we accept mail for
	 * @param domain domain to remove.
	 */
	void DeleteDomain(const string& domain);

	// External Address management
	/**
	 * @brief SetAddress update or create an address in this domain
	 * @param domain domain to use
	 * @param address local part of address
	 * @param user user that should receive email to above address
	 */
	void SetAddress(const string& domain, const string& address, const string& user);

	/**
	 * @brief DeleteAddress delete address in domain
	 *
	 * Delete address from domain and possibly deleting the domain as well. If last
	 * address in domain.
	 *
	 * @param domain domain to operate on
	 * @param address address to delete
	 */
	void DeleteAddress(const string& domain, const string& address);


	/**
	 * @brief DeleteAddresses delete all addresses for user
	 * @param user
	 */
	void DeleteAddresses(const string& user);

	/**
	 * @brief GetAddresses get addresses for specific domain
	 * @param domain domain to retrieve adresses for
	 * @return list with tuples localpart,username
	 */
	list<tuple<string,string>> GetAddresses(const string& domain);

	/**
	 * @brief ChangeDomain replace one domain with a new one
	 *
	 * Moves all users in one domain to a new, to, one deleting the
	 * old, from, domain.
	 *
	 * @param from currently existing domain on system
	 * @param to none existant, new, domain in system
	 */
	void ChangeDomain(const string& from, const string& to);

	/**
	 * @brief GetAddress gets user for specific domain and local part
	 * @param domain domain to use
	 * @param address adress to retrieve
	 * @return tuple localpart of address and destination username
	 */
	tuple<string,string> GetAddress(const string& domain, const string& address);

	bool hasDomain(const string& domain);
	bool hasAddress(const string& domain, const string& address);

	// Internal, local address management
	bool SetLocalAddress(const string& user);
	bool RemoveLocalAddress(const string& user);

	// Remote addresses
	/**
	 * @brief GetRemoteAccounts retrieve remote accounts for user
	 * @param user user to fetch accounts for
	 * @return list of maps with key value account info
	 */
	list<map<string,string>> GetRemoteAccounts(const string& user);

	/**
	 * @brief GetRemoteAccount get info on remote account
	 * @param hostname provider hostnama
	 * @param identity remote identity
	 * @return map with account data key, value
	 */
	map<string, string> GetRemoteAccount( const string& hostname, const string& identity);

	/**
	 * @brief AddRemoteAccount
	 * @param email email address
	 * @param host remote host
	 * @param identity identity at remote host
	 * @param password password at remote host
	 * @param user local user
	 * @param ssl use ssl or not
	 */
	void AddRemoteAccount(const string& email, const string& host, const string& identity, const string& password, const string& user, bool ssl);

	/**
	 * @brief UpdateRemoteAccount
	 * @param email email address
	 * @param host remote host
	 * @param identity identity at remote host
	 * @param password password at remote host
	 * @param user local user
	 * @param ssl use ssl or not
	 */
	void UpdateRemoteAccount(const string& email, const string& host, const string& identity, const string& password, const string& user, bool ssl);

	/**
	 * @brief DeleteRemoteAccount
	 * @param hostname remote hostname
	 * @param identity remote identity
	 */
	void DeleteRemoteAccount(const string& hostname, const string& identity);

	// Aliases management - MailAliasFile
	/**
	 * @brief GetAliases get all aliases
	 * @return
	 */
	list<string> GetAliases( );

	/**
	 * @brief GetAliasUsers get all users of alias
	 * @param alias
	 * @return
	 */
	list<string> GetAliasUsers( const string& alias );

	/**
	 * @brief AddUserAlias add user to alias
	 * @param alias
	 * @param user
	 * @return
	 */
	bool AddUserAlias(const string& alias, const string& user);

	/**
	 * @brief RemoveUserAlias remove user from specific alias
	 * @param alias
	 * @param user
	 * @return
	 */
	bool RemoveUserAlias(const string& alias, const string& user);

	/**
	 * @brief RemoveUserAliases remove all aliases for user
	 * @param user
	 * @return
	 */
	bool RemoveUserAliases(const string& user);


	// Misc functions

	/**
	 * @brief Synchronize synchronize settings with mailserver
	 * (Restarts/reloads mailsystem)
	 */
	bool Synchronize();

	/**
	 * @brief SetupEnvironment
	 *
	 * Somewhat of a hack currently needed by opi-backend
	 * Make sure we have all files we need, right permissions etc.
	 *
	 */
	static void SetupEnvironment();

	virtual ~MailManager();
private:
	bool fetchmailupdated;
	bool postfixupdated;
};
} // Namespace KGP

#endif // MAILMANAGER_H
