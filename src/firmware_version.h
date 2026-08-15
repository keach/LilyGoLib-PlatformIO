#pragma once

#ifndef T_WATCH_GIT_TAG
#define T_WATCH_GIT_TAG "unknown"
#endif

#ifndef T_WATCH_GIT_COMMIT
#define T_WATCH_GIT_COMMIT "unknown"
#endif

#ifndef T_WATCH_GIT_DIRTY
#define T_WATCH_GIT_DIRTY 0
#endif

#ifndef T_WATCH_GIT_COMMIT_DATE_KNOWN
#define T_WATCH_GIT_COMMIT_DATE_KNOWN 0
#endif

#ifndef T_WATCH_GIT_COMMIT_DAY
#define T_WATCH_GIT_COMMIT_DAY "unknown"
#endif

#ifndef T_WATCH_GIT_COMMIT_TIME
#define T_WATCH_GIT_COMMIT_TIME "unknown"
#endif

#ifndef T_WATCH_FIRMWARE_VERSION
#define T_WATCH_FIRMWARE_VERSION "unknown+unknown"
#endif

namespace FirmwareVersion {

inline constexpr const char *kTag = T_WATCH_GIT_TAG;
inline constexpr const char *kCommit = T_WATCH_GIT_COMMIT;
inline constexpr bool kDirty = T_WATCH_GIT_DIRTY != 0;
#if T_WATCH_GIT_COMMIT_DATE_KNOWN
inline constexpr const char *kCommitDate =
    T_WATCH_GIT_COMMIT_DAY " " T_WATCH_GIT_COMMIT_TIME " JST";
#else
inline constexpr const char *kCommitDate = "unknown";
#endif
inline constexpr const char *kVersion = T_WATCH_FIRMWARE_VERSION;

}  // namespace FirmwareVersion
