/*
 * Copyright (c) 2010, 2011, Jiale "calio" Zhi <vipcalio@gmail.com>.
 * Copyright (c) 2010-2016, Yichun "agentzh" Zhang <agentzh@gmail.com>,
 *     CloudFlare Inc.
 * Copyright (c) 2026, Jochen Neumeister <joneum@FreeBSD.org>.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * See the LICENSE file in the root of this distribution.
 */


#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"


#include <ndk.h>
#include <nginx.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


#define form_urlencoded_type "application/x-www-form-urlencoded"
#define form_urlencoded_type_len (sizeof(form_urlencoded_type) - 1)

#define form_multipart_type "multipart/form-data"
#define form_multipart_type_len (sizeof(form_multipart_type) - 1)

/* RFC 2046 gives the boundary 1 to 70 characters */
#define form_boundary_max 70


typedef struct {
    unsigned        used;  /* :1 */
} ngx_http_form_input_main_conf_t;


typedef struct {
    ngx_flag_t      used;
    ngx_flag_t      multipart;
} ngx_http_form_input_loc_conf_t;


typedef struct {
    unsigned          done:1;
    unsigned          waiting_more_body:1;
    unsigned          body_read:1;
    unsigned          multipart:1;
    ngx_str_t         body;
    ngx_str_t         delimiter;   /* CRLF, two dashes, the boundary */
} ngx_http_form_input_ctx_t;


/* walks the parts of a multipart body, one call per part */
typedef struct {
    u_char           *pos;
    u_char           *last;
    ngx_str_t         delimiter;
    unsigned          started:1;
    unsigned          done:1;
} ngx_http_form_input_part_t;


static ngx_int_t ngx_http_set_form_input(ngx_http_request_t *r, ngx_str_t *res,
    ngx_http_variable_value_t *v);
static ngx_int_t ngx_http_form_input_empty_array(ngx_http_request_t *r,
    ngx_str_t *res);
static ngx_flag_t ngx_http_form_input_have_array_var(ngx_conf_t *cf);
static char *ngx_http_set_form_input_conf_handler(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);
static void *ngx_http_form_input_create_main_conf(ngx_conf_t *cf);
static void *ngx_http_form_input_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_form_input_merge_loc_conf(ngx_conf_t *cf, void *parent,
    void *child);
static ngx_int_t ngx_http_form_input_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_form_input_handler(ngx_http_request_t *r);
static void ngx_http_form_input_post_read(ngx_http_request_t *r);
static ngx_int_t ngx_http_form_input_arg(ngx_http_request_t *r, u_char *name,
    size_t len, ngx_str_t *value, ngx_flag_t multi);
static ngx_int_t ngx_http_form_input_read_body(ngx_http_request_t *r,
    ngx_str_t *body);
static ngx_flag_t ngx_http_form_input_is_type(ngx_str_t *value,
    const char *type, size_t len);
static ngx_int_t ngx_http_form_input_param(ngx_str_t *header,
    const char *name, size_t len, ngx_str_t *value);
static ngx_int_t ngx_http_form_input_boundary(ngx_str_t *type,
    ngx_str_t *boundary);
static u_char *ngx_http_form_input_find(u_char *p, u_char *last,
    u_char *needle, size_t n);
static ngx_flag_t ngx_http_form_input_plain_encoding(ngx_str_t *v);
static ngx_int_t ngx_http_form_input_disposition(u_char *p, u_char *last,
    ngx_str_t *value);
static ngx_flag_t ngx_http_form_input_delimiter_ends(u_char *p, u_char *last);
static u_char *ngx_http_form_input_delimiter(
    ngx_http_form_input_part_t *mp, u_char *p);
static ngx_int_t ngx_http_form_input_part(ngx_http_form_input_part_t *mp,
    ngx_str_t *name, ngx_str_t *content);
static ngx_int_t ngx_http_form_input_multipart(ngx_http_request_t *r,
    ngx_http_form_input_ctx_t *ctx, u_char *arg_name, size_t arg_len,
    ngx_str_t *value, ngx_flag_t multi, ngx_array_t *array);


