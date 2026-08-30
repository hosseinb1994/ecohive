/******************************************************************************
 * @file           : SD_SPI.c
 * @brief          : Bare-metal SD/MMC-over-SPI block driver implementation
 * @author         : Hossein Baghaei
 * @date           : 22-Jul-2026
 * @version        : 1.0
 *
 * @details
 * Implements the card power-up/identification sequence and single-block
 * read/write for an SD card in SPI mode, per the SD Physical Layer
 * Simplified Specification (section 7, "SPI Mode"). Built entirely on top
 * of the project's own register-level SPI driver (SPI.c/h) - no HAL SPI/SD
 * driver, no DMA.
 *
 * Hardware (see PAPER_CHANGES.md, Part 3, for the full wiring table):
 *   SPI2: SCK=PB10, MISO=PB14, MOSI=PB15 (shared with the now-unused ESP32
 *   link - the ESP32 must be physically disconnected before wiring the SD
 *   module here, they cannot coexist on the bus).
 *   CS:   PC0 (same pin previously used for the ESP32's NSS line).
 *
 * Clock speeds:
 *   Init phase   : requested 400 kHz -> actual 328.125 kHz (APB1 42 MHz / 128),
 *                  safely under the SD spec's <=400 kHz init-phase limit.
 *   Data phase   : requested 10 MHz -> actual 10.5 MHz (APB1 42 MHz / 4).
 *
 * All wait loops are bounded (iteration-counted, matching this project's
 * existing ADC_ReadChannel() convention) - a missing/dead/unresponsive card
 * can never hang the calling task forever.
 ******************************************************************************/
#include "stm32f4xx.h"
#include "SD_SPI.h"
#include "SPI.h"

/* ---------------------------------------------------------------------------
 * Tunables
 * ------------------------------------------------------------------------- */
#define SD_INIT_SPI_HZ          400000U    /* -> actual 328.125 kHz, see header */
#define SD_DATA_SPI_HZ          10000000U  /* -> actual 10.5 MHz, see header    */

#define SD_CMD_RESPONSE_TRIES   16         /* bytes polled waiting for R1        */
#define SD_READY_TRIES          200000U    /* bytes polled waiting for "not busy"*/
#define SD_ACMD41_TRIES         2000U      /* ACMD41 polls waiting to leave idle */
#define SD_DATA_TOKEN_TRIES     200000U    /* bytes polled waiting for data token */
#define SD_WRITE_BUSY_TRIES     500000U    /* bytes polled waiting out write busy */

/* SD SPI-mode command tokens (send as 0x40 | cmd_index) */
#define CMD0    0   /* GO_IDLE_STATE                     */
#define CMD8    8   /* SEND_IF_COND                      */
#define CMD16   16  /* SET_BLOCKLEN                      */
#define CMD17   17  /* READ_SINGLE_BLOCK                 */
#define CMD24   24  /* WRITE_BLOCK                       */
#define CMD55   55  /* APP_CMD (prefixes an ACMD)        */
#define CMD58   58  /* READ_OCR                          */
#define ACMD41  41  /* SD_SEND_OP_COND (after CMD55)     */

#define SD_DATA_TOKEN_START     0xFE /* single-block read/write data token */

/* ---------------------------------------------------------------------------
 * Module state
 * ------------------------------------------------------------------------- */
static SPI_Handle s_sd_spi;
static SD_CardType s_card_type = SD_CARD_NONE;
static SD_InitDiag s_diag = {0};

/* ---------------------------------------------------------------------------
 * CS + low-level byte transfer helpers
 * ------------------------------------------------------------------------- */
static void SD_CS_GPIO_Init(void)
{
    // CS = PC0, push-pull output, idle high (card deselected)
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    GPIOC->MODER  &= ~(3U << (0 * 2));
    GPIOC->MODER  |=  (1U << (0 * 2));   // general-purpose output
    GPIOC->OTYPER &= ~(1U << 0);          // push-pull
    GPIOC->OSPEEDR|=  (3U << (0 * 2));    // high speed
    GPIOC->ODR    |=  (1U << 0);          // CS idle high
}

static inline void SD_CS_Low(void)  { GPIOC->ODR &= ~(1U << 0); }
static inline void SD_CS_High(void) { GPIOC->ODR |=  (1U << 0); }

static uint8_t SD_SPI_Byte(uint8_t out)
{
    uint8_t in = 0xFF;
    SPI_TransmitReceive(&s_sd_spi, &out, &in, 1);
    return in;
}

/** 8 extra clocks with CS high - required after most operations per spec. */
static void SD_ClockOut(void)
{
    (void)SD_SPI_Byte(0xFF);
}

