#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <cmath>
#include <exception>

namespace test {

struct TestCase {
    std::string name;
    std::function<void()> func;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> test_cases;
    return test_cases;
}

inline bool register_test(const std::string& name, std::function<void()> func) {
    registry().push_back({name, func});
    return true;
}

inline int& failed_checks() {
    static int count = 0;
    return count;
}

inline int& total_checks() {
    static int count = 0;
    return count;
}

inline bool approx_equal(double a, double b, double eps = 1e-4) {
    return std::fabs(a - b) <= eps;
}

struct Approx {
    double val;
    explicit Approx(double v) : val(v) {}
    bool operator==(double other) const { return approx_equal(val, other); }
    friend bool operator==(double other, const Approx& ap) { return approx_equal(ap.val, other); }
};

inline int run_all_tests() {
    int passed = 0;
    int failed = 0;
    auto& tests = registry();

    std::cout << "=================================================\n";
    std::cout << " Running " << tests.size() << " test case(s)...\n";
    std::cout << "=================================================\n";

    for (const auto& t : tests) {
        int checks_before = failed_checks();
        try {
            t.func();
            if (failed_checks() == checks_before) {
                std::cout << " [PASS] " << t.name << "\n";
                passed++;
            } else {
                std::cout << " [FAIL] " << t.name << "\n";
                failed++;
            }
        } catch (const std::exception& e) {
            std::cout << " [FAIL] " << t.name << " (Exception: " << e.what() << ")\n";
            failed++;
            failed_checks()++;
        } catch (...) {
            std::cout << " [FAIL] " << t.name << " (Unknown Exception)\n";
            failed++;
            failed_checks()++;
        }
    }

    std::cout << "=================================================\n";
    std::cout << " Summary: " << passed << " passed, " << failed << " failed (" 
              << total_checks() << " checks executed)\n";
    std::cout << "=================================================\n";
    return failed > 0 ? 1 : 0;
}

} // namespace test

#define TEST_CONCAT_IMPL(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT_IMPL(a, b)
#define TEST_UNIQUE_NAME(prefix) TEST_CONCAT(prefix, __LINE__)

#define TEST_CASE(name) \
    static void TEST_UNIQUE_NAME(_test_body_)(); \
    static bool TEST_UNIQUE_NAME(_test_reg_) = test::register_test(name, TEST_UNIQUE_NAME(_test_body_)); \
    static void TEST_UNIQUE_NAME(_test_body_)()

#define CHECK(expr) do { \
    test::total_checks()++; \
    if (!(expr)) { \
        std::cerr << "  FAILED: CHECK(" #expr ") at " << __FILE__ << ":" << __LINE__ << "\n"; \
        test::failed_checks()++; \
    } \
} while(0)

#define REQUIRE(expr) do { \
    test::total_checks()++; \
    if (!(expr)) { \
        std::cerr << "  FATAL REQUIRE(" #expr ") at " << __FILE__ << ":" << __LINE__ << "\n"; \
        test::failed_checks()++; \
        return; \
    } \
} while(0)

namespace doctest {
    using Approx = test::Approx;
}

#ifdef TEST_MAIN
int main() {
    return test::run_all_tests();
}
#endif
