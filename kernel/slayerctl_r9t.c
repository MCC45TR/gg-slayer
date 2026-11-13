// SPDX-License-Identifier: GPL-2.0-only
/* Model-specific R9T/IDK bridge. No general EC, TDP or arbitrary UEFI access.
 * Charge-type mapping/mask follows Linux uniwill-acpi and TUXEDO's documented
 * 0x07a6[5:4] contract. Live IDK project26 and support6 bit3 were read on target.
 */
#include <linux/acpi.h>
#include <linux/wmi.h>
#include <linux/workqueue.h>
#include <linux/bitops.h>
#include <linux/dmi.h>
#include <linux/hwmon.h>
#include <linux/efi.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <acpi/battery.h>
#include "profile_bits.h"
#include "thermal_profile.h"
#include "model_contract.h"
#include "mux_transaction.h"
#define R9T_DRIVER_VERSION "0.1.1"

static acpi_handle ec;
static struct platform_device *pdev;
static struct device *hwmon;
static bool usb_charging_supported;
static bool event_driver_registered;
static atomic_t event_devices = ATOMIC_INIT(0);
static u8 last_thermal;
/* These GPL-exported EC query APIs are declared in drivers/acpi/internal.h.
 * We only augment this exact BIOS's _Q88 and execute its original AML once. */
struct acpi_ec;
extern int acpi_ec_add_query_handler(struct acpi_ec *, u8, acpi_handle, int (*)(void *), void *);
extern void acpi_ec_remove_query_handler(struct acpi_ec *, u8);
static struct acpi_ec *profile_ec;
static struct device *profile_ec_device;
static atomic64_t profile_event_count = ATOMIC64_INIT(0);
static DEFINE_MUTEX(lock);
static int read_byte(u16 address, u8 *value)
{
	union acpi_object input = { .type = ACPI_TYPE_INTEGER };
	struct acpi_object_list args = { .count = 1, .pointer = &input };
	unsigned long long result;
	input.integer.value = address;
	if (ACPI_FAILURE(acpi_evaluate_integer(ec, "ECRR", &args, &result)) || result > 255)
		return -EIO;
	*value = result;
	return 0;
}
/* Caller holds lock. Update only the named bits, verify and mask rollback. */
static int write_byte(u16 address, u8 value)
{
 union acpi_object in[2] = {{ .type = ACPI_TYPE_INTEGER }, { .type = ACPI_TYPE_INTEGER }};
 struct acpi_object_list args = { .count = 2, .pointer = in };
 in[0].integer.value = address; in[1].integer.value = value;
 return ACPI_FAILURE(acpi_evaluate_object(ec, "ECRW", &args, NULL)) ? -EIO : 0;
}
static int update_bits(u16 address, u8 mask, u8 value)
{
 u8 old, live;
 int ret = read_byte(address, &old);
 if (ret) return ret;
 if ((old & mask) == value) return 0;
 ret = write_byte(address, slayer_mask_bits(old, mask, value));
 if (!ret) ret = read_byte(address, &live);
 if (!ret && (live & mask) == value) return 0;
 if (read_byte(address, &live) || write_byte(address, slayer_mask_bits(live, mask, old)) ||
     read_byte(address, &live) || (live & mask) != (old & mask))
  pr_err_ratelimited("slayerctl_r9t: masked EC rollback failed\n");
 return ret ? ret : -EIO;
}
#define BOOL_ATTRIBUTE(name, address, mask) \
static ssize_t name##_show(struct device *dev, struct device_attribute *attr, char *buf) \
{ u8 v; int ret; mutex_lock(&lock); ret = read_byte(address, &v); mutex_unlock(&lock); \
 return ret ? ret : sysfs_emit(buf, "%d\n", !!(v & (mask))); } \
static ssize_t name##_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) \
{ bool v; int ret = kstrtobool(buf, &v); if (ret) return ret; \
 mutex_lock(&lock); ret = update_bits(address, mask, v ? mask : 0); mutex_unlock(&lock); \
 return ret ? ret : count; } \
static DEVICE_ATTR_RW(name)
BOOL_ATTRIBUTE(fn_lock, 0x074e, BIT(4));
BOOL_ATTRIBUTE(usb_sleep_charging, 0x0767, BIT(4));
static ssize_t usb_sleep_charging_supported_show(struct device *dev, struct device_attribute *attr, char *buf)
{ return sysfs_emit(buf, "%u\n", usb_charging_supported); }
static DEVICE_ATTR_RO(usb_sleep_charging_supported);

