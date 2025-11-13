#define PLUG_NAME "DynaCore"
#define PLUG_MFR "AcmeInc"
#define PLUG_VERSION_HEX 0x00010000
#define PLUG_VERSION_STR "1.0.0"
#define PLUG_UNIQUE_ID 'Ipef'
#define PLUG_MFR_ID 'Acme'
#define PLUG_URL_STR "https://iplug2.github.io"
#define PLUG_EMAIL_STR "spam@me.com"
#define PLUG_COPYRIGHT_STR "Copyright 2020 Acme Inc"
#define PLUG_CLASS_NAME IPlugEffect

#define BUNDLE_NAME "IPlugEffect"
#define BUNDLE_MFR "AcmeInc"
#define BUNDLE_DOMAIN "com"

#define SHARED_RESOURCES_SUBPATH "IPlugEffect"

#define PLUG_CHANNEL_IO "1-1 2-2"

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

#define AUV2_ENTRY IPlugEffect_Entry
#define AUV2_ENTRY_STR "IPlugEffect_Entry"
#define AUV2_FACTORY IPlugEffect_Factory
#define AUV2_VIEW_CLASS IPlugEffect_View
#define AUV2_VIEW_CLASS_STR "IPlugEffect_View"

#define AAX_TYPE_IDS 'IEF1', 'IEF2'
#define AAX_TYPE_IDS_AUDIOSUITE 'IEA1', 'IEA2'
#define AAX_PLUG_MFR_STR "Acme"
#define AAX_PLUG_NAME_STR "IPlugEffect\nIPEF"
#define AAX_PLUG_CATEGORY_STR "Effect"
#define AAX_DOES_AUDIOSUITE 1

#define VST3_SUBCATEGORY "Fx"

#define CLAP_MANUAL_URL "https://iplug2.github.io/manuals/example_manual.pdf"
#define CLAP_SUPPORT_URL "https://github.com/iPlug2/iPlug2/wiki"
#define CLAP_DESCRIPTION "A simple audio effect for modifying gain"
#define CLAP_FEATURES "audio-effect"//, "utility"

#define APP_NUM_CHANNELS 2
#define APP_N_VECTOR_WAIT 0
#define APP_MULT 1
#define APP_COPY_AUV3 0
#define APP_SIGNAL_VECTOR_SIZE 64

#define ROBOTO_FN "Roboto-Regular.ttf"
#define INTER_FN  "Inter-Medium.otf"


// DYNACORE VISUALS

#define MAIN_BACKGROUND_FN "main_background.png"

// COMPRESSOR
#define COMP_OFF_FN "compress_off.png"
#define COMP_ON_FN "compress_on.png"

// PRESETS
#define SELECT_PRESET_FN "select_preset.png"
#define PRESETS_PAGE_FN "presets_page.png"

#define REVERT_TO_DEFAULT_FN "revert_to_default.png"

#define PRESET_GROUP_VOCALS_SELECT_FN "preset_group_vocals_select.png"
#define PRESET_GROUP_PADS_SELECT_FN "preset_group_pads_select.png"
#define PRESET_GROUP_DRUMS_SELECT_FN "preset_group_drums_select.png"
#define PRESET_GROUP_EXP_SELECT_FN "preset_group_exp_select.png"

#define PRESET_GROUP_SELECT_ARROW_FN "preset_group_select_arrow.png"

#define PRESET_divider_FN "preset_divider.png"

#define PRESET_VOCALS_LABLE_FN "preset_vocals_label.png"
#define PRESET_PADS_LABLE_FN "preset_pads_label.png"
#define PRESET_DRUMS_LABLE_FN "preset_drums_label.png"
#define PRESET_EXP_LABLE_FN "preset_exp_label.png"

// BYPASS
#define BYPASS_OFF_FN "bypass_off.png"
#define BYPASS_ON_FN "bypass_on.png"

// MODULES
#define MODULE_OFF_FN "module_off.png"
#define MODULE_ON_FN "module_on.png"

// KNOBS
#define BIG_KNOB_FN "big_knob.png"
#define MID_KNOB_FN "mid_knob.png"
#define SMALL_KNOB_FN "small_knob.png"

