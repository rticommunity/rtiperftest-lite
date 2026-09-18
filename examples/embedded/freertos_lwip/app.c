/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

#include "perftest_lite.h"

int perftest_lite_example_run(void)
{
    PerftestLiteInputArgs arguments;
    PerftestLiteSizeResult result_storage[1];
    PerftestLiteResults results;

    perftest_lite_input_args_init(&arguments);
#if defined(PERFTEST_LITE_BUILD_PUB) && PERFTEST_LITE_BUILD_PUB
    arguments.role = PERFTEST_LITE_ROLE_PUBLISHER;
#elif defined(PERFTEST_LITE_BUILD_SUB) && PERFTEST_LITE_BUILD_SUB
    arguments.role = PERFTEST_LITE_ROLE_SUBSCRIBER;
#else
#error "Build exactly one Perftest Lite role"
#endif

    perftest_lite_results_init(&results, result_storage, 1);
    return (int)perftest_lite_main(&arguments, &results);
}
