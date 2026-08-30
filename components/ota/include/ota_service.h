#pragma once
#include "common/result.h"
#include <string>

namespace smart_device {
namespace ota {

enum class OtaState { IDLE, CHECKING, DOWNLOADING, VERIFYING, READY, FAILED };

struct OtaStatus {
    OtaState state{OtaState::IDLE};
    int progress_percent{0};
    char version[32]{""};
    char error[64]{""};
};

class OtaService {
public:
    static OtaService& instance();
    Result<void> initialize();
    // Blocking — use only from dedicated OTA task (see check_and_update_async)
    Result<void> check_and_update(const std::string& url, const std::string& cert_pem = "");
    // Non-blocking: spawns OTA task on Core0, feeds watchdog, enforces HTTPS + cert
    Result<void> check_and_update_async(const std::string& url, const std::string& cert_pem = "");
    Result<void> rollback();
    OtaStatus status() const { return status_; }
    bool is_in_progress() const { return status_.state==OtaState::DOWNLOADING || status_.state==OtaState::CHECKING; }

private:
    OtaStatus status_{};
    bool initialized_{false};
    static void ota_task_entry(void* arg);
};

} // namespace ota
} // namespace smart_device
