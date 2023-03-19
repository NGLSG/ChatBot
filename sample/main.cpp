#include "ChatBot.h"
#include "VoiceToText.h"

int main() {
    try {
        OpenAIData chatData;
        chatData.api_key = "";//your api key

        Proxy proxy;
        proxy.proxy = "";//if you need proxy
        chatData.proxy = proxy;

        ChatBot bot(chatData);
        VoiceToText voiceToText(chatData);

        std::string ques = "Hello nice to meet you today";
        std::string res = bot.Submit(ques, Role::System);
        Log::Info("bot:" + res);
        //Log::Info("Convert voice to text: {0}",voiceToText.Convert("test.mp3"));
    }
    catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }

    system("pause");
    return 0;
}