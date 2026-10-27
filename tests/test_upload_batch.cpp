#include "gpu_upload_batch.h"
#include <cassert>
#include <limits>
int main() {
    assert(!SotcUploadBatch::Enabled(false));
    const char* setting=std::getenv("SOTC_UPLOAD_BATCH");
    assert(SotcUploadBatch::Enabled(true)==(!setting || std::strcmp(setting,"0")!=0));
    using SotcUploadBatch::Budget;
    Budget b;
    assert(!b.CompletedUpload(1,0));
    assert(!b.CompletedUpload(1,Budget::Limit-1));
    assert(b.CompletedUpload(1,1));
    b.Reset();assert(!b.CompletedUpload(1,1));
    assert(!b.CompletedUpload(2,Budget::Limit-1));
    assert(b.CompletedUpload(2,1));
    assert(b.CompletedUpload(3,std::numeric_limits<std::uint64_t>::max()));
    assert(b.bytes==Budget::Limit);
}
