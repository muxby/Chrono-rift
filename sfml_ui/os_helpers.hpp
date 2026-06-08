#pragma once

template <typename T>
inline T cr_min(T a, T b) { return (a < b) ? a : b; }

template <typename T>
inline T cr_max(T a, T b) { return (a > b) ? a : b; }
