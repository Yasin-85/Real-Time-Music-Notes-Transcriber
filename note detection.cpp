#include "kissfft/kiss_fft.h"
#include "kissfft/kiss_fftr.h"
#include "json.hpp"
#include <RtAudio.h>

#include <iostream>
#include <fstream>
#include <vector>

using json = nlohmann::json;

int main() 
{
    RtAudio audio;

    unsigned int device_count = audio.getDeviceCount();

    std::vector<RtAudio::DeviceInfo> info;
    info.reserve(device_count);

    for (int i = 0; i < device_count; i++)
    {
        info.push_back(audio.getDeviceInfo(i));
    }

    for (const auto& v : info)
    {
        std::string input = v.isDefaultInput ? "default input" : "not default input";
        std::string output = v.isDefaultOutput ? "default output" : "not default output";
        
        std::cout << v.name << " Duplex channels : " << v.duplexChannels << ", " << input << ", " << output << std::endl;
        std::cout << "input channels : " << v.inputChannels << ", output channels : " << v.outputChannels << std::endl << std::endl;
    }

    return 0;
}