# QtMeet — Development environment setup

How to install dependencies and build so **1:1 calls and screen share** work on one PC (2 clients + 1 server).

---

## 1. What you need

| Piece | Env var | Required for |
|---|---|---|
| Qt 6.8+ (Multimedia, Quick, Network, SQL, Widgets) | Qt kit in Creator | UI + networking |
| MinGW 13+ or MSVC 2022 / GCC 11+ | — | compile |
| **protobuf** (+ `protoc`) | `PROTOBUF_DIR` | control protocol |
| **FFmpeg** (LGPL, with `avdevice`) | `FFMPEG_DIR` | camera, mic, screen, encode/decode |
| **libdatachannel** | `DATACHANNEL_DIR` | **WebRTC media** (calls / screen share) |
| Opus | `OPUS_DIR` (optional on Linux: pkg-config) | audio codec prefer |
| webrtc-audio-processing 1.x | `APM_DIR` (optional on Linux: pkg-config) | AEC / NS |

Without `DATACHANNEL_DIR`, the app builds but logs:

```text
RtcPeer built without libdatachannel (set DATACHANNEL_DIR)
```

and remote call / screen-share video will not work.

Each `*_DIR` must be an **install prefix** that contains:

```text
PREFIX/
  include/     # headers
  lib/         # .lib / .a / import libs
  bin/         # .dll / .so / tools (protoc, ffmpeg DLLs…)
```

---

## 2. Choose app mode (multi-instance)

Edit **`config/common.pri`**:

```qmake
QM_APP_MODE = development   # allow 2 clients on 1 PC
# QM_APP_MODE = production  # single-instance only
```

Or run without rebuild:

```bat
QtMeetClient.exe --dev
QtMeetServer.exe --dev
```

---

## 3. Windows (MinGW) — recommended path for this project

Defaults already in `.pri` if you use the same folders:

| Var | Default in repo (if env empty) |
|---|---|
| `FFMPEG_DIR` | `D:/Apps/FFmpeg_9_Mingw/install_mingw` |
| `PROTOBUF_DIR` | `D:/Apps/protobuf_21_mingw` |

Change those paths or set env vars to match your machine.

### 3.1 Qt

