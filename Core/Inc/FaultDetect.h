/******************************************************************************
 * @file           : FaultDetect.h
 * @brief          : Header file for lightweight on-MCU MQ-9 fault detection
 * @author         : Hossein Baghaei
 * @date           : 22-Jul-2026
 * @version        : 1.0
 *
 * @details
 * This module runs a set of simple, deterministic rule-based checks against
 * each MQ-9 measurement cycle and reports the result as a one-byte bitfield
 * ("quality_flags"). It is intentionally bare-metal: no HAL calls, no
 * dynamic memory, no floating-point statistics library. It keeps a small
 * fixed-size ring buffer of raw ADC samples plus running min/max, and is
 * meant to be called once per MQ-9 measurement (i.e. once per MQ9_Task
 * loop iteration).
 *
 * Bit assignment of the returned quality_flags byte:
 *   bit0 (0x01) - STUCK      : raw ADC has not moved beyond noise for N reads
 *   bit1 (0x02) - SATURATED  : raw ADC at/near the 0..4095 rail
 *   bit2 (0x04) - RANGE      : computed ppm is negative or above datasheet max
 *   bit3 (0x08) - RATE       : ppm changed faster than physically plausible
 *
 * All other bits are reserved and always read back as 0.
 ******************************************************************************/
#ifndef INC_FAULTDETECT_H_
#define INC_FAULTDETECT_H_

#include <stdint.h>

/* Bitfield positions returned by FaultDetect_Update() */
#define FAULT_STUCK_BIT      (1u << 0)
#define FAULT_SATURATED_BIT  (1u << 1)
#define FAULT_RANGE_BIT      (1u << 2)
#define FAULT_RATE_BIT       (1u << 3)

/**
 * @brief Resets the ring buffer and history. Call once before the first
 *        FaultDetect_Update() call (e.g. at the top of MQ9_Task).
 */
void FaultDetect_Init(void);

/**
 * @brief Runs all four MQ-9 fault checks for one measurement cycle.
 * @param raw_adc Raw 12-bit ADC reading for the MQ-9 analog output this cycle.
 * @param ppm     Computed ppm value for this cycle. MQ9_GetPPM() is written
 *                to always return a finite number (never NaN/Inf), using a
 *                defined negative sentinel for invalid readings instead -
 *                the !isfinite() check here is a defensive backstop only.
 * @return quality_flags bitfield (see bit definitions above).
 */
uint8_t FaultDetect_Update(uint16_t raw_adc, float ppm);

/**
 * @brief Optional diagnostics accessor for the current ring buffer window.
 *        Useful for printing "#"-prefixed debug lines while tuning
 *        thresholds; does not affect FaultDetect_Update()'s decisions.
 */
void FaultDetect_GetStats(uint16_t *min_out, uint16_t *max_out, float *variance_out);

#endif /* INC_FAULTDETECT_H_ */
