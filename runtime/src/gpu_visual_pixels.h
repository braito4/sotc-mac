// Original CPU-only analysis of explicitly captured game-frame pixels.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace SotcVisual {
inline constexpr std::uint64_t MaxReadbackBytes = 16ull << 20;
struct Pixels {
    unsigned width{}, height{};
    std::vector<std::uint8_t> rgb;
    std::array<double, 32> tile_luma{};
    std::uint64_t hash{14695981039346656037ull};
    double mean_luma{}, luma_deviation{}, black_fraction{}, mean_change{}, changed_fraction{};
    bool compared{}, identical{}, near_black{};
};

inline Pixels AnalyzePixels(std::span<const std::uint8_t> rgba, unsigned width,
                            unsigned height, bool bgra, const Pixels* previous = nullptr) {
    Pixels out;
    const std::uint64_t source_pixels = std::uint64_t(width) * height;
    if (!width || !height || source_pixels > MaxReadbackBytes / 4 || source_pixels * 4 > rgba.size()) return out;
    const double scale = std::min({1.0, 320.0 / width, 180.0 / height});
    out.width = std::max(1u, unsigned(width * scale));
    out.height = std::max(1u, unsigned(height * scale));
    out.rgb.resize(std::size_t(out.width) * out.height * 3);
    // Box-average the complete source, so sparse UI and dark scene contents
    // are not reduced to a single nearest-neighbour pixel per thumbnail cell.
    for (unsigned y = 0; y < out.height; ++y) for (unsigned x = 0; x < out.width; ++x) {
        const unsigned x0 = std::uint64_t(x) * width / out.width;
        const unsigned x1 = std::uint64_t(x + 1) * width / out.width;
        const unsigned y0 = std::uint64_t(y) * height / out.height;
        const unsigned y1 = std::uint64_t(y + 1) * height / out.height;
        std::uint64_t sums[3]{};
        for (unsigned sy = y0; sy < y1; ++sy) for (unsigned sx = x0; sx < x1; ++sx) {
            const auto* p = rgba.data() + (std::size_t(sy) * width + sx) * 4;
            sums[0] += p[bgra ? 2 : 0]; sums[1] += p[1]; sums[2] += p[bgra ? 0 : 2];
        }
        const auto count = std::uint64_t(x1 - x0) * (y1 - y0);
        for (unsigned c = 0; c < 3; ++c)
            out.rgb[(std::size_t(y) * out.width + x) * 3 + c] = (sums[c] + count / 2) / count;
    }
    const auto count = std::size_t(out.width) * out.height;
    out.compared = previous && previous->width == out.width && previous->height == out.height &&
                   previous->rgb.size() == out.rgb.size();
    std::array<unsigned, 32> tile_count{};
    double sum{}, squared{}, black{}, change{}, changed{};
    for (unsigned y = 0; y < out.height; ++y) for (unsigned x = 0; x < out.width; ++x) {
        const auto offset = (std::size_t(y) * out.width + x) * 3;
        const auto* p = out.rgb.data() + offset;
        // Encoded 8-bit RGB luma, not calibrated physical luminance/HDR.
        const double luma = (54.0 * p[0] + 183.0 * p[1] + 19.0 * p[2]) / 256.0;
        sum += luma; squared += luma * luma;
        black += std::max({p[0], p[1], p[2]}) <= 4;
        const auto tile = std::min(3u, y * 4 / out.height) * 8 + std::min(7u, x * 8 / out.width);
        out.tile_luma[tile] += luma; ++tile_count[tile];
        unsigned largest_change{};
        for (unsigned c = 0; c < 3; ++c) {
            out.hash = (out.hash ^ p[c]) * 1099511628211ull;
            if (out.compared) {
                const auto d = unsigned(std::abs(int(p[c]) - int(previous->rgb[offset + c])));
                change += d; largest_change = std::max(largest_change, d);
            }
        }
        changed += out.compared && largest_change > 8;
    }
    out.mean_luma = sum / count;
    out.luma_deviation = std::sqrt(std::max(0.0, squared / count - out.mean_luma * out.mean_luma));
    out.black_fraction = black / count;
    out.near_black = out.black_fraction >= 0.99;
    if (out.compared) {
        out.mean_change = change / (count * 3);
        out.changed_fraction = changed / count;
        out.identical = out.rgb == previous->rgb;
    }
    for (unsigned i = 0; i < 32; ++i) if (tile_count[i]) out.tile_luma[i] /= tile_count[i];
    return out;
}
} // namespace SotcVisual
