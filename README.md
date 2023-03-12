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
    ChatData chatData;
    chatData.api_key = "sk-xxxx";//设置你的API_Key
    ChatBot bot(chatData);//初始化
```

* 会话相关
  *
      * 提交对话
      ```c++
      bot.submit("Hello","user");//对话内容,及对话模式 默认为user
      ```
        
* * 保存会话
    ```c++
    bot.save("Conversation Name");//保存会话,默认为default
    ```
       
* * 加载会话

    ```c++
    bot.load("Conversation Name");//加载会话 默认为default
    ```
        
* * 删除会话

    ```c++
    bot.del("Conversation Name");//删除会话
    ```
        
* * 重置会话

    ```c++
    bot.reset();//重置当前会话
    ```
        

# 依赖

你需要使用[vcpkg](https://github.com/microsoft/vcpkg)安装cpr,spdlog,nlohmann

 ```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
bootstrap-vcpkg.bat
vcpkg install nlohmann-json
vcpkg install cpr
vcpkg install spdlog
vcpkg integrate install
```  


# 编译
```bash
mkdir build
cd build
cmake -B build/ -S . -DCMAKE_TOOLCHAIN_FILE=vcpkgdirectory/scripts/buildsystems/vcpkg.cmake
cd build
cmake --build .
```
