// SPDX-License-Identifier: GPL-2.0+
/*!
 * Copyright (c) 2020-2024 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
 *
 * This file is part of tuxedo-drivers.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <https://www.gnu.org/licenses/>.
 */

/*
 * All numbers in hex format
 * 
 * Turn off (same as "set params" described below). This is called with all other bytes set to zero when turned-off.
 * Control: 08 01 00 00 00 00 00 00
 * 
 * Set brightness only
 * Control: 09 02 [brightness] 00 00 00 00 00
 * 
 * Announce 64 byte data chunks (for "user mode" 0x33). To be followed by specified number on chunks on interrupt endpoint.
 * Control: 12 00 00 [nr of chunks] 00 00 00 00
 * 
 * [nr of chunks]
 * 	1 -> 8
 * 
 * Announce row data chunk (for "user mode" 0x33). To be followed by 64 bytes of row data on interrupt endpoint.
 * Control: 16 00 [row number] 00 00 00 00 00
 * 
 * [row number]
 * 	0 -> 5
 * 
 * 
 * Most "special modes" uses seven colors that can be defined individually
 * Set color define
 * Control: 14 00 [color define number] [red] [green] [blue] 00 00
 * 
 * [color define number]
 * 	1 -> 7
 * 
 * 
 * Set params
 * Control: 08 [power state] [anim mode] [speed] [brightness] 08 [behaviour] 00
 * 
 * [power state]
 * 	01 off
 * 	02 on
 * 
 * [anim mode]
 * 	02 breath
 * 	03 wave ([behaviour]: direction)
 * 	04 reactive ([behaviour]: key mode)
 * 	05 rainbow (no color set)
 * 	06 ripple ([behaviour]: key mode)
 * 	09 marquee
 * 	0a raindrop
 * 	0e aurora ([behaviour]: key mode)
 * 	11 spark ([behaviour]: key mode)
 * 
 * 	33 per key control, needs additional "data announcing", then data on interrupt endpoint
 * 
 * [speed]
 * 	0a -> 01
 * 
 * [brightness]
 * 	00 -> 32
 * 
 * [behaviour]
 * 	00 when not used, otherwise dependent on [anim mode]
 * 
 * [behaviour]: direction
 * 	01 left to right
 * 	02 right to left
 * 	03 bottom to top
 * 	04 top to bottom
 * 
 * [behaviour]: key mode
 * 	00 key press
 * 	01 auto
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/usb.h>
#include <linux/hid.h>
#include <linux/dmi.h>
#include <linux/led-class-multicolor.h>
#include <linux/of.h>
#include <linux/hex.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include "color_contract.h"
#include "color_model.h"
#include "brightness_contract.h"

// USB HID control data write size
#define HID_DATA_SIZE 8

// led_classdev names, default and max brightness
//#define ITE8291_KBD_BRIGHTNESS_MAX	0xff
#define ITE8291_KBD_BRIGHTNESS_MAX	0x32
#define ITE8291_KBD_BRIGHTNESS_DEFAULT	0x00

#define ITE8291_KB_COLOR_DEFAULT_RED	0xff
#define ITE8291_KB_COLOR_MAX_RED	0xff
#define ITE8291_KB_COLOR_DEFAULT_GREEN	0xff
#define ITE8291_KB_COLOR_MAX_GREEN	0xff
#define ITE8291_KB_COLOR_DEFAULT_BLUE	0xff
#define ITE8291_KB_COLOR_MAX_BLUE	0xff
#define ITE8291_KB_COLOR_DEFAULT	((UNIWILL_KB_COLOR_DEFAULT_RED << 16) + (UNIWILL_KB_COLOR_DEFAULT_GREEN << 8) + UNIWILL_KB_COLOR_DEFAULT_BLUE)

#define ITE8291_LEDS_PER_ROW_MAX	21
// Data length needs one byte (0x00) initial padding for the sending function
// and one byte (also seemingly 0x00) before the color data starts
#define ITE8291_ROW_DATA_PADDING	(1 + 1)
#define ITE8291_ROW_DATA_LENGTH		(ITE8291_ROW_DATA_PADDING + (ITE8291_LEDS_PER_ROW_MAX * 3))
#define ITE8291_NR_ROWS			6

#define ITE8291_PARAM_MODE_USER		0x33

struct ite8291_driver_data_t {
	u16 bcd_device;
	struct hid_device *hid_dev;
	void *device_data;
	bool device_has_buffer_input_control;
	bool device_buffer_input;
	struct mutex io_lock;
	struct delayed_work frame_work;
	bool color_dirty;
	bool stopping;
	bool powered;
	u8 mode, speed, behaviour;
	u16 brightness_scale;
	u8 white_point[3];
	u8 color_profile[SLAYER_COLOR_MODEL_MAX];
	u16 color_profile_size;
	bool color_profile_active;
	u64 frame_count, brightness_count;
	int last_error;
	int (*device_add)(struct hid_device *);
	int (*device_remove)(struct hid_device *);
	int (*device_write_on)(struct hid_device *);
	int (*device_write_off)(struct hid_device *);
	int (*device_write_state)(struct hid_device *);
};

// Per key device specific defines
typedef u8 row_data_t[ITE8291_NR_ROWS][ITE8291_ROW_DATA_LENGTH];
struct ite8291_driver_data_perkey_t {
	row_data_t row_data;
	u8 brightness;
	u8 colors[ITE8291_NR_ROWS][ITE8291_LEDS_PER_ROW_MAX][3];
	struct led_classdev_mc mcled_cdevs[ITE8291_NR_ROWS][ITE8291_LEDS_PER_ROW_MAX];
	struct mc_subled mcled_cdevs_subleds[ITE8291_NR_ROWS][ITE8291_LEDS_PER_ROW_MAX][3];
};

static int ite8291_perkey_add(struct hid_device *);
static int ite8291_perkey_remove(struct hid_device *);
static int ite8291_perkey_write_on(struct hid_device *);
static int ite8291_perkey_write_off(struct hid_device *);
static int ite8291_perkey_write_state(struct hid_device *);

// Zones device specific defines
#define ITE8291_NR_ZONES 			4
#define ITE8291_KBD_ZONES_BRIGHTNESS_MAX	0x32
#define ITE8291_KBD_ZONES_BRIGHTNESS_DEFAULT	0x00
struct ite8291_driver_data_zones_t {
	u8 brightness;
	struct led_classdev_mc mcled_cdevs[ITE8291_NR_ZONES];
	struct mc_subled mcled_cdevs_subleds[ITE8291_NR_ZONES][3];
};

static int ite8291_zones_add(struct hid_device *);
static int ite8291_zones_remove(struct hid_device *);
static int ite8291_zones_write_on(struct hid_device *);
static int ite8291_zones_write_off(struct hid_device *);
static int ite8291_zones_write_state(struct hid_device *);

/**
 * Color scaling quirk list
 */
