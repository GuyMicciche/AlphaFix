/******************************************************************************
 * AlphaFixDecoder.h — FFmpeg-based alpha channel decoder
 *
 * Reads ProRes 4444 .mov files directly and extracts the alpha channel,
 * bypassing AE's broken 8-bit alpha interpretation.
 *
 * Design: Thread-safe per-context. Each effect instance gets its own Context.
 * Uses a small LRU cache of decoded alpha frames to avoid redundant decoding
 * during AE's multi-pass rendering.
 ******************************************************************************/

#pragma once

#include <cstdint>
#include <cstddef>

// Forward-declare AE types we need
#include "AE_Effect.h"

namespace AlphaFixDecoder {

// ── Configuration ────────────────────────────────────────────────────────────

constexpr int MAX_CACHE_FRAMES = 8;     // Number of decoded frames to cache
constexpr int MAX_PATH_LENGTH  = 2048;

// ── Cached frame entry ───────────────────────────────────────────────────────

struct CachedFrame {
    int32_t     frameNumber;    // -1 if slot is empty
    uint8_t*    alphaData;      // Alpha channel buffer (width * height bytes)
    int32_t     width;
    int32_t     height;
    uint64_t    accessTime;     // For LRU eviction
};

// ── Decoder Context ──────────────────────────────────────────────────────────
// Opaque to the effect plugin. Contains all FFmpeg state.

struct Context {
    // File info
    char        filePath[MAX_PATH_LENGTH];
    double      fps;
    int32_t     fpsNum;         // Frame rate as rational (e.g. 30/1)
    int32_t     fpsDen;         // — used for PTS math to stay in sync with fps double
    int64_t     totalFrames;
    int32_t     videoWidth;
    int32_t     videoHeight;
    int32_t     videoStreamIndex;
    bool        hasAlpha;

    // FFmpeg handles (stored as void* to avoid leaking FFmpeg headers)
    void*       formatCtx;      // AVFormatContext*
    void*       codecCtx;       // AVCodecContext*
    void*       frame;          // AVFrame* (reusable decode buffer)
    void*       packet;         // AVPacket* (reusable packet buffer)

    // Frame cache (LRU)
    CachedFrame cache[MAX_CACHE_FRAMES];
    uint64_t    cacheAccessCounter;

    // Thread safety
    void*       mutex;          // Platform mutex handle

    // Debug
    bool        debugLog;       // When true, writes decode info to log file
};


// ═══════════════════════════════════════════════════════════════════════════════
// Public API
// ═══════════════════════════════════════════════════════════════════════════════

/// Create a new decoder context for the given file.
/// Returns nullptr on failure.
Context* Create(const char* filePath);

/// Destroy a decoder context and free all resources.
void Destroy(Context* ctx);

/// Decode the alpha channel for a specific frame.
/// On success, *outAlpha points to a width*height buffer of uint8_t alpha values.
/// The buffer is owned by the cache — do NOT free it directly.
/// Instead, call FreeAlphaBuffer when done.
///
/// Returns PF_Err_NONE on success.
PF_Err DecodeAlpha(
    Context*    ctx,
    int32_t     frameNumber,
    uint8_t**   outAlpha,
    int32_t*    outWidth,
    int32_t*    outHeight
);

/// Free an alpha buffer returned by DecodeAlpha.
/// (Currently a no-op since buffers are cache-owned, but call it anyway
///  for forward compatibility if we switch to copy-out semantics.)
void FreeAlphaBuffer(uint8_t* buffer);

/// Query file info without decoding any frames.
/// Returns false if the file can't be opened or has no alpha.
bool GetFileInfo(
    const char* filePath,
    int32_t*    outWidth,
    int32_t*    outHeight,
    double*     outFps,
    int64_t*    outTotalFrames,
    bool*       outHasAlpha
);


// ═══════════════════════════════════════════════════════════════════════════════
// Internal helpers (exposed for testing, not for plugin use)
// ═══════════════════════════════════════════════════════════════════════════════

namespace Internal {

    /// Find the video stream with alpha in an AVFormatContext.
    /// Returns stream index, or -1 if not found.
    int FindAlphaVideoStream(void* formatCtx);

    /// Extract alpha channel from a decoded AVFrame into a flat uint8_t buffer.
    /// Handles various pixel formats (yuva444p10le, yuva444p, etc.)
    /// Returns newly allocated buffer, or nullptr on failure.
    uint8_t* ExtractAlphaFromFrame(
        void*       avFrame,
        int32_t     width,
        int32_t     height
    );

    /// Seek to a specific frame by number using timestamp calculation.
    /// Returns true on success.
    bool SeekToFrame(Context* ctx, int32_t frameNumber);

    /// Cache management: find a cached frame or evict LRU entry.
    CachedFrame* FindOrAllocCacheSlot(Context* ctx, int32_t frameNumber);

}  // namespace Internal

}  // namespace AlphaFixDecoder