1. Install [Qt Online Installer](https://www.qt.io/download).
2. Select **Qt 6.8+** with **MinGW 64-bit**, modules: Qt Multimedia, Qt Quick, Qt Quick Controls, Qt Network, Qt SQL, Qt Widgets.
3. In Qt Creator: open `QtMeet.pro`, pick the MinGW kit.

### 3.2 FFmpeg — build DLL + import libs (MinGW)

Need a **shared** build with **libavdevice** (dshow / gdigrab).

```bat
set PATH=C:\Qt\Tools\mingw1310_64\bin;%PATH%
git clone https://git.ffmpeg.org/ffmpeg.git ffmpeg && cd ffmpeg

REM MSYS2 or Git Bash recommended for ./configure
./configure --prefix=/d/Apps/FFmpeg_9_Mingw/install_mingw ^
  --enable-shared --disable-static ^
  --enable-avdevice --enable-dxva2 --enable-d3d11va ^
  --disable-doc --disable-debug
mingw32-make -j%NUMBER_OF_PROCESSORS%
mingw32-make install
```

Result:

```text
install_mingw\
  include\libavcodec\...
  lib\avcodec.dll.a   (and friends)   ← link libs
  bin\avcodec-*.dll                   ← runtime DLLs
```

```bat
set FFMPEG_DIR=D:\Apps\FFmpeg_9_Mingw\install_mingw
```

QtMeet post-link copies `bin\*.dll` next to the exe when `FFMPEG_DIR` is set.

**Easier:** download a MinGW shared FFmpeg build that already has `include/`, `lib/`, `bin/` and only set `FFMPEG_DIR`.

### 3.3 protobuf

```text
D:\Apps\protobuf_21_mingw\
  bin\protoc.exe
  include\google\protobuf\...
  lib\libprotobuf.a   (or .lib)
```

```bat
set PROTOBUF_DIR=D:\Apps\protobuf_21_mingw
```

Protobuf ≥ 22 also needs Abseil; either use 21.x or set `PROTOBUF_LIBS` to the full link line.

### 3.4 libdatachannel (required for calls)

WebRTC **must** use real TLS/DTLS. You cannot replace it with a homemade “easy crypt”.
You **can skip OpenSSL** and use **GnuTLS** instead (free, from MSYS2 — recommended for now).

QtMeet links `-ldatachannel`. After install you need:

| File | Role |
|---|---|
| `bin\datachannel.dll` (or under `bin` of prefix) | runtime |
| `lib\libdatachannel.dll.a` | link import lib (MinGW) |
| `include\rtc\...` | headers |

---

#### Path A — skip OpenSSL (recommended now): GnuTLS via MSYS2

Use the **MSYS2 UCRT64** terminal only (prompt ends with `UCRT64 ~`, not `C:\Users\...>`).

```bash
# 1) Tools + GnuTLS (no OpenSSL install needed)
pacman -S --needed \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-gnutls \
  git

# 2) Source (submodules required)
cd /d/libdatachannel          # = D:\libdatachannel
git submodule update --init --recursive
rm -rf build

# 3) Build with GnuTLS (USE_GNUTLS=1 → skip OpenSSL)
cmake -G "MinGW Makefiles" -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/c/libs/libdatachannel \
  -DBUILD_SHARED_LIBS=ON \
  -DUSE_GNUTLS=1 \
  -DUSE_NICE=0 \
  -DNO_WEBSOCKET=ON

cmake --build build -j$(nproc)
cmake --install build
```

Then in Windows / Qt Creator:

```bat
set DATACHANNEL_DIR=C:\libs\libdatachannel
```

Copy runtime DLLs next to `QtMeetClient.exe` / `QtMeetServer.exe`:

```bat
copy C:\libs\libdatachannel\bin\*.dll  <your>\build\bin\
copy C:\msys64\ucrt64\bin\libgnutls*.dll C:\msys64\ucrt64\bin\libhogweed*.dll C:\msys64\ucrt64\bin\libnettle*.dll C:\msys64\ucrt64\bin\libgmp*.dll <your>\build\bin\
```

(Exact DLL names: `dir C:\msys64\ucrt64\bin\libgnutls*`)

---

#### Path B — OpenSSL later (optional)

When you want OpenSSL instead of GnuTLS:

1. Install free OpenSSL **or** `pacman -S mingw-w64-ucrt-x86_64-openssl`
2. Rebuild libdatachannel with:

```bash
cmake -G "MinGW Makefiles" -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/c/libs/libdatachannel \
  -DBUILD_SHARED_LIBS=ON \
  -DUSE_GNUTLS=0 \
  -DOPENSSL_ROOT_DIR=/ucrt64
```

Keep Path A working until Path B is ready; QtMeet does not care which TLS backend libdatachannel was built with.

### 3.5 Opus (optional but recommended)

```bat
set OPUS_DIR=C:\libs\opus
```

If unset, FFmpeg’s Opus may still be used for some paths; native Opus helps the RTC encoder.

### 3.6 webrtc-audio-processing (optional)

```bat
set APM_DIR=C:\libs\webrtc-audio-processing
```

If unset on Windows you get a qmake warning; calls still run without AEC.

### 3.7 Qt Creator — Build Environment

**Projects → Build → Build Environment → Add:**

| Name | Example value |
|---|---|
| `FFMPEG_DIR` | `D:\Apps\FFmpeg_9_Mingw\install_mingw` |
| `PROTOBUF_DIR` | `D:\Apps\protobuf_21_mingw` |
| `DATACHANNEL_DIR` | `C:\libs\libdatachannel` |
| `OPUS_DIR` | `C:\libs\opus` |
| `APM_DIR` | `C:\libs\webrtc-audio-processing` |

Then **Run qmake** and **Rebuild All**.

### 3.8 Verify

Start client; log should **not** contain:

```text
RtcPeer built without libdatachannel
```

You should see something like:

```text
QtMeetClient mode: development (multi-instance)
```

---

## 4. Linux

```bash
# Qt via distro or online installer
sudo apt install build-essential qt6-base-dev qt6-declarative-dev \
  qt6-multimedia-dev libprotobuf-dev protobuf-compiler \
  libavformat-dev libavcodec-dev libavdevice-dev libavfilter-dev \
  libswscale-dev libswresample-dev libavutil-dev \
  libopus-dev libwebrtc-audio-processing-1-dev

# libdatachannel — build & install, then:
export DATACHANNEL_DIR=/usr/local   # or your prefix
# FFmpeg/Opus/APM can use pkg-config if *_DIR unset
```

```bash
cd qt-meet
mkdir -p build && cd build
qmake ../QtMeet.pro
make -j$(nproc)
```

Binaries: `build/bin/QtMeetServer`, `build/bin/QtMeetClient`.

---

## 5. Build (any OS)

```bat
cd qt-meet
mkdir build
cd build
qmake ..\QtMeet.pro
mingw32-make
```

Outputs:

- `build/bin/QtMeetServer.exe` (or `QtMeetServer`)
- `build/bin/QtMeetClient.exe` (or `QtMeetClient`)

TLS certs are copied next to the server from `server/certs/`.

---

## 6. Run a 1-PC test (2 clients)

1. Ensure `QM_APP_MODE = development` in `config/common.pri` (or use `--dev`).
2. Start **one** server → Library → pick hosted folder → Service → Start.
3. Start **client A** → register/login.
4. Start **client B** → register/login.
5. Call A ↔ B; try **Share screen**.

Ports used:

| Port | Role |
|---|---|
| TCP 7400 | control (TLS + protobuf) |
| TCP 7401 | HTTP media |
| UDP 40000–40100 | WebRTC |

---

## 7. Common problems

| Symptom | Fix |
|---|---|
| `RtcPeer built without libdatachannel` | Set `DATACHANNEL_DIR`, re-run qmake, rebuild `qmrtc` + apps |
| Missing `avcodec-*.dll` | Set `FFMPEG_DIR`; DLLs should copy to `bin/`, or copy manually |
| `protoc` not found | Set `PROTOBUF_DIR` so `bin/protoc` exists |
| Second client exits immediately | Use `development` mode or `--dev` |
| Opus / h264_mf warnings | Encoder/path issues; WebRTC still needs libdatachannel first |
| Screen share UI OK, remote black | Almost always missing libdatachannel |

---

## 8. Quick checklist

- [ ] Qt 6 MinGW kit selected  
- [ ] `FFMPEG_DIR` points at FFmpeg with `include/` + `lib/` + `bin/`  
- [ ] `PROTOBUF_DIR` has `bin/protoc`  
- [ ] `DATACHANNEL_DIR` set and DLLs next to exe  
- [ ] `QM_APP_MODE = development` for two clients  
- [ ] qmake + full rebuild  
- [ ] Log has no “without libdatachannel”  
