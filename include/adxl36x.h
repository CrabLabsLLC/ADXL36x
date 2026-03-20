/* Copyright (c) 2025 Crab Labs LLC */
/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file adxl36x.h
 * @brief ADXL36x accelerometer driver - public API
 *
 * Zephyr device driver for ADXL362/363/366/367 ultralow-power accelerometers.
 * Supports both low-level register access and high-level streaming with
 * automatic FIFO drain and internal sample caching on interrupt.
 *
 * @author Orion Serup <orion@crablabs.io>
 * @author Claude <noreply@anthropic.com>
 *
 * @reviewer
 */

#pragma once
#ifndef ADXL36X_H
#define ADXL36X_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>

#include "adxl36x_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/** @brief Output data rate codes (FILTER_CTL[2:0]) */
typedef enum
{
	ADXL36X_ODR_12P5HZ = 0,   ///< 12.5 Hz
	ADXL36X_ODR_25HZ   = 1,   ///< 25 Hz
	ADXL36X_ODR_50HZ   = 2,   ///< 50 Hz
	ADXL36X_ODR_100HZ  = 3,   ///< 100 Hz
	ADXL36X_ODR_200HZ  = 4,   ///< 200 Hz
	ADXL36X_ODR_400HZ  = 5,   ///< 400 Hz
} ADXL36xODR;

/** @brief Measurement range codes (FILTER_CTL[7:6]) */
typedef enum
{
	ADXL36X_RANGE_2G = 0,   ///< +/- 2g
	ADXL36X_RANGE_4G = 1,   ///< +/- 4g
	ADXL36X_RANGE_8G = 2,   ///< +/- 8g
} ADXL36xRange;

/** @brief FIFO operating mode (FIFO_CONTROL[1:0]) */
typedef enum
{
	ADXL36X_FIFO_DISABLED  = 0,   ///< FIFO disabled
	ADXL36X_FIFO_OLDEST    = 1,   ///< Oldest saved mode
	ADXL36X_FIFO_STREAM    = 2,   ///< Stream mode (continuous)
	ADXL36X_FIFO_TRIGGERED = 3,   ///< Triggered mode
} ADXL36xFIFOMode;

/** @brief FIFO data format / axis selection (FIFO_CONTROL[6:3]) */
typedef enum
{
	ADXL36X_FIFO_FORMAT_XYZ = 0,   ///< All three axes
	ADXL36X_FIFO_FORMAT_X   = 1,   ///< X-axis only
	ADXL36X_FIFO_FORMAT_Y   = 2,   ///< Y-axis only
	ADXL36X_FIFO_FORMAT_XY  = 3,   ///< X and Y axes
} ADXL36xFIFOFormat;

/** @brief Noise mode selection (POWER_CTL[5:4]) */
typedef enum
{
	ADXL36X_NOISE_SEL_NORMAL    = 0,  ///< Normal operation
	ADXL36X_NOISE_SEL_LOW_NOISE = 1,  ///< Low noise mode
} ADXL36xNoiseMode;

/** @brief Link/loop mode (ACT_INACT_CTL[5:4]) */
typedef enum
{
	ADXL36X_LINK_LOOP_DEFAULT   = 0,  ///< Default (no link/loop)
	ADXL36X_LINK_LOOP_LINKED    = 1,  ///< Linked mode
	ADXL36X_LINK_LOOP_LOOP_HOST = 2,  ///< Loop mode (host acknowledge)
	ADXL36X_LINK_LOOP_LOOP_AUTO = 3,  ///< Loop mode (auto acknowledge)
} ADXL36xLinkLoopMode;

/** @brief Wake-up sampling rate (TIMER_CTL[7:6]) */
typedef enum
{
	ADXL36X_WAKEUP_SEL_12_SPS  = 0,  ///< 12 sps (80 ms period)
	ADXL36X_WAKEUP_SEL_6_SPS   = 1,  ///< 6 sps (160 ms period)
	ADXL36X_WAKEUP_SEL_3_SPS   = 2,  ///< 3 sps (320 ms period)
	ADXL36X_WAKEUP_SEL_1P5_SPS = 3,  ///< 1.5 sps (640 ms period)
} ADXL36xWakeupRate;