/* Firmware's native 180-byte object; NEVER sizeof(managed NVRAM_STRUCT).
 * N.1.46GAR16 OemDisplayModeDxe consumes payload[SLAYER_MUX_OFFSET] and updates Setup/PBS
 * on next boot. No Setup variable, SMI, PCI register or live display writes. */
static efi_guid_t oem_guid = EFI_GUID(0x9f33f85c, 0x13ca, 0x4fd1, 0x9c, 0x4a, 0x96, 0x21, 0x77, 0x22, 0xc5, 0x93);
/* The efivar API requires its global lock even for reads. */
static int mux_read_locked(void *context, u8 *data)
{
 unsigned long size = SLAYER_OEM_SIZE, door_size = 1;
 u32 attrs = 0, door_attrs = 0;
 u8 door = 0;
 efi_status_t st;
 if (!efi_enabled(EFI_RUNTIME_SERVICES)) return -EOPNOTSUPP;
 st = efivar_get_variable(L"OemMagicVariable", &oem_guid, &attrs, &size, data);
 if (st != EFI_SUCCESS) return -EIO;
 if (!slayer_oem_valid(data, size, attrs)) return -EPROTO;
 st = efivar_get_variable(L"OemMagicDoor", &oem_guid, &door_attrs, &door_size, &door);
 if (st != EFI_SUCCESS || door_size != 1 || door_attrs != 7 || door != 1) return -EPROTO;
 return 0;
}
static int mux_read(u8 *data)
{
 int ret;
 if (!efi_enabled(EFI_RUNTIME_SERVICES) || !efivar_is_available())
  return -EOPNOTSUPP;
 ret = efivar_lock();
 if (ret) return ret;
 ret = mux_read_locked(NULL, data);
 efivar_unlock();
 return ret;
}
static int mux_write_locked(void *context, u8 *data)
{
 if (!efivar_supports_writes()) return -EOPNOTSUPP;
 return efivar_set_variable_locked(L"OemMagicVariable", &oem_guid, 7,
                                  SLAYER_OEM_SIZE, data, false) == EFI_SUCCESS ? 0 : -EIO;
}
static const struct slayer_mux_ops mux_ops = {
 .read = mux_read_locked, .write = mux_write_locked,
};
static const char * const mux_names[] = { "hybrid", "dedicated", "integrated" };
static ssize_t mux_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
 u8 data[SLAYER_OEM_SIZE]; int ret;
 mutex_lock(&lock); ret = mux_read(data); mutex_unlock(&lock);
 return ret ? ret : sysfs_emit(buf, "%s\n", mux_names[data[SLAYER_MUX_OFFSET]]);
}
static ssize_t mux_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
 unsigned int mode;
 int ret;
 for (mode = 0; mode < ARRAY_SIZE(mux_names); ++mode)
  if (sysfs_streq(buf, mux_names[mode])) break;
 if (mode == ARRAY_SIZE(mux_names)) return -EINVAL;
 mutex_lock(&lock);
 ret = efivar_lock();
 if (!ret) {
  ret = slayer_mux_update(mode, &mux_ops, NULL);
  efivar_unlock();
 }
 mutex_unlock(&lock);
 if (ret == -EUCLEAN)
  pr_err_ratelimited("slayerctl_r9t: MUX rollback could not be verified\n");
 return ret ? ret : count;
}
static DEVICE_ATTR_RW(mux_mode);
static ssize_t mux_modes_available_show(struct device *dev, struct device_attribute *attr, char *buf)
{ return sysfs_emit(buf, "hybrid dedicated integrated\n"); }
static DEVICE_ATTR_RO(mux_modes_available);

/* Keep OEM firmware state independent of desktop CPU power policy. Registering
 * platform_profile here lets tuned/PPD silently change the EC even with Slayer
 * synchronization disabled. The broker explicitly bridges these two policies. */
