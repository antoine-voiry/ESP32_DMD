// Firmware services: config.json, storage, media library, settings, MQTT, Wi-Fi setup, NTP brightness.

#include <WiFi.h>

#include "fw_test.h"
#include "matrix/Hub75_Matrix.h"
#include "util/MQTTHelper.h"
#include "util/MediaLibrary.h"
#include "util/Settings.h"
#include "util/Storage.h"
#include "util/TimeService.h"
#include "util/WifiManagerHelper.h"

static void testStorageMountFailure() {
    // Must run first: storageBegin() remembers a successful mount.
    fake::resetAll();
    fake::fsMountFails = true;
    CHECK(!storageBegin());
    CHECK(!ConfigHelper::getInstance().loadConfigFile());
    fake::fsMountFails = false;
    CHECK(storageBegin());
    CHECK(storageBegin());
}

static void testConfigHelper() {
    freshBoard();
    ConfigHelper& config = ConfigHelper::getInstance();
    CHECK(!config.loadConfigFile());  // no file yet

    fake::writeFile("/config.json", "{not json");
    CHECK(!config.loadConfigFile());

    // Values of the wrong type must neither crash nor count as configured.
    fake::writeFile("/config.json", R"({"mqtt_url":12,"mqtt_path":null,"hostname":["x"],"settings":{"A.b":3}})");
    CHECK(!config.loadConfigFile());
    CHECK(config.getMqttUrl().empty());
    CHECK_EQ(config.getSetting("A", "b", "fallback"), std::string("fallback"));

    fake::writeFile("/config.json", R"({"mqtt_url":"raspydarts.local","mqtt_path":"raspydarts/#","hostname":"dmd",)"
                                    R"("brightness":40,"brightnesshours":"1,2,3",)"
                                    R"("settings":{"TextRenderer.MaxCharacter":"12","Running.standalone":"1","odd":"x"}})");
    CHECK(config.loadConfigFile());
    CHECK_EQ(config.getMqttUrl(), std::string("raspydarts.local"));
    CHECK_EQ(config.getMqttPath(), std::string("raspydarts/#"));
    CHECK_EQ(config.getHostname(), std::string("dmd"));
    CHECK_EQ(config.getBrightness(), 40);
    CHECK_EQ(config.getSetting("DMDRenderer", "Brightness", ""), std::string("40"));
    CHECK_EQ(config.getSetting("DMDRenderer", "brightnesshours", "x"), std::string("1,2,3"));
    CHECK_EQ(config.getSettingInt("TextRenderer", "maxcharacter", 0), 12L);
    CHECK_EQ(config.getSettingInt("Running", "STANDALONE", 0), 1L);
    CHECK_EQ(config.getSetting("odd", "", "f"), std::string("f"));

    config.setSetting("OpenWeatherMap", "cityname", "Paris");
    config.setSetting("DMDRenderer", "brightness", "70");
    config.setSetting("DMDRenderer", "brightnesshours", "");
    config.setSetting("Running", "attract_mode", "abc");
    CHECK_EQ(config.getBrightness(), 70);
    CHECK_EQ(config.getSetting("DMDRenderer", "brightnesshours", "def"), std::string("def"));
    CHECK_EQ(config.getSettingInt("Running", "attract_mode", 5), 5L);  // not a number
    CHECK_EQ(config.getSettingInt("Running", "missing", 7), 7L);
    config.setMqttUrl("10.0.0.2");
    CHECK(config.saveConfigFile());

    // Saved values come back, and nothing is written to the serial port (the file holds the API key).
    CHECK(config.loadConfigFile());
    CHECK_EQ(config.getMqttUrl(), std::string("10.0.0.2"));
    CHECK_EQ(config.getSetting("OpenWeatherMap", "cityname", ""), std::string("Paris"));
    CHECK_EQ(config.getBrightness(), 70);

    // Unwritable file (a folder in its place).
    fake::fsReset();
    fake::writeFile("/config.json/x", "");
    CHECK(!config.saveConfigFile());
}

