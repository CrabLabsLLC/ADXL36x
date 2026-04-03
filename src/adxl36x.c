/* Copyright (c) 2025 Crab Labs LLC */
/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file adxl36x.c
 * @brief ADXL36x accelerometer driver implementation
 *
 * Supports both the custom ADXL36x API and Zephyr's standard sensor API.
 * Provides FIFO-based bulk read, interrupt management, activity detection,
 * and automatic streaming with internal sample caching.
 *
 * When a work queue is attached and streaming is started, the driver
 * auto-fetches FIFO data on each watermark interrupt, parses 14-bit
 * samples with channel ID, and caches them in an internal ring buffer.
 *
 * @author Orion Serup <orion@crablabs.io>
 * @author Claude <noreply@anthropic.com>
 *
 * @reviewer
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#define DT_DRV_COMPAT adi_adxl36x

#include "adxl36x.h"

LOG_MODULE_REGISTER(adxl36x, CONFIG_ADXL36X_LOG_LEVEL);

/* ---------------------------------------------------------------------------
 * Internal Constants
 * ------------------------------------------------------------------------- */

/** Maximum FIFO entries to read in a single burst (96 entries = 192 bytes) */
#define FETCH_CHUNK_ENTRIES  96
#define FETCH_CHUNK_BYTES    (FETCH_CHUNK_ENTRIES * ADXL36X_BYTES_PER_FIFO_ENTRY)

/* ---------------------------------------------------------------------------
 * Types
 * ------------------------------------------------------------------------- */

struct adxl36x_data
{
	// Cached XYZ (for Zephyr sensor API)
	int16_t x_raw;
	int16_t y_raw;
	int16_t z_raw;

	// Tracked hardware settings
	ADXL36xRange range;
	ADXL36xODR odr;
	uint8_t part_id;
	atomic_t is_streaming;

#ifdef CONFIG_ADXL36X_IRQ_ENABLE
	// GPIO interrupt
	struct gpio_callback gpio_cb;
	ADXL36xInterruptCallback int_cb;
	void* int_user_data;
	const struct device* dev;   // back-pointer for CONTAINER_OF

	// Auto-fetch streaming
	struct k_work fetch_work;
	ADXL36xSample cache[CONFIG_ADXL36X_CACHE_SIZE];
	uint16_t cache_head;
	uint16_t cache_tail;
	uint16_t cache_count;
	struct k_spinlock cache_lock;
	ADXL36xDataCallback data_cb;
	void* data_cb_user_data;
	uint8_t fetch_buf[FETCH_CHUNK_BYTES];
#endif // CONFIG_ADXL36X_IRQ_ENABLE

	atomic_t sample_count;
};

struct adxl36x_config
{
	struct i2c_dt_spec i2c;

#ifdef CONFIG_ADXL36X_IRQ_ENABLE
	struct gpio_dt_spec int1_gpio;
#endif

	uint8_t odr;
	uint8_t range;
};

/* ---------------------------------------------------------------------------
 * Static function declarations
 * ------------------------------------------------------------------------- */

static int regRead(const struct device* const dev, const uint8_t register_address, uint8_t* const register_value);
static int regWrite(const struct device* const dev, const uint8_t register_address, const uint8_t register_value);
static int regUpdate(const struct device* const dev, const uint8_t register_address, const uint8_t mask, const uint8_t register_value);
static int burstRead(const struct device* const dev, const uint8_t start_register, uint8_t* const read_buffer, const size_t length);
static int16_t signExtend14(const uint16_t raw);

#ifdef CONFIG_ADXL36X_IRQ_ENABLE
static void gpioCallback(const struct device* const port, struct gpio_callback* const cb, const gpio_port_pins_t pins);
static void fetchWorkHandler(struct k_work* const work);
#endif

static int sensorSampleFetch(const struct device* const dev, const enum sensor_channel chan);
static int sensorChannelGet(const struct device* const dev, const enum sensor_channel chan, struct sensor_value* const val);
static int sensorAttrSet(const struct device* const dev, const enum sensor_channel chan, const enum sensor_attribute attr, const struct sensor_value* const val);
static int32_t rangeLsbPerG(const ADXL36xRange range);
static void rawToSensorValue(const int16_t raw, const ADXL36xRange range, struct sensor_value* const val);
static int adxl36xDeviceInit(const struct device* const dev);

/* ---------------------------------------------------------------------------
 * Public - Lifecycle
 * ------------------------------------------------------------------------- */

int adxl36xReset(const struct device* const dev)
{
	struct adxl36x_data* const data = dev->data;

	// Clear streaming state -- hardware reset returns chip to standby
	atomic_set(&data->is_streaming, 0);

	const int status = regWrite(dev, ADXL36X_REG_SOFT_RESET, ADXL36X_RESET_CODE);
	if (status < 0)
		return status;

	// Datasheet: 5ms typical power-on time; ADI reference uses 20ms
	k_sleep(K_MSEC(10));
	return 0;
}

int adxl36xEnable(const struct device* const dev)
{
	return regUpdate(dev, ADXL36X_REG_POWER_CTL,
	                 ADXL36X_POWER_CTL_MEASURE_MSK,
	                 ADXL36X_OP_MEASURE);
}

int adxl36xDisable(const struct device* const dev)
{
	return regUpdate(dev, ADXL36X_REG_POWER_CTL,
	                 ADXL36X_POWER_CTL_MEASURE_MSK,
	                 ADXL36X_OP_STANDBY);
}

/* ---------------------------------------------------------------------------
 * Public - Configuration
 * ------------------------------------------------------------------------- */

int adxl36xSetODR(const struct device* const dev, const ADXL36xODR odr)
{
	if (odr > ADXL36X_ODR_400HZ)
		return -EINVAL;

	const int status = regUpdate(dev, ADXL36X_REG_FILTER_CTL,
	                             ADXL36X_FILTER_CTL_ODR_MSK,
	                             (uint8_t)odr << ADXL36X_FILTER_CTL_ODR_POS);
	if (status == 0)
	{
		struct adxl36x_data* const data = dev->data;
		data->odr = odr;
	}

	return status;
}

int adxl36xSetRange(const struct device* const dev, const ADXL36xRange range)
{
	if (range > ADXL36X_RANGE_8G)
		return -EINVAL;

	const int status = regUpdate(dev, ADXL36X_REG_FILTER_CTL,
	                             ADXL36X_FILTER_CTL_RANGE_MSK,
	                             (uint8_t)range << ADXL36X_FILTER_CTL_RANGE_POS);
	if (status == 0)
	{
		struct adxl36x_data* const data = dev->data;
		data->range = range;
	}

	return status;
}

