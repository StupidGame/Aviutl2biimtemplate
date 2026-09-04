#define NOMINMAX
#include <windows.h>

#include <cstddef>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <iostream>
#include <string>
#include <vector>

#include "plugin2.h"
#include "filter2.h"

namespace {

std::vector<PIXEL_RGBA> captured_pixels;
int captured_width = 0;
int captured_height = 0;
int anchor_width = 0;
int anchor_height = 0;

struct CreatedMedia {
    std::wstring path;
    int layer = 0;
    int frame = 0;
    int length = 0;
    OBJECT_HANDLE handle = nullptr;
};

struct ItemChange {
    OBJECT_HANDLE object = nullptr;
    std::wstring effect;
    std::wstring item;
    std::string value;
};

std::vector<CreatedMedia> created_media;
std::vector<ItemChange> item_changes;
std::vector<std::wstring> object_names;
std::vector<std::wstring> layer_names;
OBJECT_LAYER_FRAME focused_range{4, 12, 111};
int media_video_tracks = 1;

struct ObjectMove {
    OBJECT_HANDLE object = nullptr;
    int layer = 0;
    int frame = 0;
};

std::vector<ObjectMove> object_moves;

void capture_image(const PIXEL_RGBA* pixels, int width, int height) {
    captured_width = width;
    captured_height = height;
    captured_pixels.assign(pixels, pixels + static_cast<std::size_t>(width) * height);
}

void capture_anchor(int width, int height) {
    anchor_width = width;
    anchor_height = height;
}

bool has_alpha_at(int x, int y, bool expected_visible) {
    const auto& pixel = captured_pixels[static_cast<std::size_t>(y) * captured_width + x];
    return expected_visible ? pixel.a > 0 : pixel.a == 0;
}

bool has_title_text_pixels() {
    for (int y = 0; y < 100; ++y) {
        for (int x = 970; x < 1270; ++x) {
            const auto& pixel = captured_pixels[static_cast<std::size_t>(y) * captured_width + x];
            if (pixel.a > 240 && pixel.r > 230 && pixel.g > 170 && pixel.b < 150) {
                return true;
            }
        }
    }
    return false;
}

int fail(const char* message) {
    std::cerr << "Smoke test failed: " << message << '\n';
    return 1;
}

struct ItemHeader {
    LPCWSTR type;
    LPCWSTR name;
};

void* find_filter_item(FILTER_PLUGIN_TABLE* table, LPCWSTR name, LPCWSTR type) {
    for (void** item = table->items; item && *item; ++item) {
        const auto* header = static_cast<const ItemHeader*>(*item);
        if (header->name && header->type &&
            std::wcscmp(header->name, name) == 0 && std::wcscmp(header->type, type) == 0) {
            return *item;
        }
    }
    return nullptr;
}

struct MockTimelineObject {
    OBJECT_HANDLE handle = nullptr;
    int layer = 0;
    int start = 0;
    int end = 0;
    std::wstring name;
    std::string alias;
};
std::vector<MockTimelineObject> mock_timeline_objects;
OBJECT_HANDLE mock_focused_obj = reinterpret_cast<OBJECT_HANDLE>(100);
int mock_cursor_layer = -1;
int mock_cursor_frame = -1;
int mock_display_layer = -1;
int mock_display_frame = -1;

void mock_set_focus_object(OBJECT_HANDLE obj) {
    mock_focused_obj = obj;
}

void mock_set_cursor_layer_frame(int layer, int frame) {
    mock_cursor_layer = layer;
    mock_cursor_frame = frame;
}

void mock_set_display_layer_frame(int layer, int frame) {
    mock_display_layer = layer;
    mock_display_frame = frame;
}

OBJECT_HANDLE mock_focus_object() {
    return mock_focused_obj;
}

OBJECT_LAYER_FRAME mock_object_range(OBJECT_HANDLE object) {
    if (object == reinterpret_cast<OBJECT_HANDLE>(100)) {
        return focused_range;
    }
    for (const auto& item : mock_timeline_objects) {
        if (item.handle == object) {
            return {item.layer, item.start, item.end};
        }
    }
    return focused_range;
}

OBJECT_HANDLE mock_find_object(int layer, int frame) {
    OBJECT_HANDLE best = nullptr;
    int best_start = 10000000;
    for (const auto& item : mock_timeline_objects) {
        if (item.layer == layer && item.end >= frame && item.start < best_start) {
            best_start = item.start;
            best = item.handle;
        }
    }
    return best;
}

LPCWSTR mock_get_object_name(OBJECT_HANDLE object) {
    for (const auto& item : mock_timeline_objects) {
        if (item.handle == object) {
            return item.name.c_str();
        }
    }
    return nullptr;
}

LPCWSTR mock_get_layer_name(int layer) {
    for (const auto& item : mock_timeline_objects) {
        if (item.layer == layer) {
            return item.name.c_str();
        }
    }
    return nullptr;
}

LPCSTR mock_get_object_alias(OBJECT_HANDLE object) {
    for (const auto& item : mock_timeline_objects) {
        if (item.handle == object) {
            return item.alias.c_str();
        }
    }
    return nullptr;
}

bool mock_layer_lock(int) {
    return false;
}

bool mock_move_object(OBJECT_HANDLE object, int layer, int frame) {
    object_moves.push_back({object, layer, frame});
    return true;
}

int mock_moved_section_frame = -1;
int mock_get_object_section_num(OBJECT_HANDLE) {
    return 1;
}

bool mock_move_object_section(OBJECT_HANDLE object, int, int frame) {
    mock_moved_section_frame = frame;
    if (object == reinterpret_cast<OBJECT_HANDLE>(100)) {
        focused_range.end = frame;
    }
    for (auto& item : mock_timeline_objects) {
        if (item.handle == object) {
            item.end = frame;
        }
    }
    return true;
}

bool mock_media_info(LPCWSTR, MEDIA_INFO* info, int) {
    info->video_track_num = media_video_tracks;
    info->audio_track_num = 1;
    info->total_time = 10.0;
    info->width = 1920;
    info->height = 1080;
    return true;
}

OBJECT_HANDLE mock_create_media(LPCWSTR path, int layer, int frame, int length) {
    const auto handle = reinterpret_cast<OBJECT_HANDLE>(created_media.size() + 1);
    created_media.push_back({path, layer, frame, length, handle});
    mock_timeline_objects.push_back({handle, layer, frame, frame + length - 1, L"", ""});
    return handle;
}

bool mock_set_item(OBJECT_HANDLE object, LPCWSTR effect, LPCWSTR item, LPCSTR value) {
    item_changes.push_back({object, effect, item, value});
    return true;
}

int mock_effect_list(OBJECT_HANDLE object, EFFECT_HANDLE* effects, int count) {
    if (!effects || count <= 0) {
        return 1;
    }
    effects[0] = reinterpret_cast<EFFECT_HANDLE>(object);
    return 1;
}

LPCSTR mock_get_effect_item(EFFECT_HANDLE, LPCWSTR item) {
    if (std::wcscmp(item, L"X") == 0 || std::wcscmp(item, L"Y") == 0 ||
        std::wcscmp(item, L"\u62e1\u5927\u7387") == 0) {
        return "0";
    }
    return nullptr;
}

bool mock_set_effect_item(EFFECT_HANDLE effect, LPCWSTR item, LPCSTR value) {
    item_changes.push_back({
        reinterpret_cast<OBJECT_HANDLE>(effect), L"\u6a19\u6e96\u63cf\u753b", item, value});
    return true;
}

void mock_set_object_name(OBJECT_HANDLE object, LPCWSTR name) {
    object_names.emplace_back(name);
    for (auto& item : mock_timeline_objects) {
        if (item.handle == object) {
            item.name = name;
        }
    }
}

void mock_set_layer_name(int layer, LPCWSTR name) {
    layer_names.emplace_back(name);
    for (auto& item : mock_timeline_objects) {
        if (item.layer == layer && item.name.empty()) {
            item.name = name;
        }
    }
}

double changed_value(OBJECT_HANDLE object, LPCWSTR item) {
    for (const auto& change : item_changes) {
        if (change.object == object && change.item == item) {
            return std::stod(change.value);
        }
    }
    return 1.0e30;
}

bool approximately(double actual, double expected, double tolerance = 0.02) {
    return std::abs(actual - expected) <= tolerance;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        return fail("the plugin path argument is missing");
    }

