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


## Automated check and ratchet

`tools/misra/run_misra.sh` runs cppcheck's MISRA addon (MISRA C:2012 with
amendments, the closest open-source checker to MISRA C:2023) over the TCB:
`kernel/`, `portable/LLVM_AArch64/`, `portable/MemMang/heap_4.c` and
`bsp/rpi4/`. `tools/misra/baseline.txt` holds the accepted per-rule counts
(264 findings across 28 rules at introduction; the largest groups are 2.5
unused macros in configuration headers, 15.5 single point of exit, 11.4/11.5
pointer/integer conversions for MMIO and `void *` APIs, and 12.2/10.x shift
and essential-type rules in register code). CI fails on any increase; when a
cleanup lowers a count, `--update` locks the improvement in. Put a licensed
rule-text file at `tools/misra/rules.txt` to get full rule messages.

Deviations specific to the safety code: frame-pointer walking and MMIO
necessarily convert integers to pointers (11.4), and the panic path uses its
own bounded string helpers instead of printf so it works with a damaged heap.