static ngx_command_t ngx_http_form_input_commands[] = {

    { ngx_string("set_form_input"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_set_form_input_conf_handler,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("set_form_input_multi"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_set_form_input_conf_handler,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("form_input_multipart"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_form_input_loc_conf_t, multipart),
      NULL },

      ngx_null_command
};


static ngx_http_module_t ngx_http_form_input_module_ctx = {
    NULL,                                   /* preconfiguration */
    ngx_http_form_input_init,               /* postconfiguration */

    ngx_http_form_input_create_main_conf,   /* create main configuration */
    NULL,                                   /* init main configuration */

    NULL,                                   /* create server configuration */
    NULL,                                   /* merge server configuration */

    ngx_http_form_input_create_loc_conf,    /* create location configuration */
    ngx_http_form_input_merge_loc_conf      /* merge location configuration */
};


ngx_module_t ngx_http_form_input_module = {
    NGX_MODULE_V1,
    &ngx_http_form_input_module_ctx,        /* module context */
    ngx_http_form_input_commands,           /* module directives */
    NGX_HTTP_MODULE,                        /* module type */
    NULL,                                   /* init master */
    NULL,                                   /* init module */
    NULL,                                   /* init process */
    NULL,                                   /* init thread */
    NULL,                                   /* exit thread */
    NULL,                                   /* exit precess */
    NULL,                                   /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_http_set_form_input(ngx_http_request_t *r, ngx_str_t *res,
    ngx_http_variable_value_t *v)
{
    ngx_http_form_input_ctx_t           *ctx;
    ngx_int_t                            rc;

    dd_enter();

    dd("set default return value");
    ngx_str_set(res, "");

    if (r->done) {
        dd("request done");
        return NGX_OK;
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_form_input_module);

    if (ctx == NULL) {
        dd("ndk handler:null ctx");
        return NGX_OK;
    }

    if (!ctx->done) {
        dd("ctx not done");
        return NGX_OK;
    }

    rc = ngx_http_form_input_arg(r, v->data, v->len, res, 0);

    return rc;
}


/* hand out an array with nothing in it.  a request that carries no form
 * body still has to leave a readable value behind: array_join and the
 * other array-var directives refuse a value whose length is not
 * sizeof(ngx_array_t) and fail the request with a 500, so the empty
 * string that used to stand here answered a plain GET with an error */
static ngx_int_t
ngx_http_form_input_empty_array(ngx_http_request_t *r, ngx_str_t *res)
{
    ngx_array_t         *array;

    array = ngx_array_create(r->pool, 1, sizeof(ngx_str_t));
    if (array == NULL) {
        return NGX_ERROR;
    }

    res->data = (u_char *) array;
    res->len = sizeof(ngx_array_t);

    return NGX_OK;
}


static ngx_int_t
ngx_http_set_form_input_multi(ngx_http_request_t *r, ngx_str_t *res,
    ngx_http_variable_value_t *v)
{
    ngx_http_form_input_ctx_t           *ctx;

    dd_enter();

    ctx = ngx_http_get_module_ctx(r, ngx_http_form_input_module);

    if (r->done || ctx == NULL || !ctx->done) {
        dd("no request body was parsed, handing out an empty array");

        return ngx_http_form_input_empty_array(r, res);
    }

    return ngx_http_form_input_arg(r, v->data, v->len, res, 1);
}


/* the three transfer encodings that leave the bytes of a part as they
 * are.  RFC 7578 tells senders to use nothing else here */
static ngx_flag_t
ngx_http_form_input_plain_encoding(ngx_str_t *v)
{
    return (v->len == sizeof("7bit") - 1
            && ngx_strncasecmp(v->data, (u_char *) "7bit", v->len) == 0)
        || (v->len == sizeof("8bit") - 1
            && ngx_strncasecmp(v->data, (u_char *) "8bit", v->len) == 0)
        || (v->len == sizeof("binary") - 1
            && ngx_strncasecmp(v->data, (u_char *) "binary", v->len) == 0);
}


/* read what the headers of a part say about it: the Content-Disposition
 * that names the field, and whether anything else stands in the way.
 * the headers are one per line and end at the blank line the caller
 * already found.
 *
 * three shapes are refused rather than worked around, because each of
 * them would let this module hand out something other than what the
 * application behind it reads:
 *
 *   * a line that starts with whitespace, a continuation of the one
 *     above that RFC 9112 deprecates and lets a recipient refuse, and
 *     behind which a filename parameter could hide
 *   * a part that carries Content-Disposition twice, same effect
 *   * a Content-Transfer-Encoding naming anything but the three that
 *     leave the bytes alone.  a form library that meets base64 here
 *     decodes it, and this module does not, so the two would disagree
 *     about the value
 *
 * either answer means the part is not a form field: NGX_DECLINED for a
 * part without the header, NGX_ERROR for one of the three shapes */
static ngx_int_t
ngx_http_form_input_disposition(u_char *p, u_char *last, ngx_str_t *value)
{
    u_char      *eol, *v, *e;
    ngx_str_t    encoding;
    ngx_flag_t   found;

    found = 0;

    while (p < last) {

        if (*p == ' ' || *p == '\t') {
            dd("a folded header line in a part");

            return NGX_ERROR;
        }

        eol = ngx_http_form_input_find(p, last, (u_char *) CRLF, 2);

        if (eol == NULL) {
            eol = last;
        }

        if ((size_t) (eol - p) > sizeof("Content-Disposition") - 1
            && ngx_strncasecmp(p, (u_char *) "Content-Disposition",
                               sizeof("Content-Disposition") - 1) == 0
            && p[sizeof("Content-Disposition") - 1] == ':')
        {
            if (found) {
                dd("more than one Content-Disposition in a part");

                return NGX_ERROR;
            }

            found = 1;
            v = p + sizeof("Content-Disposition");

            while (v < eol && (*v == ' ' || *v == '\t')) {
                v++;
            }

            value->data = v;
            value->len = eol - v;

        } else if ((size_t) (eol - p) > sizeof("Content-Transfer-Encoding") - 1
                   && ngx_strncasecmp(p,
                                      (u_char *) "Content-Transfer-Encoding",
                                      sizeof("Content-Transfer-Encoding") - 1)
                      == 0
                   && p[sizeof("Content-Transfer-Encoding") - 1] == ':')
        {
            v = p + sizeof("Content-Transfer-Encoding");
            e = eol;

            while (v < e && (*v == ' ' || *v == '\t')) {
                v++;
            }

            while (e > v && (e[-1] == ' ' || e[-1] == '\t')) {
                e--;
            }

            encoding.data = v;
            encoding.len = e - v;

            if (!ngx_http_form_input_plain_encoding(&encoding)) {
                dd("a part encoded in a way this module does not undo");

                return NGX_ERROR;
            }
        }

        if (eol == last) {
            break;
        }

        p = eol + 2;
    }

    return found ? NGX_OK : NGX_DECLINED;
}


/* a delimiter stands on a line of its own: RFC 2046 lets nothing follow
 * the boundary but optional whitespace and the line break, or the two
 * dashes that close the body.  an occurrence with anything else behind
 * it is content, and reading it as a delimiter would cut a value short
 * or drop the fields behind it. */
static ngx_flag_t
ngx_http_form_input_delimiter_ends(u_char *p, u_char *last)
{
    if (last - p >= 2 && p[0] == '-' && p[1] == '-') {

        /* the two dashes that close the body.  behind them RFC 2046
         * allows whitespace and then the epilogue, or nothing at all,
         * but not text of its own */

        p += 2;

        while (p < last && (*p == ' ' || *p == '\t')) {
            p++;
        }

        return p == last || (last - p >= 2 && p[0] == CR && p[1] == LF);
    }

    while (p < last && (*p == ' ' || *p == '\t')) {
        p++;
    }

    return last - p >= 2 && p[0] == CR && p[1] == LF;
}


/* the next delimiter at or after p, passing over what only looks like
 * one */
static u_char *
ngx_http_form_input_delimiter(ngx_http_form_input_part_t *mp, u_char *p)
{
    u_char      *delim;

    for ( ;; ) {
        delim = ngx_http_form_input_find(p, mp->last, mp->delimiter.data,
                                         mp->delimiter.len);
        if (delim == NULL) {
            return NULL;
        }

        if (ngx_http_form_input_delimiter_ends(delim + mp->delimiter.len,
                                               mp->last))
        {
            return delim;
        }

        p = delim + 1;
    }
}


/* hand out the next part that is a plain form field.
 *
 * NGX_OK      name and content are filled in
 * NGX_AGAIN   a part that is not a form field, ask again
 * NGX_DONE    the closing delimiter was reached
 * NGX_ERROR   the body does not hold together */
static ngx_int_t
ngx_http_form_input_part(ngx_http_form_input_part_t *mp, ngx_str_t *name,
    ngx_str_t *content)
{
    u_char      *p, *delim, *headers, *end;
    ngx_str_t    disposition;
    ngx_int_t    rc;

    if (mp->done) {
        return NGX_DONE;
    }

    p = mp->pos;

    if (!mp->started) {

        /* the opening delimiter either starts the body, and then comes
         * without the line break, or it closes a preamble and comes
         * with one.  an occurrence in the middle of a line is neither,
         * and taking it for a delimiter would let a client show this
         * module a different set of fields than the application behind
         * it reads */

        if ((size_t) (mp->last - p) >= mp->delimiter.len - 2
            && ngx_memcmp(p, mp->delimiter.data + 2,
                          mp->delimiter.len - 2) == 0
            && ngx_http_form_input_delimiter_ends(p + mp->delimiter.len - 2,
                                                  mp->last))
        {
            p += mp->delimiter.len - 2;

        } else {
            delim = ngx_http_form_input_delimiter(mp, p);

            if (delim == NULL) {
                dd("no delimiter line in the body at all");
                mp->done = 1;

                return NGX_DONE;
            }

            p = delim + mp->delimiter.len;
        }

        mp->started = 1;
    }

    /* two more dashes close the body */

    if (mp->last - p >= 2 && p[0] == '-' && p[1] == '-') {
        mp->done = 1;

        return NGX_DONE;
    }

    /* transport padding, then the line break that starts the part */

    while (p < mp->last && (*p == ' ' || *p == '\t')) {
        p++;
    }

    if (mp->last - p < 2 || p[0] != CR || p[1] != LF) {
        dd("no line break behind the delimiter");
        mp->done = 1;

        return NGX_ERROR;
    }

    p += 2;
    headers = p;

    /* find where the part ends before looking inside it.  the headers
     * are searched within that range and not beyond, so a part whose
     * blank line is missing cannot borrow the one of the part behind
     * it and claim its content as a field */

    delim = ngx_http_form_input_delimiter(mp, p);

    if (delim == NULL) {
        dd("a part is not closed by a delimiter");
        mp->done = 1;

        return NGX_ERROR;
    }

    if (delim - p >= 2 && p[0] == CR && p[1] == LF) {

        /* the blank line comes first, so this part carries no headers
         * at all and everything behind it is content */

        end = p;
        p += 2;

    } else if (delim == p) {
        end = p;

    } else {
        end = ngx_http_form_input_find(p, delim, (u_char *) CRLF CRLF, 4);

        if (end == NULL) {
            dd("the headers of a part are not closed");
            mp->done = 1;

            return NGX_ERROR;
        }

        p = end + 4;
    }

    content->data = p;
    content->len = delim - p;

    mp->pos = delim + mp->delimiter.len;

    if (ngx_http_form_input_disposition(headers, end, &disposition) != NGX_OK)
    {
        dd("no usable Content-Disposition in a part");

        return NGX_AGAIN;
    }

    /* RFC 7578 gives every part of a form the disposition type
     * form-data.  a part calling itself something else is one a strict
     * reader drops, and taking it for a field would show this module
     * more than the application behind it sees */

    if (!ngx_http_form_input_is_type(&disposition, "form-data",
                                     sizeof("form-data") - 1))
    {
        dd("a part that does not call itself form-data");

        return NGX_AGAIN;
    }

    /* a part that names a file is an upload rather than a form field.
     * its content is arbitrary bytes and arbitrarily large, so it does
     * not belong in a variable */

    rc = ngx_http_form_input_param(&disposition, "filename",
                                   sizeof("filename") - 1, NULL);

    if (rc != NGX_DECLINED) {
        dd("skipping a file part");

        return NGX_AGAIN;
    }

    if (ngx_http_form_input_param(&disposition, "name", sizeof("name") - 1,
                                  name) != NGX_OK)
    {
        dd("a part without a name");

        return NGX_AGAIN;
    }

    return NGX_OK;
}


/* the multipart counterpart of the loop in ngx_http_form_input_arg.
 * the body and the delimiter come from the request context, where the
 * phase handler and ngx_http_form_input_read_body left them. */
static ngx_int_t
ngx_http_form_input_multipart(ngx_http_request_t *r,
    ngx_http_form_input_ctx_t *ctx, u_char *arg_name, size_t arg_len,
    ngx_str_t *value, ngx_flag_t multi, ngx_array_t *array)
{
    ngx_str_t                    name, content;
    ngx_str_t                   *s;
    ngx_int_t                    rc;
    ngx_http_form_input_part_t   mp;

    mp.pos = ctx->body.data;
    mp.last = ctx->body.data + ctx->body.len;
    mp.delimiter = ctx->delimiter;
    mp.started = 0;
    mp.done = 0;

    for ( ;; ) {
        rc = ngx_http_form_input_part(&mp, &name, &content);

        if (rc == NGX_AGAIN) {
            continue;
        }

        if (rc == NGX_ERROR) {
            ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                          "form-input: the multipart body does not hold "
                          "together, fields after the fault are not read");

            return NGX_OK;
        }

        if (rc == NGX_DONE) {
            return NGX_OK;
        }

        if (name.len != arg_len
            || ngx_strncasecmp(name.data, arg_name, arg_len) != 0)
        {
            continue;
        }

        dd("multipart field: %.*s", (int) content.len, content.data);

        if (!multi) {
            *value = content;

            return NGX_OK;
        }

        s = ngx_array_push(array);
        if (s == NULL) {
            return NGX_ERROR;
        }

        *s = content;
    }
}


/* fork from ngx_http_arg.
 * read argument(s) with name arg_name and length arg_len into value variable,
 * if multi flag is set, multi arguments with name arg_name will be read and
 * stored in an ngx_array_t struct, this can be operated by directives in
 * array-var-nginx-module */
static ngx_int_t
ngx_http_form_input_arg(ngx_http_request_t *r, u_char *arg_name, size_t arg_len,
    ngx_str_t *value, ngx_flag_t multi)
{
    u_char                      *p, *v, *last, *buf;
    ngx_array_t                 *array = NULL;
    ngx_str_t                   *s;
    ngx_str_t                    body;
    ngx_http_form_input_ctx_t   *ctx;

    if (multi) {
        array = ngx_array_create(r->pool, 1, sizeof(ngx_str_t));
        if (array == NULL) {
            return NGX_ERROR;
        }
        value->data = (u_char *)array;
        value->len = sizeof(ngx_array_t);

    } else {
        ngx_str_set(value, "");
    }

    if (arg_len == 0) {
        dd("empty field name");
        return NGX_OK;
    }

    if (ngx_http_form_input_read_body(r, &body) != NGX_OK) {
        return NGX_ERROR;
    }

    if (body.len == 0) {
        return NGX_OK;
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_form_input_module);

    if (ctx != NULL && ctx->multipart) {
        return ngx_http_form_input_multipart(r, ctx, arg_name, arg_len,
                                             value, multi, array);
    }

    buf = body.data;
    last = body.data + body.len;

    for (p = buf; p < last; p++) {
        /* we need '=' after name, so drop one char from last */

        p = ngx_strlcasestrn(p, last - 1, arg_name, arg_len - 1);
        if (p == NULL) {
            return NGX_OK;
        }

        dd("found argument name, offset: %d", (int) (p - buf));

        if ((p == buf || *(p - 1) == '&') && *(p + arg_len) == '=') {
            v = p + arg_len + 1;
            dd("v = %d...", (int) (v - buf));

            dd("buf now (len %d): %.*s",
               (int) (last - v), (int) (last - v), v);

            p = ngx_strlchr(v, last, '&');
            if (p == NULL) {
                dd("& not found, pointing it to last...");
                p = last;

            } else {
                dd("found &, pointing it to %d...", (int) (p - buf));
            }

            if (multi) {
                s = ngx_array_push(array);
                if (s == NULL) {
                    return NGX_ERROR;
                }
                s->data = v;
                s->len = p - v;
                dd("array var:%.*s", (int) s->len, s->data);

            } else {
                value->data = v;
                value->len = p - v;
                dd("value: [%.*s]", (int) value->len, value->data);
                return NGX_OK;
            }
        }
    }

    return NGX_OK;
}


/* make the whole request body available as one contiguous buffer.
 * nginx writes the body to a temporary file as soon as it exceeds
 * client_body_buffer_size, so a buffer may live in a file rather than
 * in memory.  the single in-memory buffer is handed out as is, anything
 * else is assembled once and kept in the module context, because this
 * runs once per set_form_input directive and not once per request. */
static ngx_int_t
ngx_http_form_input_read_body(ngx_http_request_t *r, ngx_str_t *body)
{
    u_char                       *p;
    size_t                        len, size;
    ssize_t                       n;
    off_t                         total;
    ngx_buf_t                    *b;
    ngx_chain_t                  *cl;
    ngx_http_form_input_ctx_t    *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_form_input_module);

    if (ctx != NULL && ctx->body_read) {
        *body = ctx->body;
        return NGX_OK;
    }

    ngx_str_null(body);

    if (r->request_body == NULL || r->request_body->bufs == NULL) {
        dd("empty rb or empty rb bufs");
        goto done;
    }

    total = 0;

    for (cl = r->request_body->bufs; cl; cl = cl->next) {
        b = cl->buf;

        total += b->in_file ? b->file_last - b->file_pos : b->last - b->pos;
    }

    /* off_t is wider than size_t on 32 bit platforms */

    if (total < 0 || (uint64_t) total > (uint64_t) NGX_MAX_SIZE_T_VALUE) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "form-input: request body of %O bytes is too large "
                      "to parse", total);
        return NGX_ERROR;
    }

    len = (size_t) total;

    dd("body len=%d", (int) len);

    if (len == 0) {
        goto done;
    }

    if (r->request_body->bufs->next == NULL
        && !r->request_body->bufs->buf->in_file)
    {
        dd("one in-memory buffer only, no copy needed");

        body->data = r->request_body->bufs->buf->pos;
        body->len = len;

        goto done;
    }

    p = ngx_palloc(r->pool, len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    body->data = p;
    body->len = len;

    for (cl = r->request_body->bufs; cl; cl = cl->next) {
        b = cl->buf;

        if (b->in_file) {
            size = (size_t) (b->file_last - b->file_pos);

            n = ngx_read_file(b->file, p, size, b->file_pos);

            if (n == NGX_ERROR) {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "form-input: failed to read the request body "
                              "from \"%V\"", &b->file->name);

                ngx_str_null(body);

                return NGX_ERROR;
            }

            if ((size_t) n != size) {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "form-input: read only %z of %uz from \"%V\"",
                              n, size, &b->file->name);

                ngx_str_null(body);

                return NGX_ERROR;
            }

            p += n;

        } else {
            p = ngx_copy(p, b->pos, b->last - b->pos);
        }
    }

    /* never hand out more than was actually written */

    body->len = (size_t) (p - body->data);

    dd("copied body (len %d)", (int) body->len);

