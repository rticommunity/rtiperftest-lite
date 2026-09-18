/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

/*
 * Top-level dispatch: perftest_lite_main / perftest_lite_main_cli.
 */
#include "perftest_lite.h"
#include "../render/perftest_lite_render.h"
#include "../type/type_interface.h"

#include <stdio.h>
#include <stdlib.h>

PerftestLiteRetcode perftest_lite_main(const PerftestLiteInputArgs *args,
                                       PerftestLiteResults *results)
{
    const PerftestLiteTypeInterface *type_iface;

    if (!args) return PERFTEST_LITE_ERR_BAD_ARGS;
    type_iface = perftest_lite_type_get();
    if (!type_iface || !type_iface->validate_input) {
        PERFTEST_LITE_PRINT("[err] selected type does not support input validation\n");
        return PERFTEST_LITE_ERR_BAD_ARGS;
    }
    if (type_iface->validate_input(args) != 0) {
        if (type_iface->print_input_error) {
            type_iface->print_input_error();
        }
        return PERFTEST_LITE_ERR_BAD_ARGS;
    }

    if (args->print_config) {
        perftest_lite_print_input_args(args);
    }
    if (args->role == PERFTEST_LITE_ROLE_PUBLISHER) {
#if defined(PERFTEST_LITE_BUILD_PUB) && PERFTEST_LITE_BUILD_PUB
        return perftest_lite_publisher(args, results);
#else
        PERFTEST_LITE_PRINT("[err] this binary was built without publisher support\n");
        return PERFTEST_LITE_ERR_BAD_ARGS;
#endif
    }
#if defined(PERFTEST_LITE_BUILD_SUB) && PERFTEST_LITE_BUILD_SUB
    return perftest_lite_subscriber(args, results);
#else
    PERFTEST_LITE_PRINT("[err] this binary was built without subscriber support\n");
    return PERFTEST_LITE_ERR_BAD_ARGS;
#endif
}

int perftest_lite_main_cli(int argc, char **argv)
{
    PerftestLiteInputArgs args;
    int rc = perftest_lite_parse_arguments(argc, argv, &args);
    if (rc == PERFTEST_LITE_PARSE_HELP) {
        perftest_lite_free_args(&args);
        return PERFTEST_LITE_EXIT_SUCCESS;     /* help already printed */
    }
    if (rc == PERFTEST_LITE_PARSE_ERROR) {
        perftest_lite_free_args(&args);
        return PERFTEST_LITE_EXIT_FAILURE;
    }

    PerftestLiteSizeResult storage[1];
    PerftestLiteResults results;
    perftest_lite_results_init(&results, storage,
                               1);

    PerftestLiteRetcode r = perftest_lite_main(&args, &results);

    /* Renderer is selected at compile time. Exactly one of the two
     * PERFTEST_LITE_HAS_RENDER_* macros is required (CMake enforces it). */
    const PerftestLiteResultRenderer *renderer =
#if defined(PERFTEST_LITE_HAS_RENDER_CSV)
        perftest_lite_renderer_csv();
#elif defined(PERFTEST_LITE_HAS_RENDER_TABLE)
        perftest_lite_renderer_table();
#else
#       error "No renderer enabled"
#endif
    perftest_lite_render_results(renderer, NULL, args.role, &args, &results);

    perftest_lite_free_args(&args);
    return (r == PERFTEST_LITE_OK) ? PERFTEST_LITE_EXIT_SUCCESS : (int)(-r);
}
