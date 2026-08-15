Import("env")

import os
import sys


project_dir = env.subst("$PROJECT_DIR")
support_dir = os.path.join(project_dir, "support")
if support_dir not in sys.path:
    sys.path.insert(0, support_dir)

from firmware_version import collect_firmware_metadata, cpp_string_literal


if env.GetProjectOption("custom_firmware", "factory") == "custom":
    metadata = collect_firmware_metadata(project_dir)
    env.Append(
        SRC_BUILD_FLAGS=[
            "-DT_WATCH_GIT_TAG={}".format(cpp_string_literal(metadata.tag)),
            "-DT_WATCH_GIT_COMMIT={}".format(
                cpp_string_literal(metadata.commit)
            ),
            "-DT_WATCH_GIT_DIRTY={}".format(1 if metadata.dirty else 0),
            "-DT_WATCH_GIT_COMMIT_DATE_KNOWN={}".format(
                1 if metadata.commit_date_known else 0
            ),
            "-DT_WATCH_GIT_COMMIT_DAY={}".format(
                cpp_string_literal(metadata.commit_day)
            ),
            "-DT_WATCH_GIT_COMMIT_TIME={}".format(
                cpp_string_literal(metadata.commit_time)
            ),
            "-DT_WATCH_FIRMWARE_VERSION={}".format(
                cpp_string_literal(metadata.version)
            ),
        ]
    )

    print(
        "Firmware version: {} (commit {})".format(
            metadata.version, metadata.commit_date
        )
    )
