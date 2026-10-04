// MessageHandler (every MQTT command) and AttractController (attract mode, carousel, standalone).

#include "fixtures.h"
#include "fw_test.h"
#include "matrix/Hub75_Matrix.h"
#include "util/AttractController.h"
#include "util/MediaLibrary.h"
#include "util/MessageHandler.h"
#include "util/OnlineService.h"
#include "util/Settings.h"
#include "util/TimeService.h"

namespace {

// The services main.cpp wires together, on a 64x32 panel.
struct Board {
    Hub75_Matrix matrix;
    DMDRenderer renderer{&matrix};
    MediaLibrary media;
    TimeService time;
    OnlineService online;
    AttractController attract{&renderer, &media, &online};
    MessageHandler handler{&renderer, &attract, &time, &media, &online};
    std::vector<std::pair<std::string, std::string>> published;

    Board() {
        media.begin();
        online.begin();
        handler.setPublisher([this](const std::string& t, const std::string& p) {
            published.emplace_back(t, p);
            return true;
        });
    }
    MatrixPanel_I2S_DMA* panel() { return fake::panel(); }
    // Handles a payload and plays what it queued.
    void send(const std::string& message, unsigned long playMs = 20000) {
        handler.handleMessage(message);
        runUntilIdle(renderer, playMs);
    }
};

void addMedia() {
    writeBytes("/gifs/anim.gif", kGif3Frames, sizeof(kGif3Frames));
    writeBytes("/gifs/fun/b.gif", kGif3Frames, sizeof(kGif3Frames));
    writeBytes("/images/Raspy2DMD.png", kPngHalfRed, sizeof(kPngHalfRed));
    writeBytes("/patterns/2.png", kPngHalfRed, sizeof(kPngHalfRed));
    fake::writeFile("/effets.txt", "1|Bravo|Bravo !|anim.gif|\n2|Texte|Juste du texte||\n3|Gif||anim.gif|\n"
                                   "4|Son|||applause.mp3\n5|Rien|||\n6|Absent|x|missing.gif|\n");
}

}  // namespace

static void testTextCommands() {
    freshBoard();
    Board b;
    b.send("msg|Hello");
    CHECK(b.panel()->litPixels() > 0);
    b.send("msg|");  // shown as -Vide-
    b.send("msg|Hold|1");
    b.send("score|T20 - T20 - T20|1");
    b.send("msgmove|Bonjour|left|1");
    b.send("msgmovebcl|Bonjour|up|2|1");
    b.send("msgcolor|Rouge|255;0;0|0;0;0");
    b.send("msgcolor|Rouge|bad|0;0;0");  // defaults
    b.send("testFont|Impact.ttf");
    b.send("fx|plasma|1");
    b.send("fx|nope");
    b.send("msgfx|Hello|rainbow|1");
    b.send("msgfx|Hello|fireworks|1");
    b.send("msgfx||wave|1");
    b.send("msgfx|Hello|nope");
    b.send("sound|x.mp3");
    CHECK(b.panel()->litPixels() > 0);

    // Malformed, unknown, missing arguments: nothing breaks.
    b.send("nopipe");
    b.send("unknownaction|x");
    b.send("msgmove|only one");
    b.send("conf|DMDRenderer");
}

static void testMediaCommands() {
    freshBoard();
    addMedia();
    Board b;
    b.send("gif|anim.gif");
    b.send("gif|missing.gif");
    b.send("gifPath|/Medias/Gifs/anim.gif|1");
    b.send("gifPath|/Medias/Gifs/missing.gif");
    b.send("gifText|anim.gif|Hello|1");
    b.send("gifText||Hello");
    b.send("gifText|anim.gif|");
    b.send("gifText|missing.gif|Hello");
    b.send("img|WELK.OME|1");
    CHECK(std::count(b.panel()->shown.begin(), b.panel()->shown.end(), 0xF800) > 0);
    b.send("img|");
    b.send("img|missing.png");
    b.send("msgimg|Hi|/Medias/Patterns/2.png");
    b.send("msgimg|Hi|missing.png");
    b.send("rand|gif|1");
    b.send("rand|img");
    b.send("rand|other");
    b.send("demo|gif", 60000);
    b.send("demo|img");
    for (const char* id : {"1", "2", "3", "4", "5", "6", "99"}) b.send(std::string("effet|") + id);
    b.send("soundeffet|Hello|anim.gif|x.mp3");
    b.send("excludeFolder|fun|/Medias/Gifs/fun");
    CHECK(fake::readFile("/exclusions.txt").find("/gifs/fun") != std::string::npos);
    b.send("excludeFile|anim.gif|/Medias/Gifs/anim.gif");
    b.send("rand|gif");  // nothing left

    freshBoard();
    Board empty;
    empty.send("rand|gif");
    empty.send("rand|img");
}

static void testClockAndAttractCommands() {
    freshBoard();
    addMedia();
    Board b;
    b.send("time|start");
    CHECK(b.panel()->litPixels() > 0);
    b.send("time|stop");
    CHECK_EQ(b.panel()->litPixels(), 0);
    setSetting("ClockRenderer", "showing_datehours", "0");
    b.handler.handleMessage("time|start");
    CHECK(b.renderer.idle());
    b.send("testPattern|2.png");
    b.send("testPattern|missing.png");

    b.handler.handleMessage("waiter|start");
    CHECK(b.attract.running());
    b.handler.handleMessage("waiter|pause");
    CHECK(!b.attract.running());
    b.handler.handleMessage("waiter|resume");
    CHECK(b.attract.running());
    b.handler.handleMessage("msgcarrou|start");
    CHECK(b.attract.running());
    b.handler.handleMessage("msgcarrou|stop");
    CHECK(!b.attract.running());
}

