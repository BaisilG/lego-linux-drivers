/*
 * Servo motor device class for LEGO MINDSTORMS EV3
 *
 * Copyright (C) 2014 David Lechner <david@lechnology.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * Note: The comment block below is used to generate docs on the ev3dev website.
 * Use kramdown (markdown) format. Use a '.' as a placeholder when blank lines
 * or leading whitespace is important for the markdown syntax.
 */

/**
 * DOC: website
 *
 * Servo Motor Class
 *
* The `servo-motor` class provides a uniform interface for using [hobby type
* servo motors](https://en.wikipedia.org/wiki/Servo_%%28radio_control%%29).
* .
* ### sysfs Attributes
* .
* Servo motors can be found at `/sys/class/servo-motor/motor<N>`, where `<N>`
* is incremented each time a servo is loaded (it is not related to which port
* the motor is plugged in to).
* .
* `command` (read/write)
* : Sets the command for the servo. Valid values are `run` and `float`. Setting
*   to `run` will cause the servo to be driven to the position set in the
*   `position` attribute. Setting to `float` will remove power from the motor.
* .
* `max_pulse_ms` (read/write)
* : Used to set the pulse size in milliseconds for the signal that tells the
*   servo to drive to the maximum (clockwise) position. Default value is 2400.
*   Valid values are 2300 to 2700. You must write to the position attribute for
*   changes to this attribute to take effect.
* .
* `mid_pulse_ms` (read/write)
* : Used to set the pulse size in milliseconds for the signal that tells the
*   servo to drive to the mid position. Default value is 1500. Valid
*   values are 1300 to 1700. For example, on a 180 degree servo, this would be
*   90 degrees. On continuous rotation servo, this is the "neutral" position
*   where the motor does not turn. You must write to the position attribute for
*   changes to this attribute to take effect.
* .
* `min_pulse_ms` (read/write)
* : Used to set the pulse size in milliseconds for the signal that tells the
*   servo to drive to the miniumum (counter-clockwise) position. Default value
*   is 600. Valid values are 300 to 700. You must write to the position
*   attribute for changes to this attribute to take effect.
* .
* `name` (read-only)
* : Returns the name of the servo controller's driver.
* .
* `polarity` (read/write)
* : Sets the polarity of the servo. Valid values are `normal` and `inverted`.
*   Setting the value to `inverted` will cause the position value to be
*   inverted. i.e `-100` will correspond to `max_pulse_ms`, and `100` will
*   correspond to `min_pulse_ms`.
* .
* `port_name` (read-only)
* : Returns the name of the port that the motor is connected to.
* .
* `position` (read/write)
* : Reading returns the current position of the servo. Writing instructs the
*   servo to move to the specified position. Units are percent. Valid values
*   are -100 to 100 (-100% to 100%) where `-100` corresponds to `min_pulse_ms`,
*   `0` corresponds to `mid_pulse_ms` and `100` corresponds to `max_pulse_ms`.
* .
* `rate` (read/write)
* : Sets the rate at which the servo travels from 0 to 100.0% (half of the full
*   range of the servo). Units are in milliseconds. Example: Setting the rate
*   to 1000 means that it will take a 180 degree servo 2 second to move from 0
*   to 180 degrees. Note: Some servo controllers may not support this in which
*   case reading and writing will fail with -ENOSYS. In continuous rotation
*   servos, this value will affect the rate at which the speed ramps up or down.
*/

#include <linux/device.h>
#include <linux/module.h>

#include <servo_motor_class.h>

const char *servo_motor_command_values[] = {
	[SERVO_MOTOR_COMMAND_RUN]	= "run",
	[SERVO_MOTOR_COMMAND_FLOAT]	= "float",
};

const char *servo_motor_polarity_values[] = {
	[SERVO_MOTOR_POLARITY_NORMAL]	= "normal",
	[SERVO_MOTOR_POLARITY_INVERTED]	= "inverted",
};

inline int servo_motor_class_scale(unsigned in_min, unsigned in_max,
				   unsigned out_min, unsigned out_max,
				   unsigned value)
{
	long scaled = value - in_min;
	scaled *= out_max - out_min;
	scaled /= in_max - in_min;
	scaled += out_min;
	return scaled;
}

int servo_motor_class_get_command(struct servo_motor_device *motor)
{
	int ret;

	ret = motor->ops.get_position(motor->context);
	if (ret < 0)
		return ret;

	return ret ? SERVO_MOTOR_COMMAND_RUN : SERVO_MOTOR_COMMAND_FLOAT;
}

int servo_motor_class_set_position(struct servo_motor_device *motor,
				   int new_position,
				   enum servo_motor_polarity new_polarity)
{
	int scaled_position;

	motor->polarity = new_polarity;
	motor->position = new_position;

