# ChatBot

_✨A chatbot based on various LLMs, supporting multiple languages and functionalities.✨_

If you like this project, please give it a ⭐️!

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