/** @brief Tap detection axis selection */
typedef enum
{
	ADXL36X_TAP_SEL_X = 0,  ///< X-axis
	ADXL36X_TAP_SEL_Y = 1,  ///< Y-axis
	ADXL36X_TAP_SEL_Z = 2,  ///< Z-axis
} ADXL36xTapAxis;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/** @brief FIFO configuration */
typedef struct
{
	ADXL36xFIFOMode mode;       ///< FIFO operating mode
	ADXL36xFIFOFormat format;   ///< Axis selection
	uint16_t watermark;         ///< Watermark level in individual axis samples
} ADXL36xFIFOConfig;

/** @brief Activity/inactivity detection configuration */
typedef struct
{
	uint16_t threshold;     ///< Raw threshold value (13-bit, 0-8191)
	uint8_t time_periods;   ///< Time in ODR periods
	bool is_referenced;     ///< Referenced vs absolute mode
	bool is_enabled;        ///< Enable detection
} ADXL36xActivityConfig;

/** @brief Tap detection configuration */
typedef struct
{
	uint8_t threshold;       ///< Tap threshold (0-255, 31.25 mg/LSB in 8g range)
	uint8_t duration;        ///< Max tap duration (0-255, 625 us/LSB)
	uint8_t latency;         ///< Double-tap latency (0-255, 1.25 ms/LSB; 0 disables double-tap)
	uint8_t window;          ///< Double-tap window (0-255, 1.25 ms/LSB)
	ADXL36xTapAxis axis;     ///< Axis for tap detection
} ADXL36xTapConfig;

/** @brief Temperature/ADC threshold configuration */
typedef struct
{
	uint16_t over_threshold;   ///< Over-limit threshold (13-bit, 0-8191)
	uint16_t under_threshold;  ///< Under-limit threshold (13-bit, 0-8191)
	uint8_t act_timer;         ///< Activity timer (4-bit, 0-15)
	uint8_t inact_timer;       ///< Inactivity timer (4-bit, 0-15)
} ADXL36xTempADCThreshold;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/** @brief Raw accelerometer sample (14-bit signed per axis) */
typedef struct
{
	int16_t x;   ///< X-axis (14-bit signed, sign-extended to int16_t)
	int16_t y;   ///< Y-axis
	int16_t z;   ///< Z-axis
} ADXL36xSample;

/* ============================================================================
 * Interrupt Source Masks (for adxl36xSetInterrupt1Sources)
 * ============================================================================ */

#define ADXL36X_INTERRUPT_SRC_FIFO_WATERMARK  ADXL36X_INT_FIFO_WATERMARK
#define ADXL36X_INTERRUPT_SRC_FIFO_FULL       ADXL36X_INT_FIFO_OVERRUN
#define ADXL36X_INTERRUPT_SRC_DATA_READY      ADXL36X_INT_DATA_RDY
#define ADXL36X_INTERRUPT_SRC_ACT             ADXL36X_INT_ACT
#define ADXL36X_INTERRUPT_SRC_INACT           ADXL36X_INT_INACT
#define ADXL36X_INTERRUPT_SRC_AWAKE           ADXL36X_INT_AWAKE
#define ADXL36X_INTERRUPT_SRC_FIFO_RDY        ADXL36X_INT_FIFO_RDY

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Raw interrupt callback (called from ISR context)
 *
 * @param[in] dev       Zephyr device handle
 * @param[in] user_data Context pointer registered with the callback
 */
typedef void (*ADXL36xInterruptCallback)(const struct device* const dev, void* const user_data);

/**
 * @brief Data-ready callback (called from work queue context)
 *
 * Invoked after the driver auto-fetches FIFO data and caches parsed
 * samples internally. The sample_count parameter is the total number
 * of cached samples available for reading.
 *
 * @param[in] dev          Zephyr device handle
 * @param[in] sample_count Total cached samples available
 * @param[in] user_data    Context pointer registered with the callback
 */
