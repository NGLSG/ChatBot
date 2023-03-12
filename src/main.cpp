#include <iostream>
#include "../include/ChatBot.h"

int main() {
    try {
        ChatData chatData;
        chatData.api_key = "sk-xxxx";//your api key

        Proxy proxy;
        proxy.proxy = "http://ip:port or https://ip:port";//if you need proxy
        chatData.proxy = proxy;
        ChatBot bot(chatData);

        std::cout << bot.submit("hello") << std::endl;
    }
    catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }


    system("pause");
    return 0;
}