static void testConfCommands() {
    freshBoard();
    Board b;
    b.send("conf|DMDRenderer|brightness:50");
    CHECK_EQ(b.panel()->brightness, 127);
    CHECK_EQ(ConfigHelper::getInstance().getBrightness(), 50);
    b.send("conf|DMDRenderer|brightness:150");  // refused
    CHECK_EQ(ConfigHelper::getInstance().getBrightness(), 50);
    b.send("conf|DMDRenderer|brightnesshours:10,20|nocolon");
    CHECK_EQ(ConfigHelper::getInstance().getBrightnessHours(), std::string("10,20"));
    b.send("conf|DMDRenderer|center_images:0");
    b.send("conf|ClockRenderer|timezone:Europe/London");
    CHECK_EQ(fake::timezone, std::string("GMT0BST,M3.5.0/1,M10.5.0"));
    b.send("conf|OpenWeatherMap|appid:abc");
    b.send("conf|TextRenderer|defaultfontcolor:255,0,0");
    CHECK_EQ(b.renderer.defaultStyle().fg.r, 255);
    CHECK(fake::readFile("/config.json").find("appid") != std::string::npos);  // saved
    CHECK_EQ(fake::restarts, 0);
    b.send("conf|DMDRenderer|cols:128");
    CHECK_EQ(fake::restarts, 1);  // panel geometry applies at start-up

    b.send("rldconf|");
    b.send("rebt|");
    CHECK_EQ(fake::restarts, 2);
    b.send("shutdwn|");
    CHECK_EQ(fake::deepSleeps, 1);
}

static void testReceipconfAndStandalone() {
    freshBoard();
    Board b;
    b.send("receipconf|");
    CHECK(b.published.empty());  // Running.resptoraspydarts is 0
    setSetting("Running", "resptoraspydarts", "1");
    b.send("receipconf|");
    CHECK(b.published.size() > 50);
    CHECK_EQ(b.published[0].first, std::string("raspydarts/dmd"));
    CHECK_EQ(b.published[0].second, std::string("DMDRenderer:cols:64"));

    CHECK(b.handler.accepts("score|T20"));
    setSetting("Running", "standalone", "1");
    CHECK(!b.handler.accepts("score|T20"));
    CHECK(b.handler.accepts("meteo|"));
    CHECK(!b.handler.accepts("garbage"));
    b.handler.handleMessage("msgmove|ignored|left");  // standalone: not even an interrupt
}

static void testOnlineCommands() {
    freshBoard();
    setSetting("OpenWeatherMap", "appid", "k");
    setSetting("OpenWeatherMap", "zipcode", "75001");
    setSetting("OpenWeatherMap", "countrycode", "FR");
    fake::httpReplies["https://api.openweathermap.org/geo/1.0/zip"] = {
        200, R"({"name":"Paris","lat":48.8592,"lon":2.3417})", false};
    Board b;
    for (const char* cmd : {"meteo|", "meteoPrevi|", "edfJoursTempo|", "perf|"}) {
        b.handler.handleMessage(cmd);
        runFor(b.renderer, 300);
        fake::runTasks();
        runUntilIdle(b.renderer, 30000);
    }
    b.handler.handleMessage("owmzc|");
    runFor(b.renderer, 300);
    fake::runTasks();
    runUntilIdle(b.renderer, 30000);
    CHECK_EQ(ConfigHelper::getInstance().getSetting("OpenWeatherMap", "cityname", ""), std::string("Paris"));
    b.send("fllcn|", 300);
}

static void testAttractController() {
    freshBoard();
    addMedia();
    fake::writeFile("/textes/a/one.txt", "DG|2\nBonjour\n");
    fake::writeFile("/textes/a/two.txt", "A\nAu hasard\n");
    fake::writeFile("/textes/a/plain.txt", "\nSans options\n");
    Board b;
    b.attract.start("Z");  // nothing playable
    CHECK(!b.attract.running());

    // Every show once, then around again.
    b.attract.start("1,2,T,4,M,P,E,S,F");
    CHECK(b.attract.running());
    for (int i = 0; i < 40; ++i) {
        b.attract.loop(millis());
        runFor(b.renderer, 200);
        fake::runTasks();
        runUntilIdle(b.renderer, 30000);
    }
    CHECK(b.attract.running());
    CHECK(b.panel()->flips > 100);

    // The carousel alone, with a file that has no message.
    fake::writeFile("/textes/a/one.txt", "DG|2\n");
    fake::writeFile("/textes/a/two.txt", "GD|x|anim.gif\nAvec fond\n");
    fake::writeFile("/textes/a/plain.txt", "");
    b.attract.start("4");
    for (int i = 0; i < 20; ++i) {
        b.attract.loop(millis());
        runUntilIdle(b.renderer);
    }

    // Nothing at all to play: attract mode stops by itself.
    fake::fsReset();
    b.attract.start("1,2,4");
    b.attract.loop(millis());
    CHECK(!b.attract.running());

    // Idle countdown (Running.attract_mode), and standalone resuming after 5 s.
    setSetting("Running", "attract_mode", "2");
    setSetting("Running", "scrollOrder", "T");
    b.attract.onMessage(millis());
    b.attract.loop(millis());
    CHECK(!b.attract.running());
    fake::advance(2500);
    b.attract.loop(millis());
    CHECK(b.attract.running());
    b.attract.stop();

    setSetting("Running", "attract_mode", "0");
    setSetting("Running", "standalone", "1");
    b.attract.onMessage(millis());
    fake::advance(5500);
    b.attract.loop(millis());
    CHECK(b.attract.running());
}

void testMessages() {
    testTextCommands();
    testMediaCommands();
    testClockAndAttractCommands();
    testConfCommands();
    testReceipconfAndStandalone();
    testOnlineCommands();
    testAttractController();
}
