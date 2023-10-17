/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_STATE_H__
#define __APP_STATE_H__

#include <net/golioth/system_client.h>

#define APP_STATE_DESIRED_ENDP "desired"
#define APP_STATE_ACTUAL_ENDP  "state"
#define APP_STATE_BUTTON_ENDP  "button"

void app_state_init(struct golioth_client *state_client);
int app_state_observe(void);
int app_state_update_actual(void);

void state_counter_change(void);

#endif /* __APP_STATE_H__ */
