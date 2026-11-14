#pragma once

namespace bucu::cli {

// The exit codes of docs/product.md section 6. Codes from 5 are reserved for later issues.
inline constexpr int kExitOk = 0;
inline constexpr int kExitFailure = 1;   // a runtime failure
inline constexpr int kExitUsage = 2;     // a usage error
inline constexpr int kExitInvalid = 3;   // a bookmark rule is broken (product section 4)
inline constexpr int kExitConflict = 4;  // the URL is already stored

}  // namespace bucu::cli
