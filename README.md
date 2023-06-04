# Introduction

Audio-keepawake is a program that periodically plays inaudible sounds in the low-frequency range. The purpose of the application is to keep audio devices active and running that would otherwise go into standby. In particular, KRK ROKIT 5 studio monitors have this annoying and impossible to disable standby feature that makes them automatically turn off unless a loud sound is played once in a while. This application ensures the speakers stay on while the PC is running.

This application is a GUI-less cross-platform implementation of [Rokit Monitor Anti Standby (RMAS)](https://janeisklar.net/2018/10/03/rokit-krk-auto-standby-deactivation-with-software/).

# How to use

Please note that, depending on your use-case, you will have to fine-tune some parameters in the C code, namely the audio device name that should be kept awake (`audio->dev_name`) and other properties like how long the interval is between playing the inaudible sounds (`audio->callback_delay_ms`).

To build, pre-requisites are `gcc` and SDL2 development libraries, which can be obtained with:

```sh
sudo apt install build-essential
sudo apt install libsdl2-dev
sudo apt install libsdl2-mixer-dev
```

Then, on Linux with `systemd`, simply running:

```sh
./build.sh
./install.sh
```

will do the job (installs the application for the current user).
