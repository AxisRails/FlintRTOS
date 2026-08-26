# FlintRTOS Coding Standard (v0.1)

FlintRTOS is developed to be **certifiable** (ISO 26262 up to ASIL D, ISO/SAE 21434 — see the Safety & Security Concept). Code compliance is grounded in **MISRA**:

- **TCB code** (kernel + HAL — the trusted, privileged core): **MISRA C:2023** (C11), the simplest and most statically-analyzable subset.
- **Non-TCB code** (system services, OSAL, drivers, tools, tests — unprivileged): **MISRA C++:2023** (C++17). MISRA C++:2023 targets C++17 and incorporates the former AUTOSAR C++14 guidelines.

Reference: MISRA C/C++ overview — https://www.perforce.com/resources/qac/misra-c-cpp

> MISRA guideline **text is copyrighted**; this document does not reproduce it. It records the *project's adopted subset, rationale, and deviations* by rule/directive number and by MISRA category (**Mandatory / Required / Advisory**; rules are further **Decidable / Undecidable**). A licensed MISRA checker (e.g. Perforce QAC, Coverity, or a clang-tidy MISRA module) is the authority in the CI pipeline.

## Why split C (TCB) and C++ (non-TCB)

The trusted core must be auditable and, ideally, formally verifiable (seL4-style). C with a tight MISRA subset gives the smallest semantic surface and the best tool support for that. Non-TCB code benefits from C++'s type safety (strong typing, RAII, `enum class`, templates for zero-cost abstraction) without endangering the certified core, since a fault there is *contained* by the kernel's isolation. The two meet only through **C ABI headers** (`extern "C"`), so C++ can call the kernel but the kernel depends on no C++.

## Adopted subset — TCB (MISRA C:2023)

Enforced project-wide for `kernel/` and `hal/` (rationale in parentheses):

- **No dynamic memory** after init (Dir 4.12 / Rule 21.3): all kernel memory is Untyped-accounted (K§2.1). `malloc`/`free`/`calloc`/`realloc` are banned in the TCB.
- **No recursion** (Rule 17.2): bounds stack use and worst-case timing (determinism, K§10).
- **Fixed-width types only** (Dir 4.6): `uint32_t`, `int32_t`, … via `<stdint.h>`; no bare `int`/`long` in interfaces.
- **No implicit conversions that lose sign/precision** (Rule 10.x): explicit casts, documented.
- **Every branch fully bracketed**; `if/else if/else` chains terminated (Rule 15.7); `switch` has `default` (Rule 16.4).
- **One `return` per function where it does not harm clarity** (Advisory 15.5): applied pragmatically — early-return guard clauses are permitted where they improve readability and are documented as a deviation of 15.5 (Advisory).
- **No undefined/unspecified behavior**: no UB shifts, no signed overflow reliance, no aliasing violations.
- **All objects `const`-correct**; internal linkage via `static` (Rule 8.x).
- **No `goto`** except the single permitted forward-only error-unwind form (Rule 15.1 is Advisory; project bans `goto` entirely in v0.1).
- **Header discipline**: include guards; headers self-contained; no definitions in headers except `static inline` where justified.
- **MISRA C `__builtin_*` / compiler intrinsics** are wrapped behind a HAL/portability shim, never used raw in portable code.

## Adopted subset — non-TCB (MISRA C++:2023)

Enforced for `tests/`, `tools/`, and future `services/`, `osal/`, `drivers/`:

- **No exceptions, no RTTI** (deterministic, freestanding-friendly): `-fno-exceptions -fno-rtti` in embedded builds. (Host test/tool builds may relax this; embedded service builds must not.)
- **No dynamic allocation** in embedded service code after init (per-domain heaps are bounded, DRV§3.3); the host test build may allocate.
- **`enum class`** over plain enums; **`nullptr`**; **`constexpr`** over macros; **RAII** for resource lifetimes.
- **No implicit narrowing** (`-Wconversion`); braced-init to catch narrowing.
- **No `reinterpret_cast`** except in an audited HW-access shim; prefer typed interfaces.
- **Rule of zero/five** observed; no raw owning pointers in service code.
- Interfaces to the kernel go through the `extern "C"` ABI headers only.

## Enforcement in this repo (v0.1)

A licensed MISRA tool is not present in this environment, so the build approximates compliance with:

- **Strict warnings-as-errors:** `-Wall -Wextra -Werror -Wconversion -Wsign-conversion -Wshadow -Wundef -Wpointer-arith -Wcast-qual -Wstrict-prototypes` (C) and the C++ equivalents. Many MISRA required rules map onto these.
- **`clang-tidy`** with `cppcoreguidelines-*`, `bugprone-*`, `cert-*`, `misc-*`, `readability-*` (a partial MISRA proxy; the CERT and CppCoreGuidelines checks overlap substantial MISRA content).
- **`.clang-format`** for a single consistent style (K&R-ish, 4-space, 100-col).
- Language standards pinned: **C11** (`-std=c11`), **C++17** (`-std=c++17`).

CI in production adds the licensed MISRA checker and produces a **compliance matrix** (each guideline: compliant / deviated-with-rationale / not-applicable), which is safety-case evidence.

## Deviation process

A deviation from a **Required** or **Advisory** rule needs: the rule id, the location, a rationale, and reviewer sign-off, recorded in `docs/misra-deviations.md` (created as deviations arise). **Mandatory** rules are never deviated. Each `/* DEVIATION(Rule x.y): reason */` comment in code cross-references that log.

## File & naming conventions

- C TCB files: `snake_case.c/.h`, symbols `flint_<module>_<verb>()`, types `flint_<name>_t`, macros `FLINT_<NAME>`.
- C++ non-TCB files: `snake_case.cpp/.hpp`, namespace `flint::`, types `PascalCase`, functions `camelCase` or `snake_case` (consistent per module).
- One module = one `.c/.h` (or `.cpp/.hpp`) pair; public API in `include/flint/`.
