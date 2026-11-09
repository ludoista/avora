#ifndef RAYGUI_H
#define RAYGUI_H

#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RAYGUI_VERSION_MAJOR 4
#define RAYGUI_VERSION_MINOR 0
#define RAYGUI_VERSION_PATCH 0
#define RAYGUI_VERSION       "4.0"

typedef enum {
    STATE_NORMAL = 0,
    STATE_FOCUSED,
    STATE_PRESSED,
    STATE_DISABLED
} GuiState;

typedef enum {
    DEFAULT = 0,
    LABEL,
    BUTTON,
    TOGGLE,
    SLIDER,
    PROGRESSBAR,
    CHECKBOX,
    COMBOBOX,
    DROPDOWNBOX,
    TEXTBOX,
    VALUEBOX,
    SPINNER,
    LISTVIEW,
    COLORPICKER,
    SCROLLBAR,
    STATUSBAR
} GuiControl;

typedef enum {
    BORDER_COLOR_NORMAL = 0,
    BASE_COLOR_NORMAL,
    TEXT_COLOR_NORMAL,
    BORDER_COLOR_FOCUSED,
    BASE_COLOR_FOCUSED,
    TEXT_COLOR_FOCUSED,
    BORDER_COLOR_PRESSED,
    BASE_COLOR_PRESSED,
    TEXT_COLOR_PRESSED,
    BORDER_COLOR_DISABLED,
    BASE_COLOR_DISABLED,
    TEXT_COLOR_DISABLED,
    BORDER_WIDTH,
    TEXT_PADDING,
    TEXT_ALIGNMENT,
    TEXT_SIZE,
    TEXT_SPACING,
    LINE_COLOR,
    BACKGROUND_COLOR
} GuiControlProperty;

void GuiEnable(void);
void GuiDisable(void);
void GuiLock(void);
void GuiUnlock(void);
bool GuiIsLocked(void);
void GuiSetFont(Font font);
Font GuiGetFont(void);
void GuiSetStyle(int control, int property, int value);
int  GuiGetStyle(int control, int property);
void GuiLoadStyleDark(void);

int GuiPanel(Rectangle bounds, const char *text);
int GuiLabel(Rectangle bounds, const char *text);
int GuiButton(Rectangle bounds, const char *text);
int GuiSliderBar(Rectangle bounds, const char *textLeft, const char *textRight, float *value, float minValue, float maxValue);
int GuiSlider(Rectangle bounds, const char *textLeft, const char *textRight, float *value, float minValue, float maxValue);
int GuiTextBox(Rectangle bounds, char *text, int textSize, bool editMode);
int GuiDropdownBox(Rectangle bounds, const char *text, int *active, bool editMode);

#ifdef RAYGUI_IMPLEMENTATION

static GuiState guiState = STATE_NORMAL;
static bool guiLocked = false;
static Font guiFont = { 0 };
static bool guiFontInitialized = false;
static int guiStyle[16][20] = { 0 };
static bool guiStyleLoaded = false;

static Color GuiGetColor(unsigned int hexValue)
{
    Color color;
    color.r = (unsigned char)((hexValue >> 24) & 0xFF);
    color.g = (unsigned char)((hexValue >> 16) & 0xFF);
    color.b = (unsigned char)((hexValue >> 8) & 0xFF);
    color.a = (unsigned char)(hexValue & 0xFF);
    return color;
}

