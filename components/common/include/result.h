#pragma once
#include "app_error.h"
#include <variant>
#include <string>

namespace smart_device {

template<typename T>
class Result {
public:
    static Result<T> Ok(T value) {
        return Result<T>(std::move(value));
    }
    static Result<T> Err(AppError err, std::string msg = {}) {
        return Result<T>(err, std::move(msg));
    }

    bool is_ok() const { return std::holds_alternative<T>(data_); }
    bool is_err() const { return !is_ok(); }

    T& value() { return std::get<T>(data_); }
    const T& value() const { return std::get<T>(data_); }

    AppError error() const { return std::get<ErrorInfo>(data_).code; }
    const std::string& error_message() const { return std::get<ErrorInfo>(data_).msg; }

private:
    struct ErrorInfo { AppError code; std::string msg; };
    std::variant<T, ErrorInfo> data_;
    Result(T v) : data_(std::move(v)) {}
    Result(AppError e, std::string m) : data_(ErrorInfo{e, std::move(m)}) {}
};

template<>
class Result<void> {
public:
    static Result<void> Ok() { return Result<void>(true); }
    static Result<void> Err(AppError err, std::string msg = {}) { return Result<void>(err, std::move(msg)); }
    bool is_ok() const { return ok_; }
    bool is_err() const { return !ok_; }
    AppError error() const { return err_; }
    const std::string& error_message() const { return msg_; }
private:
    bool ok_{false};
    AppError err_{AppError::OK};
    std::string msg_;
    Result(bool ok) : ok_(ok) {}
    Result(AppError e, std::string m) : ok_(false), err_(e), msg_(std::move(m)) {}
};

} // namespace smart_device
