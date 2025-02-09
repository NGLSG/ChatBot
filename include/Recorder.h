#ifndef RECORDER_H
#define RECORDER_H

#include "utils.h"

class Recorder
{
public:
    const std::string filepath = "recorded.wav";

    std::atomic<uint32_t> silentTimer = 0;
    const std::atomic<uint32_t> SILENT_TIMEOUT = 2000;

    Recorder(int sampleRate, int framesPerBuffer);

    ~Recorder();

    void startRecording();

    void stopRecording(bool del = false);

    std::vector<float> getRecordedData() const;

    void saveToWav(const std::string& fileName = "recorded.wav");

    void ChangeDevice(std::string device);

    int sampleRate;

    static int recordCallback(const void* inputBuffer, void* outputBuffer, unsigned long framesPerBuffer,
                              const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags,
                              void* userData);

    std::vector<std::string> Devices;

private:
    int deviceId = -1;
    std::string device;
    static const int SAMPLE_RATE = 16000;
    int framesPerBuffer;
    PaStream* stream;

    std::vector<float> recordedData;
};

class Listener
{
public:
    Listener(int sampleRate, int framesPerBuffer);

    ~Listener();

    void listen(std::string file = "recorded0.wav");

    void EndListen(bool save = true);

    bool IsRecorded();;

    void ResetRecorded();

    void playRecorded(bool islisten = true);

    void changeFile(std::string filename);

    void ChangeDevice(std::string device);

    std::vector<float> getRecordedData();

private:
    std::string taskPath;
    std::atomic<bool> isRecorded = false;
    bool run = true;
    std::shared_ptr<Recorder> recorder;
    std::vector<float> recordedData;
    std::mutex recordedData_mutex; // 互斥锁
    friend int Recorder::recordCallback(const void* inputBuffer, void* outputBuffer, unsigned long framesPerBuffer,
                                        const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags,
                                        void* userData);
};

class Audio
{
public:
    static void playRecordedAudio(const std::vector<float>& audioData);
};

#endif
