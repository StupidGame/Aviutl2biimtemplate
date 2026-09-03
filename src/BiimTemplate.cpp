#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
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

void place_media_files(EDIT_SECTION* edit);

auto media_group = FILTER_ITEM_GROUP(L"\u52d5\u753b\u30fb\u753b\u50cf");
constexpr wchar_t media_file_filter[] =
    L"Media Files (*.mp4;*.mkv;*.avi;*.mov;*.webm;*.png;*.jpg;*.jpeg;*.bmp;*.webp)\0"
    L"*.mp4;*.mkv;*.avi;*.mov;*.webm;*.png;*.jpg;*.jpeg;*.bmp;*.webp\0"
    L"All Files (*.*)\0*.*\0";
auto game_media_file = FILTER_ITEM_FILE(L"\u30b2\u30fc\u30e0\u6b04\u306e\u52d5\u753b\u30fb\u753b\u50cf", L"", media_file_filter);
auto bottom_right_media_file = FILTER_ITEM_FILE(L"\u53f3\u4e0b\u306e\u52d5\u753b\u30fb\u753b\u50cf", L"", media_file_filter);
auto place_media_button = FILTER_ITEM_BUTTON(L"\u9078\u3093\u3060\u7d20\u6750\u3092\u914d\u7f6e", place_media_files);

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
    &game_media_file,
    &bottom_right_media_file,
    &place_media_button,
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
    std::wstring bottom_right_media;
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
    result.bottom_right_media = read_text(bottom_right_media_file.value);
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
    if (settings.bottom_right_media.empty()) {
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

    const bool x_set = set_standard_drawing_value(edit, object, L"X", center_x);
    const bool y_set = set_standard_drawing_value(edit, object, L"Y", center_y);
    const bool scale_set = set_standard_drawing_value(
        edit, object, L"\u62e1\u5927\u7387", scale * 100.0);
    return x_set && y_set && scale_set;
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
    if (!edit->get_media_info(path, &media, sizeof(media)) ||
        media.video_track_num <= 0 || media.width <= 0 || media.height <= 0) {
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

void place_media_files(EDIT_SECTION* edit) {
    if (!edit || !edit->info || !edit->get_focus_object || !edit->get_object_layer_frame) {
        return;
    }

    const bool has_game_media = game_media_file.value && *game_media_file.value;
    const bool has_bottom_right_media = bottom_right_media_file.value && *bottom_right_media_file.value;
    if (!has_game_media && !has_bottom_right_media) {
        show_media_message(
            L"\u52d5\u753b\u307e\u305f\u306f\u753b\u50cf\u3092\u9078\u3093\u3067\u304b\u3089\u914d\u7f6e\u3057\u3066\u304f\u3060\u3055\u3044\u3002");
        return;
    }

    OBJECT_HANDLE template_object = edit->get_focus_object();
    if (!template_object) {
        show_media_message(
            L"biim\u30c6\u30f3\u30d7\u30ec\u30fc\u30c8\u3092\u9078\u629e\u3057\u3066\u304b\u3089\u5b9f\u884c\u3057\u3066\u304f\u3060\u3055\u3044\u3002");
        return;
    }

    const OBJECT_LAYER_FRAME template_range = edit->get_object_layer_frame(template_object);
    const int length = std::max(1, template_range.end - template_range.start + 1);
    const int scene_width = edit->info->width;
    const int scene_height = edit->info->height;
    if (scene_width <= 0 || scene_height <= 0) {
        return;
    }

    const int game_right = scene_width - std::clamp(
        static_cast<int>(std::lround(scene_width * sidebar_width.value / 100.0)), 1, scene_width);
    const int game_bottom = scene_height - std::clamp(
        static_cast<int>(std::lround(scene_height * bottom_height.value / 100.0)), 1, scene_height);
    const double resolution_scale = auto_scale.value
        ? std::clamp(static_cast<double>(scene_height) / 720.0, 0.25, 8.0)
        : 1.0;
    const int line_width = std::max(0, static_cast<int>(std::lround(separator_width.value * resolution_scale)));
    const int left_half = line_width / 2;
    const int right_half = line_width - left_half;

    const MediaTarget game_target{
        0.0,
        0.0,
        static_cast<double>(std::max(1, game_right - left_half)),
        static_cast<double>(std::max(1, game_bottom - left_half)),
    };
    const MediaTarget bottom_right_target{
        static_cast<double>(std::min(scene_width, game_right + right_half)),
        static_cast<double>(std::min(scene_height, game_bottom + right_half)),
        static_cast<double>(scene_width),
        static_cast<double>(scene_height),
    };

    int next_layer = std::max(template_range.layer + 1, edit->info->layer_max + 1);
    int requested = 0;
    int placed = 0;
    if (has_game_media) {
        ++requested;
        if (create_media_object(
                edit,
                game_media_file.value,
                next_layer,
                template_range.start,
                length,
                game_target,
                L"biim: \u30b2\u30fc\u30e0\u7d20\u6750",
                L"biim: \u30b2\u30fc\u30e0\u7d20\u6750")) {
            ++placed;
        }
        next_layer += 2;
    }
    if (has_bottom_right_media) {
        ++requested;
        if (create_media_object(
                edit,
                bottom_right_media_file.value,
                next_layer,
                template_range.start,
                length,
                bottom_right_target,
                L"biim: \u53f3\u4e0b\u7d20\u6750",
                L"biim: \u53f3\u4e0b\u7d20\u6750")) {
            ++placed;
        }
    }

    if (placed != requested) {
        show_media_message(
            L"\u4e00\u90e8\u306e\u7d20\u6750\u3092\u914d\u7f6e\u3067\u304d\u307e\u305b\u3093\u3067\u3057\u305f\u3002\n"
            L"\u5bfe\u5fdc\u5f62\u5f0f\u3068\u7a7a\u304d\u30ec\u30a4\u30e4\u30fc\u3092\u78ba\u8a8d\u3057\u3066\u304f\u3060\u3055\u3044\u3002");
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
    L"Biim Template for AviUtl2 version 1.1.0",
    filter_items,
    process_video,
    nullptr,
    create_instance,
    destroy_instance,
};

}  // namespace

extern "C" __declspec(dllexport) DWORD RequiredVersion() {
    return 2003300;
}

extern "C" __declspec(dllexport) bool InitializePlugin(DWORD) {
    return true;
}

extern "C" __declspec(dllexport) void UninitializePlugin() {
}

extern "C" __declspec(dllexport) FILTER_PLUGIN_TABLE* GetFilterPluginTable() {
    return &plugin_table;
}
