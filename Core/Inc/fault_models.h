/* fault_models.h - auto-generated on-device fault classifiers
 * Compares three lightweight models for MQ-9 fault detection.
 * Select ONE model at compile time via Core/Inc/bench_config.h:
 *     MODEL_TREE      // decision tree (9 nodes, if-based)
 *     MODEL_LOGREG    // logistic regression (5x5)
 *     MODEL_MLP       // MLP 5-8-8-5, float
 * Input feature vector order: {raw, ppm, mcu_t, am_t, hum}
 * Output: class index 0..4
 *   0=disconnect 1=gnd 2=healthy 3=stuck 4=v33
 */
#ifndef FAULT_MODELS_H
#define FAULT_MODELS_H

/* Picks the active MODEL_TREE / MODEL_LOGREG / MODEL_MLP define (defaults to
 * MODEL_TREE) and the ENABLE_BENCH / BENCH_ITERATIONS / BENCH_SYSCLK_HZ
 * benchmark switches. Included here so every TU that includes fault_models.h
 * (fault_models.c, main.c) sees the same selection. */
#include "bench_config.h"

#define FM_N_FEATURES 5
#define FM_N_CLASSES  5

/* class index -> name (for UART printing) */
static const char* const FM_CLASS_NAMES[FM_N_CLASSES] = {
    "disconnect", "gnd", "healthy", "stuck", "v33"
};

/* Active model's name (for UART printing, e.g. in the #BENCH line) */
#if defined(MODEL_TREE)
#define FM_MODEL_NAME "MODEL_TREE"
#elif defined(MODEL_LOGREG)
#define FM_MODEL_NAME "MODEL_LOGREG"
#elif defined(MODEL_MLP)
#define FM_MODEL_NAME "MODEL_MLP"
#endif

int fault_classify(const float x[FM_N_FEATURES]);

#endif