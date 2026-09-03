#define NOMINMAX
#include <windows.h>

#include <cstddef>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "filter2.h"

namespace {

std::vector<PIXEL_RGBA> captured_pixels;
int captured_width = 0;
int captured_height = 0;
int anchor_width = 0;
int anchor_height = 0;

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
    if (required() == 0 || !initialize(required())) {
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
    } else if (!has_alpha_at(960, 100, true)) {
        result = fail("the separator is transparent");
    } else if (!has_title_text_pixels()) {
        result = fail("the title text was not rendered");
    } else if (captured_pixels.size() != first_frame.size() ||
               std::memcmp(captured_pixels.data(), first_frame.data(),
                           captured_pixels.size() * sizeof(PIXEL_RGBA)) != 0) {
        result = fail("the cached second frame differs from the first frame");
    }

    table->func_destroy(1, userdata);
    uninitialize();
    FreeLibrary(module);
    if (result == 0) {
        std::cout << "Plugin smoke test passed.\n";
    }
    return result;
}