	if (motor->command == SERVO_MOTOR_COMMAND_RUN) {
		if (new_polarity == SERVO_MOTOR_POLARITY_INVERTED)
			new_position = -new_position;
		if (new_position > 0)
			scaled_position = servo_motor_class_scale(0, 100,
				motor->mid_pulse_ms, motor->max_pulse_ms,
				new_position);
		else
			scaled_position = servo_motor_class_scale(-100, 0,
				motor->min_pulse_ms, motor->mid_pulse_ms,
				new_position);
		return motor->ops.set_position(motor->context, scaled_position);
	}
	return 0;
}

static ssize_t name_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct servo_motor_device *motor = to_servo_motor_device(dev);

	return snprintf(buf, SERVO_MOTOR_NAME_SIZE, "%s\n", motor->name);
}

static ssize_t port_name_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct servo_motor_device *motor = to_servo_motor_device(dev);

	return snprintf(buf, SERVO_MOTOR_NAME_SIZE, "%s\n", motor->port_name);
}

static ssize_t min_pulse_ms_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct servo_motor_device *motor = to_servo_motor_device(dev);

	return sprintf(buf, "%u\n", motor->min_pulse_ms);
}

static ssize_t min_pulse_ms_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct servo_motor_device *motor = to_servo_motor_device(dev);
	unsigned value;

	if (sscanf(buf, "%ud", &value) != 1 || value > 700 || value < 300)
		return -EINVAL;
	motor->min_pulse_ms = value;

	return count;
}

static ssize_t mid_pulse_ms_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct servo_motor_device *motor = to_servo_motor_device(dev);

	return sprintf(buf, "%u\n", motor->mid_pulse_ms);
}

static ssize_t mid_pulse_ms_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct servo_motor_device *servo = to_servo_motor_device(dev);
	unsigned value;

	if (sscanf(buf, "%ud", &value) != 1 || value > 1700 || value < 1300)
		return -EINVAL;
	servo->mid_pulse_ms = value;

	return count;
}

static ssize_t max_pulse_ms_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct servo_motor_device *motor = to_servo_motor_device(dev);

	return snprintf(buf, SERVO_MOTOR_NAME_SIZE, "%u\n", motor->max_pulse_ms);
}

static ssize_t max_pulse_ms_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct servo_motor_device *motor = to_servo_motor_device(dev);
	unsigned value;

	if (sscanf(buf, "%ud", &value) != 1 || value > 2700 || value < 2300)
		return -EINVAL;
	motor->max_pulse_ms = value;

	return count;
}

static ssize_t command_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct servo_motor_device *motor = to_servo_motor_device(dev);

	return sprintf(buf, "%s\n", servo_motor_command_values[motor->command]);
}

static ssize_t command_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t size)
{
	struct servo_motor_device *motor = to_servo_motor_device(dev);
	int i, err;

	for (i = 0; i < NUM_SERVO_MOTOR_COMMAND; i++) {
		if (!sysfs_streq(buf, servo_motor_command_values[i]))
			continue;
		if (motor->command == i)
			return size;

		motor->command = i;
		if (motor->command == SERVO_MOTOR_COMMAND_RUN)
			err = servo_motor_class_set_position(motor, motor->position,
							     motor->polarity);
		else
			err = motor->ops.set_position(motor->context, 0);
		if (err)
			return err;
		return size;
	}

	return -EINVAL;
}

static ssize_t polarity_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct servo_motor_device *motor = to_servo_motor_device(dev);

	return sprintf(buf, "%s\n", servo_motor_polarity_values[motor->polarity]);
}

static ssize_t polarity_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct servo_motor_device *motor = to_servo_motor_device(dev);
	int i, err;

	for (i = 0; i < NUM_SERVO_MOTOR_POLARITY; i++) {
		if (!sysfs_streq(buf, servo_motor_polarity_values[i]))
			continue;

		if (motor->polarity != i) {
			err = servo_motor_class_set_position(motor,
							     motor->position, i);
			if (err)
				return err;
		}
		return size;
	}

	return -EINVAL;
}

static ssize_t position_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct servo_motor_device *motor = to_servo_motor_device(dev);
	int ret;

	ret = motor->ops.get_position(motor->context);
	if (ret < 0)
		return ret;
	if (ret == 0)
		ret = motor->position;
	else if (ret < motor->mid_pulse_ms)
		ret =  servo_motor_class_scale(motor->min_pulse_ms,
			motor->mid_pulse_ms, -100, 0, ret);
	else
		ret = servo_motor_class_scale(motor->mid_pulse_ms,
			motor->max_pulse_ms, 0, 100, ret);
	return sprintf(buf, "%d\n", ret);
}

static ssize_t position_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct servo_motor_device *motor = to_servo_motor_device(dev);
	int value, err;

	if (sscanf(buf, "%d", &value) != 1 || value > 100 || value < -100)
		return -EINVAL;
	if (motor->position != value) {
		err = servo_motor_class_set_position(motor, value,
						     motor->polarity);
		if (err)
			return err;
	}

	return count;
}

