# Unit Testing — Ecohive

This document covers the bugs found during a full read-through of `Core/`, the
Unity-based unit test suite added under `Test/`, and how to build/run it from
VS Code.

## Bugs found and fixed

All of the following have been fixed in the source (not just documented).
The ARM firmware (`cmake --build build/Debug`) and the host test suite
(`ctest --test-dir Test/build`) were both re-verified after the fixes.

### Math.c (MQ-9 gas sensor math)
- **`MQ9_GetPPM()` didn't guard against a non-finite `Rs_Ro`.** When
  `MQ9_GetRs()` returns `INFINITY` (sensor disconnected / near-zero ADC
  reading), `MQ9_GetPPM()` used to silently return **0 ppm** instead of an
  error value — a sensor fault was indistinguishable from "clean air."
  - Why it returned 0 and not infinity: `m` is negative, so
    `log10f(inf) = +inf`, and `(+inf - b) / m` flips sign to `-inf`, and
    `powf(10, -inf) == 0`.
  - **Fix**: `MQ9_GetPPM()` now checks `!isfinite(ratio) || ratio <= 0` and
    returns `NAN`, which can't be confused with a real ppm reading.
    Covered by `test_MQ9_GetPPM_ZeroRatio_ReturnsNaN`,
    `test_MQ9_GetPPM_NegativeRatio_ReturnsNaN`, and
    `test_MQ9_GetPPM_InfiniteRatio_ReturnsNaN` in `test_Math.c`.

### ADC.c
- `ADC_ReadTempSensor()` divided by `vref_raw` (channel 17 reading) with no
  zero-check, going infinite if that channel ever read 0.
  **Fix**: added `if (vref_raw == 0) return NAN;`.
- `ADC_ReadChannel()` had an unbounded `while (!(ADC1->SR & ADC_SR_EOC));` —
  could hang forever if a conversion never completed.
  **Fix**: added a bounded timeout that returns `0xFFFF` (outside the valid
  12-bit range) on timeout.

### SPI.c / SPI.h
- **`SPI_GPIO_Init()` only configured pins for `SPI2`.** The `SPI1` branch
  was a stub (`// SPI1` then `return;`), despite the file's header comment
  claiming "Tested on STM32F401RE (SPI1, SPI2)".
  **Fix**: implemented the SPI1 pin mapping (PA5=SCK, PA6=MISO, PA7=MOSI,
  AF5), mirroring the existing SPI2 block.
- `SPI.h` declared `SPI_SetNSS()` but it was never implemented in `SPI.c` —
  a link error waiting to happen. Checked actual usage in `main.c`:
  chip-select is (and always was) driven directly as a plain GPIO pin
  (`GPIOC->ODR`), and `SPI_Handle`/`SPI_Config` have no NSS pin field to
  back a real implementation. **Fix**: removed the dead declaration rather
  than invent an unused feature; left a comment pointing at the actual
  CS-handling pattern in `main.c`.
- `SPI_GetPrescaler()` computed `apb_frequency / spi_frequency` with no
  zero-check — divide-by-zero if `spi_frequency == 0`.
  **Fix**: returns the slowest prescaler (7) as a safe fallback instead of
  dividing.

### UART.c / UART.h
- **Name mismatch**: `UART.h` declared `UART2_Tx_DMA_Init(...)`, but the
  implementation in `UART.c` was named `UART1_Tx_DMA_Init` (and configures
  USART1/DMA2 Stream7, matching the "UART1" name). **Fix**: renamed the
  header declaration to `UART1_Tx_DMA_Init` to match the implementation.
- `Print_Message()` did `strcpy(First_Data->Data, SrcAddr)` into a fixed
  100-byte buffer with no length check — a buffer overflow if the source
  string was ≥100 bytes; `dataSize` was accepted but never used to bound
  the copy. **Fix**: now uses `memcpy` bounded to `sizeof(Data) - 1`,
  explicitly null-terminates, and stores the clamped length.
- The doc comments on `Split_Bits` and the DMA init function were swapped
  relative to what each function actually does. **Fix**: corrected.

### GPIO.c
- Cosmetic: the comment on `GPIOA_Init()` said "configure pull-down" but
  the code sets pull-up (`01`) — the code was correct, the comment wasn't.
  **Fix**: corrected the comment to say pull-up.

