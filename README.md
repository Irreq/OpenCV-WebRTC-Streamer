# OpenCV WebRTC Streamer

This project demonstrates how to stream an OpenCV video feed through WebRTC into a web browser.  
It also includes a minimal HTTP server that serves the web app (`static/index.html`) and accepts control commands.  
Useful for developing or monitoring OpenCV programs on headless devices (e.g. robots, embedded systems).

---

## Prerequisites

- C++17 compiler
- [CMake ≥ 3.16](https://cmake.org/)
- [OpenCV](https://opencv.org/)
- [GStreamer 1.0](https://gstreamer.freedesktop.org/) with plugins
- [nlohmann/json](https://github.com/nlohmann/json)
- [libdatachannel](https://github.com/paullouisageneau/libdatachannel)
- [cpp-httplib](https://github.com/yhirose/cpp-httplib)

Clone repository and submodules:

```bash
git clone https://github.com/Irreq/OpenCV-WebRTC-Streamer.git
cd OpenCV-WebRTC-Streamer
git submodule update --init --recursive
```

## Building

```bash
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

## Usage

```bash
./main --device /dev/video0
```

Then go to the web interface [http://127.0.0.1:9000](http://127.0.0.1:9000)

## Options

See:

```bash
./main -h
```