int adxl36xSetFIFOConfig(const struct device* const dev, const ADXL36xFIFOConfig* const config)
{
	if (!config)
		return -EINVAL;

	// Clamp watermark to 9-bit max
	const uint16_t watermark_value = (config->watermark > 0x1FF)
	                                 ? 0x1FF : config->watermark;

	// FIFO_CONTROL: [6:3]=channel, [2]=watermark_msb, [1:0]=mode
	const uint8_t fifo_control_byte =
		((uint8_t)config->format << ADXL36X_FIFO_CTL_CHANNEL_POS)
		| ((uint8_t)config->mode << ADXL36X_FIFO_CTL_MODE_POS)
		| ((watermark_value & 0x100) ? ADXL36X_FIFO_CTL_SAMPLES_MSK : 0);

	const int status = regWrite(dev, ADXL36X_REG_FIFO_CONTROL, fifo_control_byte);
	if (status < 0)
		return status;

	return regWrite(dev, ADXL36X_REG_FIFO_SAMPLES,
	                (uint8_t)(watermark_value & 0xFF));
}

int adxl36xSetActivity(const struct device* const dev, const ADXL36xActivityConfig* const config)
{
	if (!config)
		return -EINVAL;

	// Threshold: 13-bit, upper 7 bits in THRESH_ACT_H, lower 6 in THRESH_ACT_L
	const uint16_t thresh = config->threshold & 0x1FFF;

	int status = regWrite(dev, ADXL36X_REG_THRESH_ACT_H,
	                      (uint8_t)(thresh >> 6));
	if (status < 0)
		return status;

	status = regWrite(dev, ADXL36X_REG_THRESH_ACT_L,
	                  (uint8_t)((thresh & 0x3F) << 2));
	if (status < 0)
		return status;

	status = regWrite(dev, ADXL36X_REG_TIME_ACT, config->time_periods);
	if (status < 0)
		return status;

	const uint8_t ctl = (config->is_enabled ? ADXL36X_ACT_INACT_ACT_EN : 0)
	                   | (config->is_referenced ? ADXL36X_ACT_INACT_ACT_REF : 0);

	return regUpdate(dev, ADXL36X_REG_ACT_INACT_CTL,
	                 ADXL36X_ACT_INACT_ACT_EN | ADXL36X_ACT_INACT_ACT_REF,
	                 ctl);
}

int adxl36xSetInactivity(const struct device* const dev, const ADXL36xActivityConfig* const config)
{
	if (!config)
		return -EINVAL;

	// Threshold: 13-bit, upper 7 bits in THRESH_INACT_H, lower 6 in THRESH_INACT_L
	const uint16_t thresh = config->threshold & 0x1FFF;

	int status = regWrite(dev, ADXL36X_REG_THRESH_INACT_H,
	                      (uint8_t)(thresh >> 6));
	if (status < 0)
		return status;

	status = regWrite(dev, ADXL36X_REG_THRESH_INACT_L,
	                  (uint8_t)((thresh & 0x3F) << 2));
	if (status < 0)
		return status;

	// Time: 16-bit big-endian (high byte first)
	status = regWrite(dev, ADXL36X_REG_TIME_INACT_H,
	                  (uint8_t)(config->time_periods >> 8));
	if (status < 0)
		return status;

	status = regWrite(dev, ADXL36X_REG_TIME_INACT_L,
	                  (uint8_t)(config->time_periods & 0xFF));
	if (status < 0)
		return status;

	const uint8_t ctl = (config->is_enabled ? ADXL36X_ACT_INACT_INACT_EN : 0)
	                   | (config->is_referenced ? ADXL36X_ACT_INACT_INACT_REF : 0);

	return regUpdate(dev, ADXL36X_REG_ACT_INACT_CTL,
	                 ADXL36X_ACT_INACT_INACT_EN | ADXL36X_ACT_INACT_INACT_REF,
	                 ctl);
}

/* ---------------------------------------------------------------------------
 * Public - Tap Detection
 * ------------------------------------------------------------------------- */

int adxl36xSetTapConfig(const struct device* const dev, const ADXL36xTapConfig* const config)
{
	if (!config)
		return -EINVAL;

	int status = regWrite(dev, ADXL36X_REG_TAP_THRESH, config->threshold);
	if (status < 0)
		return status;

	status = regWrite(dev, ADXL36X_REG_TAP_DUR, config->duration);
	if (status < 0)
		return status;

	status = regWrite(dev, ADXL36X_REG_TAP_LATENT, config->latency);
	if (status < 0)
		return status;

	status = regWrite(dev, ADXL36X_REG_TAP_WINDOW, config->window);
	if (status < 0)
		return status;

	// Set tap axis in AXIS_MASK[5:4], preserving activity/inactivity mask bits
	return regUpdate(dev, ADXL36X_REG_AXIS_MASK,
	                 ADXL36X_AXIS_MASK_TAP_AXIS_MSK,
	                 (uint8_t)config->axis << ADXL36X_AXIS_MASK_TAP_AXIS_POS);
}

/* ---------------------------------------------------------------------------
 * Public - Offset Trim
 * ------------------------------------------------------------------------- */

int adxl36xSetOffset(const struct device* const dev, const int8_t x_mg, const int8_t y_mg, const int8_t z_mg)
{
	int status = regWrite(dev, ADXL36X_REG_X_OFFSET,
	                      (uint8_t)x_mg & ADXL36X_OFFSET_MSK);
	if (status < 0)
		return status;

	status = regWrite(dev, ADXL36X_REG_Y_OFFSET,
	                  (uint8_t)y_mg & ADXL36X_OFFSET_MSK);
	if (status < 0)
		return status;

	return regWrite(dev, ADXL36X_REG_Z_OFFSET,
	                (uint8_t)z_mg & ADXL36X_OFFSET_MSK);
}

int adxl36xGetOffset(const struct device* const dev, int8_t* const x_mg, int8_t* const y_mg, int8_t* const z_mg)
{
	if (!x_mg || !y_mg || !z_mg)
		return -EINVAL;

	uint8_t offset_byte;

	int status = regRead(dev, ADXL36X_REG_X_OFFSET, &offset_byte);
	if (status < 0)
		return status;

	// Sign-extend 5-bit to int8_t
	*x_mg = (int8_t)((offset_byte & ADXL36X_OFFSET_MSK) << 3) >> 3;

	status = regRead(dev, ADXL36X_REG_Y_OFFSET, &offset_byte);
	if (status < 0)
		return status;

	*y_mg = (int8_t)((offset_byte & ADXL36X_OFFSET_MSK) << 3) >> 3;

	status = regRead(dev, ADXL36X_REG_Z_OFFSET, &offset_byte);
	if (status < 0)
		return status;

	*z_mg = (int8_t)((offset_byte & ADXL36X_OFFSET_MSK) << 3) >> 3;

	return 0;
}

/* ---------------------------------------------------------------------------
 * Public - Sensitivity Trim
 * ------------------------------------------------------------------------- */

