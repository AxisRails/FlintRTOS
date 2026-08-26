// FlintRTOS - unit-test runner entry point (non-TCB, C++17).
#include "test_framework.hpp"

int main() {
    return ::flint::test::Registry::instance().run();
}
