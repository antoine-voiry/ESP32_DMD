// Scenes drawn on the panel: text and its movements, effects, clock, PNG and GIF, renderer facade.

#include <Adafruit_GFX.h>

#include "fixtures.h"
#include "fw_test.h"
#include "matrix/Hub75_Matrix.h"
#include "render/Bitmap.h"
#include "render/ClockScene.h"
#include "render/FxScene.h"
#include "render/MediaScenes.h"
#include "render/TextImage.h"
#include "render/TextScene.h"
#include "util/MediaLibrary.h"

namespace {

const uint16_t kRed = 0xF800;

// Plays a scene to its end (or `maxMs`); returns the number of ticks that kept it running.
int play(dmd::Scene& scene, unsigned long maxMs = 30000, unsigned long stepMs = 10) {
    scene.start(millis());
    int ticks = 0;
    for (unsigned long t = 0; t < maxMs; t += stepMs) {
        fake::advance(stepMs);
        if (!scene.tick(millis())) return ticks;
        ++ticks;
    }
    return ticks;
}

int litColumns(const MatrixPanel_I2S_DMA* p) {
    int cols = 0;
    for (int x = 0; x < p->width; ++x) {
        for (int y = 0; y < p->height; ++y) {
            if (p->shownAt(x, y)) {
                ++cols;
                break;
            }
        }
    }
    return cols;
}

}  // namespace

static void testTextImage() {
    freshBoard();
    TextLayoutOptions options;
    std::unique_ptr<GFXcanvas1> image = renderTextImage("HELLO", 64, 32, options);
    CHECK(image != nullptr);
    CHECK_EQ(image->width(), 64);

    options.singleLine = true;
    image = renderTextImage("A long line that scrolls", 64, 32, options);
    CHECK(image->width() > 64);
    image = renderTextImage("", 64, 32, options);
    CHECK_EQ(image->width(), 1);
    image = renderTextImage(std::string(2000, 'W'), 64, 32, options);
    CHECK_EQ(image->width(), 4096);  // capped

    TextLayoutOptions tiny;
    tiny.maxFontPx = 8;  // only the built-in font
    image = renderTextImage("Many words that need several lines here", 64, 32, tiny);
    CHECK(image != nullptr);
}

static void testTextScene() {
    freshBoard();
    Hub75_Matrix matrix;
    MatrixPanel_I2S_DMA* p = fake::panel();

    TextStyle style;
    TextScene still(matrix, "180", style);
    CHECK_EQ(play(still), 0);  // static text: drawn once, done
    CHECK(p->litPixels() > 0);
    still.abort();
    CHECK_EQ(p->litPixels(), 0);

    // Each movement animates, then ends.
    const dmd::Motion motions[] = {dmd::Motion::Left,   dmd::Motion::Right,      dmd::Motion::Up,
                                   dmd::Motion::Down,   dmd::Motion::Rotate,     dmd::Motion::AntiRotate,
                                   dmd::Motion::Flip,   dmd::Motion::Twirl};
    for (dmd::Motion m : motions) {
        style.motion = m;
        TextScene scene(matrix, "Bonjour", style);
        CHECK(play(scene) > 3);
    }

    // A long stall resumes the animation instead of fast-forwarding through it.
    style.motion = dmd::Motion::Left;
    TextScene scroll(matrix, "Bonjour a tous", style);
    scroll.start(millis());
    fake::advance(5000);
    CHECK(scroll.tick(millis()));
    CHECK(litColumns(p) > 0);

    // Background colour fills the panel.
    TextStyle red;
    red.bg = dmd::Rgb(255, 0, 0);
    TextScene onRed(matrix, "", red);
    play(onRed);
    CHECK_EQ(p->shownAt(0, 0), kRed);
}

static void testFxScene() {
    freshBoard();
    Hub75_Matrix matrix;
    MatrixPanel_I2S_DMA* p = fake::panel();
    const dmd::FxBackground backgrounds[] = {dmd::FxBackground::None, dmd::FxBackground::Plasma,
                                             dmd::FxBackground::Fireworks, dmd::FxBackground::Starfield,
                                             dmd::FxBackground::MatrixRain};
    const dmd::FxText effects[] = {dmd::FxText::Solid, dmd::FxText::Rainbow, dmd::FxText::Wave,
                                   dmd::FxText::Typewriter, dmd::FxText::Sparkle};
    for (dmd::FxBackground bg : backgrounds) {
        for (dmd::FxText fx : effects) {
            FxSpec spec;
            spec.background = bg;
            spec.textFx = fx;
            spec.text = "Bravo";
            spec.durationMs = 600;
            FxScene scene(matrix, spec);
            const int flipsBefore = p->flips;
            CHECK(play(scene) > 10);
            CHECK(p->flips - flipsBefore > 10);
        }
    }
    // The text stays once the effect is over; a background alone leaves nothing behind.
    CHECK(p->litPixels() > 0);
    FxSpec plain;
    plain.background = dmd::FxBackground::Plasma;
    plain.durationMs = 100;
    FxScene background(matrix, plain);
    play(background);
    background.abort();
    CHECK_EQ(p->litPixels(), 0);
}

