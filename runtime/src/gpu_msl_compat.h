/* Original SotC compatibility policy around bloodborne-native's GPL-2.0-or-later
 * PerVertexKHR rewriter (bbport/shadPS4, Wooloomooloo2 and Huax-1839).
 * Reuses the native Metal compiler's rewriter before MoltenVK conversion.
 */
#pragma once
#include <utility>
#include "video_core/renderer_metal/mtl_spirv_pervertex.h"

namespace Sotc {
inline std::vector<u32> MslCompatibleSpirv(std::span<const u32> code,
                                           bool first_vertex_provoking) {
    if (first_vertex_provoking && Metal::HasPerVertexInputs(code)) {
        auto rewritten = Metal::RewritePerVertexInputs(code);
        // Only the exact first-vertex case is admitted here. Its Flat input
        // preserves vertex zero and its bits when provoking vertex is fixed to
        // the first vertex. Configurable/last-vertex pipelines are not admitted.
        // Other selections retain their original module and unsupported error;
        // do not use the
        // rewriter's derivative reconstruction or its degenerate fallback.
        if (rewritten.ok && rewritten.interpolated_inputs == 0) {
            return std::move(rewritten.spirv);
        }
    }
    return {code.begin(), code.end()};
}
} // namespace Sotc