void GuiLoadStyleDark(void)
{
    for (int i = 0; i < 16; i++) {
        guiStyle[i][BORDER_COLOR_NORMAL]   = 0x52525BFF;
        guiStyle[i][BASE_COLOR_NORMAL]     = 0x222226FF;
        guiStyle[i][TEXT_COLOR_NORMAL]     = 0xF4F4F5FF;
        guiStyle[i][BORDER_COLOR_FOCUSED]  = 0xFAFAFAFF;
        guiStyle[i][BASE_COLOR_FOCUSED]    = 0x38383EFF;
        guiStyle[i][TEXT_COLOR_FOCUSED]    = 0xFFFFFFFF;
        guiStyle[i][BORDER_COLOR_PRESSED]  = 0xFFFFFFFF;
        guiStyle[i][BASE_COLOR_PRESSED]    = 0xE4E4E7FF;
        guiStyle[i][TEXT_COLOR_PRESSED]    = 0x18181BFF;
        guiStyle[i][BORDER_COLOR_DISABLED] = 0x2E2E33FF;
        guiStyle[i][BASE_COLOR_DISABLED]   = 0x18181BFF;
        guiStyle[i][TEXT_COLOR_DISABLED]   = 0x6E6E77FF;
        guiStyle[i][BORDER_WIDTH]          = 1;
        guiStyle[i][TEXT_PADDING]          = 6;
        guiStyle[i][TEXT_SIZE]             = 15;
        guiStyle[i][TEXT_SPACING]          = 1;
        guiStyle[i][LINE_COLOR]            = 0x38383EFF;
        guiStyle[i][BACKGROUND_COLOR]      = 0x141416FF;
    }
    guiStyleLoaded = true;
}

static void EnsureStyleLoaded(void)
{
    if (!guiStyleLoaded) {
        GuiLoadStyleDark();
    }
    if (!guiFontInitialized) {
        guiFont = GetFontDefault();
        guiFontInitialized = true;
    }
}

void GuiEnable(void)  { guiState = STATE_NORMAL; }
void GuiDisable(void) { guiState = STATE_DISABLED; }
void GuiLock(void)    { guiLocked = true; }
void GuiUnlock(void)  { guiLocked = false; }
bool GuiIsLocked(void){ return guiLocked; }

void GuiSetFont(Font font)
{
    guiFont = font;
    guiFontInitialized = true;
}

Font GuiGetFont(void)
{
    EnsureStyleLoaded();
    return guiFont;
}

void GuiSetStyle(int control, int property, int value)
{
    EnsureStyleLoaded();
    if (control >= 0 && control < 16 && property >= 0 && property < 20) {
        guiStyle[control][property] = value;
        if (control == DEFAULT) {
            for (int i = 1; i < 16; i++) {
                guiStyle[i][property] = value;
            }
        }
    }
}

int GuiGetStyle(int control, int property)
{
    EnsureStyleLoaded();
    if (control >= 0 && control < 16 && property >= 0 && property < 20) {
        return guiStyle[control][property];
    }
    return 0;
}

static void GuiDrawTextCentered(const char *text, Rectangle bounds, Color color, bool alignLeft)
{
    if (!text || text[0] == '\0') return;
    if (bounds.width <= 4.0f || bounds.height <= 4.0f) return;
    EnsureStyleLoaded();
    float fontSize = (float)guiStyle[DEFAULT][TEXT_SIZE];
    float spacing  = (float)guiStyle[DEFAULT][TEXT_SPACING];
    Vector2 size   = MeasureTextEx(guiFont, text, fontSize, spacing);
    Vector2 pos;
    if (alignLeft) {
        pos.x = bounds.x + (float)guiStyle[DEFAULT][TEXT_PADDING];
    } else {
        pos.x = bounds.x + (bounds.width - size.x) * 0.5f;
        if (pos.x < bounds.x + 3.0f) pos.x = bounds.x + 3.0f;
    }
    pos.y = bounds.y + (bounds.height - size.y) * 0.5f;
    DrawTextEx(guiFont, text, pos, fontSize, spacing, color);
}

int GuiPanel(Rectangle bounds, const char *text)
{
    EnsureStyleLoaded();
    Color bg     = GuiGetColor((unsigned int)guiStyle[DEFAULT][BACKGROUND_COLOR]);
    Color border = GuiGetColor((unsigned int)guiStyle[DEFAULT][LINE_COLOR]);
    DrawRectangleRec(bounds, bg);
    DrawRectangleLinesEx(bounds, 1.0f, border);

    if (text && text[0] != '\0') {
        Rectangle header = { bounds.x, bounds.y, bounds.width, 26.0f };
        DrawRectangleRec(header, GuiGetColor(0x1E1E22FF));
        DrawLineEx(
            (Vector2){ bounds.x, bounds.y + 26.0f },
            (Vector2){ bounds.x + bounds.width, bounds.y + 26.0f },
            1.0f,
            border
        );
        BeginScissorMode((int)header.x + 2, (int)header.y, (int)header.width - 4, (int)header.height);
        GuiDrawTextCentered(text, header, GuiGetColor(0xF4F4F5FF), true);
        EndScissorMode();
    }
    return 0;
}

