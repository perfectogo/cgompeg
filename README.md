# cgompeg
C Go Mpeg

## Dependencies
### **1. FFmpeg Libraries**
The `cgompeg` bindings directly interface with the FFmpeg libraries, so you must have the development versions of these libraries installed on your system. These include:

1. **`libavcodec`**: For encoding and decoding audio and video.
2. **`libavformat`**: For handling multimedia container formats (e.g., MP4, MKV).
3. **`libavutil`**: For utilities such as frame allocation and common data structures.
4. **`libswscale`**: For image scaling and pixel format conversion.
5. **`libavdevice`** *(optional)*: For capturing from devices like cameras.
6. **`libavfilter`** *(optional)*: For applying filters to audio and video streams.

- **Ubuntu/Debian:**
```
sudo apt update
sudo apt install -y libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libavdevice-dev libavfilter-dev
```

- **MacOS(arch -arm64):**
1. Ensure your terminal is configured for Apple Silicon (ARM64):

```sh
arch -arm64 /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```
2. Install FFmpeg Libraries
To install FFmpeg and its development libraries, use the following:
```sh
brew install ffmpeg
```
This will install both the FFmpeg binaries and the required development libraries (libavcodec, libavformat, libavutil, etc.).
<!-- 
### **2. `pkg-config`**

The `cgompeg` package uses `pkg-config` to locate the FFmpeg libraries. Ensure `pkg-config` is installed and correctly configured:

- **Linux**: Install `pkg-config`:
    
```bash
sudo apt install pkg-config
```

Check `pkg-config` configuration with:

```bash
pkg-config --modversion libavcodec libavformat libavutil
``` -->

### man

#### libavformat
**function** `avformat_network_init`:
```
Do global initialization of network libraries. This is optional,
and not recommended anymore.
This functions only exists to work around thread-safety issues
with older GnuTLS or OpenSSL libraries. If libavformat is linked
to newer versions of those libraries, or if you do not use them,
calling this function is unnecessary. Otherwise, you need to call
this function before any other threads using them are started.
This function will be deprecated once support for older GnuTLS and
OpenSSL libraries is removed, and this function has no purpose
anymore.
 ```

open swagger ui
```
http://localhost:8080/swagger/index.html
```
On Linux, you'll need to install the necessary development packages.
Depending on your Linux distribution, use one of these commands:
For Ubuntu/Debian:
```
sudo apt-get update
sudo apt-get install build-essential
```
```
gcc -c ccode/main.c -o ccode/main.o
```