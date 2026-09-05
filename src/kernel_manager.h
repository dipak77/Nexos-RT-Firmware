#pragma once

#include <Arduino.h>
#include "mk.h"
#include "mk_enclave.h"

struct KernelStats {
    const char* os_name;
    const char* version;
    uint32_t uptime_seconds;
    uint32_t free_heap_kb;
    uint32_t min_free_heap_kb;
    uint32_t psram_size_kb;
    uint32_t free_psram_kb;
    uint8_t core_count;
    uint8_t active_tasks_count;
    bool is_preemptive;
    bool healthy;
    char health_text[24];
};

class KernelManager {
public:
    static KernelManager& getInstance() {
        static KernelManager instance;
        return instance;
    }

    bool init();
    bool startTasks(mk_task_entry_t gui, mk_task_entry_t sys, mk_task_entry_t cli);

    const char* getActiveKernelName() const { return MK_CONFIG_OS_NAME; }
    KernelStats getStats();
    void printKernelInfo();
    void delayMs(uint32_t ms) const;
    void heartbeat(const char* task_name);
    int currentCore() const;

    bool lockDisplay(uint32_t timeout_ms);
    bool lockDisplayRetry(uint32_t timeout_ms, int tries);
    void unlockDisplay();

    mk_task_handle_t guiHandle() const { return gui_; }
    mk_task_handle_t sysHandle() const { return sys_; }
    mk_task_handle_t cliHandle() const { return cli_; }

private:
    KernelManager() : initialized(false), display_lock_(nullptr), gui_(nullptr), sys_(nullptr), cli_(nullptr),
                      gui_enc_(nullptr), sys_enc_(nullptr), cli_enc_(nullptr) {}
    bool initialized;
    mk_mutex_t* display_lock_;
    mk_task_handle_t gui_;
    mk_task_handle_t sys_;
    mk_task_handle_t cli_;
    // V2 bound enclaves (1:1 with tasks above). Rollback reclaims these, which
    // deletes the task and frees the slot — never raw-delete a bound task.
    mk_enclave_desc_t* gui_enc_;
    mk_enclave_desc_t* sys_enc_;
    mk_enclave_desc_t* cli_enc_;

    mk_task_handle_t spawn(const char* name, uint8_t prio, size_t stack, int core, mk_task_entry_t entry,
                           uint8_t enc_type, uint32_t enc_caps);
    void rollback();
    void rollback_one(const char* wdt_name, mk_task_handle_t& task, mk_enclave_desc_t*& enc);
};

class DisplayGuard {
public:
    explicit DisplayGuard(uint32_t ms)
        : locked_(KernelManager::getInstance().lockDisplayRetry(ms, 6)) {}
    ~DisplayGuard() {
        if (locked_) KernelManager::getInstance().unlockDisplay();
    }
    explicit operator bool() const { return locked_; }
private:
    bool locked_;
};
