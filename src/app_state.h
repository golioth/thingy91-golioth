/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_STATE_H__
#define __APP_STATE_H__

#include <golioth/client.h>

int app_state_observe(struct golioth_client *state_client);
int app_state_counter_change(void);

#endif /* __APP_STATE_H__ */
