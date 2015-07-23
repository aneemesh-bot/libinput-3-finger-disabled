/*
 * Copyright © 2013 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <config.h>

#include <stdio.h>
#include <check.h>
#include <errno.h>
#include <fcntl.h>
#include <libinput.h>
#include <math.h>
#include <unistd.h>
#include <values.h>

#include "libinput-util.h"
#include "litest.h"

static void
test_relative_event(struct litest_device *dev, int dx, int dy)
{
	struct libinput *li = dev->libinput;
	struct libinput_event_pointer *ptrev;
	struct libinput_event *event;
	double ev_dx, ev_dy;
	double expected_dir;
	double expected_length;
	double actual_dir;
	double actual_length;

	litest_event(dev, EV_REL, REL_X, dx);
	litest_event(dev, EV_REL, REL_Y, dy);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);

	libinput_dispatch(li);

	event = libinput_get_event(li);
	ptrev = litest_is_motion_event(event);

	expected_length = sqrt(4 * dx*dx + 4 * dy*dy);
	expected_dir = atan2(dx, dy);

	ev_dx = libinput_event_pointer_get_dx(ptrev);
	ev_dy = libinput_event_pointer_get_dy(ptrev);
	actual_length = sqrt(ev_dx*ev_dx + ev_dy*ev_dy);
	actual_dir = atan2(ev_dx, ev_dy);

	/* Check the length of the motion vector (tolerate 1.0 indifference). */
	litest_assert(fabs(expected_length) >= actual_length);

	/* Check the direction of the motion vector (tolerate 2π/4 radians
	 * indifference). */
	litest_assert(fabs(expected_dir - actual_dir) < M_PI_2);

	libinput_event_destroy(event);

	litest_drain_events(dev->libinput);
}

static void
disable_button_scrolling(struct litest_device *device)
{
	struct libinput_device *dev = device->libinput_device;
	enum libinput_config_status status,
				    expected;

	status = libinput_device_config_scroll_set_method(dev,
					LIBINPUT_CONFIG_SCROLL_NO_SCROLL);

	expected = LIBINPUT_CONFIG_STATUS_SUCCESS;
	litest_assert_int_eq(status, expected);
}

START_TEST(pointer_motion_relative)
{
	struct litest_device *dev = litest_current_device();

	/* send a single event, the first movement
	   is always decelerated by 0.3 */
	litest_event(dev, EV_REL, REL_X, 1);
	litest_event(dev, EV_REL, REL_Y, 0);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
	libinput_dispatch(dev->libinput);

	litest_drain_events(dev->libinput);

	test_relative_event(dev, 1, 0);
	test_relative_event(dev, 1, 1);
	test_relative_event(dev, 1, -1);
	test_relative_event(dev, 0, 1);

	test_relative_event(dev, -1, 0);
	test_relative_event(dev, -1, 1);
	test_relative_event(dev, -1, -1);
	test_relative_event(dev, 0, -1);
}
END_TEST

START_TEST(pointer_motion_relative_zero)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	int i;

	/* NOTE: this test does virtually nothing. The kernel should not
	 * allow 0/0 events to be passed to userspace. If it ever happens,
	 * let's hope this test fails if we do the wrong thing.
	 */
	litest_drain_events(li);

	for (i = 0; i < 5; i++) {
		litest_event(dev, EV_REL, REL_X, 0);
		litest_event(dev, EV_REL, REL_Y, 0);
		litest_event(dev, EV_SYN, SYN_REPORT, 0);
		libinput_dispatch(li);
	}
	litest_assert_empty_queue(li);

	/* send a single event, the first movement
	   is always decelerated by 0.3 */
	litest_event(dev, EV_REL, REL_X, 1);
	litest_event(dev, EV_REL, REL_Y, 0);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
	libinput_dispatch(li);

	libinput_event_destroy(libinput_get_event(li));
	litest_assert_empty_queue(li);

	for (i = 0; i < 5; i++) {
		litest_event(dev, EV_REL, REL_X, 0);
		litest_event(dev, EV_REL, REL_Y, 0);
		litest_event(dev, EV_SYN, SYN_REPORT, 0);
		libinput_dispatch(dev->libinput);
	}
	litest_assert_empty_queue(li);

}
END_TEST

START_TEST(pointer_motion_relative_min_decel)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event_pointer *ptrev;
	struct libinput_event *event;
	double evx, evy;
	int dx, dy;
	int cardinal = _i; /* ranged test */
	double len;

	int deltas[8][2] = {
		/* N, NE, E, ... */
		{ 0, 1 },
		{ 1, 1 },
		{ 1, 0 },
		{ 1, -1 },
		{ 0, -1 },
		{ -1, -1 },
		{ -1, 0 },
		{ -1, 1 },
	};

	litest_drain_events(dev->libinput);

	dx = deltas[cardinal][0];
	dy = deltas[cardinal][1];

	litest_event(dev, EV_REL, REL_X, dx);
	litest_event(dev, EV_REL, REL_Y, dy);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
	libinput_dispatch(li);

	event = libinput_get_event(li);
	ptrev = litest_is_motion_event(event);
	evx = libinput_event_pointer_get_dx(ptrev);
	evy = libinput_event_pointer_get_dy(ptrev);

	ck_assert((evx == 0.0) == (dx == 0));
	ck_assert((evy == 0.0) == (dy == 0));

	len = hypot(evx, evy);
	ck_assert(fabs(len) >= 0.3);

	libinput_event_destroy(event);
}
END_TEST

static void
test_absolute_event(struct litest_device *dev, double x, double y)
{
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	struct libinput_event_pointer *ptrev;
	double ex, ey;
	enum libinput_event_type type = LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE;

	litest_touch_down(dev, 0, x, y);
	libinput_dispatch(li);

	event = libinput_get_event(li);
	litest_assert_notnull(event);
	litest_assert_int_eq(libinput_event_get_type(event), type);

	ptrev = libinput_event_get_pointer_event(event);
	litest_assert(ptrev != NULL);

	ex = libinput_event_pointer_get_absolute_x_transformed(ptrev, 100);
	ey = libinput_event_pointer_get_absolute_y_transformed(ptrev, 100);
	litest_assert_int_eq((int)(ex + 0.5), (int)x);
	litest_assert_int_eq((int)(ey + 0.5), (int)y);

	libinput_event_destroy(event);
}

