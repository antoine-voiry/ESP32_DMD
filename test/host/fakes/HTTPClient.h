#ifndef FAKE_HTTP_CLIENT_H
#define FAKE_HTTP_CLIENT_H

// Answers come from fake::httpReplies, matched by URL prefix.

#include <string>

#include "WiFi.h"

#define HTTP_CODE_OK 200
#define HTTPC_ERROR_STREAM_WRITE (-10)

class HTTPClient {
public:
    void setTimeout(uint16_t) {}
    bool begin(WiFiClient& client, const char* url);
    int GET();
    // Content-Length, or -1 for chunked replies.
    int getSize();
    // Bytes written, or a negative error when the stream refuses data.
    int writeToStream(Stream* stream);
    void end() {}

private:
    std::string _url;
    int _code = 0;
    std::string _body;
    bool _chunked = false;
};

#endif
