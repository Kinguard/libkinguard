#ifndef IDENTITYMANAGER_H
#define IDENTITYMANAGER_H

#include "BaseManager.h"
#include <libutils/Thread.h>
#include <libutils/ClassTools.h>

#include <list>
#include <memory>
#include <string>

typedef shared_ptr<Utils::Thread> ThreadPtr;


using namespace std;

namespace KGP
{


/**
 * @brief The IdentityManager class
 *
 * The KGP system currently supports two types of identity
 *
 * A traditional hostname and domain assigned to the unit.
 * This will result in the system to be configured to use this name.
 *
 * Further more the system might support registering, currently max 1,
 * name with a DNS aka, DynDNS support.
 *
 * If DNS name is registered it will also be used as the default hostname
 * and domain.
 *
 * Todo here is a generalisation to allow register multiple names
 *
 */

class IdentityManager: public BaseManager
{
private:
	IdentityManager();
public:

	static IdentityManager& Instance();


	/**
	 * @brief SetHostname Set system hostname
	 * @param name
	 * @return true upon success
	 */
	bool SetHostname(const string& name);

	/**
	 * @brief GetHostname get current hostname
	 * @return hostname or empty string
	 */
	string GetHostname(void);

	/**
	 * @brief SetDomain Set domain name for system
	 * @param domain
	 * @return true upon success
	 */
	bool SetDomain(const string& domain);

	/**
	 * @brief GetDomain get current domain
	 * @return domain or empty string
	 */
	string GetDomain(void);

	/**
	 * @brief GetFqdn get complete fqdn
	 * @return <hostname, domain> or <"",""">
	 */
	tuple<string, string> GetFqdn(void);


	/**
	 * @brief CreateCertificate, create self signed certificate for set
	 *        hostname and domain.
	 * @return true upon success
	 */
	bool CreateCertificate();

	/**
	 * @brief DnsNameAvailable Check availability of dns-name
	 * @param hostname
	 * @param domain
	 * @return true if available
	 */
	bool DnsNameAvailable(const string& hostname, const string& domain);

	/**
	 * @brief AddDnsName Register a new DNS entry for this unit
	 *        (Currently only one name allowed this this is
	 *		   effectively update if old name is registererd)
	 * @param hostname
	 * @param domain
	 * @return true if succesfull
	 */
	bool AddDnsName(const string& hostname, const string& domain);

	/**
	 * @brief GetCurrentDNSName get
	 * @return <hostname, domain>
	 */
	tuple<string,string> GetCurrentDnsName(void);

	/**
	 * @brief HasDNSProvider
	 * @return true if there is at least one provider
	 */
	bool HasDnsProvider(void);

	/**
	 * @brief DNSAvailableDomains list available dns-domains
	 * @return list with domains
	 */
	list<string> DnsAvailableDomains(void);


	/**
	 * @brief CleanUp clean up environment if needed
	 *        should be called when Mgr not needed anymore
	 */
	void CleanUp();

	virtual ~IdentityManager();
private:

	bool GetCertificate(const string& fqdn, const string& provider);
	bool GetSignedCert(const string& fqdn);

	bool CheckUnitID();
	bool OPLogin();

	string unitid;
	string token;
	ThreadPtr signerthread; // Used by letsencrypt signer thread
};


} // Namespace OPI

#endif // IDENTITYMANAGER_H
