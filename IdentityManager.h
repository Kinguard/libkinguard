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
 * Copyright 2018 OpenProducts 237 AB
 */

class IdentityManager: public BaseManager
{
private:
	IdentityManager();
public:

	static IdentityManager& Instance();


	/**
	 * @brief GetHostname Get system hostname
	 * @return hostname upon success, emtpy string on failure
	 */
	string GetHostname();


	/**
	 * @brief GetDomain Get system domain name
	 * @return hostname upon success, emtpy string on failure
	 */
	string GetDomain();

	/**
	 * @brief SetFqdn Set system hostname and domain
	 * @param name
	 * @param domain
	 * @return true upon success
	 */
	bool SetFqdn(const string& name, const string &domain);

	/**
	 * @brief GetFqdn get complete fqdn
	 * @return <hostname, domain> or <"",""">
	 */
	tuple<string, string> GetFqdn(void);

	/**
	 * @brief GetFqdn Get system FQDN as string
	 * @return string FQDN upon success, emtpy string on failure
	 */
	string GetFqdnAsString();


	/**
	 * @brief CreateCertificate, create self signed certificate for set
	 *        hostname and domain.
	 * @param Force generation of DNS Provider / Self signed certificate
	 * @param Set the certificate type to sysconfig
	 * @return true upon success
	 */
	bool CreateCertificate(bool forceProvider, string certtype);

	/**
	 * @brief CreateCertificate, create self signed certificate for set
	 *        hostname and domain.
	 * @return true upon success
	 */
	bool CreateCertificate();

	/**
	 * @brief WriteCustomCertificat, write a custom supplied key/cert to use.
	 * @param key
	 * @param certificate
	 * @return true upon success
	 */
	tuple<bool,string>  WriteCustomCertificate(string key, string cert);

	/**
	 * @brief DnsNameAvailable Check availability of dns-name
	 * @param hostname
	 * @param domain
	 * @return true if available
	 */
	bool DnsNameAvailable(const string& hostname, const string& domain);

	/**
	 * @brief DnsDomainAvailable Check availability of dns-domain
	 * @param domain
	 * @return true if available
	 */
	bool DnsDomainAvailable(const string& domain);

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
	 * @brief EnableDnsProvider enable dns provider
	 * @param provider
	 * @return true upon success
	 */
	bool EnableDnsProvider(const string& provider);


	/**
	 * @brief DNSAvailableDomains list available dns-domains
	 * @return list with domains
	 */
	list<string> DnsAvailableDomains(void);

	/**
	 * @brief SetDNSProvider Set and Enable DNS Provider
	 * @param provider
	 * @return true if possible to set provider
	 */
	bool SetDNSProvider(string provider);

	/**
	 * @brief EnableDNSProvider Enable currently configured DNS Provider
	 * @return true if successful
	 */
	bool EnableDNS();

	/**
	 * @brief DisableDNS Disable currently configured DNS Provider
	 * @param
	 * @return true if successful
	 */
	bool DisableDNS();

	/**
	 * @brief Generate and register auth keys in secop
	 * @return true if successful
	 */
	bool RegisterKeys();

	/**
	 * @brief Upload auth keys to provider backend
	 * @param provider for the backend to use
	 * @param mpwd password to use to sign the keys
	 * @return true if successful
	 */
	tuple<bool,string> UploadKeys(string unitid, string mpwd);

	/**
	 * @brief CleanUp clean up environment if needed
	 *        should be called when Mgr not needed anymore
	 */
	void CleanUp();

	virtual ~IdentityManager();
private:

	bool GetCertificate(const string& fqdn, const string& provider);
	bool GetSignedCert(const string& fqdn);
	bool writeCertificate(string cert, string CustomCertLocation);

	bool CheckUnitID();
	bool OPLogin();
	bool UploadDnsKey(string unitid, string token);

	string unitid;
	string hostname;
	string domain;
	string token;
	ThreadPtr signerthread; // Used by letsencrypt signer thread
	char tmpfilename[128];
};


} // Namespace KGP

#endif // IDENTITYMANAGER_H