done:

    if (ctx != NULL) {
        ctx->body = *body;
        ctx->body_read = 1;
    }

    return NGX_OK;
}


/* does Content-Type name this media type?  RFC 9110 ends the type at
 * the end of the header value or where the parameters begin, and allows
 * whitespace before that semicolon.  a longer type that merely starts
 * with the same characters is a different one */
static ngx_flag_t
ngx_http_form_input_is_type(ngx_str_t *value, const char *type, size_t len)
{
    u_char      *p;

    if (value->len < len
        || ngx_strncasecmp(value->data, (u_char *) type, len) != 0)
    {
        return 0;
    }

    if (value->len == len) {
        return 1;
    }

    p = value->data + len;

    return *p == ';' || *p == ' ' || *p == '\t';
}


/* look up a parameter of a header value.  parameters are separated by
 * semicolons, their order is not fixed and the value may be quoted, so
 * this walks them instead of assuming a spelling.  the result points
 * into the header, which lives as long as the request.
 *
 * NGX_DECLINED means the parameter is not there, which is an ordinary
 * answer for an optional one such as filename.
 *
 * a backslash inside a quoted value is not treated as an escape.  RFC
 * 7578 asks senders not to use one, browsers percent encode instead. */
static ngx_int_t
ngx_http_form_input_param(ngx_str_t *header, const char *name, size_t len,
    ngx_str_t *value)
{
    u_char      *p, *last, *start, *q;
    ngx_flag_t   quoted;

    p = header->data;
    last = header->data + header->len;

    while (p < last) {

        /* step over the parameter in front, where a semicolon between
         * quotes separates nothing.  reading it as a separator would
         * let a value smuggle a parameter of its own past this */

        quoted = 0;

        while (p < last) {

            if (*p == '"') {
                quoted = !quoted;

            } else if (*p == ';' && !quoted) {
                break;
            }

            p++;
        }

        if (p == last) {
            break;
        }

        p++;

        while (p < last && (*p == ' ' || *p == '\t')) {
            p++;
        }

        if ((size_t) (last - p) < len + 1
            || ngx_strncasecmp(p, (u_char *) name, len) != 0)
        {
            continue;
        }

        q = p + len;

        if (value == NULL) {

            /* a lookup that only asks whether the parameter is there is
             * on purpose the forgiving one.  it is what decides that a
             * part holds an upload, and a counterparty that reads
             * "filename*=" or "filename =" as a file name must not be
             * able to show this module a plain field in its place */

            if (*q == '=' || *q == '*' || *q == ' ' || *q == '\t') {
                return NGX_OK;
            }

            continue;
        }

        if (*q != '=') {
            continue;
        }

        p = q + 1;

        if (p < last && *p == '"') {
            p++;
            start = p;

            while (p < last && *p != '"') {
                p++;
            }

            if (p == last) {
                dd("quoted parameter value is not closed");
                return NGX_ERROR;
            }

        } else {
            start = p;

            while (p < last && *p != ';' && *p != ' ' && *p != '\t') {
                p++;
            }
        }

        if (value != NULL) {
            value->data = start;
            value->len = p - start;
        }

        return NGX_OK;
    }

    return NGX_DECLINED;
}