static ssize_t rate_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct servo_motor_device *motor = to_servo_motor_device(dev);
	int ret;

	if (!motor->ops.get_rate)
		return -ENOSYS;
	ret = motor->ops.get_rate(motor->context);
	if (ret < 0)
		return ret;
	return sprintf(buf, "%d\n", ret);
}

static ssize_t rate_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct servo_motor_device *motor = to_servo_motor_device(dev);
	unsigned value;
	int err;

	if (!motor->ops.set_rate)
		return -ENOSYS;

	if (sscanf(buf, "%ud", &value) != 1)
		return -EINVAL;
	err = motor->ops.set_rate(motor->context, value);
	if (err < 0)
		return err;

	return count;
}

static DEVICE_ATTR_RO(name);
static DEVICE_ATTR_RO(port_name);
static DEVICE_ATTR_RW(min_pulse_ms);
static DEVICE_ATTR_RW(mid_pulse_ms);
static DEVICE_ATTR_RW(max_pulse_ms);
static DEVICE_ATTR_RW(command);
static DEVICE_ATTR_RW(polarity);
static DEVICE_ATTR_RW(position);
static DEVICE_ATTR_RW(rate);

static struct attribute *servo_motor_class_attrs[] = {
	&dev_attr_name.attr,
	&dev_attr_port_name.attr,
	&dev_attr_min_pulse_ms.attr,
	&dev_attr_mid_pulse_ms.attr,
	&dev_attr_max_pulse_ms.attr,
	&dev_attr_command.attr,
	&dev_attr_polarity.attr,
	&dev_attr_position.attr,
	&dev_attr_rate.attr,
	NULL
};

static const struct attribute_group servo_motor_class_group = {
	.attrs		= servo_motor_class_attrs,
};

static const struct attribute_group *servo_motor_class_groups[] = {
	&servo_motor_class_group,
	NULL
};

static void servo_motor_release(struct device *dev)
{
}

static unsigned servo_motor_class_id = 0;
struct class servo_motor_class;

int register_servo_motor(struct servo_motor_device *servo, struct device *parent)
{
	int ret;

	if (!servo || !servo->port_name || !parent)
		return -EINVAL;

	servo->dev.release = servo_motor_release;
	servo->dev.parent = parent;
	servo->dev.class = &servo_motor_class;
	dev_set_name(&servo->dev, "motor%d", servo_motor_class_id++);
	servo->min_pulse_ms = 600;
	servo->mid_pulse_ms = 1500;
	servo->max_pulse_ms = 2400;
	ret = servo_motor_class_get_command(servo);
	if (ret < 0)
		return ret;
	servo->command = ret;

	ret = device_register(&servo->dev);
	if (ret)
		return ret;

	dev_info(&servo->dev, "Bound to device '%s'\n", dev_name(parent));

	return 0;
}
EXPORT_SYMBOL_GPL(register_servo_motor);

void unregister_servo_motor(struct servo_motor_device *servo)
{
	dev_info(&servo->dev, "Unregistered\n");
	device_unregister(&servo->dev);
}
EXPORT_SYMBOL_GPL(unregister_servo_motor);

static int servo_motor_dev_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct servo_motor_device *servo = to_servo_motor_device(dev);
	int ret;

	ret = add_uevent_var(env, "NAME=%s", servo->name);
	if (ret) {
		dev_err(dev, "failed to add uevent NAME\n");
		return ret;
	}
	add_uevent_var(env, "PORT_NAME=%s", servo->port_name);
	if (ret) {
		dev_err(dev, "failed to add uevent PORT_NAME\n");
		return ret;
	}

	return 0;
}

static char *servo_motor_devnode(struct device *dev, umode_t *mode)
{
	return kasprintf(GFP_KERNEL, "servo-motor/%s", dev_name(dev));
}

struct class servo_motor_class = {
	.name		= "servo-motor",
	.owner		= THIS_MODULE,
	.dev_groups	= servo_motor_class_groups,
	.dev_uevent	= servo_motor_dev_uevent,
	.devnode	= servo_motor_devnode,
};
EXPORT_SYMBOL_GPL(servo_motor_class);

static int __init servo_motor_class_init(void)
{
	int err;

	err = class_register(&servo_motor_class);
	if (err) {
		pr_err("unable to register servo motor device class\n");
		return err;
	}

	return 0;
}
module_init(servo_motor_class_init);

static void __exit servo_motor_class_exit(void)
{
	class_unregister(&servo_motor_class);
}
module_exit(servo_motor_class_exit);

MODULE_DESCRIPTION("Servo motor device class for LEGO MINDSTORMS EV3");
MODULE_AUTHOR("David Lechner <david@lechnology.com>");
MODULE_LICENSE("GPL");