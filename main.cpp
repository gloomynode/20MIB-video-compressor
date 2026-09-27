/*
 * Copyright (c) 2026 gloomynode
 *
 * SPDX-License-Identifier: Apache-2.0
 * Version: 1.2
 *
 */

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <array>
#include <charconv>
#include <algorithm>
#include <cctype>
#include <chrono>

#include <vulkan/vulkan.h>

#include <Windows.h>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

//fully made this myself by merging the get av1 and get Gpu vendor function
void VkHardwareCheck(uint32_t &Vendor_ID, bool &AV1Supported, uint32_t &ErrCode) {
    ErrCode = 0;
    AV1Supported = false;
    Vendor_ID = 0x0;

    VkInstanceCreateInfo createInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    VkInstance instance;
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS){
        ErrCode = 0x1;
        return;
    }
    VkPhysicalDevice physicalDevice;
    uint32_t gpuCount = 1;
    vkEnumeratePhysicalDevices(instance, &gpuCount, &physicalDevice);
    if (gpuCount == 0){
        vkDestroyInstance(instance, nullptr);
        ErrCode = 0x2;
        return;}

    //Vendor_ID
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    std::cout << "GPU Name: " << props.deviceName << "\n";
    Vendor_ID = props.vendorID;

    //AV1 Capability
    std::vector<VkPhysicalDevice> gpus(gpuCount);
    vkEnumeratePhysicalDevices(instance, &gpuCount, gpus.data());
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(gpus[0], nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extCount);
    vkEnumerateDeviceExtensionProperties(gpus[0], nullptr, &extCount, extensions.data());


    for (const auto& ext : extensions) {
        if (std::strcmp(ext.extensionName, "VK_KHR_video_encode_av1") == 0) {
            AV1Supported = true;
            break;
        }
    }

    vkDestroyInstance(instance, nullptr);
}

void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
                return !std::isspace(ch);
            }).base(), s.end());
}

struct encoder_vars{
    std::string video_bitrate_string = "0";
    uint32_t video_bitrate = 0;
    uint32_t target_video_bitrate = 0;
    uint32_t audio_bitrate = 0;
    std::string audio_bitrate_string = "0";
    uint64_t total_bitrate = 0;
    uint32_t video_length_seconds = 0;
    std::string video_length_seconds_string = "0";

    std::string bufsize_string = "0";
};

