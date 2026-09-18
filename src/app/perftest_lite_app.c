/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

/*
 * Application entry point.
 *
 * Always defines `emain(argc, argv)` so it can be reused by:
 *   - The Linux/host build: a thin `main()` shim below forwards to emain.
 *   - The Forge / substrate build: the platform-provided `main()` (e.g. the
 *     embedded startup task) calls `emain()` directly.
 */
#include <stddef.h>

#include "../core/perftest_lite.h"

#ifndef PERFTEST_LITE_USE_FORGE
int emain(int argc, char **argv)
{
    return perftest_lite_main_cli(argc, argv);
}
#else
int emain(int argc __attribute__((unused)), char **argv __attribute__((unused)))
{
    return perftest_lite_main_cli(0, NULL);
}
#endif

/* Host main() shim. Compiled out on substrate builds, where the platform
 * provides main() and calls emain() itself (e.g. from the embedded startup task). */
#ifndef PERFTEST_LITE_USE_FORGE
int main(int argc, char **argv)
{
    return emain(argc, argv);
}
#endif
