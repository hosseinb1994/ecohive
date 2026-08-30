/******************************************************************************
 * @file           : SD_SPI.h
 * @brief          : Header for a bare-metal SD/MMC-over-SPI block driver
 * @author         : Hossein Baghaei
 * @date           : 22-Jul-2026
 * @version        : 1.0
 *
 * @details
 * Implements the standard SD/MMC "SPI mode" card protocol (as described in
 * the SD Physical Layer Simplified Specification, section 7) on top of the
 * project's existing register-level SPI driver (SPI.c/h) and a manual GPIO
 * chip-select. No HAL, no DMA - single-block synchronous reads/writes only,
 * which is all FatFs needs for this project's append-only CSV logging.
 *
 * This module owns the SPI2 peripheral and PC0 chip-select pin. It is meant
 * to be called only from the FatFs disk I/O glue (Core/Src/diskio.c) - see
 * SD_Log.h for the application-facing logging API.
 ******************************************************************************/
#ifndef INC_SD_SPI_H_
#define INC_SD_SPI_H_

#include <stdint.h>
#include <stdbool.h>

/** SD card type detected during SD_SPI_Init(), needed for block vs byte
 *  addressing further up the stack. */
typedef enum {
    SD_CARD_NONE = 0,   /* not initialized / not detected               */
    SD_CARD_MMC,        /* MMC / SDv1 without high-capacity support     */
    SD_CARD_SDV1,        /* SDSC (byte addressed)                       */
    SD_CARD_SDV2_BLOCK, /* SDHC/SDXC (block addressed)                  */
} SD_CardType;

/**
 * @brief Step-by-step record of the last SD_SPI_Init() attempt, for UART
 *        debug output when bring-up isn't working (bad wiring, dead level
 *        shifter, incompatible/dead card, wrong CS pin, etc).
 */
typedef struct {
    uint8_t cmd0_r1;    /* raw R1 response to CMD0. 0xFF = card never
                         * responded at all (classic "nothing is actually
                         * talking on the bus" signature - check power to
                         * the level shifter, CS wiring, MISO connection).
                         * 0x01 = success (idle). Anything else = the card
                         * responded but rejected/garbled the command. */
    bool    cmd0_ok;    /* cmd0_r1 == 0x01 */
    bool    cmd8_ok;    /* SEND_IF_COND echoed back correctly -> SDv2 */
    bool    acmd41_ok;  /* card left idle via ACMD41 (SD) ...          */
    bool    cmd1_ok;    /* ... or via CMD1 fallback (MMC)              */
    bool    cmd58_ok;   /* OCR read succeeded (only attempted for SDv2) */
} SD_InitDiag;

/**
 * @brief Runs the full SD SPI-mode power-up sequence: >=74 dummy clocks at
 *        low speed, CMD0, CMD8, ACMD41 polling, CMD58 - then switches SPI2
 *        to the higher data-phase clock. All internal waits are bounded
 *        (never hangs forever on a missing/dead card).
 * @return true if a card was detected and is ready for block I/O.
 */
bool SD_SPI_Init(void);

/** @return a snapshot of how far the last SD_SPI_Init() call got - use this
 *  to print a precise UART debug breakdown when init fails. */
SD_InitDiag SD_SPI_GetInitDiag(void);

/** @return the card type detected by the last successful SD_SPI_Init(). */
SD_CardType SD_SPI_GetCardType(void);

/**
 * @brief Reads one 512-byte sector.
 * @param block_addr Sector index (0-based).
 * @param buf        Destination buffer, must be >= 512 bytes.
 */
bool SD_SPI_ReadBlock(uint32_t block_addr, uint8_t *buf);

/**
 * @brief Writes one 512-byte sector.
 * @param block_addr Sector index (0-based).
 * @param buf        Source buffer, must be >= 512 bytes.
 */
bool SD_SPI_WriteBlock(uint32_t block_addr, const uint8_t *buf);

#endif /* INC_SD_SPI_H_ */
