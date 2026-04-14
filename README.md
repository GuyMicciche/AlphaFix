# AlphaFix — After Effects ProRes 4444 Alpha Repair Plugin

## The Problem

When encoding ProRes 4444 with `-alpha_bits 8` via FFmpeg, Adobe After Effects (and Premiere Pro) incorrectly interprets the 8-bit alpha channel, producing black fringing and edge artifacts on transparent areas. Other applications (Nuke, DaVinci Resolve, etc.) import the same files correctly.

This is an Adobe-side decoder bug. The "Interpret Footage > Alpha" settings do not fix it.

## The Solution

AlphaFix is a SmartFX effect plugin that:

1. Reads the original `.mov` file directly using FFmpeg's libraries
2. Decodes the correct alpha channel per-frame
3. Replaces AE's broken alpha with the correct one
4. Handles un-premultiplication and re-premultiplication to fix RGB edge colors

## Usage

1. Import your ProRes 4444 (`-alpha_bits 8`) footage into After Effects as usual
2. Apply **Effect > Channel > AlphaFix** to the layer
3. The plugin automatically detects the layer's source file and repairs the alpha

### Parameters

| Parameter | Description |
|-----------|-------------|
| **Enable** | Toggle the fix on/off for A/B comparison |
| **Alpha Only** | Debug view — displays the corrected alpha as grayscale |
| **Alpha Mode** | `Replace` (default), `Multiply`, or `Maximum` |
| **Frame Offset** | Adjust sync if frames don't align (rare edge case) |
| **Debug Log** | Writes per-frame decode info to a log file for troubleshooting |

### Alpha Modes

- **Replace** — Completely replaces AE's broken alpha with FFmpeg's correctly decoded alpha. This is the right choice in almost every situation.
- **Multiply** — Multiplies both alpha channels together. Useful if you've applied masks or effects that modify the alpha and want to preserve those on top of the fix.
- **Maximum** — Takes the greater of both alpha values per pixel. A niche option for specific compositing scenarios.

---

## Building

### Prerequisites

Before configuring the build, you need to extract two dependency archives into the project root:

**After Effects SDK**

Download the AE SDK from [Adobe Developer](https://developer.adobe.com/after-effects/) and place the archive at:

```
AfterEffectsSDK\AfterEffectsSDK.zip
```

Extract it in place so the result looks like:

```
AfterEffectsSDK\
  Headers\
  Resources\
  Util\
  ...
```

**FFmpeg Shared Build (Windows)**

Download `ffmpeg-8.1-full_build-shared.7z` from [gyan.dev FFmpeg builds](https://www.gyan.dev/ffmpeg/builds/) and place the archive at:

```
ffmpeg-shared\ffmpeg-8.1-full_build-shared.7z
```

Extract it in place so the result looks like:

```
ffmpeg-shared\
  bin\
  include\
  lib\
```

You need at minimum: `avformat`, `avcodec`, `avutil`, `swscale`.

**Other requirements:**

- CMake 3.20+
- Visual Studio 2022 (Windows) or Xcode (macOS)

---

### Windows Build

Once both dependencies are extracted, run `buildwin.bat` from the project root, or build manually:

```bat
set AE_SDK=AfterEffectsSDK
set FFMPEG=ffmpeg-shared

cmake -B build -S . -G "Visual Studio 17 2022" -A x64 ^
    -DAE_SDK_PATH="%AE_SDK%" ^
    -DFFMPEG_PATH="%FFMPEG%"

cmake --build build --config Release

cmake --install build --config Release
```

### macOS Build

```bash
export AE_SDK=/path/to/AfterEffectsSDK
export FFMPEG=/usr/local/opt/ffmpeg

cmake -B build -S . -G Xcode \
    -DAE_SDK_PATH="$AE_SDK" \
    -DFFMPEG_PATH="$FFMPEG"

cmake --build build --config Release
cmake --install build --config Release
```

### PiPL Resource Note (Windows)

The PiPL resource (`AlphaFix_PiPL.r`) is compiled automatically by the CMake build using `PiPLtool.exe` from the AE SDK. In some cases PiPLtool may omit commas in the generated `.rc` file — if After Effects shows a PiPL error after installing the plugin, open `build/AlphaFix_PiPL.rc` and add a comma after `"4L"` on lines containing `eVER`, `eINF`, and `aeFL` properties, then rebuild.

---

## Architecture

```
AlphaFix/
├── CMakeLists.txt          # Build configuration
├── buildwin.bat            # Windows convenience build script
├── README.md               # This file
└── src/
    ├── AlphaFix.h          # Plugin header, parameter definitions
    ├── AlphaFix.cpp        # Main plugin entry point and SmartFX render
    ├── AlphaFix_PiPL.r     # PiPL resource for AE plugin registration
    ├── AlphaFix.def        # DLL export definitions (Windows)
    ├── AlphaFixDecoder.h   # FFmpeg decoder interface
    └── AlphaFixDecoder.cpp # FFmpeg decoder implementation
```

### How It Works

1. **On apply** — The plugin initializes and reads the layer's source file path via AEGP suites.
2. **On render:**
   - Opens the source `.mov` with FFmpeg's `avformat`
   - Seeks to the current frame using PTS-based rational math (frame-accurate for all-intra ProRes)
   - Decodes the frame with `avcodec`
   - Extracts the alpha channel from the decoded frame, handling `yuva444p10le`, `yuva444p`, `rgba`, and other pixel formats
   - Caches decoded alpha frames in an 8-frame LRU cache to avoid redundant decoding during AE's multi-pass rendering
   - Replaces AE's mangled alpha with the correct one
   - Un-premultiplies RGB using the old (broken) alpha and re-premultiplies using the corrected alpha

### Thread Safety

- Each effect instance maintains its own FFmpeg decoder context
- Decode and cache operations are mutex-protected
- `PF_OutFlag2_SUPPORTS_THREADED_RENDERING` is enabled

---

## Known Limitations

- Requires the original `.mov` file to remain accessible at its original path. If the file is moved or renamed, the plugin falls back to passthrough.
- Frame-accurate seeking depends on the source file having proper keyframe structure. ProRes is all-intra, so this is not an issue in practice.
- The first render after applying the effect may be slightly slower while the decoder initializes.

---

## License

MIT

## Author

Guy Micciche