    HMODULE module = LoadLibraryW(argv[1]);
    if (!module) {
        return fail("LoadLibraryW could not load the plugin");
    }

    using GetTable = FILTER_PLUGIN_TABLE* (*)();
    using Initialize = bool (*)(DWORD);
    using Uninitialize = void (*)();
    using Required = DWORD (*)();
    auto get_table = reinterpret_cast<GetTable>(GetProcAddress(module, "GetFilterPluginTable"));
    auto initialize = reinterpret_cast<Initialize>(GetProcAddress(module, "InitializePlugin"));
    auto uninitialize = reinterpret_cast<Uninitialize>(GetProcAddress(module, "UninitializePlugin"));
    auto required = reinterpret_cast<Required>(GetProcAddress(module, "RequiredVersion"));
    if (!get_table || !initialize || !uninitialize || !required) {
        FreeLibrary(module);
        return fail("one or more required exports are missing");
    }
    if (required() < 2010200 || !initialize(required())) {
        FreeLibrary(module);
        return fail("plugin initialization failed");
    }

    FILTER_PLUGIN_TABLE* table = get_table();
    if (!table || !table->func_proc_video || !table->func_create || !table->func_destroy) {
        uninitialize();
        FreeLibrary(module);
        return fail("the filter plugin table is incomplete");
    }

