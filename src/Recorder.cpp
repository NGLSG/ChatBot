#include "Recorder.h"

extern bool NoRecord;
bool Iscount = false;

Recorder::Recorder(int sampleRate, int framesPerBuffer)
    : sampleRate(sampleRate), framesPerBuffer(framesPerBuffer)
{
    Logger::Init();
    Iscount = false;
    PaError err = Pa_Initialize();
    if (err != paNoError)
    {
        LogError("PortAudio initialization failed: {0}", Pa_GetErrorText(err));
    }
}

Recorder::~Recorder()
{
    Pa_Terminate();
}

void Recorder::startRecording()
{
    if (deviceId != -1)
    {
        LogInfo("Starting recording...");

        PaStreamParameters inputParameters;
        inputParameters.device = deviceId;
        inputParameters.channelCount = 1;
        inputParameters.sampleFormat = paFloat32;
        inputParameters.suggestedLatency = Pa_GetDeviceInfo(deviceId)->defaultLowInputLatency;
        inputParameters.hostApiSpecificStreamInfo = NULL;

        PaError err = Pa_OpenStream(&stream,
                                    &inputParameters,
                                    NULL, // no output parameters for recording
                                    sampleRate,
                                    framesPerBuffer,
                                    paClipOff,
                                    recordCallback,
                                    this);
        if (err != paNoError)
        {
            LogError("PortAudio open stream failed: %s", Pa_GetErrorText(err));
            return;
        }

        err = Pa_StartStream(stream);
        if (err != paNoError)
        {
            LogError("PortAudio start stream failed: %s", Pa_GetErrorText(err));
        }
    }
}

void Recorder::stopRecording(bool del)
{
    if (deviceId != -1)
    {
        LogInfo("Stopping recording...");
        silentTimer = 0;
        Iscount = false;
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        stream = NULL;
    }
}

std::vector<float> Recorder::getRecordedData() const
{
    return recordedData;
}

void Recorder::saveToWav(const std::string& fileName)
{
    if (!getRecordedData().empty())
    {
        const std::vector<float>& pcmData = getRecordedData();
        recordedData.clear();
        FILE* file = fopen(fileName.c_str(), "wb");
        if (file)
        {
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
            for (int i = 0; i < pcmData.size(); i++)
            {
                wavData[i] = static_cast<short>(pcmData[i] * 32767.0f);
            }
            fwrite(wavData.data(), 2, wavData.size(), file);

            fclose(file);
            LogInfo("Saved audio to {0}", fileName);
        }
        else
        {
            LogError("Failed to open file {0}", fileName);
        }
    }
}

void Recorder::ChangeDevice(std::string device)
{
    this->device = device;
    int numDevices = Pa_GetDeviceCount();
    deviceId = -1;
    for (int i = 0; i < numDevices; i++)
    {
        const PaDeviceInfo* devInfo = Pa_GetDeviceInfo(i);
        if (devInfo && !device.empty() && strcmp(devInfo->name, device.c_str()) == 0)
        {
            deviceId = i;
            break;
        }
    }

    if (deviceId == -1)
    {
        LogError("Could not find device: %s", device.c_str());
        return;
    }
}

int Recorder::recordCallback(const void* inputBuffer, void* outputBuffer, unsigned long framesPerBuffer,
                             const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags,
                             void* userData)
{
    bool silence = true;
    if (NoRecord)
    {
        return paContinue;
    }


    Recorder* recorder = static_cast<Recorder*>(userData);
    const float* input = static_cast<const float*>(inputBuffer);


    for (int i = 0; i < framesPerBuffer; i++)
    {
        if (std::abs(input[i]) > 0.01)
        {
            silence = false;
            break;
        }
        else
        {
            silence = true;
        }
    }

    if (!silence)
    {
        Iscount = true;
        recorder->silentTimer = 0.0;
    }

    if (silence && recorder->silentTimer >= recorder->SILENT_TIMEOUT)
    {
        return paComplete;
    }


    if (!silence)
    {
        recorder->recordedData.insert(recorder->recordedData.end(), input, input + framesPerBuffer);
    }

    if (Iscount)
        recorder->silentTimer += framesPerBuffer * 1000 / recorder->sampleRate;
    return paContinue;
}

Listener::Listener(int sampleRate, int framesPerBuffer)
{
    recorder = std::make_shared<Recorder>(sampleRate, framesPerBuffer);
}

Listener::~Listener()
{
}

void Listener::EndListen(bool save)
{
    ResetRecorded();
    if (save)
        recorder->saveToWav(taskPath);
}

bool Listener::IsRecorded()
{
    std::lock_guard<std::mutex> lock(recordedData_mutex); // 加锁
    return isRecorded;
}

void Listener::ResetRecorded()
{
    run = false;
    isRecorded = false;
    recorder->stopRecording(); // 停止录音
    std::lock_guard<std::mutex> lock(recordedData_mutex); // 加锁
    recordedData.clear(); // 清空录音数据
}

void Listener::playRecorded(bool islisten)
{
    // 使用std::shared_ptr来管理filename的生命周期
    auto filename_ptr = std::make_shared<std::string>(taskPath);

    // 播放音频文件
    if (islisten)
        Utils::playAudioAsync(*filename_ptr, [&]()
        {
            listen();
        });
    else
        Utils::playAudioAsync(*filename_ptr, nullptr);

    // 在回调函数中使用std::weak_ptr来获取filename的引用
    auto callback = [filename_ptr]()
    {
        std::weak_ptr<std::string> weak_ptr = filename_ptr;
        if (auto ptr = weak_ptr.lock())
        {
            // 在回调函数中使用filename的引用
            LogInfo("Audio file {0} finished playing", *ptr);
        }
    };

    // 启动异步任务，并将Lambda表达式作为参数传递
    std::async(std::launch::async, callback);
}

void Listener::changeFile(std::string filename)
{
    taskPath = filename;
}

void Listener::ChangeDevice(std::string device)
{
    LogInfo("更换设备");
    recorder->stopRecording();
    recorder->ChangeDevice(device);
    recorder->startRecording();
}

std::vector<float> Listener::getRecordedData()
{
    std::lock_guard<std::mutex> lock(recordedData_mutex); // 加锁
    return recordedData;
}

void Listener::listen(std::string file)
{
    run = true;
    taskPath = file;
    std::thread([&]()
    {
        recorder->startRecording();

        while (run)
        {
            if (recorder->silentTimer.load() > recorder->SILENT_TIMEOUT)
            {
                LogInfo("Silent Timer:{0}", recorder->silentTimer.load());
                isRecorded = true;
                break;
            }
        }
    }).detach();
}

void Audio::playRecordedAudio(const std::vector<float>& audioData)
{
    // Play audio data
    PaStream* stream;
    Pa_Initialize();
    Pa_OpenDefaultStream(&stream, 0, 1, paFloat32, 44100, 512, NULL, NULL);
    Pa_StartStream(stream);
    Pa_WriteStream(stream, audioData.data(), audioData.size());
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
}
