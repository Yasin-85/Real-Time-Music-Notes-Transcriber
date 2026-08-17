#include "kissfft/kiss_fft.h"
#include "kissfft/kiss_fftr.h"
#include "json.hpp"
#include <RtAudio.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <iomanip>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using json = nlohmann::json;

// Note struct aka the result
struct Note
{
    int beat, subdivision, note, duration;
};

// Grid Slot struct aka a raw value to then be turned into a Note
struct Grid_Slot
{
    double frequency, amplitude;
};

// Call Back Data to be passed onto the record_callback function to calculate the frequencies and slots and everything
struct Call_Back_Data
{
    std::vector<double> accumulator;
    int samples_per_slot, sample_rate, current_slot, slots_per_bar, RMS_buffer_index;
    std::vector<Grid_Slot> grid;
    double RMS_avg, RMS_history[10];
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// global variables

// grid settings
double BPM{ 0 };
int time_sig_num{ 0 }, time_sig_den{ 0 }, subdivision{ 0 };

// grid calculations
double seconds_per_beat, seconds_per_subdivision;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// function prototypes
double pitch_detect(const std::vector<double>& audio, int sample_rate);
std::string frequency_to_note_name(double frequency);
double get_RMS(const std::vector<double>& chunk);
static int record_callback(void* output_buffer, void* input_buffer, unsigned int n_frames, double stream_time, RtAudioStreamStatus status, void* user_data);
void print_line();
void print_grid(const std::vector<Grid_Slot>& grid, int current_slot, int slots_per_bar);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main()
{
    RtAudio audio;

    // params
    RtAudio::StreamParameters params;
    params.deviceId = audio.getDefaultInputDevice();
    params.nChannels = 1;
    params.firstChannel = 0;
    unsigned int buffer_size = 256;

    while (true)
    {
        int choice;

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
        {
            print_line();

            if (BPM == 0 || time_sig_num == 0 || time_sig_den == 0 || subdivision == 0)
            {
                std::cout << "please set grid settings first (option 2)\n";
                break;
            }

            // creating call back data
            Call_Back_Data call_back_data;
            call_back_data.sample_rate = 44100;
            call_back_data.current_slot = 0;
            call_back_data.slots_per_bar = (subdivision / time_sig_den) * time_sig_num;
            call_back_data.RMS_avg = 0.0;
            call_back_data.RMS_buffer_index = 0;

            for (int i = 0; i < 10; i++)
            {
                call_back_data.RMS_history[i] = 0.0;
            }

            // calculate samples per slot
            seconds_per_beat = 60 / BPM;
            seconds_per_subdivision = seconds_per_beat / ((double)subdivision / time_sig_den);
            call_back_data.samples_per_slot = (int)(seconds_per_subdivision * call_back_data.sample_rate);

            // pre allocate grid (10 minutes worth of slots)
            int max_slots = (int)(600 / seconds_per_subdivision) + 1;
            call_back_data.grid.resize(max_slots, { 0.0, 0.0 });
            call_back_data.accumulator.reserve(call_back_data.samples_per_slot * 2);

            std::cout << "samples per slot : " << call_back_data.samples_per_slot << '\n';
            std::cout << "press enter to start recording...\n";
            std::cin.ignore();
            std::cin.get();

            // open stream with call back data
            try
            {
                if (audio.isStreamOpen())
                {
                    audio.closeStream();
                }

                audio.openStream(nullptr, &params, RTAUDIO_FLOAT64, 44100, &buffer_size, &record_callback, &call_back_data);
                audio.startStream();
            }
            catch (RtAudioError& e)
            {
                std::cerr << "error : " << e.getMessage() << '\n';
                break;
            }

            std::cout << "recording, press enter to stop...\n";
            std::cin.get();

            try
            {
                audio.stopStream();
                audio.closeStream();
            }
            catch (RtAudioError& e)
            {
                std::cerr << "error : " << e.getMessage() << '\n';
                break;
            }

            print_line();
            std::cout << "recorded " << call_back_data.current_slot << " grid slots\n";

            // print summary
            print_grid(call_back_data.grid, call_back_data.current_slot, call_back_data.slots_per_bar);

            break;
        }

        case 2:
        {
            print_line();

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
                        std::cout << "enter subdivision (e.g., 4, 8, 16, 32), (cannot be smaller than the time signiture's den and must be a multiplie of time signiture's den): ";
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
            print_line();

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double pitch_detect(const std::vector<double>& audio, int sample_rate)
{
    const int N = 1024; // FFT size

    // allocate FFT config
    kiss_fftr_cfg cfg = kiss_fftr_alloc(N, 0, nullptr, nullptr);
    kiss_fft_cpx out[N];
    kiss_fft_scalar in[N];

    // apply hanning window to reduce spectral leakage
    for (int i = 0; i < N; i++)
    {
        double window = 0.5 * (1.0 - cos(2.0 * 3.141592653589793 * i / (N - 1)));
        in[i] = audio[i] * window;
    }

    // run FFT
    kiss_fftr(cfg, in, out);
    free(cfg);

    // finding the peak magnitude
    double max_magnitude = 0.0;
    int peak_index = 0;

    int min_bin = 20 * N / sample_rate;   // lowest piano note
    int max_bin = 5000 * N / sample_rate; // highest piano note

    for (int i = min_bin; i < max_bin && i < N / 2; i++)
    {
        double mag = sqrt(out[i].r * out[i].r + out[i].i * out[i].i);

        if (mag > max_magnitude)
        {
            max_magnitude = mag;
            peak_index = i;
        }
    }

    // If peak is too quiet, treat as silence
    if (max_magnitude < 0.001)
    {
        return 0.0;
    }

    // convert bin index to frequency
    return (double)peak_index * sample_rate / N;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string frequency_to_note_name(double frequency)
{
    if (frequency <= 0)
        return "---";

    // MIDI formula : 69 = A4 at 440 HZ
    int midi_note = (int)round(69 + 12 * log2(frequency / 440.0));

    if (midi_note < 0 || midi_note > 127)
        return "---";

    const char* note_names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int octave = midi_note / 12 - 1;
    int note_index = midi_note % 12;

    return std::string(note_names[note_index]) + std::to_string(octave);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double get_RMS(const std::vector<double>& chunk)
{
    double sum = 0.0;

    for (double sample : chunk)
    {
        sum += sample * sample;
    }

    return sqrt(sum / chunk.size());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int record_callback(void* output_buffer, void* input_buffer, unsigned int n_frames, double stream_time, RtAudioStreamStatus status, void* user_data)
{
    // capture the call back data given from main
    Call_Back_Data* data = static_cast<Call_Back_Data*>(user_data);

    // cast input to doubles
    double* input = static_cast<double*>(input_buffer);

    // accumulate samples
    for (unsigned int i = 0; i < n_frames; i++)
    {
        data->accumulator.push_back(input[i]);
    }

    // check to see if enough samples are given for a grid slot
    if (data->accumulator.size() >= data->samples_per_slot)
    {
        // extract enough sample for one grid slot
        std::vector<double> chunk(data->accumulator.begin(), data->accumulator.begin() + data->samples_per_slot);
        data->accumulator.erase(data->accumulator.begin(), data->accumulator.begin() + data->samples_per_slot);

        // detect the pitch
        double frequency = pitch_detect(chunk, data->sample_rate);
        double rms = get_RMS(chunk);

        // RMS avg for RMS attack check
        data->RMS_history[data->RMS_buffer_index] = rms;
        data->RMS_buffer_index = (data->RMS_buffer_index + 1) % 10;

        double sum = 0.0;

        for (int i = 0; i < 10; i++)
        {
            sum += data->RMS_history[i];
        }
        data->RMS_avg = sum / 10.0;

        if (rms >= data->RMS_avg && rms > 0.001)
        {
            // attack detected -> register note
            data->grid[data->current_slot].frequency = frequency;
        }
        else
            data->grid[data->current_slot].frequency = 0.0;

        // store in grid
        data->grid[data->current_slot].amplitude = rms;

        // Move to next slot
        data->current_slot++;

        // real time printing of every slot, each time
        print_grid(data->grid, data->current_slot, data->slots_per_bar);
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void print_line()
{
    std::cout << "##########################################" << '\n';
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void print_grid(const std::vector<Grid_Slot>& grid, int current_slot, int slots_per_bar)
{
    std::cout << "\033[2J\033[1;1H";

    int total_bars = current_slot / slots_per_bar;
    int current_bar_slots = current_slot % slots_per_bar;

    // print all completed bars
    for (int bar = 0; bar < total_bars; bar++)
    {
        std::cout << "bar " << std::setw(2) << bar + 1 << " : | ";
        for (int slot = bar * slots_per_bar; slot < (bar + 1) * slots_per_bar; slot++)
        {
            if (grid[slot].frequency > 0) {
                std::cout << std::setw(4) << std::left << frequency_to_note_name(grid[slot].frequency);
            }
            else {
                std::cout << std::setw(4) << std::left << ".";
            }
            if ((slot + 1) % (slots_per_bar / time_sig_den) == 0) {
                std::cout << "| ";
            }
        }
        std::cout << '\n';
    }

    // print the current bar (may be incomplete) - NOW ALIGNED
    if (current_bar_slots > 0)
    {
        std::cout << "bar " << std::setw(2) << total_bars + 1 << " : | ";
        for (int slot = total_bars * slots_per_bar; slot < current_slot; slot++)
        {
            if (grid[slot].frequency > 0) {
                std::cout << std::setw(4) << std::left << frequency_to_note_name(grid[slot].frequency);
            }
            else {
                std::cout << std::setw(4) << std::left << ".";
            }
            if ((slot + 1) % (slots_per_bar / time_sig_den) == 0) {
                std::cout << "| ";
            }
        }
        std::cout << '\n';
    }
}