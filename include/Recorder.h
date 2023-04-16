#ifndef RECORDER_H
#define RECORDER_H

#include "portaudio.h"
#include <vector>
#include <thread>
#include <chrono>
#include "Logger.h"

class Recorder {
public:

    Recorder(int sampleRate, int framesPerBuffer)
            : sampleRate(sampleRate), framesPerBuffer(framesPerBuffer) {
        Log::Logger::Init();
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            Log::Error("PortAudio initialization failed: {0}", Pa_GetErrorText(err));
        }

    }

    ~Recorder() {
        Pa_Terminate();
    }

    void startRecording() {
        Pa_OpenDefaultStream(&stream, 1, 0, paFloat32, sampleRate, framesPerBuffer, recordCallback, this);
        Pa_StartStream(stream);
    }

    void stopRecording() {
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
    }

    std::vector<float> getRecordedData() const {
        return recordedData;
    }

    void saveToWav(const std::string &fileName) {
        const std::vector<float> &pcmData = getRecordedData();
        FILE * file = fopen(fileName.c_str(), "wb");
        if (file) {
            // Write WAV header
            const int bitsPerSample = 16;
            const int numChannels = 1;
            const int blockAlign = numChannels * bitsPerSample / 8;
            const int byteRate = sampleRate * blockAlign;

            const int dataSize = pcmData.size() * sizeof(float);
            const int fileSize = dataSize + 44;

            fwrite("RIFF", 1, 4, file);
            fwrite(&fileSize, 4, 1, file);
            fwrite("WAVE", 1, 4, file);
            fwrite("fmt ", 1, 4, file);
            const int fmtSize = 16;
            fwrite(&fmtSize, 4, 1, file);
            const short int audioFormat = 1;
            fwrite(&audioFormat, 2, 1, file);
            fwrite(&numChannels, 2, 1, file);
            fwrite(&sampleRate, 4, 1, file);
            fwrite(&byteRate, 4, 1, file);
            fwrite(&blockAlign, 2, 1, file);
            fwrite(&bitsPerSample, 2, 1, file);
            fwrite("data", 1, 4, file);
            fwrite(&dataSize, 4, 1, file);

            // Convert float PCM data to 16-bit signed integer and write to file
            std::vector<short> wavData(pcmData.size());
            for (int i = 0; i < pcmData.size(); i++) {
                wavData[i] = static_cast<short>(pcmData[i] * 32767.0f);
            }
            fwrite(wavData.data(), 2, wavData.size(), file);

            fclose(file);
            Log::Info("Saved audio to {0}", fileName);
        } else {
            Log::Error("Failed to open file {0}", fileName);
        }
    }

private:
    static const int SAMPLE_RATE = 16000;

    PaStream *stream;
    std::vector<float> recordedData;

    static int recordCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
                              const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags,
                              void *userData) {
        Recorder *recorder = static_cast<Recorder *>(userData);
        float *input = static_cast<float *>(const_cast<void *>(inputBuffer));

        // Check if there is any audio input
        bool hasAudioInput = false;
        for (int i = 0; i < framesPerBuffer; i++) {
            if (input[i] != 0) {
                hasAudioInput = true;
                break;
            }
        }

        // If there is no audio input for 2 seconds, stop recording
        static int silentFrames = 0;
        if (!hasAudioInput) {
            silentFrames += framesPerBuffer;
            if (silentFrames >= SAMPLE_RATE * 2) {
                recorder->stopRecording();
                return paComplete;
            }
        } else {
            silentFrames = 0;
        }

        // Save audio data to memory
        for (int i = 0; i < framesPerBuffer; i++) {
            recorder->recordedData.push_back(input[i]);
        }

        return paContinue;
    }

    int sampleRate;
    int framesPerBuffer;
};

void playRecordedAudio(const std::vector<float> &audioData) {
    // Play audio data
    PaStream *stream;
    Pa_Initialize();
    Pa_OpenDefaultStream(&stream, 0, 1, paFloat32, 44100, 512, NULL, NULL);
    Pa_StartStream(stream);
    Pa_WriteStream(stream, audioData.data(), audioData.size());
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
}

void savePcmToFile(const std::vector<float> &pcmData, const std::string &fileName) {
    FILE * file = fopen(fileName.c_str(), "wb");
    if (file) {
        fwrite(pcmData.data(), sizeof(float), pcmData.size(), file);
        fclose(file);
        Log::Info("Saved PCM data to {0}", fileName);
    } else {
        Log::Error("Failed to open file {0}", fileName);
    }
}

#endif // RECORDER_H