#include <stdint.h>
#include <stdio.h>

#include <libcore/libcore.h>

#include "extra.h"
#include "server/server.h"

static int mob_spawning = 0;
static uint32_t getSpawnMobs_injection(__attribute__((unused)) int32_t obj) {
    return mob_spawning;
}

static int is_right_click = 0;
void extra_set_is_right_click(int val) {
    is_right_click = val;
}

typedef void (*releaseUsingItem_t)(unsigned char *t, unsigned char *player);
static releaseUsingItem_t survival_releaseUsingItem = (releaseUsingItem_t) 0x1a598;
static releaseUsingItem_t creative_releaseUsingItem = (releaseUsingItem_t) 0x1b1a0;

typedef void (*handle_input_t)(unsigned char *, unsigned char *, unsigned char *, unsigned char *);
static handle_input_t handle_input = (handle_input_t) 0x15ffc;
static void *handle_input_original = NULL;

static int is_survival = -1;

static void handle_input_injection(unsigned char *param_1, unsigned char *param_2, unsigned char *param_3, unsigned char *param_4) {
    // Call Original Method
    revert_overwrite((void *) handle_input, handle_input_original);
    (*handle_input)(param_1, param_2, param_3, param_4);
    revert_overwrite((void *) handle_input, handle_input_original);

    // GameMode Is Offset From param_1 By 0x160
    // Player Is Offset From param_1 By 0x18c
    if (!is_right_click) {
        unsigned char *game_mode = *(unsigned char **) (param_1 + 0x160);
        unsigned char *player = *(unsigned char **) (param_1 + 0x18c);
        if (player != NULL && game_mode != NULL) {
            (*(is_survival ? survival_releaseUsingItem : creative_releaseUsingItem))(game_mode, player);
        }
    }

    extra_clear_input();
}

typedef void (*tickItemDrop_t)(unsigned char *);
static tickItemDrop_t tickItemDrop = (tickItemDrop_t) 0x27778;
static void *tickItemDrop_original = NULL;

#include <SDL/SDL_events.h>

static void tickItemDrop_injection(unsigned char *this) {
    if (SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE) {
        revert_overwrite((void *) tickItemDrop, tickItemDrop_original);
        (*tickItemDrop)(this);
        revert_overwrite((void *) tickItemDrop, tickItemDrop_original);
    }
}

typedef void (*handleClick_t)(unsigned char *, unsigned char *, unsigned char *, unsigned char *);
static handleClick_t handleClick = (handleClick_t) 0x2599c;
static void *handleClick_original = NULL;

static void handleClick_injection(unsigned char *this, unsigned char *param_2, unsigned char *param_3, unsigned char *param_4) {
    if (SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE) {
        revert_overwrite((void *) handleClick, handleClick_original);
        (*handleClick)(this, param_2, param_3, param_4);
        revert_overwrite((void *) handleClick, handleClick_original);
    }
}

static void *Creator = (void *) 0x1a044;
static void *SurvivalMode = (void *) 0x1b7d8;
static void *CreativeMode = (void *) 0x1b258;

// Patch Game Mode
static void set_is_survival(int new_is_survival) {
    if (is_survival != new_is_survival) {
        fprintf(stderr, "Setting Game Mode: %s\n", new_is_survival ? "Survival" : "Creative");

        // Correct Inventpry UI
        unsigned char inventory_patch[4] = {new_is_survival ? 0x00 : 0x01, 0x30, 0xa0, 0xe3};
        patch((void *) 0x16efc, inventory_patch);

        // Use Correct Size For GameMode Object
        unsigned char size_patch[4] = {new_is_survival ? 0x24 : 0x18, 0x00, 0xa0, 0xe3};
        patch((void *) 0x16ee4, size_patch);

        // Replace Creator Constructor With CreatorMode Or SurvivalMode Constructor
        overwrite(Creator, new_is_survival ? SurvivalMode : CreativeMode);

        is_survival = new_is_survival;
    }
}

typedef void (*setIsCreativeMode_t)(unsigned char *, int32_t);
static setIsCreativeMode_t setIsCreativeMode = (setIsCreativeMode_t) 0x16ec4;
static void *setIsCreativeMode_original = NULL;

static void setIsCreativeMode_injection(unsigned char *this, int32_t new_game_mode) {
    set_is_survival(!new_game_mode);

    revert_overwrite((void *) setIsCreativeMode, setIsCreativeMode_original);
    (*setIsCreativeMode)(this, new_game_mode);
    revert_overwrite((void *) setIsCreativeMode, setIsCreativeMode_original);
}

static char *get_username() {
    char *username = getenv("MCPI_USERNAME");
    if (username == NULL) {
        username = "StevePi";
    }
    return username;
}

typedef void (*minecraft_init_t)(unsigned char *);
static minecraft_init_t minecraft_init = (minecraft_init_t) 0x1700c;
static void *minecraft_init_original = NULL;