static void testStorage() {
    freshBoard();
    CHECK(!storageExists(""));
    CHECK(!storageExists("/nope"));
    CHECK(storageWriteText("/a/b/c.txt", "hello"));
    CHECK(storageExists("/a/b/c.txt"));
    CHECK_EQ(storageReadText("/a/b/c.txt"), std::string("hello"));
    CHECK_EQ(storageReadText("/a/b/c.txt", 3), std::string("hel"));
    CHECK_EQ(storageReadText("/a"), std::string(""));  // a folder
    CHECK_EQ(storageReadText("/missing"), std::string(""));
    fake::writeFile("/a/d.txt", "x");
    std::vector<std::string> all = storageListFiles("/a");
    CHECK_EQ(all.size(), 2u);
    CHECK_EQ(storageListFiles("/a", false).size(), 1u);
    CHECK_EQ(storageListFiles("/missing").size(), 0u);
    CHECK_EQ(storageListFiles("/a/d.txt").size(), 0u);  // not a folder
    const std::vector<std::string> dirs = storageListDirs("/a");
    CHECK_EQ(dirs.size(), 1u);
    CHECK_EQ(dirs[0], std::string("/a/b"));
    CHECK(!storageWriteText("relative.txt", "x"));
    std::string big(20000, 'z');
    CHECK(storageWriteText("/big.txt", big));
    CHECK_EQ(storageReadText("/big.txt").size(), 16384u);  // default cap
}

static void testMediaLibrary() {
    freshBoard();
    fake::writeFile("/gifs/b.gif", "x");
    fake::writeFile("/gifs/fun/a.gif", "x");
    fake::writeFile("/gifs/notes.txt", "x");
    fake::writeFile("/images/i.png", "x");
    fake::writeFile("/scores/T20/s.gif", "x");
    fake::writeFile("/effets.txt", "# id|name|text|gif|sound\n12|Bravo|Bravo !|b.gif|\n");
    MediaLibrary media;
    media.begin();
    CHECK_EQ(media.resolve("b.gif", dmd::media::kGifs), std::string("/gifs/b.gif"));
    CHECK_EQ(media.resolve("/Medias/Gifs/fun/a.gif", dmd::media::kGifs), std::string("/gifs/fun/a.gif"));
    CHECK_EQ(media.resolve("nope.gif", dmd::media::kGifs), std::string(""));
    CHECK(dmd::isGifFile(media.randomFile(dmd::media::kGifs, ".gif")));
    CHECK_EQ(media.randomFile("/images", ".gif"), std::string(""));
    CHECK_EQ(media.randomFileIn("/scores/T20", ".gif"), std::string("/scores/T20/s.gif"));
    CHECK_EQ(media.randomFile(dmd::media::kTextes, ""), std::string(""));
    std::vector<std::string> gifs = media.allGifs();
    CHECK_EQ(gifs.size(), 2u);
    CHECK_EQ(gifs[0], std::string("/gifs/b.gif"));

    CHECK(media.toggleExclusion(true, "fun", "/Medias/Gifs/fun"));
    CHECK_EQ(media.allGifs().size(), 1u);
    CHECK(fake::readFile("/exclusions.txt").find("/gifs/fun") != std::string::npos);
    MediaLibrary reloaded;
    reloaded.begin();
    CHECK_EQ(reloaded.allGifs().size(), 1u);
    CHECK(!media.toggleExclusion(true, "fun", "/Medias/Gifs/fun"));
    CHECK_EQ(media.allGifs().size(), 2u);

    dmd::Effect effect;
    CHECK(media.findEffect("12", effect));
    CHECK_EQ(effect.text, std::string("Bravo !"));
    CHECK(!media.findEffect("13", effect));
}

static void testSettings() {
    freshBoard();
    ClockSpec clock = settings::clockSpec();
    CHECK_EQ(clock.mode, 2);
    CHECK_EQ(clock.dateMs, 2000u);
    CHECK_EQ(clock.pattern, std::string("/patterns/OldGame.png"));
    CHECK_EQ(settings::timezone(), std::string("CET-1CEST,M3.5.0,M10.5.0/3"));
    TextStyle style = settings::textStyle();
    CHECK_EQ(style.maxCharsPerLine, 22u);
    CHECK(settings::centerImages());
    CHECK_EQ(settings::scrollOrder(), std::string("1,T"));
    CHECK(!settings::standalone());
    CHECK(settings::showWebAddress());
    CHECK_EQ(settings::attractAfterMs(), 0u);
    CHECK_EQ(settings::owmConfig().appid, std::string("0"));

    setSetting("ClockRenderer", "clockBackgroundImage", "");
    setSetting("ClockRenderer", "defaultfontcolor_clock", "10,20,30");
    setSetting("TextRenderer", "maxcharacter", "-4");
    setSetting("TextRenderer", "defaultfontcolor", "bad");
    setSetting("DMDRenderer", "center_images", "0");
    setSetting("Running", "standalone", "1");
    setSetting("Running", "default", "0");
    setSetting("Running", "attract_mode", "30");
    setSetting("OpenWeatherMap", "callevery", "5");
    clock = settings::clockSpec();
    CHECK_EQ(clock.pattern, std::string("/patterns/OldGame.png"));  // empty = default
    CHECK_EQ(clock.fg.g, 20);
    style = settings::textStyle();
    CHECK_EQ(style.maxCharsPerLine, 22u);
    CHECK_EQ(style.fg.b, 255);  // default blue
    CHECK(!settings::centerImages());
    CHECK(settings::standalone());
    CHECK(!settings::showWebAddress());
    CHECK_EQ(settings::attractAfterMs(), 30000u);
    CHECK_EQ(settings::owmConfig().callEveryMin, 5);
}

