#include <string>
#include <vector>
#include <sstream>
#include <string.h>

#include <libreborn/libreborn.h>

#include "../feature/feature.h"
#include "../home/home.h"
#include "options.h"

#include <symbols/minecraft.h>

// Fix Initial Option Button Rendering
// The calling function doesn't exist in MCPE v0.6.1, so its name is unknown.
static unsigned char *OptionsPane_unknown_toggle_creating_function_OptionButton_injection(unsigned char *option_button, unsigned char *option) {
    // Call Original Method
    unsigned char *ret = (*OptionButton)(option_button, option);

    // Setup Image
    (*OptionButton_updateImage)(option_button, stored_options);

    // Return
    return ret;
}

// Actually Save options.txt
// Hook Last Options::addOptionToSaveOutput Call
static void Options_save_Options_addOptionToSaveOutput_injection(unsigned char *options, std::vector<std::string> &data, std::string option, int32_t value) {
    // Call Original Method
    (*Options_addOptionToSaveOutput)(options, data, option, value);

    // Save Fancy Graphics
    (*Options_addOptionToSaveOutput)(options, data, "gfx_fancygraphics", *(options + Options_fancy_graphics_property_offset));

    // Save File
    unsigned char *options_file = options + Options_options_file_property_offset;
    (*OptionsFile_save)(options_file, data);
}

// MCPI's OptionsFile::getOptionStrings is broken, this is the version in v0.7.0
static std::vector<std::string> OptionsFile_getOptionStrings_injection(unsigned char *options_file) {
    // Get options.txt Path
    std::string path = *(std::string *) (options_file + OptionsFile_options_txt_path_property_offset);
    // Parse
    std::vector<std::string> ret;
    FILE *stream = fopen(path.c_str(), "r");
    char line[128];
    if (stream != NULL) {
        while (fgets(line, 0x80, stream) != NULL) {
            size_t sVar1 = strlen(line);
            if (2 < sVar1) {
                std::stringstream string_stream(line);
                while (true) {
                    std::string data;
                    std::getline(string_stream, data, ':');
                    int iVar2 = data.find_last_not_of(" \n\r\t");
                    data.erase(iVar2 + 1);
                    if (data.length() == 0) {
                        break;
                    }
                    ret.push_back(data);
                }
            }
        }
        fclose(stream);
    }
    return ret;
}

// Get New options.txt Path
#ifndef MCPI_SERVER_MODE
static char *get_new_options_txt_path() {
    static char *path = NULL;
    // Path
    if (path == NULL) {
        safe_asprintf(&path, "%s/options.txt", home_get());
    }
    // Return
    return path;
}
// Free
__attribute__((destructor)) static void _free_new_options_txt_path() {
    free(get_new_options_txt_path());
}
#else
static char *get_new_options_txt_path() {
    // Block options.txt On Servers
    return (char *) "/dev/null";
}
#endif

// Modify Option Toggles
static void OptionsPane_unknown_toggle_creating_function_injection(unsigned char *options_pane, unsigned char *unknown_object, std::string const& name, unsigned char *option) {
    // Modify
    if (name == "Fancy Graphics") {
        option = Options_Option_GRAPHICS;
    } else if (name == "Soft shadows") {
        option = Options_Option_AMBIENT_OCCLUSION;
    } else if (name == "Fancy Skies" || name == "Animated water") {
        // These have no corresponding option, so disable the toggle.
        return;
    } else if (name == "Third person camera") {
        // This isn't saved/loaded, so disable the toggle.
        return;
    } else if (name == "Lefty" || name == "Use touch screen" || name == "Split touch controls") {
        // These toggles require touch support, so disable them.
        return;
    } else if (name == "Vibrate on destroy") {
        // This toggle requires vibration support, so disable it.
        return;
    }

    // Call Original Method
    (*OptionsPane_unknown_toggle_creating_function)(options_pane, unknown_object, name, option);
}

// Add Missing Options To Options::getBooleanValue
static bool Options_getBooleanValue_injection(unsigned char *options, unsigned char *option) {
    // Check
    if (option == Options_Option_GRAPHICS) {
        return *(options + Options_fancy_graphics_property_offset);
    } else {
        // Call Original Method
        return (*Options_getBooleanValue)(options, option);
    }
}

// Add Options Button Back To Classic Start Screen
static void StartMenuScreen_init_injection(unsigned char *screen) {
    // Call Original Method
    (*StartMenuScreen_init)(screen);

    // Add Button
    std::vector<unsigned char *> *rendered_buttons = (std::vector<unsigned char *> *) (screen + Screen_rendered_buttons_property_offset);
    std::vector<unsigned char *> *selectable_buttons = (std::vector<unsigned char *> *) (screen + Screen_selectable_buttons_property_offset);
    unsigned char *options_button = screen + StartMenuScreen_options_button_property_offset;
    rendered_buttons->push_back(options_button);
    selectable_buttons->push_back(options_button);
}

