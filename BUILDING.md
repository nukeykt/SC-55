# Building

## Linux

## MacOS

### Xcode


**install prerequisites (brew/macports - untested):**
```
git
cmake
SDL2
SDL2_image
```

open terminal.app

`git clone --recurse-submodules https://github.com/nukeykt/Nuked-SC55.git`

create an Xcode project:

```
$ cd Nuked-SC55
$ cmake -G Xcode .
```

- open created Xcode project
- Product -> Scheme -> Edit Scheme -> Build Configuration set to Release
- build
- copy data/back.data the same directory as built binary


## Windows

### VisualStudio 2022

#### **install prerequisites:**
- ##### Visual Studio with Windows SDK and [CMake](https://learn.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio?view=msvc-170)
  
- ##### git:
in cmd:
```
winget install Git.Git
```
  
- ##### pkg-config:
in new cmd:
```
c:
cd %ProgramData%
md pkg-config
cd pkg-config
setx PKG_CONFIG_PATH %cd%
winget install wget
wget https://download.gnome.org/binaries/win64/glib/2.26/glib_2.26.1-1_win64.zip
wget https://download.gnome.org/binaries/win64/dependencies/gettext-runtime_0.18.1.1-2_win64.zip
wget https://download.gnome.org/binaries/win64/dependencies/pkg-config_0.23-2_win64.zip
tar -xf glib_2.26.1-1_win64.zip
tar -xf gettext-runtime_0.18.1.1-2_win64.zip
tar -xf pkg-config_0.23-2_win64.zip
del glib_2.26.1-1_win64.zip
del gettext-runtime_0.18.1.1-2_win64.zip
del pkg-config_0.23-2_win64.zip
cd bin
setx PATH "%PATH%;%cd%"
```

- ##### [vcpkg](https://github.com/microsoft/vcpkg):
in new cmd:
```
c:
cd %ProgramData%
git clone https://github.com/microsoft/vcpkg
cd vcpkg
setx VCPKG_PATH %cd%
setx PATH "%PATH%;%cd%"
.\bootstrap-vcpkg.bat -disableMetrics
```

in new admin-cmd:

`C:\ProgramData\vcpkg\vcpkg integrate install`

- ##### [SDL2](https://github.com/libsdl-org)
###### Get SDL2

in new cmd:
```
vcpkg install SDL2 SDL2-image
setx SDL2_DIR %VCPKG_PATH%\installed\x64-windows\share\sdl2
```

- ##### build
###### **Example in new cmd:**

```
cd..
git clone --recurse-submodules https://github.com/nukeykt/Nuked-SC55.git
cd .\Nuked-SC55
cmake . -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -G"Visual Studio 17 2022"
cmake --build . --config Release
copy .\data\back.data .\Release
explorer .\Release
```


### MSYS2

[MSYS2](https://www.msys2.org/wiki/MSYS2-installation/)-**MSYS2 MinGW32** shell

#### **install prerequisites:**
```
pacman -S base-devel libtool pkg-config make gettext gcc git cmake mingw-w64-i686-gcc mingw-w64-x86_64-gcc mingw-w64-i686-cmake mingw-w64-x86_64-cmake mingw-w64-i686-pkg-config mingw-w64-x86_64-pkg-config mingw-w64-i686-toolchain mingw-w64-x86_64-toolchain mingw-w64-i686-SDL2 mingw-w64-i686-SDL2_image mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_image
```
Note: you are asked twice to make a selection - just press "Return"/"Enter" to select all


#### **use `msys2mingw32-build-release.sh`**

```
git clone --recurse-submodules https://github.com/nukeykt/Nuked-SC55.git
cd ./Nuked-SC55
sh ./msys2mingw32-build-release.sh
```