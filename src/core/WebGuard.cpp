#include "WebGuard.h"

#include "ConfigSchema.h"
#include "PublicTlds.h"

#include <cstring>

namespace dmd {

std::string hostWithoutPort(const std::string& host) {
    std::string h = normaliseKey(host);
    if (!h.empty() && h[0] == '[') {
        const size_t end = h.find(']');
        return end == std::string::npos ? h : h.substr(0, end + 1);
    }
    const size_t colon = h.find(':');
    return colon == std::string::npos ? h : h.substr(0, colon);
}

namespace {

// Domains that only exist on a LAN (mDNS, RFC 8375, ICANN's private-use names, common router
// domains), alone or under a subdomain ("home.lan"). A public domain after the hostname is
// refused: its owner could point it at the board's address.
bool isPrivateSuffix(const std::string& suffix) {
    if (suffix.empty() || suffix[0] == '.' || suffix.find("..") != std::string::npos) return false;
    static const char* const kSuffixes[] = {"local", "lan", "home", "home.arpa", "internal", "intranet",
                                            "corp", "private", "localdomain", "fritz.box", "speedport.ip"};
    for (const char* s : kSuffixes) {
        const std::string tail = s;
        if (suffix == tail) return true;
        if (suffix.size() > tail.size() + 1 && suffix.compare(suffix.size() - tail.size(), tail.size(), tail) == 0 &&
            suffix[suffix.size() - tail.size() - 1] == '.') {
            return true;
        }
    }
    return false;
}

}  // namespace

bool isValidLanDomain(const std::string& domain) {
    // Labels of letters, digits and '-' ("station", "office.corp").
    if (domain.empty() || domain.size() > 253) return false;
    size_t start = 0;
    while (true) {
        const size_t dot = domain.find('.', start);
        const std::string label = domain.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        if (label.empty() || label.size() > 63 || label.front() == '-' || label.back() == '-') return false;
        for (char c : label) {
            const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-';
            if (!ok) return false;
        }
        if (dot == std::string::npos) break;
        start = dot + 1;
    }
    // Its top-level label must not be public: anyone could register "<hostname>.<domain>" under a
    // public one ("example.net", "co.uk") and rebind it to the board. Every two-letter label
    // (country codes) and every IANA TLD is refused. The list is a snapshot: a TLD delegated later
    // passes until tools/gen_public_tlds.py is rerun, so the /config page asks for a private name.
    const std::string tld = normaliseKey(domain.substr(domain.rfind('.') == std::string::npos ? 0 : domain.rfind('.') + 1));
    if (tld.size() <= 2) return false;
    const std::string needle = "|" + tld + "|";
    return std::strstr(kPublicTlds, needle.c_str()) == nullptr;  // no copy of the 10 KB list
}

namespace {

// Lower-case, without the port or a trailing dot ("DMD.Local.:80" -> "dmd.local").
std::string normaliseHost(const std::string& hostHeader) {
    std::string host = hostWithoutPort(hostHeader);
    if (host.size() > 1 && host.back() == '.') host.pop_back();  // "dmd.local." is the same name
    return host;
}

// What follows "<name>." in an already normalised host, or "".
std::string suffixUnder(const std::string& host, const std::string& name) {
    if (name.empty() || host.size() <= name.size() + 1 || host.compare(0, name.size(), name) != 0 ||
        host[name.size()] != '.') {
        return "";
    }
    return host.substr(name.size() + 1);
}

}  // namespace

std::string lanSuffix(const std::string& hostHeader, const std::string& hostname) {
    return suffixUnder(normaliseHost(hostHeader), normaliseKey(hostname));
}

bool hostAllowed(const std::string& hostHeader, const std::string& ip, const std::string& hostname,
                 const std::string& lanDomain) {
    if (hostHeader.empty()) return true;
    const std::string host = normaliseHost(hostHeader);
    if (!ip.empty() && host == ip) return true;
    const std::string name = normaliseKey(hostname);
    if (name.empty()) return false;
    if (host == name) return true;
    const std::string suffix = suffixUnder(host, name);
    if (suffix.empty()) return false;
    return isPrivateSuffix(suffix) || (!lanDomain.empty() && suffix == normaliseKey(lanDomain));
}

bool originAllowed(const std::string& originHeader, const std::string& ip, const std::string& hostname,
                   const std::string& lanDomain) {
    if (originHeader.empty()) return true;
    static const std::string kScheme = "http://";
    if (originHeader.compare(0, kScheme.size(), kScheme) != 0) return false;  // also rejects "null"
    const std::string rest = originHeader.substr(kScheme.size());
    if (rest.empty() || rest.find('/') != std::string::npos) return false;
    return hostAllowed(rest, ip, hostname, lanDomain);
}

}  // namespace dmd