typedef void (*ADXL36xDataCallback)(const struct device* const dev, const uint16_t sample_count, void* const user_data);

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/**
 * @brief Soft-reset the device and wait for completion
 *
 * Writes the reset code to SOFT_RESET register and waits 0.5ms.
 *
 * @param[in] dev Zephyr device handle
 * @return 0 on success, negative errno on failure
 */
int adxl36xReset(const struct device* const dev);

/**
 * @brief Enter measurement mode
 *
 * Sets POWER_CTL measurement bits to measurement mode.
 *
 * @param[in] dev Zephyr device handle
 * @return 0 on success, negative errno on failure
 */
int adxl36xEnable(const struct device* const dev);

/**
 * @brief Enter standby mode (low-power idle)
 *
 * Sets POWER_CTL measurement bits to standby.
 *
 * @param[in] dev Zephyr device handle
 * @return 0 on success, negative errno on failure
 */
int adxl36xDisable(const struct device* const dev);

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Set the output data rate
 *
 * @param[in] dev Zephyr device handle
 * @param[in] odr Desired output data rate
 * @return 0 on success, -EINVAL for invalid ODR, negative errno
 */
int adxl36xSetODR(const struct device* const dev, const ADXL36xODR odr);

/**
 * @brief Set the measurement range
 *
 * Also updates internal range tracking for sensor API conversions.
 *
 * @param[in] dev   Zephyr device handle
 * @param[in] range Desired measurement range
 * @return 0 on success, -EINVAL for invalid range, negative errno
 */
int adxl36xSetRange(const struct device* const dev, const ADXL36xRange range);

/**
 * @brief Configure FIFO mode and watermark
 *
 * Writes FIFO_CONTROL and FIFO_SAMPLES registers. Watermark value
 * is clamped to 0x1FF (max 511 individual axis samples).
 *
 * @param[in] dev    Zephyr device handle
 * @param[in] config FIFO configuration. Must not be NULL.
 * @return 0 on success, negative errno on failure
 */
int adxl36xSetFIFOConfig(const struct device* const dev, const ADXL36xFIFOConfig* const config);

/**
 * @brief Configure activity detection
 *
 * Sets threshold, time, and referenced/absolute mode for activity detection.
 *
 * @param[in] dev    Zephyr device handle
 * @param[in] config Activity configuration. Must not be NULL.
 * @return 0 on success, negative errno on failure
 */
int adxl36xSetActivity(const struct device* const dev, const ADXL36xActivityConfig* const config);

/**
 * @brief Get current measurement range
 *
 * Returns the cached range value (last set via adxl36xSetRange or init).
 *
 * @param[in] dev Zephyr device handle
 * @return Current range setting
 */
ADXL36xRange adxl36xGetRange(const struct device* const dev);

/**
 * @brief Get current output data rate
 *
 * Returns the cached ODR value (last set via adxl36xSetODR or init).
 *
 * @param[in] dev Zephyr device handle
 * @return Current ODR setting
 */
ADXL36xODR adxl36xGetODR(const struct device* const dev);

/* ============================================================================
 * Data Acquisition - Direct Register Read
 * ============================================================================ */

/**
 * @brief Get the number of FIFO entries available
 *
 * Reads FIFO_ENTRIES_L and FIFO_ENTRIES_H (10-bit value, max 512).
 *
 * @param[in]  dev     Zephyr device handle
 * @param[out] entries Output: available FIFO entries. Must not be NULL.
 * @return 0 on success, negative errno on failure
 */
int adxl36xGetFIFOEntries(const struct device* const dev, uint16_t* const entries);

/**
 * @brief Read raw FIFO data bytes via I2C FIFO port
 *
 * Burst-reads from the I2C_FIFO_DATA register (0x18).
 *
 * @param[in]  dev        Zephyr device handle
 * @param[out] buffer     Destination buffer. Must not be NULL.
 * @param[in]  byte_count Number of bytes to read
 * @return 0 on success, negative errno on failure
 */
int adxl36xReadFIFO(const struct device* const dev, uint8_t* const buffer, const size_t byte_count);

