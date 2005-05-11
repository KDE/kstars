/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#define APN_DRIVER_VERSION				"2.0.13.0"

#define APN_HBINNING_MAX				10
#define APN_VBINNING_MAX				2048

#define APN_TIMER_RESOLUTION			0.00000256
#define APN_PERIOD_TIMER_RESOLUTION		0.000000040

#define	APN_TIMER_OFFSET_COUNT			3

#define APN_SEQUENCE_DELAY_RESOLUTION	0.000327
#define APN_SEQUENCE_DELAY_LIMIT		21.429945

#define APN_EXPOSURE_TIME_MIN			0.00001		// 10us is the defined min.
#define APN_EXPOSURE_TIME_MAX			10990.0		// seconds

#define APN_TDI_RATE_RESOLUTION			0.00000512
#define APN_TDI_RATE_MIN				0.00000512
#define APN_TDI_RATE_MAX				0.336

#define APN_VOLTAGE_RESOLUTION			0.00439453

#define APN_SHUTTER_CLOSE_DIFF			0.00001024

#define APN_STROBE_POSITION_MIN			0.00000331
#define APN_STROBE_POSITION_MAX			0.1677
#define APN_STROBE_PERIOD_MIN			0.000000045
#define APN_STROBE_PERIOD_MAX			0.0026

#define APN_TEMP_COUNTS					4096
#define APN_TEMP_KELVIN_SCALE_OFFSET	273.16

#define APN_TEMP_SETPOINT_MIN			213
#define APN_TEMP_SETPOINT_MAX			313

#define APN_TEMP_HEATSINK_MIN			240
#define APN_TEMP_HEATSINK_MAX			340

#define APN_TEMP_SETPOINT_ZERO_POINT	2458
#define APN_TEMP_HEATSINK_ZERO_POINT	1351

#define APN_TEMP_DEGREES_PER_BIT		0.024414

#define APN_FAN_SPEED_OFF				0
#define APN_FAN_SPEED_LOW				3100
#define APN_FAN_SPEED_MEDIUM			3660
#define APN_FAN_SPEED_HIGH				4095
