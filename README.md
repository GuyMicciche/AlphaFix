# AlphaFix — After Effects ProRes 4444 Alpha Repair Plugin

## The Problem

When encoding ProRes 4444 with `-alpha_bits 8` via FFmpeg, Adobe After Effects (and Premiere Pro) incorrectly interprets the 8-bit alpha channel, creating black fringing and edge artifacts on transparent areas. Other applications (Nuke, DaVinci Resolve, etc.) import the same files correctly.

This is an Adobe-side decoder bug. The "Interpret Footage > Alpha" settings do not fix it.

## The Solution

AlphaFix is a SmartFX effect plugin that:

1. Reads the original `.mov` file directly using FFmpeg's libraries
2. Decodes the correct alpha channel per-frame
3. Replaces AE's broken alpha with the correct one
4. Handles un-premultiplication and re-premultiplication to fix RGB edge colors

## Usage

1. Import your ProRes 4444 (alpha_bits 8) footage into AE as usual
2. Apply **Effect > GeoSniper > AlphaFix** to the layer
3. The plugin automatically detects the layer's source file and fixes the alpha

### Parameters

| Parameter | Description |
|-----------|-------------|
| **Enable** | Toggle the fix on/off for comparison |
| **Alpha Only** | Debug view — shows the corrected alpha as grayscale |
| **Alpha Mode** | `Replace` (default), `Multiply`, or `Maximum` |
| **Frame Offset** | Adjust sync if frames don't align (rare edge case) |

### Alpha Modes

- **Replace**: Completely replaces AE's broken alpha with FFmpeg's correct decode. This is what you want 99% of the time.
- **Multiply**: Multiplies both alphas together. Useful if you've applied masks or effects that modify alpha and want to preserve those changes.
- **Maximum**: Takes the greater of both alpha values per-pixel. Niche use case.

## Building

### Prerequisites

- **After Effects SDK** — Download from [Adobe Developer](https://developer.adobe.com/after-effects/)
- **FFmpeg Shared Build** — Download from [FFmpeg builds](https://www.gyan.dev/ffmpeg/builds/) (Windows) or build from source
  - You need the `include/` headers and `lib/` import libraries
  - At minimum: `avformat`, `avcodec`, `avutil`, `swscale`
- **CMake 3.20+**
- **Visual Studio 2022** (Windows) or **Xcode** (macOS)

### Windows Build

```bat
REM Set paths
set AE_SDK=C:\path\to\AfterEffectsSDK
set FFMPEG=C:\path\to\ffmpeg-shared

REM Configure
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 ^
    -DAE_SDK_PATH="%AE_SDK%" ^
    -DFFMPEG_PATH="%FFMPEG%"

REM Build
cmake --build build --config Release

REM Install (copies to AE plugins folder)
cmake --install build --config Release
```

### macOS Build

```bash
# Set paths
export AE_SDK=/path/to/AfterEffectsSDK
export FFMPEG=/usr/local/opt/ffmpeg  # or wherever FFmpeg is installed

# Configure
cmake -B build -S . -G Xcode \
    -DAE_SDK_PATH="$AE_SDK" \
    -DFFMPEG_PATH="$FFMPEG"

# Build
cmake --build build --config Release

# Install
cmake --install build --config Release
```

### PiPL Resource (Windows)

The PiPL resource (`AlphaFix.r`) needs to be compiled with Adobe's resource compiler (`PiPLtool.exe`) found in the AE SDK. If using the CMake build, you may need to run PiPLtool manually or add a custom build step. See the AE SDK documentation for details.

Alternatively, use the Visual Studio solution template from the AE SDK examples and integrate the AlphaFix sources into that project structure.

## Architecture

```
AlphaFix/
├── CMakeLists.txt          # Build configuration
├── README.md               # This file
└── src/
    ├── AlphaFix.h           # Plugin header, param definitions
    ├── AlphaFix.cpp         # Main plugin: entry point, SmartFX render
    ├── AlphaFix.r           # PiPL resource for AE registration
    ├── AlphaFixDecoder.h    # FFmpeg decoder interface
    └── AlphaFixDecoder.cpp  # FFmpeg decoder implementation
```

### How It Works

1. **On apply**: Plugin initializes, reads the layer's source file path via AEGP suites
2. **On render**: 
   - Opens the source `.mov` with FFmpeg's `avformat`
   - Seeks to the current frame using PTS-based calculation
   - Decodes the frame with `avcodec` (ProRes decoder)
   - Extracts the alpha channel from the decoded frame (handles `yuva444p10le`, `yuva444p`, `rgba`, etc.)
   - Caches decoded alpha frames in an LRU cache (8 frames)
   - Replaces AE's mangled alpha with the correct one
   - Un-premultiplies RGB with old (broken) alpha and re-premultiplies with correct alpha

### Thread Safety

- Each effect instance gets its own FFmpeg decoder context
- Mutex-protected decode/cache operations
- `PF_OutFlag2_SUPPORTS_THREADED_RENDERING` enabled

## Known Limitations

- Requires the original `.mov` file to be accessible at its original path
- If the source file is moved/renamed, the plugin falls back to passthrough
- Frame-accurate seeking depends on the source file having proper keyframe structure (ProRes is all-intra, so this should always work)
- First render after applying may be slightly slower as the decoder initializes

## License

MIT

## Author

Guy Micciche — [GeoSniper Tools](https://github.com/GuyMicciche)
