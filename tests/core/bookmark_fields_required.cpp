// Not a CMake target. ctest runs the compiler on this file once for each required field of
// BookmarkFields (tests/core/CMakeLists.txt), with the macro of one field defined, and passes only
// when the build fails naming the missing field (FR-003). Defining no macro gives a file that
// builds.
#include "bucu/core/bookmark.hpp"

#ifdef OMIT_URL
#define BUCU_URL
#else
#define BUCU_URL .url = "about:blank",
#endif

#ifdef OMIT_TITLE
#define BUCU_TITLE
#else
#define BUCU_TITLE .title = "t",
#endif

#ifdef OMIT_DESCRIPTION
#define BUCU_DESCRIPTION
#else
#define BUCU_DESCRIPTION .description = "d",
#endif

auto build() { return bucu::core::make_bookmark({BUCU_URL BUCU_TITLE BUCU_DESCRIPTION}); }
