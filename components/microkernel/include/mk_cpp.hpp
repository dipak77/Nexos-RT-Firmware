#pragma once
// Nexos-RT V2 Modern C++ SDK — Enterprise RAII Primitives
#ifdef __cplusplus
#include <functional>
#include <utility>
#include "mk_types.h"
#include "mk_mutex.h"
#include "mk_semaphore.h"
#include "mk_queue.h"
#include "mk_task.h"
#include "mk_enclave.h"
#include "mk_svc.h"
#include "mk_fault.h"

namespace nexos {

/**
 * @brief Robust RAII Mutex wrapper with priority inheritance and OWNER_DEAD detection
 */
class Mutex {
public:
    explicit Mutex(const char* name = nullptr) { handle_ = mk_mutex_create(name); }
    ~Mutex() { if (handle_) mk_mutex_delete(handle_); }

    mk_status_t lock(uint32_t timeout_ms = 0xFFFFFFFF) { return mk_mutex_lock(handle_, timeout_ms); }
    mk_status_t try_lock() { return mk_mutex_try_lock(handle_); }
    mk_status_t unlock() { return mk_mutex_unlock(handle_); }
    bool is_locked() const { return mk_mutex_is_locked(handle_); }
    mk_mutex_t* native_handle() const { return handle_; }

    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;
private:
    mk_mutex_t* handle_{nullptr};
};

/**
 * @brief Scoped RAII Lock Guard with timeout and status reporting
 */
class LockGuard {
public:
    explicit LockGuard(Mutex& m, uint32_t timeout_ms = 0xFFFFFFFF) : mutex_(m) {
        status_ = mutex_.lock(timeout_ms);
    }
    ~LockGuard() {
        if (status_ == MK_OK || status_ == MK_ERR_DEADLOCK_OWNER_DEAD) {
            mutex_.unlock();
        }
    }
    bool locked() const { return status_ == MK_OK || status_ == MK_ERR_DEADLOCK_OWNER_DEAD; }
    bool was_owner_dead() const { return status_ == MK_ERR_DEADLOCK_OWNER_DEAD; }
    mk_status_t status() const { return status_; }
    explicit operator bool() const { return locked(); }

    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;
private:
    Mutex& mutex_;
    mk_status_t status_;
};

/**
 * @brief RAII Capability Enclave Container
 */
class Enclave {
public:
    Enclave(const char* name, uint8_t priority, uint32_t caps, size_t stack_size = 4096,
            uint32_t budget_us = 50000, uint32_t deadline_us = 10000) {
        desc_ = mk_enclave_create(name, priority, MK_ENCLAVE_TYPE_RT, 0, 0, stack_size, caps, budget_us, deadline_us);
    }

    template<typename Func>
    bool start(Func&& f) {
        if (!desc_) return false;
        struct Wrapper { Func func; };
        auto* w = new Wrapper{std::forward<Func>(f)};
        mk_status_t st = mk_enclave_start(desc_, [](void* arg) {
            auto* wrapper = (Wrapper*)arg;
            wrapper->func();
            delete wrapper;
        }, w);
        return st == MK_OK;
    }

    void trap(uint16_t code, const char* reason) {
        if (desc_) mk_enclave_trap(desc_, code, reason);
    }

    void reclaim() {
        if (desc_) mk_enclave_reclaim(desc_);
    }

    mk_enclave_desc_t* descriptor() const { return desc_; }

private:
    mk_enclave_desc_t* desc_{nullptr};
};

/**
 * @brief Type-safe bounded IPC Queue
 */
template<typename T>
class Queue {
public:
    explicit Queue(size_t len, const char* name = nullptr) {
        handle_ = mk_queue_create(len, sizeof(T), name);
    }
    ~Queue() { if (handle_) mk_queue_delete(handle_); }

    mk_status_t send(const T& item, uint32_t timeout_ms = 0xFFFFFFFF) {
        return mk_queue_send(handle_, &item, timeout_ms);
    }
    mk_status_t receive(T& out, uint32_t timeout_ms = 0xFFFFFFFF) {
        return mk_queue_receive(handle_, &out, timeout_ms);
    }
    size_t count() const { return mk_queue_get_count(handle_); }

    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;
private:
    mk_queue_t* handle_{nullptr};
};

/**
 * @brief RAII Counting Semaphore wrapper
 */
class Semaphore {
public:
    Semaphore(uint32_t max_c, uint32_t init_c, const char* name = nullptr) {
        handle_ = mk_semaphore_create(max_c, init_c, name);
    }
    ~Semaphore() { if (handle_) mk_semaphore_delete(handle_); }
    mk_status_t take(uint32_t t = 0xFFFFFFFF) { return mk_semaphore_take(handle_, t); }
    mk_status_t give() { return mk_semaphore_give(handle_); }
private:
    mk_semaphore_t* handle_{nullptr};
};

/**
 * @brief Lightweight thread helper
 */
class Thread {
public:
    template<typename Func>
    static mk_task_handle_t create(const char* name, Func&& f, size_t stack = 4096, uint8_t prio = MK_PRIO_COMMAND) {
        struct Wrapper { Func func; };
        auto* w = new Wrapper{std::forward<Func>(f)};
        return mk_task_create(name, [](void* arg) {
            auto* wrapper = (Wrapper*)arg;
            wrapper->func();
            delete wrapper;
        }, w, nullptr, stack, prio);
    }
};

} // namespace nexos

// Backwards compatibility namespace alias
namespace mk {
    using Mutex = nexos::Mutex;
    using LockGuard = nexos::LockGuard;
    using Semaphore = nexos::Semaphore;
    using Thread = nexos::Thread;
    template<typename T> using Queue = nexos::Queue<T>;
}
#endif
