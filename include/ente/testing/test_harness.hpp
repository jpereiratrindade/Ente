#pragma once

#include <iostream>
#include <string>
#include <string_view>
#include <format>
#include <source_location>
#include <cstdlib>
#include <exception>

namespace ente::testing {

class TestFailureException : public std::exception {
public:
    explicit TestFailureException(std::string message) : message_(std::move(message)) {}
    [[nodiscard]] const char* what() const noexcept override { return message_.c_str(); }
private:
    std::string message_;
};

inline void fail_test(
    std::string_view expression,
    std::string_view extra_msg,
    const std::source_location location = std::source_location::current()
) {
    std::string err = std::format(
        "\n[TEST_FAILURE] Assertion failed at {}:{}: in function '{}'\n  Expression: {}\n  Details: {}\n",
        location.file_name(),
        location.line(),
        location.function_name(),
        expression,
        extra_msg.empty() ? "Condition evaluated to false" : extra_msg
    );
    std::cerr << err << std::endl;
    throw TestFailureException(err);
}

template <typename T, typename U>
inline void assert_eq_impl(
    const T& actual,
    const U& expected,
    std::string_view actual_expr,
    std::string_view expected_expr,
    const std::source_location location = std::source_location::current()
) {
    if (!(actual == expected)) {
        std::string msg;
        if constexpr (std::formattable<T, char> && std::formattable<U, char>) {
            msg = std::format("Expected {} ({}) == {} ({})", actual_expr, actual, expected_expr, expected);
        } else {
            msg = std::format("Expected {} == {}", actual_expr, expected_expr);
        }
        fail_test(std::format("{} == {}", actual_expr, expected_expr), msg, location);
    }
}


template <typename T, typename U>
inline void assert_ne_impl(
    const T& actual,
    const U& expected,
    std::string_view actual_expr,
    std::string_view expected_expr,
    const std::source_location location = std::source_location::current()
) {
    if (actual == expected) {
        fail_test(std::format("{} != {}", actual_expr, expected_expr), "Values were unexpectedly equal", location);
    }
}

inline void assert_true_impl(
    bool condition,
    std::string_view expr,
    const std::source_location location = std::source_location::current()
) {
    if (!condition) {
        fail_test(expr, "Condition evaluated to false", location);
    }
}

inline void assert_false_impl(
    bool condition,
    std::string_view expr,
    const std::source_location location = std::source_location::current()
) {
    if (condition) {
        fail_test(expr, "Condition evaluated to true (expected false)", location);
    }
}

} // namespace ente::testing

// Mandatory deterministic test assertions (Never optimized out under NDEBUG/Release builds)
#define ENTE_TEST_ASSERT(expr) \
    ::ente::testing::assert_true_impl(static_cast<bool>(expr), #expr, std::source_location::current())

#define ENTE_TEST_ASSERT_EQ(a, b) \
    ::ente::testing::assert_eq_impl((a), (b), #a, #b, std::source_location::current())

#define ENTE_TEST_ASSERT_NE(a, b) \
    ::ente::testing::assert_ne_impl((a), (b), #a, #b, std::source_location::current())

#define ENTE_TEST_ASSERT_TRUE(expr) \
    ::ente::testing::assert_true_impl(static_cast<bool>(expr), #expr, std::source_location::current())

#define ENTE_TEST_ASSERT_FALSE(expr) \
    ::ente::testing::assert_false_impl(static_cast<bool>(expr), #expr, std::source_location::current())

#define ENTE_TEST_FAIL(msg) \
    ::ente::testing::fail_test(#msg, msg, std::source_location::current())
