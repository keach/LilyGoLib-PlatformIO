Import("env")

import os


firmware = env.GetProjectOption("custom_firmware", "factory")

if firmware == "factory":
    # Keep PROJECT_SRC_DIR as configured in platformio.ini. PlatformIO must see
    # an .ino directory before it creates the project library builder, otherwise
    # the generated .ino.cpp file can be omitted from the final link.
    source_dir = env.subst("$PROJECT_SRC_DIR")
elif firmware == "custom":
    source_dir = os.path.join(env.subst("$PROJECT_DIR"), "src")
    env.Replace(PROJECT_SRC_DIR=source_dir, PROJECTSRC_DIR=source_dir)

    # main_integrated.cpp includes main.cpp in the same translation unit so the
    # kitchen timer can reuse the existing clock application's internal screen,
    # sleep, and power-management helpers without duplicating the large source
    # file. Exclude the standalone main.cpp object to avoid duplicate symbols.
    env.Replace(SRC_FILTER=["+<*>", "-<main.cpp>"])
else:
    raise ValueError("custom_firmware must be either 'factory' or 'custom'")

if not os.path.isdir(source_dir):
    raise FileNotFoundError("Firmware source directory not found: {}".format(source_dir))

print("Firmware source: {} ({})".format(firmware, source_dir))
