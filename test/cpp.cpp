/* string api test */
#include "logpool.h"
static struct logpool_param_string STRING_API_PARAM = {LOG_NOTICE, 8, 1024};
#define LOGAPI_PARAM cast(logpool_param_t *, &STRING_API_PARAM)
#define LOGAPI STRING_API
#include "test_main.c"