static void testMqtt() {
    freshBoard();
    fake::mqtt.acceptConnections = false;
    MQTTHelper mqtt("10.0.0.2", "dmd-salon", "raspydarts/#");
    CHECK_EQ(fake::mqtt.host, std::string("10.0.0.2"));
    CHECK_EQ(fake::mqtt.port, 1883);
    CHECK_EQ(fake::mqtt.bufferSize, 1024);
    CHECK(!mqtt.loop(1000));
    CHECK_EQ(fake::mqtt.connectAttempts, 1);
    CHECK(!mqtt.loop(3000));  // waits kRetryMs between attempts
    CHECK_EQ(fake::mqtt.connectAttempts, 1);
    CHECK(!mqtt.publish("t", "x"));

    fake::mqtt.acceptConnections = true;
    CHECK(mqtt.loop(7000));
    CHECK_EQ(fake::mqtt.connectAttempts, 2);
    CHECK_EQ(fake::mqtt.clientId, std::string("dmd-salon"));  // unique per board, not a shared constant
    CHECK_EQ(fake::mqtt.subscriptions.size(), 1u);

    fake::mqtt.incoming = {{"raspydarts/dmd", "msg|a"}, {"raspydarts/dmd", "msg|b"}, {"raspydarts/dmd", "msg|c"}};
    CHECK(mqtt.loop(7010));
    std::vector<std::string> got = mqtt.takeMessages(2);
    CHECK_EQ(got.size(), 2u);
    CHECK_EQ(got[0], std::string("msg|a"));  // arrival order
    CHECK_EQ(mqtt.takeMessages().size(), 1u);
    CHECK(mqtt.takeMessages().empty());

    // A flood keeps only the newest kMaxQueued messages.
    for (int i = 0; i < 40; ++i) fake::mqtt.incoming.emplace_back("t", "msg|" + std::to_string(i));
    mqtt.loop(7020);
    CHECK_EQ(mqtt.droppedMessages(), 8u);
    got = mqtt.takeMessages(100);
    CHECK_EQ(got.size(), MQTTHelper::kMaxQueued);
    CHECK_EQ(got.front(), std::string("msg|8"));

    CHECK(mqtt.publish("raspydarts/dmd", "Running:standalone:0"));
    CHECK(!mqtt.publish("t", std::string(2000, 'x')));  // larger than the buffer
    CHECK_EQ(fake::mqtt.published.size(), 1u);

    fake::mqtt.connected = false;  // broker went away
    CHECK(!mqtt.loop(7030));
    CHECK_EQ(fake::mqtt.connectAttempts, 2);  // retried kRetryMs after the last attempt
    CHECK(mqtt.loop(12030));
}

