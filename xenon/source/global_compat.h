#pragma once
// Force-included into every C++ translation unit (see CXXFLAGS -include).
// The Xbox/VC++ build got unqualified min()/max() from <windows.h>; several
// game files call them without std::. Provide them globally (function
// templates, not macros, so std::min/max still work).
#ifdef __cplusplus
#include <algorithm>
template <class T> inline T min(T a, T b) { return a < b ? a : b; }
template <class T> inline T max(T a, T b) { return a > b ? a : b; }
#endif
