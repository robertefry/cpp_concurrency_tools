
#ifndef CTS_TESTS_HELPER_STATIC_STRING_HH
#define CTS_TESTS_HELPER_STATIC_STRING_HH

#include <cstddef>
#include <algorithm>
#include <string_view>

template <size_t N>
struct FixedString {
    static_assert(N >= 1 && "N == 0 is undefined behaviour");
    char _buffer[N];
    inline constexpr FixedString(char const (&name)[N]) { std::copy_n(name, N, _buffer); }
    inline constexpr operator std::string_view() { return std::string_view{_buffer, N-1}; }
    inline constexpr bool operator==(std::string_view other) { return std::string_view{*this} == other; }
};

#endif /* CTS_TESTS_HELPER_STATIC_STRING_HH */
