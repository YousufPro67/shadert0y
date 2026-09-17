#pragma once
// ffmpeg_dl.h - runtime-loaded FFmpeg for shadert0y video decoding.
//
// The plugin borrows the FFmpeg libraries already loaded inside the host
// process (Kdenlive/MLT link them) via dlopen / LoadLibrary. No FFmpeg
// development headers are needed at build time; the small struct prefixes
// below mirror the ABI-stable beginnings of the real FFmpeg structs for
// the supported generations:
//
//   FFmpeg 7.1 : libavformat 61, libavcodec 61, libavutil 59, libswscale 8
//   FFmpeg 7.0 : libavformat 60, libavcodec 60, libavutil 59, libswscale 7
//   FFmpeg 6.x : libavformat 60, libavcodec 60, libavutil 58, libswscale 7
//   FFmpeg 5.x : libavformat 59, libavcodec 59, libavutil 57, libswscale 6
//   FFmpeg 4.4 : libavformat 58, libavcodec 58, libavutil 56, libswscale 5
//
// Only fields from those stable prefixes are read; everything else is kept
// opaque. FFmpeg 8+ reworked the AVFrame layout and is deliberately not
// loaded. Streams are cross-checked at runtime (st->index == array index,
// sane time bases) so a layout mismatch degrades to "no video" instead of
// undefined behavior.

#include <cstdint>
#include <cstddef>

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#else
  #include <dlfcn.h>
#endif

namespace ffdl {

static const int64_t kNoPTS = (int64_t)0x8000000000000000ULL;
enum { kAVSeekBackward = 1 };            // AVSEEK_FLAG_BACKWARD
enum { kSWSBilinear = 2 };               // SWS_BILINEAR
enum { kAVMediaVideo = 0 };              // AVMEDIA_TYPE_VIDEO
enum { kAVPixFmtRGBA = 26 };             // AV_PIX_FMT_RGBA (ABI-stable enum value)
enum { kFrameDataDisplayMatrix = 6 };    // AV_FRAME_DATA_DISPLAYMATRIX
static const int kAVErrorEOF = -541478725; // AVERROR_EOF
static const int kAVErrorAgain = -11;      // AVERROR(EAGAIN) (Linux and MinGW)

struct Ratio { int32_t num, den; };      // AVRational

// AVFormatContext prefix (identical across libavformat 58-61; after
// `streams`, libavformat 61 adds nb_stream_groups and removed filename[],
// so nothing past streams is read - duration comes from the AVStream).
struct FmtCtx {
    const void* av_class;       // 0
    const void* iformat;        // 8
    void* oformat;              // 16
    void* priv_data;            // 24
    void* pb;                   // 32
    int32_t ctx_flags;          // 40
    uint32_t nb_streams;        // 44
    void* const* streams;       // 48  (AVStream**)
};

// AVStream prefix (identical across libavformat 58-61)
struct Stream {
    const void* av_class;       // 0
    int32_t index;              // 8
    int32_t id;                 // 12
    void* codecpar;             // 16  (AVCodecParameters*)
    void* priv_data;            // 24
    Ratio time_base;            // 32
    int64_t start_time;         // 40
    int64_t duration;           // 48  (in time_base units)
};

// AVCodecParameters (first two fields stable since 2016)
struct CodecPar {
    int32_t codec_type;         // 0
    int32_t codec_id;           // 4
};

// AVPacket prefix (identical across libavutil 56-59)
struct Packet {
    void* buf;                  // 0
    int64_t pts;                // 8
    int64_t dts;                // 16
    uint8_t* data;              // 24
    int32_t size;               // 32
    int32_t stream_index;       // 36
    int32_t flags;              // 40
};

// AVFrame prefix (identical across libavutil 56-59; changed in 60 / FFmpeg 8)
struct Frame {
    uint8_t* data[8];           // 0
    int32_t linesize[8];        // 64
    uint8_t** extended_data;    // 96
    int32_t width, height;      // 104, 108
    int32_t nb_samples;         // 112
    int32_t format;             // 116  (AVPixelFormat)
    int32_t key_frame;          // 120  (not read)
    int32_t pict_type;          // 124
    Ratio sample_aspect_ratio;  // 128
    int64_t pts;                // 136
    int64_t pkt_dts;            // 144
};

// AVFrameSideData (av_class first since libavutil 55.45)
struct FrameSideData {
    const void* av_class;       // 0
    uint8_t* data;              // 8  (int32_t[9] display matrix when type == 6)
};

// Layout checks: expected offsets depend on pointer size (8 bytes on
// 64-bit, 4 on 32-bit). Members are always read by name, so the structs
// are correct on both; these asserts only guard against field mistakes.
#if UINTPTR_MAX == 0xFFFFFFFFu
// 32-bit (e.g. MinGW-w64 i686): int64_t stays 8-aligned, as in the FFmpeg DLLs.
static_assert(alignof(int64_t) == 8, "32-bit ABI with 4-aligned int64: update offsets below");
static_assert(offsetof(FmtCtx, streams) == 28,    "AVFormatContext layout mismatch");
static_assert(offsetof(Stream, time_base) == 20,  "AVStream layout mismatch");
static_assert(offsetof(Stream, duration) == 40,   "AVStream layout mismatch");
static_assert(offsetof(CodecPar, codec_id) == 4,  "AVCodecParameters layout mismatch");
static_assert(offsetof(Packet, stream_index) == 32, "AVPacket layout mismatch");
static_assert(offsetof(Frame, width) == 68,       "AVFrame layout mismatch");
static_assert(offsetof(Frame, pts) == 104,        "AVFrame layout mismatch");
static_assert(offsetof(FrameSideData, data) == 4, "AVFrameSideData layout mismatch");
#else
static_assert(offsetof(FmtCtx, streams) == 48,    "AVFormatContext layout mismatch");
static_assert(offsetof(Stream, time_base) == 32,  "AVStream layout mismatch");
static_assert(offsetof(Stream, duration) == 48,   "AVStream layout mismatch");
static_assert(offsetof(CodecPar, codec_id) == 4,  "AVCodecParameters layout mismatch");
static_assert(offsetof(Packet, stream_index) == 36, "AVPacket layout mismatch");
static_assert(offsetof(Frame, width) == 104,      "AVFrame layout mismatch");
static_assert(offsetof(Frame, pts) == 136,        "AVFrame layout mismatch");
static_assert(offsetof(FrameSideData, data) == 8, "AVFrameSideData layout mismatch");
#endif

struct Api {
    bool ok = false;
    void* hFmt = nullptr;
    void* hCod = nullptr;
    void* hUtil = nullptr;
    void* hSws = nullptr;
    const char* nameCod = "";