static const char * const thermal_names[] = { "low-power", "balanced", "performance" };
static ssize_t thermal_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
 u8 value; int ret, mode;
 mutex_lock(&lock); ret = read_byte(0x0751, &value); mutex_unlock(&lock);
 if (ret) return ret;
 mode = slayer_thermal_decode(value);
 return sysfs_emit(buf, "%s\n", mode < 0 ? "custom" : thermal_names[mode]);
}
static ssize_t thermal_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
 unsigned int mode; u8 bits; int ret;
 for (mode = 0; mode < ARRAY_SIZE(thermal_names); ++mode)
  if (sysfs_streq(buf, thermal_names[mode])) break;
 if (mode >= ARRAY_SIZE(thermal_names)) return -EINVAL;
 mutex_lock(&lock);
 ret = slayer_thermal_encode(0, mode, &bits);
 if (!ret) ret = update_bits(0x0751, SLAYER_THERMAL_MASK, bits);
 mutex_unlock(&lock);
 if (ret) return ret;
 sysfs_notify(&pdev->dev.kobj, NULL, "thermal_mode");
 kobject_uevent(&pdev->dev.kobj, KOBJ_CHANGE);
 return count;
}
static DEVICE_ATTR_RW(thermal_mode);
static ssize_t thermal_mode_choices_show(struct device *dev, struct device_attribute *attr, char *buf)
{ return sysfs_emit(buf, "low-power balanced performance\n"); }
static DEVICE_ATTR_RO(thermal_mode_choices);
/* Firmware hotkeys are notification hints, never a substitute for EC state.
 * The Uniwill event GUID is documented by the upstream TUXEDO interface.
 * No WMI methods, EC writes, input interception or periodic polling here. */
static void thermal_event_work(struct work_struct *work)
{
 u8 live; int ret;
 mutex_lock(&lock); ret = read_byte(0x0751, &live);
 mutex_unlock(&lock);
 if (ret || (live & 0xf0) == last_thermal) return;
 last_thermal = live & 0xf0;
 sysfs_notify(&pdev->dev.kobj, NULL, "thermal_mode");
 sysfs_notify(&pdev->dev.kobj, NULL, "fan_mode");
 kobject_uevent(&pdev->dev.kobj, KOBJ_CHANGE);
}
static DECLARE_DELAYED_WORK(thermal_event, thermal_event_work);
static int query_profile_event(void *data)
{
 acpi_status status = acpi_evaluate_object((acpi_handle)data, NULL, NULL, NULL);
 atomic64_inc(&profile_event_count);
 mod_delayed_work(system_wq, &thermal_event, msecs_to_jiffies(150));
 return ACPI_FAILURE(status) ? -EIO : 0;
}
static int match_ec_device(struct device *device, const void *data)
{
 return ACPI_COMPANION(device) == data && device->driver &&
        !strcmp(device->driver->name, "acpi-ec");
}
static void attach_query_observer(void)
{
 acpi_handle handle, query; struct acpi_device *device;
 if (ACPI_FAILURE(acpi_get_handle(NULL, "\\_SB.PCI0.SBRG.EC0", &handle)) ||
     ACPI_FAILURE(acpi_get_handle(handle, "_Q88", &query))) return;
 device = acpi_fetch_acpi_dev(handle);
 if (!device) return;
 /* Current Linux binds the EC as a platform device, so the ACPI object's
  * driver_data is empty. Retain the physical device until detach. */
 profile_ec_device = bus_find_device(&platform_bus_type, NULL, device, match_ec_device);
 if (profile_ec_device) profile_ec = dev_get_drvdata(profile_ec_device);
 else profile_ec = acpi_driver_data(device);
 if (!profile_ec || acpi_ec_add_query_handler(profile_ec, 0x88, NULL, query_profile_event, query)) {
  profile_ec = NULL;
  put_device(profile_ec_device);
  profile_ec_device = NULL;
  pr_warn("slayerctl_r9t: firmware profile query observer unavailable\n");
 }
}
static int event_probe(struct wmi_device *device, const void *context)
{ atomic_inc(&event_devices); return 0; }
static void event_remove(struct wmi_device *device)
{ atomic_dec(&event_devices); }
static void event_notify(struct wmi_device *device, const struct wmi_buffer *data)
{ atomic64_inc(&profile_event_count); mod_delayed_work(system_wq, &thermal_event, msecs_to_jiffies(150)); }
static const struct wmi_device_id event_ids[] = {
 { .guid_string = "ABBC0F70-8EA1-11D1-00A0-C90629100000" },
 { .guid_string = "ABBC0F71-8EA1-11D1-00A0-C90629100000" },
 { .guid_string = "ABBC0F72-8EA1-11D1-00A0-C90629100000" }, { }
};
static struct wmi_driver event_driver = {
 .driver = { .name = "slayer-r9t-events" },
 .id_table = event_ids, .probe = event_probe, .remove = event_remove,
 .notify_new = event_notify,
};
static ssize_t profile_events_show(struct device *dev, struct device_attribute *attr, char *buf)
{ return sysfs_emit(buf, "%s\n", profile_ec ? (atomic_read(&event_devices) ? "acpi-query+wmi" : "acpi-query") : (atomic_read(&event_devices) ? "wmi" : "unavailable")); }
static DEVICE_ATTR_RO(profile_events);
static ssize_t profile_event_count_show(struct device *dev, struct device_attribute *attr, char *buf)
{ return sysfs_emit(buf, "%lld\n", atomic64_read(&profile_event_count)); }
static DEVICE_ATTR_RO(profile_event_count);
static ssize_t thermal_control_show(struct device *dev, struct device_attribute *attr, char *buf)
{ u8 value; int ret; mutex_lock(&lock); ret = read_byte(0x0751, &value); mutex_unlock(&lock);
 return ret ? ret : sysfs_emit(buf, "0x%02x\n", value); }