int adxl36xSetSensitivity(const struct device* const dev, const int8_t x_pct, const int8_t y_pct, const int8_t z_pct)
{
	int status = regWrite(dev, ADXL36X_REG_X_SENS,
	                      (uint8_t)x_pct & ADXL36X_SENS_MSK);
	if (status < 0)
		return status;

	status = regWrite(dev, ADXL36X_REG_Y_SENS,
	                  (uint8_t)y_pct & ADXL36X_SENS_MSK);
	if (status < 0)
		return status;

	return regWrite(dev, ADXL36X_REG_Z_SENS,
	                (uint8_t)z_pct & ADXL36X_SENS_MSK);
}

int adxl36xGetSensitivity(const struct device* const dev, int8_t* const x_pct, int8_t* const y_pct, int8_t* const z_pct)
{
	if (!x_pct || !y_pct || !z_pct)
		return -EINVAL;

	uint8_t sensitivity_byte;

	int status = regRead(dev, ADXL36X_REG_X_SENS, &sensitivity_byte);
	if (status < 0)
		return status;

	// Sign-extend 6-bit to int8_t
	*x_pct = (int8_t)((sensitivity_byte & ADXL36X_SENS_MSK) << 2) >> 2;

	status = regRead(dev, ADXL36X_REG_Y_SENS, &sensitivity_byte);
	if (status < 0)
		return status;

	*y_pct = (int8_t)((sensitivity_byte & ADXL36X_SENS_MSK) << 2) >> 2;

	status = regRead(dev, ADXL36X_REG_Z_SENS, &sensitivity_byte);
	if (status < 0)
		return status;

	*z_pct = (int8_t)((sensitivity_byte & ADXL36X_SENS_MSK) << 2) >> 2;

	return 0;
}

/* ---------------------------------------------------------------------------
 * Public - Temperature
 * ------------------------------------------------------------------------- */

int adxl36xReadTempRaw(const struct device* const dev, int16_t* const raw)
{
	if (!raw)
		return -EINVAL;

	uint8_t temperature_bytes[2];
	const int status = burstRead(dev, ADXL36X_REG_TEMP_H, temperature_bytes, 2);
	if (status < 0)
		return status;

	// 14-bit signed: TEMP_H[7:0] = data[13:6], TEMP_L[7:2] = data[5:0]
	*raw = signExtend14(((uint16_t)temperature_bytes[0] << 6)
	                    | (temperature_bytes[1] >> 2));
	return 0;
}

int adxl36xReadTempC(const struct device* const dev, float* const temp_c)
{
	if (!temp_c)
		return -EINVAL;

	int16_t raw;
	const int status = adxl36xReadTempRaw(dev, &raw);
	if (status < 0)
		return status;

	*temp_c = 25.0f + ((float)(raw - ADXL36X_TEMP_BIAS_25C)
	                    / (float)ADXL36X_TEMP_SENSITIVITY);
	return 0;
}

int adxl36xEnableTemp(const struct device* const dev, const bool is_enabled)
{
	const uint8_t enable_bits = is_enabled ? ADXL36X_TEMP_CTL_TEMP_EN : 0;
	return regUpdate(dev, ADXL36X_REG_TEMP_CTL,
	                 ADXL36X_TEMP_CTL_TEMP_EN, enable_bits);
}

/* ---------------------------------------------------------------------------
 * Public - External ADC
 * ------------------------------------------------------------------------- */

int adxl36xReadADCRaw(const struct device* const dev, int16_t* const raw)
{
	if (!raw)
		return -EINVAL;

	uint8_t adc_bytes[2];
	const int status = burstRead(dev, ADXL36X_REG_EX_ADC_H, adc_bytes, 2);
	if (status < 0)
		return status;

	// 14-bit signed: same format as temperature
	*raw = signExtend14(((uint16_t)adc_bytes[0] << 6) | (adc_bytes[1] >> 2));
	return 0;
}

int adxl36xEnableADC(const struct device* const dev, const bool is_enabled)
{
	const uint8_t enable_bits = is_enabled ? ADXL36X_ADC_CTL_ADC_EN : 0;
	return regUpdate(dev, ADXL36X_REG_ADC_CTL,
	                 ADXL36X_ADC_CTL_ADC_EN, enable_bits);
}

int adxl36xSetFIFODataFormat(const struct device* const dev, const uint8_t format)
{
	return regUpdate(dev, ADXL36X_REG_ADC_CTL,
	                 ADXL36X_ADC_CTL_FIFO_FMT_MSK,
	                 format << ADXL36X_ADC_CTL_FIFO_FMT_POS);
}

/* ---------------------------------------------------------------------------
 * Public - Self-Test
 * ------------------------------------------------------------------------- */

int adxl36xSetSelfTest(const struct device* const dev, const bool is_enabled)
{
	return regWrite(dev, ADXL36X_REG_SELF_TEST,
	                is_enabled ? ADXL36X_SELF_TEST_ST : 0x00);
}

/* ---------------------------------------------------------------------------
 * Public - Power Modes
 * ------------------------------------------------------------------------- */

int adxl36xSetNoiseMode(const struct device* const dev, const ADXL36xNoiseMode mode)
{
	return regUpdate(dev, ADXL36X_REG_POWER_CTL,
	                 ADXL36X_POWER_CTL_NOISE_MSK,
	                 (uint8_t)mode << ADXL36X_POWER_CTL_NOISE_POS);
}

int adxl36xSetWakeupMode(const struct device* const dev, const bool is_enabled, const ADXL36xWakeupRate rate)
{
	if (!is_enabled)
	{
		// Clear wake-up bit in POWER_CTL
		return regUpdate(dev, ADXL36X_REG_POWER_CTL,
		                 ADXL36X_POWER_CTL_WAKEUP, 0);
	}

	// Set wake-up rate in TIMER_CTL[7:6]
	const int status = regUpdate(dev, ADXL36X_REG_TIMER_CTL,
	                             ADXL36X_TIMER_CTL_WAKEUP_RATE_MSK,
	                             (uint8_t)rate << ADXL36X_TIMER_CTL_WAKEUP_RATE_POS);
	if (status < 0)
		return status;

	// Enable wake-up bit in POWER_CTL
	return regUpdate(dev, ADXL36X_REG_POWER_CTL,
	                 ADXL36X_POWER_CTL_WAKEUP,
	                 ADXL36X_POWER_CTL_WAKEUP);
}

int adxl36xSetAutoSleep(const struct device* const dev, const bool is_enabled)
{
	const uint8_t enable_bits = is_enabled ? ADXL36X_POWER_CTL_AUTOSLEEP : 0;
	return regUpdate(dev, ADXL36X_REG_POWER_CTL,
	                 ADXL36X_POWER_CTL_AUTOSLEEP, enable_bits);
}

int adxl36xSetLinkLoopMode(const struct device* const dev, const ADXL36xLinkLoopMode mode)
{
	return regUpdate(dev, ADXL36X_REG_ACT_INACT_CTL,
	                 ADXL36X_ACT_INACT_LINKLOOP_MSK,
	                 (uint8_t)mode << 4);
}