/**
 * @brief Read 14-bit XYZ from data registers (not FIFO)
 *
 * Reads X_DATA_H/L, Y_DATA_H/L, Z_DATA_H/L in a single burst.
 * Any output pointer may be NULL if that axis is not needed.
 * Also updates internal cached values for Zephyr sensor API.
 *
 * @param[in]  dev Zephyr device handle
 * @param[out] x   Output: raw 14-bit signed X. May be NULL.
 * @param[out] y   Output: raw 14-bit signed Y. May be NULL.
 * @param[out] z   Output: raw 14-bit signed Z. May be NULL.
 * @return 0 on success, negative errno on failure
 */
int adxl36xReadXYZ(const struct device* const dev, int16_t* const x, int16_t* const y, int16_t* const z);

/**
 * @brief Read STATUS register
 *
 * @param[in]  dev    Zephyr device handle
 * @param[out] status Output: STATUS register byte. Must not be NULL.
 * @return 0 on success, negative errno on failure
 */
int adxl36xReadStatus(const struct device* const dev, uint8_t* const status);

/**
 * @brief Read Part ID register
 *
 * @param[in]  dev     Zephyr device handle
 * @param[out] part_id Output: Part ID byte. Must not be NULL.
 * @return 0 on success, negative errno on failure
 */
int adxl36xGetPartID(const struct device* const dev, uint8_t* const part_id);

/**
 * @brief Read Revision ID register
 *
 * @param[in]  dev    Zephyr device handle
 * @param[out] rev_id Output: Revision ID byte. Must not be NULL.
 * @return 0 on success, negative errno on failure
 */
int adxl36xGetRevisionID(const struct device* const dev, uint8_t* const rev_id);

/* ============================================================================
 * Streaming - Auto-Fetch with Internal Cache
 *
 * High-level API: the driver configures FIFO, enables measurement,
 * enables IRQ, and auto-fetches + caches parsed samples on each
 * watermark interrupt. Application reads from the cache.
 *
 * Requires: work queue attached via adxl36xAttachWorkQueue().
 * Requires: CONFIG_ADXL36X_IRQ_ENABLE=y
 *
 * While streaming is active, do not call adxl36xEnable/Disable or
 * adxl36xSetFIFOConfig directly - use adxl36xStopStreaming() first.
 * ============================================================================ */

/**
 * @brief Attach a work queue for deferred data processing
 *
 * Must be called before adxl36xStartStreaming(). The driver submits
 * FIFO drain work to this queue on each interrupt.
 *
 * @param[in] dev   Zephyr device handle
 * @param[in] workq Work queue for deferred I/O. Must not be NULL.
 * @return 0 on success, negative errno on failure
 */
int adxl36xAttachWorkQueue(const struct device* const dev, struct k_work_q* const workq);

/**
 * @brief Start streaming with automatic FIFO drain
 *
 * Configures FIFO, enables measurement, maps FIFO watermark to INT1,
 * and enables the GPIO interrupt. On each watermark IRQ, the driver
 * auto-fetches FIFO data, parses 14-bit samples, and caches them.
 *
 * @param[in] dev    Zephyr device handle
 * @param[in] config FIFO configuration. Must not be NULL.
 * @return 0 on success, -EINVAL if no work queue attached, negative errno
 */
int adxl36xStartStreaming(const struct device* const dev, const ADXL36xFIFOConfig* const config);

/**
 * @brief Stop streaming and disable interrupts
 *
 * Disables GPIO interrupt, cancels pending fetch work, enters standby,
 * and disables FIFO. Cached samples are preserved until flushed.
 *
 * @param[in] dev Zephyr device handle
 * @return 0 on success, negative errno on failure
 */
int adxl36xStopStreaming(const struct device* const dev);

/**
 * @brief Check if streaming is active
 *
 * @param[in] dev Zephyr device handle
 * @return true if streaming is active
 */
bool adxl36xIsStreaming(const struct device* const dev);

/**
 * @brief Get number of cached samples available
 *
 * @param[in] dev Zephyr device handle
 * @return Number of cached XYZ sample triplets
 */