START_TEST(pointer_motion_absolute)
{
	struct litest_device *dev = litest_current_device();

	litest_drain_events(dev->libinput);

	test_absolute_event(dev, 0, 100);
	test_absolute_event(dev, 100, 0);
	test_absolute_event(dev, 50, 50);
}
END_TEST

START_TEST(pointer_absolute_initial_state)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *libinput1, *libinput2;
	struct libinput_event *ev1, *ev2;
	struct libinput_event_pointer *p1, *p2;
	int axis = _i; /* looped test */

	libinput1 = dev->libinput;
	litest_touch_down(dev, 0, 40, 60);
	litest_touch_up(dev, 0);

	/* device is now on some x/y value */
	litest_drain_events(libinput1);

	libinput2 = litest_create_context();
	libinput_path_add_device(libinput2,
				 libevdev_uinput_get_devnode(dev->uinput));
	litest_drain_events(libinput2);

	if (axis == ABS_X)
		litest_touch_down(dev, 0, 40, 70);
	else
		litest_touch_down(dev, 0, 70, 60);
	litest_touch_up(dev, 0);

	litest_wait_for_event(libinput1);
	litest_wait_for_event(libinput2);

	while (libinput_next_event_type(libinput1)) {
		ev1 = libinput_get_event(libinput1);
		ev2 = libinput_get_event(libinput2);

		ck_assert_int_eq(libinput_event_get_type(ev1),
				 LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE);
		ck_assert_int_eq(libinput_event_get_type(ev1),
				 libinput_event_get_type(ev2));

		p1 = libinput_event_get_pointer_event(ev1);
		p2 = libinput_event_get_pointer_event(ev2);

		ck_assert_int_eq(libinput_event_pointer_get_absolute_x(p1),
				 libinput_event_pointer_get_absolute_x(p2));
		ck_assert_int_eq(libinput_event_pointer_get_absolute_y(p1),
				 libinput_event_pointer_get_absolute_y(p2));

		libinput_event_destroy(ev1);
		libinput_event_destroy(ev2);
	}

	libinput_unref(libinput2);
}
END_TEST

static void
test_unaccel_event(struct litest_device *dev, int dx, int dy)
{
      struct libinput *li = dev->libinput;
      struct libinput_event *event;
      struct libinput_event_pointer *ptrev;
      double ev_dx, ev_dy;

      litest_event(dev, EV_REL, REL_X, dx);
      litest_event(dev, EV_REL, REL_Y, dy);
      litest_event(dev, EV_SYN, SYN_REPORT, 0);

      libinput_dispatch(li);

      event = libinput_get_event(li);
      ptrev = litest_is_motion_event(event);

      ev_dx = libinput_event_pointer_get_dx_unaccelerated(ptrev);
      ev_dy = libinput_event_pointer_get_dy_unaccelerated(ptrev);

      litest_assert_int_eq(dx, ev_dx);
      litest_assert_int_eq(dy, ev_dy);

      libinput_event_destroy(event);

      litest_drain_events(dev->libinput);
}

START_TEST(pointer_motion_unaccel)
{
      struct litest_device *dev = litest_current_device();

      litest_drain_events(dev->libinput);

      test_unaccel_event(dev, 10, 0);
      test_unaccel_event(dev, 10, 10);
      test_unaccel_event(dev, 10, -10);
      test_unaccel_event(dev, 0, 10);

      test_unaccel_event(dev, -10, 0);
      test_unaccel_event(dev, -10, 10);
      test_unaccel_event(dev, -10, -10);
      test_unaccel_event(dev, 0, -10);
}
END_TEST

static void
test_button_event(struct litest_device *dev, unsigned int button, int state)
{
	struct libinput *li = dev->libinput;

	litest_event(dev, EV_KEY, button, state);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);

	litest_assert_button_event(li, button,
				   state ?  LIBINPUT_BUTTON_STATE_PRESSED :
					   LIBINPUT_BUTTON_STATE_RELEASED);
}

START_TEST(pointer_button)
{
	struct litest_device *dev = litest_current_device();

	disable_button_scrolling(dev);

	litest_drain_events(dev->libinput);

	test_button_event(dev, BTN_LEFT, 1);
	test_button_event(dev, BTN_LEFT, 0);

	/* press it twice for good measure */
	test_button_event(dev, BTN_LEFT, 1);
	test_button_event(dev, BTN_LEFT, 0);

	if (libevdev_has_event_code(dev->evdev, EV_KEY, BTN_RIGHT)) {
		test_button_event(dev, BTN_RIGHT, 1);
		test_button_event(dev, BTN_RIGHT, 0);
	}

	/* Skip middle button test on trackpoints (used for scrolling) */
	if (libevdev_has_event_code(dev->evdev, EV_KEY, BTN_MIDDLE)) {
		test_button_event(dev, BTN_MIDDLE, 1);
		test_button_event(dev, BTN_MIDDLE, 0);
	}
}
END_TEST