/* ---------------------------------------------------------------------------
 * Public - Extended Interrupts
 * ------------------------------------------------------------------------- */

int adxl36xSetInterrupt2Sources(const struct device* const dev, const uint8_t sources, const uint8_t polarity)
{
	const uint8_t interrupt_config = sources | (polarity ? ADXL36X_INT_LOW : 0);
	return regWrite(dev, ADXL36X_REG_INTMAP2_LOWER, interrupt_config);
}

int adxl36xSetInterrupt1UpperSources(const struct device* const dev, const uint8_t sources)
{
	return regWrite(dev, ADXL36X_REG_INTMAP1_UPPER, sources);
}

int adxl36xSetInterrupt2UpperSources(const struct device* const dev, const uint8_t sources)
{
	return regWrite(dev, ADXL36X_REG_INTMAP2_UPPER, sources);
}

/* ---------------------------------------------------------------------------
 * Public - Status
 * ------------------------------------------------------------------------- */

int adxl36xReadStatus2(const struct device* const dev, uint8_t* const status2)
{
	if (!status2)
		return -EINVAL;

	return regRead(dev, ADXL36X_REG_STATUS2, status2);
}

/* ---------------------------------------------------------------------------
 * Public - Identification
 * ------------------------------------------------------------------------- */

int adxl36xGetSerialNumber(const struct device* const dev, uint32_t* const serial)
{
	if (!serial)
		return -EINVAL;

	uint8_t serial_number_bytes[4];
	const int status = burstRead(dev, ADXL36X_REG_SERIAL_NR_3,
	                             serial_number_bytes, 4);
	if (status < 0)
		return status;

	*serial = ((uint32_t)serial_number_bytes[0] << 24)
	         | ((uint32_t)serial_number_bytes[1] << 16)
	         | ((uint32_t)serial_number_bytes[2] << 8)
	         | (uint32_t)serial_number_bytes[3];
	return 0;
}

/* ---------------------------------------------------------------------------
 * Public - Axis Mask
 * ------------------------------------------------------------------------- */

int adxl36xSetAxisMask(const struct device* const dev, const uint8_t exclude_mask)
{
	return regUpdate(dev, ADXL36X_REG_AXIS_MASK, 0x07,
	                 exclude_mask & 0x07);
}

/* ---------------------------------------------------------------------------
 * Public - Temperature / ADC Thresholds
 * ------------------------------------------------------------------------- */

int adxl36xSetTempADCThresholds(const struct device* const dev, const ADXL36xTempADCThreshold* const config)
{
	if (!config)
		return -EINVAL;

	// Over threshold: 13-bit, high byte[6:0]=threshold[12:6], low byte[7:2]=threshold[5:0]
	const uint16_t over = config->over_threshold & 0x1FFF;

	int status = regWrite(dev, ADXL36X_REG_TEMP_ADC_OVER_THRSH_H,
	                      (uint8_t)(over >> 6));
	if (status < 0)
		return status;

	status = regWrite(dev, ADXL36X_REG_TEMP_ADC_OVER_THRSH_L,
	                  (uint8_t)((over & 0x3F) << 2));
	if (status < 0)
		return status;

	// Under threshold: same format
	const uint16_t under = config->under_threshold & 0x1FFF;

	status = regWrite(dev, ADXL36X_REG_TEMP_ADC_UNDER_THRSH_H,
	                  (uint8_t)(under >> 6));
	if (status < 0)
		return status;

	status = regWrite(dev, ADXL36X_REG_TEMP_ADC_UNDER_THRSH_L,
	                  (uint8_t)((under & 0x3F) << 2));
	if (status < 0)
		return status;

	// Timer: upper nibble = inact_timer, lower nibble = act_timer
	const uint8_t timer = ((config->inact_timer & 0x0F) << ADXL36X_TEMP_ADC_TIMER_INACT_POS)
	                     | (config->act_timer & 0x0F);

	return regWrite(dev, ADXL36X_REG_TEMP_ADC_TIMER, timer);
}

/* ---------------------------------------------------------------------------
 * Public - Timer / Keep-Alive
 * ------------------------------------------------------------------------- */

int adxl36xSetKeepAlive(const struct device* const dev, const uint8_t period_count)
{
	return regUpdate(dev, ADXL36X_REG_TIMER_CTL,
	                 ADXL36X_TIMER_CTL_KEEP_ALIVE_MSK,
	                 period_count & 0x1F);
}

ADXL36xRange adxl36xGetRange(const struct device* const dev)
{
	const struct adxl36x_data* const data = dev->data;
	return data->range;
}

ADXL36xODR adxl36xGetODR(const struct device* const dev)
{
	const struct adxl36x_data* const data = dev->data;
	return data->odr;
}

/* ---------------------------------------------------------------------------
 * Public - Data Acquisition (Direct Register Read)
 * ------------------------------------------------------------------------- */

int adxl36xGetFIFOEntries(const struct device* const dev, uint16_t* const entries)
{
	if (!entries)
		return -EINVAL;

	uint8_t entry_count_bytes[2];
	const int status = burstRead(dev, ADXL36X_REG_FIFO_ENTRIES_L,
	                             entry_count_bytes, 2);
	if (status < 0)
		return status;

	*entries = (uint16_t)entry_count_bytes[0]
	          | ((uint16_t)(entry_count_bytes[1] & 0x03) << 8);
	return 0;
}

int adxl36xReadFIFO(const struct device* const dev, uint8_t* const buffer, const size_t byte_count)
{
	if (!buffer || byte_count == 0)
		return -EINVAL;

	return burstRead(dev, ADXL36X_REG_I2C_FIFO_DATA, buffer, byte_count);
}

int adxl36xReadXYZ(const struct device* const dev, int16_t* const x, int16_t* const y, int16_t* const z)
{
	uint8_t xyz_bytes[6];
	const int status = burstRead(dev, ADXL36X_REG_X_DATA_H, xyz_bytes, 6);
	if (status < 0)
		return status;

	// 14-bit signed: DATA_H[7:0] = data[13:6], DATA_L[7:2] = data[5:0]
	const int16_t raw_x = signExtend14(((uint16_t)xyz_bytes[0] << 6)
	                                   | (xyz_bytes[1] >> 2));
	const int16_t raw_y = signExtend14(((uint16_t)xyz_bytes[2] << 6)
	                                   | (xyz_bytes[3] >> 2));
	const int16_t raw_z = signExtend14(((uint16_t)xyz_bytes[4] << 6)
	                                   | (xyz_bytes[5] >> 2));

	if (x)
		*x = raw_x;
	if (y)
		*y = raw_y;
	if (z)
		*z = raw_z;

	// Cache for Zephyr sensor API
	struct adxl36x_data* const data = dev->data;
	data->x_raw = raw_x;
	data->y_raw = raw_y;
	data->z_raw = raw_z;

	return 0;
}

int adxl36xReadStatus(const struct device* const dev, uint8_t* const status)
{
	if (!status)
		return -EINVAL;

	return regRead(dev, ADXL36X_REG_STATUS, status);
}

