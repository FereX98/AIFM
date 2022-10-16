#pragma once

#include <cstdint>

namespace far_memory {

// Global.
// This would affects size of certain data structures, keep it as small as possible
// Shi*: This may possibly cause performance issues.
constexpr static uint32_t kMaxNumDSIDs = 500000;
// For data structures inrelavent to DataFrame, use original bound to avoid enlarging them
constexpr static uint8_t kFillerMaxNumDSIDs = 255;
constexpr static uint8_t kMaxNumDSTypes = 255;

// Vanilla ptr.
constexpr static uint8_t kVanillaPtrDSType = 0;
constexpr static uint32_t kVanillaPtrDSID = 0; // Reserve 0 as its fixed DS ID.
constexpr static uint32_t kVanillaPtrObjectIDSize =
    sizeof(uint64_t); // Its object ID is always the remote object addr.

// Hashtable.
constexpr static uint32_t kHashTableDSType = 1;

// DataFrameVector.
constexpr static uint32_t kDataFrameVectorDSType = 2;

} // namespace far_memory