static DEVICE_ATTR_RO(thermal_control);
static ssize_t fan_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{ u8 v; int ret; mutex_lock(&lock); ret = read_byte(0x0751, &v); mutex_unlock(&lock);
 return ret ? ret : sysfs_emit(buf, "%s\n", v & BIT(6) ? "boost" : (slayer_thermal_decode(v) < 0 ? "firmware-custom" : "auto")); }
static ssize_t fan_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
 u8 support; int ret; bool boost = sysfs_streq(buf, "boost");
 if (!boost && !sysfs_streq(buf, "auto")) return -EINVAL;
 mutex_lock(&lock); ret = read_byte(0x0765, &support);
 if (!ret) ret = support & BIT(7) ? update_bits(0x0751, BIT(6), boost ? BIT(6) : 0) : -EOPNOTSUPP;
 mutex_unlock(&lock); return ret ? ret : count;
}
static DEVICE_ATTR_RW(fan_mode);
static ssize_t fan_modes_available_show(struct device *dev, struct device_attribute *attr, char *buf)
{ return sysfs_emit(buf, "auto boost\n"); }
static DEVICE_ATTR_RO(fan_modes_available);
static int profile_read(unsigned int *value)
{
	u8 byte;
	int ret = read_byte(0x07a6, &byte);
	if (ret)
		return ret;
	*value = (byte >> 4) & 3;
	return *value > 2 ? -EPROTO : 0;
}
static int profile_write(unsigned int value)
{
	union acpi_object input[2] = { { .type = ACPI_TYPE_INTEGER }, { .type = ACPI_TYPE_INTEGER } };
	struct acpi_object_list args = { .count = 2, .pointer = input };
	u8 old, support, next;
	unsigned int observed;
	int ret;
	if (value > 2)
		return -EINVAL;
	ret = read_byte(0x078e, &support);
	if (ret || !(support & BIT(3)))
		return ret ? ret : -EOPNOTSUPP;
	ret = read_byte(0x07a6, &old);
	if (ret || ((old >> 4) & 3) > 2)
		return ret ? ret : -EPROTO;
	/* Avoid unnecessary writes, including the default profile on module load. */
	if (((old >> 4) & 3) == value)
		return 0;
	input[0].integer.value = 0x07a6;
	if (slayer_profile_bits(old, value, &next))
		return -EINVAL;
	input[1].integer.value = next;
	ret = ACPI_FAILURE(acpi_evaluate_object(ec, "ECRW", &args, NULL)) ? -EIO : 0;
	if (!ret)
		ret = profile_read(&observed);
	if (!ret && observed == value)
		return 0;
	/* Only restore these two bits; never overwrite other live firmware flags. */
	{
		u8 live_byte;
		if (!read_byte(0x07a6, &live_byte)) {
			input[1].integer.value = (live_byte & ~0x30) | (old & 0x30);
			if (ACPI_FAILURE(acpi_evaluate_object(ec, "ECRW", &args, NULL)) ||
			    profile_read(&observed) || observed != ((old >> 4) & 3))
				pr_err_ratelimited("slayerctl_r9t: charge profile rollback failed\n");
		} else {
			pr_err_ratelimited("slayerctl_r9t: charge profile unreadable during rollback\n");
		}
	}
	return ret ? ret : -EIO;
}
static const char * const names[] = { "high-capacity", "balanced", "stationary" };
static ssize_t charging_profile_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int value;
	int ret;
	mutex_lock(&lock);
	ret = profile_read(&value);
	mutex_unlock(&lock);
	return ret ? ret : sysfs_emit(buf, "%s\n", names[value]);
}
static ssize_t charging_profile_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value;
	int ret;
	for (value = 0; value < ARRAY_SIZE(names); ++value)
		if (sysfs_streq(buf, names[value]))
			break;
	if (value == ARRAY_SIZE(names))
		return -EINVAL;
	mutex_lock(&lock);
	ret = profile_write(value);
	mutex_unlock(&lock);
	return ret ? ret : count;
}
static DEVICE_ATTR_RW(charging_profile);
static ssize_t charging_profiles_available_show(struct device *dev, struct device_attribute *attr, char *buf)
{ return sysfs_emit(buf, "high-capacity balanced stationary\n"); }
static DEVICE_ATTR_RO(charging_profiles_available);
static ssize_t backend_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{ return sysfs_emit(buf, R9T_DRIVER_VERSION " R9T-IDK26-N.1.46GAR16\n"); }
static DEVICE_ATTR_RO(backend_version);
static struct attribute *attrs[] = { &dev_attr_profile_event_count.attr, &dev_attr_thermal_mode.attr, &dev_attr_thermal_mode_choices.attr, &dev_attr_profile_events.attr, &dev_attr_thermal_control.attr, &dev_attr_backend_version.attr, &dev_attr_charging_profile.attr,
	&dev_attr_charging_profiles_available.attr, &dev_attr_fn_lock.attr,
 &dev_attr_fan_mode.attr, &dev_attr_fan_modes_available.attr,
 &dev_attr_usb_sleep_charging_supported.attr, NULL };
