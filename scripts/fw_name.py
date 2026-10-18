# PlatformIO extra_script: after the build, copies the firmware with version number to firmware/
#   firmware/obd2mqtt-v<VERSION>-<env>-ota.bin      for the update via the web interface / online update
#   firmware/obd2mqtt-v<VERSION>-<env>-factory.bin  complete image for the first USB flash at address 0x0
import os, re, shutil, subprocess
Import("env")

def version():
    for f in env.get("BUILD_FLAGS", []):
        m = re.search(r'FW_VERSION=\\?"([^"\\]+)', str(f))
        if m:
            return m.group(1)
    return "dev"

def copy_fw(source, target, env):
    out = os.path.join(env.subst("$PROJECT_DIR"), "firmware")
    os.makedirs(out, exist_ok=True)
    base = os.path.join(out, "obd2mqtt-v%s-%s" % (version(), env.subst("$PIOENV")))
    shutil.copyfile(str(target[0]), base + "-ota.bin")
    print("Firmware copied to", base + "-ota.bin")
    # factory image: bootloader + partition table + boot_app0 + app, at the offsets PlatformIO flashes them
    # (bootloader at 0x1000 on the ESP32, at 0x0 on the ESP32-S3/C3)
    parts = []
    for off, img in env.get("FLASH_EXTRA_IMAGES", []):
        parts += [env.subst(off), env.subst(img)]
    parts += [env.subst("$ESP32_APP_OFFSET"), str(target[0])]
    cmd = [env.subst("$PYTHONEXE"), env.subst("$OBJCOPY"), "--chip", env.BoardConfig().get("build.mcu"),
           "merge_bin", "-o", base + "-factory.bin"] + parts
    if subprocess.call(cmd, stdout=subprocess.DEVNULL) == 0:
        print("Factory image", base + "-factory.bin")
    else:
        print("Factory image could not be created:", " ".join(cmd))

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_fw)