int GuiLabel(Rectangle bounds, const char *text)
{
    EnsureStyleLoaded();
    GuiDrawTextCentered(text, bounds, GuiGetColor((unsigned int)guiStyle[LABEL][TEXT_COLOR_NORMAL]), true);
    return 0;
}

int GuiButton(Rectangle bounds, const char *text)
{
    EnsureStyleLoaded();
    GuiState state = guiState;
    int pressed = 0;

    if (state != STATE_DISABLED && !guiLocked) {
        Vector2 mousePoint = GetMousePosition();
        if (CheckCollisionPointRec(mousePoint, bounds)) {
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                state = STATE_PRESSED;
            } else {
                state = STATE_FOCUSED;
            }
            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                pressed = 1;
            }
        }
    }

    Color base   = GuiGetColor((unsigned int)guiStyle[BUTTON][BASE_COLOR_NORMAL + state * 3]);
    Color border = GuiGetColor((unsigned int)guiStyle[BUTTON][BORDER_COLOR_NORMAL + state * 3]);
    Color txtCol = GuiGetColor((unsigned int)guiStyle[BUTTON][TEXT_COLOR_NORMAL + state * 3]);

    DrawRectangleRounded(bounds, 0.18f, 6, base);
    DrawRectangleRoundedLinesEx(bounds, 0.18f, 6, 1.2f, border);
    GuiDrawTextCentered(text, bounds, txtCol, false);

    return pressed;
}

int GuiSliderBar(Rectangle bounds, const char *textLeft, const char *textRight, float *value, float minValue, float maxValue)
{
    EnsureStyleLoaded();
    GuiState state = guiState;
    Vector2 mousePoint = GetMousePosition();

    if (state != STATE_DISABLED && !guiLocked) {
        if (CheckCollisionPointRec(mousePoint, bounds)) {
            state = STATE_FOCUSED;
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                state = STATE_PRESSED;
                float normalized = (mousePoint.x - bounds.x) / bounds.width;
                if (normalized < 0.0f) normalized = 0.0f;
                if (normalized > 1.0f) normalized = 1.0f;
                *value = minValue + normalized * (maxValue - minValue);
            }
        }
    }

    DrawRectangleRounded(bounds, 0.25f, 4, GuiGetColor(0x18181BFF));
    float pct = (*value - minValue) / (maxValue - minValue);
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;

    Rectangle fill = { bounds.x + 2.0f, bounds.y + 2.0f, (bounds.width - 4.0f) * pct, bounds.height - 4.0f };
    Color fillCol = (state == STATE_FOCUSED || state == STATE_PRESSED)
                    ? GuiGetColor(0xFAFAFAFF)
                    : GuiGetColor(0xA1A1AAFF);
    if (fill.width > 2.0f) {
        DrawRectangleRounded(fill, 0.25f, 4, fillCol);
    }
    DrawRectangleRoundedLinesEx(bounds, 0.25f, 4, 1.0f, GuiGetColor((unsigned int)guiStyle[SLIDER][BORDER_COLOR_NORMAL + state * 3]));

    if (textLeft && textLeft[0] != '\0') {
        float fs = (float)guiStyle[DEFAULT][TEXT_SIZE];
        Vector2 sz = MeasureTextEx(guiFont, textLeft, fs, 1.0f);
        Rectangle lr = { bounds.x - sz.x - 8.0f, bounds.y, sz.x + 4.0f, bounds.height };
        GuiDrawTextCentered(textLeft, lr, GuiGetColor(0xE4E4E7FF), false);
    }
    if (textRight && textRight[0] != '\0') {
        float fs = (float)guiStyle[DEFAULT][TEXT_SIZE];
        Vector2 sz = MeasureTextEx(guiFont, textRight, fs, 1.0f);
        Rectangle rr = { bounds.x + bounds.width + 6.0f, bounds.y, sz.x + 8.0f, bounds.height };
        GuiDrawTextCentered(textRight, rr, GuiGetColor(0xF4F4F5FF), true);
    }
    return 0;
}

