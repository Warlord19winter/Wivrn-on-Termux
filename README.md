# Wivrn on Termux

Run a WiVRn server on an Android phone under Termux and stream OpenXR to a
standalone headset over Wi-Fi, with no PC in the loop. This covers the
phone-side server build. The headset runs the normal WiVRn client app from
its own store.

Built and tested on a Snapdragon 8 Gen 3 phone (Adreno 750, Turnip Vulkan)
with a Quest 3S as the client. A Snapdragon 8 Gen 2 (Adreno 740) works the
same way.

## What this is not

This is not a prebuilt APK. WiVRn's server is meant for desktop Linux, so
getting it onto Termux means building it from source and working around the
places where Android's Bionic libc and Termux's toolchain differ from a
normal Linux box. It is involved. If you just want VR streaming and own a PC,
run the normal desktop WiVRn server instead.

## What you need

- A phone with an Adreno GPU and a working Turnip Vulkan driver (Snapdragon
  8-series). Mali and other GPUs are not covered here and probably will not
  work.
- Termux, from F-Droid or the GitHub releases. Not the Play Store build, it
  is too old.
- Termux:X11. The server brings up a display and needs an X server running.
- A headset with the WiVRn client installed (Quest, Pico, and so on), on the
  same Wi-Fi network as the phone.

## Match the client version first

WiVRn checks a protocol version on connect. If the server and the headset
client disagree, the headset is refused with "incompatible protocol version"
and nothing else will work.

So before building anything: check which WiVRn version the client app on your
headset is, and build the server from the matching git tag. My Quest client
was v26.9, so I built the v26.9 tag. Do not build `master` and hope.

## Dependencies

From Termux (native aarch64, not the glibc side):

```
pkg install git cmake ninja clang x264 vulkan-loader vulkan-headers
```

Turnip: install your phone's working native Vulkan driver package
(`mesa-vulkan-icd-freedreno`) and confirm `vulkaninfo` lists your Adreno
before going further. If Vulkan is not up, the server has nothing to encode
from.

udev: WiVRn and Monado expect libudev, which Termux does not have. Use
libudev-zero (https://github.com/illiliti/libudev-zero) built and installed
into your prefix. It provides the symbols without any real device management,
which is all that is needed here.

## Shims

Termux's Bionic libc and its libc++ are missing a few things the build
assumes. Two small headers in `shims/` cover the gap, both force-included at
configure time:

- `shm_compat.h` gives Bionic a `shm_open` / `shm_unlink` through
  `memfd_create`. Bionic has no POSIX shared memory. Its include guard has to
  let `__OHOS__` through as well (see the flags below for why that define is
  set).
- `enumerate_view.h` provides `std::ranges::enumerate_view`, which the libc++
  shipped in Termux does not carry yet.

## Build

Clone at the tag that matches your client, alongside a clean build tree:

```
git clone https://github.com/WiVRn/WiVRn
cd WiVRn
git checkout v26.9
```

Configure. The important part is forcing a Linux build even though the
compiler still thinks it is on Android:

```
cmake -S . -B build-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
  -DCMAKE_C_FLAGS="-U__ANDROID__ -D__OHOS__ -include $HOME/shims/shm_compat.h" \
  -DCMAKE_CXX_FLAGS="-U__ANDROID__ -D__OHOS__ -include $HOME/shims/shm_compat.h -include $HOME/shims/enumerate_view.h"
```

Why each piece:

- `CMAKE_SYSTEM_NAME=Linux` stops Monado building its Android backend, which
  would demand `XR_KHR_android_create_instance` and a JVM that does not exist
  here. Without this, no OpenXR client can even create an instance.
- `-U__ANDROID__` because CMake alone does not stop Termux's clang defining
  `__ANDROID__`. Left defined, Monado picks the Linux source files but the
  Android struct layouts, and the two do not match.
- `-D__OHOS__` keeps Boost off the glibc `pthread_setcancelstate` path that
  Bionic does not implement the same way.
- `CMAKE_CXX_SCAN_FOR_MODULES=OFF` turns off C++20 module scanning, which the
  toolchain here cannot do.

Turn off what will not build: Ceres is not needed for streaming, and only the
x264 encoder is wanted (hardware and other software encoders are not
available under Termux). The exact option names for these move between WiVRn
versions, so check `cmake -LAH build-linux` for the current `WIVRN_*` and
encoder flags on the tag you checked out and disable the rest there.

Build:

```
cmake --build build-linux
```

## Runtime setup

WiVRn registers itself as an OpenXR runtime through a JSON manifest. Point
`XR_RUNTIME_JSON` at the manifest the build produced so clients resolve to it:

```
export XR_RUNTIME_JSON=$HOME/WiVRn/build-linux/openxr_wivrn-dev.json
export XDG_RUNTIME_DIR=$PREFIX/tmp
```

`XDG_RUNTIME_DIR` has to be writable; `$PREFIX/tmp` works. The compositor
opens its IPC socket under there.

## Running it

Start Termux:X11 and have `DISPLAY` set, then start the server. Avahi is not
running under Termux, so service publishing is off:

```
./build-linux/server/wivrn-server --no-publish-service
```

On the headset, open the WiVRn client and connect to the phone by its IP
address (service discovery will not find it, so type the address in). The
phone's IP is on your Wi-Fi; `ip addr` shows it.

Once the headset is connected, launch an OpenXR app against the runtime and it
renders on the phone, gets x264-encoded, and streams to the headset.

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

## Credits

WiVRn (https://github.com/WiVRn/WiVRn) and Monado do the real work. This is
only what it took to get the server building and running on Termux.