uint16_t adxl36xGetCachedSampleCount(const struct device* const dev);

/**
 * @brief Read samples from internal cache
 *
 * Copies up to max_count samples from the cache and removes them.
 * Safe to call from the data callback context.
 *
 * @param[in]  dev       Zephyr device handle
 * @param[out] buf       Output buffer. Must not be NULL.
 * @param[in]  max_count Maximum samples to read
 * @return Number of samples actually read (0 to max_count)
 */
int adxl36xGetCachedSamples(const struct device* const dev, ADXL36xSample* const buf, const size_t max_count);

/**
 * @brief Flush internal sample cache
 *
 * Resets the cache to empty. Does not affect hardware FIFO.
 *
 * @param[in] dev Zephyr device handle
 */
void adxl36xFlushCache(const struct device* const dev);

/**
 * @brief Register data-ready callback
 *
 * Called from work queue context (not ISR) after the driver caches
 * new FIFO samples. Pass NULL to disable.
 *
 * @param[in] dev       Zephyr device handle
 * @param[in] callback  Function to call, or NULL to disable
 * @param[in] user_data Context pointer passed to callback
 * @return 0 on success, negative errno on failure
 */
int adxl36xSetDataCallback(const struct device* const dev, const ADXL36xDataCallback callback, void* const user_data);

/* ============================================================================
 * Interrupt Management
 * ============================================================================ */

/**
 * @brief Register raw interrupt callback (ISR context)
 *
 * Called from ISR context on each INT1 edge. For streaming use,
 * prefer adxl36xSetDataCallback() which fires from work queue
 * context after data is cached.
 *
 * @note When streaming is active, the driver auto-submits fetch work
 *       on each IRQ regardless of whether this callback is registered.
 *
 * @param[in] dev       Zephyr device handle
 * @param[in] callback  Function to call from ISR, or NULL to disable
 * @param[in] user_data Context pointer
 * @return 0 on success, -ENOTSUP if IRQ not enabled, negative errno
 */
int adxl36xSetInterruptCallback(const struct device* const dev, const ADXL36xInterruptCallback callback, void* const user_data);

/**
 * @brief Set INT1 interrupt source mask and polarity
 *
 * Writes INTMAP1_LOWER register.
 *
 * @param[in] dev      Zephyr device handle
 * @param[in] sources  Bitmask of ADXL36X_INTERRUPT_SRC_* flags
 * @param[in] polarity Nonzero for active-low
 * @return 0 on success, negative errno on failure
 */
int adxl36xSetInterrupt1Sources(const struct device* const dev, const uint8_t sources, const uint8_t polarity);

/**
 * @brief Enable or disable INT1 GPIO interrupt
 *
 * @param[in] dev        Zephyr device handle
 * @param[in] is_enabled True to enable, false to disable
 * @return 0 on success, -ENOTSUP if no INT GPIO, negative errno
 */
int adxl36xEnableInterrupt1(const struct device* const dev, const bool is_enabled);

/**
 * @brief Check if interrupt GPIO is configured
 *
 * @param[in] dev Zephyr device handle
 * @return true if INT1 GPIO is available
 */
bool adxl36xHasInterruptGPIO(const struct device* const dev);

/* ============================================================================
 * Sample Counting
 * ============================================================================ */

/**
 * @brief Get interrupt event count since last reset
 *
 * @param[in] dev Zephyr device handle
 * @return Number of interrupt events
 */
uint32_t adxl36xGetSampleCount(const struct device* const dev);

/**
 * @brief Reset interrupt counter to zero
 *
 * @param[in] dev Zephyr device handle
 */
void adxl36xResetSampleCount(const struct device* const dev);

/**
 * @brief Get and atomically reset interrupt counter
 *
 * @param[in] dev Zephyr device handle
 * @return Count before reset
 */
uint32_t adxl36xGetAndResetSampleCount(const struct device* const dev);

/* ============================================================================
 * Inactivity Detection
 * ============================================================================ */

