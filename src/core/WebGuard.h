#ifndef DMD_CORE_WEB_GUARD_H
#define DMD_CORE_WEB_GUARD_H

// Checks that keep other web sites out of the board's web pages:
// - Host must be the board's IP address or its hostname, alone or followed by a domain
//   ("dmd", "dmd.local", "dmd.fritz.box"), so a DNS-rebinding page cannot read or drive them;
// - a browser's Origin on POST requests must name the board too (cross-site request forgery).

#include <string>

namespace dmd {

// "Host" header value without its port, lower-cased ("[::1]:80" -> "[::1]").
std::string hostWithoutPort(const std::string& host);

// Empty Host (HTTP/1.0 clients) is allowed; browsers always send one.
bool hostAllowed(const std::string& hostHeader, const std::string& ip, const std::string& hostname);

// No Origin (non-browser clients) is allowed; "null" or another site is not.
bool originAllowed(const std::string& originHeader, const std::string& ip, const std::string& hostname);

}  // namespace dmd

#endif
