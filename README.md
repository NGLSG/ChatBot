<p align="center">
    <img src="https://github.com/NGLSG/ChatBot/raw/main/img/self.png" width="200" height="200" alt="ChatBot">
</p>

<div align="center">

<h1 style="background: linear-gradient(to right, #00ffcc, #0099ff, #6600ff); -webkit-background-clip: text; color: transparent; font-style: italic;">ChatBot</h1>

[English](READEME_EN.md) / [中文](README.md)

_✨基于各种LLM的聊天机器人框架，支持多语言，语音唤醒,语音对话,本地执行功能,支持 OpenAI，Claude，讯飞星火，Stable Diffusion，ChatGLM，通义千问，腾讯混元，360 智脑，百川 AI，火山方舟，Ollama ,Gemini等API✨_

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

Demo示例:![img.png](img/img.png)

# 依赖项

ChatBot项目需要以下依赖项：

* nlohmann-json
* cpr
* PortAudio
* OpenGL
* imgui
* glfw3
* yaml-cpp
* sol2
* Lua
* Stb
* SDL2
* SDL2_image
* glad
* OpenSSL

您可以使用[vcpkg](https://github.com/microsoft/vcpkg)来安装这些依赖项。以下是安装这些依赖项的步骤：

* **VCPKG 安装**

    * Windows
      ```bash
      git clone https://github.com/Microsoft/vcpkg.git
      cd vcpkg
      ./bootstrap-vcpkg.bat
      ```  

    * Linux
      ```bash
      git clone https://github.com/Microsoft/vcpkg.git
      cd vcpkg
      ./bootstrap-vcpkg.sh
      ```  

* **使用vcpkg安装依赖**
    ```bash
    vcpkg install nlohmann-json
    vcpkg install cpr
    vcpkg install PortAudio
    vcpkg install OpenGL
    vcpkg install imgui
    vcpkg install glfw3
    vcpkg install yaml-cpp
    vcpkg install sol2
    vcpkg install Lua
    vcpkg install Stb
    vcpkg install SDL2
    vcpkg install SDL2_image
    vcpkg install glad
    vcpkg install OpenSSL
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