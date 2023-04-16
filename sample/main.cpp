#include "ChatBot.h"
#include "VoiceToText.h"
#include "Translate.h"
#include "Recorder.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>


using namespace std;

int main(int argc, char *argv[]) {
    try {
        Log::Logger::Init();
/*        OpenAIData chatData;
        chatData.api_key = "";//your api key

        Proxy proxy;
        proxy.proxy = "";//if you need proxy
        chatData.proxy = proxy;
        ChatBot bot(chatData);
        TranslateData data;
        data.appId = "";
        data.APIKey = "";
        Translate translate(data);*/
        const int sampleRate = 16000;
        const int framesPerBuffer = 512;
        Recorder recorder(sampleRate, framesPerBuffer);
        recorder.startRecording();

        // Wait for 5 seconds
        std::this_thread::sleep_for(std::chrono::seconds(5));

        recorder.stopRecording();

        // Save recorded audio to WAV file

        std::vector<float> recordedData = recorder.getRecordedData();
        recorder.saveToWav("recorded.wav");

        /*
        VoiceToText voiceToText(chatData);
        std::string ques = "Hello nice to meet you today";
        std::string res = bot.Submit(ques, Role::System);
        Log::Info("bot:" + res);*/
        //Log::Info("Convert voice to text: {0}",voiceToText.Convert("recorded.wav"));
    }
    catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }

    system("pause");
    return 0;
}

