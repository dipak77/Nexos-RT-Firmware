#pragma once
// C++ RAII wrappers - premium API for product code
#ifdef __cplusplus
#include <functional>
namespace mk {

class Mutex {
public:
    Mutex(const char* name=nullptr) { handle_ = mk_mutex_create(name); }
    ~Mutex() { if(handle_) mk_mutex_delete(handle_); }
    mk_status_t lock(uint32_t timeout_ms=0xFFFFFFFF) { return mk_mutex_lock(handle_, timeout_ms); }
    mk_status_t unlock() { return mk_mutex_unlock(handle_); }
    // non-copyable
    Mutex(const Mutex&)=delete; Mutex& operator=(const Mutex&)=delete;
private:
    mk_mutex_t* handle_{nullptr};
};

class LockGuard {
public:
    LockGuard(Mutex& m): mutex_(m) { mutex_.lock(); }
    ~LockGuard() { mutex_.unlock(); }
private:
    Mutex& mutex_;
};

class Semaphore {
public:
    Semaphore(uint32_t max_c, uint32_t init_c, const char* name=nullptr){ handle_=mk_semaphore_create(max_c, init_c, name); }
    ~Semaphore(){ if(handle_) mk_semaphore_delete(handle_); }
    mk_status_t take(uint32_t t=0xFFFFFFFF){ return mk_semaphore_take(handle_, t); }
    mk_status_t give(){ return mk_semaphore_give(handle_); }
private:
    mk_semaphore_t* handle_{nullptr};
};

template<typename T>
class Queue {
public:
    Queue(size_t len, const char* name=nullptr){ handle_=mk_queue_create(len, sizeof(T), name); }
    ~Queue(){ if(handle_) mk_queue_delete(handle_); }
    mk_status_t send(const T& item, uint32_t t=0xFFFFFFFF){ return mk_queue_send(handle_, &item, t); }
    mk_status_t receive(T& out, uint32_t t=0xFFFFFFFF){ return mk_queue_receive(handle_, &out, t); }
    size_t count() const { return mk_queue_get_count(handle_); }
private:
    mk_queue_t* handle_{nullptr};
};

class Thread {
public:
    template<typename Func>
    static mk_task_handle_t create(const char* name, Func&& f, size_t stack=4096, uint8_t prio=MK_PRIO_COMMAND){
        struct Wrapper { Func func; };
        auto* w = new Wrapper{std::forward<Func>(f)};
        return mk_task_create(name, [](void* arg){
            auto* wrapper = (Wrapper*)arg;
            wrapper->func();
            delete wrapper;
        }, w, nullptr, stack, prio);
    }
};

} // namespace mk
#endif
