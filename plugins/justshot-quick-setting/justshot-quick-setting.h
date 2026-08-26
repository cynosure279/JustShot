/*
 * Copyright (C) 2026 The JustShot Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "quick-setting.h"

G_BEGIN_DECLS

#define JUSTSHOT_TYPE_QUICK_SETTING (justshot_quick_setting_get_type ())
G_DECLARE_FINAL_TYPE (JustshotQuickSetting, justshot_quick_setting, JUSTSHOT, QUICK_SETTING, PhoshQuickSetting)

G_END_DECLS
