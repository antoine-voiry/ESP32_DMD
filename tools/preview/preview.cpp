// Renders the firmware's own effects and icons (src/core) into raw frames for docs/img.
// Usage: make -C tools/preview   (needs Python 3 + Pillow for the final images)
#include <cstdio>
#include <string>
#include <vector>

#include "core/Canvas.h"
#include "core/FxRender.h"
#include "core/Online.h"

using namespace dmd;

namespace {

// Binary PPM (P6), RGB565 expanded to 8 bits per channel.
bool writePpm(const std::string& path, const Canvas& c) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%d %d\n255\n", c.w, c.h);
    for (uint16_t p : c.px) {
        const unsigned char rgb[3] = {static_cast<unsigned char>(((p >> 11) & 0x1F) * 255 / 31),
                                      static_cast<unsigned char>(((p >> 5) & 0x3F) * 255 / 63),
                                      static_cast<unsigned char>((p & 0x1F) * 255 / 31)};
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
    return true;
}

void renderFx(const std::string& out, const char* name, FxBackground kind, uint32_t warmupMs, int frames) {
    FxBackgroundRenderer r(kind, 64, 32, 1234);
    Canvas c(64, 32);
    uint32_t t = 0;
    for (; t < warmupMs; t += 33) {
        c.clear();
        r.render(c, t, 33);
    }
    for (int i = 0; i < frames; ++i, t += 33) {
        c.clear();
        r.render(c, t, 33);
        char path[256];
        std::snprintf(path, sizeof(path), "%s/%s_%03d.ppm", out.c_str(), name, i);
        writePpm(path, c);
    }
}

void renderIcons(const std::string& out) {
    // Two rows of 64x32 panels side by side: day icons, then night icons.
    const WeatherKind kinds[] = {WeatherKind::Clear,   WeatherKind::FewClouds, WeatherKind::Clouds,
                                 WeatherKind::Showers, WeatherKind::Rain,      WeatherKind::Thunder,
                                 WeatherKind::Snow,    WeatherKind::Mist};
    Canvas sheet(128, 64);
    for (int i = 0; i < 8; ++i) {
        drawWeatherIcon(sheet, kinds[i], false, (i % 4) * 32 + 4, (i / 4) * 32 + 4, 24);
    }
    writePpm(out + "/icons_day.ppm", sheet);
    Canvas wind(64, 32);
    for (int i = 0; i < 4; ++i) {
        drawWindArrow(wind, 8 + i * 16, 16, 12, static_cast<float>(i * 90), rgb565(0, 160, 255));
    }
    writePpm(out + "/wind.ppm", wind);
    Canvas tempo(64, 32);
    tempo.fillRect(6, 4, 20, 20, rgb565(0, 80, 255));
    tempo.fillRect(38, 4, 20, 20, rgb565(255, 0, 0));
    writePpm(out + "/tempo.ppm", tempo);
}

}  // namespace

int main(int argc, char** argv) {
    const std::string out = argc > 1 ? argv[1] : "frames";
    renderFx(out, "fireworks", FxBackground::Fireworks, 1500, 75);
    renderFx(out, "plasma", FxBackground::Plasma, 0, 32);
    renderFx(out, "stars", FxBackground::Starfield, 0, 45);
    renderFx(out, "matrix", FxBackground::MatrixRain, 2000, 45);
    renderIcons(out);
    std::printf("frames written to %s\n", out.c_str());
    return 0;
}
