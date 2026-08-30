#include "nvs_store.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char* TAG = "NVS_STORE";
static const char* NVS_NAMESPACE = "smart_dev";

namespace smart_device {
namespace storage {

NvsStore& NvsStore::instance(){ static NvsStore s; return s; }

Result<void> NvsStore::initialize(){
    if(initialized_) return Result<void>::Ok();
    esp_err_t ret = nvs_flash_init();
    if(ret==ESP_ERR_NVS_NO_FREE_PAGES || ret==ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_LOGW(TAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if(ret!=ESP_OK){
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(ret));
        return Result<void>::Err(AppError::STORAGE_NVS_INIT_FAILED, esp_err_to_name(ret));
    }
    initialized_ = true;
    ESP_LOGI(TAG, "NVS initialized");
    return Result<void>::Ok();
}

Result<void> NvsStore::erase_all(){
    esp_err_t ret = nvs_flash_erase();
    if(ret!=ESP_OK) return Result<void>::Err(AppError::STORAGE_WRITE_FAILED, esp_err_to_name(ret));
    ret = nvs_flash_init();
    if(ret!=ESP_OK) return Result<void>::Err(AppError::STORAGE_NVS_INIT_FAILED, esp_err_to_name(ret));
    ESP_LOGI(TAG, "NVS erased");
    return Result<void>::Ok();
}

Result<std::string> NvsStore::get_string(const char* key){
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if(ret!=ESP_OK) return Result<std::string>::Err(AppError::STORAGE_NVS_OPEN_FAILED, esp_err_to_name(ret));
    size_t required = 0;
    ret = nvs_get_str(h, key, nullptr, &required);
    if(ret!=ESP_OK){ nvs_close(h); return Result<std::string>::Err(AppError::STORAGE_KEY_NOT_FOUND, key); }
    if (required == 0) { nvs_close(h); return Result<std::string>::Ok(std::string()); }
    // required includes trailing '\0'
    std::string val;
    val.resize(required);
    size_t len = required;
    ret = nvs_get_str(h, key, val.data(), &len);
    nvs_close(h);
    if(ret!=ESP_OK) return Result<std::string>::Err(AppError::STORAGE_KEY_NOT_FOUND, key);
    // nvs_get_str writes len bytes including '\0'; len may be required or smaller
    if (!val.empty() && val.back() == '\0') val.pop_back();
    // val may still contain embedded nulls; trim to actual strlen if we stored C-string
    // For binary-safe, don't strlen; but keys here are C-strings so strlen is intentional.
    // If value was binary with embedded zeros, strlen would truncate — we keep as-is if contains null in middle.
    // Heuristic: if popped once and still has '\0' inside, keep raw length.
    return Result<std::string>::Ok(val);
}

Result<void> NvsStore::set_string(const char* key, const std::string& value){
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if(ret!=ESP_OK) return Result<void>::Err(AppError::STORAGE_NVS_OPEN_FAILED, esp_err_to_name(ret));
    ret = nvs_set_str(h, key, value.c_str());
    if(ret!=ESP_OK){ nvs_close(h); return Result<void>::Err(AppError::STORAGE_WRITE_FAILED, esp_err_to_name(ret)); }
    ret = nvs_commit(h);
    nvs_close(h);
    if(ret!=ESP_OK) return Result<void>::Err(AppError::STORAGE_WRITE_FAILED, esp_err_to_name(ret));
    return Result<void>::Ok();
}

Result<int32_t> NvsStore::get_i32(const char* key){
    nvs_handle_t h;
    if(nvs_open(NVS_NAMESPACE, NVS_READONLY, &h)!=ESP_OK) return Result<int32_t>::Err(AppError::STORAGE_NVS_OPEN_FAILED, "");
    int32_t v=0;
    esp_err_t ret = nvs_get_i32(h, key, &v);
    nvs_close(h);
    if(ret!=ESP_OK) return Result<int32_t>::Err(AppError::STORAGE_KEY_NOT_FOUND, key);
    return Result<int32_t>::Ok(v);
}
Result<void> NvsStore::set_i32(const char* key, int32_t value){
    nvs_handle_t h;
    if(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h)!=ESP_OK) return Result<void>::Err(AppError::STORAGE_NVS_OPEN_FAILED, "");
    esp_err_t ret = nvs_set_i32(h, key, value);
    if(ret==ESP_OK) ret = nvs_commit(h);
    nvs_close(h);
    if(ret!=ESP_OK) return Result<void>::Err(AppError::STORAGE_WRITE_FAILED, esp_err_to_name(ret));
    return Result<void>::Ok();
}
Result<bool> NvsStore::get_bool(const char* key){
    auto r = get_i32(key);
    if(r.is_err()) return Result<bool>::Err(r.error(), r.error_message());
    return Result<bool>::Ok(r.value()!=0);
}
Result<void> NvsStore::set_bool(const char* key, bool value){
    return set_i32(key, value?1:0);
}

} // namespace storage
} // namespace smart_device