/** Bounded wait for the card to stop pulling MISO busy (0x00 while busy). */
static bool SD_WaitReady(void)
{
    for (uint32_t i = 0; i < SD_READY_TRIES; i++)
    {
        if (SD_SPI_Byte(0xFF) == 0xFF)
        {
            return true;
        }
    }
    return false;
}

/* ---------------------------------------------------------------------------
 * Command layer
 * ------------------------------------------------------------------------- */

/**
 * @brief Sends a command frame and returns the R1 response byte.
 * @param crc Only CMD0 (0x95) and CMD8-with-arg-0x1AA (0x87) need a real
 *            CRC in SPI mode; every other command's CRC field is ignored by
 *            the card unless CRC mode was explicitly enabled (it never is
 *            here), so 0x01 (still sets the required stop bit) is fine.
 */
static uint8_t SD_SendCommand(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    // A command may only be issued once the card has finished any previous
    // busy period.
    SD_WaitReady();

    uint8_t frame[6] = {
        (uint8_t)(0x40 | cmd),
        (uint8_t)(arg >> 24),
        (uint8_t)(arg >> 16),
        (uint8_t)(arg >> 8),
        (uint8_t)(arg),
        crc
    };

    for (int i = 0; i < 6; i++)
    {
        SD_SPI_Byte(frame[i]);
    }

    // R1 is preceded by 0-8 "not ready yet" 0xFF bytes (NCR).
    uint8_t r1 = 0xFF;
    for (uint32_t i = 0; i < SD_CMD_RESPONSE_TRIES; i++)
    {
        r1 = SD_SPI_Byte(0xFF);
        if ((r1 & 0x80) == 0) break; // MSB clear = valid R1
    }
    return r1;
}

/* ---------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */
bool SD_SPI_Init(void)
{
    s_card_type = SD_CARD_NONE;
    s_diag = (SD_InitDiag){0};
    s_diag.cmd0_r1 = 0xFF;

    SD_CS_GPIO_Init();

    // Bring SPI2 up at the low, spec-mandated init speed first.
    SPI_Config low_speed_cfg = {
        .spi_frequency = SD_INIT_SPI_HZ,
        .data_size     = 8,
        .lsb_first     = false,
        .cpol          = false,   // SD SPI mode 0
        .cpha          = false,
        .crc_enable    = false,   // SD's own CRC7/CRC16, not the SPI peripheral's
        .crc_polynomial = 0
    };
    SPI_Init(&s_sd_spi, SPI2, &low_speed_cfg);

    // >=74 dummy clocks with CS high so the card boots into SPI mode.
    SD_CS_High();
    for (int i = 0; i < 10; i++)
    {
        SD_SPI_Byte(0xFF);
    }

    // CMD0: GO_IDLE_STATE - must get R1 == 0x01 (idle, no errors).
    SD_CS_Low();
    uint8_t r1 = SD_SendCommand(CMD0, 0, 0x95);
    s_diag.cmd0_r1 = r1;
    s_diag.cmd0_ok = (r1 == 0x01);
    if (!s_diag.cmd0_ok)
    {
        SD_CS_High();
        SD_ClockOut();
        return false; // no card, or card not responding
    }

    // CMD8: SEND_IF_COND - distinguishes SDv2 (high-capacity capable) from
    // SDv1/MMC. Echo pattern 0x1AA must come back unchanged in the R7 body.
    bool is_v2 = false;
    r1 = SD_SendCommand(CMD8, 0x1AA, 0x87);
    if (r1 == 0x01)
    {
        uint8_t r7[4];
        for (int i = 0; i < 4; i++) r7[i] = SD_SPI_Byte(0xFF);
        if (r7[2] == 0x01 && r7[3] == 0xAA)
        {
            is_v2 = true;
        }
    }
    s_diag.cmd8_ok = is_v2;

    // ACMD41 (CMD55 + CMD41): poll until the card leaves idle state.
    // HCS (bit 30 of the argument) tells an SDv2 card that the host supports
    // high-capacity (SDHC/SDXC) cards.
    uint32_t acmd41_arg = is_v2 ? (1UL << 30) : 0;
    bool left_idle = false;
    for (uint32_t i = 0; i < SD_ACMD41_TRIES; i++)
    {
        SD_SendCommand(CMD55, 0, 0x01);
        r1 = SD_SendCommand(ACMD41, acmd41_arg, 0x01);
        if (r1 == 0x00) { left_idle = true; break; }
        if (r1 & 0xFE) break; // any error bit other than "still idle" -> give up
    }
    s_diag.acmd41_ok = left_idle;

    if (!left_idle)
    {
        // Not an SD card (or one that rejected ACMD41) - try plain MMC CMD1.
        bool mmc_ready = false;
        for (uint32_t i = 0; i < SD_ACMD41_TRIES; i++)
        {
            r1 = SD_SendCommand(1, 0, 0x01);
            if (r1 == 0x00) { mmc_ready = true; break; }
            if (r1 & 0xFE) break;
        }
        s_diag.cmd1_ok = mmc_ready;
        if (!mmc_ready)
        {
            SD_CS_High();
            SD_ClockOut();
            return false;
        }
        s_card_type = SD_CARD_MMC;
    }
    else if (is_v2)
    {
        // CMD58: READ_OCR - CCS bit (bit 30 of the OCR) tells us whether the
        // card uses block addressing (SDHC/SDXC) or byte addressing (SDSC).
        r1 = SD_SendCommand(CMD58, 0, 0x01);
        uint8_t ocr[4] = {0, 0, 0, 0};
        for (int i = 0; i < 4; i++) ocr[i] = SD_SPI_Byte(0xFF);
        s_diag.cmd58_ok = (r1 == 0x00);
        if (r1 == 0x00 && (ocr[0] & 0x40))
        {
            s_card_type = SD_CARD_SDV2_BLOCK;
        }
        else
        {
            s_card_type = SD_CARD_SDV1;
        }
    }
    else
    {
        s_card_type = SD_CARD_SDV1;
    }

    // Byte-addressed cards (SDSC/MMC) must be told the block length; SDHC/
    // SDXC ignore CMD16 and are always fixed at 512 bytes/block.
    if (s_card_type != SD_CARD_SDV2_BLOCK)
    {
        SD_SendCommand(CMD16, 512, 0x01);
    }

    SD_CS_High();
    SD_ClockOut();

    // Data phase: switch SPI2 up to the higher clock now that the card is
    // identified and configured.
    SPI_Config high_speed_cfg = low_speed_cfg;
    high_speed_cfg.spi_frequency = SD_DATA_SPI_HZ;
    SPI_Init(&s_sd_spi, SPI2, &high_speed_cfg);

    return true;
}

