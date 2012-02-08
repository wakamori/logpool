#include <stdio.h>
#include "logpool.h"
#include "logpool_internal.h"
#include "lpstring.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct flog {
    char *buf;
    FILE *fp;
    char base[1];
} flog_t;

void *logpool_FILE2_init(logctx ctx, struct logpool_param *p)
{
    struct logpool_param_file *args = cast(struct logpool_param_file *, p);
    char *fname = cast(char *, args->fname);
    flog_t *fl  = cast(flog_t *, logpool_string_init(ctx, p));
    fl->fp = fopen(fname, "w");
    return cast(void *, fl);
}

void logpool_FILE2_close(logctx ctx)
{
    flog_t *fl = cast(flog_t *, ctx->connection);
    fclose(fl->fp);
    logpool_string_close(ctx);
}

void logpool_FILE2_flush(logctx ctx, void **args __UNUSED__)
{
    flog_t *fl = cast(flog_t *, ctx->connection);
    logpool_string_flush(ctx);
    assert(fl->buf[-1] == '\0');
    fl->buf[-1] = '\n';
    fl->buf[ 0] = '\0';
    fwrite(fl->base, fl->buf - fl->base, 1, fl->fp);
    logpool_string_reset(ctx);
}

struct logapi FILE2_API = {
    logpool_string_null,
    logpool_string_bool,
    logpool_string_int,
    logpool_string_hex,
    logpool_string_float,
    logpool_string_char,
    logpool_string_string,
    logpool_string_raw,
    logpool_string_delim,
    logpool_FILE2_flush,
    logpool_FILE2_init,
    logpool_FILE2_close,
};

#ifdef __cplusplus
}
#endif