START_TEST(pointer_button_auto_release)
{
	struct libinput *libinput;
	struct litest_device *dev;
	struct libinput_event *event;
	enum libinput_event_type type;
	struct libinput_event_pointer *pevent;
	struct {
		int code;
		int released;
	} buttons[] = {
		{ .code = BTN_LEFT, },
		{ .code = BTN_MIDDLE, },
		{ .code = BTN_EXTRA, },
		{ .code = BTN_SIDE, },
		{ .code = BTN_BACK, },
		{ .code = BTN_FORWARD, },
		{ .code = BTN_4, },
	};
	int events[2 * (ARRAY_LENGTH(buttons) + 1)];
	unsigned i;
	int button;
	int valid_code;

	/* Enable all tested buttons on the device */
	for (i = 0; i < 2 * ARRAY_LENGTH(buttons);) {
		button = buttons[i / 2].code;
		events[i++] = EV_KEY;
		events[i++] = button;
	}
	events[i++] = -1;
	events[i++] = -1;

	libinput = litest_create_context();
	dev = litest_add_device_with_overrides(libinput,
					       LITEST_MOUSE,
					       "Generic mouse",
					       NULL, NULL, events);

	litest_drain_events(libinput);

	/* Send pressed events, without releasing */
	for (i = 0; i < ARRAY_LENGTH(buttons); ++i) {
		test_button_event(dev, buttons[i].code, 1);
	}

	litest_drain_events(libinput);

	/* "Disconnect" device */
	litest_delete_device(dev);

	/* Mark all released buttons until device is removed */
	while (1) {
		event = libinput_get_event(libinput);
		ck_assert_notnull(event);
		type = libinput_event_get_type(event);

		if (type == LIBINPUT_EVENT_DEVICE_REMOVED) {
			libinput_event_destroy(event);
			break;
		}

		ck_assert_int_eq(type, LIBINPUT_EVENT_POINTER_BUTTON);
		pevent = libinput_event_get_pointer_event(event);
		ck_assert_int_eq(libinput_event_pointer_get_button_state(pevent),
				 LIBINPUT_BUTTON_STATE_RELEASED);
		button = libinput_event_pointer_get_button(pevent);

		valid_code = 0;
		for (i = 0; i < ARRAY_LENGTH(buttons); ++i) {
			if (buttons[i].code == button) {
				ck_assert_int_eq(buttons[i].released, 0);
				buttons[i].released = 1;
				valid_code = 1;
			}
		}
		ck_assert_int_eq(valid_code, 1);
		libinput_event_destroy(event);
	}

	/* Check that all pressed buttons has been released. */
	for (i = 0; i < ARRAY_LENGTH(buttons); ++i) {
		ck_assert_int_eq(buttons[i].released, 1);
	}

	libinput_unref(libinput);
}
END_TEST

static void
test_wheel_event(struct litest_device *dev, int which, int amount)
{
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	struct libinput_event_pointer *ptrev;
	enum libinput_pointer_axis axis;

	/* the current evdev implementation scales the scroll wheel events
	   up by a factor 15 */
	const int scroll_step = 15;
	int expected = amount * scroll_step;
	int discrete = amount;

	if (libinput_device_config_scroll_get_natural_scroll_enabled(dev->libinput_device)) {
		expected *= -1;
		discrete *= -1;
	}

	/* mouse scroll wheels are 'upside down' */
	if (which == REL_WHEEL)
		amount *= -1;
	litest_event(dev, EV_REL, which, amount);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);

	libinput_dispatch(li);

	axis = (which == REL_WHEEL) ?
				LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL :
				LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL;
	event = libinput_get_event(li);
	ptrev = litest_is_axis_event(event,
				     axis,
				     LIBINPUT_POINTER_AXIS_SOURCE_WHEEL);

	litest_assert_int_eq(libinput_event_pointer_get_axis_value(ptrev, axis),
			 expected);
	litest_assert_int_eq(libinput_event_pointer_get_axis_value_discrete(ptrev, axis),
			     discrete);
	libinput_event_destroy(event);
}

START_TEST(pointer_scroll_wheel)
{
	struct litest_device *dev = litest_current_device();

	litest_drain_events(dev->libinput);

	/* make sure we hit at least one of the below two conditions */
	ck_assert(libevdev_has_event_code(dev->evdev, EV_REL, REL_WHEEL) ||
		  libevdev_has_event_code(dev->evdev, EV_REL, REL_HWHEEL));

	if (libevdev_has_event_code(dev->evdev, EV_REL, REL_WHEEL)) {
		test_wheel_event(dev, REL_WHEEL, -1);
		test_wheel_event(dev, REL_WHEEL, 1);

		test_wheel_event(dev, REL_WHEEL, -5);
		test_wheel_event(dev, REL_WHEEL, 6);
	}

	if (libevdev_has_event_code(dev->evdev, EV_REL, REL_HWHEEL)) {
		test_wheel_event(dev, REL_HWHEEL, -1);
		test_wheel_event(dev, REL_HWHEEL, 1);

		test_wheel_event(dev, REL_HWHEEL, -5);
		test_wheel_event(dev, REL_HWHEEL, 6);
	}
}
END_TEST

START_TEST(pointer_scroll_natural_defaults)
{
	struct litest_device *dev = litest_current_device();

	ck_assert_int_ge(libinput_device_config_scroll_has_natural_scroll(dev->libinput_device), 1);
	ck_assert_int_eq(libinput_device_config_scroll_get_natural_scroll_enabled(dev->libinput_device), 0);
	ck_assert_int_eq(libinput_device_config_scroll_get_default_natural_scroll_enabled(dev->libinput_device), 0);
}
END_TEST

START_TEST(pointer_scroll_natural_enable_config)
{
	struct litest_device *dev = litest_current_device();
	enum libinput_config_status status;

	status = libinput_device_config_scroll_set_natural_scroll_enabled(dev->libinput_device, 1);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);
	ck_assert_int_eq(libinput_device_config_scroll_get_natural_scroll_enabled(dev->libinput_device), 1);

	status = libinput_device_config_scroll_set_natural_scroll_enabled(dev->libinput_device, 0);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);
	ck_assert_int_eq(libinput_device_config_scroll_get_natural_scroll_enabled(dev->libinput_device), 0);
}
END_TEST

START_TEST(pointer_scroll_natural_wheel)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;

	litest_drain_events(dev->libinput);

	libinput_device_config_scroll_set_natural_scroll_enabled(device, 1);

	/* make sure we hit at least one of the below two conditions */
	ck_assert(libevdev_has_event_code(dev->evdev, EV_REL, REL_WHEEL) ||
		  libevdev_has_event_code(dev->evdev, EV_REL, REL_HWHEEL));

	if (libevdev_has_event_code(dev->evdev, EV_REL, REL_WHEEL)) {
		test_wheel_event(dev, REL_WHEEL, -1);
		test_wheel_event(dev, REL_WHEEL, 1);

		test_wheel_event(dev, REL_WHEEL, -5);
		test_wheel_event(dev, REL_WHEEL, 6);
	}

	if (libevdev_has_event_code(dev->evdev, EV_REL, REL_HWHEEL)) {
		test_wheel_event(dev, REL_HWHEEL, -1);
		test_wheel_event(dev, REL_HWHEEL, 1);

		test_wheel_event(dev, REL_HWHEEL, -5);
		test_wheel_event(dev, REL_HWHEEL, 6);
	}
}
END_TEST

