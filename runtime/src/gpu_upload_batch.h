// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
namespace SotcUploadBatch {
inline bool Enabled(bool metal_frame) {
    // Preserve other backends. Explicit zero is the regression-comparison switch.
    static const bool enabled=[] {const char* p=std::getenv("SOTC_UPLOAD_BATCH");
        return !p || std::strcmp(p,"0")!=0;}();
    return metal_frame && enabled;
}
inline bool Trace() {
    const char* p=std::getenv("SOTC_UPLOAD_BATCH_TRIAL");
    return p && std::strcmp(p,"1")==0;
}
struct Budget {
    static constexpr std::uint64_t Limit=64ull<<20;
    std::uint64_t tick{}, bytes{};
    bool CompletedUpload(std::uint64_t current_tick, std::uint64_t size) {
        if (current_tick!=tick) {tick=current_tick;bytes=0;}
        // Saturating accounting: one oversized image is still an indivisible upload.
        bytes += size > Limit-bytes ? Limit-bytes : size;
        return bytes==Limit;
    }
    void Reset() {bytes=0;}
};
}
