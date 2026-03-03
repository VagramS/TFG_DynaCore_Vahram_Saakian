#!/usr/bin/python3
# prepare_resources-mac.py
# Unify identity for APP/AUv2/VST3 using PLUG_NAME, while keeping template plist filenames based on BUNDLE_NAME.

kAudioUnitType_MusicDevice      = "aumu"
kAudioUnitType_MusicEffect      = "aumf"
kAudioUnitType_Effect           = "aufx"
kAudioUnitType_MIDIProcessor    = "aumi"

import plistlib, os, sys, shutil

scriptpath = os.path.dirname(os.path.realpath(__file__))
projectpath = os.path.abspath(os.path.join(scriptpath, os.pardir))
IPLUG2_ROOT = "../../.."

sys.path.insert(0, os.path.join(os.getcwd(), IPLUG2_ROOT + '/Scripts'))
from parse_config import parse_config, parse_xcconfig

def load_plist(path: str):
  with open(path, 'rb') as fp:
    return plistlib.load(fp)

def save_plist(path: str, obj):
  with open(path, 'wb') as fp:
    plistlib.dump(obj, fp)

def make_id(cfg, kind: str, product_name: str):
  # com.<domain>.<mfr>.<kind>.<product>
  return f"{cfg['BUNDLE_DOMAIN']}.{cfg['BUNDLE_MFR']}.{kind}.{product_name}"

def stamp_common_bundle_keys(pl: dict, *, executable: str, bundle_id: str, bundle_name: str,
                             info_string: str, version_str: str, signature_4cc: str,
                             min_sys_ver: str):
  # Common across APP/AU/VST3
  pl['CFBundleExecutable'] = executable
  pl['CFBundleIdentifier'] = bundle_id
  pl['CFBundleName'] = bundle_name
  pl['CFBundleGetInfoString'] = info_string
  pl['CFBundleVersion'] = version_str
  pl['CFBundleShortVersionString'] = version_str
  pl['LSMinimumSystemVersion'] = min_sys_ver

  # These are common in iPlug2 templates; keep consistent
  pl['CFBundlePackageType'] = "BNDL"
  pl['CFBundleSignature'] = signature_4cc
  pl['CSResourcesFileMapped'] = True