START_TEST(pointer_seat_button_count)
{
	const int num_devices = 4;
	struct litest_device *devices[num_devices];
	struct libinput *libinput;
	struct libinput_event *ev;
	struct libinput_event_pointer *tev;
	int i;
	int seat_button_count = 0;
	int expected_seat_button_count = 0;
	char device_name[255];

	libinput = litest_create_context();
	for (i = 0; i < num_devices; ++i) {
		sprintf(device_name, "litest Generic mouse (%d)", i);
		devices[i] = litest_add_device_with_overrides(libinput,
							      LITEST_MOUSE,
							      device_name,
							      NULL, NULL, NULL);
	}

	for (i = 0; i < num_devices; ++i)
		litest_button_click(devices[i], BTN_LEFT, true);

	libinput_dispatch(libinput);
	while ((ev = libinput_get_event(libinput))) {
		if (libinput_event_get_type(ev) !=
		    LIBINPUT_EVENT_POINTER_BUTTON) {
			libinput_event_destroy(ev);
			libinput_dispatch(libinput);
			continue;
		}

		tev = libinput_event_get_pointer_event(ev);
		ck_assert_notnull(tev);
		ck_assert_int_eq(libinput_event_pointer_get_button(tev),
				 BTN_LEFT);
		ck_assert_int_eq(libinput_event_pointer_get_button_state(tev),
				 LIBINPUT_BUTTON_STATE_PRESSED);

		++expected_seat_button_count;
		seat_button_count =
			libinput_event_pointer_get_seat_button_count(tev);
		ck_assert_int_eq(expected_seat_button_count, seat_button_count);

		libinput_event_destroy(ev);
		libinput_dispatch(libinput);
	}

	ck_assert_int_eq(seat_button_count, num_devices);

	for (i = 0; i < num_devices; ++i)
		litest_button_click(devices[i], BTN_LEFT, false);

	libinput_dispatch(libinput);
	while ((ev = libinput_get_event(libinput))) {
		if (libinput_event_get_type(ev) !=
		    LIBINPUT_EVENT_POINTER_BUTTON) {
			libinput_event_destroy(ev);
			libinput_dispatch(libinput);
			continue;
		}

		tev = libinput_event_get_pointer_event(ev);
		ck_assert_notnull(tev);
		ck_assert_int_eq(libinput_event_pointer_get_button(tev),
				 BTN_LEFT);
		ck_assert_int_eq(libinput_event_pointer_get_button_state(tev),
				 LIBINPUT_BUTTON_STATE_RELEASED);

		--expected_seat_button_count;
		seat_button_count =
			libinput_event_pointer_get_seat_button_count(tev);
		ck_assert_int_eq(expected_seat_button_count, seat_button_count);

		libinput_event_destroy(ev);
		libinput_dispatch(libinput);
	}

	ck_assert_int_eq(seat_button_count, 0);

	for (i = 0; i < num_devices; ++i)
		litest_delete_device(devices[i]);
	libinput_unref(libinput);
}
END_TEST

START_TEST(pointer_no_calibration)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *d = dev->libinput_device;
	enum libinput_config_status status;
	int rc;
	float calibration[6] = {0};

	rc = libinput_device_config_calibration_has_matrix(d);
	ck_assert_int_eq(rc, 0);
	rc = libinput_device_config_calibration_get_matrix(d, calibration);
	ck_assert_int_eq(rc, 0);
	rc = libinput_device_config_calibration_get_default_matrix(d,
								   calibration);
	ck_assert_int_eq(rc, 0);

	status = libinput_device_config_calibration_set_matrix(d,
							       calibration);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_UNSUPPORTED);
}
END_TEST

START_TEST(pointer_left_handed_defaults)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *d = dev->libinput_device;
	int rc;

	rc = libinput_device_config_left_handed_is_available(d);
	ck_assert_int_ne(rc, 0);

	rc = libinput_device_config_left_handed_get(d);
	ck_assert_int_eq(rc, 0);

	rc = libinput_device_config_left_handed_get_default(d);
	ck_assert_int_eq(rc, 0);
}
END_TEST

START_TEST(pointer_left_handed)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *d = dev->libinput_device;
	struct libinput *li = dev->libinput;
	enum libinput_config_status status;

	status = libinput_device_config_left_handed_set(d, 1);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);

	litest_drain_events(li);
	litest_button_click(dev, BTN_LEFT, 1);
	litest_button_click(dev, BTN_LEFT, 0);

	litest_assert_button_event(li,
				   BTN_RIGHT,
				   LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li,
				   BTN_RIGHT,
				   LIBINPUT_BUTTON_STATE_RELEASED);

	litest_button_click(dev, BTN_RIGHT, 1);
	litest_button_click(dev, BTN_RIGHT, 0);
	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_RELEASED);

	if (libevdev_has_event_code(dev->evdev,
				    EV_KEY,
				    BTN_MIDDLE)) {
		litest_button_click(dev, BTN_MIDDLE, 1);
		litest_button_click(dev, BTN_MIDDLE, 0);
		litest_assert_button_event(li,
					   BTN_MIDDLE,
					   LIBINPUT_BUTTON_STATE_PRESSED);
		litest_assert_button_event(li,
					   BTN_MIDDLE,
					   LIBINPUT_BUTTON_STATE_RELEASED);
	}
}
END_TEST

START_TEST(pointer_left_handed_during_click)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *d = dev->libinput_device;
	struct libinput *li = dev->libinput;
	enum libinput_config_status status;

	litest_drain_events(li);
	litest_button_click(dev, BTN_LEFT, 1);
	libinput_dispatch(li);

	/* Change while button is down, expect correct release event */
	status = libinput_device_config_left_handed_set(d, 1);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);

	litest_button_click(dev, BTN_LEFT, 0);

	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_RELEASED);
}
END_TEST