static ngx_int_t
ngx_http_form_input_boundary(ngx_str_t *type, ngx_str_t *boundary)
{
    if (ngx_http_form_input_param(type, "boundary", sizeof("boundary") - 1,
                                  boundary) != NGX_OK)
    {
        dd("no boundary parameter");
        return NGX_ERROR;
    }

    if (boundary->len == 0 || boundary->len > form_boundary_max) {
        dd("boundary of %d bytes is out of range", (int) boundary->len);
        return NGX_ERROR;
    }

    return NGX_OK;
}


/* find a byte sequence in a range, without needing a terminator the way
 * ngx_strstrn does.  a request body carries no terminating zero. */
static u_char *
ngx_http_form_input_find(u_char *p, u_char *last, u_char *needle, size_t n)
{
    if (n == 0 || (size_t) (last - p) < n) {
        return NULL;
    }

    last -= n - 1;

    while (p < last) {
        p = ngx_strlchr(p, last, needle[0]);

        if (p == NULL) {
            return NULL;
        }

        if (ngx_memcmp(p, needle, n) == 0) {
            return p;
        }

        p++;
    }

    return NULL;
}


/* set_form_input_multi hands out an ngx_array_t behind a value of
 * sizeof(ngx_array_t) bytes.  that is the calling convention of
 * array-var-nginx-module and the only way to read such a variable.
 * whatever else reads it sees the bare structure, so a configuration
 * without array-var can do nothing with the variable but write the
 * structure, live heap addresses included, into a response. */