    void* userdata = table->func_create(1);
    if (!userdata) {
        uninitialize();
        FreeLibrary(module);
        return fail("instance allocation failed");
    }

    SCENE_INFO scene{};
    scene.width = 1280;
    scene.height = 720;
    scene.rate = 60;
    scene.scale = 1;
    scene.sample_rate = 48000;
    OBJECT_INFO object{};
    FILTER_PROC_VIDEO video{};
    video.scene = &scene;
    video.object = &object;
    video.set_image_data = capture_image;
    video.set_default_anchor = capture_anchor;
    video.userdata = userdata;

    const bool first_result = table->func_proc_video(&video);
    const std::vector<PIXEL_RGBA> first_frame = captured_pixels;
    const bool second_result = table->func_proc_video(&video);

    int result = 0;
    if (!first_result || !second_result) {
        result = fail("video processing returned false");
    } else if (captured_width != 1280 || captured_height != 720 ||
               anchor_width != 1280 || anchor_height != 720) {
        result = fail("the generated dimensions are incorrect");
    } else if (captured_pixels.size() != static_cast<std::size_t>(1280) * 720) {
        result = fail("the generated pixel count is incorrect");
    } else if (!has_alpha_at(100, 100, false)) {
        result = fail("the gameplay area is not transparent");
    } else if (!has_alpha_at(1200, 100, true)) {
        result = fail("the sidebar is transparent");
    } else if (!has_alpha_at(100, 650, true)) {
        result = fail("the bottom panel is transparent");
    } else if (!has_alpha_at(1200, 650, true)) {
        result = fail("the empty bottom-right panel is transparent");
    } else if (!has_alpha_at(960, 100, true)) {
        result = fail("the separator is transparent");
    } else if (!has_title_text_pixels()) {
        result = fail("the title text was not rendered");
    } else if (captured_pixels.size() != first_frame.size() ||
               std::memcmp(captured_pixels.data(), first_frame.data(),
                           captured_pixels.size() * sizeof(PIXEL_RGBA)) != 0) {
        result = fail("the cached second frame differs from the first frame");
    }

