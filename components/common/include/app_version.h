#pragma once

#define APP_VERSION_MAJOR 1
#define APP_VERSION_MINOR 2
#define APP_VERSION_PATCH 0
#define APP_VERSION_STRING "1.2.0"

#define APP_HW_VERSION "S3-DK-V1.1"
#define APP_MODEL "S3-GC9A01"
#define APP_NAME "SmartDisplay"

#ifndef GIT_COMMIT
#define GIT_COMMIT "unknown"
#endif
#ifndef BUILD_TIMESTAMP
#define BUILD_TIMESTAMP __DATE__ " " __TIME__
#endif

#define IDF_TARGET_VERSION "6.1"

namespace smart_device {
struct VersionInfo {
    static constexpr const char* fw_version = APP_VERSION_STRING;
    static constexpr const char* hw_version = APP_HW_VERSION;
    static constexpr const char* model = APP_MODEL;
    static constexpr const char* name = APP_NAME;
    static const char* git_commit();
    static const char* build_time();
    static const char* idf_version();
};
} // namespace smart_device
