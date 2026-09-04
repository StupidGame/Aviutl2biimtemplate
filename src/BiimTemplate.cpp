#define NOMINMAX
#include <windows.h>
#include <commdlg.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <functional>
#include <limits>
#include <mutex>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "plugin2.h"
#include "filter2.h"

namespace {

// All non-ASCII literals use universal character names so this source can be
// compiled together with the SDK's Shift-JIS headers without changing them.

auto layout_group = FILTER_ITEM_GROUP(L"\u30ec\u30a4\u30a2\u30a6\u30c8");
auto sidebar_width = FILTER_ITEM_TRACK(L"\u53f3\u6b04\u306e\u5e45 (%)", 25.0, 10.0, 45.0, 0.1);
auto bottom_height = FILTER_ITEM_TRACK(L"\u4e0b\u6b04\u306e\u9ad8\u3055 (%)", 25.0, 10.0, 45.0, 0.1);
auto padding = FILTER_ITEM_TRACK(L"\u4f59\u767d (px)", 20.0, 0.0, 120.0, 1.0);
auto separator_width = FILTER_ITEM_TRACK(L"\u533a\u5207\u308a\u7dda (px)", 4.0, 0.0, 32.0, 1.0);
auto auto_scale = FILTER_ITEM_CHECK(L"720p\u57fa\u6e96\u3067\u62e1\u7e2e", true);

void open_media_manager(EDIT_SECTION* edit);

auto media_group = FILTER_ITEM_GROUP(L"\u52d5\u753b\u30fb\u753b\u50cf");
constexpr wchar_t media_file_filter[] =
    L"Media Files (*.mp4;*.mkv;*.avi;*.mov;*.webm;*.png;*.jpg;*.jpeg;*.bmp;*.webp)\0"
    L"*.mp4;*.mkv;*.avi;*.mov;*.webm;*.png;*.jpg;*.jpeg;*.bmp;*.webp\0"
    L"All Files (*.*)\0*.*\0";
auto place_media_button = FILTER_ITEM_BUTTON(
    L"\u7d20\u6750\u306e\u8ffd\u52a0\u30fb\u7de8\u96c6", open_media_manager);
auto bottom_right_transparent = FILTER_ITEM_CHECK(L"\u53f3\u4e0b\u6b04\u3092\u900f\u904e", false);

auto background_group = FILTER_ITEM_GROUP(L"\u80cc\u666f");
auto sidebar_color = FILTER_ITEM_COLOR(L"\u53f3\u6b04\u306e\u80cc\u666f", 0x20242b);
auto bottom_color = FILTER_ITEM_COLOR(L"\u4e0b\u6b04\u306e\u80cc\u666f", 0x17191f);
auto bottom_right_color = FILTER_ITEM_COLOR(L"\u53f3\u4e0b\u6b04\u306e\u80cc\u666f", 0x111318);
auto fill_game_area = FILTER_ITEM_CHECK(L"\u30b2\u30fc\u30e0\u6b04\u3092\u5857\u308b", false);
auto game_color = FILTER_ITEM_COLOR(L"\u30b2\u30fc\u30e0\u6b04\u306e\u8272", 0x000000);
auto panel_opacity = FILTER_ITEM_TRACK(L"\u80cc\u666f\u306e\u4e0d\u900f\u660e\u5ea6 (%)", 96.0, 0.0, 100.0, 1.0);
auto separator_color = FILTER_ITEM_COLOR(L"\u533a\u5207\u308a\u7dda\u306e\u8272", 0xd84f7b);

auto text_group = FILTER_ITEM_GROUP(L"\u30c6\u30ad\u30b9\u30c8");
auto sidebar_title = FILTER_ITEM_STRING(L"\u53f3\u6b04\u306e\u898b\u51fa\u3057", L"biim\u30b7\u30b9\u30c6\u30e0");
auto sidebar_text = FILTER_ITEM_TEXT(
    L"\u53f3\u6b04\u306e\u672c\u6587",
    L"\u53f3\u6b04\u306e\u30c6\u30ad\u30b9\u30c8\u3092\n\u3053\u3053\u306b\u5165\u529b");
auto speaker_name = FILTER_ITEM_STRING(L"\u8a71\u8005\u540d", L"\u89e3\u8aac");
auto bottom_text = FILTER_ITEM_TEXT(
    L"\u4e0b\u6b04\u306e\u672c\u6587",
    L"\u4e0b\u6b04\u306e\u30c6\u30ad\u30b9\u30c8\u3092\u5165\u529b");
auto font_name = FILTER_ITEM_STRING(L"\u30d5\u30a9\u30f3\u30c8\u540d", L"Yu Gothic UI");
auto title_font_size = FILTER_ITEM_TRACK(L"\u898b\u51fa\u3057\u30b5\u30a4\u30ba", 30.0, 10.0, 144.0, 1.0);
auto sidebar_font_size = FILTER_ITEM_TRACK(L"\u53f3\u6b04\u6587\u5b57\u30b5\u30a4\u30ba", 24.0, 10.0, 144.0, 1.0);
auto speaker_font_size = FILTER_ITEM_TRACK(L"\u8a71\u8005\u30b5\u30a4\u30ba", 24.0, 10.0, 144.0, 1.0);
auto bottom_font_size = FILTER_ITEM_TRACK(L"\u4e0b\u6b04\u6587\u5b57\u30b5\u30a4\u30ba", 32.0, 10.0, 180.0, 1.0);
auto text_color = FILTER_ITEM_COLOR(L"\u6587\u5b57\u8272", 0xffffff);
auto accent_text_color = FILTER_ITEM_COLOR(L"\u5f37\u8abf\u6587\u5b57\u8272", 0xffd866);
auto bold_text = FILTER_ITEM_CHECK(L"\u592a\u5b57", true);
auto outline_width = FILTER_ITEM_TRACK(L"\u7e01\u53d6\u308a (px)", 2.0, 0.0, 8.0, 1.0);
auto outline_color = FILTER_ITEM_COLOR(L"\u7e01\u53d6\u308a\u306e\u8272", 0x000000);

void* filter_items[] = {
    &layout_group,
    &sidebar_width,
    &bottom_height,
    &padding,
    &separator_width,
    &auto_scale,
    &media_group,
    &place_media_button,
    &bottom_right_transparent,
    &background_group,
    &sidebar_color,
    &bottom_color,
    &bottom_right_color,
    &fill_game_area,
    &game_color,
    &panel_opacity,
    &separator_color,
    &text_group,
    &sidebar_title,
    &sidebar_text,
    &speaker_name,
    &bottom_text,
    &font_name,
    &title_font_size,
    &sidebar_font_size,
    &speaker_font_size,
    &bottom_font_size,
    &text_color,
    &accent_text_color,
    &bold_text,
    &outline_width,
    &outline_color,
    nullptr,
};

struct Rgb {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    bool operator==(const Rgb&) const = default;
};

struct Settings {
    double sidebar_percent = 25.0;
    double bottom_percent = 25.0;
    int padding_px = 20;
    int separator_px = 4;
    bool scale_from_720p = true;
    Rgb sidebar_background{};
    Rgb bottom_background{};
    Rgb bottom_right_background{};
    bool fill_game = false;
    Rgb game_background{};
    int background_opacity = 96;
    Rgb separator{};
    bool bottom_right_is_transparent = false;
    std::wstring title;
    std::wstring side_text;
    std::wstring speaker;
    std::wstring caption;
    std::wstring font;
    int title_size = 30;
    int side_size = 24;
    int speaker_size = 24;
    int caption_size = 32;
    Rgb foreground{};
    Rgb accent{};
    bool bold = true;
    int outline_px = 2;
    Rgb outline{};