static void color_scaling(struct hid_device *hdev, u8 *red, u8 *green, u8 *blue, bool row_col_set, u8 row, u8 col)
{
	struct ite8291_driver_data_t *driver_data = hid_get_drvdata(hdev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
	/* Preserve the RGB values selected by KDE/Slayer on the verified R9T.
	 * Generic TUXEDO white-balance guesses halve green and blue here. */
	if (dmi_match(DMI_SYS_VENDOR, "GAME GARAJ") &&
	    dmi_match(DMI_PRODUCT_NAME, "SLAYER R9T") &&
	    dmi_match(DMI_PRODUCT_SKU, "R9T V1") &&
	    hdev->product == 0x600b && driver_data->bcd_device == 0x0003) {
		return;
	} else if (dmi_match(DMI_PRODUCT_SKU, "STEPOL1XA04") && hdev->product == 0x600a) {
		*red = (126 * *red) / 255;
	} else if (dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XI05") && hdev->product == 0x600a) {
		*red = (200 * *red) / 255;
		*blue = (220 * *blue) / 255;

		// top row: reduce some additional violett
		if(row_col_set && row == 5) {
			*red = ( 230 * *red) / 255;
			*blue = ( 200 * *blue) / 255;
		}
	} else if (dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XA05")) {
		*red = (128 * *red) / 255;
	} else if (dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XI05") &&
		   hdev->product == 0xce00 && driver_data->bcd_device == 0x0002) {
		*red = (255 * *red) / 255;
		*green = (220 * *green) / 255;
		*blue = (200 * *blue) / 255;
	} else if (dmi_match(DMI_PRODUCT_SKU, "STELLARIS17I06") &&
		   hdev->product == 0xce00 && driver_data->bcd_device == 0x0002) {
		*green = (180 * *green) / 255;
		*blue = (180 * *blue) / 255;
	} else if (dmi_match(DMI_PRODUCT_SKU, "STELLARIS17I06") &&
		   hdev->product == 0x600a) {
		*red = (200 * *red) / 255;
		*blue = (220 * *blue) / 255;
	} else if ((dmi_match(DMI_PRODUCT_SKU, "STELLSL15I06") || dmi_match(DMI_PRODUCT_SKU, "STELLARIS16I06"))
		   && hdev->product == 0x600b) {
		// all keys: reduce pink
		*red = (155 * *red) / 255;
		*blue = (140 * *blue) / 255;

		// bottom row: reduce green (adding red and blue)
		if (row_col_set && row == 0) {
			*red = (279 * *red) / 255;
			*blue = (282 * *blue) / 255;
		}

		// top row: reduce violett
		if(row_col_set && row == 5) {
			*red = (148 * *red) / 255;
			*blue = (137 * *blue) / 255;
		}
	} else if ((dmi_match(DMI_PRODUCT_SKU, "STELLARIS16I07") ||
		    dmi_match(DMI_PRODUCT_SKU, "STELLARIS16A07") ||
		    dmi_match(DMI_BOARD_NAME, "X6AR55xU"))
		   && hdev->product == 0x600b) {
		// all keys: reduce pink
		*red = (170 * *red) / 255;
		*blue = (125 * *blue) / 255;
	} else if (dmi_match(DMI_BOARD_NAME, "X5KK45xS_X5SP45xS")) {
		*red = (180 * *red) / 255;
	} else if (dmi_match(DMI_BOARD_NAME, "X5AR45xS")) {
		// No scaling
	} else if (dmi_match(DMI_BOARD_NAME, "X6KK45xU_X6SP45xU")) {
		*red = (187 * *red) / 255;
		*blue = (153 * *blue) / 255;
	} else if (dmi_match(DMI_BOARD_NAME, "XxAF5xxx") ||
		   dmi_match(DMI_BOARD_NAME, "XxAF5xxx_mLED")) {
		// No scaling
	} else {
		*green = (126 * *green) / 255;
		*blue = (120 * *blue) / 255;
	}
#endif
}

/**
 * Set color for specified [row, column] in row based data structure
 * 
 * @param row_data Data structure to fill
 * @param row Row number 0 - 5
 * @param column Column number 0 - 20
 * @param red Red brightness 0x00 - 0xff
 * @param green Green brightness 0x00 - 0xff
 * @param blue Blue brightness 0x00 - 0xff
 * 
 * @returns 0 on success, otherwise error
 */
static int row_data_set(struct hid_device *hdev, row_data_t row_data, int row, int column, u8 red, u8 green, u8 blue)
{
	int column_index_red, column_index_green, column_index_blue;
	struct ite8291_driver_data_t *driver = hid_get_drvdata(hdev);
	u8 rgb[3] = {red,green,blue}, corrected[3];
	if(driver->color_profile_active && slayer_color_model_apply(driver->color_profile,rgb,corrected)) {
		red=corrected[0];green=corrected[1];blue=corrected[2];
	} else {
		red = slayer_color_channel(red, driver->white_point[0]);
		green = slayer_color_channel(green, driver->white_point[1]);
		blue = slayer_color_channel(blue, driver->white_point[2]);
	}

	color_scaling(hdev, &red, &green, &blue, true, row, column);

	if (row < 0 || row >= ITE8291_NR_ROWS)
		return -EINVAL;

	if (column < 0 || column >= ITE8291_LEDS_PER_ROW_MAX)
		return -EINVAL;

	column_index_red = ITE8291_ROW_DATA_PADDING + (2 * ITE8291_LEDS_PER_ROW_MAX) + column;
	column_index_green = ITE8291_ROW_DATA_PADDING + (1 * ITE8291_LEDS_PER_ROW_MAX) + column;
	column_index_blue = ITE8291_ROW_DATA_PADDING + (0 * ITE8291_LEDS_PER_ROW_MAX) + column;

	row_data[row][column_index_red] = red;
	row_data[row][column_index_green] = green;
	row_data[row][column_index_blue] = blue;

	return 0;
}

/* HID buffers include the report-number byte. This device's descriptor has
 * an unnumbered eight-byte feature report; command 08/09 is payload, not ID. */
static int ite8291_write_control(struct hid_device *hdev, u8 *ctrl_data)
{
	u8 *buf;
	int ret;
	bool numbered;
	if (!hdev) return -ENODEV;
	numbered = hdev->report_enum[HID_FEATURE_REPORT].numbered;
	buf = kmalloc(HID_DATA_SIZE + !numbered, GFP_KERNEL);
	if (!buf) return -ENOMEM;
	if (numbered) memcpy(buf, ctrl_data, HID_DATA_SIZE);
	else { buf[0] = 0; memcpy(buf + 1, ctrl_data, HID_DATA_SIZE); }
	ret = hid_hw_raw_request(hdev, numbered ? ctrl_data[0] : 0, buf,
				HID_DATA_SIZE + !numbered, HID_FEATURE_REPORT, HID_REQ_SET_REPORT);
	kfree(buf);
	return ret < 0 ? ret : ret == HID_DATA_SIZE + !numbered ? 0 : -EIO;
}
static int ite8291_write_brightness(struct hid_device *hdev, u8 brightness)
{
	u8 command[] = {0x09, 0x02, brightness, 0, 0, 0, 0, 0};
	return ite8291_write_control(hdev, command);
}



/**
 * Write color (and brightness) to the whole keyboard from row data
 */
static int ite8291_write_rows(struct hid_device *hdev, row_data_t row_data, u8 brightness)
{
	int result = 0, row_index;
	u8 ctrl_params[] = { 0x08,
			     0x02,
			     ITE8291_PARAM_MODE_USER,
			     0x00,
			     brightness > ITE8291_KBD_BRIGHTNESS_MAX ? ITE8291_KBD_BRIGHTNESS_MAX : brightness,
			     0x00,
			     0x00,
			     0x00 };
	u8 ctrl_announce_row_data[] = { 0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	if (hdev == NULL)
		return -ENODEV;

	result = ite8291_write_control(hdev, ctrl_params);
	if (result) return result;

	for (row_index = 0; row_index < ITE8291_NR_ROWS; ++row_index) {
		ctrl_announce_row_data[2] = row_index;
		result = ite8291_write_control(hdev, ctrl_announce_row_data);
		if (result) return result;
		result = hdev->ll_driver->output_report(
			hdev, row_data[row_index], ITE8291_ROW_DATA_LENGTH);
		if (result != ITE8291_ROW_DATA_LENGTH)
			return result < 0 ? result : -EIO;
	}
	if (result > 0)
		result = 0;

	return result;
}

static void stop_hw(struct hid_device *hdev)
{
	hid_hw_power(hdev, PM_HINT_NORMAL);
	hid_hw_close(hdev);
	hid_hw_stop(hdev);
}

static int start_hw(struct hid_device *hdev)
{
	int result;
	result = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (result) {
		pr_err("hid_hw_start failed\n");
		goto err_stop_hw;
	}

	hid_hw_power(hdev, PM_HINT_FULLON);

	result = hid_hw_open(hdev);
	if (result) {
		pr_err("hid_hw_open failed\n");
		goto err_stop_hw;
	}

	return 0;

err_stop_hw:
	stop_hw(hdev);
	return result;
}

/* Caller owns io_lock. A burst of per-key sysfs writes becomes one USB
 * frame; brightness uses its own command and preserves hardware effects. */
static void cache_brightness(struct ite8291_driver_data_t *driver, u8 brightness)
{
	struct ite8291_driver_data_perkey_t *data = driver->device_data;
	int row, col;
	data->brightness = brightness;
	for (row = 0; row < ITE8291_NR_ROWS; ++row)
		for (col = 0; col < ITE8291_LEDS_PER_ROW_MAX; ++col)
			WRITE_ONCE(data->mcled_cdevs[row][col].led_cdev.brightness, brightness);
}
/* A transient physical limit. Never change the LED class value consumed by
 * UPower/PowerDevil, and never emit a brightness notification for an idle fade. */
static u8 effective_brightness(struct ite8291_driver_data_t *driver, u8 value)
{
	return slayer_effective_brightness(value, driver->brightness_scale);
}
static int flush_frame(struct ite8291_driver_data_t *driver)
{
	int ret = 0;
	if (driver->color_dirty) {
		ret = driver->device_write_state(driver->hid_dev);
		if (!ret) { driver->color_dirty = false; driver->frame_count++; kobject_uevent(&driver->hid_dev->dev.kobj, KOBJ_CHANGE); }
	}
	driver->last_error = ret;
	return ret;
}
static void write_frame_work(struct work_struct *work)
{
	struct ite8291_driver_data_t *driver = container_of(to_delayed_work(work), struct ite8291_driver_data_t, frame_work);
	mutex_lock(&driver->io_lock);
	if (!driver->stopping && !driver->device_buffer_input && flush_frame(driver))
		dev_err_ratelimited(&driver->hid_dev->dev, "RGB frame write failed: %d\n", driver->last_error);
	mutex_unlock(&driver->io_lock);
}
static int leds_set_brightness_mc(struct led_classdev *led_cdev, enum led_brightness brightness)
{
	struct led_classdev_mc *mc = lcdev_to_mccdev(led_cdev);
	struct hid_device *hdev = to_hid_device(led_cdev->dev->parent);
	struct ite8291_driver_data_t *driver = hid_get_drvdata(hdev);
	struct ite8291_driver_data_perkey_t *data = driver->device_data;
	unsigned int row = mc->subled_info[0].channel / ITE8291_LEDS_PER_ROW_MAX;
	unsigned int col = mc->subled_info[0].channel % ITE8291_LEDS_PER_ROW_MAX;
	u8 rgb[] = {mc->subled_info[0].intensity, mc->subled_info[1].intensity, mc->subled_info[2].intensity};
	int ret = 0;
	u8 previous;
	mutex_lock(&driver->io_lock);
	previous = data->brightness;
	if (driver->stopping) goto out;
	if (memcmp(data->colors[row][col], rgb, sizeof(rgb))) {
		memcpy(data->colors[row][col], rgb, sizeof(rgb));
		row_data_set(hdev, data->row_data, row, col, rgb[0], rgb[1], rgb[2]);
		driver->color_dirty = true;
		driver->mode = ITE8291_PARAM_MODE_USER;
	}
	cache_brightness(driver, brightness);
	if (!driver->device_buffer_input) {
		if (driver->color_dirty) mod_delayed_work(system_wq, &driver->frame_work, msecs_to_jiffies(16));
		else if (brightness != previous) {
			if (!driver->powered && brightness) {
				u8 command[] = {8, 2, driver->mode, driver->speed, effective_brightness(driver, brightness), 8, driver->behaviour, 0};
				ret = ite8291_write_control(hdev, command);
				if (!ret) driver->powered = true;
			} else ret = ite8291_write_brightness(hdev, effective_brightness(driver, brightness));
			if (!ret) {
				driver->brightness_count++;
				led_classdev_notify_brightness_hw_changed(&data->mcled_cdevs[0][0].led_cdev, brightness);
			}
			driver->last_error = ret;
		}
	}
out:
	if (ret) cache_brightness(driver, previous);
	mutex_unlock(&driver->io_lock);
	return ret;
}

static int register_leds(struct hid_device *hdev)
{
	int res, i, j, k, l;
	struct ite8291_driver_data_t *ite8291_driver_data = hid_get_drvdata(hdev);
	struct ite8291_driver_data_perkey_t *device_data = ite8291_driver_data->device_data;

	for (i = 0; i < ITE8291_NR_ROWS; ++i) {
		for (j = 0; j < ITE8291_LEDS_PER_ROW_MAX; ++j) {
			device_data->mcled_cdevs[i][j].led_cdev.name = "rgb:" LED_FUNCTION_KBD_BACKLIGHT;
			device_data->mcled_cdevs[i][j].led_cdev.max_brightness = ITE8291_KBD_BRIGHTNESS_MAX;
			device_data->mcled_cdevs[i][j].led_cdev.brightness_set_blocking = &leds_set_brightness_mc;
			if (i == 0 && j == 0) device_data->mcled_cdevs[i][j].led_cdev.flags |= LED_BRIGHT_HW_CHANGED;
			device_data->mcled_cdevs[i][j].led_cdev.brightness = ITE8291_KBD_BRIGHTNESS_DEFAULT;
			device_data->mcled_cdevs[i][j].num_colors = 3;
			device_data->mcled_cdevs[i][j].subled_info = device_data->mcled_cdevs_subleds[i][j];
			device_data->mcled_cdevs[i][j].subled_info[0].color_index = LED_COLOR_ID_RED;
			device_data->mcled_cdevs[i][j].subled_info[0].intensity = ITE8291_KB_COLOR_DEFAULT_RED;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(7, 2, 0)
			device_data->mcled_cdevs[i][j].subled_info[0].max_intensity = ITE8291_KB_COLOR_MAX_RED;
#endif
			device_data->mcled_cdevs[i][j].subled_info[0].channel = ITE8291_LEDS_PER_ROW_MAX * i + j;
			device_data->mcled_cdevs[i][j].subled_info[1].color_index = LED_COLOR_ID_GREEN;
			device_data->mcled_cdevs[i][j].subled_info[1].intensity = ITE8291_KB_COLOR_DEFAULT_GREEN;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(7, 2, 0)
			device_data->mcled_cdevs[i][j].subled_info[1].max_intensity = ITE8291_KB_COLOR_MAX_GREEN;
#endif
			device_data->mcled_cdevs[i][j].subled_info[1].channel = ITE8291_LEDS_PER_ROW_MAX * i + j;
			device_data->mcled_cdevs[i][j].subled_info[2].color_index = LED_COLOR_ID_BLUE;
			device_data->mcled_cdevs[i][j].subled_info[2].intensity = ITE8291_KB_COLOR_DEFAULT_BLUE;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(7, 2, 0)
			device_data->mcled_cdevs[i][j].subled_info[2].max_intensity = ITE8291_KB_COLOR_MAX_BLUE;
#endif
			device_data->mcled_cdevs[i][j].subled_info[2].channel = ITE8291_LEDS_PER_ROW_MAX * i + j;

			res = devm_led_classdev_multicolor_register(&hdev->dev, &device_data->mcled_cdevs[i][j]);
			if (res) {
				for (k = 0; k <= i; ++k) {
					for (l = 0; l <= j; ++l) {
						devm_led_classdev_multicolor_unregister(&hdev->dev, &device_data->mcled_cdevs[i][j]);
					}
				}
				return res;
			}
		}
	}
	return 0;
}

static void unregister_leds(struct hid_device *hdev) {
	int i, j;
	struct ite8291_driver_data_t *ite8291_driver_data = hid_get_drvdata(hdev);
	struct ite8291_driver_data_perkey_t *device_data = ite8291_driver_data->device_data;

	for (i = 0; i < ITE8291_NR_ROWS; ++i) {
		for (j = 0; j < ITE8291_LEDS_PER_ROW_MAX; ++j) {
			devm_led_classdev_multicolor_unregister(&hdev->dev, &device_data->mcled_cdevs[i][j]);
		}
	}
}

static int ite8291_perkey_add(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *driver_data;
	struct ite8291_driver_data_perkey_t *perkey_data;
	int result, i, j;

	driver_data = hid_get_drvdata(hdev);

	perkey_data = devm_kzalloc(&hdev->dev, sizeof(*perkey_data), GFP_KERNEL);
	if (!perkey_data)
		return -ENOMEM;

	driver_data->device_data = perkey_data;

	perkey_data->brightness = ITE8291_KBD_BRIGHTNESS_DEFAULT;
	for (i = 0; i < ITE8291_NR_ROWS; ++i) {
		for (j = 0; j < ITE8291_LEDS_PER_ROW_MAX; ++j) {
			memset(perkey_data->colors[i][j], 255, 3);
			row_data_set(hdev, perkey_data->row_data, i, j,
				     ITE8291_KB_COLOR_DEFAULT_RED,
				     ITE8291_KB_COLOR_DEFAULT_GREEN,
				     ITE8291_KB_COLOR_DEFAULT_BLUE);
		}
	}

	result = register_leds(hdev);
	if (result)
		return result;

	return 0;
}

static int ite8291_perkey_remove(struct hid_device *hdev)
{
	unregister_leds(hdev);
	return 0;
}

static int ite8291_perkey_write_on(struct hid_device *hdev)
{
	return 0;
}

static int ite8291_perkey_write_off(struct hid_device *hdev)
{
	u8 ctrl_params_off[] = {0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	return ite8291_write_control(hdev, ctrl_params_off);
}

static int ite8291_perkey_write_state(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *driver_data = hid_get_drvdata(hdev);
	struct ite8291_driver_data_perkey_t *device_data = driver_data->device_data;
	int ret;
	if (driver_data->mode == ITE8291_PARAM_MODE_USER) {
		ret = ite8291_write_rows(hdev, device_data->row_data, effective_brightness(driver_data, device_data->brightness));
	} else {
		u8 command[] = {8, driver_data->powered ? 2 : 1, driver_data->mode,
			driver_data->speed, effective_brightness(driver_data, device_data->brightness), 8, driver_data->behaviour, 0};
		ret = ite8291_write_control(hdev, command);
	}
	if (!ret && driver_data->mode == ITE8291_PARAM_MODE_USER) driver_data->powered = true;
	return ret;
}

static void leds_zones_set_brightness_mc(struct led_classdev *led_cdev, enum led_brightness brightness) {
	int i;
	struct led_classdev_mc *mcled_cdev = lcdev_to_mccdev(led_cdev);
	struct device *dev = led_cdev->dev->parent;
	struct hid_device *hdev = to_hid_device(dev);
	struct ite8291_driver_data_t *driver_data = hid_get_drvdata(hdev);
	struct ite8291_driver_data_zones_t *device_data = driver_data->device_data;

	pr_debug("leds_set_brightness_mc: channel: %d, brightness: %d, saved brightness: %d, red: %d, green: %d, blue: %d\n",
		 mcled_cdev->subled_info[0].channel, brightness, device_data->brightness, mcled_cdev->subled_info[0].intensity,
		 mcled_cdev->subled_info[1].intensity, mcled_cdev->subled_info[2].intensity);

	device_data->brightness = brightness;

	for (i = 0; i < ITE8291_NR_ZONES; ++i) {
		device_data->mcled_cdevs[i].led_cdev.brightness = brightness;
	}

	ite8291_zones_write_state(hdev);
}

static int ite8291_zones_add(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *driver_data;
	struct ite8291_driver_data_zones_t *zones_data;
	int result, i, k;

	driver_data = hid_get_drvdata(hdev);

	zones_data = devm_kzalloc(&hdev->dev, sizeof(*zones_data), GFP_KERNEL);
	if (!zones_data)
		return -ENOMEM;

	driver_data->device_data = zones_data;

	for (i = 0; i < ITE8291_NR_ZONES; ++i) {
		zones_data->mcled_cdevs[i].led_cdev.name = "rgb:" LED_FUNCTION_KBD_BACKLIGHT;
		zones_data->mcled_cdevs[i].led_cdev.max_brightness = ITE8291_KBD_ZONES_BRIGHTNESS_MAX;
		zones_data->mcled_cdevs[i].led_cdev.brightness_set = &leds_zones_set_brightness_mc;
		zones_data->mcled_cdevs[i].led_cdev.brightness = ITE8291_KBD_ZONES_BRIGHTNESS_DEFAULT;
		zones_data->mcled_cdevs[i].num_colors = 3;
		zones_data->mcled_cdevs[i].subled_info = zones_data->mcled_cdevs_subleds[i];
		zones_data->mcled_cdevs[i].subled_info[0].color_index = LED_COLOR_ID_RED;
		zones_data->mcled_cdevs[i].subled_info[0].intensity = 255;
		zones_data->mcled_cdevs[i].subled_info[0].channel = i;
		zones_data->mcled_cdevs[i].subled_info[1].color_index = LED_COLOR_ID_GREEN;
		zones_data->mcled_cdevs[i].subled_info[1].intensity = 255;
		zones_data->mcled_cdevs[i].subled_info[1].channel = i;
		zones_data->mcled_cdevs[i].subled_info[2].color_index = LED_COLOR_ID_BLUE;
		zones_data->mcled_cdevs[i].subled_info[2].intensity = 255;
		zones_data->mcled_cdevs[i].subled_info[2].channel = i;

		result = devm_led_classdev_multicolor_register(&hdev->dev, &zones_data->mcled_cdevs[i]);
		if (result) {
			for (k = 0; k < i; ++k)
				devm_led_classdev_multicolor_unregister(&hdev->dev, &zones_data->mcled_cdevs[k]);
			return result;
		}
	}

	return 0;
}

static int ite8291_zones_remove(struct hid_device *hdev)
{
	int i;
	struct ite8291_driver_data_t *ite8291_driver_data = hid_get_drvdata(hdev);
	struct ite8291_driver_data_zones_t *zones_data = ite8291_driver_data->device_data;

	for (i = 0; i < ITE8291_NR_ZONES; ++i)
		devm_led_classdev_multicolor_unregister(&hdev->dev, &zones_data->mcled_cdevs[i]);

	return 0;
}

static int ite8291_zones_write_on(struct hid_device *hdev)
{
	ite8291_write_control(hdev, (u8[]){ 0x1a, 0x00, 0x01, 0x04, 0x00, 0x00, 0x00, 0x01 });
	return 0;
}

static int ite8291_zones_write_off(struct hid_device *hdev)
{
	ite8291_write_control(hdev, (u8[]){ 0x09, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
	ite8291_write_control(hdev, (u8[]){ 0x12, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00 });
	ite8291_write_control(hdev, (u8[]){ 0x08, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
	ite8291_write_control(hdev, (u8[]){ 0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
	ite8291_write_control(hdev, (u8[]){ 0x1a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 });
	return 0;
}

static int ite8291_zones_write_state(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *ite8291_driver_data = hid_get_drvdata(hdev);
	struct ite8291_driver_data_zones_t *zones_data = ite8291_driver_data->device_data;
	struct led_classdev_mc *mcled_cdev;
	int i;
	u8 red, green, blue;

	u8 brightness  = zones_data->mcled_cdevs[0].led_cdev.brightness;

	for (i = 0; i < ITE8291_NR_ZONES; ++i) {
		mcled_cdev = &zones_data->mcled_cdevs[i];
		red = mcled_cdev->subled_info[0].intensity;
		green = mcled_cdev->subled_info[1].intensity;
		blue = mcled_cdev->subled_info[2].intensity;
		color_scaling(hdev, &red, &green, &blue, false, 0, 0);
		ite8291_write_control(hdev, (u8[]){ 0x14, 0x00, i + 1, red, green, blue, 0x00, 0x00 });
	}

	ite8291_write_control(hdev, (u8[]){ 0x08, 0x02, 0x01, 0x03, brightness, 0x08, 0x00, 0x00 });

	return 0;
}

static int ite8291_driver_data_setup(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *driver_data;

	struct usb_device *usb_dev;
	struct usb_device_descriptor *usb_desc;

	driver_data = hid_get_drvdata(hdev);

	usb_dev = to_usb_device(hdev->dev.parent->parent);
	usb_desc = &(usb_dev->descriptor);

	driver_data->hid_dev = hdev;
	driver_data->bcd_device = le16_to_cpu(usb_desc->bcdDevice);

	// Initialize device specific data
	if (hdev->product == 0xce00 && driver_data->bcd_device == 0x0002) {
		driver_data->device_has_buffer_input_control = false;
		driver_data->device_add = ite8291_zones_add;
		driver_data->device_remove = ite8291_zones_remove;
		driver_data->device_write_on = ite8291_zones_write_on;
		driver_data->device_write_off = ite8291_zones_write_off;
		driver_data->device_write_state = ite8291_zones_write_state;
	} else {
		driver_data->device_has_buffer_input_control = true;
		driver_data->device_add = ite8291_perkey_add;
		driver_data->device_remove = ite8291_perkey_remove;
		driver_data->device_write_on = ite8291_perkey_write_on;
		driver_data->device_write_off = ite8291_perkey_write_off;
		driver_data->device_write_state = ite8291_perkey_write_state;
	}

	return 0;
}

static ssize_t buffer_input_show(struct device *device,
				 struct device_attribute *attr,
				 char *buf)
{
	struct hid_device *hdev;
	struct ite8291_driver_data_t *driver_data;
	hdev = container_of(device, struct hid_device, dev);
	driver_data = hid_get_drvdata(hdev);
	return sysfs_emit(buf, "%d\n", (bool) driver_data->device_buffer_input);

}

/* multi_intensity_store queues LED-core work for blocking callbacks. Drain
 * it before closing a transaction or starting a hardware effect. Never hold
 * io_lock while waiting: every LED callback needs the same mutex. */
static void drain_led_work(struct ite8291_driver_data_t *driver)
{
	struct ite8291_driver_data_perkey_t *data = driver->device_data;
	int row, col;
	for (row = 0; row < ITE8291_NR_ROWS; ++row)
		for (col = 0; col < ITE8291_LEDS_PER_ROW_MAX; ++col)
			flush_work(&data->mcled_cdevs[row][col].led_cdev.set_brightness_work);
}
static ssize_t buffer_input_store(struct device *device, struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct ite8291_driver_data_t *driver = hid_get_drvdata(to_hid_device(device));
	bool buffered;
	int ret = kstrtobool(buf, &buffered);
	if (ret) return ret;
	if (!buffered) drain_led_work(driver);
	cancel_delayed_work_sync(&driver->frame_work);
	mutex_lock(&driver->io_lock);
	driver->device_buffer_input = buffered;
	if (buffered) { driver->color_dirty = true; driver->mode = ITE8291_PARAM_MODE_USER; }
	if (!buffered) ret = flush_frame(driver);
	mutex_unlock(&driver->io_lock);
	return ret ? ret : size;
}
/* A semantic effect endpoint keeps the LED cache, UPower and hardware aligned.
 * Only known RAM-only lighting commands are accepted; no raw/flash interface. */
static ssize_t hardware_effect_show(struct device *device, struct device_attribute *attr, char *buf)
{
	struct ite8291_driver_data_t *d = hid_get_drvdata(to_hid_device(device));
	struct ite8291_driver_data_perkey_t *data = d->device_data;
	ssize_t ret;
	mutex_lock(&d->io_lock);
	ret = sysfs_emit(buf, "%u %u %u %u\n", d->powered ? d->mode : 0, d->speed, data->brightness, d->behaviour);
	mutex_unlock(&d->io_lock);
	return ret;
}
static ssize_t hardware_effect_store(struct device *device, struct device_attribute *attr, const char *buf, size_t size)
{
	struct hid_device *hdev = to_hid_device(device);
	struct ite8291_driver_data_t *d = hid_get_drvdata(hdev);
	struct ite8291_driver_data_perkey_t *data = d->device_data;
	unsigned int mode, speed, brightness, direction;
	char extra;
	u8 command[8];
	int ret;
	if (sscanf(buf, "%u %u %u %u %c", &mode, &speed, &brightness, &direction, &extra) != 4 ||
	    speed < 1 || speed > 10 || brightness > 50 ||
	    !((mode == 3 && direction >= 1 && direction <= 4) || (mode != 3 && direction == 0))) return -EINVAL;
	switch (mode) { case 0: case 2: case 3: case 4: case 5: case 6: case 9: case 10: case 14: case 17: break; default: return -EINVAL; }
	drain_led_work(d);
	cancel_delayed_work_sync(&d->frame_work);
	mutex_lock(&d->io_lock);
	if (d->device_buffer_input || d->stopping) { ret = -EBUSY; goto out; }
	memset(command, 0, sizeof(command));
	command[0] = 8; command[1] = mode ? 2 : 1;
	if (mode) { command[2] = mode; command[3] = speed; command[4] = effective_brightness(d, brightness); command[5] = 8; command[6] = direction; }
	ret = ite8291_write_control(hdev, command);
	if (!ret) {
		d->mode = mode ? mode : d->mode; d->speed = speed; d->behaviour = direction;
		d->powered = !!mode; d->color_dirty = false;
		cache_brightness(d, mode ? brightness : 0);
		kobject_uevent(&hdev->dev.kobj, KOBJ_CHANGE);
		led_classdev_notify_brightness_hw_changed(&data->mcled_cdevs[0][0].led_cdev, mode ? brightness : 0);
	}
	d->last_error = ret;
out:
	mutex_unlock(&d->io_lock);
	return ret ? ret : size;
}
static DEVICE_ATTR_RW(hardware_effect);
static ssize_t brightness_scale_show(struct device *device, struct device_attribute *attr, char *buf)
{
	struct ite8291_driver_data_t *d = hid_get_drvdata(to_hid_device(device));
	return sysfs_emit(buf, "%u\n", READ_ONCE(d->brightness_scale));
}
static ssize_t brightness_scale_store(struct device *device, struct device_attribute *attr, const char *buf, size_t size)
{
	struct hid_device *hdev = to_hid_device(device);
	struct ite8291_driver_data_t *d = hid_get_drvdata(hdev);
	struct ite8291_driver_data_perkey_t *data = d->device_data;
	unsigned int scale;
	u8 previous, next;
	int ret = 0;
	if (kstrtouint(buf, 10, &scale) || scale < 1 || scale > 1000) return -EINVAL;
	mutex_lock(&d->io_lock);
	if (d->stopping) { ret = -ENODEV; goto out; }
	previous = effective_brightness(d, data->brightness);
	next = slayer_effective_brightness(data->brightness, scale);
	if (previous != next && d->powered) {
		ret = ite8291_write_brightness(hdev, next);
		if (!ret) d->brightness_count++;
	}
	if (!ret) d->brightness_scale = scale;
	d->last_error = ret;
out:
	mutex_unlock(&d->io_lock);
	return ret ? ret : size;
}
static DEVICE_ATTR_RW(brightness_scale);
static ssize_t brightness_minimum_show(struct device *device, struct device_attribute *attr, char *buf)
{ return sysfs_emit(buf, "2\n"); }
static DEVICE_ATTR_RO(brightness_minimum);
static ssize_t transfer_stats_show(struct device *device, struct device_attribute *attr, char *buf)
{
	struct ite8291_driver_data_t *d = hid_get_drvdata(to_hid_device(device));
	ssize_t ret;
	mutex_lock(&d->io_lock);
	ret = sysfs_emit(buf, "frames=%llu brightness=%llu error=%d\n", d->frame_count, d->brightness_count, d->last_error);
	mutex_unlock(&d->io_lock);
	return ret;
}
static DEVICE_ATTR_RO(transfer_stats);

static ssize_t white_point_show(struct device *device, struct device_attribute *attr, char *buf)
{
	struct ite8291_driver_data_t *d = hid_get_drvdata(to_hid_device(device));
	ssize_t ret;
	mutex_lock(&d->io_lock);
	ret = sysfs_emit(buf, "%u %u %u\n", d->white_point[0], d->white_point[1], d->white_point[2]);
	mutex_unlock(&d->io_lock);
	return ret;
}
static ssize_t white_point_store(struct device *device, struct device_attribute *attr, const char *buf, size_t size)
{
	struct hid_device *hdev = to_hid_device(device);
	struct ite8291_driver_data_t *d = hid_get_drvdata(hdev);
	struct ite8291_driver_data_perkey_t *data = d->device_data;
	unsigned int red, green, blue;
	char extra;
	int row, col, ret = 0;
	if (sscanf(buf, "%u %u %u %c", &red, &green, &blue, &extra) != 3 ||
	    red < 1 || red > 255 || green < 1 || green > 255 || blue < 1 || blue > 255) return -EINVAL;
	drain_led_work(d);
	cancel_delayed_work_sync(&d->frame_work);
	mutex_lock(&d->io_lock);
	if (d->stopping) { ret = -ENODEV; goto out; }
	d->color_profile_active = false;
	d->white_point[0] = red; d->white_point[1] = green; d->white_point[2] = blue;
	for (row = 0; row < ITE8291_NR_ROWS; ++row)
		for (col = 0; col < ITE8291_LEDS_PER_ROW_MAX; ++col)
			row_data_set(hdev, data->row_data, row, col, data->colors[row][col][0], data->colors[row][col][1], data->colors[row][col][2]);
	/* Built-in firmware animations own their palette. Keep them running;
	 * the correction applies when a static/KDE frame is next selected. */
	if (d->mode == ITE8291_PARAM_MODE_USER) {
		d->color_dirty = true;
		if (!d->device_buffer_input) ret = flush_frame(d);
	}
	kobject_uevent(&hdev->dev.kobj, KOBJ_CHANGE);
out:
	mutex_unlock(&d->io_lock);
	return ret ? ret : size;
}
static DEVICE_ATTR_RW(white_point);

static ssize_t color_profile_show(struct device *device, struct device_attribute *attr, char *buf)
{
	struct ite8291_driver_data_t *d=hid_get_drvdata(to_hid_device(device));
	ssize_t count;
	mutex_lock(&d->io_lock);
	if(!d->color_profile_active)count=sysfs_emit(buf,"off\n");
	else {bin2hex(buf,d->color_profile,d->color_profile_size);count=d->color_profile_size*2;buf[count++]='\n';}
	mutex_unlock(&d->io_lock);
	return count;
}
static ssize_t color_profile_store(struct device *device, struct device_attribute *attr, const char *buf, size_t size)
{
	struct hid_device *hdev=to_hid_device(device);
	struct ite8291_driver_data_t *d=hid_get_drvdata(hdev);
	struct ite8291_driver_data_perkey_t *data=d->device_data;
	u8 model[SLAYER_COLOR_MODEL_MAX];
	size_t length=size;
	bool off=sysfs_streq(buf,"off"),reset=sysfs_streq(buf,"reset");
	int ret=0,row,col;
	if(!off&&!reset){
		if(length && buf[length-1]=='\n')length--;
		if(length%2 || length/2>sizeof(model) || hex2bin(model,buf,length/2) || !slayer_color_model_valid(model,length/2))return -EINVAL;
	}
	drain_led_work(d);cancel_delayed_work_sync(&d->frame_work);
	mutex_lock(&d->io_lock);
	if(d->stopping){ret=-ENODEV;goto out;}
	d->color_profile_active=!off&&!reset;
	if(d->color_profile_active){
		int i;
		d->color_profile_size=length/2;memcpy(d->color_profile,model,d->color_profile_size);
		for(i=0;i<model[5];++i){const u8 *v=slayer_color_vertex(model,i);if(v[0]==255&&v[1]==255&&v[2]==255)memcpy(d->white_point,v+3,3);}
	}
	if(reset)memset(d->white_point,255,3);
	for(row=0;row<ITE8291_NR_ROWS;++row)for(col=0;col<ITE8291_LEDS_PER_ROW_MAX;++col)
		row_data_set(hdev,data->row_data,row,col,data->colors[row][col][0],data->colors[row][col][1],data->colors[row][col][2]);
	if(d->mode==ITE8291_PARAM_MODE_USER){d->color_dirty=true;if(!d->device_buffer_input)ret=flush_frame(d);}
	kobject_uevent(&hdev->dev.kobj,KOBJ_CHANGE);
out:
	mutex_unlock(&d->io_lock);
	return ret?ret:size;
}
static DEVICE_ATTR_RW(color_profile);

static ssize_t frame_color_show(struct device *device, struct device_attribute *attr, char *buf)
{
	struct ite8291_driver_data_t *d = hid_get_drvdata(to_hid_device(device));
	struct ite8291_driver_data_perkey_t *data = d->device_data;
	ssize_t ret;
	mutex_lock(&d->io_lock);
	ret = sysfs_emit(buf, "%u %u %u\n", data->row_data[0][44], data->row_data[0][23], data->row_data[0][2]);
	mutex_unlock(&d->io_lock);
	return ret;
}
static DEVICE_ATTR_RO(frame_color);

DEVICE_ATTR_RW(buffer_input);

static struct attribute *control_group_attrs[] = {
	&dev_attr_buffer_input.attr,
	&dev_attr_hardware_effect.attr,
	&dev_attr_brightness_scale.attr,
	&dev_attr_brightness_minimum.attr,
	&dev_attr_transfer_stats.attr,
	&dev_attr_frame_color.attr,
	&dev_attr_white_point.attr,
	&dev_attr_color_profile.attr,
	NULL
};

static struct attribute_group control_group = {
	.name = "controls",
	.attrs = control_group_attrs
};

static int driver_probe_callb(struct hid_device *hdev, const struct hid_device_id *id)
{
	int result;
	struct ite8291_driver_data_t *ite8291_driver_data;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
	// Unused device on Stellaris Intel Gen5 (membrane), avoid binding to it
	if (dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XI05") && hdev->product == 0x5000)
		return -ENODEV;
#endif

	result = hid_parse(hdev);
	if (result) {
		pr_err("hid_parse failed\n");
		stop_hw(hdev);
		return result;
	}

	result = start_hw(hdev);
	if (result != 0)
		return result;

	ite8291_driver_data = devm_kzalloc(&hdev->dev, sizeof(*ite8291_driver_data), GFP_KERNEL);
	if (!ite8291_driver_data)
		return -ENOMEM;

	hid_set_drvdata(hdev, ite8291_driver_data);
	mutex_init(&ite8291_driver_data->io_lock);
	INIT_DELAYED_WORK(&ite8291_driver_data->frame_work, write_frame_work);
	ite8291_driver_data->mode = ITE8291_PARAM_MODE_USER;
	ite8291_driver_data->speed = 5;
	ite8291_driver_data->brightness_scale = 1000;
	memset(ite8291_driver_data->white_point, 255, 3);

	result = ite8291_driver_data_setup(hdev);
	if (result != 0)
		return result;

	result = ite8291_driver_data->device_add(hdev);
	if (result != 0)
		return result;

	ite8291_driver_data->device_write_on(hdev);
	ite8291_driver_data->device_write_state(hdev);

	if (ite8291_driver_data->device_has_buffer_input_control) {
		result = sysfs_create_group(&hdev->dev.kobj, &control_group);
		if (result != 0) {
			mutex_lock(&ite8291_driver_data->io_lock);
			ite8291_driver_data->stopping = true;
			mutex_unlock(&ite8291_driver_data->io_lock);
			cancel_delayed_work_sync(&ite8291_driver_data->frame_work);
			ite8291_driver_data->device_remove(hdev);
			stop_hw(hdev);
			return result;
		}
	}

	return 0;
}

static void driver_remove_callb(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *driver_data;
	pr_debug("driver remove\n");
	driver_data = hid_get_drvdata(hdev);
	if (driver_data->device_has_buffer_input_control)
		sysfs_remove_group(&hdev->dev.kobj, &control_group);
	mutex_lock(&driver_data->io_lock);
	driver_data->stopping = true;
	mutex_unlock(&driver_data->io_lock);
	cancel_delayed_work_sync(&driver_data->frame_work);
	driver_data->device_write_off(hdev);
	driver_data->device_remove(hdev);
	stop_hw(hdev);
}

#ifdef CONFIG_PM
static int driver_suspend_callb(struct hid_device *hdev, pm_message_t message)
{
	struct ite8291_driver_data_t *driver_data;
	pr_debug("driver suspend\n");
	driver_data = hid_get_drvdata(hdev);
	cancel_delayed_work_sync(&driver_data->frame_work);
	mutex_lock(&driver_data->io_lock);
	driver_data->device_write_off(hdev);
	mutex_unlock(&driver_data->io_lock);
	return 0;
}

static int driver_resume_callb(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *driver_data;
	pr_debug("driver resume\n");
	driver_data = hid_get_drvdata(hdev);
	mutex_lock(&driver_data->io_lock);
	driver_data->device_write_on(hdev);
	int ret = driver_data->device_write_state(hdev);
	mutex_unlock(&driver_data->io_lock);
	return ret;
}

static int driver_reset_resume_callb(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *driver_data;
	pr_debug("driver reset resume\n");
	driver_data = hid_get_drvdata(hdev);
	mutex_lock(&driver_data->io_lock);
	driver_data->device_write_on(hdev);
	int ret = driver_data->device_write_state(hdev);
	mutex_unlock(&driver_data->io_lock);
	return ret;
}
#endif

static const struct hid_device_id ite8291_device_table[] = {
	{ HID_USB_DEVICE(0x048d, 0xce00) },
	{ HID_USB_DEVICE(0x048d, 0x6004) },
	{ HID_USB_DEVICE(0x048d, 0x600a) },
	{ HID_USB_DEVICE(0x048d, 0x600b) },
	{ }
};
MODULE_DEVICE_TABLE(hid, ite8291_device_table);

static struct hid_driver ite8291_driver = {
	.name = KBUILD_MODNAME,
	.probe = driver_probe_callb,
	.remove = driver_remove_callb,
	.id_table = ite8291_device_table,
#ifdef CONFIG_PM
	.suspend = driver_suspend_callb,
	.resume = driver_resume_callb,
	.reset_resume = driver_reset_resume_callb
#endif
};
module_hid_driver(ite8291_driver);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("Driver for ITE Device(8291) RGB LED keyboard backlight.");
MODULE_LICENSE("GPL");

MODULE_VERSION("0.0.20261114.2");