// Init C++
void _init_options_cpp() {
    // NOP
    unsigned char nop_patch[4] = {0x00, 0xf0, 0x20, 0xe3}; // "nop"

    // Fix Options Screen
    if (feature_has("Fix Options Screen", server_disabled)) {
        // Fix Initial Option Button Rendering
        overwrite_call((void *) 0x24510, (void *) OptionsPane_unknown_toggle_creating_function_OptionButton_injection);

        // "Gui Scale" slider is broken, so disable it.
        patch((void *) 0x35a10, nop_patch);
        // "Vibrate on destroy" is disabled, so "Feedback" is empty, so disable it.
        patch((void *) 0x35960, nop_patch);

        // Disconnect "This works?" Slider From Difficulty
        unsigned char this_works_slider_patch[4] = {0x00, 0x30, 0xa0, 0xe3}; // "mov r3, #0x0"
        patch((void *) 0x3577c, this_works_slider_patch);

        // Modify Option Toggles
        overwrite_calls((void *) OptionsPane_unknown_toggle_creating_function, (void *) OptionsPane_unknown_toggle_creating_function_injection);

        // Add Missing Options To Options::getBooleanValue
        overwrite_calls((void *) Options_getBooleanValue, (void *) Options_getBooleanValue_injection);

        // Add Options Button Back To Classic Start Screen
        patch_address(StartMenuScreen_init_vtable_addr, (void *) StartMenuScreen_init_injection);
        // Fix Classic UI Options Button Size
        unsigned char classic_options_button_width_patch[4] = {0xa0, 0x00, 0xa0, 0xe3}; // "mov r0, #0xa0"
        patch((void *) 0x39a98, classic_options_button_width_patch);
        unsigned char classic_options_button_height_patch[4] = {0x18, 0x30, 0xa0, 0xe3}; // "mov r3, #0x18"
        patch((void *) 0x39a9c, classic_options_button_height_patch);
        // Fix Classic UI Buttons Spacing
        {
            // Join Button
            unsigned char classic_join_button_spacing_patch[4] = {0x12, 0x20, 0x83, 0xe2}; // "add r2, r3, #0x12"
            patch((void *) 0x39894, classic_join_button_spacing_patch);
            // Start Button
            unsigned char classic_start_button_spacing_patch[4] = {0x08, 0x20, 0x43, 0xe2}; // "sub r2, r3, #0x08"
            patch((void *) 0x3988c, classic_start_button_spacing_patch);
            // Options Button
            unsigned char classic_options_button_spacing_patch[4] = {0x2c, 0x30, 0x83, 0xe2}; // "add r3, r3, #0x2c"
            patch((void *) 0x39898, classic_options_button_spacing_patch);
        }
    }

    // Actually Save options.txt
    overwrite_call((void *) 0x197fc, (void *) Options_save_Options_addOptionToSaveOutput_injection);
    // Fix options.txt Path
    patch_address((void *) options_txt_path, (void *) get_new_options_txt_path());
    // When Loading, options.txt Should Be Opened In Read Mode
    patch_address((void *) options_txt_fopen_mode_when_loading, (void *) "r");
    // Fix OptionsFile::getOptionStrings
    overwrite_calls((void *) OptionsFile_getOptionStrings, (void *) OptionsFile_getOptionStrings_injection);

    // Sensitivity Loading/Saving Is Broken, Disable It
    patch((void *) 0x1931c, nop_patch);
    patch((void *) 0x1973c, nop_patch);

    // Unsplit Touch Controls Breaks Things, Never Load/Save It
    unsigned char cmp_r0_r0_patch[4] = {0x00, 0x00, 0x50, 0xe1}; // "cmp r0, r0"
    patch((void *) 0x19378, cmp_r0_r0_patch);
    patch((void *) 0x197cc, nop_patch);

    // Custom Username Is Loaded Manually, Disable Loading From options.txt
    patch((void *) 0x192ac, nop_patch);

    // Replace "feedback_vibration" Loading/Saving With "gfx_ao"
    {
        // Replace String
        static const char *new_feedback_vibration_options_txt_nam = "gfx_ao";
        patch_address((void *) feedback_vibration_options_txt_name_1, (void *) &new_feedback_vibration_options_txt_nam);
        patch_address((void *) feedback_vibration_options_txt_name_2, (void *) &new_feedback_vibration_options_txt_nam);
        // Loading
        unsigned char gfx_ao_loading_patch[4] = {(unsigned char) Options_ambient_occlusion_property_offset, 0x10, 0x84, 0xe2}; // "add param_2, r4, #OFFSET"
        patch((void *) 0x193b8, gfx_ao_loading_patch);
        // Saving
        unsigned char gfx_ao_saving_patch[4] = {(unsigned char) Options_ambient_occlusion_property_offset, 0x30, 0xd4, 0xe5}; // "ldrb r3, [r4, #OFFSET]"
        patch((void *) 0x197f8, gfx_ao_saving_patch);
    }

    // Disable "gfx_lowquality" Loading
    patch((void *) 0x19414, nop_patch);
    patch((void *) 0x1941c, nop_patch);
}
