#ifndef SYSTEMMANAGER_H
#define SYSTEMMANAGER_H

#include <string>
#include <tuple>

#include "BaseManager.h"

using namespace std;

/**
 * @brief The SystemManager class
 *
 * This class is responsible for system maintanence etc which
 * does not fit into a specific domain, such as user/backup etc.
 *
 */

class SystemManager : public KGP::BaseManager
{
private:
	SystemManager();
public:
	static SystemManager& Instance();

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


	virtual ~SystemManager();
private:
	list<string> providers;
};

#endif // SYSTEMMANAGER_H