int GuiSlider(Rectangle bounds, const char *textLeft, const char *textRight, float *value, float minValue, float maxValue)
{
    return GuiSliderBar(bounds, textLeft, textRight, value, minValue, maxValue);
}

int GuiTextBox(Rectangle bounds, char *text, int textSize, bool editMode)
{
    EnsureStyleLoaded();
    GuiState state = editMode ? STATE_PRESSED : guiState;
    int result = 0;
    Vector2 mousePoint = GetMousePosition();

    if (guiState != STATE_DISABLED && !guiLocked) {
        if (CheckCollisionPointRec(mousePoint, bounds)) {
            if (!editMode) state = STATE_FOCUSED;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                result = 1;
            }
        } else if (editMode && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            result = 1;
        }

        if (editMode) {
            bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
            if (ctrl && IsKeyPressed(KEY_V)) {
                const char *clip = GetClipboardText();
                if (clip && clip[0]) {
                    int len = (int)strlen(text);
                    int clen = (int)strlen(clip);
                    if (len + clen < textSize - 1) {
                        strcat(text, clip);
                    }
                }
            } else {
                int len = (int)strlen(text);
                int key = GetCharPressed();
                while (key > 0) {
                    if (key >= 32 && key < 127 && len < textSize - 1) {
                        text[len] = (char)key;
                        text[len + 1] = '\0';
                        len++;
                    }
                    key = GetCharPressed();
                }
            }
            if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
                int len = (int)strlen(text);
                if (len > 0) text[len - 1] = '\0';
            }
            if (IsKeyPressed(KEY_ENTER)) {
                result = 1;
            }
        }
    }

    DrawRectangleRounded(bounds, 0.15f, 4, GuiGetColor(0x18181BFF));
    Color border = editMode ? GuiGetColor(0xFAFAFAFF) : GuiGetColor((unsigned int)guiStyle[TEXTBOX][BORDER_COLOR_NORMAL + state * 3]);
    DrawRectangleRoundedLinesEx(bounds, 0.15f, 4, editMode ? 1.6f : 1.0f, border);

    if (bounds.width > 6.0f && bounds.height > 6.0f) {
        BeginScissorMode((int)bounds.x + 2, (int)bounds.y + 2, (int)bounds.width - 4, (int)bounds.height - 4);
        GuiDrawTextCentered(text, bounds, GuiGetColor(0xF4F4F5FF), true);
        if (editMode && ((int)(GetTime() * 2.0) % 2 == 0)) {
            float fs = (float)guiStyle[DEFAULT][TEXT_SIZE];
            float sp = (float)guiStyle[DEFAULT][TEXT_SPACING];
            Vector2 sz = MeasureTextEx(guiFont, text, fs, sp);
            float cx = bounds.x + (float)guiStyle[DEFAULT][TEXT_PADDING] + sz.x + 2.0f;
            DrawRectangle((int)cx, (int)(bounds.y + 5.0f), 2, (int)(bounds.height - 10.0f), GuiGetColor(0xFAFAFAFF));
        }
        EndScissorMode();
    }

    return result;
}

