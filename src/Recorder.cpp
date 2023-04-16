/*
#include <iostream>
#include "Recorder.h"

Recorder::Recorder() : m_stream(nullptr), m_running(false) {}

Recorder::~Recorder() {
    if (m_stream) {
        Pa_CloseStream(m_stream);
    }
    Pa_Terminate();
}

bool Recorder::start() {
    if (m_running) {
        return true;
    }

    // Initialize PortAudio
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "Failed to initialize PortAudio: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }

    // Open default input audio device
    err = Pa_OpenDefaultStream(&m_stream, 1, 0, paFloat32, 48000, 1024, audioCallback, this);
    if (err != paNoError) {
        std::cerr << "Failed to open audio stream: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }

    // Start audio stream
    err = Pa_StartStream(m_stream);
    if (err != paNoError) {
        std::cerr << "Failed to start audio stream: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }

    m_running = true;
    m_audioBuffer.clear();
    m_lastAudioTime = std::chrono::high_resolution_clock::now();

    return true;
}

bool Recorder::stop() {
    if (!m_running) {
        return true;
    }

    // Stop audio stream
    PaError err = Pa_StopStream(m_stream);
    if (err != paNoError) {
        std::cerr << "Failed to stop audio stream: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }

    m_running = false;

    return true;
}

std::vector<float> Recorder::getAudioBuffer() const {
    return m_audioBuffer;
}

int Recorder::audioCallback(const void* inputBuffer, void* outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags, void* userData) {
    Recorder* recorder = static_cast<Recorder*>(userData);
    recorder->processAudio(inputBuffer, framesPerBuffer);
    return paContinue;
}

void Recorder::processAudio(const void* inputBuffer, unsigned long framesPerBuffer) {
    if (!m_running) {
        return;
    }

    // Calculate RMS value of audio samples
    float rms = 0.0f;
    const float* samples = static_cast<const float*>(inputBuffer);
    for (unsigned int i = 0; i < framesPerBuffer; i++) {
        rms += samples[i] * samples[i];
    }
    rms = sqrt(rms / framesPerBuffer);

    // Check if there is audio signal
    if (rms > 0.001f) {
        // Add audio samples to buffer
        const float* inBuffer = static_cast<const float*>(inputBuffer);
        for (unsigned int i = 0; i < framesPerBuffer; i++) {
            m_audioBuffer.push_back(inBuffer[i]);
        }

        // Update last audio time
        m_lastAudioTime = std::chrono::high_resolution_clock::now();
    } else {
        // Check if there has been no audio signal for 2 seconds
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastAudioTime);
        if (duration.count() > 2000) {
            stop();
        }
    }
}*/