int adxl36xGetPartID(const struct device* const dev, uint8_t* const part_id)
{
	if (!part_id)
		return -EINVAL;

	return regRead(dev, ADXL36X_REG_PARTID, part_id);
}

int adxl36xGetRevisionID(const struct device* const dev, uint8_t* const rev_id)
{
	if (!rev_id)
		return -EINVAL;

	return regRead(dev, ADXL36X_REG_REVID, rev_id);
}

/* ---------------------------------------------------------------------------
 * Public - Streaming (Auto-Fetch with Internal Cache)
 * ------------------------------------------------------------------------- */

int adxl36xStartStreaming(const struct device* const dev, const ADXL36xFIFOConfig* const config)
{
#ifdef CONFIG_ADXL36X_IRQ_ENABLE
	struct adxl36x_data* const data = dev->data;

	if (!config)
		return -EINVAL;

	const struct adxl36x_config* const device_config = dev->config;
	if (!device_config->int1_gpio.port)
	{
		LOG_ERR("INT1 GPIO not configured - cannot stream");
		return -ENOTSUP;
	}

	if (atomic_get(&data->is_streaming))
	{
		LOG_WRN("Already streaming");
		return -EALREADY;
	}

	// Flush cache
	data->cache_head = 0;
	data->cache_tail = 0;
	data->cache_count = 0;
	atomic_set(&data->sample_count, 0);

	// Configure FIFO
	int status = adxl36xSetFIFOConfig(dev, config);
	if (status < 0)
	{
		LOG_ERR("FIFO config failed: %d", status);
		return status;
	}

	// On ADXL366/367, set FIFO format to 14-bit with channel ID
	if (data->part_id == ADXL36X_PARTID_366_VAL ||
	    data->part_id == ADXL36X_PARTID_367_VAL)
	{
		status = regUpdate(dev, ADXL36X_REG_ADC_CTL,
		                   ADXL36X_ADC_CTL_FIFO_FMT_MSK,
		                   ADXL36X_FIFO_FMT_14B_CHID << ADXL36X_ADC_CTL_FIFO_FMT_POS);
		if (status < 0)
			LOG_WRN("FIFO format config failed: %d (may be unsupported)", status);
	}

	// Enable measurement
	status = adxl36xEnable(dev);
	if (status < 0)
	{
		LOG_ERR("Enable failed: %d", status);
		return status;
	}

	// Map FIFO watermark to INT1, matching DTS GPIO polarity
	const uint8_t int_polarity = adxl36xIsInterruptActiveLow(dev) ? 1 : 0;
	status = adxl36xSetInterrupt1Sources(dev,
	                                     ADXL36X_INTERRUPT_SRC_FIFO_WATERMARK,
	                                     int_polarity);
	if (status < 0)
	{
		LOG_ERR("INT1 source config failed: %d", status);
		adxl36xDisable(dev);
		return status;
	}

	// Enable GPIO interrupt
	status = adxl36xEnableInterrupt1(dev, true);
	if (status < 0)
	{
		LOG_ERR("INT1 enable failed: %d", status);
		adxl36xDisable(dev);
		return status;
	}

	atomic_set(&data->is_streaming, 1);
	LOG_INF("Streaming started (watermark=%u)", config->watermark);
	return 0;
#else
	ARG_UNUSED(dev);
	ARG_UNUSED(config);
	return -ENOTSUP;
#endif
}

int adxl36xStopStreaming(const struct device* const dev)
{
#ifdef CONFIG_ADXL36X_IRQ_ENABLE
	struct adxl36x_data* const data = dev->data;

	if (!atomic_get(&data->is_streaming))
		return 0;

	atomic_set(&data->is_streaming, 0);

	// Disable GPIO interrupt
	adxl36xEnableInterrupt1(dev, false);

	// Cancel pending fetch work
	{
		struct k_work_sync sync;
		k_work_cancel_sync(&data->fetch_work, &sync);
	}

	// Enter standby
	adxl36xDisable(dev);

	// Disable FIFO
	const ADXL36xFIFOConfig fifo_off =
	{
		.mode = ADXL36X_FIFO_DISABLED,
		.format = ADXL36X_FIFO_FORMAT_XYZ,
		.watermark = 0,
	};
	adxl36xSetFIFOConfig(dev, &fifo_off);

	LOG_INF("Streaming stopped (cache: %u samples)", data->cache_count);
	return 0;
#else
	ARG_UNUSED(dev);
	return -ENOTSUP;
#endif
}

bool adxl36xIsStreaming(const struct device* const dev)
{
#ifdef CONFIG_ADXL36X_IRQ_ENABLE
	const struct adxl36x_data* const data = dev->data;
	return atomic_get(&data->is_streaming) != 0;
#else
	ARG_UNUSED(dev);
	return false;
#endif
}

uint16_t adxl36xGetCachedSampleCount(const struct device* const dev)
{
#ifdef CONFIG_ADXL36X_IRQ_ENABLE
	const struct adxl36x_data* const data = dev->data;
	return data->cache_count;
#else
	ARG_UNUSED(dev);
	return 0;
#endif
}

int adxl36xGetCachedSamples(const struct device* const dev, ADXL36xSample* const buf, const size_t max_count)
{
#ifdef CONFIG_ADXL36X_IRQ_ENABLE
	if (!buf || max_count == 0)
		return -EINVAL;

	struct adxl36x_data* const data = dev->data;

	k_spinlock_key_t key = k_spin_lock(&data->cache_lock);

	const size_t available = data->cache_count;
	const size_t to_read = MIN(max_count, available);

	for (size_t sample_index = 0; sample_index < to_read; sample_index++)
	{
		buf[sample_index] = data->cache[data->cache_tail];
		data->cache_tail = (data->cache_tail + 1) % CONFIG_ADXL36X_CACHE_SIZE;
	}
	data->cache_count -= to_read;

	k_spin_unlock(&data->cache_lock, key);

	return (int)to_read;
#else
	ARG_UNUSED(dev);
	ARG_UNUSED(buf);
	ARG_UNUSED(max_count);
	return 0;
#endif
}

void adxl36xFlushCache(const struct device* const dev)
{
#ifdef CONFIG_ADXL36X_IRQ_ENABLE
	struct adxl36x_data* const data = dev->data;
	k_spinlock_key_t key = k_spin_lock(&data->cache_lock);
	data->cache_head = 0;
	data->cache_tail = 0;
	data->cache_count = 0;
	k_spin_unlock(&data->cache_lock, key);
#else
	ARG_UNUSED(dev);
#endif
}