/**
 * @brief Configure inactivity detection
 *
 * Sets threshold, time, and referenced/absolute mode for inactivity detection.
 * Time is a 16-bit value written big-endian to TIME_INACT_H/L.
 *
 * @param[in] dev    Zephyr device handle
 * @param[in] config Inactivity configuration. Must not be NULL.
 * @return 0 on success, negative errno on failure
 */
int adxl36xSetInactivity(const struct device* const dev, const ADXL36xActivityConfig* const config);

/* ============================================================================
 * Tap Detection
 * ============================================================================ */

/**
 * @brief Configure tap detection
 *
 * Sets tap threshold, duration, latency, window, and axis.
 * Setting latency to 0 disables double-tap detection.
 *
 * @param[in] dev    Zephyr device handle
 * @param[in] config Tap configuration. Must not be NULL.
 * @return 0 on success, negative errno on failure
 */
int adxl36xSetTapConfig(const struct device* const dev, const ADXL36xTapConfig* const config);

/* ============================================================================
 * Offset Trim
 * ============================================================================ */

/**
 * @brief Set user offset trim for all axes
 *
 * 5-bit two's complement values, 15 mg/LSB. Valid range: -16 to +15.
 *
 * @param[in] dev  Zephyr device handle
 * @param[in] x_mg X-axis offset in raw LSB units (-16 to +15)
 * @param[in] y_mg Y-axis offset in raw LSB units (-16 to +15)
 * @param[in] z_mg Z-axis offset in raw LSB units (-16 to +15)
 * @return 0 on success, negative errno on failure
 */
int adxl36xSetOffset(const struct device* const dev, const int8_t x_mg, const int8_t y_mg, const int8_t z_mg);

/**
 * @brief Get current user offset trim for all axes
 *
 * Reads and sign-extends the 5-bit values to int8_t.
 *
 * @param[in]  dev  Zephyr device handle
 * @param[out] x_mg X-axis offset. Must not be NULL.
 * @param[out] y_mg Y-axis offset. Must not be NULL.
 * @param[out] z_mg Z-axis offset. Must not be NULL.
 * @return 0 on success, negative errno on failure
 */
int adxl36xGetOffset(const struct device* const dev, int8_t* const x_mg, int8_t* const y_mg, int8_t* const z_mg);

/* ============================================================================
 * Sensitivity Trim
 * ============================================================================ */

/**
 * @brief Set user sensitivity trim for all axes
 *
 * 6-bit two's complement values, 1.56%/LSB. Valid range: -32 to +31.
 *
 * @param[in] dev   Zephyr device handle
 * @param[in] x_pct X-axis sensitivity in raw LSB units (-32 to +31)
 * @param[in] y_pct Y-axis sensitivity in raw LSB units (-32 to +31)
 * @param[in] z_pct Z-axis sensitivity in raw LSB units (-32 to +31)
 * @return 0 on success, negative errno on failure
 */
int adxl36xSetSensitivity(const struct device* const dev, const int8_t x_pct, const int8_t y_pct, const int8_t z_pct);

/**
 * @brief Get current user sensitivity trim for all axes
 *
 * Reads and sign-extends the 6-bit values to int8_t.
 *
 * @param[in]  dev   Zephyr device handle
 * @param[out] x_pct X-axis sensitivity. Must not be NULL.
 * @param[out] y_pct Y-axis sensitivity. Must not be NULL.
 * @param[out] z_pct Z-axis sensitivity. Must not be NULL.
 * @return 0 on success, negative errno on failure
 */
int adxl36xGetSensitivity(const struct device* const dev, int8_t* const x_pct, int8_t* const y_pct, int8_t* const z_pct);

/* ============================================================================
 * Temperature
 * ============================================================================ */

/**
 * @brief Read raw temperature value
 *
 * 14-bit signed value from TEMP_H/TEMP_L registers, sign-extended to int16_t.
 *
 * @param[in]  dev Zephyr device handle
 * @param[out] raw Output: raw 14-bit temperature value. Must not be NULL.
 * @return 0 on success, negative errno on failure
 */
