#include "usb_hal.h"
#include "esp_log.h"

static const char* TAG = "USB_HAL";

namespace smart_device {
namespace hal {

UsbHal& UsbHal::instance(){ static UsbHal s; return s; }

Result<void> UsbHal::initialize_console(){
    ESP_LOGI(TAG, "UART console ready (UART0)");
    return Result<void>::Ok();
}

Result<void> UsbHal::initialize_cdc(){
    // Native USB CDC-ACM is configured via sdkconfig (CONFIG_USB_CDC_ENABLED /
    // TinyUSB) and the CDC VFS is registered by the console component. Calling
    // esp_vfs_cdcacm_register() directly is NOT available in IDF 6.1 and would
    // either fail to link or hard-fault. Keep it a no-op: UART0 is the primary
    // console and is always sufficient for bring-up.
    ESP_LOGI(TAG, "USB CDC console left to sdkconfig (UART0 primary)");
    return Result<void>::Ok();
}

} // namespace hal
} // namespace smart_device
