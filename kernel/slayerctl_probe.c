// SPDX-License-Identifier: GPL-2.0-only
/* R9T/IDK diagnostic backend. Deliberately contains NO EC/UEFI write path. */
#include <linux/acpi.h>
#include <linux/dmi.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>

static acpi_handle ec_handle;
static struct platform_device *probe_device;
static DEFINE_MUTEX(ec_mutex);

struct register_cache {
	u16 address;
	u8 value;
	bool valid;
	unsigned long updated;
};

/* Constants from this device family's vendor metadata. Exposing the raw
 * values does not imply that a feature is available or writable. */
static struct register_cache registers[] = {
	{ .address = 0x0740 }, /* project identifier */
	{ .address = 0x0765 }, /* support byte 1 */
	{ .address = 0x0766 }, /* support byte 2 */
	{ .address = 0x0742 }, /* support byte 5 */
	{ .address = 0x078e }, /* support byte 6 */
	{ .address = 0x07a6 }, /* charging profile, raw */
	{ .address = 0x07b9 }, /* upper charge threshold/status, raw */
	{ .address = 0x07d0 }, /* lower threshold, raw */
	{ .address = 0x0751 }, /* fan control bits, raw */
	{ .address = 0x072a }, /* GPU status bits, raw */
};

static ssize_t read_register(unsigned int index, char *buffer)
{
	struct register_cache *r = &registers[index];
	union acpi_object input = { .type = ACPI_TYPE_INTEGER };
	struct acpi_object_list arguments = { .count = 1, .pointer = &input };
	unsigned long long value;
	acpi_status status;
	int error = 0;

	mutex_lock(&ec_mutex);
	if (!r->valid || time_after(jiffies, r->updated + HZ)) {
		input.integer.value = r->address;
		status = acpi_evaluate_integer(ec_handle, "ECRR", &arguments, &value);
		if (ACPI_FAILURE(status) || value > 0xff) {
			error = -EIO;
			goto out;
		}
		r->value = value;
		r->valid = true;
		r->updated = jiffies;
	}
	error = sysfs_emit(buffer, "0x%02x\n", r->value);
out:
	mutex_unlock(&ec_mutex);
	return error;
}

#define EC_ATTRIBUTE(name, index) \
static ssize_t name##_show(struct device *dev, struct device_attribute *attr, char *buf) \
{ return read_register(index, buf); } \
static DEVICE_ATTR_RO(name)

EC_ATTRIBUTE(ec_project, 0);
EC_ATTRIBUTE(ec_support_1, 1);
EC_ATTRIBUTE(ec_support_2, 2);
EC_ATTRIBUTE(ec_support_5, 3);
EC_ATTRIBUTE(ec_support_6, 4);
EC_ATTRIBUTE(ec_charge_profile, 5);
EC_ATTRIBUTE(ec_charge_upper, 6);
EC_ATTRIBUTE(ec_charge_lower, 7);
EC_ATTRIBUTE(ec_fan_control, 8);
EC_ATTRIBUTE(ec_gpu_status, 9);

static ssize_t backend_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "0.0.1 read-only R9T-IDK-N.1.46GAR16\n");
}
static DEVICE_ATTR_RO(backend_version);

static struct attribute *probe_attrs[] = {
	&dev_attr_backend_version.attr, &dev_attr_ec_project.attr,
	&dev_attr_ec_support_1.attr, &dev_attr_ec_support_2.attr,
	&dev_attr_ec_support_5.attr, &dev_attr_ec_support_6.attr,
	&dev_attr_ec_charge_profile.attr, &dev_attr_ec_charge_upper.attr,
	&dev_attr_ec_charge_lower.attr, &dev_attr_ec_fan_control.attr,
	&dev_attr_ec_gpu_status.attr, NULL,
};
ATTRIBUTE_GROUPS(probe);

static int __init slayer_probe_init(void)
{
	struct pci_dev *gpu;
	bool matched = false;
	int error;

	/* No force parameter: a generic ACPI method name is not a model contract. */
	if (!dmi_match(DMI_SYS_VENDOR, "GAME GARAJ") ||
	    !dmi_match(DMI_PRODUCT_NAME, "SLAYER R9T") ||
	    !dmi_match(DMI_PRODUCT_SKU, "R9T V1") ||
	    !dmi_match(DMI_BIOS_VERSION, "N.1.46GAR16"))
		return -ENODEV;
	for_each_pci_dev(gpu) {
		if (gpu->vendor == 0x10de && gpu->device == 0x2d19 &&
		    gpu->subsystem_vendor == 0x1d05 && gpu->subsystem_device == 0x605c) {
			matched = true;
			pci_dev_put(gpu);
			break;
		}
	}
	if (!matched || ACPI_FAILURE(acpi_get_handle(NULL, "\\_SB.INOU", &ec_handle)))
		return -ENODEV;
	probe_device = platform_device_register_simple("slayerctl", PLATFORM_DEVID_NONE, NULL, 0);
	if (IS_ERR(probe_device))
		return PTR_ERR(probe_device);
	error = sysfs_create_groups(&probe_device->dev.kobj, probe_groups);
	if (error) {
		platform_device_unregister(probe_device);
		return error;
	}
	pr_info("slayerctl: read-only R9T diagnostic interface registered; no EC writes\n");
	return 0;
}
static void __exit slayer_probe_exit(void)
{
	sysfs_remove_groups(&probe_device->dev.kobj, probe_groups);
	platform_device_unregister(probe_device);
}
module_init(slayer_probe_init);
module_exit(slayer_probe_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Read-only model-gated Game Garaj R9T EC diagnostics");
MODULE_VERSION("0.0.1");
