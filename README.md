# Image Processing Template

- [Image Processing Template](#image-processing-template)
  - [Specs](#specs)
    - [Notes](#notes)
  - [Setup the requirements on your system](#setup-the-requirements-on-your-system)
    - [Arch Linux](#arch-linux)
    - [Unknown Linux](#unknown-linux)
    - [MacOS](#macos)
    - [Windows](#windows)
  - [Build](#build)
    - [Help menu](#help-menu)
    - [Get up and running](#get-up-and-running)
    - [Fast rebuild and run](#fast-rebuild-and-run)
    - [Build without the helper script](#build-without-the-helper-script)
  - [Recommendations](#recommendations)
    - [VSCode](#vscode)
    - [CLion](#clion)
    - [VisualStudio](#visualstudio)

## Specs

This is a template project for IP. It contains a handful of utilities and it should be considered as a playground for image processing. It should work on any device with **conan** and **cmake** installed.

**It features:**

1. `.cpp` and `.h` files are automatically included into the main executable
2. Automatic `target_include_directories` via the `HEADER_DIRECTORIES` CMAKE macro
3. Mini script runner to enchance the build process.
4. Lightweight macro wrappers over `spdlog` (for debugging)

> Currently, windows requires manual adjustments for OpenCV's `find_package`.

### Notes

The following folder structure is mandatory

```text
assets \
  exports
```

The following packages are mandatory:

```text
gcc make cmake python conan opencv
```

For the current UI (radio buttons + per-parameter checkboxes), OpenCV must be built with Qt HighGUI support (`WITH_QT=ON`).

<div class="page" />

## Setup the requirements on your system

### Arch Linux

```bash
sudo pacman -S base-devel cmake opencv qt6-base
```

There is a [Conan AUR package](https://aur.archlinux.org/packages/conan)

```bash
yay -S conan
```

### Unknown Linux

Search for base devel packages, usually every distro has one.

**Debian based example:**

```bash
apt-get install build-essential
```

For Qt-capable OpenCV on Debian/Ubuntu-like systems, ensure Qt development packages are installed before building OpenCV from source (for example: `qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools libqt5opengl5-dev`).

Check the conan [installation options](https://conan.io/downloads.html). You can go for the python package installation via pip.

```bash
pip install conan
```

### MacOS

Homebrew instructions are available [here](https://brew.sh/).

```bash
brew install opencv
```

```bash
brew update && brew upgrade && brew cleanup
```

```bash
brew install pkg-config
```

Taken from the [conan download options](https://conan.io/downloads.html) available:

```bash
brew install conan
```

### Windows

Manual setup is required. After the installation of CMake and Conana, you must download the OpenCV library and point `find_package(OpenCV REQUIRED)` to the correct location or **manually overwrite** `${OpenCV_LIBS}` such that `target_link_libraries(${PROJECT_NAME} PUBLIC ${OpenCV_LIBS})` works

## Build

Generally, the order is

1. Install dependencies
2. Build
3. Run

**Several ways to do that will be provided**, both *using* the helper script, and *without using* it.

At the time of writing this, the conan version used is `Conan version 1.44.0` with experimental features enabled. Future versions of conan (>2.0+) should not be affected.

This workspace already uses Conan 2-compatible scripts (`scripts/conan_install.sh`).

The following aliases are used in this document:

```bash
alias run="./run.sh"
alias log="./log_run.sh"
```

Both **run** and **log** accept the same arguments, that of the command to **run**. Both commands will log runtime information, however `log.sh` will also log the whole building process.

<div class="page" />

### Help menu

```bash
run help
```

```text
Example:
  run clean build opengl_template
  run opengl_template
  run build
  run clean build
  run clean
  run conan

  :help   displays this message
  :clean  clean build folder
  :build  build project
  :exec   execute the executable
  :cb     clean build shorthand
  :conan  same as build
  :dependencies  conan install dependencies
```

The run script will take its arguments and execute them sequentially.

Custom commands can be provided as stringified paths to the run command.

Since this is a simple script, i recommend to read it for further customisation and understanding of the build process.

### Get up and running

```bash
run clean dependencies build execute
```

### Fast rebuild and run

```bash
run build execute
```

### Verify Qt backend

The app now requires OpenCV Qt HighGUI support and exits early if the Qt backend is missing.

At runtime, check `cv::getBuildInformation()` and confirm the GUI section reports Qt.

### Runtime controls

- Main window (`Image Processing`) displays source/result side-by-side.
- The same main window (`Image Processing`) hosts effect buttons, trackbars, and ON/OFF toggles.
- Left/Right arrows cycle effects.
- Number keys `1..9` select effects by index.
- Qt push buttons are generated dynamically (one per effect) and switch the active effect directly.
- Each parameter has a trackbar and an `ON/OFF` checkbox.
- When a parameter is OFF, its effect uses that parameter's declared neutral value.
- Trackbars enforce each parameter bounds (`minValue..maxValue`); invalid specs are auto-corrected silently.

### Add multiple trackbars to one effect

Define all parameters in the `trackbars` list of one `SliderEntry`.

```cpp
{ "Additive Factor",
  [&]() { additive_factor(img, outImg, controls); },
  {
      {"factor", 128, 255, 128, true, 0},
      {"offset", 50, 100, 50, true, 0}
  },
  "Brightness shift"
}
```

In the effect logic, read each parameter by the same key using `ControlsManager`:

```cpp
int factor = controls.getEffective("factor") - 128;
int offset = controls.getEffective("offset");
```

- Use `get("key")` for raw current trackbar value.
- Use `getEffective("key")` when you want `ON/OFF` to matter (returns `neutralValue` when OFF).

<div class="page" />

### Build without the helper script

```bash
# Create build folder
mkdir build 
cd build
# Copy dependencies that will be compiled alongside this project
conan source .. --source-folder dependencies
# Build and Link other dependencies that do not require dependency management
conan install .. --build missing
# Build the project
conan build ..
```

## Recommendations

### VSCode

If using VSCode, I recommend the following settings for **command-runner**.

```json
"command-runner.terminal.autoClear": true,
"command-runner.terminal.autoFocus": true,
"command-runner.commands": {
  "install": "./run.sh dependencies",
  "build": "./run.sh conan",
  "run": "./run.sh conan execute",
  "clean": "./run.sh clean",
  "build run": "./run.sh dependencies conan execute",
  "log build": "./log_run.sh dependencies conan",
  "log run": "./log_run.sh conan execute",
  "log build run": "./log_run.sh dependencies conan execute"
}
```

Now useful commands are accesible via the `CTRL + SHIFT + R` shortcut.

If you want to extend the build process with packages that are not on the conan repository, you can fill in the `source` method of the `ImageProcessingTemplate` class inside [the conan.py file](./conanfile.py).

More details about extending the codebase using source can be read [here](https://docs.conan.io/en/1.44/reference/conanfile/methods.html).

### CLion

I recommend using the VSCode setup, it provides enough functionality and intelisence. However, if you are going for CLion, try to explicitly disable the automatic append of new files in the `add_executable()` statement of the `CMakeLists.txt` file. It is redundant since it already scans for all the **.cpp** and **.h** and includes them in the executable. This can cause errors such as `function x redefined here` or `undefined refference to x`.

### VisualStudio

> Bruh...
