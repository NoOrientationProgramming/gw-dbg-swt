
<h2 id="codeorb-start" style="display:none;"></h2>

![GitHub](https://img.shields.io/github/license/NoOrientationProgramming/code-orb?style=plastic)

[![Linux Native](https://github.com/NoOrientationProgramming/code-orb/actions/workflows/linux-native.yml/badge.svg?branch=main)](https://github.com/NoOrientationProgramming/code-orb/actions/workflows/linux-native.yml) [![Windows Native](https://github.com/NoOrientationProgramming/code-orb/actions/workflows/windows-native.yml/badge.svg?branch=main)](https://github.com/NoOrientationProgramming/code-orb/actions/workflows/windows-native.yml) [![MacOS Native](https://github.com/NoOrientationProgramming/code-orb/actions/workflows/macos-native.yml/badge.svg)](https://github.com/NoOrientationProgramming/code-orb/actions/workflows/macos-native.yml) [![Cross arm & mingw](https://github.com/NoOrientationProgramming/code-orb/actions/workflows/arm-and-mingw.yml/badge.svg?branch=main)](https://github.com/NoOrientationProgramming/code-orb/actions/workflows/arm-and-mingw.yml)

<p align="center">
  <kbd>
    <img src="https://raw.githubusercontent.com/NoOrientationProgramming/code-orb/main/doc/res/codeorb.jpg" style="width: 700px; max-width:100%"/>
  </kbd>
</p>

## The Microcontroller Debugger

When working with small targets, simple log outputs are often the only feedback available.
With [CodeOrb](https://github.com/NoOrientationProgramming/code-orb#codeorb-start) on the PC and
[Processing()](https://github.com/NoOrientationProgramming/ProcessingCore#processing-start) on the target,
you get two additional features: a task viewer and a command interface.
The task viewer provides a detailed insight into the entire system, whereas the command interface gives full control over the microcontroller.

## What You Get

- Full control over your target
- Crystal-clear insight into your system
- Through three dedicated channels
  - Process Tree
  - Log
  - Command Interface

## Status

- Pre alpha
- ETA: June 2025

## How To Use

### Topology

This repository provides `CodeOrb` the microcontroller debugger highlighted in orange. Check out the [example for STM32](https://github.com/NoOrientationProgramming/hello-world-stm32) as well!

<p align="center">
  <kbd>
    <img src="https://raw.githubusercontent.com/NoOrientationProgramming/code-orb/main/doc/system/topology.svg" style="width: 300px; max-width:100%"/>
  </kbd>
</p>

### Process Tree

<p align="center">
  <kbd>
    <img src="https://raw.githubusercontent.com/NoOrientationProgramming/code-orb/main/doc/screenshots/Screenshot%20from%202025-04-08%2022-40-02.png" style="width: 700px; max-width:100%"/>
  </kbd>
</p>

### Log

TODO: Screenshot

### Command Interface

TODO: Screenshot

## How To Build

### Requirements

You will need [meson](https://mesonbuild.com/) and [ninja](https://ninja-build.org/) for the build.
Check the instructions for your OS on how to install these tools.

### Steps

Clone repo
```
git clone https://github.com/NoOrientationProgramming/code-orb.git
```

Enter the directory
```
cd code-orb
```

Initialize GIT submodules
```
git submodule update --init --recursive
```

Setup build directory
```
meson setup build-native
```

Build the application
```
ninja -C build-native
```

Test CodeOrb on Windows
```
.\build-native\CodeOrb.exe --help
```

Test CodeOrb on UNIX systems
```
./build-native/codeorb --help
```

The output should look like this
```
CodeOrb - Microcontroller Debugging
Version: CodeOrb_1-25.04-1

Usage: codeorb [OPTION]

Required

None

Optional

  -d,  --device <string>             Device used for UART communication. Default: /dev/ttyACM0
  -c,  --code <string>               Code used for UART initialization. Default: aaaaa
  -v,  --verbosity <uint8>           Verbosity: high => more output
  --,  --ignore_rest                 Ignores the rest of the labeled arguments following this flag.
  -h,  --help                        Displays usage information and exits.
       --start-ports-target <uint16> Start of 3-port interface for the target. Default: 3000
       --start-ports-orb <uint16>    Start of 3-port interface for CodeOrb. Default: 2000
       --refresh-rate <uint16>       Refresh rate of process tree in [ms]
       --ctrl-manual                 Use manual control (automatic control disabled)
       --core-dump                   Enable core dumps
       --version                     Displays version information and exits.
```
