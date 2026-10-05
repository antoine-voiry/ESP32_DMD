#ifndef FAKE_WEBSERVER_H
#define FAKE_WEBSERVER_H

// Routes requests built by tests (fake::request()) to the handlers the firmware registered.

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Arduino.h"

enum HTTPMethod { HTTP_ANY, HTTP_GET, HTTP_POST };
enum HTTPUploadStatus { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };

#define HTTP_UPLOAD_BUFLEN 1436

struct HTTPUpload {
    HTTPUploadStatus status = UPLOAD_FILE_START;
    String filename;
    size_t totalSize = 0;
    size_t currentSize = 0;
    uint8_t buf[HTTP_UPLOAD_BUFLEN];
};

class WebServer {
public:
    using THandlerFunction = std::function<void()>;

    explicit WebServer(int port = 80);
    ~WebServer();

    void on(const char* uri, HTTPMethod method, THandlerFunction handler);
    void on(const char* uri, HTTPMethod method, THandlerFunction handler, THandlerFunction upload);
    void onNotFound(THandlerFunction handler) { _notFound = std::move(handler); }
    void collectHeaders(const char* keys[], size_t count);
    void begin() { _begun = true; }
    void handleClient() { ++_handleClientCalls; }

    String uri() const { return _uri.c_str(); }
    String hostHeader() const { return _host.c_str(); }
    String header(const char* name) const;
    bool hasArg(const char* name) const;
    String arg(const char* name) const;
    String arg(int i) const;
    String argName(int i) const;
    int args() const { return static_cast<int>(_args.size()); }
    HTTPUpload& upload() { return _upload; }

    void sendHeader(const char* name, const char* value);
    void send(int code, const char* contentType = nullptr, const String& body = String());

    // State of the request being handled, set by fake::request().
    struct Route {
        std::string uri;
        HTTPMethod method;
        THandlerFunction handler;
        THandlerFunction upload;
    };
    std::vector<Route> _routes;
    THandlerFunction _notFound;
    std::vector<std::string> _collected;
    bool _begun = false;
    int _handleClientCalls = 0;
    // Current request.
    std::string _uri;
    std::string _host;
    std::map<std::string, std::string> _headers;
    std::vector<std::pair<std::string, std::string>> _args;
    HTTPUpload _upload;
};

#endif