    // libavformat
    int    (*avformat_open_input)(FmtCtx**, const char*, const void*, void**) = nullptr;
    int    (*avformat_find_stream_info)(FmtCtx*, void**) = nullptr;
    void   (*avformat_close_input)(FmtCtx**) = nullptr;
    int    (*av_read_frame)(FmtCtx*, void*) = nullptr;
    int    (*avformat_seek_file)(FmtCtx*, int, int64_t, int64_t, int64_t, int) = nullptr;

    // libavcodec
    const void* (*avcodec_find_decoder)(int) = nullptr;
    void*       (*avcodec_alloc_context3)(const void*) = nullptr;
    int         (*avcodec_parameters_to_context)(void*, const CodecPar*) = nullptr;
    int         (*avcodec_open2)(void*, const void*, void**) = nullptr;
    int         (*avcodec_send_packet)(void*, const void*) = nullptr;
    int         (*avcodec_receive_frame)(void*, Frame*) = nullptr;
    void        (*avcodec_flush_buffers)(void*) = nullptr;
    void        (*avcodec_free_context)(void**) = nullptr;
    void*       (*av_packet_alloc)(void) = nullptr;
    void        (*av_packet_free)(void**) = nullptr;
    void        (*av_packet_unref)(void*) = nullptr;

    // libavutil
    Frame*        (*av_frame_alloc)(void) = nullptr;
    void          (*av_frame_free)(Frame**) = nullptr;
    void          (*av_frame_unref)(Frame*) = nullptr;
    void          (*av_frame_move_ref)(Frame*, Frame*) = nullptr;
    FrameSideData* (*av_frame_get_side_data)(const Frame*, int) = nullptr;
    // Real signature: double av_display_rotation_get(const int32_t matrix[9])
    double        (*av_display_rotation_get)(const void*) = nullptr;
    int           (*av_strerror)(int, char*, size_t) = nullptr;