static void minecraft_init_injection(unsigned char *this) {
    revert_overwrite((void *) minecraft_init, minecraft_init_original);
    (*minecraft_init)(this);
    revert_overwrite((void *) minecraft_init, minecraft_init_original);

    // Enable Fancy Graphics
    *(this + 83) = 1;
}

// Is Dedicated Server
static int is_server = 0;

// Check For Feature
int extra_has_feature(const char *name) {
    if (is_server) {
        // Enable All Features In Server
        return 1;
    } else {
        char *env = getenv("MCPI_FEATURES");
        char *features = strdup(env != NULL ? env : "");
        char *tok = strtok(features, "|");
        int ret = 0;
        while (tok != NULL) {
            if (strcmp(tok, name) == 0) {
                ret = 1;
                break;
            }
            tok = strtok(NULL, "|");
        }
        free(features);
        fprintf(stderr, "Feature: %s: %s\n", name, ret ? "Enabled" : "Disabled");
        return ret;
    }
}

int extra_get_is_server() {
    return getenv("MCPI_SERVER") != NULL;
}

__attribute__((constructor)) static void init() {
    is_server = extra_get_is_server();
    if (is_server) {
        server_init();
    }

    if (extra_has_feature("Touch GUI")) {
        // Use Touch UI
        unsigned char touch_gui_patch[4] = {0x01, 0x00, 0x50, 0xe3};
        patch((void *) 0x292fc, touch_gui_patch);
    }

    // Get Default Game Mode
    int default_game_mode;
    if (is_server) {
        default_game_mode = server_get_default_game_mode();
    } else {
        default_game_mode = !extra_has_feature("Survival Mode");
    }

    // Dyanmic Game Mode Switching
    set_is_survival(!default_game_mode);
    setIsCreativeMode_original = overwrite((void *) setIsCreativeMode, setIsCreativeMode_injection);

    // Set Default Game Mode
    unsigned char default_game_mode_patch[4] = {default_game_mode ? 0x01 : 0x00, 0x30, 0xa0, 0xe3};
    patch((void *) 0x3d9b8, default_game_mode_patch);
    patch((void *) 0x38a78, default_game_mode_patch);

    // Disable Item Dropping When Cursor Is Hidden
    tickItemDrop_original = overwrite((void *) tickItemDrop, tickItemDrop_injection);
    // Disable Opening Inventory Using The Cursor When Cursor Is Hidden
    handleClick_original = overwrite((void *) handleClick, handleClick_injection);

    if (extra_has_feature("Fix Bow & Arrow")) {
        // Fix Bow
        handle_input_original = overwrite((void *) handle_input, handle_input_injection);
    }

    if (extra_has_feature("Fix Attacking")) {
        // Allow Attacking Mobs
        unsigned char attacking_patch[4] = {0x00, 0xf0, 0x20, 0xe3};
        patch((void *) 0x162d4, attacking_patch);
        unsigned char instamine_patch[4] = {0x61, 0x00, 0x00, 0xea};
        patch((void *) 0x15b0c, instamine_patch);
    }

    if (is_server) {
        mob_spawning = server_get_mob_spawning();
    } else {
        mob_spawning = extra_has_feature("Mob Spawning");
    }
    // Set Mob Spawning
    overwrite((void *) 0xbabec, getSpawnMobs_injection);

    // Replace CreatorLevel With ServerLevel (This Fixes Beds And Mob Spawning)
    unsigned char patch_data_4[4] = {0x68, 0x7e, 0x01, 0xeb};
    patch((void *) 0x16f84, patch_data_4);

    // Allocate Correct Size For ServerLevel
    unsigned char patch_data_5[4] = {0x94, 0x0b, 0x00, 0x00};
    patch((void *) 0x17004, patch_data_5);

    if (extra_has_feature("Fancy Graphics")) {
        // Enable Fancy Graphics
        minecraft_init_original = overwrite((void *) minecraft_init, minecraft_init_injection);
    }

    // Allow Connecting To Non-Pi Servers
    unsigned char patch_data_9[4] = {0x0f, 0x00, 0x00, 0xea};
    patch((void *) 0x6dc70, patch_data_9);

    // Change Username
    const char *username;
    if (is_server) {
        // MOTD is Username
        username = server_get_motd();
    } else {
        username = get_username();
        fprintf(stderr, "Setting Username: %s\n", username);
    }
    patch_address((void *) 0x18fd4, (void *) username);

    if (extra_has_feature("Disable Autojump By Default")) {
        // Disable Autojump By Default
        unsigned char autojump_patch[4] = {0x00, 0x30, 0xa0, 0xe3};
        patch((void *) 0x44b90, autojump_patch);
    }

    if (extra_has_feature("Show Block Outlines")) {
        // Show Block Outlines
        unsigned char outline_patch[4] = {0x00, 0xf0, 0x20, 0xe3};
        patch((void *) 0x4a214, outline_patch);
    }
}