### Scope note
Everything except `Math.c` is tightly coupled to STM32 registers/HAL, so
none of the above (besides the `Math.c` fix) could be exercised by a host
unit test without a register-mocking layer — they were verified instead by
rebuilding the real ARM firmware target (`cmake --build build/Debug`) after
each change and confirming it still links cleanly.

## What was built

```
Test/
├── Unity/src/       — vendored, unmodified upstream Unity framework
│                       (unity.c, unity.h, unity_internals.h)
├── test_Math.h      — test case declarations
├── test_Math.c      — 9 Unity test cases for MQ9_GetRs() / MQ9_GetPPM()
├── CMakeLists.txt   — standalone host-native build
└── README.md        — this file
```

`Math.c` is the only module with no STM32 register/HAL dependency, so it's
the only one that can be compiled and run natively on a dev machine with no
mocking. Expected values in the tests are recomputed independently from the
documented formulas (not copy-pasted from the implementation), so the tests
actually catch regressions like a flipped sign, swapped operand, or wrong
constant.

### Test cases (`test_Math.c`)

| Test | What it checks |
|---|---|
| `test_MQ9_GetRs_ZeroADC_ReturnsInfinity` | `adc_raw = 0` → `Vout = 0` → guarded, returns `INFINITY` |
| `test_MQ9_GetRs_BelowVoltageThreshold_ReturnsInfinity` | `adc_raw = 1` → `Vout ≈ 0.0008V`, still below the 0.001V guard |
| `test_MQ9_GetRs_MidRangeADC_MatchesDividerFormula` | `adc_raw = 2048` matches the voltage-divider formula |
| `test_MQ9_GetRs_MaxADC_MatchesDividerFormula` | `adc_raw = 4095` (`Vout == Vref`) matches the formula |
| `test_MQ9_GetPPM_ZeroRatio_ReturnsNaN` | `Rs_Ro = 0` → returns `NAN` |
| `test_MQ9_GetPPM_NegativeRatio_ReturnsNaN` | `Rs_Ro = -1` → returns `NAN` |
| `test_MQ9_GetPPM_RatioOfOne_MatchesCurveFormula` | `Rs_Ro = 1` matches the CO curve-fit formula |
| `test_MQ9_GetPPM_TypicalRatio_MatchesCurveFormula` | `Rs_Ro = 0.3` matches the CO curve-fit formula |
| `test_MQ9_GetPPM_InfiniteRatio_ReturnsNaN` | `INFINITY` in → `NAN` out (the fixed safety gap) |

All 9 pass against the fixed implementation (verified via `cmake --build` +
`ctest`).

## Building and running

### From a terminal

```sh
cmake -S Test -B Test/build
cmake --build Test/build
ctest --test-dir Test/build --output-on-failure
```

### From VS Code

1. Install the **CMake Tools** extension (`ms-vscode.cmake-tools`) if not
   already installed. The repo root's `CMakeLists.txt` is pinned to the
   `arm-none-eabi` cross-compiler (`cmake/gcc-arm-none-eabi.cmake`) for the
   firmware build, so it can't run tests on your machine — `Test/` is a
   second, independent CMake project that uses your regular host `gcc`.
2. Point CMake Tools at `Test/` as the CMake source directory (via the
   `cmake.sourceDirectory` setting, or the Command Palette →
   **CMake: Select Configure Preset**), then **CMake: Build**.
3. Once configured, tests are auto-discovered by CMake Tools and show up in
   the **Testing** sidebar (flask icon) — run or debug individual cases from
   there.
4. Alternatively, the **C/C++ TestMate** extension reads Unity's
   `PASS`/`FAIL` output directly and lists tests without needing CTest
   integration at all.

### Extending to other modules

To add coverage for another module, mirror this pattern: a
`test_<Module>.c/.h` pair, plus linking that module's `.c` file into a new
CMake target in `Test/CMakeLists.txt`. For anything that includes
`stm32f4xx.h` and touches real registers, add a thin mock header for the
peripheral structs first — otherwise the test binary will crash trying to
read/write real hardware addresses on your dev machine.
