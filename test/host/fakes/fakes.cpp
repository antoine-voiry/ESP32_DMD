// Implementations of the host fakes (see Fake.h).

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_log.h>
#include <esp_random.h>
#include <esp_sleep.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace stdfs = std::filesystem;

extern int g_failures;  // test/host/check.h

////////////////////////////////////////////////////////////////////////////////
// Control state

namespace fake {

static unsigned long g_millis = 0;
time_t epoch = 0;
std::string timezone;
int restarts = 0;
int deepSleeps = 0;
static uint32_t g_random = 1;
WiFiState wifi;
bool fsMountFails = false;
static stdfs::path g_root;
std::map<std::string, HttpReply> httpReplies;
std::vector<std::string> httpRequests;
MqttBroker mqtt;
std::map<std::string, std::string> mdnsHosts;
std::string mdnsName;
std::vector<std::string> mdnsServices;
Portal portal;
bool panelBeginFails = false;
static MatrixPanel_I2S_DMA* g_panel = nullptr;
static WebServer* g_server = nullptr;

void setMillis(unsigned long ms) { g_millis = ms; }
void advance(unsigned long ms) { g_millis += ms; }
void seedRandom(uint32_t seed) { g_random = seed ? seed : 1; }

static stdfs::path hostPath(const std::string& path) {
    return g_root / (path.empty() || path[0] != '/' ? path : path.substr(1));
}

void fsReset() {
    std::error_code ec;
    if (!g_root.empty()) stdfs::remove_all(g_root, ec);
    std::string tmpl = (stdfs::temp_directory_path() / "dmd-fs-XXXXXX").string();
    g_root = mkdtemp(&tmpl[0]);
    fsMountFails = false;
}

void writeFile(const std::string& path, const std::string& content) {
    const stdfs::path p = hostPath(path);
    stdfs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << content;
}

std::string readFile(const std::string& path) {
    std::ifstream in(hostPath(path), std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool fileExists(const std::string& path) { return stdfs::exists(hostPath(path)); }

MatrixPanel_I2S_DMA* panel() { return g_panel; }

int webHandleClientCalls();

struct Task {
    TaskFunction_t fn;
    void* arg;
    uint32_t notifications;
};
static std::vector<Task> g_tasks;
static int g_currentTask = -1;
struct TaskWaits {};

void runTasks() {
    for (size_t i = 0; i < g_tasks.size(); ++i) {
        g_currentTask = static_cast<int>(i);
        try {
            g_tasks[i].fn(g_tasks[i].arg);
        } catch (const TaskWaits&) {
        }
    }
    g_currentTask = -1;
}

void resetAll() {
    struct tm t = {};
    t.tm_year = 2026 - 1900;
    t.tm_mon = 9;
    t.tm_mday = 4;
    t.tm_hour = 12;
    epoch = timegm(&t);
    setenv("TZ", "UTC0", 1);
    tzset();
    timezone.clear();
    g_millis = 1000;
    restarts = 0;
    deepSleeps = 0;
    seedRandom(1);
    wifi = WiFiState();
    fsReset();
    httpReplies.clear();
    httpRequests.clear();
    mqtt = MqttBroker();
    mdnsHosts.clear();
    mdnsName.clear();
    mdnsServices.clear();
    portal = Portal();
    panelBeginFails = false;
    g_tasks.clear();
}

}  // namespace fake

extern "C" time_t __wrap_time(time_t* out) {
    if (out) *out = fake::epoch;
    return fake::epoch;
}

////////////////////////////////////////////////////////////////////////////////
// Arduino core

HardwareSerial Serial;
EspClass ESP;
WiFiClass WiFi;
fs::LittleFSFS LittleFS;
MDNSResponder MDNS;

void EspClass::restart() { ++fake::restarts; }
unsigned long millis() { return fake::g_millis; }
void delay(unsigned long ms) { fake::g_millis += ms; }
float temperatureRead() { return 42.5f; }
uint32_t getCpuFrequencyMhz() { return 240; }

void configTzTime(const char* tz, const char*, const char*, const char*) {
    fake::timezone = tz;
    setenv("TZ", tz, 1);
    tzset();
}

uint32_t esp_random() {
    uint32_t x = fake::g_random;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return fake::g_random = x;
}

void esp_deep_sleep_start() { ++fake::deepSleeps; }

void fake_log(char level, const char* tag, const char* format, ...) {
    static const bool enabled = std::getenv("DMD_TEST_LOG") != nullptr;
    if (!enabled) return;
    std::printf("%c (%s) ", level, tag);
    va_list args;
    va_start(args, format);
    std::vprintf(format, args);
    va_end(args);
    std::printf("\n");
}

bool IPAddress::fromString(const char* s) {
    unsigned a, b, c, d;
    char extra;
    if (std::sscanf(s, "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) != 4 || a > 255 || b > 255 || c > 255 || d > 255) {
        return false;
    }
    _v = (a << 24) | (b << 16) | (c << 8) | d;
    return true;
}

String IPAddress::toString() const {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u", _v >> 24, (_v >> 16) & 255, (_v >> 8) & 255, _v & 255);
    return buf;
}

////////////////////////////////////////////////////////////////////////////////
// Wi-Fi, mDNS, WiFiManager

wl_status_t WiFiClass::status() { return fake::wifi.connected ? WL_CONNECTED : WL_DISCONNECTED; }
IPAddress WiFiClass::localIP() {
    IPAddress ip;
    if (fake::wifi.connected) ip.fromString(fake::wifi.ip.c_str());
    return ip;
}
const char* WiFiClass::getHostname() { return fake::wifi.hostname.c_str(); }
int WiFiClass::RSSI() { return fake::wifi.rssi; }
bool WiFiClass::reconnect() {
    ++fake::wifi.reconnects;
    return true;
}
void WiFiClass::onEvent(void (*handler)(WiFiEvent_t)) {
    fake::wifi.eventHandler = [handler](int e) { handler(static_cast<WiFiEvent_t>(e)); };
}

bool MDNSResponder::begin(const char* hostname) {
    fake::mdnsName = hostname;
    return true;
}
void MDNSResponder::addService(const char* service, const char* proto, uint16_t port) {
    fake::mdnsServices.push_back(std::string(service) + "/" + proto + "/" + std::to_string(port));
}
IPAddress MDNSResponder::queryHost(const char* host, uint32_t) {
    IPAddress ip;
    auto it = fake::mdnsHosts.find(host);
    if (it != fake::mdnsHosts.end()) ip.fromString(it->second.c_str());
    return ip;
}

WiFiManagerParameter::WiFiManagerParameter(const char* id, const char*, const char* defaultValue, int)
    : _id(id), _value(defaultValue) {}

void WiFiManager::setHostname(const char* hostname) { fake::portal.hostname = hostname; }
void WiFiManager::setEnableConfigPortal(bool enable) { _portalEnabled = enable; }
void WiFiManager::setConfigPortalTimeout(unsigned long seconds) { fake::portal.timeoutSec = seconds; }

bool WiFiManager::autoConnect() {
    fake::portal.portalEnabledAtAutoConnect = _portalEnabled;
    if (fake::portal.savedNetworkWorks) {
        fake::wifi.connected = true;
        return true;
    }
    if (_portalEnabled) {
        // What the real library does: an open access point named after the chip.
        return startConfigPortal("ESP32_AP", nullptr);
    }
    return false;
}

bool WiFiManager::startConfigPortal(const char* ssid, const char* password) {
    ++fake::portal.opened;
    fake::portal.ssid = ssid;
    fake::portal.password = password ? password : "";
    if (_ap) _ap(this);
    if (!fake::portal.userConnects) return false;
    for (WiFiManagerParameter* p : _params) {
        auto it = fake::portal.entered.find(p->getID());
        if (it != fake::portal.entered.end()) p->setValue(it->second);
    }
    if (fake::portal.userSaves && _save) _save();
    fake::wifi.connected = true;
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// HTTP client

bool HTTPClient::begin(WiFiClient&, const char* url) {
    _url = url;
    return _url.compare(0, 4, "http") == 0;
}

int HTTPClient::GET() {
    fake::httpRequests.push_back(_url);
    size_t best = 0;
    const fake::HttpReply* reply = nullptr;
    for (const auto& kv : fake::httpReplies) {
        if (_url.compare(0, kv.first.size(), kv.first) == 0 && kv.first.size() >= best) {
            best = kv.first.size();
            reply = &kv.second;
        }
    }
    if (!reply) return -1;  // HTTPC_ERROR_CONNECTION_REFUSED
    _code = reply->code;
    _body = reply->body;
    _chunked = reply->chunked;
    return _code;
}

int HTTPClient::getSize() { return _chunked ? -1 : static_cast<int>(_body.size()); }

int HTTPClient::writeToStream(Stream* stream) {
    size_t sent = 0;
    while (sent < _body.size()) {
        const size_t n = std::min<size_t>(512, _body.size() - sent);
        if (stream->write(reinterpret_cast<const uint8_t*>(_body.data() + sent), n) != n) {
            return HTTPC_ERROR_STREAM_WRITE;
        }
        sent += n;
    }
    return static_cast<int>(sent);
}

////////////////////////////////////////////////////////////////////////////////
// MQTT

PubSubClient& PubSubClient::setServer(const char* host, uint16_t port) {
    fake::mqtt.host = host;
    fake::mqtt.port = port;
    return *this;
}
PubSubClient& PubSubClient::setCallback(Callback callback) {
    _callback = std::move(callback);
    return *this;
}
bool PubSubClient::setBufferSize(uint16_t size) {
    _bufferSize = size;
    fake::mqtt.bufferSize = size;
    return true;
}
bool PubSubClient::connect(const char* clientId) {
    ++fake::mqtt.connectAttempts;
    fake::mqtt.clientId = clientId;
    fake::mqtt.connected = fake::mqtt.acceptConnections;
    return fake::mqtt.connected;
}
bool PubSubClient::connected() { return fake::mqtt.connected; }
int PubSubClient::state() { return fake::mqtt.connected ? 0 : -2; }
bool PubSubClient::subscribe(const char* topic) {
    fake::mqtt.subscriptions.push_back(topic);
    return fake::mqtt.connected;
}
bool PubSubClient::publish(const char* topic, const char* payload) {
    // The packet (header, topic, payload) must fit the client buffer.
    if (!fake::mqtt.connected || std::strlen(topic) + std::strlen(payload) + 7 > _bufferSize) return false;
    fake::mqtt.published.emplace_back(topic, payload);
    return true;
}
bool PubSubClient::loop() {
    if (!fake::mqtt.connected) return false;
    std::vector<std::pair<std::string, std::string>> incoming;
    incoming.swap(fake::mqtt.incoming);
    for (auto& m : incoming) {
        if (m.first.size() + m.second.size() + 7 > _bufferSize) continue;  // dropped, as the real client does
        std::vector<char> topic(m.first.begin(), m.first.end());
        topic.push_back('\0');
        std::vector<uint8_t> payload(m.second.begin(), m.second.end());
        if (_callback) _callback(topic.data(), payload.data(), static_cast<unsigned>(payload.size()));
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// FreeRTOS

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn, const char*, uint32_t, void* arg, UBaseType_t,
                                   TaskHandle_t* handle, BaseType_t) {
    fake::g_tasks.push_back({fn, arg, 0});
    if (handle) *handle = reinterpret_cast<TaskHandle_t>(fake::g_tasks.size());
    return pdPASS;
}

uint32_t ulTaskNotifyTake(BaseType_t clearOnExit, TickType_t) {
    fake::Task& task = fake::g_tasks.at(static_cast<size_t>(fake::g_currentTask));
    if (task.notifications == 0) throw fake::TaskWaits();
    const uint32_t value = task.notifications;
    task.notifications = clearOnExit ? 0 : value - 1;
    return value;
}

BaseType_t xTaskNotifyGive(TaskHandle_t handle) {
    const size_t i = reinterpret_cast<size_t>(handle);
    if (i >= 1 && i <= fake::g_tasks.size()) ++fake::g_tasks[i - 1].notifications;
    return pdPASS;
}

SemaphoreHandle_t xSemaphoreCreateMutex() {
    static int dummy;
    return reinterpret_cast<SemaphoreHandle_t>(&dummy);
}

////////////////////////////////////////////////////////////////////////////////
// HUB75 panel

MatrixPanel_I2S_DMA::MatrixPanel_I2S_DMA(const HUB75_I2S_CFG& cfg)
    : width(cfg.mx_width * cfg.chain_length), height(cfg.mx_height), config(cfg) {
    back.assign(static_cast<size_t>(width) * height, 0);
    shown = back;
    fake::g_panel = this;
}

MatrixPanel_I2S_DMA::~MatrixPanel_I2S_DMA() {
    if (fake::g_panel == this) fake::g_panel = nullptr;
}

bool MatrixPanel_I2S_DMA::begin() { return !fake::panelBeginFails; }

void MatrixPanel_I2S_DMA::fillScreen(uint16_t color) { std::fill(back.begin(), back.end(), color); }

void MatrixPanel_I2S_DMA::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x >= 0 && y >= 0 && x < width && y < height) back[static_cast<size_t>(y) * width + x] = color;
}

void MatrixPanel_I2S_DMA::flipDMABuffer() {
    shown = back;
    ++flips;
}

int MatrixPanel_I2S_DMA::litPixels() const {
    return static_cast<int>(std::count_if(shown.begin(), shown.end(), [](uint16_t p) { return p != 0; }));
}

////////////////////////////////////////////////////////////////////////////////
// LittleFS

struct fs::File::State {
    std::string path;
    bool dir = false;
    std::FILE* fp = nullptr;
    std::vector<std::string> entries;
    size_t next = 0;
    ~State() {
        if (fp) std::fclose(fp);
    }
};

size_t fs::File::write(const uint8_t* buffer, size_t size) {
    return _state && _state->fp ? std::fwrite(buffer, 1, size, _state->fp) : 0;
}

size_t fs::File::read(uint8_t* buffer, size_t size) {
    return _state && _state->fp ? std::fread(buffer, 1, size, _state->fp) : 0;
}

int fs::File::read() {
    uint8_t c;
    return read(&c, 1) == 1 ? c : -1;
}

int fs::File::peek() {
    if (!_state || !_state->fp) return -1;
    const int c = std::fgetc(_state->fp);
    if (c != EOF) std::ungetc(c, _state->fp);
    return c == EOF ? -1 : c;
}

int fs::File::available() { return static_cast<int>(size() - position()); }

bool fs::File::seek(uint32_t position) {
    return _state && _state->fp && std::fseek(_state->fp, static_cast<long>(position), SEEK_SET) == 0;
}

size_t fs::File::position() const {
    return _state && _state->fp ? static_cast<size_t>(std::ftell(_state->fp)) : 0;
}

size_t fs::File::size() const {
    if (!_state || _state->dir) return 0;
    std::fflush(_state->fp);
    std::error_code ec;
    const auto n = stdfs::file_size(fake::hostPath(_state->path), ec);
    return ec ? 0 : static_cast<size_t>(n);
}

void fs::File::close() { _state.reset(); }
bool fs::File::isDirectory() const { return _state && _state->dir; }
const char* fs::File::path() const { return _state ? _state->path.c_str() : ""; }

const char* fs::File::name() const {
    if (!_state) return "";
    const size_t slash = _state->path.rfind('/');
    return _state->path.c_str() + (slash == std::string::npos ? 0 : slash + 1);
}

fs::File fs::File::openNextFile() {
    if (!_state || !_state->dir || _state->next >= _state->entries.size()) return File();
    return LittleFS.open(_state->entries[_state->next++].c_str(), "r");
}

fs::File fs::FS::open(const char* path, const char* mode) {
    File f;
    if (!path || path[0] != '/') return f;
    const stdfs::path p = fake::hostPath(path);
    std::error_code ec;
    auto state = std::make_shared<File::State>();
    state->path = path;
    if (stdfs::is_directory(p, ec)) {
        if (mode[0] != 'r') return f;
        state->dir = true;
        for (const auto& entry : stdfs::directory_iterator(p, ec)) {
            std::string child = std::string(path) + (state->path == "/" ? "" : "/") + entry.path().filename().string();
            state->entries.push_back(child);
        }
        std::sort(state->entries.begin(), state->entries.end());
    } else {
        const char* m = mode[0] == 'w' ? "wb" : (mode[0] == 'a' ? "ab" : "rb");
        state->fp = std::fopen(p.c_str(), m);
        if (!state->fp) return f;
    }
    f._state = state;
    return f;
}

bool fs::FS::exists(const char* path) { return path && path[0] == '/' && stdfs::exists(fake::hostPath(path)); }

bool fs::FS::remove(const char* path) {
    std::error_code ec;
    return exists(path) && !stdfs::is_directory(fake::hostPath(path)) && stdfs::remove(fake::hostPath(path), ec);
}

bool fs::FS::mkdir(const char* path) {
    std::error_code ec;
    return stdfs::create_directory(fake::hostPath(path), ec) || stdfs::is_directory(fake::hostPath(path));
}

bool fs::FS::rmdir(const char* path) {
    std::error_code ec;
    return stdfs::remove(fake::hostPath(path), ec);
}

bool fs::LittleFSFS::begin(bool) { return !fake::fsMountFails; }
size_t fs::LittleFSFS::totalBytes() { return 896 * 1024; }

size_t fs::LittleFSFS::usedBytes() {
    size_t used = 0;
    std::error_code ec;
    for (const auto& e : stdfs::recursive_directory_iterator(fake::g_root, ec)) {
        if (e.is_regular_file()) used += static_cast<size_t>(e.file_size());
    }
    return used;
}

////////////////////////////////////////////////////////////////////////////////
// Web server

struct FakeWebAccess {
    static fake::WebResponse response;
};
fake::WebResponse FakeWebAccess::response;

WebServer::WebServer(int) { fake::g_server = this; }
WebServer::~WebServer() {
    if (fake::g_server == this) fake::g_server = nullptr;
}

void WebServer::on(const char* uri, HTTPMethod method, THandlerFunction handler) {
    _routes.push_back({uri, method, std::move(handler), nullptr});
}

void WebServer::on(const char* uri, HTTPMethod method, THandlerFunction handler, THandlerFunction upload) {
    _routes.push_back({uri, method, std::move(handler), std::move(upload)});
}

void WebServer::collectHeaders(const char* keys[], size_t count) {
    for (size_t i = 0; i < count; ++i) _collected.push_back(keys[i]);
}

String WebServer::header(const char* name) const {
    auto it = _headers.find(name);
    return it == _headers.end() ? String() : String(it->second.c_str());
}

bool WebServer::hasArg(const char* name) const {
    return std::any_of(_args.begin(), _args.end(), [&](const auto& a) { return a.first == name; });
}

String WebServer::arg(const char* name) const {
    for (const auto& a : _args) {
        if (a.first == name) return a.second.c_str();
    }
    return String();
}

String WebServer::arg(int i) const { return _args.at(static_cast<size_t>(i)).second.c_str(); }
String WebServer::argName(int i) const { return _args.at(static_cast<size_t>(i)).first.c_str(); }

void WebServer::sendHeader(const char* name, const char* value) { FakeWebAccess::response.headers[name] = value; }

void WebServer::send(int code, const char* contentType, const String& body) {
    fake::WebResponse& r = FakeWebAccess::response;
    r.code = code;
    r.type = contentType ? contentType : "";
    r.body = body.c_str();
    ++r.sends;
}

namespace fake {

int webHandleClientCalls() { return g_server ? g_server->_handleClientCalls : 0; }

WebResponse request(const WebRequest& r) {
    FakeWebAccess::response = WebResponse();
    WebServer* s = g_server;
    if (!s) return FakeWebAccess::response;
    s->_uri = r.uri;
    s->_host = r.host;
    s->_headers.clear();
    // Only collected headers are kept, as in the real server.
    if (!r.origin.empty() && std::find(s->_collected.begin(), s->_collected.end(), "Origin") != s->_collected.end()) {
        s->_headers["Origin"] = r.origin;
    }
    s->_args = r.args;
    const HTTPMethod method = r.post ? HTTP_POST : HTTP_GET;
    for (WebServer::Route& route : s->_routes) {
        if (route.uri != r.uri || (route.method != HTTP_ANY && route.method != method)) continue;
        if (route.upload) {
            for (const Upload& u : r.uploads) {
                HTTPUpload& up = s->_upload;
                up = HTTPUpload();
                up.filename = u.filename.c_str();
                up.status = UPLOAD_FILE_START;
                route.upload();
                size_t sent = 0;
                while (sent < u.content.size()) {
                    const size_t n = std::min<size_t>(HTTP_UPLOAD_BUFLEN, u.content.size() - sent);
                    std::memcpy(up.buf, u.content.data() + sent, n);
                    up.currentSize = n;
                    up.status = UPLOAD_FILE_WRITE;
                    route.upload();
                    sent += n;
                    up.totalSize = sent;
                    if (u.abort) break;
                }
                up.status = u.abort ? UPLOAD_FILE_ABORTED : UPLOAD_FILE_END;
                route.upload();
            }
        }
        route.handler();
        if (FakeWebAccess::response.sends != 1) {
            // A handler must answer exactly once.
            ++g_failures;
            std::printf("FAIL %s answered %d times\n", r.uri.c_str(), FakeWebAccess::response.sends);
        }
        return FakeWebAccess::response;
    }
    if (s->_notFound) s->_notFound();
    return FakeWebAccess::response;
}

}  // namespace fake
