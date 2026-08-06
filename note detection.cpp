#include "kissfft/kiss_fft.h"
#include "kissfft/kiss_fftr.h"
#include "json.hpp"
#include <RtAudio.h>

#include <iostream>
#include <fstream>
#include <vector>

using json = nlohmann::json;

static int record_callback(void* output_buffer, void* input_buffer, unsigned int n_frames, double stream_time, RtAudioStreamStatus status, void* user_data);

int main()
{
	RtAudio audio;

	RtAudio::StreamParameters params;
	params.deviceId = audio.getDefaultInputDevice();
	params.nChannels = 1;
	params.firstChannel = 0;

	std::vector<double> audio_buffer;
	unsigned int buffer_size = 256;

	try
	{
		audio.openStream(nullptr, &params, RTAUDIO_FLOAT64, 44100, &buffer_size, &record_callback, &audio_buffer);
	}
	catch (RtAudioError& e)
	{
		std::cerr << e.getMessage() << '\n';
		return 1;
	}

	RtAudio::DeviceInfo info = audio.getDeviceInfo(params.deviceId);
	std::cout << "Using microphone: " << info.name << std::endl;

	std::cout << "press enter to start recording..." << "\n";
	std::cin.get();

	try
	{
		audio.startStream();
	}
	catch (RtAudioError& e)
	{
		std::cerr << "error starting stream : " << e.getMessage() << '\n';
	}

	std::cout << "recording, press enter to stop..." << '\n';
	std::cin.get();

	try 
	{
		audio.stopStream();
		audio.closeStream();
	}
	catch (RtAudioError& e) 
	{
		std::cerr << "error stopping stream: " << e.getMessage() << std::endl;
		return 1;
	}

	std::cout << "Recorded " << audio_buffer.size() << " samples." << std::endl;

	return 0;
}


static int record_callback(void* output_buffer, void* input_buffer, unsigned int n_frames, double stream_time, RtAudioStreamStatus status, void* user_data)
{
	double* input = static_cast<double*>(input_buffer);

	std::vector<double>* buffer = static_cast<std::vector<double>*>(user_data);

	for (unsigned int i = 0; i < n_frames; i++)
	{
		buffer->push_back(input[i]);
	}

	return 0;
}