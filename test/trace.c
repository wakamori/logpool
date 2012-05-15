/*trace api test*/
#include "logpool.h"
#include <stdlib.h>
#include <stdio.h>
static struct logpool_param_trace TRACE_API_PARAM = {
    8,
    1024,
    "127.0.0.1", 14801
};
#define LOGAPI_PARAM cast(logpool_param_t *, &TRACE_API_PARAM)
#define LOGAPI_INIT_FLAG (LOGPOOL_TRACE)
#define LOGPOOL_TEST_COUNT get_count()
#define LOGAPI TRACE_API
static int get_count(void)
{
    char *env = getenv("LOGPOOL_TESTCASE_SIZE");
    env = (env) ? env : "100";
    fprintf(stderr, "%s:%d test_size=%s\n", __FILE__, __LINE__, env);
    return strtol(env, NULL, 10);
}
#include "test_main.c"

