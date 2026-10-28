#pragma once
namespace SotcIndirectIndex {
// This exact function is compiled as MSL and by the CPU sanitizer fixture.
inline constexpr char Validation[] = R"SOTC(
uint validate_indices(ulong length, ulong base, uint width, uint first,
                      uint count, uint instances, ulong capacity) {
    if (width!=2 && width!=4) return 1;
    if (count==0 || instances==0) return 0;
    ulong advance=ulong(first)*width;
    ulong bytes=ulong(count)*width;
    if (base>length || advance>length-base) return 1;
    if (bytes>length-base-advance || (base+advance)%width!=0) return 1;
    return bytes>capacity ? 2 : 0;
}
// Call only after validate_indices succeeded with unlimited copy capacity.
bool bypass_indices(ulong base, uint width, uint first, uint count, uint instances,
                    bool selective) {
    if (!selective) return false;
    if (!count || !instances) return true;
    ulong at=base+ulong(first)*width;
    return at<0x100000000ul && ulong(count)*width<=0x100000000ul-at;
}
)SOTC";
}