START_TEST(pointer_left_handed_during_click_multiple_buttons)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *d = dev->libinput_device;
	struct libinput *li = dev->libinput;
	enum libinput_config_status status;

	litest_drain_events(li);
	litest_button_click(dev, BTN_LEFT, 1);
	libinput_dispatch(li);

	status = libinput_device_config_left_handed_set(d, 1);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);

	/* No left-handed until all buttons were down */
	litest_button_click(dev, BTN_RIGHT, 1);
	litest_button_click(dev, BTN_RIGHT, 0);
	litest_button_click(dev, BTN_LEFT, 0);

	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li,
				   BTN_RIGHT,
				   LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li,
				   BTN_RIGHT,
				   LIBINPUT_BUTTON_STATE_RELEASED);
	litest_assert_button_event(li,
				   BTN_LEFT,
				   LIBINPUT_BUTTON_STATE_RELEASED);
}
END_TEST

START_TEST(pointer_scroll_button)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	/* Make left button switch to scrolling mode */
	libinput_device_config_scroll_set_method(dev->libinput_device,
					LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN);
	libinput_device_config_scroll_set_button(dev->libinput_device,
					BTN_LEFT);

	litest_drain_events(li);

	litest_button_scroll(dev, BTN_LEFT, 1, 6);
	litest_assert_scroll(li, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL, 6);
	litest_button_scroll(dev, BTN_LEFT, 1, -7);
	litest_assert_scroll(li, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL, -7);
	litest_button_scroll(dev, BTN_LEFT, 8, 1);
	litest_assert_scroll(li, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL, 8);
	litest_button_scroll(dev, BTN_LEFT, -9, 1);
	litest_assert_scroll(li, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL, -9);

	/* scroll smaller than the threshold should not generate events */
	litest_button_scroll(dev, BTN_LEFT, 1, 1);
	/* left press without movement should not generate events */
	litest_button_scroll(dev, BTN_LEFT, 0, 0);

	litest_assert_empty_queue(li);

	/* Restore default scroll behavior */
	libinput_device_config_scroll_set_method(dev->libinput_device,
		libinput_device_config_scroll_get_default_method(
			dev->libinput_device));
	libinput_device_config_scroll_set_button(dev->libinput_device,
		libinput_device_config_scroll_get_default_button(
			dev->libinput_device));
}
END_TEST

START_TEST(pointer_scroll_nowheel_defaults)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	enum libinput_config_scroll_method method;
	uint32_t button;

	method = libinput_device_config_scroll_get_method(device);
	ck_assert_int_eq(method, LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN);

	method = libinput_device_config_scroll_get_default_method(device);
	ck_assert_int_eq(method, LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN);

	button = libinput_device_config_scroll_get_button(device);
	ck_assert_int_eq(button, BTN_MIDDLE);
	button = libinput_device_config_scroll_get_default_button(device);
	ck_assert_int_eq(button, BTN_MIDDLE);
}
END_TEST

START_TEST(pointer_accel_defaults)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	enum libinput_config_status status;
	double speed;

	ck_assert(libinput_device_config_accel_is_available(device));
	ck_assert_double_eq(libinput_device_config_accel_get_default_speed(device),
			    0.0);
	ck_assert_double_eq(libinput_device_config_accel_get_speed(device),
			    0.0);

	for (speed = -2.0; speed < -1.0; speed += 0.2) {
		status = libinput_device_config_accel_set_speed(device,
								speed);
		ck_assert_int_eq(status,
				 LIBINPUT_CONFIG_STATUS_INVALID);
		ck_assert_double_eq(libinput_device_config_accel_get_speed(device),
				    0.0);
	}

	for (speed = -1.0; speed <= 1.0; speed += 0.2) {
		status = libinput_device_config_accel_set_speed(device,
								speed);
		ck_assert_int_eq(status,
				 LIBINPUT_CONFIG_STATUS_SUCCESS);
		ck_assert_double_eq(libinput_device_config_accel_get_speed(device),
				    speed);
	}

	for (speed = 1.2; speed <= -2.0; speed += 0.2) {
		status = libinput_device_config_accel_set_speed(device,
								speed);
		ck_assert_int_eq(status,
				 LIBINPUT_CONFIG_STATUS_INVALID);
		ck_assert_double_eq(libinput_device_config_accel_get_speed(device),
				    1.0);
	}

}
END_TEST

START_TEST(pointer_accel_invalid)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	enum libinput_config_status status;

	ck_assert(libinput_device_config_accel_is_available(device));

	status = libinput_device_config_accel_set_speed(device,
							NAN);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_INVALID);
	status = libinput_device_config_accel_set_speed(device,
							INFINITY);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_INVALID);
}
END_TEST

START_TEST(pointer_accel_defaults_absolute)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	enum libinput_config_status status;
	double speed;

	ck_assert(!libinput_device_config_accel_is_available(device));
	ck_assert_double_eq(libinput_device_config_accel_get_default_speed(device),
			    0.0);
	ck_assert_double_eq(libinput_device_config_accel_get_speed(device),
			    0.0);

	for (speed = -2.0; speed <= 2.0; speed += 0.2) {
		status = libinput_device_config_accel_set_speed(device,
								speed);
		if (speed >= -1.0 && speed <= 1.0)
			ck_assert_int_eq(status,
					 LIBINPUT_CONFIG_STATUS_UNSUPPORTED);
		else
			ck_assert_int_eq(status,
					 LIBINPUT_CONFIG_STATUS_INVALID);
		ck_assert_double_eq(libinput_device_config_accel_get_speed(device),
				    0.0);
	}
}
END_TEST

START_TEST(pointer_accel_defaults_absolute_relative)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;

	ck_assert(libinput_device_config_accel_is_available(device));
	ck_assert_double_eq(libinput_device_config_accel_get_default_speed(device),
			    0.0);
	ck_assert_double_eq(libinput_device_config_accel_get_speed(device),
			    0.0);
}
END_TEST

