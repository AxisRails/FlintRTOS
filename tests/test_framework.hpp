// FlintRTOS - minimal C++17 unit-test framework (non-TCB tooling).
// No dynamic allocation, no exceptions; safe under -fno-exceptions.
// Demonstrates the C/C++ split: C++ tests drive the C-ABI kernel.
#ifndef FLINT_TEST_FRAMEWORK_HPP
#define FLINT_TEST_FRAMEWORK_HPP

#include <cstdio>

namespace flint::test {

using TestFn = void (*)();

class Registry {
public:
    static constexpr int kMax = 256;

    static Registry& instance() {
        static Registry inst;
        return inst;
    }

    void add(const char* name, TestFn fn) {
        if (count_ < kMax) {
            names_[count_] = name;
            fns_[count_] = fn;
            ++count_;
        }
    }

    int run() {
        std::printf("Running %d test group(s)...\n\n", count_);
        for (int i = 0; i < count_; ++i) {
            std::printf("[ RUN  ] %s\n", names_[i]);
            fns_[i]();
        }
        std::printf("\n%d checks, %d failure(s)\n", checks_, failures_);
        return (failures_ == 0) ? 0 : 1;
    }

    void reportCheck(bool ok, const char* expr, const char* file, int line) {
        ++checks_;
        if (!ok) {
            ++failures_;
            std::printf("  FAIL: %s  (%s:%d)\n", expr, file, line);
        }
    }

private:
    const char* names_[kMax] = {};
    TestFn fns_[kMax] = {};
    int count_ = 0;
    int checks_ = 0;
    int failures_ = 0;
};

class Registrar {
public:
    Registrar(const char* name, TestFn fn) { Registry::instance().add(name, fn); }
};

}  // namespace flint::test

#define FLINT_TEST(name)                                                    \
    static void name();                                                     \
    static ::flint::test::Registrar flint_reg_##name(#name, name);          \
    static void name()

#define CHECK(cond) \
    ::flint::test::Registry::instance().reportCheck((cond), #cond, __FILE__, __LINE__)

#endif  // FLINT_TEST_FRAMEWORK_HPP