def main():
  cfg = parse_config(projectpath)
  xc = parse_xcconfig(os.path.join(os.getcwd(), IPLUG2_ROOT + '/common-mac.xcconfig'))

  TEMPLATE_BASE = cfg['BUNDLE_NAME']      # legacy template base ("IPlugEffect")
  PRODUCT_NAME  = cfg['PLUG_NAME']        # actual product name ("DynaCore")
  PLUG_CLASS    = cfg['PLUG_CLASS_NAME']  # may still be "IPlugEffect" (ok)
  MFR_STR       = cfg['PLUG_MFR']         # e.g. "UCM" (string)
  MFR_ID        = cfg['PLUG_MFR_ID']      # e.g. "UCM1" (4cc)
  UNIQUE_ID     = cfg['PLUG_UNIQUE_ID']   # e.g. "DnCr" (4cc)

  min_sys_ver = xc['DEPLOYMENT_TARGET']
  ver_str = cfg['FULL_VER_STR']
  info_string = f"{PRODUCT_NAME} v{ver_str} {cfg['PLUG_COPYRIGHT_STR']}"

  # -------------------------
  # Copy resources into bundle (unchanged)
  print("Copying resources ...")
  if cfg['PLUG_SHARED_RESOURCES']:
    dst = os.path.expanduser("~") + "/Music/" + PRODUCT_NAME + "/Resources"
  else:
    dst = os.path.join(os.environ["TARGET_BUILD_DIR"], os.environ["UNLOCALIZED_RESOURCES_FOLDER_PATH"].lstrip('/'))

  os.makedirs(dst, exist_ok=True)

  if os.path.exists(projectpath + "/resources/img/"):
    for img in os.listdir(projectpath + "/resources/img/"):
      shutil.copy(projectpath + "/resources/img/" + img, dst)

  if os.path.exists(projectpath + "/resources/fonts/"):
    for font in os.listdir(projectpath + "/resources/fonts/"):
      shutil.copy(projectpath + "/resources/fonts/" + font, dst)

  print("Processing Info.plist files...")

  # -------------------------
  # VST3
  vst3_path = projectpath + "/resources/" + TEMPLATE_BASE + "-VST3-Info.plist"
  vst3 = load_plist(vst3_path)
  stamp_common_bundle_keys(
    vst3,
    executable=PRODUCT_NAME,
    bundle_id=make_id(cfg, "vst3", PRODUCT_NAME),
    bundle_name=PRODUCT_NAME,
    info_string=info_string,
    version_str=ver_str,
    signature_4cc=UNIQUE_ID,
    min_sys_ver=min_sys_ver
  )
  save_plist(vst3_path, vst3)

  # -------------------------
  # AUv2
  au_path = projectpath + "/resources/" + TEMPLATE_BASE + "-AU-Info.plist"
  au = load_plist(au_path)
  stamp_common_bundle_keys(
    au,
    executable=PRODUCT_NAME,
    bundle_id=make_id(cfg, "audiounit", PRODUCT_NAME),
    bundle_name=PRODUCT_NAME,
    info_string=info_string,
    version_str=ver_str,
    signature_4cc=UNIQUE_ID,
    min_sys_ver=min_sys_ver
  )

  # Component type mapping (iPlug2 convention)
  if cfg['PLUG_TYPE'] == 0:
    if cfg['PLUG_DOES_MIDI_IN']:
      component_type = kAudioUnitType_MusicEffect
    else:
      component_type = kAudioUnitType_Effect
  elif cfg['PLUG_TYPE'] == 1:
    component_type = kAudioUnitType_MusicDevice
  else:
    component_type = kAudioUnitType_MIDIProcessor

  au['AudioUnit Version'] = cfg['PLUG_VERSION_HEX']

  au['AudioComponents'] = [{}]
  au['AudioComponents'][0]['description'] = PRODUCT_NAME
  au['AudioComponents'][0]['factoryFunction'] = cfg['AUV2_FACTORY']
  au['AudioComponents'][0]['manufacturer'] = MFR_ID
  au['AudioComponents'][0]['name'] = f"{MFR_STR}: {PRODUCT_NAME}"
  au['AudioComponents'][0]['subtype'] = UNIQUE_ID
  au['AudioComponents'][0]['type'] = component_type
  au['AudioComponents'][0]['version'] = cfg['PLUG_VERSION_INT']
  au['AudioComponents'][0]['sandboxSafe'] = True

  # Must match a real Obj-C class inside the AU binary.
  # In iPlug2 this is usually <PLUG_CLASS_NAME>_View (even if product is renamed).
  au['NSPrincipalClass'] = PLUG_CLASS + "_View"

  save_plist(au_path, au)

  # -------------------------
  # APP
  app_path = projectpath + "/resources/" + TEMPLATE_BASE + "-macOS-Info.plist"
  app = load_plist(app_path)
  stamp_common_bundle_keys(
    app,
    executable=PRODUCT_NAME,
    bundle_id=make_id(cfg, "app", PRODUCT_NAME),
    bundle_name=PRODUCT_NAME,
    info_string=info_string,
    version_str=ver_str,
    signature_4cc=UNIQUE_ID,
    min_sys_ver=min_sys_ver
  )

  app['NSPrincipalClass'] = "SWELLApplication"
  app['LSApplicationCategoryType'] = "public.app-category.music"
  # Nib name is still legacy unless you renamed nib too
  app['NSMainNibFile'] = TEMPLATE_BASE + "-macOS-MainMenu"
  # Icon: expects PRODUCT_NAME.icns in bundle Resources
  app['CFBundleIconFile'] = PRODUCT_NAME + ".icns"
  app['NSMicrophoneUsageDescription'] = "This app needs mic access to process audio."
  save_plist(app_path, app)

if __name__ == '__main__':
  main()