int GuiDropdownBox(Rectangle bounds, const char *text, int *active, bool editMode)
{
    EnsureStyleLoaded();
    int result = 0;
    Vector2 mousePoint = GetMousePosition();

    /* Parse ';' separated items (up to 24 presets) */
    char items[24][128];
    int itemCount = 0;
    if (text) {
        const char *start = text;
        const char *p = text;
        while (*p && itemCount < 24) {
            if (*p == ';') {
                size_t len = (size_t)(p - start);
                if (len >= sizeof(items[0])) len = sizeof(items[0]) - 1;
                memcpy(items[itemCount], start, len);
                items[itemCount][len] = '\0';
                itemCount++;
                start = p + 1;
            }
            p++;
        }
        if (itemCount < 24 && *start) {
            size_t len = strlen(start);
            if (len >= sizeof(items[0])) len = sizeof(items[0]) - 1;
            memcpy(items[itemCount], start, len);
            items[itemCount][len] = '\0';
            itemCount++;
        }
    }

    int current = (*active >= 0 && *active < itemCount) ? *active : 0;

    bool hovered = CheckCollisionPointRec(mousePoint, bounds);
    if (guiState != STATE_DISABLED && (!guiLocked || editMode)) {
        if (hovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            result = 1;
        }
    }

    DrawRectangleRounded(bounds, 0.15f, 4, hovered ? GuiGetColor(0x323238FF) : GuiGetColor(0x222226FF));
    DrawRectangleRoundedLinesEx(bounds, 0.15f, 4, 1.2f, (hovered || editMode) ? GuiGetColor(0xFAFAFAFF) : GuiGetColor(0x52525BFF));
    if (itemCount > 0 && bounds.width > 28.0f) {
        BeginScissorMode((int)bounds.x + 2, (int)bounds.y, (int)bounds.width - 26, (int)bounds.height);
        GuiDrawTextCentered(items[current], bounds, GuiGetColor(0xF4F4F5FF), true);
        EndScissorMode();
    }
    /* Dropdown arrow */
    Vector2 v1 = { bounds.x + bounds.width - 18.0f, bounds.y + bounds.height * 0.40f };
    Vector2 v2 = { bounds.x + bounds.width - 8.0f,  bounds.y + bounds.height * 0.40f };
    Vector2 v3 = { bounds.x + bounds.width - 13.0f, bounds.y + bounds.height * 0.66f };
    DrawTriangle(v1, v3, v2, GuiGetColor(0xF4F4F5FF));

    if (editMode && itemCount > 0) {
        /* Draw drop shadow under menu */
        float item_h = 24.0f;
        Rectangle menuBg = {
            bounds.x,
            bounds.y + bounds.height + 2.0f,
            bounds.width,
            (float)itemCount * (item_h + 1.0f) + 2.0f
        };
        DrawRectangleRounded((Rectangle){ menuBg.x + 3.0f, menuBg.y + 4.0f, menuBg.width, menuBg.height },
                             0.04f, 4, (Color){ 0, 0, 0, 155 });
        DrawRectangleRec(menuBg, GuiGetColor(0x161618FF));
        DrawRectangleLinesEx(menuBg, 1.2f, GuiGetColor(0xE4E4E7FF));

        for (int i = 0; i < itemCount; i++) {
            Rectangle itemRect = {
                bounds.x + 2.0f,
                bounds.y + bounds.height + 3.0f + (float)i * (item_h + 1.0f),
                bounds.width - 4.0f,
                item_h
            };
            bool itemHover = CheckCollisionPointRec(mousePoint, itemRect);
            Color bg = itemHover ? GuiGetColor(0x3F3F46FF)
                                 : (i == current ? GuiGetColor(0x2E2E34FF) : GuiGetColor(0x1E1E22FF));
            DrawRectangleRec(itemRect, bg);
            BeginScissorMode((int)itemRect.x + 2, (int)itemRect.y, (int)itemRect.width - 4, (int)itemRect.height);
            GuiDrawTextCentered(items[i], itemRect, itemHover ? GuiGetColor(0xFFFFFFFF) : GuiGetColor(0xE4E4E7FF), true);
            EndScissorMode();
            if (itemHover && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                *active = i;
                result = 1;
            }
        }
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && !hovered && result == 0) {
            result = 1;
        }
    }

    return result;
}

#endif /* RAYGUI_IMPLEMENTATION */
#endif /* RAYGUI_H */
