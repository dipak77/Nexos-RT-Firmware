#include "app_error.h"
#include "app_version.h"

namespace smart_device {

const char* app_error_to_string(AppError err) {
    switch(err){
        case AppError::OK: return "OK";
        case AppError::WIFI_TIMEOUT: return "WIFI_TIMEOUT";
        case AppError::WIFI_AUTH_FAILED: return "WIFI_AUTH_FAILED";
        case AppError::TIME_NOT_SYNCED: return "TIME_NOT_SYNCED";
        case AppError::COMMAND_UNKNOWN: return "COMMAND_UNKNOWN";
        default: return "ERROR";
    }
}

ErrorDomain app_error_domain(AppError err){
    auto value = static_cast<uint32_t>(err);
    if(value >= 5000) return ErrorDomain::OTA;
    if(value >= 4000) return ErrorDomain::COMMAND;
    if(value >= 3000) return ErrorDomain::TIME;
    if(value >= 2200) return ErrorDomain::USB;
    if(value >= 2100) return ErrorDomain::BLE;
    if(value >= 2000) return ErrorDomain::WIFI;
    if(value >= 1400) return ErrorDomain::I2C;
    if(value >= 1300) return ErrorDomain::SPI;
    if(value >= 1200) return ErrorDomain::DISPLAY;
    if(value >= 1100) return ErrorDomain::STORAGE;
    if(value >= 1000) return ErrorDomain::SYSTEM;
    return ErrorDomain::SUCCESS;
}

const char* VersionInfo::git_commit(){ return GIT_COMMIT; }
const char* VersionInfo::build_time(){ return BUILD_TIMESTAMP; }
const char* VersionInfo::idf_version(){ return IDF_TARGET_VERSION; }

} // namespace smart_device
