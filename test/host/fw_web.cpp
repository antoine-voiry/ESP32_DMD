// LocalWebServer: pages, forms, uploads, and the checks that keep other sites out.

#include "fw_test.h"
#include "util/LocalWebServer.h"

namespace {

fake::WebResponse get(const std::string& uri, const std::string& host = "192.168.1.42") {
    fake::WebRequest r;
    r.uri = uri;
    r.host = host;
    return fake::request(r);
}

fake::WebResponse post(const std::string& uri, std::vector<std::pair<std::string, std::string>> args,
                       const std::string& origin = "http://192.168.1.42") {
    fake::WebRequest r;
    r.post = true;
    r.uri = uri;
    r.args = std::move(args);
    r.origin = origin;
    return fake::request(r);
}

bool contains(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }

}  // namespace

static void testDashboard() {
    freshBoard();
    LocalWebServer web;
    web.begin();
    fake::WebResponse r = get("/");
    CHECK_EQ(r.code, 200);
    CHECK_EQ(r.type, std::string("text/html"));
    CHECK(contains(r.body, "Send to the panel"));
    CHECK(!contains(r.body, "Status"));  // no provider

    std::vector<std::string> commands;
    bool accept = true;
    web.setOnCommand([&](const std::string& c) {
        commands.push_back(c);
        return accept;
    });
    web.setStatusProvider([] { return LocalWebServer::Status{{"MQTT", "<b>broker</b>"}}; });
    r = get("/");
    CHECK(contains(r.body, "&lt;b&gt;broker&lt;/b&gt;"));  // escaped

    r = post("/send", {{"cmd", "msg|Hello"}});
    CHECK_EQ(r.code, 303);
    CHECK_EQ(r.headers["Location"], std::string("/?sent=1"));
    CHECK_EQ(commands.size(), 1u);
    accept = false;
    r = post("/send", {{"cmd", "bad"}});
    CHECK_EQ(r.headers["Location"], std::string("/?sent=0"));
    r = post("/send", {});
    CHECK_EQ(r.headers["Location"], std::string("/?sent=0"));
    CHECK_EQ(commands.size(), 2u);

    fake::WebRequest sent;
    sent.args = {{"sent", "1"}};
    CHECK(contains(fake::request(sent).body, "Sent to the panel."));
    sent.args = {{"sent", "0"}};
    CHECK(contains(fake::request(sent).body, "Rejected"));

    CHECK_EQ(get("/nothing").code, 404);
    web.handleClient();
    fake::wifi.connected = false;
    web.handleClient();
    CHECK_EQ(fake::webHandleClientCalls(), 1);
}

static void testRequestChecks() {
    freshBoard();
    LocalWebServer web;
    web.setHostname("dmd-salon");
    int commands = 0;
    web.setOnCommand([&](const std::string&) { return ++commands > 0; });

    // DNS rebinding: a page on another name cannot read or post.
    CHECK_EQ(get("/", "evil.example").code, 403);
    CHECK_EQ(get("/settings", "evil.example:80").code, 403);
    CHECK_EQ(get("/", "dmd-salon").code, 200);
    CHECK_EQ(get("/", "DMD-Salon.local:80").code, 200);
    CHECK_EQ(get("/", "dmd-salon.fritz.box").code, 200);
    CHECK_EQ(get("/", "dmd-salon2.local").code, 403);
    CHECK_EQ(get("/", "192.168.1.42:80").code, 200);
    CHECK_EQ(get("/", "").code, 200);  // HTTP/1.0 client

    // Cross-site posts.
    CHECK_EQ(post("/send", {{"cmd", "rebt|"}}, "http://evil.example").code, 403);
    CHECK_EQ(post("/send", {{"cmd", "rebt|"}}, "null").code, 403);
    CHECK_EQ(post("/send", {{"cmd", "rebt|"}}, "https://192.168.1.42").code, 403);
    CHECK_EQ(post("/settings", {{"Running.standalone", "1"}}, "http://evil.example").code, 403);
    CHECK_EQ(commands, 0);
    CHECK_EQ(fake::restarts, 0);
    CHECK_EQ(post("/send", {{"cmd", "msg|x"}}, "http://dmd-salon.local").code, 303);
    CHECK_EQ(post("/send", {{"cmd", "msg|x"}}, "").code, 303);  // curl, scripts
    CHECK_EQ(commands, 2);

    // Rate limit: 30 posts a minute.
    int limited = 0;
    for (int i = 0; i < 40; ++i) limited += post("/send", {{"cmd", "msg|x"}}).code == 429;
    CHECK_EQ(limited, 12);
    fake::advance(61000);
    CHECK_EQ(post("/send", {{"cmd", "msg|x"}}).code, 303);
}