int adxl36xReadTempRaw(const struct device* const dev, int16_t* const raw);

/**
 * @brief Read temperature in degrees Celsius
 *
 * Converts raw reading using: temp = 25.0 + (raw - bias) / sensitivity.
 *
 * @param[in]  dev    Zephyr device handle
 * @param[out] temp_c Output: temperature in degrees Celsius. Must not be NULL.
 * @return 0 on success, negative errno on failure
 */
int adxl36xReadTempC(const struct device* const dev, float* const temp_c);

/**
 * @brief Enable or disable the on-chip temperature sensor
 *
 * @param[in] dev        Zephyr device handle
 * @param[in] is_enabled True to enable, false to disable
 * @return 0 on success, negative errno on failure
 */
int adxl36xEnableTemp(const struct device* const dev, const bool is_enabled);

/* ============================================================================
 * External ADC
 * ============================================================================ */

/**
 * @brief Read raw external ADC value
 *
 * 14-bit signed value from EX_ADC_H/EX_ADC_L registers, sign-extended to int16_t.
 *
 * @param[in]  dev Zephyr device handle
 * @param[out] raw Output: raw 14-bit ADC value. Must not be NULL.
 * @return 0 on success, negative errno on failure
 */
int adxl36xReadADCRaw(const struct device* const dev, int16_t* const raw);

/**
 * @brief Enable or disable the external ADC channel
 *
 * @param[in] dev        Zephyr device handle
 * @param[in] is_enabled True to enable, false to disable
 * @return 0 on success, negative errno on failure
 */
int adxl36xEnableADC(const struct device* const dev, const bool is_enabled);

/* ============================================================================
 * Self-Test
 * ============================================================================ */

/**
 * @brief Enable or disable self-test mode
 *
 * When enabled, applies an electrostatic force to the sensor to
 * verify mechanical/electrical integrity.
 *
 * @param[in] dev        Zephyr device handle
 * @param[in] is_enabled True to enable, false to disable
 * @return 0 on success, negative errno on failure
 */
int adxl36xSetSelfTest(const struct device* const dev, const bool is_enabled);

/* ============================================================================
 * Power Modes
 * ============================================================================ */

/**
 * @brief Set noise mode (normal vs low-noise)
 *
 * @param[in] dev  Zephyr device handle
 * @param[in] mode Desired noise mode
 * @return 0 on success, negative errno on failure
 */
int adxl36xSetNoiseMode(const struct device* const dev, const ADXL36xNoiseMode mode);

/**
 * @brief Enable or disable wake-up mode with configurable rate
 *
 * Wake-up mode samples at a reduced rate to conserve power.
 * When disabling, the rate parameter is ignored.
 *
 * @param[in] dev        Zephyr device handle
 * @param[in] is_enabled True to enable, false to disable
 * @param[in] rate       Wake-up sampling rate (ignored when disabling)
 * @return 0 on success, negative errno on failure
 */
int adxl36xSetWakeupMode(const struct device* const dev, const bool is_enabled, const ADXL36xWakeupRate rate);

/**
 * @brief Enable or disable auto-sleep
 *
 * When enabled, the device automatically enters wake-up mode
 * after inactivity is detected (requires link/loop mode).
 *
 * @param[in] dev        Zephyr device handle
 * @param[in] is_enabled True to enable, false to disable
 * @return 0 on success, negative errno on failure
 */
int adxl36xSetAutoSleep(const struct device* const dev, const bool is_enabled);

/**
 * @brief Set link/loop operating mode
 *
 * Controls how activity and inactivity detection interact.
 *
 * @param[in] dev  Zephyr device handle
 * @param[in] mode Desired link/loop mode
 * @return 0 on success, negative errno on failure
 */
int adxl36xSetLinkLoopMode(const struct device* const dev, const ADXL36xLinkLoopMode mode);

/* ============================================================================
 * Extended Interrupts
 * ============================================================================ */