int adxl36xReadFIFOSamples(const struct device* const dev, ADXL36xSample* const buf, const size_t max_count)
{
	if (!buf || max_count == 0)
		return -EINVAL;

	struct adxl36x_data* const data = dev->data;
	if (atomic_get(&data->is_streaming))
	{
		LOG_WRN("Cannot read FIFO while streaming is active");
		return -EBUSY;
	}

	uint16_t entries = 0;
	int status = adxl36xGetFIFOEntries(dev, &entries);
	if (status < 0)
		return status;
	if (entries == 0)
		return 0;

	int16_t x = 0, y = 0, z = 0;
	bool has_x = false, has_y = false, has_z = false;
	size_t sample_count = 0;

	uint8_t read_buf[FETCH_CHUNK_BYTES];

	while (entries > 0 && sample_count < max_count)
	{
		const size_t chunk = MIN((size_t)entries, (size_t)FETCH_CHUNK_ENTRIES);
		status = adxl36xReadFIFO(dev, read_buf,
		                         chunk * ADXL36X_BYTES_PER_FIFO_ENTRY);
		if (status < 0)
			break;

		for (size_t i = 0; i < chunk && sample_count < max_count; i++)
		{
			// I2C FIFO gives MSB first (channel ID + upper bits)
			const uint8_t hi = read_buf[i * 2];
			const uint8_t lo = read_buf[i * 2 + 1];
			const uint8_t ch = (hi >> 6) & 0x03;

			const int16_t raw = signExtend14(
				((uint16_t)(hi & 0x3F) << 8) | lo);

			switch (ch)
			{
			case ADXL36X_FIFO_AXIS_X:
				x = raw;
				has_x = true;
				has_y = false;
				has_z = false;
				break;
			case ADXL36X_FIFO_AXIS_Y:
				y = raw;
				has_y = true;
				break;
			case ADXL36X_FIFO_AXIS_Z:
				z = raw;
				has_z = true;
				break;
			default:
				break;
			}

			if (has_x && has_y && has_z)
			{
				buf[sample_count].x = x;
				buf[sample_count].y = y;
				buf[sample_count].z = z;
				sample_count++;
				has_x = false;
				has_y = false;
				has_z = false;
			}
		}

		entries -= (uint16_t)chunk;
	}

	return (int)sample_count;
}

int adxl36xSetDataCallback(const struct device* const dev, const ADXL36xDataCallback callback, void* const user_data)
{
#ifdef CONFIG_ADXL36X_IRQ_ENABLE
	struct adxl36x_data* const data = dev->data;
	data->data_cb = callback;
	data->data_cb_user_data = user_data;
	return 0;
#else
	ARG_UNUSED(dev);
	ARG_UNUSED(callback);
	ARG_UNUSED(user_data);
	return -ENOTSUP;
#endif
}

/* ---------------------------------------------------------------------------
 * Public - Interrupt Management
 * ------------------------------------------------------------------------- */

int adxl36xSetInterruptCallback(const struct device* const dev, const ADXL36xInterruptCallback callback, void* const user_data)
{
#ifdef CONFIG_ADXL36X_IRQ_ENABLE
	struct adxl36x_data* const data = dev->data;

	// Just store the pointer; interrupt enable/disable is handled by adxl36xEnableInterrupt1()
	data->int_cb = callback;
	data->int_user_data = user_data;

	return 0;
#else
	ARG_UNUSED(dev);
	ARG_UNUSED(callback);
	ARG_UNUSED(user_data);
	return -ENOTSUP;
#endif
}

int adxl36xSetInterrupt1Sources(const struct device* const dev, const uint8_t sources, const uint8_t polarity)
{
	const uint8_t interrupt_config = sources | (polarity ? ADXL36X_INT_LOW : 0);
	return regWrite(dev, ADXL36X_REG_INTMAP1_LOWER, interrupt_config);
}

int adxl36xEnableInterrupt1(const struct device* const dev, const bool is_enabled)
{
#ifdef CONFIG_ADXL36X_IRQ_ENABLE
	const struct adxl36x_config* const device_config = dev->config;

	if (!device_config->int1_gpio.port)
		return -ENOTSUP;

	const gpio_flags_t flags = is_enabled ? GPIO_INT_EDGE_TO_ACTIVE
	                                      : GPIO_INT_DISABLE;
	return gpio_pin_interrupt_configure_dt(&device_config->int1_gpio, flags);
#else
	ARG_UNUSED(dev);
	ARG_UNUSED(is_enabled);
	return -ENOTSUP;
#endif
}

bool adxl36xHasInterruptGPIO(const struct device* const dev)
{
#ifdef CONFIG_ADXL36X_IRQ_ENABLE
	const struct adxl36x_config* const device_config = dev->config;
	return device_config->int1_gpio.port != NULL;
#else
	ARG_UNUSED(dev);
	return false;
#endif
}

bool adxl36xIsInterruptActiveLow(const struct device* const dev)
{
#ifdef CONFIG_ADXL36X_IRQ_ENABLE
	const struct adxl36x_config* const device_config = dev->config;
	return (device_config->int1_gpio.dt_flags & GPIO_ACTIVE_LOW) != 0;
#else
	ARG_UNUSED(dev);
	return false;
#endif
}

/* ---------------------------------------------------------------------------
 * Public - Sample Counting
 * ------------------------------------------------------------------------- */

uint32_t adxl36xGetSampleCount(const struct device* const dev)
{
	const struct adxl36x_data* const data = dev->data;
	return (uint32_t)atomic_get(&data->sample_count);
}

void adxl36xResetSampleCount(const struct device* const dev)
{
	struct adxl36x_data* const data = dev->data;
	atomic_set(&data->sample_count, 0);
}

uint32_t adxl36xGetAndResetSampleCount(const struct device* const dev)
{
	struct adxl36x_data* const data = dev->data;
	return (uint32_t)atomic_clear(&data->sample_count);
}

/* ---------------------------------------------------------------------------
 * Static - Register I/O
 * ------------------------------------------------------------------------- */

static int regRead(const struct device* const dev, const uint8_t register_address, uint8_t* const register_value)
{
	const struct adxl36x_config* const device_config = dev->config;
	return i2c_reg_read_byte_dt(&device_config->i2c, register_address, register_value);
}

static int regWrite(const struct device* const dev, const uint8_t register_address, const uint8_t register_value)
{
	const struct adxl36x_config* const device_config = dev->config;
	return i2c_reg_write_byte_dt(&device_config->i2c, register_address, register_value);
}

static int regUpdate(const struct device* const dev, const uint8_t register_address, const uint8_t mask, const uint8_t register_value)
{
	uint8_t old_value;
	const int status = regRead(dev, register_address, &old_value);
	if (status < 0)
		return status;

	const uint8_t new_value = (old_value & ~mask) | (register_value & mask);
	if (new_value == old_value)
		return 0;

	return regWrite(dev, register_address, new_value);
}

static int burstRead(const struct device* const dev, const uint8_t start_register, uint8_t* const read_buffer, const size_t length)
{
	const struct adxl36x_config* const device_config = dev->config;
	return i2c_burst_read_dt(&device_config->i2c, start_register, read_buffer, length);
}

