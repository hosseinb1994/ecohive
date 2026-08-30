# MQ-9 Fault-Injection Test Protocol

**Purpose:** produce a labelled fault dataset to complement the 48-hour clean
baseline capture already collected and backed up. This session must land in
its **own** log file on the SD card, never mixed into the clean file.

Read the whole document once before touching any wires. The physical steps
are simple, but the timing requirements (how long to hold each fault) come
directly from thresholds hard-coded in `FaultDetect.c` and are easy to get
wrong if you go too fast.

---

## 0. Ground truth in this protocol

The on-board `quality_flags` byte (`FaultDetect.c`) is a **rule-based
sanity check**, not your label source. Your label source is the wall-clock
start/stop time you write down for each fault in the table in Section 5 (plus
`timestamp_ms` from the CSV if you can see it live). The flags are only there
so you can visually confirm a fault physically registered — for one fault
type below (open/floating) the flags may not fire in a predictable way at
all, and that's fine; log the times anyway.

CSV column order (unchanged from your clean capture):
```
timestamp_ms,raw_mq9_adc,computed_ppm,mcu_temp_c,am2302_temp_c,humidity_pct,quality_flags
```
- `timestamp_ms` = FreeRTOS tick count since **this boot**, in ms (not wall-clock).
- Row cadence: one row every **7 s** (`CSV_LOG_INTERVAL_MS`).
- `quality_flags` bit meanings:

| Bit | Value | Name | Fires when |
|---|---|---|---|
| 0 | 0x01 | STUCK | last 300 consecutive raw ADC reads (~5 min at the internal ~1 Hz sensor loop) are all within 1 ADC count of each other |
| 1 | 0x02 | SATURATED | raw ADC < 20 or > 4075 (out of 0–4095) |
| 2 | 0x04 | RANGE | computed ppm is negative or > 1000 |
| 3 | 0x08 | RATE | ppm jumped by > 500 ppm between two consecutive ~1 Hz reads (usually only visible as a one-row blip right at a transition) |

Flags OR together (e.g. SATURATED+RANGE = 6).

---

## 1. CRITICAL: keep this session in a separate file

`SD_Log.c` auto-increments the filename (`/ECOHIVE/LOG_0001.CSV`,
`LOG_0002.CSV`, ...) but **only decides the next number once, at the moment
`SD_Log_Init()` runs** — i.e. once at every board reset/power-up. It never
prints the chosen filename over UART, so you have to control this by
controlling *when the board resets*, not by reading a filename off the
console.

**Procedure:**
1. Before you touch anything, confirm your 48-hour clean file is safe: pull
   the SD card, plug it into a PC, and note the highest `LOG_XXXX.CSV`
   number present (this should be your clean file — you already backed it
   up, but re-confirm it's there and don't delete/rename it now).
2. Put the card back in the board.
3. Do **exactly one** reset or full power-cycle to start the fault session.
   This is the only action that opens a new file. From this point, every
   row — baseline and every fault — lands in one single new
   `LOG_XXXX.CSV`, numbered one higher than your clean file.
4. **Do not reset or power-cycle again until the entire session (Sections
   3–4) is complete.** A second reset mid-session would split your fault
   run across two files and make the `timestamp_ms` column non-contiguous
   (wall-clock times in your table still work as the real label source, but
   it's simpler to avoid this).
5. **Don't disturb the SD card connection during the session.** There's a
   second, less obvious way a new file can get opened mid-run: if the card
   ever glitches (`SD_Log_WriteRow` fails → status `SD_LOG_FAILED` → fast
   LED blink) and then recovers on its own retry, the recovery path calls
   `SD_Log_Init()` again and silently opens *another* new incremented file.
   Keep the card seated and the board undisturbed. Afterward, check the
   heartbeat LED was on its normal slow 5s/5s blink throughout (fast
   150ms/150ms blink at any point = an SD hiccup happened — check the card
   afterward for an unexpected extra `LOG_XXXX.CSV` and stitch by
   wall-clock time if so).
6. After the session, pull the card again and confirm: your clean file is
   untouched, and exactly one new `LOG_XXXX.CSV` exists containing this
   whole fault run. Back it up immediately, same as you did for the clean
   capture.

---

## 2. Safety

- **The MQ-9 `AO` line only ever goes to GND or 3.3V in this protocol —
  never 5V.** `AO` feeds directly into PA0, an STM32 ADC input pin whose
  absolute max is roughly VDD + 0.3V (~3.6V). The sensor's own `VCC` stays
  wired to 5V as normal throughout — you are only ever touching the `AO`
  signal wire (or the PA0 header pin, whichever is easier for you to
  access), not the sensor's power pins.
- Double-check which pin is 3.3V before connecting — use a labelled 3V3 pin
  (e.g. on the ST-LINK/board header), not something assumed from memory.
- Power down before changing any wiring if you're not confident you can do
  it live without a slip.

---

## 3. What you'll need

- The board running, SD card seated, UART console open if you want to watch
  `timestamp_ms`/flags live (optional but recommended).
- A jumper wire to short `AO`/PA0 to GND, and another to short it to 3.3V.
- A watch/phone clock synced to what you'll write in the table (doesn't need
  to be exact to the second, just consistent).
