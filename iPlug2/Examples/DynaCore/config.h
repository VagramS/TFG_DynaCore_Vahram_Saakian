#pragma once

// ======================================================
// DynaCore - Main Plugin Config
// Target formats in this project: APP + AUv2 + VST3
// (AAX / CLAP / VST2 / AUv3 removed from targets)
// ======================================================

#define PLUG_NAME "DynaCore"
#define PLUG_MFR "UCM"
#define PLUG_VERSION_HEX 0x00010000
#define PLUG_VERSION_STR "1.0.0"

// 4-char IDs (must be exactly 4 chars)
#define PLUG_UNIQUE_ID 'DnCr'
#define PLUG_MFR_ID 'UCM1'

// Public metadata
#define PLUG_URL_STR "https://github.com/VagramS/TFG_DynaCore_Vahram_Saakian"
#define PLUG_EMAIL_STR "vahramsa@ucm.es"
#define PLUG_COPYRIGHT_STR "Copyright 2025/26 Vahram Saakian"

// Must match class name in DynaCore.h / DynaCore.cpp
#define PLUG_CLASS_NAME DynaCore

// Bundle metadata
#define BUNDLE_NAME "DynaCore"
#define BUNDLE_MFR "UCM1"
#define BUNDLE_DOMAIN "com"

// I/O
#define PLUG_CHANNEL_IO "1-1 2-2"

// Plugin behavior
#define PLUG_LATENCY 0
#define PLUG_TYPE 0
#define PLUG_DOES_MIDI_IN 0
#define PLUG_DOES_MIDI_OUT 0
#define PLUG_DOES_MPE 0
#define PLUG_DOES_STATE_CHUNKS 0
#define PLUG_HAS_UI 1
#define PLUG_WIDTH 1000
#define PLUG_HEIGHT 600
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0
#define PLUG_HOST_RESIZE 0

// AUv2 entry points (must be unique per plugin)
#define AUV2_ENTRY DynaCore_Entry
#define AUV2_ENTRY_STR "DynaCore_Entry"
#define AUV2_FACTORY DynaCore_Factory
#define AUV2_VIEW_CLASS DynaCore_View
#define AUV2_VIEW_CLASS_STR "DynaCore_View"

// VST3
#define VST3_SUBCATEGORY "Fx"

// Standalone APP
#define APP_NUM_CHANNELS 2
#define APP_N_VECTOR_WAIT 0
#define APP_MULT 1
#define APP_COPY_AUV3 0
#define APP_SIGNAL_VECTOR_SIZE 64

// ======================================================
// Resources / Fonts
// ======================================================

#define ROBOTO_FN "Roboto-Regular.ttf"
#define INTER_SEMI_BOLD_FN "Inter-Semi-Bold.ttf"
#define INTER_REGULAR_FN "Inter-Regular.ttf"
#define INTER_MEDIUM_FN "Inter-Medium.ttf"

// ======================================================
// DynaCore Visual Resources
// ======================================================

#define MAIN_BACKGROUND_FN "main_background.png"

// === COMPRESSOR ===
#define COMP_OFF_FN "compress_off.png"
#define COMP_ON_FN "compress_on.png"

// === PRESETS ===
#define SELECT_PRESET_FN "select_preset.png"
#define PRESETS_PAGE_FN "presets_page.png"
#define REVERT_TO_DEFAULT_FN "revert_to_default.png"

#define PRESET_GROUP_VOCALS_SELECT_FN "preset_group_vocals_select.png"
#define PRESET_GROUP_PADS_SELECT_FN "preset_group_pads_select.png"
#define PRESET_GROUP_DRUMS_SELECT_FN "preset_group_drums_select.png"
#define PRESET_GROUP_EXP_SELECT_FN "preset_group_exp_select.png"

#define PRESET_VOCALS_LABLE_FN "preset_vocals_label.png"
#define PRESET_PADS_LABLE_FN "preset_pads_label.png"
#define PRESET_DRUMS_LABLE_FN "preset_drums_label.png"
#define PRESET_EXP_LABLE_FN "preset_exp_label.png"

#define PRESET_GROUP_SELECT_ARROW_FN "preset_group_select_arrow.png"
#define PRESET_DIVIDER_FN "preset_divider.png"

#define PRESET_FROM_GROUP_SELECT_FIRST_FN "preset_from_group_select_first.png"
#define PRESET_FROM_GROUP_SELECT_REST_FN "preset_from_group_select_rest.png"

#define PRESET_POINTER_FN "preset_pointer.png"

#define NEXT_PRESET_FN "next_preset.png"
#define PREVIOUS_PRESET_FN "previous_preset.png"

// === BYPASS ===
#define BYPASS_OFF_FN "bypass_off.png"
#define BYPASS_ON_FN "bypass_on.png"

// === MODULES ===
#define MODULE_OFF_FN "module_off.png"
#define MODULE_ON_FN "module_on.png"

// === KNOBS ===
#define BIG_KNOB_FN "big_knob.png"
#define MID_KNOB_FN "mid_knob.png"
#define SMALL_KNOB_FN "small_knob.png"
