#pragma once

#include <cstdint>
#include <string>

namespace neurons::engine::persistence {

struct PatchDocumentInfo {
    std::string patchId;
    std::uint32_t schemaVersion{1};
    std::string appVersion;
};

} // namespace neurons::engine::persistence
