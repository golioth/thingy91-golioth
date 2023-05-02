/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_SETTINGS_H__
#define __APP_SETTINGS_H__

int32_t get_loop_delay_s(void);
void app_settings_init(struct golioth_client *state_client);
void app_settings_observe(void);
int app_register_settings(struct golioth_client *settings_client);
// int turn_on_and_update_all_led_pwms(void);
// int blink_all_leds(int);
int app_led_pwm_init(void);
int all_leds_on(void);
int all_leds_off(void);

#endif /* __APP_SETTINGS_H__ */