START_TEST(pointer_accel_direction_change)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	struct libinput_event_pointer *pev;
	int i;
	double delta;
	double max_accel;

	litest_drain_events(li);

	for (i = 0; i < 10; i++) {
		litest_event(dev, EV_REL, REL_X, -1);
		litest_event(dev, EV_SYN, SYN_REPORT, 0);
	}
	litest_event(dev, EV_REL, REL_X, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
	libinput_dispatch(li);

	litest_wait_for_event_of_type(li,
				      LIBINPUT_EVENT_POINTER_MOTION,
				      -1);
	event = libinput_get_event(li);
	do {
		pev = libinput_event_get_pointer_event(event);

		delta = libinput_event_pointer_get_dx(pev);
		ck_assert_double_le(delta, 0.0);
		max_accel = delta;
		libinput_event_destroy(event);
		event = libinput_get_event(li);
	} while (libinput_next_event_type(li) != LIBINPUT_EVENT_NONE);

	pev = libinput_event_get_pointer_event(event);
	delta = libinput_event_pointer_get_dx(pev);
	ck_assert_double_gt(delta, 0.0);
	ck_assert_double_lt(delta, -max_accel);
	libinput_event_destroy(event);
}
END_TEST

START_TEST(middlebutton)
{
	struct litest_device *device = litest_current_device();
	struct libinput *li = device->libinput;
	enum libinput_config_status status;
	unsigned int i;
	const int btn[][4] = {
		{ BTN_LEFT, BTN_RIGHT, BTN_LEFT, BTN_RIGHT },
		{ BTN_LEFT, BTN_RIGHT, BTN_RIGHT, BTN_LEFT },
		{ BTN_RIGHT, BTN_LEFT, BTN_LEFT, BTN_RIGHT },
		{ BTN_RIGHT, BTN_LEFT, BTN_RIGHT, BTN_LEFT },
	};

	disable_button_scrolling(device);

	status = libinput_device_config_middle_emulation_set_enabled(
					    device->libinput_device,
					    LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED);
	if (status == LIBINPUT_CONFIG_STATUS_UNSUPPORTED)
		return;

	litest_drain_events(li);

	for (i = 0; i < ARRAY_LENGTH(btn); i++) {
		litest_button_click(device, btn[i][0], true);
		litest_button_click(device, btn[i][1], true);

		litest_assert_button_event(li,
					   BTN_MIDDLE,
					   LIBINPUT_BUTTON_STATE_PRESSED);
		litest_assert_empty_queue(li);

		litest_button_click(device, btn[i][2], false);
		litest_button_click(device, btn[i][3], false);
		litest_assert_button_event(li,
					   BTN_MIDDLE,
					   LIBINPUT_BUTTON_STATE_RELEASED);
		litest_assert_empty_queue(li);
	}
}
END_TEST

START_TEST(middlebutton_timeout)
{
	struct litest_device *device = litest_current_device();
	struct libinput *li = device->libinput;
	enum libinput_config_status status;
	unsigned int button;

	disable_button_scrolling(device);

	status = libinput_device_config_middle_emulation_set_enabled(
					    device->libinput_device,
					    LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED);
	if (status == LIBINPUT_CONFIG_STATUS_UNSUPPORTED)
		return;

	for (button = BTN_LEFT; button <= BTN_RIGHT; button++) {
		litest_drain_events(li);
		litest_button_click(device, button, true);
		litest_assert_empty_queue(li);
		litest_timeout_middlebutton();

		litest_assert_button_event(li,
					   button,
					   LIBINPUT_BUTTON_STATE_PRESSED);

		litest_button_click(device, button, false);
		litest_assert_button_event(li,
					   button,
					   LIBINPUT_BUTTON_STATE_RELEASED);
		litest_assert_empty_queue(li);
	}
}
END_TEST

START_TEST(middlebutton_doubleclick)
{
	struct litest_device *device = litest_current_device();
	struct libinput *li = device->libinput;
	enum libinput_config_status status;
	unsigned int i;
	const int btn[][4] = {
		{ BTN_LEFT, BTN_RIGHT, BTN_LEFT, BTN_RIGHT },
		{ BTN_LEFT, BTN_RIGHT, BTN_RIGHT, BTN_LEFT },
		{ BTN_RIGHT, BTN_LEFT, BTN_LEFT, BTN_RIGHT },
		{ BTN_RIGHT, BTN_LEFT, BTN_RIGHT, BTN_LEFT },
	};

	disable_button_scrolling(device);

	status = libinput_device_config_middle_emulation_set_enabled(
				    device->libinput_device,
				    LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED);
	if (status == LIBINPUT_CONFIG_STATUS_UNSUPPORTED)
		return;

	litest_drain_events(li);

	for (i = 0; i < ARRAY_LENGTH(btn); i++) {
		litest_button_click(device, btn[i][0], true);
		litest_button_click(device, btn[i][1], true);
		litest_assert_button_event(li,
					   BTN_MIDDLE,
					   LIBINPUT_BUTTON_STATE_PRESSED);
		litest_assert_empty_queue(li);

		litest_button_click(device, btn[i][2], false);
		litest_button_click(device, btn[i][2], true);
		litest_assert_button_event(li,
					   BTN_MIDDLE,
					   LIBINPUT_BUTTON_STATE_RELEASED);
		litest_assert_button_event(li,
					   BTN_MIDDLE,
					   LIBINPUT_BUTTON_STATE_PRESSED);
		litest_button_click(device, btn[i][3], false);

		litest_assert_button_event(li,
					   BTN_MIDDLE,
					   LIBINPUT_BUTTON_STATE_RELEASED);
		litest_assert_empty_queue(li);
	}
}
END_TEST

