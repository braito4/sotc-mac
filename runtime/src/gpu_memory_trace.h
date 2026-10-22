// Original, bounded CPU observations. Never controls allocation, eviction or GPU completion.
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <locale>
#include <mutex>
#include <sstream>
#include <string>

namespace SotcMemory {
using U64 = std::uint64_t;
inline constexpr U64 Large = 64ull << 20;
inline std::atomic<U64> observation_errors{}, thread_sequence{};
inline U64 ThreadId() { static thread_local const U64 id = ++thread_sequence; return id; }
inline U64 Now() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline bool Enabled() {
    static const bool value = [] { const char* p = std::getenv("SOTC_MEMORY_TRACE"); return p && !std::strcmp(p, "1"); }();
    return value;
}
inline std::string Quote(const char* text) {
    std::string out{"\""};
    if (text) for (unsigned i = 0; text[i] && i < 128; ++i) {
        const unsigned char c = text[i];
        if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
        else if (c < 32) { char code[7]; std::snprintf(code, sizeof(code), "\\u%04x", c); out += code; }
        else out += char(c);
    }
    return out + '"';
}
struct Binding {
    U64 shader{}, slot{}, address{}, declared{}, clamped{};
    bool written{}, formatted{};
};
inline thread_local Binding binding{};
struct BindingScope {
    Binding previous;
    explicit BindingScope(Binding value) : previous{binding} { binding = value; }
    ~BindingScope() { binding = previous; }
    BindingScope(const BindingScope&) = delete;
};
struct BufferObservation {
    U64 address{}, size{}, physical{}, mapping_end{}, window_start{}, window_size{};
    U64 buffer{}, allocation{}, offset{}, tick{}, memory_type{}, elapsed_ns{};
    bool created{}, held{}, written{};
};
struct ImageDescription {
    U64 width{}, height{}, depth{}, levels{}, layers{}, samples{}, format{}, usage{}, allocation_flags{};
};
struct Image {
    U64 id{}, bytes{}, created_ns{}, retired_ns{}, owner{}, guest_address{}, guest_bytes{}, tick{}, flags{};
    unsigned state{}; // 0 unknown/constructed, 1 registered, 2 unregistered, 3 deletion queued
    unsigned role{}; // 0 unspecified, 1 reduced scene proxy (not a registered cache image)
    U64 source_level{};
    ImageDescription description{};
};

// Fixed storage: overflow is explicit and never confused with a complete inventory.
template<std::size_t Capacity> struct ImageTable {
    static_assert(Capacity && !(Capacity & (Capacity - 1)));
    struct Slot { unsigned occupied{}; Image image{}; };
    std::array<Slot, Capacity> slots{};
    static std::size_t Hash(U64 id) { id ^= id >> 33; id *= 0xff51afd7ed558ccdull; return id & (Capacity - 1); }
    Image* Find(U64 id) {
        for (std::size_t i = 0, at = Hash(id); i < Capacity; ++i, at = (at + 1) & (Capacity - 1)) {
            auto& slot = slots[at];
            if (!slot.occupied) return nullptr;
            if (slot.occupied == 1 && slot.image.id == id) return &slot.image;
        }
        return nullptr;
    }
    bool Insert(const Image& image) {
        if (!image.id || Find(image.id)) return false;
        Slot* free = nullptr;
        for (std::size_t i = 0, at = Hash(image.id); i < Capacity; ++i, at = (at + 1) & (Capacity - 1)) {
            auto& slot = slots[at];
            if (slot.occupied != 1 && !free) free = &slot;
            if (!slot.occupied) break;
        }
        if (!free) return false;
        *free = {1, image}; return true;
    }
    bool Erase(U64 id) {
        auto* image = Find(id);
        if (!image) return false;
        for (auto& slot : slots) if (&slot.image == image) { slot.occupied = 2; return true; }
        return false;
    }
};
struct GC {
    U64 used{}, trigger{}, pressure{}, critical{}, tick{}, scheduler_tick{}, kept{}, freed{};
    bool enabled{}, driver_usage{};
};
struct Limits { std::size_t images{8 << 20}, buffers{512 << 10}, summaries{1 << 20}; };

class Recorder {
public:
    explicit Recorder(Limits value = {}) : limits{value} {}
    ~Recorder() { for (auto* file : files) if (file) std::fclose(file); }
    void Buffer(const char* event, const BufferObservation& observation) {
        if (observation.size < Large) return;
        std::lock_guard lock{mutex};
        if (!std::strcmp(event, "copy_begin")) { ++copies; copy_requested_bytes += observation.size; }
        if (!std::strcmp(event, "copy_complete")) { ++completed_copies; copy_completed_bytes += observation.size; }
        if (written[1] >= limits.buffers) { ++dropped[1]; return; }
        auto out = Start(event);
        out << ",\"shader\":" << binding.shader << ",\"slot\":" << binding.slot
            << ",\"binding_address\":" << binding.address
            << ",\"declared_bytes\":" << binding.declared << ",\"clamped_bytes\":" << binding.clamped
            << ",\"binding_written\":" << int(binding.written) << ",\"formatted\":" << int(binding.formatted)
            << ",\"address\":" << observation.address << ",\"size\":" << observation.size
            << ",\"physical\":" << observation.physical << ",\"mapping_end\":" << observation.mapping_end
            << ",\"window_start\":" << observation.window_start << ",\"window_size\":" << observation.window_size
            << ",\"buffer\":" << observation.buffer << ",\"allocation\":" << observation.allocation
            << ",\"offset\":" << observation.offset << ",\"tick\":" << observation.tick
            << ",\"memory_type\":" << observation.memory_type << ",\"elapsed_ns\":" << observation.elapsed_ns
            << ",\"created\":" << int(observation.created) << ",\"held\":" << int(observation.held)
            << ",\"written\":" << int(observation.written);
        Write(1, out.str() + '}');
    }
    void Allocate(U64 id, U64 bytes, const ImageDescription& description) {
        std::lock_guard lock{mutex};
        ++allocated_count; allocated_bytes += bytes; ++live_count; live_bytes += bytes;
        Image image{.id = id, .bytes = bytes, .created_ns = Now(), .description = description};
        if (!images.Insert(image)) ++tracking_overflow;
        auto out = Start("image_allocate"); ImageFields(out, image); Write(0, out.str() + '}');
    }
    void Free(U64 id, U64 bytes) {
        std::lock_guard lock{mutex};
        if (const auto* image = images.Find(id); image && image->role == 1) {
            ++scene_proxy_destroyed_count; scene_proxy_destroyed_bytes += bytes;
        }
        ++freed_count; freed_bytes += bytes;
        if (live_count && live_bytes >= bytes) { --live_count; live_bytes -= bytes; }
        else ++unmatched_frees;
        if (!images.Erase(id)) ++untracked_frees;
        auto out = Start("image_destroy"); out << ",\"id\":" << id << ",\"bytes\":" << bytes;
        Write(0, out.str() + '}');
    }
    void Owner(U64 id, U64 owner, U64 address, U64 bytes, U64 flags, U64 tick, unsigned state) {
        std::lock_guard lock{mutex};
        if (auto* image = images.Find(id)) {
            image->owner = owner; image->guest_address = address; image->guest_bytes = bytes;
            image->flags = flags; image->tick = tick; image->state = std::min(state,3u);
            if (state == 3 && !image->retired_ns) image->retired_ns = Now();
        }
        auto out = Start("image_owner");
        out << ",\"id\":" << id << ",\"owner\":" << owner << ",\"guest_address\":" << address
            << ",\"guest_bytes\":" << bytes << ",\"flags\":" << flags << ",\"tick\":" << tick << ",\"state\":" << state;
        Write(0, out.str() + '}');
    }
    void AllocationFailed(std::int64_t result, const ImageDescription& description) {
        std::lock_guard lock{mutex};
        auto out = Start("image_allocation_failed");
        out << ",\"result\":" << result; ImageFields(out, Image{.description = description});
        Write(2, out.str() + '}');
    }
    void SceneProxy(U64 id, U64 owner, U64 level, U64 tick, bool retired) {
        std::lock_guard lock{mutex};
        if (auto* image = images.Find(id)) {
            if (image->role != 1) {
                ++scene_proxy_created_count; scene_proxy_created_bytes += image->bytes;
            }
            image->role = 1; image->owner = owner; image->source_level = level;
            image->tick = tick; image->state = retired ? 3 : 0;
            if (retired && !image->retired_ns) image->retired_ns = Now();
        }
        auto out = Start("image_scene_proxy");
        out << ",\"id\":" << id << ",\"owner\":" << owner << ",\"source_level\":" << level
            << ",\"tick\":" << tick << ",\"role\":1,\"state\":" << (retired ? 3 : 0);
        Write(0, out.str() + '}');
    }
    void Keep(bool tiled_download) { (tiled_download ? kept_tiled : kept_download).fetch_add(1, std::memory_order_relaxed); }
    void Collection(const GC& value) {
        const auto now = Now(); auto previous = last_snapshot.load(std::memory_order_relaxed);
        if (now - previous < 5'000'000'000ull || !last_snapshot.compare_exchange_strong(previous, now)) return;
        std::lock_guard lock{mutex}; gc = value; gc_ns = now; Summary("memory_snapshot", 0, 0, 0);
    }
    void Failure(std::int64_t code, U64 allocated, U64 recommended) {
        std::lock_guard lock{mutex}; Summary("memory_failure", code, allocated, recommended);
    }
private:
    std::ostringstream Start(const char* event) {
        std::ostringstream out; out.imbue(std::locale::classic());
        out << "{\"schema\":1,\"event\":" << Quote(event) << ",\"monotonic_ns\":" << Now()
            << ",\"sequence\":" << ++sequence << ",\"thread_id\":" << ThreadId();
        return out;
    }
    static void ImageFields(std::ostringstream& out, const Image& image) {
        const auto& d = image.description;
        out << ",\"id\":" << image.id << ",\"bytes\":" << image.bytes
            << ",\"owner\":" << image.owner << ",\"guest_address\":" << image.guest_address
            << ",\"guest_bytes\":" << image.guest_bytes << ",\"state\":" << image.state
            << ",\"role\":" << image.role << ",\"source_level\":" << image.source_level
            << ",\"flags\":" << image.flags << ",\"tick\":" << image.tick
            << ",\"created_ns\":" << image.created_ns << ",\"retired_ns\":" << image.retired_ns
            << ",\"width\":" << d.width << ",\"height\":" << d.height << ",\"depth\":" << d.depth
            << ",\"levels\":" << d.levels << ",\"layers\":" << d.layers << ",\"samples\":" << d.samples
            << ",\"format\":" << d.format << ",\"usage\":" << d.usage << ",\"allocation_flags\":" << d.allocation_flags;
    }
    void Summary(const char* event, std::int64_t code, U64 allocated, U64 recommended) {
        std::array<const Image*, 12> largest{};
        std::array<U64, 4> state_bytes{}, state_count{};
        std::array<U64, 2> role_bytes{}, role_count{};
        U64 scene_proxy_pending_count = 0, scene_proxy_pending_bytes = 0;
        std::array<U64, 5> size_bytes{}, size_count{};
        U64 tracked_bytes = 0, tracked_count = 0;
        for (const auto& slot : images.slots) if (slot.occupied == 1) {
            const auto& image = slot.image; tracked_bytes += image.bytes; ++tracked_count;
            state_bytes[image.state] += image.bytes; ++state_count[image.state];
            role_bytes[image.role] += image.bytes; ++role_count[image.role];
            if (image.role == 1 && image.state == 3) {
                ++scene_proxy_pending_count; scene_proxy_pending_bytes += image.bytes;
            }
            const unsigned bin = image.bytes < (1ull << 20) ? 0 : image.bytes < (4ull << 20) ? 1 :
                image.bytes < (16ull << 20) ? 2 : image.bytes < (64ull << 20) ? 3 : 4;
            size_bytes[bin] += image.bytes; ++size_count[bin];
            for (std::size_t i = 0; i < largest.size(); ++i) if (!largest[i] || image.bytes > largest[i]->bytes) {
                for (std::size_t j = largest.size() - 1; j > i; --j) largest[j] = largest[j - 1];
                largest[i] = &image; break;
            }
        }
        auto out = Start(event);
        out << ",\"code\":" << code << ",\"native_allocated_bytes\":" << allocated
            << ",\"native_recommended_bytes\":" << recommended << ",\"live_count\":" << live_count
            << ",\"live_bytes\":" << live_bytes << ",\"allocated_count\":" << allocated_count
            << ",\"allocated_bytes\":" << allocated_bytes << ",\"destroyed_count\":" << freed_count
            << ",\"destroyed_bytes\":" << freed_bytes << ",\"tracked_count\":" << tracked_count
            << ",\"tracked_bytes\":" << tracked_bytes << ",\"tracking_overflow\":" << tracking_overflow
            << ",\"untracked_frees\":" << untracked_frees << ",\"unmatched_frees\":" << unmatched_frees
            << ",\"observation_errors\":" << observation_errors.load()
            << ",\"large_copy_requests\":" << copies << ",\"large_copy_requested_bytes\":" << copy_requested_bytes
            << ",\"large_copy_completions\":" << completed_copies << ",\"large_copy_completed_bytes\":" << copy_completed_bytes
            << ",\"gc_age_ns\":" << (gc_ns ? Now() - gc_ns : 0) << ",\"gc_sample_known\":" << int(gc_ns != 0)
            << ",\"gc_used_bytes\":" << gc.used << ",\"gc_trigger_bytes\":" << gc.trigger
            << ",\"gc_pressure_bytes\":" << gc.pressure << ",\"gc_critical_bytes\":" << gc.critical
            << ",\"gc_tick\":" << gc.tick << ",\"scheduler_tick\":" << gc.scheduler_tick
            << ",\"gc_enabled\":" << int(gc.enabled) << ",\"gc_uses_driver_usage\":" << int(gc.driver_usage)
            << ",\"gc_kept_window_sample\":" << gc.kept << ",\"gc_freed_window_sample\":" << gc.freed
            << ",\"gc_kept_tiled_download\":" << kept_tiled.load() << ",\"gc_kept_download_no_pressure\":" << kept_download.load();
        const auto array = [&](const char* name, const auto& values) {
            out << ',' << Quote(name) << ":[";
            for (unsigned i = 0; i < values.size(); ++i) out << (i ? "," : "") << values[i];
            out << ']';
        };
        array("state_bytes", state_bytes); array("state_count", state_count);
        array("role_bytes", role_bytes); array("role_count", role_count);
        out << ",\"scene_proxy_created_count\":" << scene_proxy_created_count
            << ",\"scene_proxy_created_bytes\":" << scene_proxy_created_bytes
            << ",\"scene_proxy_destroyed_count\":" << scene_proxy_destroyed_count
            << ",\"scene_proxy_destroyed_bytes\":" << scene_proxy_destroyed_bytes
            << ",\"scene_proxy_pending_count\":" << scene_proxy_pending_count
            << ",\"scene_proxy_pending_bytes\":" << scene_proxy_pending_bytes;
        array("size_bytes", size_bytes); array("size_count", size_count); array("dropped_records", dropped);
        out << ",\"largest\":["; bool comma = false;
        for (const auto* image : largest) if (image) {
            out << (comma ? ",{\"resource\":\"image\"" : "{\"resource\":\"image\"");
            ImageFields(out, *image); out << '}'; comma = true;
        }
        Write(2, out.str() + "]}");
    }
    bool Open() {
        if (attempted) return files[0] && files[1] && files[2];
        attempted = true;
        const char* root = std::getenv("BB_GPU_USER_DIR");
        if (!root || !*root || !std::filesystem::path(root).is_absolute()) return false;
        directory = std::filesystem::path(root) / "memory";
        std::error_code error;
        if (!std::filesystem::create_directory(directory, error) || error) return false;
        for (unsigned i = 0; i < files.size(); ++i) files[i] = std::fopen((directory / names[i]).c_str(), "wx");
        if (!(files[0] && files[1] && files[2])) return false;
        std::fprintf(stderr, "MEMORY_TRACE: private buffer origins, image lifetimes and GC summaries in %s\n", directory.c_str());
        return true;
    }
    void Write(unsigned stream, const std::string& line) {
        if (!Open()) { ++dropped[stream]; return; }
        const std::size_t cap = stream == 0 ? limits.images : stream == 1 ? limits.buffers : limits.summaries;
        if (line.size() + 1 > cap) { ++dropped[stream]; return; }
        if (written[stream] + line.size() + 1 > cap) {
            if (stream != 2) { ++dropped[stream]; return; }
            const auto previous = directory / "summary.previous.jsonl";
            std::error_code error;
            if (std::filesystem::is_symlink(previous, error)) { ++dropped[stream]; return; }
            std::fclose(files[stream]); files[stream] = nullptr;
            std::filesystem::remove(previous, error); error.clear();
            std::filesystem::rename(directory / names[stream], previous, error);
            if (error) { ++dropped[stream]; return; }
            files[stream] = std::fopen((directory / names[stream]).c_str(), "wx"); written[stream] = 0;
            if (!files[stream]) { ++dropped[stream]; return; }
        }
        const bool ok = std::fwrite(line.data(), 1, line.size(), files[stream]) == line.size() &&
            std::fputc('\n', files[stream]) != EOF && std::fflush(files[stream]) == 0;
        if (ok) written[stream] += line.size() + 1;
        else { std::fclose(files[stream]); files[stream] = nullptr; ++dropped[stream]; }
    }
    std::mutex mutex;
    ImageTable<8192> images;
    Limits limits;
    std::filesystem::path directory;
    inline static constexpr const char* names[] = {"images.jsonl", "buffers.jsonl", "summary.jsonl"};
    std::array<FILE*, 3> files{};
    std::array<std::size_t, 3> written{};
    std::array<U64, 3> dropped{};
    U64 sequence{}, allocated_count{}, allocated_bytes{}, freed_count{}, freed_bytes{}, live_count{}, live_bytes{};
    U64 tracking_overflow{}, untracked_frees{}, unmatched_frees{}, copies{}, copy_requested_bytes{}, completed_copies{}, copy_completed_bytes{};
    U64 scene_proxy_created_count{}, scene_proxy_created_bytes{}, scene_proxy_destroyed_count{}, scene_proxy_destroyed_bytes{};
    std::atomic<U64> last_snapshot{}, kept_tiled{}, kept_download{};
    GC gc{}; U64 gc_ns{}; bool attempted{};
};
inline Recorder& Get() { static auto* recorder = new Recorder; return *recorder; }
template<class Function> inline void Observe(Function&& function) noexcept {
    if (!Enabled()) return;
    try { function(Get()); } catch (...) { ++observation_errors; /* Preserve the workload and mark incomplete evidence. */ }
}
inline void Buffer(const char* event, const BufferObservation& value) noexcept {
    if (value.size >= Large) Observe([&](Recorder& r) { r.Buffer(event, value); });
}
inline void ImageAllocated(U64 id, U64 bytes, const ImageDescription& value) noexcept { Observe([&](Recorder& r) { r.Allocate(id, bytes, value); }); }
inline void ImageFreed(U64 id, U64 bytes) noexcept { Observe([&](Recorder& r) { r.Free(id, bytes); }); }
inline void ImageOwner(U64 id, U64 owner, U64 address, U64 bytes, U64 flags, U64 tick, unsigned state) noexcept {
    Observe([&](Recorder& r) { r.Owner(id, owner, address, bytes, flags, tick, state); });
}
inline void ImageSceneProxy(U64 id, U64 owner, U64 level, U64 tick, bool retired) noexcept {
    Observe([&](Recorder& r) { r.SceneProxy(id, owner, level, tick, retired); });
}
inline void ImageAllocationFailed(std::int64_t result, const ImageDescription& value) noexcept { Observe([&](Recorder& r) { r.AllocationFailed(result, value); }); }
inline void Collection(const GC& value) noexcept { Observe([&](Recorder& r) { r.Collection(value); }); }
inline void Keep(bool tiled_download) noexcept { Observe([&](Recorder& r) { r.Keep(tiled_download); }); }
inline void Failure(std::int64_t code, U64 allocated, U64 recommended) noexcept { Observe([&](Recorder& r) { r.Failure(code, allocated, recommended); }); }
} // namespace SotcMemory
