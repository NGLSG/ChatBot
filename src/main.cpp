#include "ChatBot.h"

int main() {
    try {
        ChatData chatData;
        chatData.api_key = "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";

        Proxy proxy;
        proxy.proxy = "http://192.168.0.100:2013";
        chatData.proxy = proxy;

        ChatBot bot(chatData);
        std::string ques = "now your name is Bot";
        std::string res = bot.submit(ques, Role::System);
        bot.LogInfo("bot:" + res);
        res = bot.submit("what your name?");
        bot.LogInfo(res);
        bot.save();
        bot.reset();
        res = bot.submit("what your name?");
        bot.LogInfo(res);
        bot.load();
        res = bot.submit("what your name?");
        bot.LogInfo(res);

    }
    catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }

    system("pause");
    return 0;
}