static ngx_flag_t
ngx_http_form_input_have_array_var(ngx_conf_t *cf)
{
    ngx_uint_t           i;
    ngx_module_t       **modules;

    modules = cf->cycle->modules;

    for (i = 0; modules[i] != NULL; i++) {
        if (modules[i]->name != NULL
            && ngx_strcmp(modules[i]->name, "ngx_http_array_var_module") == 0)
        {
            return 1;
        }
    }

    return 0;
}


static char *
ngx_http_set_form_input_conf_handler(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    ndk_set_var_t                            filter;
    ngx_str_t                               *value, s;
    u_char                                  *p;
    ngx_http_form_input_main_conf_t         *fmcf;
    ngx_http_form_input_loc_conf_t          *flcf = conf;

    ngx_str_null(&s);

    fmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_form_input_module);

    /* the main flag decides whether the phase handler is installed at
     * all, the location flag whether it does anything for a request */

    fmcf->used = 1;
    flcf->used = 1;

    filter.type = NDK_SET_VAR_MULTI_VALUE;
    filter.size = 1;

    value = cf->args->elts;

    if ((value->len == sizeof("set_form_input_multi") - 1) &&
        ngx_strncmp(value->data, "set_form_input_multi", value->len) == 0)
    {
        dd("use ngx_http_form_input_multi");

        if (!ngx_http_form_input_have_array_var(cf)) {
            return "needs array-var-nginx-module, which is missing from "
                   "this build";
        }

        filter.func = (void *) ngx_http_set_form_input_multi;

    } else {
        filter.func = (void *) ngx_http_set_form_input;
    }

    value++;

    if (cf->args->nelts == 2) {
        p = value->data;
        p++;
        s.len = value->len - 1;
        s.data = p;

    } else if (cf->args->nelts == 3) {
        s.len = (value + 1)->len;
        s.data = (value + 1)->data;
    }

    return ndk_set_var_multi_value_core(cf, value, &s, &filter);
}