static void testConfigPage() {
    freshBoard();
    ConfigHelper::getInstance().setMqttUrl("raspy'darts.local");
    LocalWebServer web;
    fake::WebResponse r = get("/config");
    CHECK(contains(r.body, "raspy&#39;darts.local"));

    r = post("/config", {{"mqtt_url", "10.0.0.2"}, {"mqtt_path", "raspydarts/#"}});
    CHECK_EQ(r.code, 400);
    r = post("/config", {{"mqtt_url", std::string(300, 'a')}, {"mqtt_path", "t"}, {"hostname", "h"}});
    CHECK_EQ(r.code, 400);
    CHECK_EQ(fake::restarts, 0);

    r = post("/config", {{"mqtt_url", "10.0.0.2"}, {"mqtt_path", "raspydarts/#"}, {"hostname", "dmd2"},
                         {"injected", "x"}});
    CHECK_EQ(r.code, 200);
    CHECK_EQ(fake::restarts, 1);
    CHECK(ConfigHelper::getInstance().loadConfigFile());
    CHECK_EQ(ConfigHelper::getInstance().getHostname(), std::string("dmd2"));
    CHECK(!contains(fake::readFile("/config.json"), "injected"));
    CHECK(!contains(fake::readFile("/config.json"), "lan_domain"));  // only saved when set

    // An extra LAN domain opens the pages on hostname.<domain>.
    web.setHostname("dmd2");
    fake::WebRequest viaRouter;
    viaRouter.host = "dmd2.station";
    viaRouter.uri = "/config";
    r = fake::request(viaRouter);
    CHECK_EQ(r.code, 403);
    CHECK(contains(r.body, "http://192.168.1.42/config") && contains(r.body, "\"station\""));
    fake::WebRequest viaPublic = viaRouter;  // no hint to trust a public domain
    viaPublic.host = "dmd2.myhome.example.com";
    r = fake::request(viaPublic);
    CHECK_EQ(r.code, 403);
    CHECK(contains(r.body, "http://192.168.1.42/") && !contains(r.body, "LAN domain"));
    r = post("/config", {{"mqtt_url", "10.0.0.2"}, {"mqtt_path", "t"}, {"hostname", "dmd2"}, {"lan_domain", "a b"}});
    CHECK_EQ(r.code, 400);
    r = post("/config", {{"mqtt_url", "10.0.0.2"}, {"mqtt_path", "t"}, {"hostname", "dmd2"}, {"lan_domain", "station"}});
    CHECK_EQ(r.code, 200);
    CHECK(ConfigHelper::getInstance().loadConfigFile());
    CHECK_EQ(ConfigHelper::getInstance().getLanDomain(), std::string("station"));
    CHECK_EQ(fake::request(viaRouter).code, 200);
    CHECK(contains(fake::request(viaRouter).body, "value='station'"));
    r = post("/config", {{"mqtt_url", "10.0.0.2"}, {"mqtt_path", "t"}, {"hostname", "dmd2"}});  // no field: kept
    CHECK(ConfigHelper::getInstance().loadConfigFile());
    CHECK_EQ(ConfigHelper::getInstance().getLanDomain(), std::string("station"));
    r = post("/config", {{"mqtt_url", "10.0.0.2"}, {"mqtt_path", "t"}, {"hostname", "dmd2"}, {"lan_domain", ""}});
    CHECK(ConfigHelper::getInstance().loadConfigFile());
    CHECK(ConfigHelper::getInstance().getLanDomain().empty());

    fake::fsReset();
    fake::writeFile("/config.json/x", "");  // cannot be written
    r = post("/config", {{"mqtt_url", "a"}, {"mqtt_path", "b"}, {"hostname", "c"}});
    CHECK_EQ(r.code, 500);
}