START_TEST(middlebutton_middleclick)
{
	struct litest_device *device = litest_current_device();
	struct libinput *li = device->libinput;
	enum libinput_config_status status;
	unsigned int button;

	disable_button_scrolling(device);

	if (!libevdev_has_event_code(device->evdev, EV_KEY, BTN_MIDDLE))
		return;

	status = libinput_device_config_middle_emulation_set_enabled(
					    device->libinput_device,
					    LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED);
	if (status == LIBINPUT_CONFIG_STATUS_UNSUPPORTED)
		return;

	/* one button down, then middle -> release buttons */
	for (button = BTN_LEFT; button <= BTN_RIGHT; button++) {
		/* release button before middle */
		litest_drain_events(li);
		litest_button_click(device, button, true);
		litest_button_click(device, BTN_MIDDLE, true);
		litest_assert_button_event(li,
					   button,
					   LIBINPUT_BUTTON_STATE_PRESSED);
		litest_assert_button_event(li,
					   BTN_MIDDLE,
					   LIBINPUT_BUTTON_STATE_PRESSED);
		litest_assert_empty_queue(li);
		litest_button_click(device, button, false);
		litest_assert_button_event(li,
					   button,
					   LIBINPUT_BUTTON_STATE_RELEASED);
		litest_button_click(device, BTN_MIDDLE, false);
		litest_assert_button_event(li,
					   BTN_MIDDLE,
					   LIBINPUT_BUTTON_STATE_RELEASED);
		litest_assert_empty_queue(li);

		/* release middle before button */
		litest_button_click(device, button, true);
		litest_button_click(device, BTN_MIDDLE, true);
		litest_assert_button_event(li,
					   button,
					   LIBINPUT_BUTTON_STATE_PRESSED);
		litest_assert_button_event(li,
					   BTN_MIDDLE,
					   LIBINPUT_BUTTON_STATE_PRESSED);
		litest_assert_empty_queue(li);
		litest_button_click(device, BTN_MIDDLE, false);
		litest_assert_button_event(li,
					   BTN_MIDDLE,
					   LIBINPUT_BUTTON_STATE_RELEASED);
		litest_button_click(device, button, false);
		litest_assert_button_event(li,
					   button,
					   LIBINPUT_BUTTON_STATE_RELEASED);
		litest_assert_empty_queue(li);
	}
}
END_TEST

START_TEST(middlebutton_middleclick_during)
{
	struct litest_device *device = litest_current_device();
	struct libinput *li = device->libinput;
	enum libinput_config_status status;
	unsigned int button;

	disable_button_scrolling(device);

	if (!libevdev_has_event_code(device->evdev, EV_KEY, BTN_MIDDLE))
		return;

	status = libinput_device_config_middle_emulation_set_enabled(
					    device->libinput_device,
					    LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED);
	if (status == LIBINPUT_CONFIG_STATUS_UNSUPPORTED)
		return;

	litest_drain_events(li);

	/* trigger emulation, then real middle */
	for (button = BTN_LEFT; button <= BTN_RIGHT; button++) {
		litest_button_click(device, BTN_LEFT, true);
		litest_button_click(device, BTN_RIGHT, true);

		litest_assert_button_event(li,
					   BTN_MIDDLE,
					   LIBINPUT_BUTTON_STATE_PRESSED);

		litest_button_click(device, BTN_MIDDLE, true);
		litest_assert_button_event(li,
					   BTN_MIDDLE,
					   LIBINPUT_BUTTON_STATE_RELEASED);
		litest_assert_button_event(li,
					   BTN_MIDDLE,
					   LIBINPUT_BUTTON_STATE_PRESSED);

		litest_assert_empty_queue(li);

		/* middle still down, release left/right */
		litest_button_click(device, button, false);
		litest_assert_empty_queue(li);
		litest_button_click(device, button, true);
		litest_assert_button_event(li,
					   button,
					   LIBINPUT_BUTTON_STATE_PRESSED);
		litest_assert_empty_queue(li);

		/* release both */
		litest_button_click(device, BTN_LEFT, false);
		litest_button_click(device, BTN_RIGHT, false);
		litest_assert_button_event(li,
					   button,
					   LIBINPUT_BUTTON_STATE_RELEASED);
		litest_assert_empty_queue(li);

		litest_button_click(device, BTN_MIDDLE, false);
		litest_assert_button_event(li,
					   BTN_MIDDLE,
					   LIBINPUT_BUTTON_STATE_RELEASED);
		litest_assert_empty_queue(li);
	}
}
END_TEST

START_TEST(middlebutton_default_enabled)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	enum libinput_config_status status;
	int available;
	enum libinput_config_middle_emulation_state deflt, state;

	available = libinput_device_config_middle_emulation_is_available(device);
	ck_assert(available);

	if (libevdev_has_event_code(dev->evdev, EV_KEY, BTN_MIDDLE))
		deflt = LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED;
	else
		deflt = LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED;

	state = libinput_device_config_middle_emulation_get_enabled(device);
	ck_assert_int_eq(state, deflt);

	state = libinput_device_config_middle_emulation_get_default_enabled(
					    device);
	ck_assert_int_eq(state, deflt);

	status = libinput_device_config_middle_emulation_set_enabled(device,
					    LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);

	status = libinput_device_config_middle_emulation_set_enabled(device,
					    LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);

	status = libinput_device_config_middle_emulation_set_enabled(device, 3);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_INVALID);
}
END_TEST

START_TEST(middlebutton_default_clickpad)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	enum libinput_config_status status;
	enum libinput_config_middle_emulation_state state;
	int available;

	available = libinput_device_config_middle_emulation_is_available(device);
	ck_assert(!available);

	state = libinput_device_config_middle_emulation_get_enabled(device);
	ck_assert_int_eq(state, LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED);
	state = libinput_device_config_middle_emulation_get_default_enabled(
					    device);
	ck_assert_int_eq(state, LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED);

	status = libinput_device_config_middle_emulation_set_enabled(device,
					    LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_UNSUPPORTED);

	status = libinput_device_config_middle_emulation_set_enabled(device,
					    LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);

	status = libinput_device_config_middle_emulation_set_enabled(device, 3);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_INVALID);
}
END_TEST

START_TEST(middlebutton_default_touchpad)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	enum libinput_config_middle_emulation_state state;
	int available;
	const char *name = libinput_device_get_name(dev->libinput_device);

	if (streq(name, "litest AlpsPS/2 ALPS GlidePoint") ||
	    streq(name, "litest AlpsPS/2 ALPS DualPoint TouchPad"))
	    return;

	available = libinput_device_config_middle_emulation_is_available(device);
	ck_assert(!available);

	if (libevdev_has_event_code(dev->evdev, EV_KEY, BTN_MIDDLE))
		return;

	state = libinput_device_config_middle_emulation_get_enabled(
					    device);
	ck_assert_int_eq(state, LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED);
	state = libinput_device_config_middle_emulation_get_default_enabled(
					    device);
	ck_assert_int_eq(state, LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED);
}
END_TEST

