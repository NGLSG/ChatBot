#include "Recorder.h"

extern bool NoRecord;

Recorder::Recorder(int sampleRate, int framesPerBuffer)
        : sampleRate(sampleRate), framesPerBuffer(framesPerBuffer) {
    Logger::Init();
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        LogError("PortAudio initialization failed: {0}", Pa_GetErrorText(err));
    }
}

Recorder::~Recorder() {
    Pa_Terminate();
}

void Recorder::startRecording() {
    LogInfo("Staring recording...");
    Pa_OpenDefaultStream(&stream, 1, 0, paFloat32, sampleRate, framesPerBuffer, recordCallback, this);
    Pa_StartStream(stream);

}

void Recorder::stopRecording(bool del) {
    silentTimer = LLONG_MIN;
    Pa_StopStream(stream);
    Pa_CloseStream(stream);

}

std::vector<float> Recorder::getRecordedData() const {
    return recordedData;
}

void Recorder::saveToWav(const std::string &fileName) {
    if (!getRecordedData().empty()) {
        const std::vector<float> &pcmData = getRecordedData();
        getRecordedData().clear();
        FILE *file = fopen(fileName.c_str(), "wb");
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
            LogInfo("Saved audio to {0}", fileName);
        } else {
            LogError("Failed to open file {0}", fileName);
        }
    }
}

int Recorder::recordCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
                             const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags,
                             void *userData) {
    bool silence = true;
    if (NoRecord) {
        return paContinue;
    }

    Recorder *recorder = static_cast<Recorder *>(userData);
    const float *input = static_cast<const float *>(inputBuffer);

    // Check for silence

    for (int i = 0; i < framesPerBuffer; i++) {
        if (std::abs(input[i]) > 0.001) {
            silence = false;
            break;
        }
    }

    // Reset timer if not silent
    if (!silence) {
        recorder->silentTimer = 0.0;
    }

    // Check for timeout
    if (silence && recorder->silentTimer >= recorder->SILENT_TIMEOUT) {
        return paComplete;
    }

    // Record audio data
    if (!silence) {
        recorder->recordedData.insert(recorder->recordedData.end(), input, input + framesPerBuffer);
    }

    // Increment timer
    recorder->silentTimer += static_cast<double>(framesPerBuffer * 1000.0 / recorder->sampleRate);
    return paContinue;
}

void Audio::playRecordedAudio(const std::vector<float> &audioData) {
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
