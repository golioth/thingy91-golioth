/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __JSON_HELPER_H_
#define __JSON_HELPER_H_

#include <zephyr/data/json.h>

struct app_state {
	int32_t counter_up;
	int32_t counter_down;
};

static const struct json_obj_descr app_state_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct app_state, counter_up, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct app_state, counter_down, JSON_TOK_NUMBER)};

#endif
