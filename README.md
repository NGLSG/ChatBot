<p align="center">
    <img src="https://github.com/NGLSG/ChatBot/raw/main/img/self.png" width="200" height="200" alt="ChatBot">
</p>

<div align="center">

# ChatBot

[English](READEME_EN.md) / [中文](README.md)

_✨基于各种LLM的聊天机器人，支持多种语言，支持多种功能。✨_

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

# 依赖项

ChatBot项目需要以下依赖项：

* nlohmann-json
* cpr
* spdlog
* boringssl
* portaudio
* opengl
* imgui
* yaml-cpp
* libarchive
* lua
* sol2
* sdl2
* glad

您可以使用[vcpkg](https://github.com/microsoft/vcpkg)来安装这些依赖项。以下是安装这些依赖项的步骤：

* _**VCPKG 安装**_
*
    * Windows

       ```bash
      git clone https://github.com/Microsoft/vcpkg.git
      cd vcpkg
      ./bootstrap-vcpkg.bat

      ```  

*
    * Linux

       ```bash
      git clone https://github.com/Microsoft/vcpkg.git
      cd vcpkg
      ./bootstrap-vcpkg.sh
      ```  
* **_使用vcpkg安装依赖_**
    ```bash
    vcpkg install nlohmann-json
    vcpkg install cpr
    vcpkg install spdlog
    vcpkg install boringssl
    vcpkg install portaudio
    vcpkg install opengl
    vcpkg install imgui
    vcpkg install yaml-cpp
    vcpkg install libarchive
    vcpkg install imgui[docking-experimental]
    vcpkg install lua
    vcpkg install sol2
    vcpkg install sdl2
    vcpkg install sdl2-image
    vcpkg install glad
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