    bool operator==(const Settings&) const = default;
};

struct RenderCache {
    std::mutex mutex;
    Settings settings;
    int width = 0;
    int height = 0;
    bool valid = false;
    std::vector<PIXEL_RGBA> pixels;
};

int rounded_track(double value) {
    return static_cast<int>(std::lround(value));
}

Rgb read_color(const FILTER_ITEM_COLOR& item) {
    return {item.value.r, item.value.g, item.value.b};
}

std::wstring read_text(LPCWSTR value) {
    return value ? std::wstring(value) : std::wstring();
}

Settings read_settings() {
    Settings result;
    result.sidebar_percent = sidebar_width.value;
    result.bottom_percent = bottom_height.value;
    result.padding_px = rounded_track(padding.value);
    result.separator_px = rounded_track(separator_width.value);
    result.scale_from_720p = auto_scale.value;
    result.sidebar_background = read_color(sidebar_color);
    result.bottom_background = read_color(bottom_color);
    result.bottom_right_background = read_color(bottom_right_color);
    result.fill_game = fill_game_area.value;
    result.game_background = read_color(game_color);
    result.background_opacity = std::clamp(rounded_track(panel_opacity.value), 0, 100);
    result.separator = read_color(separator_color);
    result.bottom_right_is_transparent = bottom_right_transparent.value;
    result.title = read_text(sidebar_title.value);
    result.side_text = read_text(sidebar_text.value);
    result.speaker = read_text(speaker_name.value);
    result.caption = read_text(bottom_text.value);
    result.font = read_text(font_name.value);
    result.title_size = rounded_track(title_font_size.value);
    result.side_size = rounded_track(sidebar_font_size.value);
    result.speaker_size = rounded_track(speaker_font_size.value);
    result.caption_size = rounded_track(bottom_font_size.value);
    result.foreground = read_color(text_color);
    result.accent = read_color(accent_text_color);
    result.bold = bold_text.value;
    result.outline_px = rounded_track(outline_width.value);
    result.outline = read_color(outline_color);
    return result;
}

void blend_pixel(PIXEL_RGBA& destination, const Rgb& source, std::uint8_t source_alpha) {
    if (source_alpha == 0) {
        return;
    }

    const unsigned int destination_alpha = destination.a;
    const unsigned int inverse_alpha = 255U - source_alpha;
    const unsigned int output_alpha = source_alpha + (destination_alpha * inverse_alpha + 127U) / 255U;
    if (output_alpha == 0) {
        destination = {0, 0, 0, 0};
        return;
    }

    auto blend_channel = [&](unsigned int source_channel, unsigned int destination_channel) {
        const unsigned int premultiplied =
            source_channel * source_alpha +
            (destination_channel * destination_alpha * inverse_alpha + 127U) / 255U;
        return static_cast<std::uint8_t>(std::min(255U, (premultiplied + output_alpha / 2U) / output_alpha));
    };

    destination.r = blend_channel(source.r, destination.r);
    destination.g = blend_channel(source.g, destination.g);
    destination.b = blend_channel(source.b, destination.b);
    destination.a = static_cast<std::uint8_t>(output_alpha);
}

void fill_rectangle(
    std::vector<PIXEL_RGBA>& pixels,
    int width,
    int height,
    int left,
    int top,
    int right,
    int bottom,
    const Rgb& color,
    std::uint8_t alpha) {
    left = std::clamp(left, 0, width);
    right = std::clamp(right, 0, width);
    top = std::clamp(top, 0, height);
    bottom = std::clamp(bottom, 0, height);
    if (left >= right || top >= bottom || alpha == 0) {
        return;
    }

    for (int y = top; y < bottom; ++y) {
        PIXEL_RGBA* row = pixels.data() + static_cast<std::size_t>(y) * width;
        for (int x = left; x < right; ++x) {
            blend_pixel(row[x], color, alpha);
        }
    }
}

class TextMask {
public:
    TextMask(int width, int height) : width_(width), height_(height) {
        BITMAPINFO bitmap_info{};
        bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmap_info.bmiHeader.biWidth = width;
        bitmap_info.bmiHeader.biHeight = -height;
        bitmap_info.bmiHeader.biPlanes = 1;
        bitmap_info.bmiHeader.biBitCount = 32;
        bitmap_info.bmiHeader.biCompression = BI_RGB;

        dc_ = CreateCompatibleDC(nullptr);
        if (!dc_) {
            return;
        }
        bitmap_ = CreateDIBSection(dc_, &bitmap_info, DIB_RGB_COLORS, &bits_, nullptr, 0);
        if (!bitmap_ || !bits_) {
            return;
        }
        old_bitmap_ = SelectObject(dc_, bitmap_);
        SetBkMode(dc_, TRANSPARENT);
        SetTextColor(dc_, RGB(255, 255, 255));
    }

    ~TextMask() {
        if (dc_ && old_bitmap_) {
            SelectObject(dc_, old_bitmap_);
        }
        if (bitmap_) {
            DeleteObject(bitmap_);
        }
        if (dc_) {
            DeleteDC(dc_);
        }
    }

    TextMask(const TextMask&) = delete;
    TextMask& operator=(const TextMask&) = delete;

    bool ready() const {
        return dc_ && bitmap_ && bits_;
    }

    void clear() {
        if (bits_) {
            std::memset(bits_, 0, static_cast<std::size_t>(width_) * height_ * 4U);
        }
    }

    void draw(const std::wstring& text, RECT rectangle, HFONT font, UINT flags) {
        if (!ready() || text.empty() || !font) {
            return;
        }
        HGDIOBJ old_font = SelectObject(dc_, font);
        DrawTextW(dc_, text.c_str(), static_cast<int>(text.size()), &rectangle, flags);
        SelectObject(dc_, old_font);
    }

    void blend_into(std::vector<PIXEL_RGBA>& pixels, const Rgb& color) {
        if (!ready()) {
            return;
        }
        GdiFlush();
        const auto* bytes = static_cast<const std::uint8_t*>(bits_);
        const std::size_t pixel_count = static_cast<std::size_t>(width_) * height_;
        for (std::size_t index = 0; index < pixel_count; ++index) {
            const std::uint8_t blue = bytes[index * 4U + 0U];
            const std::uint8_t green = bytes[index * 4U + 1U];
            const std::uint8_t red = bytes[index * 4U + 2U];
            const std::uint8_t coverage = std::max({red, green, blue});
            blend_pixel(pixels[index], color, coverage);
        }
    }

private:
    int width_ = 0;
    int height_ = 0;
    HDC dc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ old_bitmap_ = nullptr;
    void* bits_ = nullptr;
};

HFONT create_font(const Settings& settings, int pixel_height) {
    const wchar_t* face = settings.font.empty() ? L"Yu Gothic UI" : settings.font.c_str();
    return CreateFontW(
        -std::max(1, pixel_height),
        0,
        0,
        0,
        settings.bold ? FW_BOLD : FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        face);
}

void draw_text_block(
    TextMask& mask,
    std::vector<PIXEL_RGBA>& pixels,
    const Settings& settings,
    const std::wstring& text,
    const RECT& rectangle,
    int font_size,
    int outline,
    const Rgb& color,
    UINT flags) {
    if (text.empty() || rectangle.left >= rectangle.right || rectangle.top >= rectangle.bottom) {
        return;
    }

    HFONT font = create_font(settings, font_size);
    if (!font) {
        return;
    }

    if (outline > 0) {
        mask.clear();
        const int squared_radius = outline * outline;
        for (int y = -outline; y <= outline; ++y) {
            for (int x = -outline; x <= outline; ++x) {
                if (x * x + y * y > squared_radius) {
                    continue;
                }
                RECT shifted = rectangle;
                OffsetRect(&shifted, x, y);
                mask.draw(text, shifted, font, flags);
            }
        }
        mask.blend_into(pixels, settings.outline);
    }

    mask.clear();
    mask.draw(text, rectangle, font, flags);
    mask.blend_into(pixels, color);
    DeleteObject(font);
}

bool render_template(RenderCache& cache, const Settings& settings, int width, int height) {
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384) {
        return false;
    }

    const std::size_t pixel_count = static_cast<std::size_t>(width) * height;
    if (pixel_count > static_cast<std::size_t>(16384) * 16384U) {
        return false;
    }

    try {
        cache.pixels.assign(pixel_count, PIXEL_RGBA{0, 0, 0, 0});
    } catch (const std::bad_alloc&) {
        return false;
    }

    const double scale = settings.scale_from_720p
        ? std::clamp(static_cast<double>(height) / 720.0, 0.25, 8.0)
        : 1.0;
    const int scaled_padding = std::max(0, static_cast<int>(std::lround(settings.padding_px * scale)));
    const int scaled_separator = std::max(0, static_cast<int>(std::lround(settings.separator_px * scale)));
    const int scaled_outline = std::max(0, static_cast<int>(std::lround(settings.outline_px * scale)));
    const auto scale_font = [scale](int size) {
        return std::max(1, static_cast<int>(std::lround(size * scale)));
    };

    const int sidebar_pixels = std::clamp(
        static_cast<int>(std::lround(width * settings.sidebar_percent / 100.0)), 1, width);
    const int bottom_pixels = std::clamp(
        static_cast<int>(std::lround(height * settings.bottom_percent / 100.0)), 1, height);
    const int game_right = width - sidebar_pixels;
    const int game_bottom = height - bottom_pixels;
    const std::uint8_t panel_alpha = static_cast<std::uint8_t>(
        std::clamp(settings.background_opacity * 255 / 100, 0, 255));

