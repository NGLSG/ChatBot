#include "ChatBot.h"

int main() {
    try {
        std::cout << "Bot:" << std::endl;
        ChatData chatData;
        chatData.api_key = "";

        Proxy proxy;
        proxy.proxy = "";
        chatData.proxy = proxy;
        ChatBot bot(chatData);
        std::string res = bot.submit("记住a=1");
        bot.LogInfo(res);
        bot.reset();
        res = bot.submit("a=?");
        bot.LogInfo(res);
        //bot.save();

    }
    catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }

    system("pause");
    return 0;
}