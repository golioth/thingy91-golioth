/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_SETTINGS_H__
#define __APP_SETTINGS_H__

#include <stdint.h>
#include <golioth/client.h>

int32_t get_loop_delay_s(void);
void app_settings_register(struct golioth_client *client);
int app_led_pwm_init(void);
void all_leds_on(void);
void all_leds_off(void);

#endif /* __APP_SETTINGS_H__ */
