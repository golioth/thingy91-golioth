/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_WORK_H__
#define __APP_WORK_H__

void app_work_init(struct golioth_client *work_client);
void app_work_sensor_read(void);
int app_buzzer_init(void);
void play_funkytown_once(void);
void play_beep_once(void);

/**
 * Each Ostentus slide needs a unique key. You may add additional slides by
 * inserting elements with the name of your choice to this enum.
 */
typedef enum {
	UP_COUNTER,
	DN_COUNTER
} slide_key;


#define BUZZER_MAX_FREQ 2500
#define BUZZER_MIN_FREQ 75


#endif /* __APP_WORK_H__ */
