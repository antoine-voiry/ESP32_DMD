// Host-side unit tests for src/core (no Arduino needed). Run: make -C test/host
#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "core/Attract.h"
#include "core/Clock.h"
#include "core/Fx.h"
#include "core/Media.h"
#include "core/Motion.h"
#include "core/Protocol.h"
#include "core/SceneRunner.h"
#include "core/SpecialMoves.h"
#include "core/TextUtil.h"

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                        \
    } while (0)

#define CHECK_EQ(a, b)                                                           \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!((a) == (b))) {                                                     \
            ++g_failures;                                                        \
            std::printf("FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b);   \
        }                                                                        \
    } while (0)

using namespace dmd;

static void testParseCommand() {
    Command c;
    CHECK(parseCommand("msg|Hello|3", c));
    CHECK_EQ(c.action, std::string("msg"));
    CHECK_EQ(c.args.size(), 2u);
    CHECK_EQ(c.args[0], std::string("Hello"));
    CHECK_EQ(c.args[1], std::string("3"));

    CHECK(parseCommand("msg|", c));
    CHECK_EQ(c.args.size(), 1u);
    CHECK_EQ(c.args[0], std::string(""));

    CHECK(parseCommand("conf|DMDRenderer|brightness:50", c));
    CHECK_EQ(c.args[0], std::string("DMDRenderer"));
    CHECK_EQ(c.args[1], std::string("brightness:50"));

    CHECK(!parseCommand("msg", c));
}

static void testFilter() {
    CHECK(isAcceptedPayload("msg|Hello"));
    CHECK(isAcceptedPayload("rebt|"));
    CHECK(!isAcceptedPayload("rebt"));           // no '|' -> rejected like the original
    CHECK(!isAcceptedPayload("unknown|x"));
    CHECK(isAcceptedPayload("score|S20 - T20 - X"));
    CHECK(isAcceptedPayload("score|sb - db - t1"));  // case-insensitive
    CHECK(!isAcceptedPayload("score|S21 - T20 - X"));
    CHECK(!isAcceptedPayload("score|S20-T20-X"));    // wrong separator
    CHECK(!isAcceptedPayload("score|S05 - T20 - X"));
    CHECK(!isAcceptedPayload("score|"));
    CHECK_EQ(minArgs("msgmovebcl"), 3u);
    CHECK_EQ(minArgs("msgcolor"), 3u);
    CHECK_EQ(minArgs("rebt"), 0u);
    CHECK_EQ(parseIntArg("3", 0), 3);
    CHECK_EQ(parseIntArg("x", 7), 7);
    CHECK_EQ(parseIntArg("", 7), 7);
}

static void testSpecialMoves() {
    auto sm = [](const char* s) { return findSpecialMove(splitScore(s)); };
    CHECK_EQ(sm("T20 - T20 - T20"), std::string("MAXIMUM_TON_80"));
    CHECK_EQ(sm("DB - DB - DB"), std::string("BLACK_HAT_THREE_IN_THE_BLACK"));
    CHECK_EQ(sm("SB - SB - SB"), std::string("RED_HAT"));
    CHECK_EQ(sm("SB - DB - SB"), std::string("HAT_TRICK"));
    CHECK_EQ(sm("S20 - S1 - S5"), std::string("BREAKFAST"));
    CHECK_EQ(sm("T5 - T20 - T1"), std::string("CHAMPAGNE_BREAKFAST"));
    CHECK_EQ(sm("S12 - S20 - S5"), std::string("NOT_OLD"));
    CHECK_EQ(sm("S20 - S20 - S20"), std::string("THREE_IN_A_BED"));  // before STEADY (60)
    CHECK_EQ(sm("S1 - S2 - S3"), std::string("CIRCLE_IT"));
    CHECK_EQ(sm("T20 - T20 - S20"), std::string("LOW_TON"));          // 140
    CHECK_EQ(sm("T20 - T20 - T19"), std::string("HIGH_TON"));         // 177
    CHECK_EQ(sm("S10 - S11 - S1"), std::string("DINKY_DOO"));         // 22
    CHECK_EQ(sm("S10 - S11 - S2"), std::string(""));                  // 23
    CHECK_EQ(findSpecialMove({"T20", "T20"}), std::string(""));       // needs 3 darts
}

static void testRgb() {
    Rgb c;
    CHECK(parseRgb("0,0,255", c));
    CHECK(c.r == 0 && c.g == 0 && c.b == 255);
    CHECK(parseRgb("255;128;7", c));
    CHECK(c.r == 255 && c.g == 128 && c.b == 7);
    CHECK(parseRgb("300;-1;5", c));
    CHECK(c.r == 255 && c.g == 0 && c.b == 5);
    CHECK(!parseRgb("1,2", c));
    CHECK(!parseRgb("a,b,c", c));
}

static void testAscii() {
    CHECK_EQ(toDisplayAscii("Fl\xC3\xA9" "chette"), std::string("Flechette"));      // é
    CHECK_EQ(toDisplayAscii("C\xC5\x93ur \xC3\xA0 100\xE2\x82\xAC"), std::string("Coeur a 100EUR"));
    CHECK_EQ(toDisplayAscii("a\nb"), std::string("a b"));
    CHECK_EQ(toDisplayAscii("\xF0\x9F\x8E\xAF"), std::string("?"));  // emoji
}

static void testWrap() {
    auto mono = [](const std::string& s) { return static_cast<int>(s.size()) * 6; };
    auto lines = wrapText("Hello  big world", 64, 22, mono);  // 10 chars per line
    CHECK_EQ(lines.size(), 2u);
    CHECK_EQ(lines[0], std::string("Hello big"));
    CHECK_EQ(lines[1], std::string("world"));

    lines = wrapText("abcdefghijklmnop", 30, 22, mono);  // 5 chars per line, long word broken
    CHECK_EQ(lines.size(), 4u);
    CHECK_EQ(lines[0], std::string("abcde"));
    CHECK_EQ(lines[3], std::string("p"));

    lines = wrapText("aa bb cc", 1000, 5, mono);  // character cap
    CHECK_EQ(lines.size(), 2u);
    CHECK_EQ(lines[0], std::string("aa bb"));
}

static void testTimeline() {
    Timeline left(Motion::Left, 100, 32, 64, 32);
    CHECK_EQ(left.size(), 164u);
    CHECK_EQ(left.at(0).x, 64);
    CHECK_EQ(left.at(163).x, -99);
    CHECK_EQ(left.at(0).delayMs, 10);

    Timeline right(Motion::Right, 100, 32, 64, 32, 2);
    CHECK_EQ(right.size(), 328u);
    CHECK_EQ(right.at(0).x, -100);
    CHECK_EQ(right.at(164).x, -100);  // second iteration restarts

    Timeline up(Motion::Up, 64, 32, 64, 32);
    CHECK_EQ(up.size(), 64u);
    CHECK_EQ(up.at(0).y, 32);
    CHECK_EQ(up.at(63).y, -31);

    Timeline down(Motion::Down, 64, 32, 64, 32);
    CHECK_EQ(down.at(0).y, -32);
    CHECK_EQ(down.at(63).y, 31);

    Timeline rot(Motion::Rotate, 64, 32, 64, 32);
    CHECK_EQ(rot.size(), 9u);
    CHECK_EQ(rot.at(0).angle, 360);
    CHECK_EQ(rot.at(8).angle, 0);

    Timeline flip(Motion::Flip, 64, 32, 64, 32);
    CHECK_EQ(flip.size(), static_cast<size_t>(1 + 32 + 31 + 32 + 31 + 1));
    CHECK_EQ(flip.at(1).h, 32);   // starts shrinking from full height
    CHECK_EQ(flip.at(32).h, 1);
    CHECK(flip.at(33).flipY);     // unfolds mirrored
    Frame last = flip.at(flip.size() - 1);
    CHECK(!last.flipY && last.h == 32 && last.y == 0);

    Timeline none(Motion::None, 64, 32, 64, 32, 5);
    CHECK_EQ(none.size(), 1u);
    CHECK_EQ(parseMotion("twirl") == Motion::Twirl, true);
    CHECK_EQ(parseMotion("sideways") == Motion::None, true);
}

// Scene that animates for a fixed number of ticks and records what happened.
struct FakeScene : Scene {
    FakeScene(std::vector<std::string>& log, std::string name, int frames)
        : log(log), name(std::move(name)), frames(frames) {}
    void start(uint32_t) override { log.push_back(name + ":start"); }
    bool tick(uint32_t) override { return --frames > 0; }
    void abort() override { log.push_back(name + ":abort"); }
    std::vector<std::string>& log;
    std::string name;
    int frames;
};

static void testSceneRunner() {
    std::vector<std::string> log;
    SceneRunner r;
    CHECK(r.idle(0));

    // Long animation interrupted by a new message.
    r.enqueue(std::make_unique<FakeScene>(log, "scroll", 100));
    r.update(0);
    CHECK(r.animating());
    r.interrupt();
    r.enqueue(std::make_unique<FakeScene>(log, "msg", 1), 3000);
    r.update(10);
    CHECK_EQ(log.size(), 3u);
    CHECK_EQ(log[1], std::string("scroll:abort"));
    CHECK_EQ(log[2], std::string("msg:start"));

    // "msg|...|3": the hold is not interrupted, the next message waits.
    r.interrupt();
    r.enqueue(std::make_unique<FakeScene>(log, "next", 1));
    r.update(1000);
    CHECK_EQ(log.size(), 3u);
    CHECK(!r.idle(1000));
    r.update(3010);
    CHECK_EQ(log.size(), 4u);
    CHECK_EQ(log[3], std::string("next:start"));
    CHECK(r.idle(3010));

    // FIFO order.
    log.clear();
    r.enqueue(std::make_unique<FakeScene>(log, "a", 2));
    r.enqueue(std::make_unique<FakeScene>(log, "b", 1));
    r.update(4000);
    CHECK_EQ(log.size(), 1u);
    r.update(4010);
    CHECK_EQ(log.size(), 2u);
    CHECK_EQ(log[1], std::string("b:start"));

    // millis() wrap-around during a hold.
    SceneRunner w;
    log.clear();
    w.enqueue(std::make_unique<FakeScene>(log, "x", 1), 100);
    w.enqueue(std::make_unique<FakeScene>(log, "y", 1));
    w.update(0xFFFFFFF0u);
    w.update(0x10u);  // only 32 ms later
    CHECK_EQ(log.size(), 1u);
    w.update(0x60u);
    CHECK_EQ(log.size(), 2u);
}

static void testFx() {
    FxBackground bg;
    CHECK(parseFxBackground("fireworks", bg) && bg == FxBackground::Fireworks);
    CHECK(!parseFxBackground("rainbow", bg));
    FxText tf;
    CHECK(parseFxText("typewriter", tf) && tf == FxText::Typewriter);
    CHECK(!parseFxText("plasma", tf));
    CHECK(isAcceptedPayload("fx|plasma"));
    CHECK(isAcceptedPayload("msgfx|Hello|rainbow|3"));
    CHECK_EQ(minArgs("msgfx"), 2u);

    Rgb red = hsv(0);
    CHECK(red.r == 255 && red.g == 0 && red.b == 0);
    Rgb gray = hsv(100, 0, 200);
    CHECK(gray.r == 200 && gray.g == 200 && gray.b == 200);
    CHECK_EQ(sin8(0), 128);
    CHECK_EQ(sin8(64), 255);
    CHECK_EQ(sin8(192), 0);
    Rgb half = scale(Rgb{200, 100, 0}, 128);
    CHECK(half.r == 100 && half.g == 50 && half.b == 0);

    CHECK_EQ(typewriterColumns(0, 18, 64), 0);
    CHECK_EQ(typewriterColumns(180, 18, 64), 10);
    CHECK_EQ(typewriterColumns(100000, 18, 64), 64);
    for (int x = 0; x < 64; ++x) {
        int o = waveOffset(x, 1234, 2);
        CHECK(o >= -2 && o <= 2);
    }

    Rng a(42), b(42);
    for (int i = 0; i < 100; ++i) {
        CHECK_EQ(a.next(), b.next());
        int r = a.range(-3, 3);
        b.range(-3, 3);
        CHECK(r >= -3 && r <= 3);
    }
    CHECK(Rng(0).next() != 0);  // a zero seed would get stuck at 0
}

static void testFireworks() {
    Fireworks fw(64, 32, 7);
    bool sawRocket = false, sawSpark = false;
    size_t maxCount = 0;
    for (int t = 0; t < 20000; t += 33) {
        fw.step(33);
        for (const auto& p : fw.particles()) {
            sawRocket |= p.rocket;
            sawSpark |= !p.rocket;
            CHECK(p.y <= 34.0f);  // offscreen particles are removed
        }
        maxCount = std::max(maxCount, fw.particles().size());
    }
    CHECK(sawRocket);
    CHECK(sawSpark);
    CHECK(maxCount <= Fireworks::kMaxParticles);

    Fireworks::Particle p{};
    p.maxLifeMs = 1000;
    p.lifeMs = 500;
    CHECK_EQ(Fireworks::level(p), 127);
}

static void testStarsAndRain() {
    Starfield sf(64, 32, 40, 9);
    for (int i = 0; i < 500; ++i) {
        sf.step(33);
        for (const auto& s : sf.stars()) {
            CHECK(s.x >= 0.0f && s.x < 64.0f && s.y >= 0 && s.y < 32);
        }
    }
    MatrixRain rain(64, 32, 11);
    bool lit = false;
    for (int i = 0; i < 300; ++i) {
        rain.step(33);
        for (int x = 0; x < 64; ++x) {
            for (int y = 0; y < 32; ++y) {
                lit |= rain.level(x, y) > 0;
            }
        }
    }
    CHECK(lit);
    CHECK_EQ(rain.level(-1, 0), 0);
    CHECK_EQ(rain.level(64, 0), 0);
}

static void testClockFormat() {
    DateTime dt;
    dt.year = 2026; dt.month = 10; dt.day = 3; dt.hour = 9; dt.minute = 5; dt.second = 7; dt.weekday = 6;
    // Raspy2DMD defaults: format_date '%d %b %Y', format_hours '%H:%M:%S', fr_FR.
    CHECK_EQ(formatDateTime("%d %b %Y", dt), std::string("03 oct. 2026"));
    CHECK_EQ(formatDateTime("%H:%M:%S", dt), std::string("09:05:07"));
    CHECK_EQ(formatDateTime("%-H:%M", dt), std::string("9:05"));
    CHECK_EQ(formatDateTime("%A %-d %B", dt), std::string("samedi 3 octobre"));
    CHECK_EQ(formatDateTime("%a %d %b", dt, "en"), std::string("Sat 03 Oct"));
    CHECK_EQ(formatDateTime("%I:%M %p", dt, "en"), std::string("09:05 AM"));
    dt.hour = 0;
    CHECK_EQ(formatDateTime("%I %p", dt), std::string("12 AM"));
    dt.month = 2;
    CHECK_EQ(toDisplayAscii(formatDateTime("%b", dt)), std::string("fevr."));
    CHECK_EQ(formatDateTime("100%% %q", dt), std::string("100% %q"));
    CHECK_EQ(languageFromLocale("en_GB"), std::string("en"));
    CHECK_EQ(languageFromLocale("fr_FR"), std::string("fr"));

    CHECK_EQ(posixTimezone("Europe/Paris"), std::string("CET-1CEST,M3.5.0,M10.5.0/3"));
    CHECK_EQ(posixTimezone("Europe/London"), std::string("GMT0BST,M3.5.0/1,M10.5.0"));
    CHECK_EQ(posixTimezone("EST5EDT"), std::string("EST5EDT"));
    CHECK_EQ(posixTimezone("Mars/Olympus"), std::string("CET-1CEST,M3.5.0,M10.5.0/3"));

    const std::string hours = "10,10,10,10,10,10,50,90,90,90,90,90,90,90,90,90,90,90,90,90,90,60,30,10";
    CHECK_EQ(brightnessForHour(hours, 0, 77), 10);
    CHECK_EQ(brightnessForHour(hours, 7, 77), 90);
    CHECK_EQ(brightnessForHour(hours, 22, 77), 30);
    CHECK_EQ(brightnessForHour(hours, 24, 77), 77);
    CHECK_EQ(brightnessForHour("90,90", 3, 77), 77);
    CHECK_EQ(brightnessForHour("", 3, 77), 77);
}

static void testAttract() {
    AttractPlaylist p("1,T, 4 ,M,X,F", "T4F");
    CHECK_EQ(p.codes().size(), 3u);
    CHECK_EQ(p.next(), 'T');
    CHECK_EQ(p.next(), '4');
    CHECK_EQ(p.next(), 'F');
    CHECK_EQ(p.next(), 'T');
    AttractPlaylist none("1,2", "T4F");  // Raspy2DMD default "1,T" minus GIFs still has T
    CHECK(none.empty());
    CHECK_EQ(none.next(), '\0');
    CHECK_EQ(AttractPlaylist("1,T", "T4F").codes().size(), 1u);

    CarouselEntry e = parseCarouselFile("DG;IT3\r\nBonjour\r\nles amis\r\n");
    CHECK(e.valid && e.hasOptions);
    CHECK(e.motion == Motion::Left);
    CHECK_EQ(e.iterations, 3);
    CHECK_EQ(e.message, std::string(" Bonjour les amis"));
    e = parseCarouselFile("\nJuste un texte");
    CHECK(e.valid && !e.hasOptions && e.motion == Motion::None);
    e = parseCarouselFile("A;\xE2\x99\xA0" "fete.gif\nBravo");
    CHECK(e.randomMotion);
    CHECK_EQ(e.gifBackground, std::string("fete.gif"));
    e = parseCarouselFile("A;GD\nX");  // later option wins
    CHECK(!e.randomMotion && e.motion == Motion::Right);
    CHECK(!parseCarouselFile("GD").valid);
    CHECK(!parseCarouselFile("").valid);
    CHECK(carouselRandomMotion(9) == Motion::Right);

    IdleTimer t;
    t.touch(0);
    CHECK(!t.expired(100000));  // disabled by default
    t.configure(30000);
    t.touch(1000);
    CHECK(!t.expired(30000));
    CHECK(t.expired(31000));
    CHECK(!t.expired(40000));   // fires once per touch
}

static void testMediaPaths() {
    CHECK_EQ(mapPiPath("/Medias/Gifs/fun/a.gif"), std::string("/gifs/fun/a.gif"));
    CHECK_EQ(mapPiPath("/Medias/SpecialsMoves/RED_HAT"), std::string("/specialsmoves/RED_HAT"));
    CHECK_EQ(mapPiPath("Gifs//x.gif"), std::string("/Gifs/x.gif"));
    CHECK_EQ(mapPiPath("\\gifs\\x.gif"), std::string("/gifs/x.gif"));
    CHECK_EQ(mapPiPath("/gifs/dir/"), std::string("/gifs/dir"));

    std::vector<std::string> fs = {"/gifs/a.gif", "/gifs/Gifs/b.gif", "/gifs/c.gif", "/images/i.png"};
    auto exists = [&](const std::string& p) { return std::find(fs.begin(), fs.end(), p) != fs.end(); };
    CHECK_EQ(resolveMedia("a.gif", "/gifs", exists), std::string("/gifs/a.gif"));
    CHECK_EQ(resolveMedia("Gifs/b.gif", "/gifs", exists), std::string("/gifs/Gifs/b.gif"));
    CHECK_EQ(resolveMedia("Gifs/c.gif", "/gifs", exists), std::string("/gifs/c.gif"));  // lower-cased folder
    CHECK_EQ(resolveMedia("/Medias/Images/i.png", "/gifs", exists), std::string("/images/i.png"));
    CHECK_EQ(resolveMedia("nope.gif", "/gifs", exists), std::string(""));
    CHECK_EQ(resolveMedia("", "/gifs", exists), std::string(""));

    CHECK(isGifFile("/gifs/A.GIF"));
    CHECK(isPngFile("x.Png"));
    CHECK(!isGifFile("/gifs/gif"));
    CHECK_EQ(lowerExtension("/a.b/c"), std::string(""));
    CHECK_EQ(baseName("/gifs/a/b.gif"), std::string("b.gif"));
    CHECK_EQ(parentDir("/gifs/a/b.gif"), std::string("/gifs/a"));
    CHECK_EQ(parentDir("/x.gif"), std::string("/"));
}

static void testExclusions() {
    ExclusionList ex;
    CHECK(ex.toggle(true, "fun", "/Medias/Gifs/fun"));
    CHECK(ex.toggle(false, "b.gif", "/Medias/Gifs/b.gif"));
    CHECK(ex.isExcluded("/gifs/fun/x.gif"));
    CHECK(ex.isExcluded("/gifs/fun/deep/y.gif"));
    CHECK(!ex.isExcluded("/gifs/funny/x.gif"));  // prefix must stop at a folder boundary
    CHECK(ex.isExcluded("/gifs/b.gif"));
    CHECK(!ex.isExcluded("/gifs/a.gif"));

    ExclusionList copy;
    copy.parse(ex.serialize() + "# comment\nbad line\n");
    CHECK_EQ(copy.entries().size(), 2u);
    CHECK(copy.isExcluded("/gifs/fun/x.gif"));

    CHECK(!ex.toggle(true, "fun", "/Medias/Gifs/fun"));  // toggling again removes it
    CHECK(!ex.isExcluded("/gifs/fun/x.gif"));

    std::vector<std::string> files = {"/gifs/a.gif", "/gifs/b.gif", "/gifs/fun/x.gif"};
    CHECK_EQ(filterExcluded(files, copy).size(), 1u);
}

static void testEffects() {
    const std::string table = "# id|name|text|gif|sound\n12|Bravo|Bien joue !|bravo.gif|pop.ogg\n13|Gif only||x.gif|\n14|Short\n";
    Effect e;
    CHECK(findEffect(table, "12", e));
    CHECK_EQ(e.text, std::string("Bien joue !"));
    CHECK_EQ(e.gif, std::string("bravo.gif"));
    CHECK_EQ(e.sound, std::string("pop.ogg"));
    CHECK(findEffect(table, " 14 ", e));
    CHECK_EQ(e.text, std::string(""));
    CHECK(!findEffect(table, "99", e));
    CHECK(effectKind("t", "g", "") == EffectKind::GifWithText);
    CHECK(effectKind("t", "", "s") == EffectKind::Text);
    CHECK(effectKind("", "g", "s") == EffectKind::Gif);
    CHECK(effectKind("", "", "s") == EffectKind::SoundOnly);
    CHECK(effectKind("", "", "") == EffectKind::Nothing);
}

static void testFitAndUpload() {
    FitRect r = fitImage(64, 32, 64, 32, true);
    CHECK(r.x == 0 && r.y == 0 && r.w == 64 && r.h == 32);
    r = fitImage(32, 16, 64, 32, true);  // never enlarged, centred
    CHECK(r.x == 16 && r.y == 8 && r.w == 32 && r.h == 16);
    r = fitImage(32, 16, 64, 32, false);
    CHECK(r.x == 0 && r.y == 0);
    r = fitImage(128, 32, 64, 32, true);  // 2:1 wider panel image -> 64x16
    CHECK(r.w == 64 && r.h == 16 && r.y == 8);
    r = fitImage(640, 480, 64, 32, true);  // HDMI-sized image -> 43x32
    CHECK(r.h == 32 && r.w == 43 && r.x == 10);
    r = fitImage(0, 10, 64, 32, true);
    CHECK(r.w == 0);

    // Every destination row is produced exactly once, whatever the scale.
    const int sizes[][2] = {{32, 32}, {480, 32}, {16, 32}, {33, 32}, {7, 3}};
    for (const auto& sz : sizes) {
        std::vector<int> hits(static_cast<size_t>(sz[1]), 0);
        for (int sr = 0; sr < sz[0]; ++sr) {
            int first = 0, count = 0;
            destRows(sr, sz[0], sz[1], first, count);
            for (int d = first; d < first + count; ++d) {
                hits[static_cast<size_t>(d)]++;
                CHECK_EQ(srcIndex(d, sz[0], sz[1]), sr);
            }
        }
        for (int h : hits) CHECK_EQ(h, 1);
    }
    int f0 = 0, c0 = 0;
    destRows(5, 0, 32, f0, c0);
    CHECK_EQ(c0, 0);

    CHECK_EQ(scoreMediaKey({"S20", "T20", "X"}), std::string("T20"));
    CHECK_EQ(scoreMediaKey({"X", "X", "X"}), std::string(""));

    CHECK_EQ(uploadPath("/gifs", "my fun.gif"), std::string("/gifs/my_fun.gif"));
    CHECK_EQ(uploadPath("/gifs", "../../config.json"), std::string("/gifs/config.json"));
    CHECK_EQ(uploadPath("/gifs/../", "a.gif"), std::string(""));
    CHECK_EQ(uploadPath("/gifs", ".."), std::string(""));
    CHECK_EQ(uploadPath("/", "effets.txt"), std::string("/effets.txt"));
    CHECK_EQ(uploadPath("/gi fs", "a.gif"), std::string(""));
    CHECK_EQ(htmlEscape("<a href='x'>&\"</a>"), std::string("&lt;a href=&#39;x&#39;&gt;&amp;&quot;&lt;/a&gt;"));
}

int main() {
    testParseCommand();
    testFilter();
    testSpecialMoves();
    testRgb();
    testAscii();
    testWrap();
    testTimeline();
    testSceneRunner();
    testFx();
    testFireworks();
    testStarsAndRain();
    testClockFormat();
    testAttract();
    testMediaPaths();
    testExclusions();
    testEffects();
    testFitAndUpload();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