/* register a new rewrite phase handler */
static ngx_int_t
ngx_http_form_input_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt             *h;
    ngx_http_core_main_conf_t       *cmcf;
    ngx_http_form_input_main_conf_t *fmcf;

    fmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_form_input_module);

    if (!fmcf->used) {
        return NGX_OK;
    }

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_REWRITE_PHASE].handlers);

    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_form_input_handler;

    return NGX_OK;
}


/* an rewrite phase handler */
static ngx_int_t
ngx_http_form_input_handler(ngx_http_request_t *r)
{
    ngx_http_form_input_ctx_t       *ctx;
    ngx_http_form_input_loc_conf_t  *flcf;
    ngx_str_t                        value, boundary, delimiter;
    ngx_flag_t                       multipart;
    ngx_int_t                        rc;

    dd_enter();

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http form_input rewrite phase handler");

    ctx = ngx_http_get_module_ctx(r, ngx_http_form_input_module);

    if (ctx != NULL) {
        if (ctx->done) {
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http form_input rewrite phase handler done");

            return NGX_DECLINED;
        }

        return NGX_DONE;
    }

    /* the handler sits in the rewrite phase of every request, so without
     * this a single directive anywhere would buffer the body of every
     * urlencoded POST and PUT in the whole server */

    flcf = ngx_http_get_module_loc_conf(r, ngx_http_form_input_module);

    if (!flcf->used) {
        return NGX_DECLINED;
    }

    if (r->method != NGX_HTTP_POST && r->method != NGX_HTTP_PUT) {
        return NGX_DECLINED;
    }

    if (r->headers_in.content_type == NULL
        || r->headers_in.content_type->value.data == NULL)
    {
        dd("content_type is %p", r->headers_in.content_type);

        return NGX_DECLINED;
    }

    value = r->headers_in.content_type->value;

    dd("r->headers_in.content_length_n:%d",
       (int) r->headers_in.content_length_n);

    /* the two encodings a form can arrive in.  multipart is only read
     * where it was asked for: it means buffering uploads, and its values
     * are literal where urlencoded ones are percent encoded */

    ngx_str_null(&boundary);
    ngx_str_null(&delimiter);
    multipart = 0;

    if (ngx_http_form_input_is_type(&value, form_urlencoded_type,
                                    form_urlencoded_type_len))
    {
        dd("content type is application/x-www-form-urlencoded");

    } else if (flcf->multipart
               && ngx_http_form_input_is_type(&value, form_multipart_type,
                                              form_multipart_type_len))
    {
        dd("content type is multipart/form-data");

        if (ngx_http_form_input_boundary(&value, &boundary) != NGX_OK) {
            ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                          "form-input: no usable boundary in \"%V\"", &value);

            return NGX_DECLINED;
        }

        /* RFC 2046 makes the line break part of the delimiter, so build
         * it that way once here rather than once per directive.  only
         * the one that opens the body may come without it */

        delimiter.len = boundary.len + 4;
        delimiter.data = ngx_palloc(r->pool, delimiter.len);

        if (delimiter.data == NULL) {
            return NGX_ERROR;
        }

        delimiter.data[0] = CR;
        delimiter.data[1] = LF;
        delimiter.data[2] = '-';
        delimiter.data[3] = '-';
        ngx_memcpy(delimiter.data + 4, boundary.data, boundary.len);

        multipart = 1;

    } else {
        dd("not a form body this module reads");

        return NGX_DECLINED;
    }

    dd("create new ctx");

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_form_input_ctx_t));
    if (ctx == NULL) {
        return NGX_ERROR;
    }

    /* set by ngx_pcalloc:
     *      ctx->done = 0;
     *      ctx->waiting_more_body = 0;
     *      ctx->body_read = 0;
     *      ctx->body = { 0, NULL };
     */

    ctx->multipart = multipart;
    ctx->delimiter = delimiter;

    ngx_http_set_ctx(r, ctx, ngx_http_form_input_module);

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http form_input start to read client request body");

    rc = ngx_http_read_client_request_body(r, ngx_http_form_input_post_read);

    if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        return rc;
    }

    if (rc == NGX_AGAIN) {
        ctx->waiting_more_body = 1;

        return NGX_DONE;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http form_input has read the request body in one run");

    return NGX_DECLINED;
}