- Optional, for the STUCK fault only: a small pot (e.g. 10kΩ) wired as a
  divider between 3.3V and GND with the wiper to PA0, plus tape or a clip to
  physically immobilize it. See Section 4.5 for why this matters.

---

## 4. Fault sequence

Do these **in order**, in a single power-on session (Section 1). For each
fault: note the wall-clock start time the instant you make the physical
change, hold for at least the **minimum hold time** below, note the
wall-clock stop time the instant you undo it, then move to the next line.
Write every start/stop into the table in Section 5 as you go — don't try to
reconstruct it afterward.

### 4.1 Baseline (start of session)
- **Action:** nothing — leave the MQ-9 wired normally.
- **Minimum hold:** 5 minutes (~40+ rows).
- **Reps:** 1 (just at the very start).
- **Expect to see:** `raw_mq9_adc` in the same range as your 48-hour clean
  capture, changing gently row to row (not pegged at one exact value for
  many rows in a row). `quality_flags` = 0 on essentially every row.

### 4.2 Disconnect (open/floating)
- **Action:** remove the jumper/wire connecting MQ-9 `AO` to PA0, leaving
  the PA0 header pin completely unconnected. Don't touch it to anything.
- **Minimum hold:** 3 minutes (~25+ rows) per rep.
- **Reps:** 3.
- **Expect to see:** this is the one fault type with **no guaranteed flag**
  — a floating high-impedance ADC input has no defined voltage, so
  behavior varies by board/environment. Watch for either (a) `raw_mq9_adc`
  swinging around erratically between rows instead of your clean baseline's
  gentle drift — this may transiently set RATE (0x08) or RANGE (0x04) — or
  (b) it settling at some fixed value that's clearly outside your normal
  clean-baseline range. Either is a valid confirmation that the fault took;
  if flags stay 0 the whole time, that's expected sometimes — trust your
  wall-clock label, not the flags, for this one.
- **After each rep:** reconnect `AO` to PA0 and hold 2 minutes (~17+ rows)
  before starting the next rep — this gives you a labelled "recovered to
  clean" segment bracketing each fault, and lets `raw_mq9_adc` settle back
  to baseline before you disconnect again.

### 4.3 Short to GND (low rail)
- **Action:** connect `AO`/PA0 directly to GND with a jumper.
- **Minimum hold:** 2 minutes (~17+ rows) per rep — the flag response here
  is near-instant (next ~1s internal read), so this is mostly about getting
  enough labelled rows for training, not about waiting for detection.
- **Reps:** 3.
- **Expect to see:** `raw_mq9_adc` near 0 (single digits, roughly 0–2).
  `computed_ppm` = **exactly -1.00** (the code's defined invalid-reading
  sentinel — Vout reads as ~0V, which the Rs formula treats as divide-by-zero
  and returns as infinite resistance, which the ppm formula then rejects).
  `quality_flags` = **6** (SATURATED 0x02 + RANGE 0x04) on essentially every
  row while held; the very first affected row may briefly show **14**
  (also RATE 0x08) from the sudden ppm jump.
- **After each rep:** disconnect the GND jumper and reconnect `AO` to PA0
  normally, hold 2 minutes before the next rep (same reasoning as 4.2).

### 4.4 Short to 3.3V (high rail)
- **Action:** connect `AO`/PA0 directly to a 3.3V pin with a jumper.
  **Never use 5V here** (see Section 2).
- **Minimum hold:** 2 minutes (~17+ rows) per rep.
- **Reps:** 3.
- **Expect to see:** `raw_mq9_adc` pegged near full scale (roughly
  4075–4095). `computed_ppm` will read as a large, clearly-abnormal number
  — with the code's uncalibrated placeholder `Ro_MQ9` it works out to
  roughly 1000+ ppm, but the exact number shifts with whatever `Ro_MQ9` your
  board is actually using, so don't worry about matching a specific figure.
  `quality_flags` will have **SATURATED (0x02) set for certain**; RANGE
  (0x04) will very likely also be set (ppm this high normally exceeds the
  1000 ppm datasheet ceiling the code checks against), giving **6** on most
  rows, possibly briefly **14** on the first row (RATE too).
