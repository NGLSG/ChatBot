<p align="center">
    <img src="https://github.com/NGLSG/ChatBot/raw/main/img/self.png" width="200" height="200" alt="ChatBot">
</p>

<div align="center">

# ChatBot

_✨基于nlohmann,cpr,spdlog的ChatGPT API✨_

如果你喜欢这个项目请点一个⭐吧

</div>


<p align="center">
  <img src="https://img.shields.io/badge/Author-Ge%E6%B1%81%E8%8F%8C-yellow">
  <a href="https://raw.githubusercontent.com/NGLSG/ChatBot/main/LICENSE">
    <img src="https://img.shields.io/github/license/NGLSG/ChatBot" alt="license">
  </a>
  <img src="https://img.shields.io/github/stars/NGLSG/ChatBot.svg" alt="stars">
  <img src="https://img.shields.io/github/forks/NGLSG/ChatBot.svg" alt="forks">
</p>

# 使用

初始化

```c++
    #include<ChatBot.h>
    OpenAIData chatData;
    chatData.api_key = "sk-xxxx";//设置你的API_Key
    ChatBot bot(chatData);//初始化
```

* **会话**
    *
        * 提交对话
      ```c++
      bot.Submit("Hello","user");//对话内容,及对话模式 默认为user
      bot.SubmitAsync("Hello","user");//异步提交对话内容,及对话模式 默认为user
      ```

*
    * 保存会话
      ```c++
      bot.Save("Conversation Name");//保存会话,默认为default
      ```

*
    * 加载会话

      ```c++
      bot.Load("Conversation Name");//加载会话 默认为default
      ```

*
    * 删除会话

      ```c++
      bot.Del("Conversation Name");//删除会话
      ```

*
    * 重置会话

      ```c++
      bot.Reset();//重置当前会话
      ```
* **日志功能**
    * Log::Trace()
    * Log::Info()
    * Log::Warn()
    * Log::Error()
    * Log::Fatal()

* **语音转文本**
    * 初始化
    ```c++
    #include <VoiceToText.h>
    OpenAIData Data;
    Data.api_key = "sk-xxx";//your api key
    VoiceToText voiceToText(Data);
    ```
* 
  * 转化
      ```c++
        string res=voiceToText.Convert("path to voice file");
      ```


# 依赖

你需要使用[vcpkg](https://github.com/microsoft/vcpkg)安装cpr,spdlog,nlohmann

* Windows
 ```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
bootstrap-vcpkg.bat
vcpkg install nlohmann-json
vcpkg install cpr
vcpkg install spdlog
vcpkg integrate install
```  
* Linux
 ```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
bootstrap-vcpkg.sh
vcpkg install nlohmann-json
vcpkg install cpr
vcpkg install spdlog
vcpkg integrate install
```  

# 编译

```bash
cd ChatBot
mkdir build
cd build
cmake -B build/ -S . -DCMAKE_TOOLCHAIN_FILE=path/to/vcpkg.cmake
cd build
cmake --build .
```
