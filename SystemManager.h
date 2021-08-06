#ifndef SYSTEMMANAGER_H
#define SYSTEMMANAGER_H

#include <memory>
#include <string>
#include <tuple>

#include "BaseManager.h"

using namespace std;

/*
 * Forward declarations for internal usage
 */
class ShellAccess;

/**
 * @brief The SystemManager class
 *
 * This class is responsible for system maintanence etc which
 * does not fit into a specific domain, such as user/backup etc.
 *
 * Subparts should be split out when starting to get to complex
 *
 */

class SystemManager : public KGP::BaseManager
{
private:
	SystemManager();
public:
	static SystemManager& Instance();


	/*
	 *
	 *  Update/upgrade functionality
	 *
	 */


	/**
	 * @brief UpgradeAvailable
	 *
	 * @return bool - true if system upgrade is available
	 *         string - description if upgrade available
	 *
	 * @details Function check for an executable at
	 * /usr/share/opi-updates/kgp-checkupgrade and
	 * executes this if present.
	 *
	 * The executable is expected to output a json
	 * encoded string object containing two keys
	 *
	 * status, bool true if update exists
	 * description, text describing update
	 *
	 */
	tuple<bool,string> UpgradeAvailable(void);


	/**
	 * @brief IsConfigured, tries to determine if system has an
	 *        active configuration. (Says nothing on storage etc)
	 *
	 * @return true if configured
	 */
	bool IsConfigured();

	/**
	 * @brief StartUpgrade, start a detached system upgrade
	 *
	 */
	void StartUpgrade(void);

	/**
	 * @brief StartUpdate, start a routine system update
	 */
	void StartUpdate(void);

	/*
	 *
	 *  Provider functionality
	 *
	 */

	/**
	 * @brief HasProviders
	 * @return true if system have providers installed
	 */
	bool HasProviders();

	/**
	 * @brief Providers
	 * @return list with provider IDs
	 */
	const list<string>& Providers();

	/*
	 *
	 *  Shell access functionality
	 *
	 */

	/**
	 * @brief ShellAccessAvailable
	 * @return true if kgp supports shell access control on this system
	 */
	bool ShellAccessAvailable();

	/**
	 * @brief ShellAccessEnabled
	 * @return true if shell access is enabled, false otherwise
	 */
	bool ShellAccessEnabled();

	/**
	 * @brief ShellAccessEnable, enable shell access if possible
	 * @return true if succesful
	 */
	bool ShellAccessEnable();

	/**
	 * @brief ShellAccessDisable, disable shell access if enabled
	 * @return true if succesful
	 */
	bool ShellAccessDisable();


	virtual ~SystemManager();
private:
	/*
	 * Provider attributes
	 */
	list<string> providers;

	unique_ptr<ShellAccess> shellaccess;
};

#endif // SYSTEMMANAGER_H