/** @brief Sign-extend a 14-bit unsigned value to a signed 16-bit integer */
static int16_t signExtend14(const uint16_t raw)
{
	const uint16_t masked = raw & ADXL36X_FIFO_DATA_14B_MASK;
	if (masked & ADXL36X_FIFO_SIGN_BIT_14B)
		return (int16_t)(masked | ADXL36X_FIFO_SIGN_EXTEND_14B);

	return (int16_t)masked;
}

/* ---------------------------------------------------------------------------
 * Static - ISR + Auto-Fetch
 * ------------------------------------------------------------------------- */

#ifdef CONFIG_ADXL36X_IRQ_ENABLE
static void gpioCallback(const struct device* const port, struct gpio_callback* const cb, const gpio_port_pins_t pins)
{
	struct adxl36x_data* const data =
		CONTAINER_OF(cb, struct adxl36x_data, gpio_cb);
	ARG_UNUSED(port);
	ARG_UNUSED(pins);

	atomic_inc(&data->sample_count);

	// Auto-submit fetch work when streaming
	if (atomic_get(&data->is_streaming))
		k_work_submit(&data->fetch_work);

	// ISR callback (snapshot to avoid TOCTOU)
	const ADXL36xInterruptCallback cb_fn = data->int_cb;
	void* const ud = data->int_user_data;
	if (cb_fn)
		cb_fn(data->dev, ud);
}

static void fetchWorkHandler(struct k_work* const work)
{
	struct adxl36x_data* const data =
		CONTAINER_OF(work, struct adxl36x_data, fetch_work);
	const struct device* const dev = data->dev;

	if (!atomic_get(&data->is_streaming))
		return;

	// Read FIFO entry count
	uint16_t entries = 0;
	if (adxl36xGetFIFOEntries(dev, &entries) < 0 || entries == 0)
		return;

	int16_t x = 0, y = 0, z = 0;
	bool has_x = false, has_y = false, has_z = false;

	while (entries > 0)
	{
		const size_t chunk = MIN((size_t)entries, (size_t)FETCH_CHUNK_ENTRIES);
		const int status = adxl36xReadFIFO(dev, data->fetch_buf,
		                                   chunk * ADXL36X_BYTES_PER_FIFO_ENTRY);
		if (status < 0)
		{
			LOG_ERR("FIFO read failed: %d", status);
			break;
		}

		for (size_t entry_index = 0; entry_index < chunk; entry_index++)
		{
			// I2C FIFO gives MSB first (channel ID + upper bits)
			const uint8_t data_high_byte = data->fetch_buf[entry_index * 2];
			const uint8_t data_low_byte = data->fetch_buf[entry_index * 2 + 1];
			const uint8_t channel_id = (data_high_byte >> 6) & 0x03;

			// Extract 14-bit value and sign-extend to int16_t
			const int16_t raw_axis_value = signExtend14(
				((uint16_t)(data_high_byte & 0x3F) << 8) | data_low_byte);

			switch (channel_id)
			{
			case ADXL36X_FIFO_AXIS_X:
				x = raw_axis_value;
				has_x = true;
				has_y = false;
				has_z = false;
				break;
			case ADXL36X_FIFO_AXIS_Y:
				y = raw_axis_value;
				has_y = true;
				break;
			case ADXL36X_FIFO_AXIS_Z:
				z = raw_axis_value;
				has_z = true;
				break;
			default:
				// Temperature or unknown - skip
				break;
			}

			// Complete XYZ triplet -> push to cache
			if (has_x && has_y && has_z)
			{
				k_spinlock_key_t key = k_spin_lock(&data->cache_lock);
				if (data->cache_count < CONFIG_ADXL36X_CACHE_SIZE)
				{
					data->cache[data->cache_head].x = x;
					data->cache[data->cache_head].y = y;
					data->cache[data->cache_head].z = z;
					data->cache_head = (data->cache_head + 1)
					                   % CONFIG_ADXL36X_CACHE_SIZE;
					data->cache_count++;
				}
				else
				{
					// Overwrite oldest on overflow
					static bool is_overflow_logged;
					if (!is_overflow_logged)
					{
						LOG_WRN("Sample cache overflow, dropping oldest samples");
						is_overflow_logged = true;
					}
					data->cache[data->cache_head].x = x;
					data->cache[data->cache_head].y = y;
					data->cache[data->cache_head].z = z;
					data->cache_head = (data->cache_head + 1)
					                   % CONFIG_ADXL36X_CACHE_SIZE;
					data->cache_tail = data->cache_head;
				}
				k_spin_unlock(&data->cache_lock, key);
				has_x = false;
				has_y = false;
				has_z = false;
			}
		}

		entries -= (uint16_t)chunk;
	}

	// Notify application
	const ADXL36xDataCallback cb = data->data_cb;
	void* const ud = data->data_cb_user_data;
	if (cb && data->cache_count > 0)
		cb(dev, data->cache_count, ud);
}
#endif // CONFIG_ADXL36X_IRQ_ENABLE

/* ---------------------------------------------------------------------------
 * Static - Zephyr Sensor API
 * ------------------------------------------------------------------------- */

static int32_t rangeLsbPerG(const ADXL36xRange range)
{
	switch (range)
	{
	case ADXL36X_RANGE_4G:
		return ADXL36X_ACCEL_4G_LSB_PER_G;
	case ADXL36X_RANGE_8G:
		return ADXL36X_ACCEL_8G_LSB_PER_G;
	default:
		return ADXL36X_ACCEL_2G_LSB_PER_G;
	}
}

static void rawToSensorValue(const int16_t raw, const ADXL36xRange range, struct sensor_value* const val)
{
	const int32_t lsb_per_g = rangeLsbPerG(range);

	// micro-m/s^2 = raw * 9806650 / lsb_per_g
	const int64_t micro_ms2 = (int64_t)raw * 9806650LL / lsb_per_g;
	val->val1 = (int32_t)(micro_ms2 / 1000000LL);
	val->val2 = (int32_t)(micro_ms2 % 1000000LL);
}

static int sensorSampleFetch(const struct device* const dev, const enum sensor_channel chan)
{
	ARG_UNUSED(chan);
	return adxl36xReadXYZ(dev, NULL, NULL, NULL);
}

static int sensorChannelGet(const struct device* const dev, const enum sensor_channel chan, struct sensor_value* const val)
{
	const struct adxl36x_data* const data = dev->data;

	switch (chan)
	{
	case SENSOR_CHAN_ACCEL_X:
		rawToSensorValue(data->x_raw, data->range, val);
		return 0;
	case SENSOR_CHAN_ACCEL_Y:
		rawToSensorValue(data->y_raw, data->range, val);
		return 0;
	case SENSOR_CHAN_ACCEL_Z:
		rawToSensorValue(data->z_raw, data->range, val);
		return 0;
	case SENSOR_CHAN_ACCEL_XYZ:
		rawToSensorValue(data->x_raw, data->range, &val[0]);
		rawToSensorValue(data->y_raw, data->range, &val[1]);
		rawToSensorValue(data->z_raw, data->range, &val[2]);
		return 0;
	default:
		return -ENOTSUP;
	}
}

