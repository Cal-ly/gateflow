/// @file app_font.hpp
/// @brief Application-wide font management.
///
/// Loads a custom TTF font at startup and provides thin wrappers around
/// Raylib's text drawing functions so every call site uses the same
/// typeface without having to pass a Font handle around.

#pragma once

#include <raylib.h>

namespace gateflow {

/// Load the application font. Must be called *after* InitWindow().
/// Tries, in order:
///   1. resources/fonts/Hack-Regular.ttf  (bundled with the repo)
///   2. /usr/share/fonts/TTF/Hack-Regular.ttf  (system fallback)
/// If both fail, the Raylib default bitmap font is used.
void init_app_font();

/// Unload the custom font (safe to call even if init failed).
/// Should be called before CloseWindow().
void cleanup_app_font();

/// Returns the loaded font (or GetFontDefault() if loading failed).
Font get_app_font();

// -----------------------------------------------------------------------
// Drop-in replacements for Raylib helpers — use the application font
// -----------------------------------------------------------------------

/// Draws text at integer (x, y) with the app font.
/// Signature intentionally mirrors Raylib's DrawText().
void DrawAppText(const char* text, int posX, int posY, int fontSize, Color color);

/// Measures the width in pixels of a string at the given size.
/// Signature intentionally mirrors Raylib's MeasureText().
int MeasureAppText(const char* text, int fontSize);

/// DrawTextEx using the app font.
void DrawAppTextEx(const char* text, Vector2 position, float fontSize, float spacing, Color color);

/// MeasureTextEx using the app font.
Vector2 MeasureAppTextEx(const char* text, float fontSize, float spacing);

} // namespace gateflow
