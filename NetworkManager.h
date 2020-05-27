#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include "BaseManager.h"

#include <memory>
#include <json/json.h>

#include <libopi/NetworkConfig.h>

namespace KGP
{

/**
 * @brief The NetworkManager class
 *
 * The NetworkManager is a thin wrapper above the different
 * Network config classes in libopi. Then manager makes sure the
 * correct configurator is used on the platform
 *
 */

class NetworkManager : public BaseManager
{
private:
	NetworkManager();
public:

	static NetworkManager& Instance();

	/**
	 * @brief GetConfiguration fetches configuration for a network device
	 *        on the systenm
	 * @param interface, string naming device to fetch info from i.e. "eth0"
	 *
	 * @return A json object with current configuration
	 *			"addressing" => "static" or "dhcp"
	 *          "options" => "address" => "IP"
	 *                       "netmask" => "netmask"
	 *                       "gateway" => "gateway"
	 *                       "dns" => ["list", "with" "dnss"]
	 */
	Json::Value GetConfiguration(const string& interface);

	/**
	 * @brief StaticConfiguration configure interface with static config
	 * @param interface
	 * @param ip
	 * @param netmask
	 * @param gateway
	 * @param dns - list with dnss
	 */
	bool StaticConfiguration(const string& interface,
							 const string& ip,
							 const string& netmask,
							 const string& gateway,
							 const list<string> dns );

	/**
	 * @brief DynamicConfiguration set dynamic configuration on interface
	 * @param interface
	 */
	bool DynamicConfiguration(const string& interface);

	virtual ~NetworkManager() = default;
private:
	shared_ptr<OPI::NetUtils::NetworkConfig> net;
};

} //KGP


#endif // NETWORKMANAGER_H
