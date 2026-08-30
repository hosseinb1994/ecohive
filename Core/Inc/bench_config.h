/******************************************************************************
 * @file           : bench_config.h
 * @brief          : Build-time switches for the ML fault-classifier
 *                    comparison: which model is compiled in, and whether
 *                    the DWT-cycle-counter inference-latency benchmark runs.
 * @author         : Hossein Baghaei
 * @date           : 2026-08-23
 *
 * @details
 * Two independent things are controlled from this single header, so both are
 * visible in one place before you build:
 *
 *  1. Active model select (fault_models.h / fault_models.c) - exactly one of
 *     MODEL_TREE / MODEL_LOGREG / MODEL_MLP. To measure a different model:
 *     comment out whichever is currently uncommented below, uncomment the
 *     one you want, then rebuild and reflash. One model per build/flash
 *     cycle, matching the "compile each one, flash three times, record the
 *     numbers" comparison plan. If none of the three is uncommented, the
 *     guard below picks MODEL_TREE automatically.
 *
 *  2. ENABLE_BENCH - the one-shot DWT cycle-counter inference-latency
 *     benchmark (Bench_Task in main.c). Independent of ENABLE_SD_LOGGING
 *     (Core/Inc/SD_Log.h): turn SD logging OFF (ENABLE_SD_LOGGING 0) while
 *     ENABLE_BENCH is on, so background SD writes from CSV_Log_Task can't
 *     add scheduling/timing noise to the other tasks - Bench_Task itself
 *     already measures inside a FreeRTOS critical section (interrupts/
 *     preemption disabled for the timed loop only), so SD writes can't land
 *     inside the measurement window either way, but keeping it off removes
 *     one more source of jitter on the run overall and matches how you'll
 *     actually want the board configured for a clean bench session.
 ******************************************************************************/
#ifndef INC_BENCH_CONFIG_H_
#define INC_BENCH_CONFIG_H_

/* ---- 1. Active fault-classifier model select (uncomment exactly one) ---- */
//#define MODEL_TREE
//#define MODEL_LOGREG
 #define MODEL_MLP

#if !defined(MODEL_TREE) && !defined(MODEL_LOGREG) && !defined(MODEL_MLP)
#define MODEL_TREE   /* default: decision tree */
#endif

#if (defined(MODEL_TREE) + defined(MODEL_LOGREG) + defined(MODEL_MLP)) > 1
#error "bench_config.h: exactly one of MODEL_TREE / MODEL_LOGREG / MODEL_MLP may be defined"
#endif

/* ---- 2. Inference latency benchmark (Bench_Task, Core/Src/main.c) ---- */
#define ENABLE_BENCH        1          /* 1 = run the benchmark once at boot, 0 = compiled out entirely */
#define BENCH_ITERATIONS    1000u      /* fault_classify() calls averaged per #BENCH report */
#define BENCH_SYSCLK_HZ     84000000UL /* SYSCLK per SystemClock_Config(): HSI 16MHz/16*336/4 = 84MHz */

#endif /* INC_BENCH_CONFIG_H_ */
