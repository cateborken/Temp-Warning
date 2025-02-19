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

    // Store the last notification time
    auto lastNotificationTime = std::chrono::system_clock::now();

    while (true) {
        std::cout << "Signature: " << std::hex << pHWiNFOMemory->dwSignature << std::dec << std::endl;
        
        // Check for valid signature
        if (pHWiNFOMemory->dwSignature != SIGNATURE_HWIS) {
            std::cerr << "Invalid HWiNFO Shared Memory Signature" << std::endl;
            break;
        }

        // Loop through all reading elements
        for (DWORD i = 0; i < pHWiNFOMemory->dwNumReadingElements; ++i) {
            PHWiNFO_SENSORS_READING_ELEMENT pReading = 
                (PHWiNFO_SENSORS_READING_ELEMENT)((BYTE*)pHWiNFOMemory + 
                pHWiNFOMemory->dwOffsetOfReadingSection + 
                (pHWiNFOMemory->dwSizeOfReadingElement * i));

            // Check for valid sensor type
            if (pReading->tReading == SENSOR_TYPE_TEMP) {
                std::string label(pReading->szLabelUser);
                double temp = pReading->Value;

                // Debug: Print the label and temperature
                std::cout << "Label: " << label << " Temp: " << temp << std::endl;

                // If label contains CPU and temperature exceeds threshold
                if (label.find("CPU") != std::string::npos && temp >= CPU_TEMP_THRESHOLD) {
                    // Only notify if a minute has passed since the last notification
                    auto now = std::chrono::system_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::minutes>(now - lastNotificationTime);
                    if (duration.count() >= 1) {
                        showNotification("CPU temperature over limit!", temp);
                        lastNotificationTime = now;  // Update the last notification time
                    }
                }

                // If label contains GPU and temperature exceeds threshold
                if (label.find("GPU") != std::string::npos && temp >= GPU_TEMP_THRESHOLD) {
                    // Only notify if a minute has passed since the last notification
                    auto now = std::chrono::system_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::minutes>(now - lastNotificationTime);
                    if (duration.count() >= 1) {
                        showNotification("GPU temperature over limit!", temp);
                        lastNotificationTime = now;  // Update the last notification time
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