    auto* place_button = static_cast<FILTER_ITEM_BUTTON*>(find_filter_item(
        table, L"\u7d20\u6750\u306e\u8ffd\u52a0\u30fb\u7de8\u96c6", L"button"));
    if (!place_button) {
        place_button = static_cast<FILTER_ITEM_BUTTON*>(find_filter_item(
            table, L"\u7d20\u6750\u3092\u8ffd\u52a0", L"button"));
    }
    auto* sync_to_media_button = static_cast<FILTER_ITEM_BUTTON*>(find_filter_item(
        table, L"\u9577\u3055\u3092\u7d20\u6750\u306b\u5408\u308f\u305b\u308b", L"button"));
    auto* sync_to_template_button = static_cast<FILTER_ITEM_BUTTON*>(find_filter_item(
        table, L"\u7d20\u6750\u3092\u67a0\u306e\u9577\u3055\u306b\u5408\u308f\u305b\u308b", L"button"));
    auto* transparent_check = static_cast<FILTER_ITEM_CHECK*>(find_filter_item(
        table, L"\u53f3\u4e0b\u6b04\u3092\u900f\u904e", L"check"));
    auto* old_game_file = find_filter_item(
        table, L"\u30b2\u30fc\u30e0\u6b04\u306e\u52d5\u753b\u30fb\u753b\u50cf", L"file");
    auto* old_place_button = find_filter_item(
        table, L"\u9078\u3093\u3060\u7d20\u6750\u3092\u914d\u7f6e", L"button");
    if (result == 0 && (!place_button || !sync_to_media_button || !sync_to_template_button ||
                        !transparent_check || old_game_file || old_place_button)) {
        result = fail("the unified media controls or timeline sync buttons are missing or old controls still exist");
    }

    if (result == 0) {
        SetEnvironmentVariableW(L"BIIM_TEMPLATE_HEADLESS_TEST", L"1");
        SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_GAME_FILE", L"C:\\media\\game.mp4");
        SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_BOTTOM_RIGHT_FILE", L"C:\\media\\speaker.png");
        SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_MEDIA_LENGTH", L"100");

        EDIT_INFO edit_info{};
        edit_info.width = 1280;
        edit_info.height = 720;
        edit_info.frame = 12;
        edit_info.layer = 4;
        edit_info.layer_max = 4;
        EDIT_SECTION edit{};
        edit.info = &edit_info;
        edit.find_object = mock_find_object;
        edit.get_focus_object = mock_focus_object;
        edit.get_object_layer_frame = mock_object_range;
        edit.get_media_info = mock_media_info;
        edit.create_object_from_media_file = mock_create_media;
        edit.set_object_item_value = mock_set_item;
        edit.get_effect_list = mock_effect_list;
        edit.get_effect_item_value = mock_get_effect_item;
        edit.set_effect_item_value = mock_set_effect_item;
        edit.set_object_name = mock_set_object_name;
        edit.set_layer_name = mock_set_layer_name;
        edit.get_layer_lock = mock_layer_lock;
        edit.move_object = mock_move_object;
        place_button->callback(&edit);

        if (created_media.size() != 2 || item_changes.size() != 7 ||
            object_names.size() != 2 || layer_names.size() != 2) {
            result = fail("the media placement callback did not create and configure two objects");
        } else if (created_media[0].layer != 3 || created_media[1].layer != 2 ||
                   created_media[0].frame != 12 || created_media[1].frame != 12 ||
                   created_media[0].length != 100 || created_media[1].length != 100) {
            result = fail("the created media object ranges are incorrect");
        } else if (!approximately(changed_value(created_media[0].handle, L"X"), -161.0) ||
                   !approximately(changed_value(created_media[0].handle, L"Y"), -91.0) ||
                   !approximately(changed_value(
                       created_media[0].handle, L"\u62e1\u5927\u7387"), 49.8148)) {
            result = fail("the gameplay media fit is incorrect");
        } else if (!approximately(changed_value(created_media[1].handle, L"X"), 481.0) ||
                   !approximately(changed_value(created_media[1].handle, L"Y"), 271.0) ||
                   !approximately(changed_value(
                       created_media[1].handle, L"\u62e1\u5927\u7387"), 16.4815)) {
            result = fail("the bottom-right media fit is incorrect");
        } else if (!table->func_proc_video(&video) || !has_alpha_at(1200, 650, false)) {
            result = fail("the bottom-right media window is not transparent");
        }
    }

