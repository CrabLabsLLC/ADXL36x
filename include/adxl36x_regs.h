/**
 * @file adxl36x_regs.h
 * @brief ADXL36x accelerometer register definitions
 *
 * Register map compatible with ADXL362, ADXL363, ADXL366, and ADXL367 variants.
 * Based on the ADXL367 datasheet (Rev. A).
 *
 * All bit-field constants use raw hex values (no BIT()/GENMASK() dependency)
 * to keep this header platform-independent.
 *
 * @author Orion Serup <orion@crablabs.io>
 *
 * @reviewer Claude <noreply@anthropic.com>
 */

/* Copyright (c) 2025 Crab Labs LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#ifndef ADXL36X_REGS_H
#define ADXL36X_REGS_H

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_id Device ID Registers
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_REG_DEVID_AD        0x00  ///< Analog Devices ID (0xAD)
#define ADXL36X_REG_DEVID_MST       0x01  ///< MEMS device ID (0x1D)
#define ADXL36X_REG_PARTID          0x02  ///< Part ID
#define ADXL36X_REG_REVID           0x03  ///< Revision ID
#define ADXL36X_REG_SERIAL_NR_3     0x04  ///< Serial number [31:24]
#define ADXL36X_REG_SERIAL_NR_2     0x05  ///< Serial number [23:16]
#define ADXL36X_REG_SERIAL_NR_1     0x06  ///< Serial number [15:8]
#define ADXL36X_REG_SERIAL_NR_0     0x07  ///< Serial number [7:0]

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_data 8-bit Data Registers
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_REG_XDATA           0x08  ///< X-axis [13:6]
#define ADXL36X_REG_YDATA           0x09  ///< Y-axis [13:6]
#define ADXL36X_REG_ZDATA           0x0A  ///< Z-axis [13:6]

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_status Status and FIFO Registers
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_REG_STATUS          0x0B  ///< Status register
#define ADXL36X_REG_FIFO_ENTRIES_L  0x0C  ///< FIFO entries low byte
#define ADXL36X_REG_FIFO_ENTRIES_H  0x0D  ///< FIFO entries high byte

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_data14 14-bit Data Registers
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_REG_X_DATA_H        0x0E  ///< X-axis data high byte
#define ADXL36X_REG_X_DATA_L        0x0F  ///< X-axis data low byte
#define ADXL36X_REG_Y_DATA_H        0x10  ///< Y-axis data high byte
#define ADXL36X_REG_Y_DATA_L        0x11  ///< Y-axis data low byte
#define ADXL36X_REG_Z_DATA_H        0x12  ///< Z-axis data high byte
#define ADXL36X_REG_Z_DATA_L        0x13  ///< Z-axis data low byte

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_temp Temperature and ADC Registers
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_REG_TEMP_H          0x14  ///< Temperature data high byte
#define ADXL36X_REG_TEMP_L          0x15  ///< Temperature data low byte
#define ADXL36X_REG_EX_ADC_H        0x16  ///< Extended ADC high byte
#define ADXL36X_REG_EX_ADC_L        0x17  ///< Extended ADC low byte

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_fifo_port I2C FIFO Data Port
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_REG_I2C_FIFO_DATA   0x18  ///< I2C FIFO data register

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_reset Soft Reset
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_REG_SOFT_RESET      0x1F  ///< Soft reset register
#define ADXL36X_RESET_CODE          0x52  ///< Write this value to trigger soft reset

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_act Activity / Inactivity Threshold and Timer Registers
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_REG_THRESH_ACT_H    0x20  ///< Activity threshold high byte
#define ADXL36X_REG_THRESH_ACT_L    0x21  ///< Activity threshold low byte
#define ADXL36X_REG_TIME_ACT        0x22  ///< Activity time register
#define ADXL36X_REG_THRESH_INACT_H  0x23  ///< Inactivity threshold high byte
#define ADXL36X_REG_THRESH_INACT_L  0x24  ///< Inactivity threshold low byte
#define ADXL36X_REG_TIME_INACT_H    0x25  ///< Inactivity time high byte
#define ADXL36X_REG_TIME_INACT_L    0x26  ///< Inactivity time low byte

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_ctrl Control Registers
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_REG_ACT_INACT_CTL   0x27  ///< Activity/inactivity control
#define ADXL36X_REG_FIFO_CONTROL    0x28  ///< FIFO control
#define ADXL36X_REG_FIFO_SAMPLES    0x29  ///< FIFO samples watermark

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_intmap Interrupt Mapping Registers
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_REG_INTMAP1_LOWER   0x2A  ///< INT1 mapping (lower)
#define ADXL36X_REG_INTMAP2_LOWER   0x2B  ///< INT2 mapping (lower)
#define ADXL36X_REG_INTMAP1_UPPER   0x3A  ///< INT1 mapping (upper)
#define ADXL36X_REG_INTMAP2_UPPER   0x3B  ///< INT2 mapping (upper)

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_filter Filter and Power Control Registers
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_REG_FILTER_CTL      0x2C  ///< Filter control
#define ADXL36X_REG_POWER_CTL       0x2D  ///< Power control
#define ADXL36X_REG_SELF_TEST       0x2E  ///< Self-test register

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_status_bits STATUS Register Bit Fields (0x0B)
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_STATUS_ERR_USER_REGS  0x80  ///< User register error
#define ADXL36X_STATUS_AWAKE          0x40  ///< Device is awake
#define ADXL36X_STATUS_INACT          0x20  ///< Inactivity detected
#define ADXL36X_STATUS_ACT            0x10  ///< Activity detected
#define ADXL36X_STATUS_FIFO_OVERRUN   0x08  ///< FIFO overrun
#define ADXL36X_STATUS_FIFO_WATERMARK 0x04  ///< FIFO watermark reached
#define ADXL36X_STATUS_FIFO_RDY       0x02  ///< FIFO has at least one sample
#define ADXL36X_STATUS_DATA_RDY       0x01  ///< New data ready

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_int_bits Interrupt Mapping Bit Fields (INTMAP_LOWER)
 *
 *  Same layout for both INT1 and INT2.
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_INT_LOW               0x80  ///< Active-low polarity
#define ADXL36X_INT_AWAKE             0x40  ///< Map awake to interrupt pin
#define ADXL36X_INT_INACT             0x20  ///< Map inactivity to interrupt pin
#define ADXL36X_INT_ACT               0x10  ///< Map activity to interrupt pin
#define ADXL36X_INT_FIFO_OVERRUN      0x08  ///< Map FIFO overrun to interrupt pin
#define ADXL36X_INT_FIFO_WATERMARK    0x04  ///< Map FIFO watermark to interrupt pin
#define ADXL36X_INT_FIFO_RDY          0x02  ///< Map FIFO ready to interrupt pin
#define ADXL36X_INT_DATA_RDY          0x01  ///< Map data ready to interrupt pin

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_filter_bits FILTER_CTL Register Bit Fields (0x2C)
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_FILTER_CTL_RANGE_MSK   0xC0  ///< Measurement range mask [7:6]
#define ADXL36X_FILTER_CTL_RANGE_POS   6     ///< Measurement range bit position
#define ADXL36X_FILTER_CTL_I2C_HS      0x20  ///< I2C high-speed enable
#define ADXL36X_FILTER_CTL_RES         0x10  ///< Resolution bit
#define ADXL36X_FILTER_CTL_EXT_SAMPLE  0x08  ///< External sampling trigger
#define ADXL36X_FILTER_CTL_ODR_MSK     0x07  ///< Output data rate mask [2:0]
#define ADXL36X_FILTER_CTL_ODR_POS     0     ///< Output data rate bit position

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_power_bits POWER_CTL Register Bit Fields (0x2D)
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_POWER_CTL_EXT_CLK      0x40  ///< External clock enable
#define ADXL36X_POWER_CTL_NOISE_MSK    0x30  ///< Noise mode mask [5:4]
#define ADXL36X_POWER_CTL_NOISE_POS    4     ///< Noise mode bit position
#define ADXL36X_POWER_CTL_WAKEUP       0x08  ///< Wake-up mode enable
#define ADXL36X_POWER_CTL_AUTOSLEEP    0x04  ///< Auto-sleep enable
#define ADXL36X_POWER_CTL_MEASURE_MSK  0x03  ///< Measurement mode mask [1:0]
#define ADXL36X_POWER_CTL_MEASURE_POS  0     ///< Measurement mode bit position

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_fifo_bits FIFO_CONTROL Register Bit Fields (0x28)
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_FIFO_CTL_CHANNEL_MSK   0x78  ///< FIFO channel selection mask [6:3]
#define ADXL36X_FIFO_CTL_CHANNEL_POS   3     ///< FIFO channel selection bit position
#define ADXL36X_FIFO_CTL_SAMPLES_MSK   0x04  ///< FIFO samples bit 8 (MSB of watermark)
#define ADXL36X_FIFO_CTL_MODE_MSK      0x03  ///< FIFO mode mask [1:0]
#define ADXL36X_FIFO_CTL_MODE_POS      0     ///< FIFO mode bit position

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_act_bits ACT_INACT_CTL Register Bit Fields (0x27)
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_ACT_INACT_LINKLOOP_MSK 0x30  ///< Link/loop mode mask [5:4]
#define ADXL36X_ACT_INACT_INACT_REF    0x08  ///< Inactivity referenced mode
#define ADXL36X_ACT_INACT_INACT_EN     0x04  ///< Inactivity detection enable
#define ADXL36X_ACT_INACT_ACT_REF      0x02  ///< Activity referenced mode
#define ADXL36X_ACT_INACT_ACT_EN       0x01  ///< Activity detection enable

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_opmodes Operating Modes
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_OP_STANDBY             0x00  ///< Standby (default, no measurement)
#define ADXL36X_OP_MEASURE             0x02  ///< Measurement mode

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_scale Scale Factors (LSB per g)
 *
 *  Used for sensitivity calculation across range settings.
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_ACCEL_2G_LSB_PER_G     4000  ///< +/-2g range
#define ADXL36X_ACCEL_4G_LSB_PER_G     2000  ///< +/-4g range
#define ADXL36X_ACCEL_8G_LSB_PER_G     1000  ///< +/-8g range

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_devid Expected Device ID Values
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_DEVID_AD_VAL           0xAD  ///< Expected Analog Devices ID
#define ADXL36X_DEVID_MST_VAL          0x1D  ///< Expected MEMS device ID
#define ADXL36X_PARTID_362_VAL         0xF2  ///< Part ID for ADXL362
#define ADXL36X_PARTID_363_VAL         0xF3  ///< Part ID for ADXL363
#define ADXL36X_PARTID_366_VAL         0xF7  ///< Part ID for ADXL366 (same as ADXL367)
#define ADXL36X_PARTID_367_VAL         0xF7  ///< Part ID for ADXL367

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_fifo_hdr FIFO Header / Channel ID Helpers
 *
 *  Helpers for extracting channel identity from 14-bit CHID mode FIFO data.
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_FIFO_HDR_AXIS(x)       (((x) >> 14) & 0x03)  ///< Extract axis ID from FIFO word

#define ADXL36X_FIFO_AXIS_X            0  ///< X-axis channel
#define ADXL36X_FIFO_AXIS_Y            1  ///< Y-axis channel
#define ADXL36X_FIFO_AXIS_Z            2  ///< Z-axis channel
#define ADXL36X_FIFO_AXIS_TEMP         3  ///< Temperature channel

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_adc_ctl ADC_CTL Register (0x3C, ADXL366/367 only)
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_REG_ADC_CTL            0x3C  ///< ADC control register (ADXL366/367)
#define ADXL36X_ADC_CTL_FIFO_FMT_MSK  0xC0  ///< FIFO format mask [7:6]
#define ADXL36X_ADC_CTL_FIFO_FMT_POS  6     ///< FIFO format bit position

#define ADXL36X_FIFO_FMT_12B_CHID     0  ///< 12-bit with channel ID (default)
#define ADXL36X_FIFO_FMT_8B           1  ///< 8-bit samples
#define ADXL36X_FIFO_FMT_12B          2  ///< 12-bit without channel ID
#define ADXL36X_FIFO_FMT_14B_CHID     3  ///< 14-bit with channel ID

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_fifo_depth FIFO Constants
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_FIFO_DEPTH             512   ///< Maximum FIFO entries (individual axis samples)
#define ADXL36X_BYTES_PER_FIFO_ENTRY   2     ///< Bytes per FIFO entry (all formats)
#define ADXL36X_FIFO_DATA_14B_MASK     0x3FFF ///< 14-bit data mask
#define ADXL36X_FIFO_SIGN_BIT_14B      0x2000 ///< Sign bit for 14-bit data
#define ADXL36X_FIFO_SIGN_EXTEND_14B   0xC000 ///< Sign-extend bits for 14-bit to 16-bit

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_self_test SELF_TEST Register Bit Fields (0x2E)
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_SELF_TEST_ST_FORCE     0x02  ///< Force self-test (override measurement mode)
#define ADXL36X_SELF_TEST_ST           0x01  ///< Self-test enable

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_tap Tap Detection Registers (0x2F-0x32)
 *
 *  Tap threshold scale depends on range:
 *    +/-8g: 31.25 mg/LSB (all 8 bits used)
 *    +/-4g: 31.25 mg/LSB (bit 7 ignored, 7-bit threshold)
 *    +/-2g: 31.25 mg/LSB (bits [7:6] ignored, 6-bit threshold)
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_REG_TAP_THRESH         0x2F  ///< Tap threshold (8-bit unsigned, 31.25 mg/LSB)
#define ADXL36X_REG_TAP_DUR            0x30  ///< Tap duration (8-bit unsigned, 625 us/LSB)
#define ADXL36X_REG_TAP_LATENT         0x31  ///< Tap latency (8-bit unsigned, 1.25 ms/LSB; 0 disables double-tap)
#define ADXL36X_REG_TAP_WINDOW         0x32  ///< Tap window (8-bit unsigned, 1.25 ms/LSB)

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_offset User Offset Registers (0x33-0x35)
 *
 *  5-bit two's complement offset trim, 15 mg/LSB.
 *  Bits [7:5] are reserved and must be written as 0.
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_REG_X_OFFSET           0x33  ///< X-axis user offset [4:0] (15 mg/LSB, two's complement)
#define ADXL36X_REG_Y_OFFSET           0x34  ///< Y-axis user offset [4:0] (15 mg/LSB, two's complement)
#define ADXL36X_REG_Z_OFFSET           0x35  ///< Z-axis user offset [4:0] (15 mg/LSB, two's complement)

#define ADXL36X_OFFSET_MSK             0x1F  ///< User offset data mask [4:0]

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_sens User Sensitivity Registers (0x36-0x38)
 *
 *  6-bit two's complement sensitivity trim, 1.56%/LSB.
 *  Bits [7:6] are reserved and must be written as 0.
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_REG_X_SENS             0x36  ///< X-axis user sensitivity [5:0] (1.56%/LSB, two's complement)
#define ADXL36X_REG_Y_SENS             0x37  ///< Y-axis user sensitivity [5:0] (1.56%/LSB, two's complement)
#define ADXL36X_REG_Z_SENS             0x38  ///< Z-axis user sensitivity [5:0] (1.56%/LSB, two's complement)

#define ADXL36X_SENS_MSK               0x3F  ///< User sensitivity data mask [5:0]

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_timer_ctl TIMER_CTL Register (0x39)
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_REG_TIMER_CTL          0x39  ///< Timer control register

#define ADXL36X_TIMER_CTL_WAKEUP_RATE_MSK   0xC0  ///< Wake-up rate mask [7:6]
#define ADXL36X_TIMER_CTL_WAKEUP_RATE_POS   6     ///< Wake-up rate bit position
#define ADXL36X_TIMER_CTL_KEEP_ALIVE_MSK    0x1F  ///< Keep-alive timer mask [4:0]
#define ADXL36X_TIMER_CTL_KEEP_ALIVE_POS    0     ///< Keep-alive timer bit position

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_intmap_upper INTMAP_UPPER Bit Fields (0x3A, 0x3B)
 *
 *  Same layout for both INT1 and INT2 upper mapping registers.
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_INT_UPPER_ERR_FUSE         0x80  ///< Map fuse error to interrupt pin
#define ADXL36X_INT_UPPER_ERR_USER_REGS    0x40  ///< Map user register error to interrupt pin
#define ADXL36X_INT_UPPER_KPALV_TIMER      0x10  ///< Map keep-alive timer to interrupt pin
#define ADXL36X_INT_UPPER_TEMP_ADC_HI      0x08  ///< Map temp/ADC over threshold to interrupt pin
#define ADXL36X_INT_UPPER_TEMP_ADC_LOW     0x04  ///< Map temp/ADC under threshold to interrupt pin
#define ADXL36X_INT_UPPER_TAP_TWO          0x02  ///< Map double tap to interrupt pin
#define ADXL36X_INT_UPPER_TAP_ONE          0x01  ///< Map single tap to interrupt pin

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_adc_ctl_ext ADC_CTL Extended Bit Fields (0x3C)
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_ADC_CTL_ADC_INACT_EN       0x08  ///< Enable inactivity detection on external ADC
#define ADXL36X_ADC_CTL_ADC_ACT_EN         0x02  ///< Enable activity detection on external ADC
#define ADXL36X_ADC_CTL_ADC_EN             0x01  ///< Enable external ADC channel

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_temp_ctl TEMP_CTL Register (0x3D)
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_REG_TEMP_CTL               0x3D  ///< Temperature control register

#define ADXL36X_TEMP_CTL_TEMP_INACT_EN     0x08  ///< Enable temperature inactivity detection
#define ADXL36X_TEMP_CTL_TEMP_ACT_EN       0x02  ///< Enable temperature activity detection
#define ADXL36X_TEMP_CTL_TEMP_EN           0x01  ///< Enable temperature sensor

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_temp_adc_thrsh Temp/ADC Threshold Registers (0x3E-0x42)
 *
 *  13-bit unsigned thresholds for temperature/ADC over and under limit detection.
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_REG_TEMP_ADC_OVER_THRSH_H  0x3E  ///< Over threshold high byte [6:0] = threshold [12:6]
#define ADXL36X_REG_TEMP_ADC_OVER_THRSH_L  0x3F  ///< Over threshold low byte [7:2] = threshold [5:0]
#define ADXL36X_REG_TEMP_ADC_UNDER_THRSH_H 0x40  ///< Under threshold high byte [6:0] = threshold [12:6]
#define ADXL36X_REG_TEMP_ADC_UNDER_THRSH_L 0x41  ///< Under threshold low byte [7:2] = threshold [5:0]
#define ADXL36X_REG_TEMP_ADC_TIMER         0x42  ///< Temp/ADC activity/inactivity timer

#define ADXL36X_THRSH_H_MSK                0x7F  ///< Threshold high byte data mask [6:0]
#define ADXL36X_THRSH_L_MSK                0xFC  ///< Threshold low byte data mask [7:2]
#define ADXL36X_THRSH_L_POS                2     ///< Threshold low byte bit position

#define ADXL36X_TEMP_ADC_TIMER_INACT_MSK   0xF0  ///< Temp/ADC inactivity timer mask [7:4]
#define ADXL36X_TEMP_ADC_TIMER_INACT_POS   4     ///< Temp/ADC inactivity timer bit position
#define ADXL36X_TEMP_ADC_TIMER_ACT_MSK     0x0F  ///< Temp/ADC activity timer mask [3:0]
#define ADXL36X_TEMP_ADC_TIMER_ACT_POS     0     ///< Temp/ADC activity timer bit position

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_axis_mask AXIS_MASK Register (0x43)
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_REG_AXIS_MASK              0x43  ///< Axis mask register

#define ADXL36X_AXIS_MASK_TAP_AXIS_MSK    0x30  ///< Tap axis selection mask [5:4]
#define ADXL36X_AXIS_MASK_TAP_AXIS_POS    4     ///< Tap axis selection bit position
#define ADXL36X_AXIS_MASK_ACT_INACT_Z     0x04  ///< Exclude Z-axis from activity/inactivity detection
#define ADXL36X_AXIS_MASK_ACT_INACT_Y     0x02  ///< Exclude Y-axis from activity/inactivity detection
#define ADXL36X_AXIS_MASK_ACT_INACT_X     0x01  ///< Exclude X-axis from activity/inactivity detection

#define ADXL36X_TAP_AXIS_X                0     ///< Tap detection on X-axis
#define ADXL36X_TAP_AXIS_Y                1     ///< Tap detection on Y-axis
#define ADXL36X_TAP_AXIS_Z                2     ///< Tap detection on Z-axis

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_status_copy STATUS_COPY Register (0x44)
 *
 *  Mirror of STATUS (0x0B). Same bit layout as @ref adxl36x_status_bits.
 *  Reset value: 0x40.
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_REG_STATUS_COPY            0x44  ///< Status copy register (same layout as STATUS)

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_status2 STATUS2 Register (0x45)
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_REG_STATUS2                0x45  ///< Status 2 register

#define ADXL36X_STATUS2_ERR_FUSE_REGS      0x80  ///< Fuse error detected
#define ADXL36X_STATUS2_FUSE_REFRESH       0x40  ///< NVM reload needed
#define ADXL36X_STATUS2_TIMER              0x10  ///< Keep-alive timer expired (cleared on read)
#define ADXL36X_STATUS2_TEMP_ADC_HI        0x08  ///< Temp/ADC over threshold
#define ADXL36X_STATUS2_TEMP_ADC_LOW       0x04  ///< Temp/ADC under threshold
#define ADXL36X_STATUS2_TAP_TWO            0x02  ///< Double tap detected
#define ADXL36X_STATUS2_TAP_ONE            0x01  ///< Single tap detected

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_noise_mode Noise Mode Values (POWER_CTL [5:4])
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_NOISE_MODE_NORMAL          0x00  ///< Normal operation
#define ADXL36X_NOISE_MODE_LOW_NOISE       0x10  ///< Low noise mode

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_linkloop Link/Loop Mode Values (ACT_INACT_CTL [5:4])
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_LINKLOOP_DEFAULT           0x00  ///< Default mode (no link/loop)
#define ADXL36X_LINKLOOP_LINKED            0x10  ///< Linked mode
#define ADXL36X_LINKLOOP_LOOP_HOST         0x20  ///< Loop mode (host acknowledge)
#define ADXL36X_LINKLOOP_LOOP_AUTO         0x30  ///< Loop mode (auto acknowledge)

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_wakeup_rate Wake-up Rate Values (TIMER_CTL [7:6])
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_WAKEUP_RATE_12_SPS         0x00  ///< 12 sps (80 ms period)
#define ADXL36X_WAKEUP_RATE_6_SPS          0x40  ///< 6 sps (160 ms period)
#define ADXL36X_WAKEUP_RATE_3_SPS          0x80  ///< 3 sps (320 ms period)
#define ADXL36X_WAKEUP_RATE_1P5_SPS        0xC0  ///< 1.5 sps (640 ms period)

/** @} */

/* -------------------------------------------------------------------------- */
/** @defgroup adxl36x_temp_sensor Temperature Sensor Constants
 *
 *  Conversion formula: temperature_C = (raw - bias) / sensitivity
 *  @{
 */
/* -------------------------------------------------------------------------- */

#define ADXL36X_TEMP_BIAS_25C              165   ///< Typical bias at 25 C (LSB)
#define ADXL36X_TEMP_SENSITIVITY           54    ///< Typical sensitivity (LSB/C)
#define ADXL36X_TEMP_RESOLUTION_BITS       14    ///< Temperature ADC resolution

/** @} */

#ifdef __cplusplus
}
#endif

#endif // ADXL36X_REGS_H
