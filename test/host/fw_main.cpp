// main.cpp end to end: first boot through the setup portal, MQTT messages to the panel, the web
// dashboard, MQTT outages. setup() keeps its objects in statics, so it runs once per process.

#include "fixtures.h"
#include "fw_test.h"

void setup();
void loop();

namespace {

void loops(int n) {
    for (int i = 0; i < n; ++i) loop();  // each loop() delays 1 ms
}

bool contains(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }

}  // namespace

void testMain() {
    freshBoard();
    writeBytes("/images/Raspy2DMD.png", kPngHalfRed, sizeof(kPngHalfRed));
    fake::wifi.hostname = "dmd-salon";
    fake::portal.savedNetworkWorks = false;
    fake::portal.entered = {{"mqtt_url", "raspydarts.local"}, {"mqtt_path", "raspydarts/#"}, {"hostname", "dmd-salon"}};
    fake::mdnsHosts["raspydarts"] = "10.0.0.5";

    setup();
    MatrixPanel_I2S_DMA* p = fake::panel();
    CHECK(p != nullptr);
    CHECK_EQ(fake::portal.opened, 1);   // no config.json yet: the portal opened...
    CHECK(p->litPixels() > 0);          // ...with its password on the panel
    CHECK_EQ(fake::mdnsName, std::string("dmd-salon"));
    CHECK_EQ(fake::mdnsServices.size(), 1u);
    CHECK_EQ(fake::mqtt.host, std::string("10.0.0.5"));  // .local resolved with mDNS
    CHECK_EQ(fake::mqtt.clientId.size(), 0u);            // not connected before loop()

    loops(10);
    CHECK(fake::mqtt.connected);
    CHECK_EQ(fake::mqtt.clientId, std::string("dmd-salon"));
    CHECK_EQ(fake::mqtt.subscriptions[0], std::string("raspydarts/#"));
    loops(8000);  // welcome text, web address, logo

    // MQTT messages reach the panel; invalid ones are dropped.
    fake::mqtt.incoming = {{"raspydarts/dmd", "garbage"}, {"raspydarts/dmd", "msg|Hello"}};
    loops(50);
    CHECK(p->litPixels() > 0);
    const int flips = p->flips;
    fake::mqtt.incoming = {{"raspydarts/dmd", "fx|plasma|1"}};
    loops(500);
    CHECK(p->flips > flips + 10);

    // The dashboard: status, and commands sent through the same path as MQTT.
    fake::WebRequest home;
    home.host = "dmd-salon.local";
    fake::WebResponse r = fake::request(home);
    CHECK_EQ(r.code, 200);
    CHECK(contains(r.body, "10.0.0.5 (connected)"));
    CHECK(contains(r.body, "64 x 32"));
    fake::WebRequest send;
    send.post = true;
    send.uri = "/send";
    send.args = {{"cmd", "msg|From the web"}};
    CHECK_EQ(fake::request(send).headers["Location"], std::string("/?sent=1"));
    send.args = {{"cmd", "nonsense|x"}};
    CHECK_EQ(fake::request(send).headers["Location"], std::string("/?sent=0"));
    fake::WebRequest settings;
    settings.post = true;
    settings.uri = "/settings";
    settings.args = {{"TextRenderer.defaultfontcolor", "0,255,0"}, {"Running.standalone", "0"}};
    CHECK_EQ(fake::request(settings).code, 303);

    // Standalone mode refuses game commands, from the web as from MQTT.
    ConfigHelper::getInstance().setSetting("Running", "standalone", "1");
    send.args = {{"cmd", "score|T20"}};
    CHECK_EQ(fake::request(send).headers["Location"], std::string("/?sent=0"));
    ConfigHelper::getInstance().setSetting("Running", "standalone", "0");

    // Broker lost: the panel says so once, then reconnects.
    fake::mqtt.connected = false;
    fake::mqtt.acceptConnections = false;
    loops(20);
    CHECK(p->litPixels() > 0);
    fake::mqtt.acceptConnections = true;
    fake::advance(6000);
    loops(5);
    CHECK(fake::mqtt.connected);
    CHECK_EQ(fake::restarts, 0);
}