    if (result == 0) {
        created_media.clear();
        mock_timeline_objects.clear();
        item_changes.clear();
        object_names.clear();
        layer_names.clear();
        object_moves.clear();
        focused_range = {0, 20, 29};
        media_video_tracks = 0;
        SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_GAME_FILE", L"C:\\media\\still.png");
        SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_BOTTOM_RIGHT_FILE", L"");
        SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_MEDIA_LENGTH", L"10");

        EDIT_INFO top_edit_info{};
        top_edit_info.width = 1280;
        top_edit_info.height = 720;
        top_edit_info.frame = 20;
        top_edit_info.layer = 0;
        top_edit_info.layer_max = 0;
        EDIT_SECTION top_edit{};
        top_edit.info = &top_edit_info;
        top_edit.find_object = mock_find_object;
        top_edit.get_focus_object = mock_focus_object;
        top_edit.get_object_layer_frame = mock_object_range;
        top_edit.get_media_info = mock_media_info;
        top_edit.create_object_from_media_file = mock_create_media;
        top_edit.set_object_item_value = mock_set_item;
        top_edit.set_object_name = mock_set_object_name;
        top_edit.set_layer_name = mock_set_layer_name;
        top_edit.get_layer_lock = mock_layer_lock;
        top_edit.move_object = mock_move_object;
        place_button->callback(&top_edit);

        if (object_moves.size() != 1 || object_moves[0].layer != 1 ||
            object_moves[0].frame != 20) {
            result = fail("the template was not moved in front of media at the top layer");
        } else if (created_media.size() != 1 || created_media[0].layer != 0 ||
                   created_media[0].frame != 20 || created_media[0].length != 10) {
            result = fail("an image without a video track was not placed behind the template");
        } else if (item_changes.size() != 3) {
            result = fail("the top-layer image was not fitted into the gameplay area");
        }

        SetEnvironmentVariableW(L"BIIM_TEMPLATE_HEADLESS_TEST", nullptr);
        SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_GAME_FILE", nullptr);
        SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_BOTTOM_RIGHT_FILE", nullptr);
        SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_MEDIA_LENGTH", nullptr);
    }