SD_CardType SD_SPI_GetCardType(void)
{
    return s_card_type;
}

SD_InitDiag SD_SPI_GetInitDiag(void)
{
    return s_diag;
}

bool SD_SPI_ReadBlock(uint32_t block_addr, uint8_t *buf)
{
    if (s_card_type == SD_CARD_NONE || buf == NULL) return false;

    // Byte-addressed cards take a byte offset; block-addressed cards take
    // the block number directly.
    uint32_t arg = (s_card_type == SD_CARD_SDV2_BLOCK) ? block_addr : (block_addr * 512U);

    SD_CS_Low();
    uint8_t r1 = SD_SendCommand(CMD17, arg, 0x01);
    if (r1 != 0x00)
    {
        SD_CS_High();
        SD_ClockOut();
        return false;
    }

    // Wait for the data start token.
    uint8_t token = 0xFF;
    for (uint32_t i = 0; i < SD_DATA_TOKEN_TRIES; i++)
    {
        token = SD_SPI_Byte(0xFF);
        if (token != 0xFF) break;
    }
    if (token != SD_DATA_TOKEN_START)
    {
        SD_CS_High();
        SD_ClockOut();
        return false;
    }

    for (uint32_t i = 0; i < 512U; i++)
    {
        buf[i] = SD_SPI_Byte(0xFF);
    }
    SD_SPI_Byte(0xFF); // CRC16 (2 bytes) - discarded, CRC checking is off
    SD_SPI_Byte(0xFF);

    SD_CS_High();
    SD_ClockOut();
    return true;
}

bool SD_SPI_WriteBlock(uint32_t block_addr, const uint8_t *buf)
{
    if (s_card_type == SD_CARD_NONE || buf == NULL) return false;

    uint32_t arg = (s_card_type == SD_CARD_SDV2_BLOCK) ? block_addr : (block_addr * 512U);

    SD_CS_Low();
    uint8_t r1 = SD_SendCommand(CMD24, arg, 0x01);
    if (r1 != 0x00)
    {
        SD_CS_High();
        SD_ClockOut();
        return false;
    }

    SD_SPI_Byte(SD_DATA_TOKEN_START);
    for (uint32_t i = 0; i < 512U; i++)
    {
        SD_SPI_Byte(buf[i]);
    }
    SD_SPI_Byte(0xFF); // dummy CRC16 - card ignores it, CRC checking is off
    SD_SPI_Byte(0xFF);

    uint8_t data_resp = SD_SPI_Byte(0xFF);
    if ((data_resp & 0x1F) != 0x05)
    {
        // Card rejected the data (CRC or write error)
        SD_CS_High();
        SD_ClockOut();
        return false;
    }

    // Programming takes a while on cheap cards - bounded wait for "not busy".
    bool ready = false;
    for (uint32_t i = 0; i < SD_WRITE_BUSY_TRIES; i++)
    {
        if (SD_SPI_Byte(0xFF) == 0xFF) { ready = true; break; }
    }

    SD_CS_High();
    SD_ClockOut();
    return ready;
}