static int sensorAttrSet(const struct device* const dev, const enum sensor_channel chan, const enum sensor_attribute attr, const struct sensor_value* const val)
{
	ARG_UNUSED(chan);

	switch (attr)
	{
	case SENSOR_ATTR_SAMPLING_FREQUENCY:
	{
		const int32_t hz = val->val1;
		const ADXL36xODR odr = (hz <= 12)  ? ADXL36X_ODR_12P5HZ
		                     : (hz <= 25)  ? ADXL36X_ODR_25HZ
		                     : (hz <= 50)  ? ADXL36X_ODR_50HZ
		                     : (hz <= 100) ? ADXL36X_ODR_100HZ
		                     : (hz <= 200) ? ADXL36X_ODR_200HZ
		                     :               ADXL36X_ODR_400HZ;
		return adxl36xSetODR(dev, odr);
	}
	case SENSOR_ATTR_FULL_SCALE:
	{
		const int32_t g = val->val1;
		const ADXL36xRange range = (g <= 2) ? ADXL36X_RANGE_2G
		                         : (g <= 4) ? ADXL36X_RANGE_4G
		                         :            ADXL36X_RANGE_8G;
		return adxl36xSetRange(dev, range);
	}
	default:
		return -ENOTSUP;
	}
}

static const struct sensor_driver_api adxl36x_sensor_api =
{
	.sample_fetch = sensorSampleFetch,
	.channel_get = sensorChannelGet,
	.attr_set = sensorAttrSet,
};

/* ---------------------------------------------------------------------------
 * Static - Device Init
 * ------------------------------------------------------------------------- */

static int adxl36xDeviceInit(const struct device* const dev)
{
	const struct adxl36x_config* const device_config = dev->config;
	struct adxl36x_data* const data = dev->data;

	// Verify I2C bus
	if (!i2c_is_ready_dt(&device_config->i2c))
	{
		LOG_ERR("I2C bus not ready");
		return -ENODEV;
	}

	// Verify Analog Devices ID
	uint8_t device_id_byte;
	int status = regRead(dev, ADXL36X_REG_DEVID_AD, &device_id_byte);
	if (status < 0)
	{
		LOG_ERR("Failed to read device ID: %d", status);
		return status;
	}

	if (device_id_byte != ADXL36X_DEVID_AD_VAL)
	{
		LOG_ERR("Unexpected device ID: 0x%02X (expected 0x%02X)",
		        device_id_byte, ADXL36X_DEVID_AD_VAL);
		return -ENODEV;
	}

	// Read Part ID for variant detection
	status = regRead(dev, ADXL36X_REG_PARTID, &data->part_id);
	if (status < 0)
	{
		LOG_WRN("Failed to read Part ID: %d", status);
		data->part_id = 0;
	}
	else
	{
		LOG_INF("Part ID: 0x%02X", data->part_id);
	}

	// Read Revision ID for logging
	uint8_t revision_id_byte;
	status = regRead(dev, ADXL36X_REG_REVID, &revision_id_byte);
	if (status == 0)
		LOG_INF("Revision ID: 0x%02X", revision_id_byte);

	// Soft reset
	status = adxl36xReset(dev);
	if (status < 0)
	{
		LOG_ERR("Reset failed: %d", status);
		return status;
	}

	// Apply default ODR from Kconfig
	status = adxl36xSetODR(dev, (ADXL36xODR)device_config->odr);
	if (status < 0)
	{
		LOG_ERR("Failed to set ODR: %d", status);
		return status;
	}

	// Apply default range from Kconfig
	status = adxl36xSetRange(dev, (ADXL36xRange)device_config->range);
	if (status < 0)
	{
		LOG_ERR("Failed to set range: %d", status);
		return status;
	}

	atomic_set(&data->is_streaming, 0);

#ifdef CONFIG_ADXL36X_IRQ_ENABLE
	data->dev = dev;
	data->int_cb = NULL;
	data->int_user_data = NULL;
	data->data_cb = NULL;
	data->data_cb_user_data = NULL;
	data->cache_head = 0;
	data->cache_tail = 0;
	data->cache_count = 0;
	atomic_set(&data->sample_count, 0);

	k_work_init(&data->fetch_work, fetchWorkHandler);

	if (device_config->int1_gpio.port)
	{
		if (!gpio_is_ready_dt(&device_config->int1_gpio))
		{
			LOG_ERR("INT1 GPIO not ready");
			return -ENODEV;
		}

		status = gpio_pin_configure_dt(&device_config->int1_gpio, GPIO_INPUT);
		if (status < 0)
		{
			LOG_ERR("INT1 GPIO config failed: %d", status);
			return status;
		}

		gpio_init_callback(&data->gpio_cb, gpioCallback,
		                   BIT(device_config->int1_gpio.pin));

		status = gpio_add_callback(device_config->int1_gpio.port, &data->gpio_cb);
		if (status < 0)
		{
			LOG_ERR("INT1 GPIO callback add failed: %d", status);
			return status;
		}

		// Explicitly disable interrupt until streaming starts
		gpio_pin_interrupt_configure_dt(&device_config->int1_gpio, GPIO_INT_DISABLE);

		LOG_DBG("INT1 GPIO configured on %s pin %d",
		        device_config->int1_gpio.port->name, device_config->int1_gpio.pin);
	}
#else
	atomic_set(&data->sample_count, 0);
#endif // CONFIG_ADXL36X_IRQ_ENABLE

	LOG_INF("ADXL36x initialized (addr 0x%02X, Part 0x%02X)",
	        device_config->i2c.addr, data->part_id);
	return 0;
}

/* ---------------------------------------------------------------------------
 * Device Tree Instantiation
 * ------------------------------------------------------------------------- */

#define ADXL36X_DEFINE(inst)                                                    \
	static struct adxl36x_data adxl36x_data_##inst;                         \
	static const struct adxl36x_config adxl36x_config_##inst =              \
	{                                                                       \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                              \
		IF_ENABLED(CONFIG_ADXL36X_IRQ_ENABLE, (                         \
			.int1_gpio = GPIO_DT_SPEC_INST_GET_OR(inst,             \
					int1_gpios, {0}),                       \
		))                                                              \
		.odr = CONFIG_ADXL36X_ODR,                                      \
		.range = CONFIG_ADXL36X_RANGE,                                  \
	};                                                                      \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, adxl36xDeviceInit, NULL,             \
	                             &adxl36x_data_##inst,                      \
	                             &adxl36x_config_##inst,                    \
	                             POST_KERNEL,                               \
	                             CONFIG_SENSOR_INIT_PRIORITY,               \
	                             &adxl36x_sensor_api);

DT_INST_FOREACH_STATUS_OKAY(ADXL36X_DEFINE)