    if (result == 0) {
        // Auto-adjustment test 1: game 40F, bottom-right none -> adjust to 40F (10..49)
        focused_range = {2, 10, 109};
        mock_moved_section_frame = -1;
        created_media.clear();
        mock_timeline_objects.clear();
        item_changes.clear();
        object_names.clear();
        layer_names.clear();

        EDIT_INFO adjust_info{};
        adjust_info.width = 1280;
        adjust_info.height = 720;
        adjust_info.frame = 10;
        adjust_info.layer = 2;
        adjust_info.layer_max = 2;
        EDIT_SECTION adjust_edit{};
        adjust_edit.info = &adjust_info;
        adjust_edit.find_object = mock_find_object;
        adjust_edit.get_focus_object = mock_focus_object;
        adjust_edit.get_object_layer_frame = mock_object_range;
        adjust_edit.get_media_info = mock_media_info;
        adjust_edit.create_object_from_media_file = mock_create_media;
        adjust_edit.set_object_item_value = mock_set_item;
        adjust_edit.get_effect_list = mock_effect_list;
        adjust_edit.get_effect_item_value = mock_get_effect_item;
        adjust_edit.set_effect_item_value = mock_set_effect_item;
        adjust_edit.set_object_name = mock_set_object_name;
        adjust_edit.set_layer_name = mock_set_layer_name;
        adjust_edit.get_object_name = mock_get_object_name;
        adjust_edit.get_layer_name = mock_get_layer_name;
        adjust_edit.get_object_alias = mock_get_object_alias;
        adjust_edit.set_focus_object = mock_set_focus_object;
        adjust_edit.set_cursor_layer_frame = mock_set_cursor_layer_frame;
        adjust_edit.set_display_layer_frame = mock_set_display_layer_frame;
        adjust_edit.get_layer_lock = mock_layer_lock;
        adjust_edit.move_object = mock_move_object;
        adjust_edit.get_object_section_num = mock_get_object_section_num;
        adjust_edit.move_object_section = mock_move_object_section;

        SetEnvironmentVariableW(L"BIIM_TEMPLATE_HEADLESS_TEST", L"1");
        SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_GAME_FILE", L"C:\\media\\game.mp4");
        SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_BOTTOM_RIGHT_FILE", L"");
        SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_GAME_LENGTH", L"40");
        place_button->callback(&adjust_edit);

        if (mock_moved_section_frame != 49 || focused_range.end != 49) {
            result = fail("the template was not shortened to match the gameplay media length");
        }

        // Auto-adjustment test 2: game 30F, bottom-right 75F -> adjust to longer 75F (10..84)
        if (result == 0) {
            focused_range = {2, 10, 109};
            mock_moved_section_frame = -1;
            created_media.clear();
            mock_timeline_objects.clear();
            item_changes.clear();
            object_names.clear();
            layer_names.clear();

            SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_GAME_FILE", L"C:\\media\\game.mp4");
            SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_BOTTOM_RIGHT_FILE", L"C:\\media\\speaker.png");
            SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_GAME_LENGTH", L"30");
            SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_BOTTOM_RIGHT_LENGTH", L"75");
            place_button->callback(&adjust_edit);

            if (mock_moved_section_frame != 84 || focused_range.end != 84) {
                result = fail("the template was not adjusted to the longer bottom-right media length");
            }
        }

        // Editing test 1: Add game media 40F, edit its length to 90F -> adjusts template to 10..99
        if (result == 0) {
            focused_range = {2, 10, 109};
            mock_moved_section_frame = -1;
            created_media.clear();
            mock_timeline_objects.clear();
            item_changes.clear();
            object_names.clear();
            layer_names.clear();

            SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_GAME_FILE", L"C:\\media\\game.mp4");
            SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_BOTTOM_RIGHT_FILE", L"");
            SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_GAME_LENGTH", L"40");
            SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_BOTTOM_RIGHT_LENGTH", L"");
            SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_EDIT_INDEX", L"0");
            SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_EDIT_NEW_LENGTH", L"90");
            place_button->callback(&adjust_edit);

            if (mock_moved_section_frame != 99 || focused_range.end != 99) {
                result = fail("editing media length did not adjust the template length");
            }

            SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_EDIT_INDEX", nullptr);
            SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_EDIT_NEW_LENGTH", nullptr);
        }

        // Editing test 2: Move destination of item 0 (game -> bottom-right)
        if (result == 0) {
            focused_range = {2, 10, 109};
            mock_moved_section_frame = -1;
            created_media.clear();
            mock_timeline_objects.clear();
            object_names.clear();

            SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_GAME_FILE", L"C:\\media\\game.mp4");
            SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_BOTTOM_RIGHT_FILE", L"");
            SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_GAME_LENGTH", L"50");
            SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_EDIT_INDEX", L"0");
            SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_EDIT_MOVE_DEST", L"1");
            place_button->callback(&adjust_edit);

            if (created_media.empty() || object_names.empty()) {
                result = fail("moving media destination did not recreate object");
            } else {
                bool found_br = false;
                for (const auto& name : object_names) {
                    if (name == L"biim: \u53f3\u4e0b\u7d20\u6750") {
                        found_br = true;
                        break;
                    }
                }
                if (!found_br) {
                    result = fail("moved media object was not configured as bottom-right media");
                }
            }

            SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_EDIT_INDEX", nullptr);
            SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_EDIT_MOVE_DEST", nullptr);
        }

        // Timeline split & sync tests:
        if (result == 0) {
            mock_timeline_objects.clear();
            // Simulate video split into 2 pieces on layer 1:
            // piece 1: frame 10..49
            // piece 2: frame 50..139
            const auto h1 = reinterpret_cast<OBJECT_HANDLE>(201);
            const auto h2 = reinterpret_cast<OBJECT_HANDLE>(202);
            mock_timeline_objects.push_back({h1, 1, 10, 49, L"biim: \u30b2\u30fc\u30e0\u7d20\u6750", "file=\"C:\\media\\game.mp4\"\n"});
            mock_timeline_objects.push_back({h2, 1, 50, 139, L"biim: \u30b2\u30fc\u30e0\u7d20\u6750", "file=\"C:\\media\\game.mp4\"\n"});

            // Bottom-right video on layer 0: frame 10..79
            const auto h_br = reinterpret_cast<OBJECT_HANDLE>(203);
            mock_timeline_objects.push_back({h_br, 0, 10, 79, L"biim: \u53f3\u4e0b\u7d20\u6750", "file=\"C:\\media\\speaker.png\"\n"});

            // Template is on layer 2, currently frame 10..49
            focused_range = {2, 10, 49};
            mock_moved_section_frame = -1;

            // Sync template to media: should find max_end among all split clips = 139!
            sync_to_media_button->callback(&adjust_edit);

            if (focused_range.end != 139 || mock_moved_section_frame != 139) {
                result = fail("sync_to_media_button did not adapt template to split timeline clips");
            }

            // Sync media to template: shorten template to frame 80, sync media
            if (result == 0) {
                focused_range.end = 80;
                mock_moved_section_frame = -1;
                sync_to_template_button->callback(&adjust_edit);

                if (mock_timeline_objects[1].end != 80 || mock_timeline_objects[2].end != 80) {
                    result = fail("sync_to_template_button did not adjust media ends to match template");
                }
            }

            // Jump to timeline selection:
            if (result == 0) {
                mock_cursor_layer = -1;
                mock_cursor_frame = -1;
                mock_display_frame = -1;
                SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_GAME_FILE", L"");
                SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_BOTTOM_RIGHT_FILE", L"");
                SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_SELECT_INDEX", L"1");
                place_button->callback(&adjust_edit);

                if (mock_focused_obj != h2 || mock_cursor_layer != 1 || mock_cursor_frame != 50 || mock_display_frame != 50) {
                    result = fail("order_select_timeline_id did not set focus, cursor, and display frame to the split clip");
                }
                SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_SELECT_INDEX", nullptr);
            }
        }

        SetEnvironmentVariableW(L"BIIM_TEMPLATE_HEADLESS_TEST", nullptr);
        SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_GAME_FILE", nullptr);
        SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_BOTTOM_RIGHT_FILE", nullptr);
        SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_GAME_LENGTH", nullptr);
        SetEnvironmentVariableW(L"BIIM_TEMPLATE_TEST_BOTTOM_RIGHT_LENGTH", nullptr);
    }

    table->func_destroy(1, userdata);
    uninitialize();
    FreeLibrary(module);
    if (result == 0) {
        std::cout << "Plugin smoke test passed.\n";
    }
    return result;
}