- **After each rep:** disconnect and reconnect `AO` to PA0 normally, hold 2
  minutes before the next rep.

### 4.5 Stuck at a fixed mid-level (best effort)
- **Action:** connect `AO`/PA0 to a steady mid-range voltage — ideally via a
  potentiometer divider between 3.3V and GND (wiper to PA0) set to roughly
  1.2–2.1V, **taped or clipped in place so it physically cannot drift**.
  Deliberately avoid the near-0V/near-3.3V ends — you want this fault to
  look like "reading is frozen," not "reading is saturated" (mixing the two
  makes the sample harder to use for training a STUCK-specific label).
- **Minimum hold: 8 minutes (480 s), held motionless.** This is not a
  soft recommendation — the STUCK check needs 300 consecutive internal
  ~1 Hz reads to land within 1 ADC count (~0.8 mV) of each other before it
  will ever set the flag, i.e. a genuine 5-minute floor before anything can
  happen, plus margin so the flag has time to show up in a few 7s-spaced CSV
  rows afterward.
- **Reps:** 1, or 2 if the first one clearly worked.
- **Why this one may just fail, and that's OK:** holding <1 ADC count of
  stability by hand for 5+ minutes is not realistically achievable — a
  bare hand-held wire will almost certainly drift by more than that from
  hand tremor and contact resistance alone. If you don't have a way to
  physically clamp the voltage divider, do this fault anyway and log the
  times, but don't be surprised if `quality_flags` never shows STUCK
  (0x01) — the wall-clock label ("I held it as steady as I physically
  could here") is still useful ground truth even if the on-board rule
  doesn't catch it.
- **Expect to see (if it works):** `raw_mq9_adc` reading the exact same
  value for many consecutive rows in a row (this is visible well before the
  5-minute mark, since your 7s-spaced CSV rows are already a sparse sample
  of the underlying 1Hz readings — seeing the identical raw value repeat
  for many rows straight is itself a good sign). After the 300s/5-minute
  mark, `quality_flags` should flip to include **1** (STUCK) and stay set
  as long as it's held.
- **After:** disconnect and reconnect `AO` to PA0 normally, hold 2 minutes
  to close out the session on a clean segment.

---

## 5. Ground-truth log

Fill this in live as you go — this table (not the on-board flags) is your
label source. Add rows as needed for extra reps.

| # | Fault type | Rep | Wall-clock start | Wall-clock stop | `timestamp_ms` start | `timestamp_ms` stop | Confirmed via (raw/ppm/flags observed) |
|---|---|---|---|---|---|---|---|
| 1 | Baseline | 1 | | | | | |
| 2 | Disconnect (open) | 1 | | | | | |
| 3 | Reconnect | 1 | | | | | |
| 4 | Disconnect (open) | 2 | | | | | |
| 5 | Reconnect | 2 | | | | | |
| 6 | Disconnect (open) | 3 | | | | | |
| 7 | Reconnect | 3 | | | | | |
| 8 | GND short | 1 | | | | | |
| 9 | Reconnect | 4 | | | | | |
| 10 | GND short | 2 | | | | | |
| 11 | Reconnect | 5 | | | | | |
| 12 | GND short | 3 | | | | | |
| 13 | Reconnect | 6 | | | | | |
| 14 | 3.3V short | 1 | | | | | |
| 15 | Reconnect | 7 | | | | | |
| 16 | 3.3V short | 2 | | | | | |
| 17 | Reconnect | 8 | | | | | |
| 18 | 3.3V short | 3 | | | | | |
| 19 | Reconnect | 9 | | | | | |
| 20 | Stuck (mid-level) | 1 | | | | | |
| 21 | Reconnect (final) | 10 | | | | | |

---

## 6. Quick-reference summary

| Fault | Min hold | Reps | Confirm via |
|---|---|---|---|
| Baseline | 5 min | 1 | flags=0, raw in normal range |
| Disconnect/open | 3 min | 3 | erratic raw, or fixed-but-abnormal raw; flags unreliable |
| GND short | 2 min | 3 | raw ≈0–2, ppm = -1.00, flags = 6 |
| 3.3V short | 2 min | 3 | raw ≈4075–4095, ppm abnormally high, flags = 6 (SATURATED guaranteed) |
| Stuck / mid-level | 8 min | 1–2 | raw frozen at one value; flags → 1 only after 5 min, best-effort |
| Reconnect (between every fault) | 2 min | after each rep | raw returns to baseline range, flags=0 |

Estimated total session length: roughly 60–70 minutes for one full pass of
Sections 4.1–4.5 as written. Never use 5V on the `AO`/PA0 line. Only one
reset/power-cycle for the whole session (Section 1).