static const struct attribute_group group = { .attrs = attrs };
static struct attribute *mux_attrs[] = { &dev_attr_mux_mode.attr, &dev_attr_mux_modes_available.attr, NULL };
static const struct attribute_group mux_group = { .attrs = mux_attrs };
static bool mux_available;


static int charge_get(struct power_supply *psy, const struct power_supply_ext *ext, void *data,
		     enum power_supply_property prop, union power_supply_propval *val)
{
	unsigned int profile;
	int ret;
	if (prop != POWER_SUPPLY_PROP_CHARGE_TYPES)
		return -EINVAL;
	mutex_lock(&lock);
	ret = profile_read(&profile);
	mutex_unlock(&lock);
	if (ret)
		return ret;
	/* Standard=high-capacity, Long Life=balanced, Trickle=stationary. */
	val->intval = profile == 0 ? POWER_SUPPLY_CHARGE_TYPE_STANDARD :
		profile == 1 ? POWER_SUPPLY_CHARGE_TYPE_LONGLIFE : POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
	return 0;
}
static int charge_set(struct power_supply *psy, const struct power_supply_ext *ext, void *data,
		     enum power_supply_property prop, const union power_supply_propval *val)
{
	unsigned int profile;
	int ret;
	if (prop != POWER_SUPPLY_PROP_CHARGE_TYPES)
		return -EINVAL;
	switch (val->intval) {
	case POWER_SUPPLY_CHARGE_TYPE_STANDARD: profile = 0; break;
	case POWER_SUPPLY_CHARGE_TYPE_LONGLIFE: profile = 1; break;
	case POWER_SUPPLY_CHARGE_TYPE_TRICKLE: profile = 2; break;
	default: return -EINVAL;
	}
	mutex_lock(&lock);
	ret = profile_write(profile);
	mutex_unlock(&lock);
	if (!ret)
		power_supply_changed(psy);
	return ret;
}
static int charge_writeable(struct power_supply *psy, const struct power_supply_ext *ext,
			    void *data, enum power_supply_property prop)
{ return prop == POWER_SUPPLY_PROP_CHARGE_TYPES; }
static const enum power_supply_property charge_props[] = { POWER_SUPPLY_PROP_CHARGE_TYPES };
static const struct power_supply_ext charge_ext = {
	.name = "slayerctl-r9t",
	.charge_types = BIT(POWER_SUPPLY_CHARGE_TYPE_STANDARD) | BIT(POWER_SUPPLY_CHARGE_TYPE_LONGLIFE) | BIT(POWER_SUPPLY_CHARGE_TYPE_TRICKLE),
	.properties = charge_props, .num_properties = ARRAY_SIZE(charge_props),
	.get_property = charge_get, .set_property = charge_set, .property_is_writeable = charge_writeable,
};
static int add_battery(struct power_supply *psy, struct acpi_battery_hook *hook)
{
	/* Never replace another driver's charge-profile/threshold interface. */
	union power_supply_propval value;
	if (power_supply_get_property(psy, POWER_SUPPLY_PROP_CHARGE_TYPES, &value) != -EINVAL ||
	    power_supply_get_property(psy, POWER_SUPPLY_PROP_CHARGE_CONTROL_END_THRESHOLD, &value) != -EINVAL)
		return -EEXIST;
	return power_supply_register_extension(psy, &charge_ext, &pdev->dev, NULL);
}
static int remove_battery(struct power_supply *psy, struct acpi_battery_hook *hook)
{ power_supply_unregister_extension(psy, &charge_ext); return 0; }
static struct acpi_battery_hook hook = { .name = "Slayer R9T charge profiles", .add_battery = add_battery, .remove_battery = remove_battery };