/**
 * @brief Set INT2 interrupt source mask and polarity
 *
 * Writes INTMAP2_LOWER register.
 *
 * @param[in] dev      Zephyr device handle
 * @param[in] sources  Bitmask of ADXL36X_INTERRUPT_SRC_* flags
 * @param[in] polarity Nonzero for active-low
 * @return 0 on success, negative errno on failure
 */
int adxl36xSetInterrupt2Sources(const struct device* const dev, const uint8_t sources, const uint8_t polarity);

/**
 * @brief Set INT1 upper interrupt source mask
 *
 * Writes INTMAP1_UPPER register (tap, temp/ADC thresholds, keep-alive, errors).
 *
 * @param[in] dev     Zephyr device handle
 * @param[in] sources Bitmask of ADXL36X_INT_UPPER_* flags
 * @return 0 on success, negative errno on failure
 */
int adxl36xSetInterrupt1UpperSources(const struct device* const dev, const uint8_t sources);

/**
 * @brief Set INT2 upper interrupt source mask
 *
 * Writes INTMAP2_UPPER register (tap, temp/ADC thresholds, keep-alive, errors).
 *
 * @param[in] dev     Zephyr device handle
 * @param[in] sources Bitmask of ADXL36X_INT_UPPER_* flags
 * @return 0 on success, negative errno on failure
 */
int adxl36xSetInterrupt2UpperSources(const struct device* const dev, const uint8_t sources);

/* ============================================================================
 * Status
 * ============================================================================ */

/**
 * @brief Read STATUS2 register
 *
 * Contains tap, temp/ADC threshold, keep-alive, and fuse error flags.
 *
 * @param[in]  dev     Zephyr device handle
 * @param[out] status2 Output: STATUS2 register byte. Must not be NULL.
 * @return 0 on success, negative errno on failure
 */
int adxl36xReadStatus2(const struct device* const dev, uint8_t* const status2);

/* ============================================================================
 * Identification
 * ============================================================================ */

/**
 * @brief Read device serial number
 *
 * Burst-reads 4 bytes from SERIAL_NR_3..SERIAL_NR_0 and combines
 * into a 32-bit value (big-endian: MSB first).
 *
 * @param[in]  dev    Zephyr device handle
 * @param[out] serial Output: 32-bit serial number. Must not be NULL.
 * @return 0 on success, negative errno on failure
 */
int adxl36xGetSerialNumber(const struct device* const dev, uint32_t* const serial);

/* ============================================================================
 * Axis Mask
 * ============================================================================ */

/**
 * @brief Set axis exclusion mask for activity/inactivity detection
 *
 * Bit 0: exclude X, Bit 1: exclude Y, Bit 2: exclude Z.
 * Only the lower 3 bits are used; tap axis bits are preserved.
 *
 * @param[in] dev          Zephyr device handle
 * @param[in] exclude_mask Axis exclusion bitmask [2:0]
 * @return 0 on success, negative errno on failure
 */
int adxl36xSetAxisMask(const struct device* const dev, const uint8_t exclude_mask);

/* ============================================================================
 * Temperature / ADC Thresholds
 * ============================================================================ */

/**
 * @brief Configure temperature/ADC over and under thresholds
 *
 * Sets 13-bit thresholds and 4-bit timers for temperature/ADC
 * threshold detection.
 *
 * @param[in] dev    Zephyr device handle
 * @param[in] config Threshold configuration. Must not be NULL.
 * @return 0 on success, negative errno on failure
 */
int adxl36xSetTempADCThresholds(const struct device* const dev, const ADXL36xTempADCThreshold* const config);

/* ============================================================================
 * Timer / Keep-Alive
 * ============================================================================ */

/**
 * @brief Set keep-alive timer period
 *
 * 5-bit value written to TIMER_CTL[4:0]. The timer fires periodically
 * and can be mapped to an interrupt pin via INTMAP_UPPER.
 *
 * @param[in] dev          Zephyr device handle
 * @param[in] period_count Keep-alive period (0-31)
 * @return 0 on success, negative errno on failure
 */
int adxl36xSetKeepAlive(const struct device* const dev, const uint8_t period_count);

#ifdef __cplusplus
}
#endif

#endif /* ADXL36X_H */