static void testFilesPage() {
    freshBoard();
    fake::writeFile("/config.json", "{}");
    fake::writeFile("/gifs/<x>.gif", "12345");
    LocalWebServer web;
    fake::WebResponse r = get("/files");
    CHECK_EQ(r.code, 200);
    CHECK(contains(r.body, "/gifs/&lt;x&gt;.gif"));
    CHECK(!contains(r.body, "value='/config.json'"));  // no delete button for it
    CHECK(contains(r.body, "KB used"));

    // Uploads.
    fake::WebRequest up;
    up.post = true;
    up.uri = "/upload";
    up.args = {{"dir", "/gifs"}};
    up.origin = "http://192.168.1.42";
    up.uploads = {{"new.gif", std::string(5000, 'g'), false}};
    r = fake::request(up);
    CHECK_EQ(r.code, 303);
    CHECK_EQ(fake::readFile("/gifs/new.gif").size(), 5000u);

    up.uploads = {{"cut.gif", std::string(5000, 'g'), true}};  // connection lost: no answer
    CHECK_EQ(fake::request(up).code, 0);
    CHECK(!fake::fileExists("/gifs/cut.gif"));
    up.uploads = {{"after.gif", "x", false}};  // nothing left over from the dropped request
    CHECK_EQ(fake::request(up).code, 303);

    // Several files: every one is reported, not just the last.
    up.uploads = {{"", "x", false}, {"two.gif", "x", false}};  // no name: refused
    r = fake::request(up);
    CHECK_EQ(r.code, 400);
    CHECK(contains(r.body, "1 of 2"));
    CHECK(fake::fileExists("/gifs/two.gif"));
    // A request that broke after its files leaves nothing behind for the next one.
    up.uploads = {{"", "x", false}};
    up.dropBeforeAnswer = true;
    CHECK_EQ(fake::request(up).code, 0);
    up.dropBeforeAnswer = false;
    up.uploads.clear();  // not even for a POST with no file
    r = fake::request(up);
    CHECK_EQ(r.code, 400);
    CHECK(contains(r.body, "No file"));
    up.uploads = {{"", "x", false}};
    up.dropBeforeAnswer = true;
    CHECK_EQ(fake::request(up).code, 0);
    up.dropBeforeAnswer = false;
    up.uploads = {{"three.gif", "x", false}};
    CHECK_EQ(fake::request(up).code, 303);
    up.uploads.clear();  // a POST without a file does not replay the previous result
    r = fake::request(up);
    CHECK_EQ(r.code, 400);
    CHECK(contains(r.body, "No file"));
    up.origin = "http://evil.example";  // ...and is checked like any other POST
    r = fake::request(up);
    CHECK_EQ(r.code, 403);
    CHECK(contains(r.body, "another web site"));
    up.origin = "http://192.168.1.42";
    // One request with many files counts once against the rate limit.
    for (int i = 0; i < 35; ++i) up.uploads.push_back({"many" + std::to_string(i) + ".gif", "x", false});
    CHECK_EQ(fake::request(up).code, 303);
    CHECK(fake::fileExists("/gifs/many34.gif"));

    up.args = {{"dir", "/"}};
    up.uploads = {{"config.json", "{\"mqtt_url\":\"evil\"}", false}};
    CHECK_EQ(fake::request(up).code, 400);
    CHECK_EQ(fake::readFile("/config.json"), std::string("{}"));

    up.args = {{"dir", "/gifs/../"}};  // traversal in the folder: refused
    up.uploads = {{"x.gif", "x", false}};
    CHECK_EQ(fake::request(up).code, 400);
    up.args = {{"dir", "/gifs"}};  // traversal in the name: kept inside /gifs
    up.uploads = {{"../../config.json", "x", false}};
    CHECK_EQ(fake::request(up).code, 303);
    CHECK(fake::fileExists("/gifs/config.json"));
    CHECK_EQ(fake::readFile("/config.json"), std::string("{}"));

    up.args = {{"dir", "/gifs"}};
    up.origin = "http://evil.example";
    up.uploads = {{"evil.gif", "x", false}};
    r = fake::request(up);
    CHECK_EQ(r.code, 403);
    CHECK_EQ(r.sends, 1);
    CHECK(!fake::fileExists("/gifs/evil.gif"));

    // Deletes.
    CHECK_EQ(post("/delete", {{"path", "/gifs/new.gif"}}).code, 303);
    CHECK(!fake::fileExists("/gifs/new.gif"));
    CHECK_EQ(post("/delete", {{"path", "/config.json"}}).code, 400);
    CHECK_EQ(post("/delete", {{"path", "/gifs/../config.json"}}).code, 400);
    CHECK_EQ(post("/delete", {{"path", "gifs/<x>.gif"}}).code, 400);
    CHECK_EQ(post("/delete", {{"path", "/missing"}}).code, 400);
    CHECK_EQ(post("/delete", {}).code, 400);
    CHECK(fake::fileExists("/config.json"));
}