static umode_t sensor_visible(const void *data, enum hwmon_sensor_types type, u32 attr, int channel)
{ return (type == hwmon_temp && (attr == hwmon_temp_input || attr == hwmon_temp_label)) ||
 (type == hwmon_fan && (attr == hwmon_fan_input || attr == hwmon_fan_label)) ? 0444 : 0; }
static int sensor_read(struct device *dev, enum hwmon_sensor_types type, u32 attr, int channel, long *value)
{
	u8 temp;
 int ret;
 if (type == hwmon_fan && attr == hwmon_fan_input && channel >= 0 && channel < 2) {
  u16 addr = channel ? 0x046c : 0x0464;
  u8 hi, lo, hi2; int attempt;
  mutex_lock(&lock);
  for (attempt = 0; attempt < 3; ++attempt) {
   ret = read_byte(addr, &hi);
   if (!ret) ret = read_byte(addr + 1, &lo);
   if (!ret) ret = read_byte(addr, &hi2);
   if (ret || hi == hi2) break;
  }
  mutex_unlock(&lock);
  if (ret) return ret;
  if (attempt == 3) return -EAGAIN;
  *value = (hi << 8) | lo;
  return *value <= 15000 ? 0 : -ENODATA;
 }
 if (type != hwmon_temp || attr != hwmon_temp_input || channel < 0 || channel > 1)
		return -EOPNOTSUPP;
	mutex_lock(&lock);
	ret = read_byte(channel ? 0x044f : 0x043e, &temp);
	mutex_unlock(&lock);
	if (ret || !temp || temp > 125)
		return ret ? ret : -ENODATA;
	*value = temp * 1000;
	return 0;
}
static int sensor_label(struct device *dev, enum hwmon_sensor_types type, u32 attr, int channel, const char **str)
{
	if ((type != hwmon_temp && type != hwmon_fan) || channel < 0 || channel > 1)
		return -EOPNOTSUPP;
	*str = type == hwmon_fan ? (channel ? "GPU fan" : "CPU fan") : (channel ? "GPU EC" : "CPU EC");
	return 0;
}
static const struct hwmon_ops sensor_ops = { .is_visible = sensor_visible, .read = sensor_read, .read_string = sensor_label };
static const struct hwmon_channel_info * const sensor_info[] = { HWMON_CHANNEL_INFO(temp, HWMON_T_INPUT | HWMON_T_LABEL, HWMON_T_INPUT | HWMON_T_LABEL), HWMON_CHANNEL_INFO(fan, HWMON_F_INPUT | HWMON_F_LABEL, HWMON_F_INPUT | HWMON_F_LABEL), NULL };
static const struct hwmon_chip_info chip = { .ops = &sensor_ops, .info = sensor_info };
static int __init r9t_init(void)
{
	struct pci_dev *gpu;
	bool matched = false;
	u8 project, support, oem[SLAYER_OEM_SIZE];
	unsigned int profile;
	int ret;
	if (!dmi_match(DMI_SYS_VENDOR, SLAYER_VENDOR) || !dmi_match(DMI_PRODUCT_NAME, SLAYER_PRODUCT) ||
	    !dmi_match(DMI_PRODUCT_SKU, SLAYER_SKU) || !dmi_match(DMI_BIOS_VERSION, SLAYER_BIOS))
		return -ENODEV;
	for_each_pci_dev(gpu) {
		if (gpu->vendor == 0x10de && gpu->device == 0x2d19 && gpu->subsystem_vendor == 0x1d05 && gpu->subsystem_device == 0x605c) {
			matched = true; pci_dev_put(gpu); break;
		}
	}
	/* Integrated-only firmware can hide NVIDIA completely. The signed BIOS,
  * exact DMI and native OEM board/project identity remain mandatory. */
 mux_available = !mux_read(oem);
 if ((!matched && (!mux_available || oem[SLAYER_MUX_OFFSET] != 2)) || ACPI_FAILURE(acpi_get_handle(NULL, "\\_SB.INOU", &ec)))
		return -ENODEV;
	if (read_byte(0x0740, &project) || project != 26 || read_byte(0x078e, &support) || !(support & BIT(3)) || profile_read(&profile))
		return -ENODEV;
	if (acpi_has_method(ec, "ECRW") == false)
		return -ENODEV;
	if (read_byte(0x0766, &support)) return -EIO;
 usb_charging_supported = !!(support & BIT(1));
 pdev = platform_device_register_simple("slayerctl", PLATFORM_DEVID_NONE, NULL, 0);
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);
	ret = sysfs_create_group(&pdev->dev.kobj, &group);
	if (ret)
		goto fail;
	hwmon = hwmon_device_register_with_info(&pdev->dev, "slayer_r9t", NULL, &chip, NULL);
	if (IS_ERR(hwmon)) {
		ret = PTR_ERR(hwmon); goto fail_group;
	}
	if (mux_available) {
  ret = sysfs_create_group(&pdev->dev.kobj, &mux_group);
  if (ret) goto fail_hwmon;
 }
 if (usb_charging_supported) {
  ret = device_create_file(&pdev->dev, &dev_attr_usb_sleep_charging);
  if (ret) goto fail_mux;
 }
 if (!read_byte(0x0751, &support)) {
  last_thermal = support & 0xf0;
 }
 ret = wmi_driver_register(&event_driver);
 if (!ret) event_driver_registered = true;
 else pr_warn("slayerctl_r9t: WMI event observer unavailable: %d\n", ret);
 attach_query_observer();
 battery_hook_register(&hook);
	pr_info("slayerctl_r9t: exact model/BIOS/project matched; factory thermal/fan/Fn and charge profiles; MUX=%d\n", mux_available);
	return 0;
