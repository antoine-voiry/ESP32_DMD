#ifndef DMD_CORE_WEB_GUARD_H
#define DMD_CORE_WEB_GUARD_H

// Checks that keep other web sites out of the board's web pages:
// - Host must be the board's IP address or its hostname, alone or followed by a private
//   network suffix ("dmd", "dmd.local", "dmd.fritz.box"), so a DNS-rebinding page cannot read
//   or drive them ("dmd.attacker.example" is refused: any public domain can be rebound). Another
//   LAN domain can be allowed on the /config page;
// - a browser's Origin on POST requests must name the board too (cross-site request forgery).

#include <string>

namespace dmd {

// "Host" header value without its port, lower-cased ("[::1]:80" -> "[::1]").
std::string hostWithoutPort(const std::string& host);

// Empty Host (HTTP/1.0 clients) is allowed; browsers always send one. lanDomain is the user's own
// LAN domain when it is not one of the built-in private ones ("station" allows "dmd.station"),
// already checked with isValidLanDomain() (the caller validates it once, not per request).
bool hostAllowed(const std::string& hostHeader, const std::string& ip, const std::string& hostname,
                 const std::string& lanDomain = "");

// No Origin (non-browser clients) is allowed; "null" or another site is not.
bool originAllowed(const std::string& originHeader, const std::string& ip, const std::string& hostname,
                   const std::string& lanDomain = "");

// A domain the user may set as lanDomain: labels of letters, digits and '-', under a top-level
// label that is not public (not two letters, not in core/PublicTlds.h): "station", "office.corp".
bool isValidLanDomain(const std::string& domain);

// What follows "<hostname>." in a Host header ("dmd.station:80" -> "station"), or "" when the
// Host is not under the hostname.
std::string lanSuffix(const std::string& hostHeader, const std::string& hostname);

}  // namespace dmd

#endif
