#include "kissfft/kiss_fft.h"
#include "kissfft/kiss_fftr.h"
#include "json.hpp"
#include <RtAudio.h>

#include <iostream>
#include <fstream>
#include <vector>

using json = nlohmann::json;

struct Note
{
	int beat, subdivision, note, duration;
};

static int record_callback(void* output_buffer, void* input_buffer, unsigned int n_frames, double stream_time, RtAudioStreamStatus status, void* user_data);
void print_line();

int main()
{
	RtAudio audio;

	RtAudio::StreamParameters params;
	params.deviceId = audio.getDefaultInputDevice();
	params.nChannels = 1;
	params.firstChannel = 0;

	std::vector<double> audio_buffer;
	unsigned int buffer_size = 256;

	int choice;

	double BPM{ 0 };
	int time_sig_num{ 0 }, time_sig_den{ 0 }, subdivision{ 0 };

	try
	{
		audio.openStream(nullptr, &params, RTAUDIO_FLOAT64, 44100, &buffer_size, &record_callback, &audio_buffer);
	}
	catch (RtAudioError& e)
	{
		std::cerr << e.getMessage() << '\n';
		return 1;
	}

	while (true)
	{
		print_line();
		std::cout << "REAL TIME NOTE TRANSCRIBER\n";
		print_line();

		std::cout << "1. start recording \n"
			"2. settings \n"
			"3. exit\n"
			"your choice : ";

		std::cin >> choice;

		switch (choice)
		{
		case 1:
			break;
		
		case 2:
		{
			RtAudio::DeviceInfo info = audio.getDeviceInfo(params.deviceId);

			std::cout << "current settings\n";
			std::cout << "defualt microphone : " << info.name << '\n';
			std::cout << "(grid settings) \n"
				"BPM : " << BPM << ", time signiture : " << time_sig_num << "/" << time_sig_den << ", dubdivision : " << subdivision << ",(0 is the unset defualt value)\n";

			char choice{ 'A' };
			while (true)
			{
				std::cout << "change grid settings ? (Y/N) ";
				std::cin >> choice;

				if (choice == 'y' || choice == 'Y')
				{
					std::cout << "enter BPM: ";
					std::cin >> BPM;

					std::cout << "enter time signature (e.g., 4 4, 6 8, 16 8): ";
					std::cin >> time_sig_num >> time_sig_den;

					do
					{
						std::cout << "enter subdivision (e.g., 4, 8, 16, 32), (cannot be smaller than the time signiture's den and must be a multiple of time signiture's den): ";
						std::cin >> subdivision;
					} while (subdivision < time_sig_den || subdivision % time_sig_den != 0);
				}
				else if (choice == 'n' || choice == 'N')
				{
					std::cout << "going back\n";
					break;
				}
				else
				{
					std::cout << "invalid choice entered\n";
				}
			}
			break;
		}

		case 3:

			std::cout << "thanks for using this program\n";
			
			try
			{
				if (audio.isStreamOpen()) 
				{
					audio.closeStream();
				}
			}
			catch (RtAudioError& e)
			{
				std::cerr << "error closing stream: " << e.getMessage() << std::endl;
				return 1;
			}
			return 0;

		default:
			std::cout << "invalid input entered\n";
			break;
		}
	}

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

void print_line()
{
	std::cout << "##########################################" << '\n';
}