fail_mux:
 if (mux_available) sysfs_remove_group(&pdev->dev.kobj, &mux_group);
fail_hwmon:
 hwmon_device_unregister(hwmon);
fail_group:
	sysfs_remove_group(&pdev->dev.kobj, &group);
fail:
	platform_device_unregister(pdev);
	return ret;
}
static void __exit r9t_exit(void)
{
 if (event_driver_registered) wmi_driver_unregister(&event_driver);
 if (profile_ec) acpi_ec_remove_query_handler(profile_ec, 0x88);
 cancel_delayed_work_sync(&thermal_event);
 put_device(profile_ec_device);
	battery_hook_unregister(&hook);
 if (usb_charging_supported) device_remove_file(&pdev->dev, &dev_attr_usb_sleep_charging);
 if (mux_available) sysfs_remove_group(&pdev->dev.kobj, &mux_group);
	hwmon_device_unregister(hwmon);
	sysfs_remove_group(&pdev->dev.kobj, &group);
	platform_device_unregister(pdev);
	/* Unload never writes or resets the user's selected battery profile. */
}
module_init(r9t_init);
module_exit(r9t_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Exact-model Game Garaj R9T controls and hwmon");
MODULE_VERSION(R9T_DRIVER_VERSION);
MODULE_IMPORT_NS("EFIVAR");
