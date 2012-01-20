#include "logpool.h"
#include "config.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

static struct keyapi *KeyAPI = NULL;

void logctx_init(logctx ctx, struct logapi *api, void **param)
{
    struct logCtx *lctx = cast(struct logCtx *, ctx);
    lctx->formatter   = api;
    lctx->connection  = api->fn_init(ctx, param);
    lctx->logkey.k.seq = 0;
    lctx->logfmt_size  = 0;
}

void logctx_format_flush(logctx ctx)
{
    struct logfmt *fmt = cast(struct logCtx *, ctx)->fmt;
    size_t i, size = ctx->logfmt_size;
    ctx->fn_key(ctx, ctx->logkey.v.u, ctx->logkey.k.seq, ctx->logkey.siz);
    ctx->formatter->fn_delim(ctx);
    if (size) {
        fmt->fn(ctx, fmt->k.key, fmt->v.u, fmt->siz);
        fmt++;
        for (i = 1; i < size; ++i, ++fmt) {
            ctx->formatter->fn_delim(ctx);
            fmt->fn(ctx, fmt->k.key, fmt->v.u, fmt->siz);
        }
        cast(struct logCtx *, ctx)->logfmt_size = 0;
    }
    ++(cast(struct logCtx *, ctx)->logkey.k.seq);
}

void logctx_append_fmtdata(logctx ctx, const char *key, uint64_t v, logFn f, sizeinfo_t siz)
{
    struct logCtx *lctx = cast(struct logCtx *, ctx);
    assert(lctx->logfmt_size < LOGFMT_MAX_SIZE);
    lctx->fmt[lctx->logfmt_size].fn    = f;
    lctx->fmt[lctx->logfmt_size].k.key = key;
    lctx->fmt[lctx->logfmt_size].v.u   = v;
    lctx->fmt[lctx->logfmt_size].siz   = siz;
    ++lctx->logfmt_size;
}

void logctx_init_logkey(logctx ctx, uint64_t v, sizeinfo_t siz)
{
    struct logCtx *lctx = cast(struct logCtx *, ctx);
    lctx->logkey.v.u = v;
    lctx->logkey.siz = siz;
    lctx->logfmt_size = 0;
}

ltrace_t *ltrace_open(ltrace_t *parent, struct logapi *api, void **param)
{
    struct ltrace *l = cast(struct ltrace *, malloc(sizeof(*l)));
    logctx_init(cast(logctx, l), api, param);
    l->parent = parent;
    l->ctx.fn_key = KeyAPI->str;
    return cast(ltrace_t*, l);
}

void ltrace_close(ltrace_t *p)
{
    struct ltrace *l = cast(struct ltrace *, p);
    free(l);
}

static uint64_t hash(uint64_t h, const char *p, size_t len)
{
    size_t i;
    for(i = 0; i < len; i++) {
        h = p[i] + 31 * h;
    }
    return h;
}

lstate_t *lstate_open(const char *state_name, struct logapi *api, void **param)
{
    struct lstate *l = cast(struct lstate *, malloc(sizeof(*l)));
    l->state = hash(0x11029, state_name, strlen(state_name));
    logctx_init(cast(logctx, l), api, param);
    l->ctx.fn_key = KeyAPI->hex;
    return cast(lstate_t*, l);
}

void lstate_close(lstate_t *p)
{
    struct lstate *l = cast(struct lstate *, p);
    free(l);
}

#ifdef LOGPOOL_USE_LLVM
extern struct keyapi *logpool_llvm_api_init(void);
#endif
extern struct keyapi *logpool_string_api_init(void);
void logpool_init(enum LOGPOOL_EXEC_MODE mode)
{
    if (mode == LOGPOOL_JIT) {
#ifdef LOGPOOL_USE_LLVM
        KeyAPI = logpool_llvm_api_init();
#else
        assert(0 && "please enable USE_LLVM flag");
#endif
    } else {
        KeyAPI = logpool_string_api_init();
    }
}

#ifdef __cplusplus
}
#endif
