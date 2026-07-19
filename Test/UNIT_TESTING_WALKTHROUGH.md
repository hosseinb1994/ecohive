# How I Added Unit Tests to Ecohive (and What They Caught)

This project is an STM32F4 firmware for an environmental monitoring node: it
reads an MQ-9 gas sensor over ADC, an AM2302 temperature/humidity sensor over
a bit-banged single-wire protocol, and ships the results to an ESP32 over
SPI, with UART used for debug logging. Almost none of that is "unit
testable" in the traditional sense — it's register writes, timing loops, and
HAL calls that only make sense on real silicon.

But one file stood out: `Core/Src/Math.c`. It converts an ADC reading into
gas concentration (ppm) using nothing but arithmetic — no registers, no
hardware, just floats and a datasheet formula. That made it the perfect,
and honestly the *only*, candidate for real unit tests in this codebase. This
doc walks through how I set that up, and — more importantly — a real bug the
tests caught along the way.

## Why Unity, and why not test everything

I picked [Unity](https://github.com/ThrowTheSwitch/Unity) because it's the
de-facto standard test framework for embedded C. It's just three files
(`unity.c`, `unity.h`, `unity_internals.h`), has zero dependencies, and
compiles anywhere a C compiler exists — including directly on my dev
machine, without touching the target MCU.

The honest scoping decision was **not** to try to test everything. Modules
like `ADC.c`, `SPI.c`, `UART.c`, and `AM2302.c` all `#include "stm32f4xx.h"`
and read/write real memory-mapped registers (`RCC->AHB1ENR`, `GPIOA->MODER`,
etc.). Compiling and running that code on a Linux/macOS/Windows dev machine
means those addresses don't point to real hardware — the process would
segfault the moment it tried to touch one. Testing those properly requires
a *mocking layer* (fake register structs standing in for the real ones),
which is a legitimate next step but is its own separate effort, not
something to bolt on as an afterthought. So I scoped this pass to the one
module that's genuinely hardware-free: `Math.c`.

## Setting it up

**1. Vendor Unity, don't half-write it.**
I pulled the real, unmodified source straight from the upstream repo:

```sh
curl -O https://raw.githubusercontent.com/ThrowTheSwitch/Unity/master/src/unity.c
curl -O https://raw.githubusercontent.com/ThrowTheSwitch/Unity/master/src/unity.h
curl -O https://raw.githubusercontent.com/ThrowTheSwitch/Unity/master/src/unity_internals.h
```

into `Test/Unity/src/`. Hand-rolling a fake test framework would have been
both wrong and pointless — Unity is small enough to vendor directly, no
package manager needed.

**2. A second, independent CMake project.**
The repo root's `CMakeLists.txt` cross-compiles the firmware for
`arm-none-eabi` (see `cmake/gcc-arm-none-eabi.cmake`) — that toolchain has no
business trying to build a test binary meant to run on my own machine.
So `Test/CMakeLists.txt` is a **completely separate** CMake project that
uses the host's normal `gcc`:

```cmake
add_executable(test_Math
  test_Math.c
  Unity/src/unity.c
  ../Core/Src/Math.c        # the real production file, not a copy
)
```

The key detail: it compiles the *actual* `Core/Src/Math.c`, not a copy or a
reimplementation. That's what makes it a real regression test — if I break
the formula in the production file, the test breaks too.

**3. Write expected values independently, not copied from the source.**
This is the part I think matters most for anyone learning to test numeric
code: don't just paste the implementation's formula into the test and call
it "covered." I recomputed the expected voltage-divider and log-log curve
values in the test file using the same physics/formula, but written
independently:

```c
uint16_t adc_raw = 2048;
float vout = (adc_raw / ADC_MAX) * VREF_MCU;
float expected_rs = RL_MQ9 * (VCC_MQ9 - vout) / vout;

TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, expected_rs, MQ9_GetRs(adc_raw));
```

If someone later flips a sign, swaps `m` and `b`, or fat-fingers a constant
in `Math.c`, this catches it — a test that just mirrors the implementation
line-for-line wouldn't.

## The bug the tests actually found

Here's the part worth teaching, because it's a real example of a test
disagreeing with my own assumption — which is exactly what tests are for.

`MQ9_GetRs()` returns `INFINITY` when the ADC reading implies a near-zero
voltage (e.g. a disconnected sensor). I assumed `MQ9_GetPPM()` would just
propagate that `INFINITY` straight through, so I wrote a test expecting
infinity out. It failed:

```
Expected TRUE Was FALSE
```

Digging in, the actual behavior was **`MQ9_GetPPM(INFINITY)` returns `0`**,
not infinity. Here's why: the curve-fit constant `m` in the formula is
*negative* (`m = -0.654`). So:

```
log10f(INFINITY)        = +inf
(+inf - b) / m           = +inf / (negative number) = -inf
powf(10, -inf)            = 0
```

The sign flip through the negative divisor turns "infinite resistance" into
"zero ppm" — which, for a gas sensor, is the worst possible failure mode: **a
disconnected or faulty sensor silently reports "clean air" instead of an
error.** That's not a cosmetic bug, that's a safety-relevant one, and I only
found it because I wrote down my expectation as an assertion and let the
test tell me I was wrong.

The fix was to explicitly reject non-finite/non-physical input and return
`NAN` instead — a value that can never be confused with a real ppm reading:

```c
if (!isfinite(ratio) || ratio <= 0) return NAN;
```

And the test now documents *why*, not just *what*:

```c
void test_MQ9_GetPPM_InfiniteRatio_ReturnsNaN(void)
{
    /* Fixed: MQ9_GetRs() can return INFINITY on a near-zero ADC reading
     * (disconnected sensor / fault). MQ9_GetPPM() now rejects any
     * non-finite Rs_Ro and returns NAN instead of silently computing
     * "0 ppm" (which used to be indistinguishable from a genuine clean-air
     * reading). */
    TEST_ASSERT_TRUE(isnan(MQ9_GetPPM(INFINITY)));
}
```

## Running it

```sh
cmake -S Test -B Test/build
cmake --build Test/build
ctest --test-dir Test/build --output-on-failure
```

`Test/build` is disposable — delete it anytime, the three commands above
regenerate it from source. See `Test/README.md` for the full test-case
table and VS Code setup (CMake Tools / C++ TestMate).

## Takeaways

- **In embedded projects, look for the seams.** Most of this firmware is
  inseparable from hardware, but one file wasn't — and that's the one worth
  testing first. Don't force host tests onto register-level code without a
  mocking layer; that's a different, bigger investment.
- **Recompute expected values independently.** Copying the formula from the
  implementation into the test gives you a false sense of coverage.
- **Write the test before you're sure of the answer.** The infinity/NaN bug
  above wasn't found by reading the code harder — it was found by writing
  down what I *expected* as an assertion and letting the framework prove me
  wrong. That's the entire value of a test suite: it argues with you.
