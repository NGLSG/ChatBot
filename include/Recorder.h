#ifndef RECORDER_H
#define RECORDER_H

#include "utils.h"

class Recorder {
public:
    const std::string filepath = "recorded.wav";

    double silentTimer;
    const int SILENT_TIMEOUT = 1000;

    Recorder(int sampleRate, int framesPerBuffer);

    ~Recorder();

    void startRecording();

    void stopRecording(bool del = false);

    std::vector<float> getRecordedData() const;

    void saveToWav(const std::string &fileName = "recorded.wav");


    int sampleRate;

    static int recordCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
                              const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags,
                              void *userData);

private:

    static const int SAMPLE_RATE = 16000;
    int framesPerBuffer;
    PaStream *stream;
    std::vector<float> recordedData;

};

class Listener {
public:
    Listener(int sampleRate, int framesPerBuffer) : recorder(sampleRate, framesPerBuffer) {
    }

    ~Listener() {
    }

    void listen(std::string file = "recorded0.wav") {
        run = true;
        taskPath = file;
        std::thread([&]() {
            recorder.startRecording();
            while (run) {
                if (recorder.silentTimer >= recorder.SILENT_TIMEOUT) {
                    isRecorded = true;
                    //recorder.stopRecording();
                    break;
                }
            }
        }).detach();
    }

    void EndListen(bool save = true) {
        ResetRecorded();
        if (save)
            recorder.saveToWav(taskPath);
    }

    bool IsRecorded() {
        std::lock_guard<std::mutex> lock(recordedData_mutex); // 加锁
        return isRecorded;
    };

    void ResetRecorded() {
        run = false;
        isRecorded = false;
        recorder.silentTimer = 0;
        recorder.stopRecording(); // 停止录音
        std::lock_guard<std::mutex> lock(recordedData_mutex); // 加锁
        recordedData.clear(); // 清空录音数据
    }

    void playRecorded(bool islisten = true) {
        // 使用std::shared_ptr来管理filename的生命周期
        auto filename_ptr = std::make_shared<std::string>(taskPath);

        // 播放音频文件
        if (islisten)
            Utils::playAudioAsync(*filename_ptr, [&]() {
                listen();
            });
        else
            Utils::playAudioAsync(*filename_ptr, nullptr);

        // 在回调函数中使用std::weak_ptr来获取filename的引用
        auto callback = [filename_ptr]() {
            std::weak_ptr<std::string> weak_ptr = filename_ptr;
            if (auto ptr = weak_ptr.lock()) {
                // 在回调函数中使用filename的引用
                LogInfo("Audio file {0} finished playing", *ptr);
            }
        };

        // 启动异步任务，并将Lambda表达式作为参数传递
        std::async(std::launch::async, callback);
    }

    void changeFile(std::string filename) {
        taskPath = filename;
    }

    std::vector<float> getRecordedData() {
        std::lock_guard<std::mutex> lock(recordedData_mutex); // 加锁
        return recordedData;
    }

private:
    std::string taskPath;
    bool isRecorded = false;
    bool run = true;
    Recorder recorder;
    std::vector<float> recordedData;
    std::mutex recordedData_mutex; // 互斥锁
    friend int Recorder::recordCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
                                        const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags,
                                        void *userData);
};

class Audio {
public:
    static void playRecordedAudio(const std::vector<float> &audioData);
};

#endif