static void testSettingsPage() {
    freshBoard();
    setSetting("OpenWeatherMap", "cityname", "<script>");
    LocalWebServer web;
    int saved = 0;
    web.setOnSettingsSaved([&] { ++saved; });
    fake::WebResponse r = get("/settings");
    CHECK(contains(r.body, "&lt;script&gt;"));
    CHECK(!contains(r.body, "<script>"));
    CHECK(contains(r.body, "<legend>OpenWeatherMap</legend>"));

    r = post("/settings", {{"TextRenderer.maxcharacter", "22"}, {"nodot", "1"}, {"Bogus.key", "1"}});
    CHECK_EQ(r.code, 303);
    CHECK_EQ(saved, 0);  // nothing changed
    CHECK(!fake::fileExists("/config.json"));

    r = post("/settings", {{"TextRenderer.maxcharacter", "12"}});
    CHECK_EQ(saved, 1);
    CHECK(contains(fake::readFile("/config.json"), "\"TextRenderer.maxcharacter\":\"12\""));

    // Same checks as conf: a brightness the panel cannot use is not stored.
    r = post("/settings", {{"DMDRenderer.brightness", "abc"}});
    CHECK_EQ(r.code, 400);
    CHECK(contains(r.body, "DMDRenderer.brightness"));
    CHECK_EQ(saved, 1);
    CHECK_EQ(ConfigHelper::getInstance().getBrightness(), 90);
    r = post("/settings", {{"DMDRenderer.brightness", "150"}, {"TextRenderer.maxcharacter", "14"}});
    CHECK_EQ(r.code, 200);  // the valid one is still saved
    CHECK(contains(r.body, "Other changes were saved"));
    CHECK_EQ(saved, 2);
    CHECK(contains(fake::readFile("/config.json"), "\"TextRenderer.maxcharacter\":\"14\""));
    r = post("/settings", {{"DMDRenderer.brightness", "40"}});
    CHECK_EQ(saved, 3);
    CHECK_EQ(ConfigHelper::getInstance().getBrightness(), 40);

    r = post("/settings", {{"DMDRenderer.cols", "128"}, {"DMDRenderer.brightness", "x"}});
    CHECK_EQ(r.code, 200);
    CHECK(contains(r.body, "restarting") && contains(r.body, "DMDRenderer.brightness"));
    CHECK_EQ(fake::restarts, 1);
    CHECK_EQ(saved, 3);
}

void testWeb() {
    testDashboard();
    testRequestChecks();
    testConfigPage();
    testFilesPage();
    testSettingsPage();
}