START_TEST(middlebutton_default_alps)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	enum libinput_config_middle_emulation_state state;
	int available;

	available = libinput_device_config_middle_emulation_is_available(device);
	ck_assert(available);

	state = libinput_device_config_middle_emulation_get_enabled(
					    device);
	ck_assert_int_eq(state, LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED);
	state = libinput_device_config_middle_emulation_get_default_enabled(
					    device);
	ck_assert_int_eq(state, LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED);
}
END_TEST

START_TEST(middlebutton_default_disabled)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	enum libinput_config_middle_emulation_state state;
	enum libinput_config_status status;
	int available;

	available = libinput_device_config_middle_emulation_is_available(device);
	ck_assert(!available);
	state = libinput_device_config_middle_emulation_get_enabled(device);
	ck_assert_int_eq(state, LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED);
	state = libinput_device_config_middle_emulation_get_default_enabled(
								    device);
	ck_assert_int_eq(state, LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED);
	status = libinput_device_config_middle_emulation_set_enabled(device,
				     LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);
	status = libinput_device_config_middle_emulation_set_enabled(device,
				     LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED);
	ck_assert_int_eq(status, LIBINPUT_CONFIG_STATUS_UNSUPPORTED);
}
END_TEST

void
litest_setup_tests(void)
{
	struct range axis_range = {ABS_X, ABS_Y + 1};
	struct range compass = {0, 7}; /* cardinal directions */

	litest_add("pointer:motion", pointer_motion_relative, LITEST_RELATIVE, LITEST_ANY);
	litest_add_for_device("pointer:motion", pointer_motion_relative_zero, LITEST_MOUSE);
	litest_add_ranged("pointer:motion", pointer_motion_relative_min_decel, LITEST_RELATIVE, LITEST_ANY, &compass);
	litest_add("pointer:motion", pointer_motion_absolute, LITEST_ABSOLUTE, LITEST_ANY);
	litest_add("pointer:motion", pointer_motion_unaccel, LITEST_RELATIVE, LITEST_ANY);
	litest_add("pointer:button", pointer_button, LITEST_BUTTON, LITEST_CLICKPAD);
	litest_add_no_device("pointer:button", pointer_button_auto_release);
	litest_add_no_device("pointer:button", pointer_seat_button_count);
	litest_add("pointer:scroll", pointer_scroll_wheel, LITEST_WHEEL, LITEST_ANY);
	litest_add("pointer:scroll", pointer_scroll_button, LITEST_RELATIVE|LITEST_BUTTON, LITEST_ANY);
	litest_add("pointer:scroll", pointer_scroll_nowheel_defaults, LITEST_RELATIVE|LITEST_BUTTON, LITEST_WHEEL);
	litest_add("pointer:scroll", pointer_scroll_natural_defaults, LITEST_WHEEL, LITEST_ANY);
	litest_add("pointer:scroll", pointer_scroll_natural_enable_config, LITEST_WHEEL, LITEST_ANY);
	litest_add("pointer:scroll", pointer_scroll_natural_wheel, LITEST_WHEEL, LITEST_ANY);

	litest_add("pointer:calibration", pointer_no_calibration, LITEST_ANY, LITEST_TOUCH|LITEST_SINGLE_TOUCH|LITEST_ABSOLUTE|LITEST_PROTOCOL_A);

									/* tests touchpads too */
	litest_add("pointer:left-handed", pointer_left_handed_defaults, LITEST_BUTTON, LITEST_ANY);
	litest_add("pointer:left-handed", pointer_left_handed, LITEST_RELATIVE|LITEST_BUTTON, LITEST_ANY);
	litest_add("pointer:left-handed", pointer_left_handed_during_click, LITEST_RELATIVE|LITEST_BUTTON, LITEST_ANY);
	litest_add("pointer:left-handed", pointer_left_handed_during_click_multiple_buttons, LITEST_RELATIVE|LITEST_BUTTON, LITEST_ANY);

	litest_add("pointer:accel", pointer_accel_defaults, LITEST_RELATIVE, LITEST_ANY);
	litest_add("pointer:accel", pointer_accel_invalid, LITEST_RELATIVE, LITEST_ANY);
	litest_add("pointer:accel", pointer_accel_defaults_absolute, LITEST_ABSOLUTE, LITEST_RELATIVE);
	litest_add("pointer:accel", pointer_accel_defaults_absolute_relative, LITEST_ABSOLUTE|LITEST_RELATIVE, LITEST_ANY);
	litest_add("pointer:accel", pointer_accel_direction_change, LITEST_RELATIVE, LITEST_ANY);

	litest_add("pointer:middlebutton", middlebutton, LITEST_BUTTON, LITEST_ANY);
	litest_add("pointer:middlebutton", middlebutton_timeout, LITEST_BUTTON, LITEST_ANY);
	litest_add("pointer:middlebutton", middlebutton_doubleclick, LITEST_BUTTON, LITEST_ANY);
	litest_add("pointer:middlebutton", middlebutton_middleclick, LITEST_BUTTON, LITEST_ANY);
	litest_add("pointer:middlebutton", middlebutton_middleclick_during, LITEST_BUTTON, LITEST_ANY);
	litest_add("pointer:middlebutton", middlebutton_default_enabled, LITEST_BUTTON, LITEST_TOUCHPAD|LITEST_POINTINGSTICK);
	litest_add("pointer:middlebutton", middlebutton_default_clickpad, LITEST_CLICKPAD, LITEST_ANY);
	litest_add("pointer:middlebutton", middlebutton_default_touchpad, LITEST_TOUCHPAD, LITEST_CLICKPAD);
	litest_add("pointer:middlebutton", middlebutton_default_disabled, LITEST_ANY, LITEST_BUTTON);
	litest_add_for_device("pointer:middlebutton", middlebutton_default_alps, LITEST_ALPS_SEMI_MT);

	litest_add_ranged("pointer:state", pointer_absolute_initial_state, LITEST_ABSOLUTE, LITEST_ANY, &axis_range);
}