    if (settings.fill_game) {
        fill_rectangle(cache.pixels, width, height, 0, 0, game_right, game_bottom,
                       settings.game_background, panel_alpha);
    }
    fill_rectangle(cache.pixels, width, height, game_right, 0, width, game_bottom,
                   settings.sidebar_background, panel_alpha);
    fill_rectangle(cache.pixels, width, height, 0, game_bottom, game_right, height,
                   settings.bottom_background, panel_alpha);
    if (!settings.bottom_right_is_transparent) {
        fill_rectangle(cache.pixels, width, height, game_right, game_bottom, width, height,
                       settings.bottom_right_background, panel_alpha);
    }

    if (scaled_separator > 0) {
        const int left_half = scaled_separator / 2;
        const int right_half = scaled_separator - left_half;
        fill_rectangle(cache.pixels, width, height,
                       game_right - left_half, 0, game_right + right_half, height,
                       settings.separator, 255);
        fill_rectangle(cache.pixels, width, height,
                       0, game_bottom - left_half, game_right, game_bottom + right_half,
                       settings.separator, 255);
    }

    TextMask mask(width, height);
    if (!mask.ready()) {
        return false;
    }

    const int title_size = scale_font(settings.title_size);
    const int side_size = scale_font(settings.side_size);
    const int speaker_size = scale_font(settings.speaker_size);
    const int caption_size = scale_font(settings.caption_size);

    const int sidebar_left = std::clamp(game_right + scaled_padding, 0, width);
    const int sidebar_right = std::clamp(width - scaled_padding, 0, width);
    const int sidebar_top = std::clamp(scaled_padding, 0, height);
    const int sidebar_bottom = std::clamp(game_bottom - scaled_padding, 0, height);
    const int title_height = std::max(title_size + scaled_padding / 2,
                                      static_cast<int>(std::lround(title_size * 1.55)));
    const int title_bottom = std::min(sidebar_bottom, sidebar_top + title_height);