static void testWifiSetup() {
    // Saved network: no portal, and never the library's unprotected fallback portal.
    freshBoard();
    ConfigHelper::getInstance().setHostname("dmd-salon");
    int notices = 0;
    WifiManagerHelper().connect(false, [&](const std::string&, const std::string&) { ++notices; });
    CHECK_EQ(fake::portal.opened, 0);
    CHECK(!fake::portal.portalEnabledAtAutoConnect);
    CHECK_EQ(fake::portal.hostname, std::string("dmd-salon"));
    CHECK_EQ(notices, 0);

    // Saved network unreachable: our portal, with a password shown through the notice.
    freshBoard();
    fake::portal.savedNetworkWorks = false;
    fake::portal.entered = {{"mqtt_url", "raspydarts.local"}, {"mqtt_path", "raspydarts/#"}, {"hostname", "dmd2"}};
    std::string shownSsid, shownPassword;
    WifiManagerHelper().connect(false, [&](const std::string& s, const std::string& p) {
        shownSsid = s;
        shownPassword = p;
    });
    CHECK_EQ(fake::portal.opened, 1);
    CHECK_EQ(fake::portal.ssid, std::string("DMD_CONFIG_WIFI"));
    CHECK_EQ(fake::portal.password.size(), 10u);
    CHECK_EQ(shownSsid, fake::portal.ssid);
    CHECK_EQ(shownPassword, fake::portal.password);
    CHECK(fake::portal.timeoutSec > 0);
    CHECK_EQ(ConfigHelper::getInstance().getHostname(), std::string("dmd2"));
    CHECK(ConfigHelper::getInstance().loadConfigFile());  // saved to config.json

    // Passwords differ from one portal to the next.
    CHECK(WifiManagerHelper::newPortalPassword() != WifiManagerHelper::newPortalPassword());

    // Forced portal, user leaves without saving: nothing changes.
    freshBoard();
    fake::portal.userSaves = false;
    WifiManagerHelper().connect(true, nullptr);
    CHECK_EQ(fake::portal.opened, 1);
    CHECK(!fake::fileExists("/config.json"));

    // Portal times out: restart.
    freshBoard();
    fake::portal.userConnects = false;
    WifiManagerHelper().connect(true, nullptr);
    CHECK_EQ(fake::restarts, 1);

    // Wi-Fi events: reconnect when the connection drops.
    fake::wifi.eventHandler(ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    fake::wifi.eventHandler(ARDUINO_EVENT_WIFI_STA_GOT_IP);
    fake::wifi.eventHandler(ARDUINO_EVENT_OTHER);
    CHECK_EQ(fake::wifi.reconnects, 1);
}

static void testTimeService() {
    freshBoard();
    Hub75_Matrix matrix;
    DMDRenderer renderer(&matrix);
    TimeService time;
    time.begin(settings::timezone());
    CHECK_EQ(fake::timezone, std::string("CET-1CEST,M3.5.0,M10.5.0/3"));
    CHECK(time.synced());

    ConfigHelper::getInstance().setBrightness(50);
    time.loop(millis(), renderer);  // no brightnesshours: the fixed brightness stays
    CHECK_EQ(fake::panel()->brightness, 230);

    std::string hours;
    for (int h = 0; h < 24; ++h) hours += (h ? "," : "") + std::to_string(h == 14 ? 20 : 80);
    ConfigHelper::getInstance().setBrightnessHours(hours);
    time.invalidate();
    time.loop(millis(), renderer);  // 12:00 UTC = 14:00 in Paris
    CHECK_EQ(fake::panel()->brightness, 20 * 255 / 100);

    fake::epoch += 3600;
    time.loop(millis() + 1000, renderer);  // checked every 10 s only
    CHECK_EQ(fake::panel()->brightness, 20 * 255 / 100);
    time.loop(millis() + 20000, renderer);
    CHECK_EQ(fake::panel()->brightness, 80 * 255 / 100);

    fake::epoch = 1000;  // before NTP sync
    TimeService unsynced;
    CHECK(!unsynced.synced());
    unsynced.loop(millis(), renderer);
}

static void testMatrix() {
    freshBoard();
    {
        Hub75_Matrix matrix(64, 32, 2);
        MatrixPanel_I2S_DMA* p = fake::panel();
        CHECK_EQ(matrix.width(), 128);
        CHECK_EQ(matrix.height(), 32);
        CHECK(p->config.double_buff);
        CHECK_EQ(p->config.driver, HUB75_I2S_CFG::FM6126A);
        CHECK_EQ(p->flips, 1);  // cleared at start-up
        matrix.drawPixel(3, 4, matrix.color(255, 255, 255));
        CHECK_EQ(p->shownAt(3, 4), 0);
        matrix.present();
        CHECK_EQ(p->shownAt(3, 4), 0xFFFF);
        matrix.fillScreen(matrix.color(255, 0, 0));
        matrix.clearScreen();
        CHECK_EQ(p->litPixels(), 0);
        matrix.setBrightnessPercent(150);
        CHECK_EQ(p->brightness, 255);
        matrix.setBrightnessPercent(-3);
        CHECK_EQ(p->brightness, 0);
    }
    CHECK(fake::panel() == nullptr);
    fake::panelBeginFails = true;
    Hub75_Matrix tall(64, 64, 1);  // also logs that 64-row panels need E_PIN
    CHECK_EQ(tall.height(), 64);
}

void testServices() {
    testStorageMountFailure();
    testConfigHelper();
    testStorage();
    testMediaLibrary();
    testSettings();
    testMqtt();
    testWifiSetup();
    testTimeService();
    testMatrix();
}