static void testBitmap() {
    freshBoard();
    writeBytes("/images/red.png", kPngHalfRed, sizeof(kPngHalfRed));
    writeBytes("/images/wide.png", kPngTooWide, sizeof(kPngTooWide));
    fake::writeFile("/images/broken.png", "not a png");

    std::shared_ptr<Bitmap565> b = loadPng("/images/red.png", 64, 32, false);
    CHECK(b != nullptr);
    CHECK_EQ(b->get(0, 0), kRed);
    CHECK_EQ(b->get(7, 0), 0);  // transparent pixels blend with black
    CHECK_EQ(b->get(8, 0), 0);  // never enlarged
    b = loadPng("/images/red.png", 4, 2, true);
    CHECK_EQ(b->get(0, 0), kRed);  // shrunk to fit
    CHECK(loadPng("/images/wide.png", 64, 32, true) == nullptr);
    CHECK(loadPng("/images/broken.png", 64, 32, true) == nullptr);
    CHECK(loadPng("/images/missing.png", 64, 32, true) == nullptr);

    Bitmap565 canvas(64, 32);
    drawTextBox(canvas, "12", 0, 0, 32, 16, dmd::Rgb(255, 255, 255));
    CHECK(canvas.count(0xFFFF) > 0);
    drawTextBox(canvas, "", 0, 0, 32, 16, dmd::Rgb(255, 255, 255));
    drawTextBox(canvas, "x", 0, 0, 0, 16, dmd::Rgb(255, 255, 255));

    Hub75_Matrix matrix;
    TextLayoutOptions options;
    std::unique_ptr<GFXcanvas1> text = renderTextImage("OK", 64, 32, options);
    Bitmap565 background(64, 32);
    background.clear(kRed);
    presentFrame(matrix, &background, text.get(), dmd::Rgb(0, 0, 255));
    MatrixPanel_I2S_DMA* p = fake::panel();
    CHECK_EQ(p->shownAt(0, 0), kRed);
    CHECK(std::count(p->shown.begin(), p->shown.end(), 0x001F) > 0);  // text
    CHECK(std::count(p->shown.begin(), p->shown.end(), 0) > 0);       // its outline
    presentFrame(matrix, nullptr, nullptr, dmd::Rgb(0, 0, 255));
    CHECK_EQ(p->litPixels(), 0);
}

static void testMediaScenes() {
    freshBoard();
    writeBytes("/gifs/anim.gif", kGif3Frames, sizeof(kGif3Frames));
    writeBytes("/images/red.png", kPngHalfRed, sizeof(kPngHalfRed));
    fake::writeFile("/gifs/broken.gif", "GIF89a nonsense");
    Hub75_Matrix matrix;
    MatrixPanel_I2S_DMA* p = fake::panel();

    GifScene gif(matrix, "/gifs/anim.gif", false);
    gif.start(millis());
    CHECK_EQ(p->shownAt(0, 0), 0x07E0);  // first frame: green
    CHECK_EQ(p->shownAt(10, 0), 0);      // not enlarged
    fake::advance(100);                  // 0 ms delay plays at 10 fps
    CHECK(gif.tick(millis()));
    CHECK_EQ(p->shownAt(0, 0), 0x001F);  // second frame: blue on the left...
    CHECK_EQ(p->shownAt(3, 0), 0x07E0);  // ...the transparent half keeps the first frame
    fake::advance(50);
    CHECK(gif.tick(millis()));
    CHECK_EQ(p->shownAt(0, 0), 0xFFFF);  // third frame
    int more = 0;  // AnimatedGIF reports the end on the call after the last frame
    while (more < 10 && (fake::advance(20), gif.tick(millis()))) ++more;
    CHECK(more < 10);
    CHECK_EQ(p->litPixels(), 0);  // cleared once played

    MediaOverlay overlay;
    overlay.text = "GO";
    GifScene withText(matrix, "/gifs/anim.gif", true, overlay);
    CHECK(play(withText) > 0);
    withText.abort();

    GifScene broken(matrix, "/gifs/broken.gif", true);
    CHECK_EQ(play(broken), 0);
    GifScene missing(matrix, "/gifs/missing.gif", true);
    CHECK_EQ(play(missing), 0);

    ImageScene image(matrix, "/images/red.png", false, overlay);
    CHECK_EQ(play(image), 0);
    CHECK_EQ(p->shownAt(0, 0), kRed);
    image.abort();
    CHECK_EQ(p->litPixels(), 0);
}

