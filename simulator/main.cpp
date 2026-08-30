// Simulator - Windows/Linux desktop mock of round display
// Structure: GUI independent of ESP hardware
//                  GUI
//                   |
//             Application API
//               /         \
//              /           \
//             v             v
//     Desktop Mock      ESP32 HAL

#include <iostream>
#include <string>
#include <chrono>
#include <thread>

struct MockUiState {
    std::string time_str = "12:24 PM";
    std::string date_str = "28 AUG 2026";
    bool wifi = true;
    bool ble = true;
    std::string status = "SYSTEM OK";
    std::string cmd = "WIFI STATUS";
    std::string result = "✓ PASS";
    std::string fw = "FW v1.0.0";
};

void render_ascii(const MockUiState& s){
    std::cout << "\n┌─────────────────────────┐\n";
    std::cout << "│ " << (s.wifi?"● WiFi":"○ WiFi") << "       " << (s.ble?"● BLE":"○ BLE") << " │\n";
    std::cout << "│                         │\n";
    std::cout << "│         " << s.time_str << "        │\n";
    std::cout << "│      " << s.date_str << "     │\n";
    std::cout << "│                         │\n";
    std::cout << "│       " << s.status << "        │\n";
    std::cout << "│                         │\n";
    std::cout << "│    CMD: " << s.cmd << "     │\n";
    std::cout << "│         " << s.result << "          │\n";
    std::cout << "│                         │\n";
    std::cout << "│       " << s.fw << "         │\n";
    std::cout << "└─────────────────────────┘\n";
}

int main(){
    std::cout << "Smart Device Simulator - GC9A01 240x240 Round\n";
    std::cout << "Commands: sim wifi connected/failed, sim ble connected, sim command success/fail, sim ota 45\n";

    MockUiState state;
    render_ascii(state);

    std::string line;
    while(std::getline(std::cin, line)){
        if(line=="sim wifi connected"){ state.wifi=true; }
        else if(line=="sim wifi failed"){ state.wifi=false; }
        else if(line=="sim ble connected"){ state.ble=true; }
        else if(line=="sim ble failed"){ state.ble=false; }
        else if(line=="sim command success"){ state.cmd="WIFI CONNECT"; state.result="✓ PASS"; }
        else if(line=="sim command fail"){ state.cmd="WIFI CONNECT"; state.result="✕ FAIL ERR 1003"; }
        else if(line.rfind("sim ota",0)==0){ state.cmd="OTA UPDATE"; state.result=line.substr(7)+"%"; }
        else if(line=="sim time invalid"){ state.time_str="--:--"; state.date_str="-- --- ----"; }
        else { state.cmd=line; }

        render_ascii(state);
        std::cout << "\ndevice> ";
    }
    return 0;
}
