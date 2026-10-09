/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_POSITIONS_H
#define MAINUI_POSITIONS_H
#include "catalog/catalog.h"
/* Stock romwinidx.json geometry, keyed by ROM directory. Save only at lifecycle
 * boundaries; Search result geometry must never be passed to this API. */
bool mainui_positions_save(const MainUICatalog *, const MainUIViewport *);
void mainui_positions_restore(const MainUICatalog *, MainUIViewport *, int rows);
#endif