static void
ngx_http_form_input_post_read(ngx_http_request_t *r)
{
    ngx_http_form_input_ctx_t     *ctx;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http form_input post read request body");

    ctx = ngx_http_get_module_ctx(r, ngx_http_form_input_module);

    ctx->done = 1;

    dd("count--");
    r->main->count--;

    dd("waiting more body: %d", (int) ctx->waiting_more_body);

    /* the rewrite phase handler is waiting for this */

    if (ctx->waiting_more_body) {
        ctx->waiting_more_body = 0;

        ngx_http_core_run_phases(r);
    }
}


static void *
ngx_http_form_input_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_form_input_main_conf_t    *fmcf;

    fmcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_form_input_main_conf_t));
    if (fmcf == NULL) {
        return NULL;
    }

    /* set by ngx_pcalloc:
     *      fmcf->used = 0;
     */

    return fmcf;
}


static void *
ngx_http_form_input_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_form_input_loc_conf_t     *flcf;

    flcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_form_input_loc_conf_t));
    if (flcf == NULL) {
        return NULL;
    }

    flcf->used = NGX_CONF_UNSET;
    flcf->multipart = NGX_CONF_UNSET;

    return flcf;
}


static char *
ngx_http_form_input_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_form_input_loc_conf_t     *prev = parent;
    ngx_http_form_input_loc_conf_t     *conf = child;

    /* used is deliberately not inherited.  the directives compile into
     * the rewrite module, and ngx_http_rewrite_merge_loc_conf leaves a
     * nested location without codes of its own empty handed, so a
     * nested location would buffer request bodies for nothing */

    if (conf->used == NGX_CONF_UNSET) {
        conf->used = 0;
    }

    /* the switch is an ordinary flag that the phase handler reads, so
     * it inherits the ordinary way and can be set for a whole server */

    ngx_conf_merge_value(conf->multipart, prev->multipart, 0);

    return NGX_CONF_OK;
}