    RECT title_rect{sidebar_left, sidebar_top, sidebar_right, title_bottom};
    draw_text_block(mask, cache.pixels, settings, settings.title, title_rect,
                    title_size, scaled_outline, settings.accent,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    const int inner_rule_top = std::min(sidebar_bottom, title_bottom + scaled_padding / 4);
    const int inner_rule_height = std::max(1, scaled_separator / 2);
    if (scaled_separator > 0) {
        fill_rectangle(cache.pixels, width, height,
                       sidebar_left, inner_rule_top, sidebar_right, inner_rule_top + inner_rule_height,
                       settings.separator, 255);
    }

    RECT side_rect{
        sidebar_left,
        std::min(sidebar_bottom, inner_rule_top + inner_rule_height + scaled_padding / 2),
        sidebar_right,
        sidebar_bottom,
    };
    draw_text_block(mask, cache.pixels, settings, settings.side_text, side_rect,
                    side_size, scaled_outline, settings.foreground,
                    DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX);

    const int bottom_left = std::clamp(scaled_padding, 0, game_right);
    const int bottom_right = std::clamp(game_right - scaled_padding, 0, game_right);
    const int bottom_top = std::clamp(game_bottom + scaled_padding, 0, height);
    const int bottom_limit = std::clamp(height - scaled_padding, 0, height);
    const int speaker_height = std::max(speaker_size + scaled_padding / 4,
                                        static_cast<int>(std::lround(speaker_size * 1.35)));
    const int speaker_bottom = std::min(bottom_limit, bottom_top + speaker_height);

    RECT speaker_rect{bottom_left, bottom_top, bottom_right, speaker_bottom};
    draw_text_block(mask, cache.pixels, settings, settings.speaker, speaker_rect,
                    speaker_size, scaled_outline, settings.accent,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    RECT caption_rect{
        bottom_left,
        std::min(bottom_limit, speaker_bottom + scaled_padding / 4),
        bottom_right,
        bottom_limit,
    };
    draw_text_block(mask, cache.pixels, settings, settings.caption, caption_rect,
                    caption_size, scaled_outline, settings.foreground,
                    DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX);

    cache.settings = settings;
    cache.width = width;
    cache.height = height;
    cache.valid = true;
    return true;
}

struct MediaTarget {
    double left = 0.0;
    double top = 0.0;
    double right = 0.0;
    double bottom = 0.0;
};

std::string format_number(double value) {
    char buffer[64]{};
    const auto result = std::to_chars(
        std::begin(buffer), std::end(buffer) - 1, value, std::chars_format::fixed, 4);
    if (result.ec != std::errc{}) {
        return "0";
    }
    return std::string(buffer, result.ptr);
}

bool set_standard_drawing_value(
    EDIT_SECTION* edit,
    OBJECT_HANDLE object,
    LPCWSTR item,
    double value) {
    if (!edit || !edit->set_object_item_value || !object) {
        return false;
    }
    const std::string formatted = format_number(value);
    return edit->set_object_item_value(
        object,
        L"\u6a19\u6e96\u63cf\u753b",
        item,
        formatted.c_str());
}

bool effect_has_item(EDIT_SECTION* edit, EFFECT_HANDLE effect, LPCWSTR item) {
    return edit && edit->get_effect_item_value && effect && item &&
           edit->get_effect_item_value(effect, item) != nullptr;
}

bool set_effect_value(
    EDIT_SECTION* edit,
    EFFECT_HANDLE effect,
    LPCWSTR item,
    double value) {
    if (!edit || !edit->set_effect_item_value || !effect || !item) {
        return false;
    }
    const std::string formatted = format_number(value);
    return edit->set_effect_item_value(effect, item, formatted.c_str());
}

bool fit_with_effect_handle(
    EDIT_SECTION* edit,
    OBJECT_HANDLE object,
    double center_x,
    double center_y,
    double scale_percent) {
    if (!edit || !edit->get_effect_list || !edit->get_effect_item_value ||
        !edit->set_effect_item_value) {
        return false;
    }

    const int effect_count = edit->get_effect_list(object, nullptr, 0);
    if (effect_count <= 0 || effect_count > 1024) {
        return false;
    }

    std::vector<EFFECT_HANDLE> effects(static_cast<std::size_t>(effect_count));
    const int returned = edit->get_effect_list(object, effects.data(), effect_count);
    for (int index = 0; index < returned; ++index) {
        const EFFECT_HANDLE effect = effects[static_cast<std::size_t>(index)];
        if (!effect_has_item(edit, effect, L"X") ||
            !effect_has_item(edit, effect, L"Y")) {
            continue;
        }

        if (effect_has_item(edit, effect, L"\u62e1\u5927\u7387")) {
            return set_effect_value(edit, effect, L"X", center_x) &&
                   set_effect_value(edit, effect, L"Y", center_y) &&
                   set_effect_value(edit, effect, L"\u62e1\u5927\u7387", scale_percent);
        }

        // Some output effects expose scale as separate X/Y tracks.
        if (effect_has_item(edit, effect, L"\u62e1\u5927\u7387X") &&
            effect_has_item(edit, effect, L"\u62e1\u5927\u7387Y")) {
            return set_effect_value(edit, effect, L"X", center_x) &&
                   set_effect_value(edit, effect, L"Y", center_y) &&
                   set_effect_value(edit, effect, L"\u62e1\u5927\u7387X", scale_percent) &&
                   set_effect_value(edit, effect, L"\u62e1\u5927\u7387Y", scale_percent);
        }
    }
    return false;
}

bool fit_media_object(
    EDIT_SECTION* edit,
    OBJECT_HANDLE object,
    const MEDIA_INFO& media,
    const MediaTarget& target,
    int scene_width,
    int scene_height) {
    const double target_width = target.right - target.left;
    const double target_height = target.bottom - target.top;
    if (media.width <= 0 || media.height <= 0 || target_width <= 0.0 || target_height <= 0.0) {
        return false;
    }

    const double scale = std::min(
        target_width / static_cast<double>(media.width),
        target_height / static_cast<double>(media.height));
    const double center_x = (target.left + target.right) * 0.5 - scene_width * 0.5;
    const double center_y = (target.top + target.bottom) * 0.5 - scene_height * 0.5;

    const double scale_percent = scale * 100.0;
    if (fit_with_effect_handle(edit, object, center_x, center_y, scale_percent)) {
        return true;
    }

    const bool x_set = set_standard_drawing_value(edit, object, L"X", center_x);
    const bool y_set = set_standard_drawing_value(edit, object, L"Y", center_y);
    const bool scale_set = set_standard_drawing_value(
        edit, object, L"\u62e1\u5927\u7387", scale_percent);
    return x_set && y_set && scale_set;
}

bool layer_is_empty(
    EDIT_SECTION* edit,
    int layer,
    int start_frame,
    int end_frame) {
    if (!edit || !edit->find_object || layer < 0 || start_frame > end_frame) {
        return false;
    }
    if (edit->get_layer_lock && edit->get_layer_lock(layer)) {
        return false;
    }
    if (edit->get_layer_enable && !edit->get_layer_enable(layer)) {
        return false;
    }

    int frame = start_frame;
    while (true) {
        if (edit->find_object(layer, frame)) {
            return false;
        }
        if (frame >= end_frame) {
            break;
        }
        ++frame;
    }
    return true;
}

bool prepare_media_layers(
    EDIT_SECTION* edit,
    OBJECT_HANDLE template_object,
    const OBJECT_LAYER_FRAME& template_range,
    int media_count,
    std::vector<int>& media_layers) {
    media_layers.clear();
    if (!edit || !edit->info || !template_object || media_count <= 0 ||
        !edit->find_object) {
        return false;
    }

    // Smaller layer indices are drawn first in AviUtl2. Media must therefore
    // be above the template in the layer editor so the frame is drawn last.
    for (int layer = template_range.layer - 1;
         layer >= 0 && static_cast<int>(media_layers.size()) < media_count;
         --layer) {
        if (layer_is_empty(edit, layer, template_range.start, template_range.end)) {
            media_layers.push_back(layer);
        }
    }
    if (static_cast<int>(media_layers.size()) == media_count) {
        return true;
    }

    // If the template is already at the top, move only the template to new
    // empty layers at the bottom. Its former layer and the inserted gap then
    // become safe background layers for the media.
    if (!edit->move_object) {
        return false;
    }
    const int missing = media_count - static_cast<int>(media_layers.size());
    if (edit->info->layer_max > (std::numeric_limits<int>::max)() - missing) {
        return false;
    }
    const int old_layer_max = edit->info->layer_max;
    const int destination_layer = old_layer_max + missing;
    if (!edit->move_object(template_object, destination_layer, template_range.start)) {
        return false;
    }

    media_layers.push_back(template_range.layer);
    for (int layer = old_layer_max + 1;
         layer < destination_layer && static_cast<int>(media_layers.size()) < media_count;
         ++layer) {
        media_layers.push_back(layer);
    }
    std::sort(media_layers.begin(), media_layers.end(), std::greater<int>());
    return static_cast<int>(media_layers.size()) == media_count;
}

bool create_media_object(
    EDIT_SECTION* edit,
    LPCWSTR path,
    int layer,
    int frame,
    int length,
    const MediaTarget& target,
    LPCWSTR object_name,
    LPCWSTR layer_name) {
    if (!path || !*path || !edit || !edit->get_media_info ||
        !edit->create_object_from_media_file || !edit->info) {
        return false;
    }

    MEDIA_INFO media{};
    if ((edit->is_support_media_file && !edit->is_support_media_file(path, true)) ||
        !edit->get_media_info(path, &media, sizeof(media)) ||
        media.width <= 0 || media.height <= 0) {
        return false;
    }

    OBJECT_HANDLE object = edit->create_object_from_media_file(path, layer, frame, length);
    if (!object) {
        return false;
    }

    if (edit->set_object_name) {
        edit->set_object_name(object, object_name);
    }
    if (edit->set_layer_name) {
        edit->set_layer_name(layer, layer_name);
    }

    return fit_media_object(
        edit, object, media, target, edit->info->width, edit->info->height);
}

void show_media_message(LPCWSTR message) {
    MessageBoxW(
        GetActiveWindow(),
        message,
        L"biim\u30c6\u30f3\u30d7\u30ec\u30fc\u30c8",
        MB_OK | MB_ICONINFORMATION);
}



std::vector<std::wstring> open_media_file_dialog() {
    std::vector<wchar_t> buffer(65536, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrFilter = media_file_filter;
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST |
                   OFN_PATHMUSTEXIST | OFN_DONTADDTORECENT;
    dialog.lpstrTitle = L"\u52d5\u753b\u30fb\u753b\u50cf\u3092\u8907\u6570\u9078\u629e";
    if (!GetOpenFileNameW(&dialog)) {
        return {};
    }

    std::vector<std::wstring> files;
    const std::wstring first(buffer.data());
    const wchar_t* cursor = buffer.data() + first.size() + 1;
    if (*cursor == L'\0') {
        files.push_back(first);
        return files;
    }

    while (*cursor != L'\0') {
        std::wstring path = first;
        if (!path.empty() && path.back() != L'\\' && path.back() != L'/') {
            path.push_back(L'\\');
        }
        path.append(cursor);
        files.push_back(std::move(path));
        cursor += std::wcslen(cursor) + 1;
    }
    return files;
}

std::wstring extract_file_path_from_alias(LPCSTR alias) {
    if (!alias) {
        return {};
    }
    std::string text(alias);
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t next_line = text.find_first_of("\r\n", pos);
        if (next_line == std::string::npos) {
            next_line = text.size();
        }
        std::string line = text.substr(pos, next_line - pos);
        if (line.rfind("file=", 0) == 0) {
            std::string path_utf8 = line.substr(5);
            if (!path_utf8.empty() && (path_utf8.front() == '"' || path_utf8.front() == '\'')) {
                path_utf8 = path_utf8.substr(1);
            }
            if (!path_utf8.empty() && (path_utf8.back() == '"' || path_utf8.back() == '\'')) {
                path_utf8.pop_back();
            }
            if (!path_utf8.empty()) {
                const int length = MultiByteToWideChar(CP_UTF8, 0, path_utf8.c_str(), -1, nullptr, 0);
                if (length > 1) {
                    std::wstring wide(static_cast<std::size_t>(length) - 1, L'\0');
                    MultiByteToWideChar(CP_UTF8, 0, path_utf8.c_str(), -1, wide.data(), length);
                    return wide;
                }
            }
        }
        pos = text.find_first_not_of("\r\n", next_line);
    }
    return {};
}

std::wstring get_media_object_path(EDIT_SECTION* edit, OBJECT_HANDLE object) {
    if (!edit || !object) {
        return {};
    }
    if (edit->get_object_alias) {
        LPCSTR alias = edit->get_object_alias(object);
        if (alias) {
            std::wstring path = extract_file_path_from_alias(alias);
            if (!path.empty()) {
                return path;
            }
        }
    }
    if (edit->get_object_item_value) {
        const LPCWSTR effects[] = {
            L"\u52d5\u753b\u30d5\u30a1\u30a4\u30eb",
            L"\u753b\u50cf\u30d5\u30a1\u30a4\u30eb",
            nullptr
        };
        const LPCWSTR items[] = {
            L"file",
            L"\u30d5\u30a1\u30a4\u30eb",
            nullptr
        };
        for (int e = 0; effects[e]; ++e) {
            for (int i = 0; items[i]; ++i) {
                LPCSTR value = edit->get_object_item_value(object, effects[e], items[i]);
                if (value && *value) {
                    const int length = MultiByteToWideChar(CP_UTF8, 0, value, -1, nullptr, 0);
                    if (length > 1) {
                        std::wstring wide(static_cast<std::size_t>(length) - 1, L'\0');
                        MultiByteToWideChar(CP_UTF8, 0, value, -1, wide.data(), length);
                        return wide;
                    }
                }
            }
        }
    }
    return {};
}

constexpr int dest_game_radio_id = 2110;
constexpr int dest_bottom_right_radio_id = 2111;
constexpr int order_list_id = 2101;
constexpr int order_add_id = 2107;
constexpr int order_change_file_id = 2108;
constexpr int order_move_dest_id = 2109;
constexpr int order_up_id = 2102;
constexpr int order_down_id = 2103;
constexpr int order_remove_id = 2104;
constexpr int order_length_edit_id = 2112;
constexpr int order_change_length_id = 2113;
constexpr int order_total_label_id = 2114;
constexpr int order_accept_id = 2105;
constexpr int order_cancel_id = 2106;

struct MediaOrderItem {
    OBJECT_HANDLE handle = nullptr;
    std::wstring path;
    int length = 1;
};

enum class MediaDestination {
    game,
    bottom_right,
};

struct UnifiedOrderDialogState {
    EDIT_SECTION* edit = nullptr;
    OBJECT_HANDLE template_object = nullptr;
    MediaDestination current_destination = MediaDestination::game;

    std::vector<MediaOrderItem> game_items;
    std::vector<MediaOrderItem> initial_game_items;
    int game_target_layer = -1;
    bool game_modified = false;

    std::vector<MediaOrderItem> bottom_right_items;
    std::vector<MediaOrderItem> initial_bottom_right_items;
    int bottom_right_target_layer = -1;
    bool bottom_right_modified = false;

    bool accepted = false;
};

std::wstring media_order_item_display_name(const MediaOrderItem& item, std::size_t index) {
    std::wstring filename;
    if (!item.path.empty()) {
        const std::size_t separator = item.path.find_last_of(L"\\/");
        filename = (separator == std::wstring::npos)
            ? item.path
            : item.path.substr(separator + 1);
    } else {
        filename = L"\u914d\u7f6e\u6e08\u307f\u7d20\u6750";
    }
    return std::to_wstring(index + 1) + L". " + filename + L" (" +
           std::to_wstring(item.length) + L"F)";
}

int media_sequence_length(const EDIT_INFO& info, const MEDIA_INFO& media) {
    const double frames_per_second = info.rate > 0 && info.scale > 0
        ? static_cast<double>(info.rate) / static_cast<double>(info.scale)
        : 30.0;
    const double seconds = media.total_time > 0.0 ? media.total_time : 5.0;
    const double frames = std::ceil(seconds * frames_per_second);
    return static_cast<int>(std::clamp(
        frames,
        1.0,
        static_cast<double>((std::numeric_limits<int>::max)() / 4)));
}

std::vector<MediaOrderItem>& get_current_items(UnifiedOrderDialogState* state) {
    return state->current_destination == MediaDestination::game
        ? state->game_items
        : state->bottom_right_items;
}

void mark_modified(UnifiedOrderDialogState* state) {
    if (state->current_destination == MediaDestination::game) {
        state->game_modified = true;
    } else {
        state->bottom_right_modified = true;
    }
}

void refresh_order_list(HWND window, UnifiedOrderDialogState* state, int selection) {
    if (!window || !state) {
        return;
    }
    const HWND list = GetDlgItem(window, order_list_id);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    const auto& items = get_current_items(state);
    for (std::size_t index = 0; index < items.size(); ++index) {
        const std::wstring label = media_order_item_display_name(items[index], index);
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }
    if (!items.empty()) {
        selection = std::clamp(selection, 0, static_cast<int>(items.size()) - 1);
        SendMessageW(list, LB_SETCURSEL, selection, 0);
        SetDlgItemInt(
            window,
            order_length_edit_id,
            static_cast<UINT>(items[static_cast<std::size_t>(selection)].length),
            FALSE);
    } else {
        SetDlgItemTextW(window, order_length_edit_id, L"");
    }

    int64_t total_length = 0;
    for (const auto& item : items) {
        total_length += item.length;
    }
    const std::wstring total_str = L"\u5408\u8a08: " + std::to_wstring(total_length) + L"F";
    SetDlgItemTextW(window, order_total_label_id, total_str.c_str());
}

void apply_length_edit(HWND window, UnifiedOrderDialogState* state) {
    if (!window || !state) {
        return;
    }
    auto& items = get_current_items(state);
    const HWND list = GetDlgItem(window, order_list_id);
    const int selected = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));
    if (selected >= 0 && selected < static_cast<int>(items.size())) {
        BOOL success = FALSE;
        const UINT val = GetDlgItemInt(window, order_length_edit_id, &success, FALSE);
        if (success && val > 0 && static_cast<int>(val) != items[static_cast<std::size_t>(selected)].length) {
            items[static_cast<std::size_t>(selected)].length = static_cast<int>(val);
            mark_modified(state);
            refresh_order_list(window, state, selected);
        }
    }
}

