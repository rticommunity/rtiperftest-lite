/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

/*
 * Renderer dispatch helper. Iterates results and invokes the renderer's
 * callbacks in order. Renderer implementations live in sibling files
 * (render_table.c, render_csv.c, ...).
 */
#include "perftest_lite_render.h"

void perftest_lite_render_results(const PerftestLiteResultRenderer *r,
                                  void *user,
                                  PerftestLiteRole role,
                                  const PerftestLiteInputArgs *args,
                                  const PerftestLiteResults *results)
{
    if (!r || !results) return;
    if (r->begin) r->begin(user, role, args);
    if (r->row) {
        for (int i = 0; i < results->size_count; ++i) {
            r->row(user, &results->sizes[i]);
        }
    }
    if (r->end) r->end(user);
}