    // libswscale
    void* (*sws_getContext)(int, int, int, int, int, int, int, void*, void*, const double*) = nullptr;
    int   (*sws_scale)(void*, const uint8_t* const*, const int*, int, int, uint8_t* const*, const int*) = nullptr;
    void  (*sws_freeContext)(void*) = nullptr;
};

inline bool symOk(void* lib, const char* name, void** out) {
#if defined(_WIN32)
    *out = (void*)(uintptr_t)GetProcAddress((HMODULE)lib, name);
#else
    *out = dlsym(lib, name);
#endif
    return *out != nullptr;
}

// Loads a consistent set of avformat/avcodec/avutil/swscale from the same
// FFmpeg generation. Libraries already loaded by the host are reused via
// RTLD_NOLOAD so the plugin and the host share one copy.
inline const Api* api() {
    static Api* cached = []() -> Api* {
        struct Set { const char* fmt; const char* cod; const char* util; const char* sws; };
#if defined(_WIN32)
        const Set sets[] = {
            {"avformat-61.dll", "avcodec-61.dll", "avutil-59.dll", "swscale-8.dll"},
            {"avformat-60.dll", "avcodec-60.dll", "avutil-59.dll", "swscale-7.dll"},
            {"avformat-60.dll", "avcodec-60.dll", "avutil-58.dll", "swscale-7.dll"},
            {"avformat-59.dll", "avcodec-59.dll", "avutil-57.dll", "swscale-6.dll"},
            {"avformat-58.dll", "avcodec-58.dll", "avutil-56.dll", "swscale-5.dll"},
        };
        auto openLib = [](const char* n) -> void* {
            return (void*)(uintptr_t)LoadLibraryA(n);
        };
#elif defined(__APPLE__)
        const Set sets[] = {
            {"libavformat.61.dylib", "libavcodec.61.dylib", "libavutil.59.dylib", "libswscale.8.dylib"},
            {"libavformat.60.dylib", "libavcodec.60.dylib", "libavutil.59.dylib", "libswscale.7.dylib"},
            {"libavformat.60.dylib", "libavcodec.60.dylib", "libavutil.58.dylib", "libswscale.7.dylib"},
            {"libavformat.59.dylib", "libavcodec.59.dylib", "libavutil.57.dylib", "libswscale.6.dylib"},
            {"libavformat.58.dylib", "libavcodec.58.dylib", "libavutil.56.dylib", "libswscale.5.dylib"},
        };
        auto openLib = [](const char* n) -> void* {
            return dlopen(n, RTLD_NOW | RTLD_LOCAL);
        };
#else
        const Set sets[] = {
            {"libavformat.so.61", "libavcodec.so.61", "libavutil.so.59", "libswscale.so.8"},
            {"libavformat.so.60", "libavcodec.so.60", "libavutil.so.59", "libswscale.so.7"},
            {"libavformat.so.60", "libavcodec.so.60", "libavutil.so.58", "libswscale.so.7"},
            {"libavformat.so.59", "libavcodec.so.59", "libavutil.so.57", "libswscale.so.6"},
            {"libavformat.so.58", "libavcodec.so.58", "libavutil.so.56", "libswscale.so.5"},
        };
        auto openLib = [](const char* n) -> void* {
#ifdef RTLD_NOLOAD
            // Prefer a copy the host (MLT/Kdenlive) already loaded.
            void* h = dlopen(n, RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD);
            if (h) return h;
#endif
            return dlopen(n, RTLD_NOW | RTLD_LOCAL);
        };
#endif
        for (const Set& s : sets) {
            void* hF = openLib(s.fmt);  if (!hF) continue;
            void* hC = openLib(s.cod);  if (!hC) continue;
            void* hU = openLib(s.util); if (!hU) continue;
            void* hS = openLib(s.sws);  if (!hS) continue;

            Api* a = new Api();
            a->hFmt = hF; a->hCod = hC; a->hUtil = hU; a->hSws = hS;
            a->nameCod = s.cod;
            bool ok = true;
            #define S0(lib, name) ok = ok && symOk(lib, #name, (void**)&a->name)
            S0(hU, av_frame_alloc);          S0(hU, av_frame_free);
            S0(hU, av_frame_unref);          S0(hU, av_frame_move_ref);
            S0(hU, av_frame_get_side_data);
            S0(hU, av_display_rotation_get); S0(hU, av_strerror);
            S0(hC, avcodec_find_decoder);    S0(hC, avcodec_alloc_context3);
            S0(hC, avcodec_parameters_to_context); S0(hC, avcodec_open2);
            S0(hC, avcodec_send_packet);     S0(hC, avcodec_receive_frame);
            S0(hC, avcodec_flush_buffers);   S0(hC, avcodec_free_context);
            S0(hC, av_packet_alloc);         S0(hC, av_packet_free);
            S0(hC, av_packet_unref);
            S0(hF, avformat_open_input);     S0(hF, avformat_find_stream_info);
            S0(hF, avformat_close_input);    S0(hF, av_read_frame);
            S0(hF, avformat_seek_file);
            S0(hS, sws_getContext);          S0(hS, sws_scale);
            S0(hS, sws_freeContext);
            #undef S0
            if (ok) { a->ok = true; return a; }
            delete a;
        }
        return nullptr;
    }();
    return cached;
}

} // namespace ffdl