void set_control_font(HWND control) {
    if (control) {
        SendMessageW(control, WM_SETFONT,
                     reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    }
}

LRESULT CALLBACK unified_order_dialog_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<UnifiedOrderDialogState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        state = static_cast<UnifiedOrderDialogState*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }

    switch (message) {
        case WM_CREATE: {
            set_control_font(CreateWindowExW(
                0, L"STATIC", L"\u914d\u7f6e\u5148:",
                WS_CHILD | WS_VISIBLE,
                14, 14, 55, 20, window,
                nullptr, GetModuleHandleW(nullptr), nullptr));
            set_control_font(CreateWindowExW(
                0, L"BUTTON", L"\u30b2\u30fc\u30e0\u6b04",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON | WS_GROUP,
                72, 12, 85, 24, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(dest_game_radio_id)),
                GetModuleHandleW(nullptr), nullptr));
            set_control_font(CreateWindowExW(
                0, L"BUTTON", L"\u53f3\u4e0b\u6b04",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON,
                162, 12, 85, 24, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(dest_bottom_right_radio_id)),
                GetModuleHandleW(nullptr), nullptr));

            CheckRadioButton(
                window, dest_game_radio_id, dest_bottom_right_radio_id,
                state->current_destination == MediaDestination::game
                    ? dest_game_radio_id
                    : dest_bottom_right_radio_id);

            set_control_font(CreateWindowExW(
                WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY,
                12, 42, 430, 290, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(order_list_id)), GetModuleHandleW(nullptr), nullptr));

            set_control_font(CreateWindowExW(
                0, L"BUTTON", L"\u7d20\u6750\u3092\u8ffd\u52a0",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                454, 42, 110, 30, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(order_add_id)), GetModuleHandleW(nullptr), nullptr));
            set_control_font(CreateWindowExW(
                0, L"BUTTON", L"\u30d5\u30a1\u30a4\u30eb\u3092\u5909\u66f4",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                454, 78, 110, 30, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(order_change_file_id)), GetModuleHandleW(nullptr), nullptr));
            set_control_font(CreateWindowExW(
                0, L"BUTTON", L"\u914d\u7f6e\u5148\u3092\u5909\u66f4",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                454, 114, 110, 30, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(order_move_dest_id)), GetModuleHandleW(nullptr), nullptr));
            set_control_font(CreateWindowExW(
                0, L"BUTTON", L"\u4e0a\u3078",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                454, 150, 110, 30, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(order_up_id)), GetModuleHandleW(nullptr), nullptr));
            set_control_font(CreateWindowExW(
                0, L"BUTTON", L"\u4e0b\u3078",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                454, 186, 110, 30, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(order_down_id)), GetModuleHandleW(nullptr), nullptr));
            set_control_font(CreateWindowExW(
                0, L"BUTTON", L"\u4e00\u89a7\u304b\u3089\u5916\u3059",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                454, 222, 110, 30, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(order_remove_id)), GetModuleHandleW(nullptr), nullptr));

            set_control_font(CreateWindowExW(
                0, L"STATIC", L"\u9078\u629e\u7d20\u6750\u306e\u9577\u3055:",
                WS_CHILD | WS_VISIBLE,
                14, 345, 110, 20, window,
                nullptr, GetModuleHandleW(nullptr), nullptr));
            set_control_font(CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL,
                128, 342, 65, 24, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(order_length_edit_id)), GetModuleHandleW(nullptr), nullptr));
            set_control_font(CreateWindowExW(
                0, L"STATIC", L"\u30d5\u30ec\u30fc\u30e0",
                WS_CHILD | WS_VISIBLE,
                198, 345, 60, 20, window,
                nullptr, GetModuleHandleW(nullptr), nullptr));
            set_control_font(CreateWindowExW(
                0, L"BUTTON", L"\u9577\u3055\u3092\u9069\u7528",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                264, 340, 95, 28, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(order_change_length_id)), GetModuleHandleW(nullptr), nullptr));

            set_control_font(CreateWindowExW(
                0, L"STATIC", L"\u5408\u8a08: 0F",
                WS_CHILD | WS_VISIBLE,
                14, 395, 250, 20, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(order_total_label_id)), GetModuleHandleW(nullptr), nullptr));

            set_control_font(CreateWindowExW(
                0, L"BUTTON", L"\u914d\u7f6e",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                342, 388, 105, 34, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(order_accept_id)), GetModuleHandleW(nullptr), nullptr));
            set_control_font(CreateWindowExW(
                0, L"BUTTON", L"\u30ad\u30e3\u30f3\u30bb\u30eb",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                454, 388, 110, 34, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(order_cancel_id)), GetModuleHandleW(nullptr), nullptr));

            refresh_order_list(window, state, 0);
            return 0;
        }
        case WM_COMMAND: {
            if (!state) {
                break;
            }
            const int command = LOWORD(wparam);
            if (command == dest_game_radio_id) {
                if (state->current_destination != MediaDestination::game) {
                    state->current_destination = MediaDestination::game;
                    refresh_order_list(window, state, 0);
                }
                return 0;
            }
            if (command == dest_bottom_right_radio_id) {
                if (state->current_destination != MediaDestination::bottom_right) {
                    state->current_destination = MediaDestination::bottom_right;
                    refresh_order_list(window, state, 0);
                }
                return 0;
            }

            auto& items = get_current_items(state);
            const HWND list = GetDlgItem(window, order_list_id);
            const int selected = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));

            if (command == order_list_id && HIWORD(wparam) == LBN_SELCHANGE) {
                if (selected >= 0 && selected < static_cast<int>(items.size())) {
                    SetDlgItemInt(
                        window,
                        order_length_edit_id,
                        static_cast<UINT>(items[static_cast<std::size_t>(selected)].length),
                        FALSE);
                }
                return 0;
            }

            if (command == order_change_length_id) {
                apply_length_edit(window, state);
                return 0;
            }

            if (command == order_add_id) {
                std::vector<std::wstring> added_files = open_media_file_dialog();
                if (!added_files.empty()) {
                    int insert_pos = (selected >= 0 && selected < static_cast<int>(items.size()))
                        ? selected + 1
                        : static_cast<int>(items.size());
                    for (const auto& path : added_files) {
                        int item_len = 150;
                        if (state->edit && state->edit->info) {
                            MEDIA_INFO media{};
                            if (state->edit->get_media_info &&
                                state->edit->get_media_info(path.c_str(), &media, sizeof(media)) &&
                                media.width > 0 && media.height > 0) {
                                item_len = media_sequence_length(*state->edit->info, media);
                            }
                        }
                        MediaOrderItem new_item{};
                        new_item.handle = nullptr;
                        new_item.path = path;
                        new_item.length = item_len;
                        items.insert(items.begin() + insert_pos, std::move(new_item));
                        insert_pos++;
                    }
                    mark_modified(state);
                    refresh_order_list(window, state, insert_pos - 1);
                }
                return 0;
            }
            if (command == order_change_file_id && selected >= 0 && selected < static_cast<int>(items.size())) {
                std::vector<std::wstring> chosen = open_media_file_dialog();
                if (!chosen.empty()) {
                    auto& item = items[static_cast<std::size_t>(selected)];
                    item.path = chosen.front();
                    if (item.handle && state->edit && state->edit->delete_object) {
                        state->edit->delete_object(item.handle);
                    }
                    item.handle = nullptr;
                    if (state->edit && state->edit->info) {
                        MEDIA_INFO media{};
                        if (state->edit->get_media_info &&
                            state->edit->get_media_info(item.path.c_str(), &media, sizeof(media)) &&
                            media.width > 0 && media.height > 0) {
                            item.length = media_sequence_length(*state->edit->info, media);
                        }
                    }
                    mark_modified(state);
                    refresh_order_list(window, state, selected);
                }
                return 0;
            }
            if (command == order_move_dest_id && selected >= 0 && selected < static_cast<int>(items.size())) {
                MediaOrderItem item = std::move(items[static_cast<std::size_t>(selected)]);
                items.erase(items.begin() + selected);
                if (item.handle && state->edit && state->edit->delete_object) {
                    state->edit->delete_object(item.handle);
                }
                item.handle = nullptr;
                if (state->current_destination == MediaDestination::game) {
                    state->bottom_right_items.push_back(std::move(item));
                } else {
                    state->game_items.push_back(std::move(item));
                }
                state->game_modified = true;
                state->bottom_right_modified = true;
                refresh_order_list(window, state, selected);
                return 0;
            }
            if (command == order_up_id && selected > 0) {
                std::swap(items[static_cast<std::size_t>(selected)],
                          items[static_cast<std::size_t>(selected - 1)]);
                mark_modified(state);
                refresh_order_list(window, state, selected - 1);
                return 0;
            }
            if (command == order_down_id && selected >= 0 &&
                selected + 1 < static_cast<int>(items.size())) {
                std::swap(items[static_cast<std::size_t>(selected)],
                          items[static_cast<std::size_t>(selected + 1)]);
                mark_modified(state);
                refresh_order_list(window, state, selected + 1);
                return 0;
            }
            if (command == order_remove_id && selected >= 0 &&
                selected < static_cast<int>(items.size())) {
                items.erase(items.begin() + selected);
                mark_modified(state);
                refresh_order_list(window, state, selected);
                return 0;
            }
            if (command == order_accept_id) {
                apply_length_edit(window, state);
                state->accepted = true;
                DestroyWindow(window);
                return 0;
            }
            if (command == order_cancel_id) {
                state->accepted = false;
                DestroyWindow(window);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            state->accepted = false;
            DestroyWindow(window);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

bool calculate_media_targets(
    int scene_width,
    int scene_height,
    MediaTarget& game_target,
    MediaTarget& bottom_right_target) {
    if (scene_width <= 0 || scene_height <= 0) {
        return false;
    }

    const int game_right = scene_width - std::clamp(
        static_cast<int>(std::lround(scene_width * sidebar_width.value / 100.0)), 1, scene_width);
    const int game_bottom = scene_height - std::clamp(
        static_cast<int>(std::lround(scene_height * bottom_height.value / 100.0)), 1, scene_height);
    const double resolution_scale = auto_scale.value
        ? std::clamp(static_cast<double>(scene_height) / 720.0, 0.25, 8.0)
        : 1.0;
    const int line_width = std::max(
        0, static_cast<int>(std::lround(separator_width.value * resolution_scale)));
    const int left_half = line_width / 2;
    const int right_half = line_width - left_half;

    game_target = {
        0.0,
        0.0,
        static_cast<double>(std::max(1, game_right - left_half)),
        static_cast<double>(std::max(1, game_bottom - left_half)),
    };
    bottom_right_target = {
        static_cast<double>(std::min(scene_width, game_right + right_half)),
        static_cast<double>(std::min(scene_height, game_bottom + right_half)),
        static_cast<double>(scene_width),
        static_cast<double>(scene_height),
    };
    return true;
}

std::vector<MediaOrderItem> collect_placed_media_items(
    EDIT_SECTION* edit,
    OBJECT_HANDLE template_object,
    MediaDestination destination,
    int& out_target_layer) {
    out_target_layer = -1;
    if (!edit || !edit->info || !template_object || !edit->get_object_layer_frame || !edit->find_object) {
        return {};
    }

    const OBJECT_LAYER_FRAME template_range = edit->get_object_layer_frame(template_object);
    const LPCWSTR target_name = destination == MediaDestination::game
        ? L"biim: \u30b2\u30fc\u30e0\u7d20\u6750"
        : L"biim: \u53f3\u4e0b\u7d20\u6750";

    std::vector<MediaOrderItem> items;
    for (int layer = 0; layer <= edit->info->layer_max; ++layer) {
        if (layer == template_range.layer) {
            continue;
        }
        LPCWSTR layer_name = edit->get_layer_name ? edit->get_layer_name(layer) : nullptr;
        const bool layer_matches = layer_name && std::wcscmp(layer_name, target_name) == 0;

        int frame = template_range.start;
        while (frame <= template_range.end) {
            OBJECT_HANDLE obj = edit->find_object(layer, frame);
            if (!obj) {
                break;
            }
            OBJECT_LAYER_FRAME obj_range = edit->get_object_layer_frame(obj);
            if (obj_range.start > template_range.end) {
                break;
            }

            LPCWSTR obj_name = edit->get_object_name ? edit->get_object_name(obj) : nullptr;
            const bool obj_matches = obj_name && std::wcscmp(obj_name, target_name) == 0;

            if (obj_matches || (layer_matches && (!obj_name || !*obj_name))) {
                MediaOrderItem item{};
                item.handle = obj;
                item.length = std::max(1, obj_range.end - obj_range.start + 1);
                item.path = get_media_object_path(edit, obj);
                items.push_back(std::move(item));
                if (out_target_layer < 0) {
                    out_target_layer = layer;
                }
            }

            if (obj_range.end < frame) {
                frame++;
            } else {
                frame = obj_range.end + 1;
            }
        }
        if (!items.empty() && out_target_layer >= 0) {
            break;
        }
    }
    return items;
}

bool apply_media_sequence_items(
    EDIT_SECTION* edit,
    OBJECT_HANDLE template_object,
    MediaDestination destination,
    int target_layer,
    const std::vector<MediaOrderItem>& initial_items,
    const std::vector<MediaOrderItem>& final_items) {
    if (!edit || !edit->info || !template_object || final_items.empty() ||
        !edit->get_object_layer_frame) {
        return false;
    }

    OBJECT_LAYER_FRAME template_range = edit->get_object_layer_frame(template_object);
    std::int64_t total_length = 0;
    for (const auto& item : final_items) {
        if (total_length > (std::numeric_limits<int>::max)() - item.length) {
            show_media_message(L"\u7d20\u6750\u306e\u5408\u8a08\u6642\u9593\u304c\u9577\u3059\u304e\u307e\u3059\u3002");
            return false;
        }
        total_length += item.length;
    }

    if (total_length <= 0 ||
        total_length > (std::numeric_limits<int>::max)() - template_range.start) {
        show_media_message(L"\u7d20\u6750\u306e\u5408\u8a08\u6642\u9593\u304c\u9577\u3059\u304e\u307e\u3059\u3002");
        return false;
    }



    MediaTarget game_target{};
    MediaTarget bottom_right_target{};
    if (!calculate_media_targets(
            edit->info->width, edit->info->height, game_target, bottom_right_target)) {
        return false;
    }
    const MediaTarget& target = destination == MediaDestination::game
        ? game_target
        : bottom_right_target;

    const LPCWSTR object_name = destination == MediaDestination::game
        ? L"biim: \u30b2\u30fc\u30e0\u7d20\u6750"
        : L"biim: \u53f3\u4e0b\u7d20\u6750";

    if (target_layer < 0) {
        std::vector<int> media_layers;
        if (!prepare_media_layers(edit, template_object, template_range, 1, media_layers)) {
            show_media_message(
                L"\u7d20\u6750\u3092\u7f6e\u304f\u80cc\u9762\u30ec\u30a4\u30e4\u30fc\u3092\u7528\u610f\u3067\u304d\u307e\u305b\u3093\u3067\u3057\u305f\u3002");
            return false;
        }
        target_layer = media_layers.front();
    }

    if (edit->delete_object) {
        for (const auto& init : initial_items) {
            if (init.handle) {
                bool kept = false;
                for (const auto& item : final_items) {
                    if (item.handle == init.handle) {
                        kept = true;
                        break;
                    }
                }
                if (!kept) {
                    edit->delete_object(init.handle);
                }
            }
        }
    }

    const int temp_layer = edit->info->layer_max + 1;
    bool evacuated = true;
    int temp_offset = 0;
    if (edit->move_object) {
        for (const auto& item : final_items) {
            if (item.handle) {
                if (!edit->move_object(item.handle, temp_layer, template_range.start + temp_offset)) {
                    evacuated = false;
                    break;
                }
                temp_offset += item.length + 10;
            }
        }
    } else {
        evacuated = false;
    }

    int frame = template_range.start;
    int placed = 0;

    if (evacuated) {
        for (const auto& item : final_items) {
            if (item.handle) {
                if (edit->move_object(item.handle, target_layer, frame)) {
                    ++placed;
                }
                if (edit->get_object_section_num && edit->move_object_section) {
                    const int section = edit->get_object_section_num(item.handle);
                    edit->move_object_section(item.handle, section, frame + item.length - 1);
                }
            } else if (!item.path.empty()) {
                if (create_media_object(
                        edit,
                        item.path.c_str(),
                        target_layer,
                        frame,
                        item.length,
                        target,
                        object_name,
                        object_name)) {
                    ++placed;
                }
            }
            frame += item.length;
        }
    } else {
        if (edit->delete_object) {
            for (const auto& item : final_items) {
                if (item.handle) {
                    edit->delete_object(item.handle);
                }
            }
        }
        for (const auto& item : final_items) {
            if (!item.path.empty()) {
                if (create_media_object(
                        edit,
                        item.path.c_str(),
                        target_layer,
                        frame,
                        item.length,
                        target,
                        object_name,
                        object_name)) {
                    ++placed;
                }
            }
            frame += item.length;
        }
    }

    if (destination == MediaDestination::bottom_right && placed > 0) {
        bottom_right_transparent.value = true;
        if (edit->set_object_item_value) {
            edit->set_object_item_value(
                template_object,
                L"Biim Template",
                L"\u53f3\u4e0b\u6b04\u3092\u900f\u904e",
                "1");
        }
    }

    return placed > 0;
}

void open_media_manager(EDIT_SECTION* edit) {
    if (!edit || !edit->info || !edit->get_focus_object ||
        !edit->get_object_layer_frame || !edit->find_object) {
        return;
    }

    OBJECT_HANDLE template_object = edit->get_focus_object();
    if (!template_object) {
        show_media_message(
            L"biim\u30c6\u30f3\u30d7\u30ec\u30fc\u30c8\u3092\u9078\u629e\u3057\u3066\u304b\u3089\u5b9f\u884c\u3057\u3066\u304f\u3060\u3055\u3044\u3002");
        return;
    }

    UnifiedOrderDialogState state{};
    state.edit = edit;
    state.template_object = template_object;
    state.current_destination = MediaDestination::game;

    state.game_items = collect_placed_media_items(
        edit, template_object, MediaDestination::game, state.game_target_layer);
    state.initial_game_items = state.game_items;

    state.bottom_right_items = collect_placed_media_items(
        edit, template_object, MediaDestination::bottom_right, state.bottom_right_target_layer);
    state.initial_bottom_right_items = state.bottom_right_items;

    wchar_t headless[16]{};
    if (GetEnvironmentVariableW(L"BIIM_TEMPLATE_HEADLESS_TEST", headless, 16) > 0) {
        wchar_t length_str[32]{};
        int override_length = 0;
        if (GetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_MEDIA_LENGTH", length_str, 32) > 0) {
            override_length = _wtoi(length_str);
        }

        wchar_t game_len_str[32]{};
        int game_override = override_length;
        if (GetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_GAME_LENGTH", game_len_str, 32) > 0) {
            game_override = _wtoi(game_len_str);
        }

        wchar_t br_len_str[32]{};
        int br_override = override_length;
        if (GetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_BOTTOM_RIGHT_LENGTH", br_len_str, 32) > 0) {
            br_override = _wtoi(br_len_str);
        }

        wchar_t game_file[MAX_PATH]{};
        if (GetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_GAME_FILE", game_file, MAX_PATH) > 0 && *game_file) {
            MediaOrderItem item{};
            item.path = game_file;
            MEDIA_INFO media{};
            if (game_override > 0) {
                item.length = game_override;
            } else if (edit->get_media_info && edit->get_media_info(game_file, &media, sizeof(media)) && media.width > 0) {
                item.length = media_sequence_length(*edit->info, media);
            } else {
                item.length = 100;
            }
            state.game_items.push_back(item);
            state.game_modified = true;
        }

        wchar_t br_file[MAX_PATH]{};
        if (GetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_BOTTOM_RIGHT_FILE", br_file, MAX_PATH) > 0 && *br_file) {
            MediaOrderItem item{};
            item.path = br_file;
            MEDIA_INFO media{};
            if (br_override > 0) {
                item.length = br_override;
            } else if (edit->get_media_info && edit->get_media_info(br_file, &media, sizeof(media)) && media.width > 0) {
                item.length = media_sequence_length(*edit->info, media);
            } else {
                item.length = 100;
            }
            state.bottom_right_items.push_back(item);
            state.bottom_right_modified = true;
        }

        wchar_t clear_game[16]{};
        if (GetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_CLEAR_GAME", clear_game, 16) > 0) {
            state.game_items.clear();
            state.game_modified = true;
        }
        wchar_t clear_br[16]{};
        if (GetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_CLEAR_BOTTOM_RIGHT", clear_br, 16) > 0) {
            state.bottom_right_items.clear();
            state.bottom_right_modified = true;
        }

        wchar_t test_dest[16]{};
        if (GetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_DEST", test_dest, 16) > 0 && _wtoi(test_dest) == 1) {
            state.current_destination = MediaDestination::bottom_right;
        }

        wchar_t edit_idx_str[16]{};
        if (GetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_EDIT_INDEX", edit_idx_str, 16) > 0) {
            const int edit_idx = _wtoi(edit_idx_str);
            auto& items = (state.current_destination == MediaDestination::game)
                ? state.game_items
                : state.bottom_right_items;
            if (edit_idx >= 0 && edit_idx < static_cast<int>(items.size())) {
                wchar_t new_len_str[32]{};
                if (GetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_EDIT_NEW_LENGTH", new_len_str, 32) > 0) {
                    items[static_cast<std::size_t>(edit_idx)].length = _wtoi(new_len_str);
                    mark_modified(&state);
                }
                wchar_t new_file[MAX_PATH]{};
                if (GetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_EDIT_NEW_FILE", new_file, MAX_PATH) > 0 && *new_file) {
                    items[static_cast<std::size_t>(edit_idx)].path = new_file;
                    if (items[static_cast<std::size_t>(edit_idx)].handle && edit->delete_object) {
                        edit->delete_object(items[static_cast<std::size_t>(edit_idx)].handle);
                    }
                    items[static_cast<std::size_t>(edit_idx)].handle = nullptr;
                    mark_modified(&state);
                }
                wchar_t move_dest[16]{};
                if (GetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_EDIT_MOVE_DEST", move_dest, 16) > 0) {
                    MediaOrderItem item = std::move(items[static_cast<std::size_t>(edit_idx)]);
                    items.erase(items.begin() + edit_idx);
                    if (item.handle && edit->delete_object) {
                        edit->delete_object(item.handle);
                    }
                    item.handle = nullptr;
                    if (state.current_destination == MediaDestination::game) {
                        state.bottom_right_items.push_back(std::move(item));
                    } else {
                        state.game_items.push_back(std::move(item));
                    }
                    state.game_modified = true;
                    state.bottom_right_modified = true;
                }
            }
        }

        state.accepted = true;
    } else {
        constexpr wchar_t class_name[] = L"BiimTemplateUnifiedMediaWindow";
        WNDCLASSW window_class{};
        window_class.lpfnWndProc = unified_order_dialog_proc;
        window_class.hInstance = GetModuleHandleW(nullptr);
        window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        window_class.lpszClassName = class_name;
        if (!RegisterClassW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return;
        }

        const HWND owner = GetActiveWindow();
        RECT rectangle{0, 0, 580, 440};
        AdjustWindowRectEx(&rectangle, WS_CAPTION | WS_SYSMENU | WS_POPUP, FALSE, WS_EX_DLGMODALFRAME);
        int x = CW_USEDEFAULT;
        int y = CW_USEDEFAULT;
        RECT owner_rectangle{};
        if (owner && GetWindowRect(owner, &owner_rectangle)) {
            const int width = rectangle.right - rectangle.left;
            const int height = rectangle.bottom - rectangle.top;
            x = owner_rectangle.left + ((owner_rectangle.right - owner_rectangle.left) - width) / 2;
            y = owner_rectangle.top + ((owner_rectangle.bottom - owner_rectangle.top) - height) / 2;
        }

        const HWND window = CreateWindowExW(
            WS_EX_DLGMODALFRAME,
            class_name,
            L"\u7d20\u6750\u306e\u8ffd\u52a0\u30fb\u7de8\u96c6",
            WS_CAPTION | WS_SYSMENU | WS_POPUP,
            x,
            y,
            rectangle.right - rectangle.left,
            rectangle.bottom - rectangle.top,
            owner,
            nullptr,
            GetModuleHandleW(nullptr),
            &state);
        if (!window) {
            return;
        }

        if (owner) {
            EnableWindow(owner, FALSE);
        }
        ShowWindow(window, SW_SHOW);
        UpdateWindow(window);
        MSG message{};
        while (IsWindow(window)) {
            const BOOL result = GetMessageW(&message, nullptr, 0, 0);
            if (result <= 0) {
                if (result == 0) {
                    PostQuitMessage(static_cast<int>(message.wParam));
                }
                break;
            }
            if (!IsDialogMessageW(window, &message)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
        if (owner) {
            EnableWindow(owner, TRUE);
            SetActiveWindow(owner);
        }
    }

    if (state.accepted) {
        const OBJECT_LAYER_FRAME template_range = edit->get_object_layer_frame(template_object);
        int needed_layers = 0;
        if (state.game_modified && !state.game_items.empty() && state.game_target_layer < 0) {
            ++needed_layers;
        }
        if (state.bottom_right_modified && !state.bottom_right_items.empty() && state.bottom_right_target_layer < 0) {
            ++needed_layers;
        }

        if (needed_layers > 0) {
            std::vector<int> media_layers;
            if (prepare_media_layers(edit, template_object, template_range, needed_layers, media_layers)) {
                std::size_t layer_idx = 0;
                if (state.game_modified && !state.game_items.empty() && state.game_target_layer < 0) {
                    state.game_target_layer = media_layers[layer_idx++];
                }
                if (state.bottom_right_modified && !state.bottom_right_items.empty() && state.bottom_right_target_layer < 0) {
                    state.bottom_right_target_layer = media_layers[layer_idx++];
                }
            }
        }

        if (state.game_modified) {
            if (!state.game_items.empty()) {
                apply_media_sequence_items(
                    edit, template_object, MediaDestination::game,
                    state.game_target_layer, state.initial_game_items, state.game_items);
            } else if (!state.initial_game_items.empty() && edit->delete_object) {
                for (const auto& item : state.initial_game_items) {
                    if (item.handle) {
                        edit->delete_object(item.handle);
                    }
                }
            }
        }
        if (state.bottom_right_modified) {
            if (!state.bottom_right_items.empty()) {
                apply_media_sequence_items(
                    edit, template_object, MediaDestination::bottom_right,
                    state.bottom_right_target_layer, state.initial_bottom_right_items, state.bottom_right_items);
            } else if (!state.initial_bottom_right_items.empty() && edit->delete_object) {
                for (const auto& item : state.initial_bottom_right_items) {
                    if (item.handle) {
                        edit->delete_object(item.handle);
                    }
                }
            }
        }

        if (state.game_modified || state.bottom_right_modified) {
            int64_t game_length = 0;
            for (const auto& item : state.game_items) {
                game_length += item.length;
            }
            int64_t bottom_right_length = 0;
            for (const auto& item : state.bottom_right_items) {
                bottom_right_length += item.length;
            }

            const int64_t max_length = std::max(game_length, bottom_right_length);
            if (max_length > 0 && edit->get_object_section_num && edit->move_object_section && edit->get_object_layer_frame) {
                const OBJECT_LAYER_FRAME current_range = edit->get_object_layer_frame(template_object);
                const int desired_end = current_range.start + static_cast<int>(max_length) - 1;
                if (desired_end != current_range.end && desired_end >= current_range.start) {
                    edit->move_object_section(
                        template_object,
                        edit->get_object_section_num(template_object),
                        desired_end);
                }
            }
        }
    }
}

bool process_video(FILTER_PROC_VIDEO* video) {
    if (!video || !video->scene || !video->set_image_data || !video->userdata) {
        return false;
    }

    const int width = video->scene->width;
    const int height = video->scene->height;
    const Settings settings = read_settings();
    auto* cache = static_cast<RenderCache*>(video->userdata);

    std::scoped_lock lock(cache->mutex);
    if (!cache->valid || cache->width != width || cache->height != height || !(cache->settings == settings)) {
        cache->valid = false;
        if (!render_template(*cache, settings, width, height)) {
            return false;
        }
    }

    video->set_image_data(cache->pixels.data(), width, height);
    if (video->set_default_anchor) {
        video->set_default_anchor(width, height);
    }
    return true;
}

void* create_instance(int64_t) {
    return new (std::nothrow) RenderCache();
}

void destroy_instance(int64_t, void* userdata) {
    delete static_cast<RenderCache*>(userdata);
}

FILTER_PLUGIN_TABLE plugin_table = {
    FILTER_PLUGIN_TABLE::FLAG_VIDEO |
        FILTER_PLUGIN_TABLE::FLAG_INPUT |
        FILTER_PLUGIN_TABLE::FLAG_USERDATA,
    L"Biim Template",
    L"biim\u30c6\u30f3\u30d7\u30ec\u30fc\u30c8",
    L"Biim Template for AviUtl2 version 1.2.0",
    filter_items,
    process_video,
    nullptr,
    create_instance,
    destroy_instance,
};

}  // namespace

extern "C" __declspec(dllexport) DWORD RequiredVersion() {
    return 2010200;
}

extern "C" __declspec(dllexport) bool InitializePlugin(DWORD) {
    return true;
}

extern "C" __declspec(dllexport) void UninitializePlugin() {
}

extern "C" __declspec(dllexport) FILTER_PLUGIN_TABLE* GetFilterPluginTable() {
    return &plugin_table;
}
