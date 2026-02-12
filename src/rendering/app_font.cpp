/// @file app_font.cpp
/// @brief Loads and manages the application-wide custom font.

#include "rendering/app_font.hpp"

#include <cstdio>

namespace gateflow {

namespace {

/// Sentinel: if true we own the font and must unload it.
bool g_font_loaded = false;

/// The loaded font handle (or default).
Font g_font = {};

/// Attempts to load a TTF from `path` at a high base size for crisp rendering.
/// Returns true on success.
bool try_load(const char* path) {
    if (!FileExists(path)) {
        return false;
    }
    // Load at a large base size so Raylib has enough resolution for all sizes
    // we'll use (10–24 px).  256 code-points covers ASCII + common symbols.
    g_font = LoadFontEx(path, 48, nullptr, 256);
    if (g_font.glyphCount <= 0) {
        return false;
    }
    // Bilinear filtering gives smooth scaling at arbitrary sizes.
    SetTextureFilter(g_font.texture, TEXTURE_FILTER_BILINEAR);
    return true;
}

} // namespace

void init_app_font() {
    // Try bundled font first (relative to working directory)
    if (try_load("resources/fonts/Hack-Regular.ttf")) {
        g_font_loaded = true;
        return;
    }
    // System fallback (Linux)
    if (try_load("/usr/share/fonts/TTF/Hack-Regular.ttf")) {
        g_font_loaded = true;
        return;
    }
    // Give up — use Raylib default bitmap font.
    g_font = GetFontDefault();
    g_font_loaded = false;
    std::fprintf(stderr, "[gateflow] Custom font not found — using Raylib default.\n");
}

void cleanup_app_font() {
    if (g_font_loaded) {
        UnloadFont(g_font);
        g_font_loaded = false;
    }
}

Font get_app_font() {
    return g_font;
}

// -----------------------------------------------------------------------
// Wrappers
// -----------------------------------------------------------------------

void DrawAppText(const char* text, int posX, int posY, int fontSize, Color color) {
    // Spacing scaled proportionally: Raylib default uses fontSize/10.
    float spacing = static_cast<float>(fontSize) / 10.0f;
    DrawTextEx(g_font, text, {static_cast<float>(posX), static_cast<float>(posY)},
               static_cast<float>(fontSize), spacing, color);
}

int MeasureAppText(const char* text, int fontSize) {
    float spacing = static_cast<float>(fontSize) / 10.0f;
    Vector2 size = MeasureTextEx(g_font, text, static_cast<float>(fontSize), spacing);
    return static_cast<int>(size.x);
}

void DrawAppTextEx(const char* text, Vector2 position, float fontSize, float spacing, Color color) {
    DrawTextEx(g_font, text, position, fontSize, spacing, color);
}

Vector2 MeasureAppTextEx(const char* text, float fontSize, float spacing) {
    return MeasureTextEx(g_font, text, fontSize, spacing);
}

} // namespace gateflow
