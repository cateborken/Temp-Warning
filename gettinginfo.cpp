#include <iostream>
#include <windows.h>
#include <string>
#include <thread>
#include <chrono>
#include <shellapi.h>
#include <iomanip>  
#include "hwisenssm2.h"

// Thresholds in Celsius
constexpr double CPU_TEMP_THRESHOLD = 50.0;
constexpr double GPU_TEMP_THRESHOLD = 55.0;
constexpr auto NOTIFICATION_COOLDOWN = std::chrono::minutes(1);

// HWIS Signature as DWORD
#define SIGNATURE_HWIS 0x53695748  

void showNotification(const std::string& title, double temperature) {
    // Format the temperature value to 1 decimal place
    std::ostringstream tempStream;
    tempStream << std::fixed << std::setprecision(1) << temperature;
    std::string tempStr = tempStream.str();

    // Properly format the temperature string with degree symbol
    std::wstring notificationMessage = L"Temperature: " + std::wstring(tempStr.begin(), tempStr.end()) + L"°C";

    // Create the PowerShell command to display the notification using BurntToast
    std::wstring cmd = L"powershell.exe -ExecutionPolicy Bypass -Command \""
                       L"Import-Module BurntToast; "
                       L"New-BurntToastNotification -Text '" + std::wstring(title.begin(), title.end()) + L"', '" + notificationMessage + L"'\"";

    // Call the PowerShell script to show the notification
    _wsystem(cmd.c_str());  // Use _wsystem for wide string support
}

void MonitorHWInfo() {
    HANDLE hHWiNFOMemory = OpenFileMappingW(FILE_MAP_READ, FALSE, L"Global\\HWiNFO_SENS_SM2");
    if (!hHWiNFOMemory) {
        std::cerr << "Failed to open HWiNFO Shared Memory" << std::endl;
        return;
    }

    PHWiNFO_SENSORS_SHARED_MEM2 pHWiNFOMemory = 
        (PHWiNFO_SENSORS_SHARED_MEM2) MapViewOfFile(hHWiNFOMemory, FILE_MAP_READ, 0, 0, 0);

    if (!pHWiNFOMemory) {
        std::cerr << "Failed to map HWiNFO Shared Memory" << std::endl;
        return;
    }

    auto lastCPUNotificationTime = std::chrono::system_clock::time_point();
    auto lastGPUNotificationTime = std::chrono::system_clock::time_point();

    while (true) {        
        // Check for valid signature
        if (pHWiNFOMemory->dwSignature != SIGNATURE_HWIS) {
            std::cerr << "Invalid HWiNFO Shared Memory Signature" << std::endl;
            break;
        }

        auto now = std::chrono::system_clock::now();

        // Iterate through sensors
        for (DWORD i = 0; i < pHWiNFOMemory->dwNumReadingElements; ++i) {
            PHWiNFO_SENSORS_READING_ELEMENT pReading = 
                (PHWiNFO_SENSORS_READING_ELEMENT)((BYTE*)pHWiNFOMemory + 
                pHWiNFOMemory->dwOffsetOfReadingSection + 
                (pHWiNFOMemory->dwSizeOfReadingElement * i));

            if (pReading->tReading == SENSOR_TYPE_TEMP) {
                std::string label(pReading->szLabelUser);
                double temp = pReading->Value;

                // CPU Temperature Check
                if (label.find("CPU") != std::string::npos && temp >= CPU_TEMP_THRESHOLD) {
                    if (now - lastCPUNotificationTime >= NOTIFICATION_COOLDOWN) {
                        showNotification("CPU temperature over limit!", temp);
                        lastCPUNotificationTime = now;
                    }
                }

                // GPU Temperature Check
                if (label.find("GPU") != std::string::npos && temp >= GPU_TEMP_THRESHOLD) {
                    if (now - lastGPUNotificationTime >= NOTIFICATION_COOLDOWN) {
                        showNotification("GPU temperature over limit!", temp);
                        lastGPUNotificationTime = now;
                    }
                }
            }
        }

        // Sleep for 5 seconds using Windows Sleep function (in milliseconds)
        Sleep(5000);
    }
}

int main() {
    std::cout << "Starting HWiNFO Monitor..." << std::endl;
    MonitorHWInfo();
    return 0;
}
