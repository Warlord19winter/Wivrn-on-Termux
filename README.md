# Wivrn on Termux

Run a WiVRn server on an Android phone under Termux and stream OpenXR to a
standalone headset over Wi-Fi, with no PC in the loop. This covers the
phone-side server build. The headset runs the normal WiVRn client app from
its own store.

Built and tested on a Snapdragon 8 Gen 3 phone (Adreno 750, Turnip Vulkan)
with a Quest 3S as the client. A Snapdragon 8 Gen 2 (Adreno 740) works the
same way.

## Contents

**Before you start**
- [What this is not](#what-this-is-not)
- [What you need](#what-you-need)
- [Match the client version first](#match-the-client-version-first)

**Building the server**
- [Step 1 - Termux packages](#step-1---termux-packages)
- [Step 2 - Helper files](#step-2---helper-files)
- [Step 3 - libudev-zero](#step-3---libudev-zero)
- [Step 4 - Get the WiVRn source](#step-4---get-the-wivrn-source)
- [Step 5 - Configure](#step-5---configure)
- [Step 6 - Build](#step-6---build)

**Running it**
- [Step 7 - Runtime setup](#step-7---runtime-setup)
- [Step 8 - Run it](#step-8---run-it)

**Reference**
- [Troubleshooting](#troubleshooting)
- [Credits](#credits)

## What this is not

This is not a prebuilt APK. WiVRn's server is meant for desktop Linux, so
getting it onto Termux means building it from source and working around the
places where Android's Bionic libc and Termux's toolchain differ from a
normal Linux box. It is involved. If you just want VR streaming and own a PC,
run the normal desktop WiVRn server instead.

→ Next: [What you need](#what-you-need) · [↑ Contents](#contents)

## What you need

- A phone with an Adreno GPU and a working Turnip Vulkan driver (Snapdragon
  8-series). Mali and other GPUs are not covered here and probably work or not if anyone has a Mali phone or device please test and report.
- Termux, from F-Droid or the GitHub releases. Not the Play Store build, it
  is too old.
- Termux:X11. The server brings up a display and needs an X server running.
- A headset with the WiVRn client installed on Quest 3s or other Quest headsets, on the same Wi-Fi network as the phone.

→ Next: [Match the client version first](#match-the-client-version-first) · [↑ Contents](#contents)

## Match the client version first

WiVRn checks a protocol version on connect. If the server and the headset
client disagree, the headset is refused with "incompatible protocol version"
and nothing else will work.

So before building anything: check which WiVRn version the client app on your
headset is, and build the server from the matching git tag. My Quest client
was v26.9, so I built the v26.9 tag. Do not build `master` and hope.

→ Next: [Step 1 - Termux packages](#step-1---termux-packages) · [↑ Contents](#contents)

## Step 1 - Termux packages

Native aarch64 packages, not the glibc side:

```
pkg install git cmake ninja clang x264 vulkan-loader vulkan-headers
```

WiVRn also uses system Boost and FreeType, so:

```
pkg install boost boost-headers freetype
```

Then install your phone's native Turnip Vulkan driver
(`mesa-vulkan-icd-freedreno`) and confirm Vulkan is up before going further:

```
vulkaninfo
```

If that does not list your Adreno, stop and fix Vulkan first. The server has
nothing to encode from without it.

→ Next: [Step 2 - Helper files](#step-2---helper-files) · [↑ Contents](#contents)

## Step 2 - Helper files

Termux's Bionic libc and its libc++ are missing a few things WiVRn and Monado
assume. The fixes are three small compat pieces in this repo under `shims/`.
Clone the repo and drop them in your home directory, where the build flags
expect them:

```
git clone https://github.com/Warlord19winter/Wivrn-on-Termux
cp Wivrn-on-Termux/shims/shm_compat.h ~/
cp Wivrn-on-Termux/shims/enumerate_compat.h ~/
cp -r Wivrn-on-Termux/shims/malloc-compat ~/
```

What each one is for:

- `shm_compat.h` gives Bionic a `shm_open` / `shm_unlink` through
  `memfd_create`. Bionic has no POSIX shared memory. Force-included for C and
  C++.
- `enumerate_compat.h` provides `std::ranges::enumerate_view`, which the
  libc++ shipped in Termux does not carry yet. Force-included for C++.
- `malloc-compat/` is an extra include directory added after the system
  headers, covering a couple of allocator declarations Bionic does not expose.

All three are wired in through the compiler flags in Step 5, so the WiVRn
source itself never gets edited.

→ Next: [Step 3 - libudev-zero](#step-3---libudev-zero) · [↑ Contents](#contents)

## Step 3 - libudev-zero

WiVRn and Monado link against libudev, which Termux has no package for.
libudev-zero provides the symbols with no real device management, which is all
the server needs. Build it and install it into your prefix:

```
git clone https://github.com/illiliti/libudev-zero
cd libudev-zero
make
make install PREFIX=$PREFIX
cd ..
```

→ Next: [Step 4 - Get the WiVRn source](#step-4---get-the-wivrn-source) · [↑ Contents](#contents)

## Step 4 - Get the WiVRn source

Clone WiVRn and check out the tag that matches your headset's client (see
above). Monado comes in automatically as a build dependency, so there is
nothing separate to fetch for it. Swap `v26.9` for your client's version:

```
git clone https://github.com/WiVRn/WiVRn
cd WiVRn
git checkout v26.9
```

→ Next: [Step 5 - Configure](#step-5---configure) · [↑ Contents](#contents)

## Step 5 - Configure

Build the server only, force a Linux build even though the compiler still
thinks it is on Android, and wire in the shims. This is the configuration I
actually use:

```
cmake -S . -B build-server -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
  -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
  -DCMAKE_DISABLE_FIND_PACKAGE_Ceres=ON \
  -DWIVRN_BUILD_CLIENT=OFF \
  -DWIVRN_USE_SYSTEM_BOOST=ON \
  -DWIVRN_USE_SYSTEM_FREETYPE=ON \
  -DWIVRN_USE_NVENC=OFF \
  -DCMAKE_C_FLAGS="-U__ANDROID__ -D__OHOS__ -include $HOME/shm_compat.h" \
  -DCMAKE_CXX_FLAGS="-U__ANDROID__ -D__OHOS__ -include $HOME/shm_compat.h -idirafter $HOME/malloc-compat -include $HOME/enumerate_compat.h"
```

Why each piece:

- `CMAKE_SYSTEM_NAME=Linux` stops Monado building its Android backend, which
  would demand `XR_KHR_android_create_instance` and a JVM that does not exist
  here. Without this, no OpenXR client can even create an instance.
- `-U__ANDROID__` because CMake alone does not stop Termux's clang defining
  `__ANDROID__`. Left defined, Monado picks the Linux source files but the
  Android struct layouts, and the two do not match.
- `-D__OHOS__` keeps Boost off the glibc `pthread_setcancelstate` path that
  Bionic does not implement the same way. The shims also look for this define.
- `CMAKE_CXX_SCAN_FOR_MODULES=OFF` turns off C++20 module scanning, which the
  toolchain here cannot do.
- `CMAKE_DISABLE_FIND_PACKAGE_Ceres=ON` drops Ceres, which is not needed for
  streaming and does not build here.
- The three `-include` / `-idirafter` entries are the shims from Step 2.

The encoder ends up as x264 (software). The hardware encoders are not
available under Termux, and NVENC is turned off explicitly. x264 came in with
`pkg install x264` in Step 1.

→ Next: [Step 6 - Build](#step-6---build) · [↑ Contents](#contents)

## Step 6 - Build

```
cmake --build build-server
```

→ Next: [Step 7 - Runtime setup](#step-7---runtime-setup) · [↑ Contents](#contents)

## Step 7 - Runtime setup

WiVRn registers itself as an OpenXR runtime through a JSON manifest the build
produces. Point `XR_RUNTIME_JSON` at it so clients resolve to it, and give the
compositor a writable runtime dir:

```
export XR_RUNTIME_JSON=$HOME/WiVRn/build-server/openxr_wivrn-dev.json
export XDG_RUNTIME_DIR=$PREFIX/tmp
```

Put both lines in your shell startup or a small launch script so every session
has them.

→ Next: [Step 8 - Run it](#step-8---run-it) · [↑ Contents](#contents)

## Step 8 - Run it

Start Termux:X11 and have `DISPLAY` set, then start the server. Avahi is not
running under Termux, so service publishing is off:

```
./build-server/server/wivrn-server --no-publish-service
```

On the headset, open the WiVRn client and connect to the phone by its IP
address (discovery will not find it, so type the address in). Find the phone's
IP with:

```
ip addr
```

Once the headset is connected, launch an OpenXR app against the runtime and it
renders on the phone, gets x264-encoded, and streams to the headset.

→ Next: [Troubleshooting](#troubleshooting) · [↑ Contents](#contents)

## Troubleshooting

- "incompatible protocol version": the server tag and the client app version
  do not match. Rebuild from the tag that matches your headset's client.
- Headset will not appear: type the phone's IP into the client by hand.
  `--no-publish-service` means it is not advertised on the network.
- App fails to create an OpenXR instance, asking for an Android extension or a
  JVM: the server was built as Android. Reconfigure with
  `CMAKE_SYSTEM_NAME=Linux` and `-U__ANDROID__` in a clean build tree.
- Restarting the server drops the headset. Reconnect the client before
  launching an app, not after.
- Build errors about `shm_open`, `std::ranges::enumerate_view`, or udev
  symbols: the shims are not being force-included, or libudev-zero is not
  installed.

→ [↑ Contents](#contents)

## Credits

WiVRn (https://github.com/WiVRn/WiVRn) and Monado do the real work. This is
only what it took to get the server building and running on Termux.

→ [↑ Contents](#contents)
