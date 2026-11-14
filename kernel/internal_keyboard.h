/* SPDX-License-Identifier: GPL-2.0-only */
/* Included after the exact-model platform device declaration. No key code,
 * character or event stream is exported. Userspace sees only last activity.
 * Input notifications are one-shot: while typing, ctl only checks the last
 * timestamp at the idle deadline. No per-keystroke userspace wakeups. */
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/timekeeping.h>
#include "internal_keyboard_contract.h"

static atomic_t keyboard_present = ATOMIC_INIT(0);
static atomic_t keyboard_monitor = ATOMIC_INIT(0);
static atomic_t keyboard_armed = ATOMIC_INIT(0);
static atomic_t keyboard_presence_changed = ATOMIC_INIT(0);
static atomic64_t keyboard_last_ms = ATOMIC64_INIT(0);
static bool keyboard_handler_registered;
static void keyboard_notify_work(struct work_struct *work)
{
 sysfs_notify(&pdev->dev.kobj, NULL, "keyboard_activity");
 if (atomic_xchg(&keyboard_presence_changed, 0)) kobject_uevent(&pdev->dev.kobj, KOBJ_CHANGE);
}
static DECLARE_WORK(keyboard_work, keyboard_notify_work);

static void keyboard_event(struct input_handle *handle, unsigned int type,
                           unsigned int code, int value)
{
 if (type != EV_KEY || (value != 1 && value != 2) || !atomic_read(&keyboard_monitor)) return;
 atomic64_set(&keyboard_last_ms, ktime_to_ms(ktime_get_boottime()));
 if (atomic_xchg(&keyboard_armed, 0)) schedule_work(&keyboard_work);
}
static int keyboard_connect(struct input_handler *handler, struct input_dev *dev,
                            const struct input_device_id *id)
{
 struct input_handle *handle;
 struct device *parent = dev->dev.parent;
 int ret;
 if (!parent || !parent->driver ||
     !slayer_internal_keyboard(dev->id.bustype, dev->phys, dev_name(parent), parent->driver->name))
  return -ENODEV;
 handle = kzalloc(sizeof(*handle), GFP_KERNEL);
 if (!handle) return -ENOMEM;
 handle->dev = dev; handle->handler = handler; handle->name = "slayer-internal-activity";
 ret = input_register_handle(handle);
 if (ret) goto fail;
 ret = input_open_device(handle);
 if (ret) { input_unregister_handle(handle); goto fail; }
 atomic_inc(&keyboard_present);
 atomic_set(&keyboard_presence_changed, 1);
 schedule_work(&keyboard_work);
 return 0;
fail:
 kfree(handle); return ret;
}
static void keyboard_disconnect(struct input_handle *handle)
{
 input_close_device(handle);
 input_unregister_handle(handle);
 atomic_dec(&keyboard_present);
 atomic_set(&keyboard_presence_changed, 1);
 schedule_work(&keyboard_work);
 kfree(handle);
}
static const struct input_device_id keyboard_ids[] = {
 { .flags = INPUT_DEVICE_ID_MATCH_BUS, .bustype = BUS_I8042 }, { }
};
static struct input_handler keyboard_handler = {
 .event = keyboard_event, .connect = keyboard_connect, .disconnect = keyboard_disconnect,
 .name = "slayer-internal-activity", .id_table = keyboard_ids, .passive_observer = true,
};
static ssize_t keyboard_activity_show(struct device *dev, struct device_attribute *attr, char *buf)
{ return sysfs_emit(buf, "%lld\n", (long long)atomic64_read(&keyboard_last_ms)); }
static DEVICE_ATTR(keyboard_activity, 0400, keyboard_activity_show, NULL);
static ssize_t keyboard_activity_present_show(struct device *dev, struct device_attribute *attr, char *buf)
{ return sysfs_emit(buf, "%u\n", atomic_read(&keyboard_present) > 0); }
static DEVICE_ATTR_RO(keyboard_activity_present);
static ssize_t keyboard_activity_monitor_show(struct device *dev, struct device_attribute *attr, char *buf)
{ return sysfs_emit(buf, "%u\n", atomic_read(&keyboard_monitor)); }
static ssize_t keyboard_activity_monitor_store(struct device *dev, struct device_attribute *attr,
                                              const char *buf, size_t count)
{
 bool enabled; int ret = kstrtobool(buf, &enabled);
 if (ret) return ret;
 if (enabled && !atomic_read(&keyboard_present)) return -ENODEV;
 atomic_set(&keyboard_armed, 0);
 if (enabled) atomic64_set(&keyboard_last_ms, ktime_to_ms(ktime_get_boottime()));
 atomic_set(&keyboard_monitor, enabled);
 return count;
}
static DEVICE_ATTR(keyboard_activity_monitor, 0600, keyboard_activity_monitor_show, keyboard_activity_monitor_store);
static ssize_t keyboard_activity_arm_store(struct device *dev, struct device_attribute *attr,
                                          const char *buf, size_t count)
{
 bool armed; int ret = kstrtobool(buf, &armed);
 if (ret) return ret;
 if (armed && (!atomic_read(&keyboard_monitor) || !atomic_read(&keyboard_present))) return -ENODEV;
 atomic_set(&keyboard_armed, armed);
 return count;
}
static DEVICE_ATTR(keyboard_activity_arm, 0200, NULL, keyboard_activity_arm_store);
static void keyboard_activity_exit(void)
{
 atomic_set(&keyboard_monitor, 0); atomic_set(&keyboard_armed, 0);
 if (keyboard_handler_registered) input_unregister_handler(&keyboard_handler);
 cancel_work_sync(&keyboard_work);
}