static void testClockScene() {
    freshBoard();
    writeBytes("/patterns/OldGame.png", kPngHalfRed, sizeof(kPngHalfRed));
    Hub75_Matrix matrix;
    MatrixPanel_I2S_DMA* p = fake::panel();

    ClockSpec spec;
    spec.pattern = "/patterns/OldGame.png";
    ClockScene both(matrix, spec);
    CHECK_EQ(both.durationMs(), 6000u);
    both.start(millis());
    CHECK(std::count(p->shown.begin(), p->shown.end(), kRed) > 0);  // pattern background
    const int flips = p->flips;
    for (int i = 0; i < 25; ++i) {  // date for 2 s, then the time, redrawn when it changes
        fake::advance(200);
        fake::epoch += 1;
        CHECK(both.tick(millis()));
    }
    CHECK(p->flips > flips + 10);
    fake::advance(1000);
    CHECK(!both.tick(millis()));

    spec.mode = 3;
    CHECK_EQ(ClockScene(matrix, spec).durationMs(), spec.dateMs);
    spec.mode = 4;
    CHECK_EQ(ClockScene(matrix, spec).durationMs(), spec.hoursMs);
    spec.mode = 1;  // did nothing on the Pi: date then time here
    CHECK_EQ(ClockScene(matrix, spec).durationMs(), spec.dateMs + spec.hoursMs);

    fake::epoch = 1000;  // NTP not answered yet: dashes
    spec.pattern.clear();
    spec.mode = 4;
    ClockScene unsynced(matrix, spec);
    play(unsynced);
    CHECK(p->litPixels() > 0);
    unsynced.abort();
    CHECK_EQ(p->litPixels(), 0);
}

static void testRenderer() {
    freshBoard();
    fake::writeFile("/scores/T20/a.gif", "");
    writeBytes("/scores/T19/a.gif", kGif3Frames, sizeof(kGif3Frames));
    writeBytes("/specialsmoves/BREAKFAST/b.gif", kGif3Frames, sizeof(kGif3Frames));
    MediaLibrary media;
    media.begin();
    Hub75_Matrix matrix;
    DMDRenderer r(&matrix);
    MatrixPanel_I2S_DMA* p = fake::panel();
    CHECK(r.idle());

    r.renderText("Hello", 500);
    r.update();
    CHECK(!r.idle());  // hold
    runUntilIdle(r);
    CHECK(r.idle());
    CHECK(p->litPixels() > 0);

    TextRequest colored;
    colored.text = "Red";
    colored.hasFg = colored.hasBg = true;
    colored.fg = dmd::Rgb(255, 255, 255);
    colored.bg = dmd::Rgb(255, 0, 0);
    colored.iterations = 0;
    r.renderText(colored);
    runUntilIdle(r);
    CHECK_EQ(p->shownAt(0, 0), kRed);

    // Special moves: fireworks for the big ones, sparkle for the others, then the darts.
    r.renderScore("t20 - t20 - t20");
    runUntilIdle(r);
    r.renderScore("S1 - S5 - S20");  // no media library yet: no GIF lookup
    runUntilIdle(r);
    r.setMediaLibrary(&media);
    r.renderScore("S1 - S20 - S5");  // BREAKFAST: its GIF
    runUntilIdle(r);
    r.renderScore("S20 - X - T19");   // a score GIF from /scores/T19, no special move with a miss
    runUntilIdle(r);
    r.renderScore("S1 - S1 - S2");
    runUntilIdle(r);
    CHECK(p->litPixels() > 0);

    r.renderImage("/scores/T19/a.gif", 0, "x");
    r.renderGif("/scores/T19/a.gif", 0, "x");
    r.renderClock(ClockSpec());
    r.renderFx(FxSpec());
    r.update();
    r.interrupt();
    r.renderStatus("MQTT not connected");
    r.update();
    CHECK(p->litPixels() > 0);
    r.setBrightnessPercent(50);
    CHECK_EQ(p->brightness, 127);
    r.clear();
    CHECK(r.idle());
    CHECK_EQ(p->litPixels(), 0);
}

void testRendering() {
    testTextImage();
    testTextScene();
    testFxScene();
    testBitmap();
    testMediaScenes();
    testClockScene();
    testRenderer();
}
