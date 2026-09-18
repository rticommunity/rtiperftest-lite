/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

/*
 * Perftest Lite - result renderer interface.
 *
 * Decouples the engine (which only fills PerftestLiteResults) from the
 * sink that formats those results. Built-ins write through
 * PERFTEST_LITE_PRINT; ports can write their own renderer that pushes
 * to UART, syslog, a ring buffer, etc.
 *
 * Selection is compile-time: the chosen renderer's source file is added
 * by CMake (PERFTEST_LITE_RENDER_TABLE / _CSV) and exposes its factory
 * here. Exactly one renderer is built per binary.
 */
#ifndef PERFTEST_LITE_RENDER_H
#define PERFTEST_LITE_RENDER_H

#include "../core/perftest_lite.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void (*begin)(void *user, PerftestLiteRole role,
                  const PerftestLiteInputArgs *args);
    void (*row)  (void *user, const PerftestLiteSizeResult *r);
    void (*end)  (void *user);
} PerftestLiteResultRenderer;

void perftest_lite_render_results(const PerftestLiteResultRenderer *r,
                                  void *user,
                                  PerftestLiteRole role,
                                  const PerftestLiteInputArgs *args,
                                  const PerftestLiteResults *results);

/* Built-in renderer factories (only the one built into this binary is
 * defined; the other resolves to a link error if referenced). */
const PerftestLiteResultRenderer *perftest_lite_renderer_table(void);
const PerftestLiteResultRenderer *perftest_lite_renderer_csv(void);

#ifdef __cplusplus
}
#endif

#endif /* PERFTEST_LITE_RENDER_H */