std::string exec(const std::string& cmd) {
    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return "";
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.wShowWindow = SW_HIDE;

    std::string result;
    if (CreateProcessA(NULL, const_cast<char*>(cmd.c_str()), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hWrite);

        std::array<char, 256> buffer;
        DWORD bytesRead;
        while (ReadFile(hRead, buffer.data(), static_cast<DWORD>(buffer.size() - 1), &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            result += buffer.data();
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        CloseHandle(hWrite);
    }

    CloseHandle(hRead);
    return result;
}

bool exists(const LPCSTR lpFileName, const LPCSTR lpExtension){
    char lpBuffer[MAX_PATH];
    DWORD result =
    SearchPathA(NULL, lpFileName, lpExtension, MAX_PATH, lpBuffer, NULL);

    return result != 0;
}

int main(int argc, char* argv[1])
{
    std::string nothing;
    //used for debugging

    if ((argc < 2) || (argc > 2)) {
        return 2;
    }

    encoder_vars ev;
    uint32_t gpu_vendor = 0x0;
    std::string arg1 = argv[1];

    if(exists("ffmpeg", ".exe") == 0){
        return 4;
    }
    std::cout<<"SearchPathA - ffmpeg.exe: true \n";

    //there are way more sanitized and better performing ways to call this. sadly i dont know them
    std::string video_out = exec(("ffprobe -v error -select_streams v:0 -show_entries stream=bit_rate -of default=noprint_wrappers=1:nokey=1 \"" + arg1 + "\"").c_str());
    rtrim(video_out);
    std::from_chars(video_out.data(), video_out.data() + video_out.size(), ev.video_bitrate);
    std::cout<<"video bps: " << ev.video_bitrate << " \n";

    std::string audio_out = exec(("ffprobe -v error -select_streams a:0 -show_entries stream=bit_rate -of default=noprint_wrappers=1:nokey=1 \"" + arg1 + "\"").c_str());
    rtrim(audio_out);
    std::from_chars(audio_out.data(), audio_out.data() + audio_out.size(), ev.audio_bitrate);
    std::cout<<"audio bps: " << ev.audio_bitrate << " \n";

    std::string seconds_out = exec(("ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"" + arg1 + "\"").c_str());
    rtrim(seconds_out);
    std::from_chars(seconds_out.data(), seconds_out.data() + seconds_out.size(), ev.video_length_seconds);
    std::cout<<"length: " << ev.video_length_seconds << " \n";

    if (ev.video_length_seconds <= 0){
        return 6;
    }
    ev.total_bitrate = 152000000 / ev.video_length_seconds;
    //roughtly equates around 19MB which is near discords free 20MIB limit

    if (ev.total_bitrate > 50000000){
        return 8;
    }

    std::cout<<"finished length calculation \n";

    ev.video_bitrate = ev.total_bitrate - ev.audio_bitrate;
    ev.video_bitrate_string = std::to_string(ev.video_bitrate);
    ev.bufsize_string = std::to_string(ev.video_bitrate / 2);
    std::cout<<"Total bitrate for this compression run: " << ev.total_bitrate << " \n";

    uint32_t GpuVendorId = 0x0;
    bool AV1Support = false;
    {
        uint32_t ErrCode = 0x0;
        uint16_t ErrRetryCount = 0;

        VkHardwareCheck(GpuVendorId, AV1Support, ErrCode);
        for(ErrRetryCount = 1; ErrCode != 0x0 && ErrRetryCount <= 3; ErrRetryCount++){
            Sleep(500);
            VkHardwareCheck(GpuVendorId, AV1Support, ErrCode);
        }
        if (ErrCode != 0x0){
            return 7;
        }
        std::cout<<"is Av1 capable: " << AV1Support << " \n";
        std::cout<<"GPU Vendor ID: " << std::hex << GpuVendorId << " \n";
    }

    std::string encoder_type = "libx265";

    if (AV1Support){
        switch (GpuVendorId) {
        case 0x10DE:
            encoder_type = "av1_nvenc";
        break;
        case 0x1002:
            encoder_type = "av1_amf";
            std::cout<<"av1_amf \n";
        break;
    case 0x8086:
        encoder_type = "av1_qsv";
        break;
    default: std::cout<<"Unknown GPU Vendor. \n"; break;
    } }
    else {
        switch (GpuVendorId) {
        case 0x10DE:
            encoder_type = "hevc_nvenc";
        break;
        case 0x1002:
            encoder_type = "hevc_amf";
            std::cout<<"hevc_amf \n";
        break;
        case 0x8086:
            encoder_type = "hevc_qsv";
        break;
    default: std::cout<<"Unknown GPU Vendor. \n"; break;
    }}

    std::chrono::milliseconds ms = std::chrono::duration_cast< std::chrono::milliseconds >(std::chrono::system_clock::now().time_since_epoch());
    std::string ms_string = std::to_string(ms.count());
    std::string output_file_name = "output_" + ms_string + ".mp4";

    //probably the worst way to call this
    std::string technical_debt_out =
    exec(("ffmpeg -i \"" + arg1 + "\" -c:v " + encoder_type + " -b:v " + ev.video_bitrate_string + " -maxrate " + ev.video_bitrate_string + " -bufsize " + ev.bufsize_string + " -c:a copy " + output_file_name).c_str());

    //i found this to be more effective at forcing the amf encoder to encode under the target size of 20 MIB than two pass encoding
    uint64_t detected_size_64 = std::filesystem::file_size(output_file_name);
    std::cout<<"detected size: " << detected_size_64 << " \n";
    if(detected_size_64 > 20971520){
        uint64_t decreased_video_bitrate64 = (ev.video_bitrate * 3) / 4;
        std::string decreased_video_bitrate_string = std::to_string(decreased_video_bitrate64);
        std::cout<<"triggered bitrate decrease! \n";
        std::cout<<"decreased video birate is: " << decreased_video_bitrate64 << " \n";
        technical_debt_out =
        exec(("ffmpeg -y -i \"" + arg1 + "\" -c:v " + encoder_type + " -b:v " + decreased_video_bitrate_string + " -maxrate " +
            decreased_video_bitrate_string + " -bufsize " + ev.bufsize_string + " -c:a copy " + output_file_name).c_str());
    }

    return 0;
}
