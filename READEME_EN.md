<p align="center">
    <img src="https://github.com/NGLSG/ChatBot/raw/main/img/self.png" width="200" height="200" alt="ChatBot">
</p>

<div align="center">

<h1 style="background: linear-gradient(to right, #00ffcc, #0099ff, #6600ff); -webkit-background-clip: text; color: transparent; font-style: italic;">ChatBot</h1>


✨A chatbot based on various LLMs, supporting multiple languages, system operations, and mathematical processing, with a variety of features.✨

If you like this project, please give it a ⭐️!

</div>

Demo示例:![img.png](img/demo.gif)

<p align="center">
  <img src="https://img.shields.io/badge/Author-Ge%E6%B1%81%E8%8F%8C-yellow">
  <a href="https://raw.githubusercontent.com/NGLSG/ChatBot/main/LICENSE">
    <img src="https://img.shields.io/github/license/NGLSG/ChatBot" alt="license">
  </a>
  <img src="https://img.shields.io/github/stars/NGLSG/ChatBot.svg" alt="stars">
  <img src="https://img.shields.io/github/forks/NGLSG/ChatBot.svg" alt="forks">
</p>

---

## Dependencies

The ChatBot project requires the following dependencies:

* nlohmann-json
* cpr
* spdlog
* openssl
* portaudio
* opengl
* imgui
* yaml-cpp
* libarchive
* lua
* sol2
* sdl2
* glad

You can use [vcpkg](https://github.com/microsoft/vcpkg) to install these dependencies. Here are the steps to install these dependencies:

1. **Install VCPKG**
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

2. **Install Dependencies using VCPKG**
    ```bash
    vcpkg install nlohmann-json
    vcpkg install cpr
    vcpkg install spdlog
    vcpkg install openssl
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

## Compilation

```bash
cd ChatBot
mkdir build
cd build
cmake -B build/ -S . -DCMAKE_TOOLCHAIN_FILE=path/to/vcpkg.cmake
cd build
cmake --build .
