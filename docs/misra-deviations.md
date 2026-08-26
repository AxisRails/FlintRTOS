# MISRA / Static-Analysis Deviations Log

Records project deviations and the static-analysis posture. Mandatory MISRA rules are never deviated. Required/Advisory deviations need rule id, rationale, and reviewer sign-off (see `CODING-STANDARD.md`).

## Static-analysis posture (increment 1)

The production pipeline uses a **licensed MISRA checker** (Perforce QAC / Coverity / clang-tidy MISRA module) as the authority. This environment approximates compliance with strict warnings-as-errors + `clang-tidy` (CERT + CppCoreGuidelines + bugprone). The build compiles cleanly under:
`-Wall -Wextra -Werror -Wconversion -Wsign-conversion -Wshadow -Wundef -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes`.

### Accepted `clang-tidy` advisory findings (not defects)

| Check | Disposition | Rationale |
|---|---|---|
| `misc-include-cleaner` | Accepted / not fixed | Policy demands each TU directly re-include every header that transitively provides a symbol. FlintRTOS convention is **self-contained module headers** (each `flint/<mod>.h` includes its own dependencies), so a `.c` including its module header is complete. Fixing would duplicate include lists against project convention. Direct `<stdint.h>/<stdbool.h>/<stddef.h>` were added where a TU uses those types directly. |
| `bugprone-branch-clone` (msgtag length vs n_caps → same error) | Accepted | Two distinct validation conditions intentionally map to the same `FLINT_ERR_MSG_TOO_LONG` result; kept as separate branches for readability over de-duplication. |
| `cppcoreguidelines-init-variables` (`st`, `i`) | Accepted | `st` is assigned on every control-flow path before use (single-assignment-per-path style, MISRA-friendly); loop index `i` is initialised in the `for` header. Declaration-time initialisation would add a dead store. |
| `readability-magic-numbers` / `-avoid-magic-numbers` | Disabled | Bit-layout constants (shifts/masks) are defined as named enums/macros; remaining literals are self-evident in context. |

These are re-evaluated against the licensed MISRA tool's actual rule mapping in CI; this log is provisional for the host increment.
