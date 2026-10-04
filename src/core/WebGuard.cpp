#include "WebGuard.h"

#include "ConfigSchema.h"

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

bool hostAllowed(const std::string& hostHeader, const std::string& ip, const std::string& hostname) {
    if (hostHeader.empty()) return true;
    const std::string host = hostWithoutPort(hostHeader);
    if (!ip.empty() && host == ip) return true;
    const std::string name = normaliseKey(hostname);
    return !name.empty() && (host == name || host.compare(0, name.size() + 1, name + ".") == 0);
}

bool originAllowed(const std::string& originHeader, const std::string& ip, const std::string& hostname) {
    if (originHeader.empty()) return true;
    static const std::string kScheme = "http://";
    if (originHeader.compare(0, kScheme.size(), kScheme) != 0) return false;  // also rejects "null"
    const std::string rest = originHeader.substr(kScheme.size());
    if (rest.empty() || rest.find('/') != std::string::npos) return false;
    return hostAllowed(rest, ip, hostname);
}

}  // namespace dmd
