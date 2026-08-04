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

    # Generate an include-only copy of the existing clock application with only
    # its two free Arduino entry-point definitions renamed. Using textual macros
    # would also rewrite calls such as instance.loop(), so keep all other tokens
    # byte-for-byte unchanged.
    source_path = os.path.join(source_dir, "main.cpp")
    generated_path = os.path.join(source_dir, "generated_clock_main.inc")
    with open(source_path, "r", encoding="utf-8") as source_file:
        generated_source = source_file.read()

    replacements = {
        "\nvoid setup()\n": "\nvoid clockApplicationSetup()\n",
        "\nvoid loop()\n": "\nvoid clockApplicationLoop()\n",
    }
    for original, replacement in replacements.items():
        count = generated_source.count(original)
        if count != 1:
            raise ValueError(
                "Expected exactly one {!r} definition in {} but found {}".format(
                    original.strip(), source_path, count
                )
            )
        generated_source = generated_source.replace(original, replacement, 1)

    with open(generated_path, "w", encoding="utf-8", newline="\n") as generated_file:
        generated_file.write(generated_source)

    # main_integrated.cpp includes the generated .inc in the same translation
    # unit. Exclude the standalone main.cpp object to avoid duplicate symbols.
    env.Replace(SRC_FILTER=["+<*>", "-<main.cpp>"])
else:
    raise ValueError("custom_firmware must be either 'factory' or 'custom'")

if not os.path.isdir(source_dir):
    raise FileNotFoundError("Firmware source directory not found: {}".format(source_dir))

print("Firmware source: {} ({})".format(firmware, source_dir))
