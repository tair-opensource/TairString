/*
 * Copyright 2021 Alibaba Tair Team
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
 * disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "tairstring.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "redismodule.h"
#include "util.h"

#define TAIR_STRING_SET_NO_FLAGS 0
#define TAIR_STRING_SET_NX (1 << 0)
#define TAIR_STRING_SET_XX (1 << 1)
#define TAIR_STRING_SET_EX (1 << 2)
#define TAIR_STRING_SET_PX (1 << 3)
#define TAIR_STRING_SET_ABS_EXPIRE (1 << 4)
#define TAIR_STRING_SET_WITH_VER (1 << 5)
#define TAIR_STRING_SET_WITH_ABS_VER (1 << 6)
#define TAIR_STRING_SET_WITH_BOUNDARY (1 << 7)
#define TAIR_STRING_SET_WITH_FLAGS (1 << 8)
#define TAIR_STRING_SET_WITH_DEF (1 << 9)
#define TAIR_STRING_SET_NONEGATIVE (1 << 10)
#define TAIR_STRING_RETURN_WITH_VER (1 << 11)
#define TAIR_STRING_SET_KEEPTTL (1 << 12)

#define TAIRSTRING_ENCVER_VER_1 0

static RedisModuleType *TairStringType;

#pragma pack(1)
typedef struct TairStringObj {
    uint64_t version;
    uint32_t flags;
    RedisModuleString *value;
} TairStringObj;

static struct TairStringObj *createTairStringTypeObject(void) {
    return (TairStringObj *)RedisModule_Calloc(1, sizeof(TairStringObj));
}

static void TairStringTypeReleaseObject(struct TairStringObj *o) {
    if (!o) return;

    if (o->value) {
        RedisModule_FreeString(NULL, o->value);
    }

    RedisModule_Free(o);
}

static int mstring2ld(RedisModuleString *val, long double *r_val) {
    if (!val) return REDISMODULE_ERR;

    size_t t_len;
    const char *t_ptr = RedisModule_StringPtrLen(val, &t_len);
    if (m_string2ld(t_ptr, t_len, r_val) == 0) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}

static int mstringcasecmp(const RedisModuleString *rs1, const char *s2) {
    size_t n1 = strlen(s2);
    size_t n2;
    const char *s1 = RedisModule_StringPtrLen(rs1, &n2);
    if (n1 != n2) {
        return -1;
    }
    return strncasecmp(s1, s2, n1);
}

/* Parse the command **argv and get those arguments. Return ex_flags. If parsing
 * get failed, It would reply with syntax error. The first appearance would be
 * accepted if there are multiple appearance of a same group, For example: "EX
 * 3 PX 4000 EXAT 127", the "EX 3" would be accepted and the other two would be
 * ignored.
 * */
static int parseAndGetExFlags(RedisModuleString **argv, int argc, int start, int *ex_flag, RedisModuleString **expire_p,
                              RedisModuleString **version_p, RedisModuleString **flags_p,
                              RedisModuleString **defaultvalue_p, RedisModuleString **min_p,
                              RedisModuleString **max_p, unsigned int allow_flags) {
    int j, ex_flags = TAIR_STRING_SET_NO_FLAGS;
    for (j = start; j < argc; j++) {
        RedisModuleString *next = (j == argc - 1) ? NULL : argv[j + 1];

        if (!mstringcasecmp(argv[j], "nx")) {
            if (ex_flags & TAIR_STRING_SET_XX) {
                return REDISMODULE_ERR;
            }
            ex_flags |= TAIR_STRING_SET_NX;
        } else if (!mstringcasecmp(argv[j], "xx")) {
            if (ex_flags & TAIR_STRING_SET_NX) {
                return REDISMODULE_ERR;
            }
            ex_flags |= TAIR_STRING_SET_XX;
        } else if (expire_p != NULL && !mstringcasecmp(argv[j], "ex") && next) {
            if (ex_flags & (TAIR_STRING_SET_PX | TAIR_STRING_SET_EX | TAIR_STRING_SET_KEEPTTL)) {
                return REDISMODULE_ERR;
            }
            ex_flags |= TAIR_STRING_SET_EX;
            *expire_p = next;
            j++;
        } else if (expire_p != NULL && !mstringcasecmp(argv[j], "exat") && next) {
            if (ex_flags & (TAIR_STRING_SET_PX | TAIR_STRING_SET_EX | TAIR_STRING_SET_KEEPTTL)) {
                return REDISMODULE_ERR;
            }
            ex_flags |= TAIR_STRING_SET_EX;
            ex_flags |= TAIR_STRING_SET_ABS_EXPIRE;
            *expire_p = next;
            j++;
        } else if (expire_p != NULL && !mstringcasecmp(argv[j], "px") && next) {
            if (ex_flags & (TAIR_STRING_SET_PX | TAIR_STRING_SET_EX | TAIR_STRING_SET_KEEPTTL)) {
                return REDISMODULE_ERR;
            }
            ex_flags |= TAIR_STRING_SET_PX;
            *expire_p = next;
            j++;
        } else if (expire_p != NULL && !mstringcasecmp(argv[j], "pxat") && next) {
            if (ex_flags & (TAIR_STRING_SET_PX | TAIR_STRING_SET_EX | TAIR_STRING_SET_KEEPTTL)) {
                return REDISMODULE_ERR;
            }
            ex_flags |= TAIR_STRING_SET_PX;
            ex_flags |= TAIR_STRING_SET_ABS_EXPIRE;
            *expire_p = next;
            j++;
        } else if (version_p != NULL && !mstringcasecmp(argv[j], "ver") && next) {
            if (ex_flags & (TAIR_STRING_SET_WITH_VER | TAIR_STRING_SET_WITH_ABS_VER)) {
                return REDISMODULE_ERR;
            }
            ex_flags |= TAIR_STRING_SET_WITH_VER;
            *version_p = next;
            j++;
        } else if (version_p != NULL && !mstringcasecmp(argv[j], "abs") && next) {
            if (ex_flags & (TAIR_STRING_SET_WITH_VER | TAIR_STRING_SET_WITH_ABS_VER)) {
                return REDISMODULE_ERR;
            }
            ex_flags |= TAIR_STRING_SET_WITH_ABS_VER;
            *version_p = next;
            j++;
        } else if (flags_p != NULL && !mstringcasecmp(argv[j], "flags") && next) {
            if (ex_flags & TAIR_STRING_SET_WITH_FLAGS) {
                return REDISMODULE_ERR;
            }
            ex_flags |= TAIR_STRING_SET_WITH_FLAGS;
            *flags_p = next;
            j++;
        } else if (defaultvalue_p != NULL && !mstringcasecmp(argv[j], "def") && next) { /* DEF disabled if XX set. */
            if (ex_flags & TAIR_STRING_SET_WITH_DEF) {
                return REDISMODULE_ERR;
            }
            ex_flags |= TAIR_STRING_SET_WITH_DEF;
            *defaultvalue_p = next;
            j++;
        } else if (min_p != NULL && !mstringcasecmp(argv[j], "min") && next) {
            if (*min_p != NULL) {
                return REDISMODULE_ERR;
            }
            ex_flags |= TAIR_STRING_SET_WITH_BOUNDARY;
            *min_p = next;
            j++;
        } else if (max_p != NULL && !mstringcasecmp(argv[j], "max") && next) {
            if (*max_p != NULL) {
                return REDISMODULE_ERR;
            }
            ex_flags |= TAIR_STRING_SET_WITH_BOUNDARY;
            *max_p = next;
            j++;
        } else if (!mstringcasecmp(argv[j], "nonegative")) {
            ex_flags |= TAIR_STRING_SET_NONEGATIVE;
        } else if (!mstringcasecmp(argv[j], "withversion")) {
            ex_flags |= TAIR_STRING_RETURN_WITH_VER;
        } else if (!mstringcasecmp(argv[j], "keepttl")) {
            if (ex_flags & (TAIR_STRING_SET_PX | TAIR_STRING_SET_EX)) {
                return REDISMODULE_ERR;
            }
            ex_flags |= TAIR_STRING_SET_KEEPTTL;
        } else {
            return REDISMODULE_ERR;
        }
    }
    
if ((~allow_flags) & ex_flags) {
        return REDISMODULE_ERR;
    }

    *ex_flag = ex_flags;
    return REDISMODULE_OK;
}

/* ========================= "tairstring" type commands =======================*/

/* EXSET <key> <value> [EX/EXAT/PX/PXAT time] [NX/XX] [VER/ABS version] [FLAGS flags] [WITHVERSION] [KEEPTTL] */
int TairStringTypeSet_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    long long milliseconds = 0, expire = 0, version = 0, flags = 0;
    RedisModuleString *expire_p = NULL, *version_p = NULL, *flags_p = NULL;
    int ex_flags = TAIR_STRING_SET_NO_FLAGS;
    unsigned int allow_flags = TAIR_STRING_SET_NX | TAIR_STRING_SET_XX | TAIR_STRING_SET_EX | TAIR_STRING_SET_PX | 
                      TAIR_STRING_SET_ABS_EXPIRE | TAIR_STRING_SET_KEEPTTL | TAIR_STRING_SET_WITH_VER |
                      TAIR_STRING_SET_WITH_ABS_VER | TAIR_STRING_SET_WITH_FLAGS | TAIR_STRING_RETURN_WITH_VER;
    if (parseAndGetExFlags(argv, argc, 3, &ex_flags, &expire_p, &version_p, &flags_p, NULL, NULL, NULL, allow_flags) != REDISMODULE_OK) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    if ((NULL != expire_p) && (RedisModule_StringToLongLong(expire_p, &expire) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    if ((NULL != version_p) && (RedisModule_StringToLongLong(version_p, &version) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    if ((NULL != flags_p) && (RedisModule_StringToLongLong(flags_p, &flags) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    if ((expire_p && expire <=0) || version < 0 || flags < 0 || flags > UINT_MAX) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    TairStringObj *tair_string_obj = NULL;
    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    int type = RedisModule_KeyType(key);
    if (REDISMODULE_KEYTYPE_EMPTY == type) {
        if (ex_flags & TAIR_STRING_SET_XX) {
            RedisModule_ReplyWithNull(ctx);
            return REDISMODULE_ERR;
        }
        tair_string_obj = createTairStringTypeObject();
        RedisModule_ModuleTypeSetValue(key, TairStringType, tair_string_obj);
    } else {
        if (RedisModule_ModuleTypeGetType(key) != TairStringType) {
            RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
            return REDISMODULE_ERR;
        }
        tair_string_obj = RedisModule_ModuleTypeGetValue(key);

        if (ex_flags & TAIR_STRING_SET_NX) {
            RedisModule_ReplyWithNull(ctx);
            return REDISMODULE_ERR;
        }

        /* Version 0 means no version checking. */
        if (ex_flags & TAIR_STRING_SET_WITH_VER && version != 0 && version != tair_string_obj->version) {
            RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_VERSION);
            return REDISMODULE_ERR;
        }
    }

    if (ex_flags & TAIR_STRING_SET_WITH_ABS_VER) {
        tair_string_obj->version = version;
    } else {
        tair_string_obj->version++;
    }

    if (type != REDISMODULE_KEYTYPE_EMPTY) {
        /* Free the old value. */
        if (tair_string_obj->value) {
            RedisModule_FreeString(ctx, tair_string_obj->value);
        }
    }
    tair_string_obj->value = argv[2];
    /* Reuse the value to avoid memory copies. */
    RedisModule_RetainString(NULL, argv[2]);

    if (ex_flags & TAIR_STRING_SET_WITH_FLAGS) {
        tair_string_obj->flags = flags;
    }

    if (expire_p) {
        if (ex_flags & TAIR_STRING_SET_EX) {
            expire *= 1000;
        }
        if (ex_flags & TAIR_STRING_SET_ABS_EXPIRE) {
            /* Since the RedisModule_SetExpire interface can only set relative
            expiration times, here we first convert the absolute time passed
            in by the user to relative time. */
            milliseconds = expire - RedisModule_Milliseconds();
            if (milliseconds < 0) {
                /* Time out now. */
                milliseconds = 0;
            }
        } else {
            milliseconds = expire;
        }

        RedisModule_SetExpire(key, milliseconds);
    } else if (!(ex_flags & TAIR_STRING_SET_KEEPTTL)) {
        RedisModule_SetExpire(key, REDISMODULE_NO_EXPIRE);
    }

    /* Rewrite relative value to absolute value. */
    size_t vlen = 4, VSIZE_MAX = 8;
    RedisModuleString **v = NULL;
    v = RedisModule_Calloc(sizeof(RedisModuleString *), VSIZE_MAX);
    v[0] = RedisModule_CreateStringFromString(ctx, argv[1]);
    v[1] = RedisModule_CreateStringFromString(ctx, argv[2]);
    v[2] = RedisModule_CreateString(ctx, "ABS", 3);
    v[3] = RedisModule_CreateStringFromLongLong(ctx, tair_string_obj->version);
    if (expire_p) {
        v[vlen] = RedisModule_CreateString(ctx, "PXAT", 4);
        v[vlen + 1] = RedisModule_CreateStringFromLongLong(ctx, milliseconds + RedisModule_Milliseconds());
        vlen += 2;
    }
    if (flags_p) {
        v[vlen] = RedisModule_CreateString(ctx, "FLAGS", 5);
        v[vlen + 1] = RedisModule_CreateStringFromLongLong(ctx, (long long)tair_string_obj->flags);
        vlen += 2;
    }
    RedisModule_Replicate(ctx, "EXSET", "v", v, vlen);
    RedisModule_Free(v);

    if (ex_flags & TAIR_STRING_RETURN_WITH_VER) {
        RedisModule_ReplyWithLongLong(ctx, tair_string_obj->version);
    } else {
        RedisModule_ReplyWithSimpleString(ctx, "OK");
    }

    return REDISMODULE_OK;
}

/* EXGET <key> [WITHFLAGS] */
int TairStringTypeGet_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc < 2 || argc > 3) {
        return RedisModule_WrongArity(ctx);
    }

    if (argc == 3 && mstringcasecmp(argv[2], "withflags")) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
    int type = RedisModule_KeyType(key);
    if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != TairStringType) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_ReplyWithNull(ctx);
        return REDISMODULE_OK;
    }

    TairStringObj *o = RedisModule_ModuleTypeGetValue(key);
    if (argc == 2) {
        RedisModule_ReplyWithArray(ctx, 2);
        RedisModule_ReplyWithString(ctx, o->value);
        RedisModule_ReplyWithLongLong(ctx, o->version);
    } else { /* argc == 3, WITHFLAGS .*/
        RedisModule_ReplyWithArray(ctx, 3);
        RedisModule_ReplyWithString(ctx, o->value);
        RedisModule_ReplyWithLongLong(ctx, o->version);
        RedisModule_ReplyWithLongLong(ctx, (long long)o->flags);
    }

    return REDISMODULE_OK;
}

/* EXINCRBY <key> <num> [DEF default_value] [EX/EXAT/PX/PXAT time] [NX/XX]
 * [VER/ABS version] [MIN/MAX maxval] [NONEGATIVE] [WITHVERSION] [KEEPTTL] */
int TairStringTypeIncrBy_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    long long min = 0, max = 0, value, incr, defaultvalue = 0; /* If DEF is not set, then defaultvalue = 0 .*/
    RedisModuleString *min_p = NULL, *max_p = NULL;
    long long milliseconds = 0, expire = 0, version = 0;
    RedisModuleString *expire_p = NULL, *version_p = NULL, *defaultvalue_p = NULL;

    int ex_flags = TAIR_STRING_SET_NO_FLAGS;
    unsigned int allow_flags = TAIR_STRING_SET_NX | TAIR_STRING_SET_XX | TAIR_STRING_SET_EX | TAIR_STRING_SET_PX | 
                      TAIR_STRING_SET_ABS_EXPIRE | TAIR_STRING_SET_KEEPTTL | TAIR_STRING_SET_WITH_VER |
                      TAIR_STRING_SET_WITH_ABS_VER  | TAIR_STRING_RETURN_WITH_VER | TAIR_STRING_SET_WITH_DEF |
                      TAIR_STRING_SET_NONEGATIVE | TAIR_STRING_SET_WITH_BOUNDARY;
    if (parseAndGetExFlags(argv, argc, 3, &ex_flags, &expire_p, &version_p, NULL, &defaultvalue_p, &min_p, &max_p, allow_flags) != REDISMODULE_OK) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    int type = RedisModule_KeyType(key);
    if (REDISMODULE_KEYTYPE_EMPTY != type && RedisModule_ModuleTypeGetType(key) != TairStringType) {
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return REDISMODULE_ERR;
    }

    if (RedisModule_StringToLongLong(argv[2], &incr) != REDISMODULE_OK) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_NO_INT);
        return REDISMODULE_ERR;
    }

    if ((NULL != defaultvalue_p) && (RedisModule_StringToLongLong(defaultvalue_p, &defaultvalue) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_NO_INT);
        return REDISMODULE_ERR;
    }

    if ((NULL != expire_p) && (RedisModule_StringToLongLong(expire_p, &expire) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    if ((NULL != version_p) && (RedisModule_StringToLongLong(version_p, &version) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    if ((expire_p && expire <=0) || version < 0) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    if ((NULL != min_p) && (RedisModule_StringToLongLong(min_p, &min))) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_MIN_MAX);
        return REDISMODULE_ERR;
    }

    if ((NULL != max_p) && (RedisModule_StringToLongLong(max_p, &max))) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_MIN_MAX);
        return REDISMODULE_ERR;
    }

    if (NULL != min_p && NULL != max_p && max < min) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_MIN_MAX);
        return REDISMODULE_ERR;
    }

    TairStringObj *tair_string_obj = NULL;
    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        if (ex_flags & TAIR_STRING_SET_XX) {
            RedisModule_ReplyWithNull(ctx);
            return REDISMODULE_ERR;
        }
        tair_string_obj = createTairStringTypeObject();
        value = defaultvalue;
    } else {
        if (ex_flags & TAIR_STRING_SET_NX) {
            RedisModule_ReplyWithNull(ctx);
            return REDISMODULE_ERR;
        }

        tair_string_obj = RedisModule_ModuleTypeGetValue(key);
        if (RedisModule_StringToLongLong(tair_string_obj->value, &value) != REDISMODULE_OK) {
            RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_NO_INT);
            return REDISMODULE_ERR;
        }

        if (ex_flags & TAIR_STRING_SET_WITH_VER && version != 0 && version != tair_string_obj->version) {
            RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_VERSION);
            return REDISMODULE_ERR;
        }
    }

    /* If DEF is set and the key is empty at first, the value won't increase, so
     * it's unnecessary to check overflow; else the value would increase by
     * incr, so overflow should be checked. "unnecessary to check overflow" =
     * "won't return ERR" = "no need to release object".
     * */
    if (!(ex_flags & TAIR_STRING_SET_WITH_DEF && type == REDISMODULE_KEYTYPE_EMPTY)) {
        /* Check overflow. */
        if ((incr < 0 && value < 0 && incr < (LLONG_MIN - value))
            || (incr > 0 && value > 0 && incr > (LLONG_MAX - value)) || (max_p != NULL && value + incr > max)
            || (min_p != NULL && value + incr < min)) {
            /* If type == EMPTY, then the tair_string_obj is created, so it
             * should be released here. */
            if (type == REDISMODULE_KEYTYPE_EMPTY && tair_string_obj) TairStringTypeReleaseObject(tair_string_obj);
            RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_OVERFLOW);
            return REDISMODULE_ERR;
        }
        value += incr;
    }

    /* value shouldn't be negative if NONEGATIVE is set; if value is negative,
     * let value = 0 */
    if (ex_flags & TAIR_STRING_SET_NONEGATIVE) value = value < 0 ? 0LL : value;

    if (type != REDISMODULE_KEYTYPE_EMPTY) {
        if (tair_string_obj->value) {
            RedisModule_FreeString(ctx, tair_string_obj->value);
            tair_string_obj->value = NULL;
        }
    } else {
        RedisModule_ModuleTypeSetValue(key, TairStringType, tair_string_obj);
    }

    tair_string_obj->value = RedisModule_CreateStringFromLongLong(NULL, value);

    if (ex_flags & TAIR_STRING_SET_WITH_ABS_VER) {
        tair_string_obj->version = version;
    } else {
        /* If the key doesn't exist and default is set, the version should be 1
         * although the value won't increase. */
        tair_string_obj->version++;
    }

    if (expire_p) {
        if (ex_flags & TAIR_STRING_SET_EX) {
            expire *= 1000;
        }
        if (ex_flags & TAIR_STRING_SET_ABS_EXPIRE) {
            milliseconds = expire - RedisModule_Milliseconds();
            if (milliseconds < 0) {
                milliseconds = 0;
            }
        } else {
            milliseconds = expire;
        }

        RedisModule_SetExpire(key, milliseconds);
    } else if (!(ex_flags & TAIR_STRING_SET_KEEPTTL)) {
        RedisModule_SetExpire(key, REDISMODULE_NO_EXPIRE);
    }

    if (expire_p) {
        RedisModule_Replicate(ctx, "EXSET", "ssclcl", argv[1], tair_string_obj->value, "ABS", tair_string_obj->version,
                              "PXAT", (milliseconds + RedisModule_Milliseconds()));
    } else {
        RedisModule_Replicate(ctx, "EXSET", "sscl", argv[1], tair_string_obj->value, "ABS", tair_string_obj->version);
    }

    if (ex_flags & TAIR_STRING_RETURN_WITH_VER) {
        RedisModule_ReplyWithArray(ctx, 2);
        RedisModule_ReplyWithLongLong(ctx, value);
        RedisModule_ReplyWithLongLong(ctx, tair_string_obj->version);
    } else {
        RedisModule_ReplyWithLongLong(ctx, value);
    }
    return REDISMODULE_OK;
}

/* EXINCRBYFLOAT <key> <num> [MIN/MAX maxval] [EX/EXAT/PX/PXAT time] [NX/XX] [VER/ABS version] [KEEPTTL] */
int TairStringTypeIncrByFloat_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    long double min = 0, max = 0, value, oldvalue, incr;
    RedisModuleString *min_p = NULL, *max_p = NULL;
    long long milliseconds = 0, expire = 0, version = 0;
    RedisModuleString *expire_p = NULL, *version_p = NULL;

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    int type = RedisModule_KeyType(key);
    if (REDISMODULE_KEYTYPE_EMPTY != type && RedisModule_ModuleTypeGetType(key) != TairStringType) {
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return REDISMODULE_ERR;
    }

    if (mstring2ld(argv[2], &incr) == REDISMODULE_ERR) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_NO_FLOAT);
        return REDISMODULE_ERR;
    }

    int ex_flags = TAIR_STRING_SET_NO_FLAGS;
    unsigned int allow_flags = TAIR_STRING_SET_NX | TAIR_STRING_SET_XX | TAIR_STRING_SET_EX | TAIR_STRING_SET_PX | 
                      TAIR_STRING_SET_ABS_EXPIRE | TAIR_STRING_SET_KEEPTTL | TAIR_STRING_SET_WITH_VER |
                      TAIR_STRING_SET_WITH_ABS_VER | TAIR_STRING_SET_WITH_BOUNDARY;
    if (parseAndGetExFlags(argv, argc, 3, &ex_flags, &expire_p, &version_p, NULL, NULL, &min_p, &max_p, allow_flags) != REDISMODULE_OK) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    if ((NULL != expire_p) && (RedisModule_StringToLongLong(expire_p, &expire) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    if ((NULL != version_p) && (RedisModule_StringToLongLong(version_p, &version) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    if ((expire_p && expire <=0) || version < 0) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    if ((NULL != min_p) && (mstring2ld(min_p, &min) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_MIN_MAX);
        return REDISMODULE_ERR;
    }

    if ((NULL != max_p) && (mstring2ld(max_p, &max) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_MIN_MAX);
        return REDISMODULE_ERR;
    }

    if (NULL != min_p && NULL != max_p && max < min) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_MIN_MAX);
        return REDISMODULE_ERR;
    }

    TairStringObj *tair_string_obj = NULL;
    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        if (ex_flags & TAIR_STRING_SET_XX) {
            RedisModule_ReplyWithNull(ctx);
            return REDISMODULE_ERR;
        }
        tair_string_obj = createTairStringTypeObject();
        value = 0;
    } else {
        if (ex_flags & TAIR_STRING_SET_NX) {
            RedisModule_ReplyWithNull(ctx);
            return REDISMODULE_ERR;
        }

        tair_string_obj = RedisModule_ModuleTypeGetValue(key);
        if (mstring2ld(tair_string_obj->value, &value) != REDISMODULE_OK) {
            RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_NO_FLOAT);
            return REDISMODULE_ERR;
        }

        if (ex_flags & TAIR_STRING_SET_WITH_VER && version != 0 && version != tair_string_obj->version) {
            RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_VERSION);
            return REDISMODULE_ERR;
        }
    }

    oldvalue = value;

    if (isnan(oldvalue + incr) || isinf(oldvalue + incr) || (max_p != NULL && oldvalue + incr > max)
        || (min_p != NULL && oldvalue + incr < min)) {
        if (type == REDISMODULE_KEYTYPE_EMPTY && tair_string_obj) TairStringTypeReleaseObject(tair_string_obj);
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_OVERFLOW);
        return REDISMODULE_ERR;
    }

    if (ex_flags & TAIR_STRING_SET_WITH_ABS_VER) {
        tair_string_obj->version = version;
    } else {
        tair_string_obj->version++;
    }

    value += incr;

    char dbuf[MAX_LONG_DOUBLE_CHARS];
    int dlen = m_ld2string(dbuf, sizeof(dbuf), value, 1);

    if (type != REDISMODULE_KEYTYPE_EMPTY) {
        if (tair_string_obj->value) {
            RedisModule_FreeString(ctx, tair_string_obj->value);
            tair_string_obj->value = NULL;
        }
    } else {
        RedisModule_ModuleTypeSetValue(key, TairStringType, tair_string_obj);
    }
    tair_string_obj->value = RedisModule_CreateString(NULL, dbuf, dlen);

    if (expire_p) {
        if (ex_flags & TAIR_STRING_SET_EX) {
            expire *= 1000;
        }
        if (ex_flags & TAIR_STRING_SET_ABS_EXPIRE) {
            milliseconds = expire - RedisModule_Milliseconds();
            if (milliseconds < 0) {
                milliseconds = 0;
            }
        } else {
            milliseconds = expire;
        }

        RedisModule_SetExpire(key, milliseconds);
    } else if (!(ex_flags & TAIR_STRING_SET_KEEPTTL)) {
        RedisModule_SetExpire(key, REDISMODULE_NO_EXPIRE);
    }

    if (expire_p) {
        RedisModule_Replicate(ctx, "EXSET", "ssclcl", argv[1], tair_string_obj->value, "ABS", tair_string_obj->version,
                              "PXAT", (milliseconds + RedisModule_Milliseconds()));
    } else {
        RedisModule_Replicate(ctx, "EXSET", "sscl", argv[1], tair_string_obj->value, "ABS", tair_string_obj->version);
    }

    RedisModule_ReplyWithString(ctx, tair_string_obj->value);
    return REDISMODULE_OK;
}

/* EXSETVER <key> <version> */
int TairStringTypeExSetVer_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    long long version = 0;

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    int type = RedisModule_KeyType(key);
    if (REDISMODULE_KEYTYPE_EMPTY != type && RedisModule_ModuleTypeGetType(key) != TairStringType) {
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return REDISMODULE_ERR;
    }

    TairStringObj *tair_string_obj = NULL;
    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_ReplyWithLongLong(ctx, 0);
        return REDISMODULE_OK;
    } else {
        if (RedisModule_ModuleTypeGetType(key) != TairStringType) {
            return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        }
        tair_string_obj = RedisModule_ModuleTypeGetValue(key);
    }

    if (RedisModule_StringToLongLong(argv[2], &version) != REDISMODULE_OK) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    if (version <= 0) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    RedisModule_ReplicateVerbatim(ctx);
    tair_string_obj->version = version;
    RedisModule_ReplyWithLongLong(ctx, 1);
    return REDISMODULE_OK;
}

/* EXCAS <key> <new_value> <version> [EX/EXAT/PX/PXAT time] [KEEPTTL] */
int TairStringTypeExCas_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc < 4) {
        return RedisModule_WrongArity(ctx);
    }

    long long version = 0;
    long long milliseconds = 0, expire = 0;
    RedisModuleString *expire_p = NULL;
    int ex_flags = TAIR_STRING_SET_NO_FLAGS;
    unsigned int allow_flags = TAIR_STRING_SET_EX | TAIR_STRING_SET_PX | TAIR_STRING_SET_ABS_EXPIRE | TAIR_STRING_SET_KEEPTTL;
    if (parseAndGetExFlags(argv, argc, 4, &ex_flags, &expire_p, NULL, NULL, NULL, NULL, NULL, allow_flags) != REDISMODULE_OK) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    if ((NULL != expire_p) && (RedisModule_StringToLongLong(expire_p, &expire) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    if (RedisModule_StringToLongLong(argv[3], &version) != REDISMODULE_OK) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_VER_INT);
        return REDISMODULE_ERR;
    }

    if ((expire_p && expire <=0) || version < 0) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_WRITE);
    int type = RedisModule_KeyType(key);
    if (REDISMODULE_KEYTYPE_EMPTY != type && RedisModule_ModuleTypeGetType(key) != TairStringType) {
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return REDISMODULE_ERR;
    }

    TairStringObj *tair_string_obj = NULL;
    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_ReplyWithLongLong(ctx, -1);
        return REDISMODULE_OK;
    } else {
        if (RedisModule_ModuleTypeGetType(key) != TairStringType) {
            return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        }
        tair_string_obj = RedisModule_ModuleTypeGetValue(key);
    }

    if (tair_string_obj->version != version) {
        RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
        /* Here we can not use RedisModule_ReplyWithError directly, because this
        will cause jedis throw an exception, and the client can not read the
        later version and value. */
        RedisModule_ReplyWithSimpleString(ctx, TAIRSTRING_STATUSMSG_VERSION);
        RedisModule_ReplyWithString(ctx, tair_string_obj->value);
        RedisModule_ReplyWithLongLong(ctx, tair_string_obj->version);
        RedisModule_ReplySetArrayLength(ctx, 3);
        return REDISMODULE_ERR;
    }

    if (tair_string_obj->value) {
        RedisModule_FreeString(ctx, tair_string_obj->value);
        tair_string_obj->value = NULL;
    }
    tair_string_obj->value = argv[2];
    RedisModule_RetainString(NULL, argv[2]);
    tair_string_obj->version++;

    if (expire_p) {
        if (ex_flags & TAIR_STRING_SET_EX) {
            expire *= 1000;
        }
        if (ex_flags & TAIR_STRING_SET_ABS_EXPIRE) {
            milliseconds = expire - RedisModule_Milliseconds();
            if (milliseconds < 0) {
                milliseconds = 0;
            }
        } else {
            milliseconds = expire;
        }

        RedisModule_SetExpire(key, milliseconds);
    } else if (!(ex_flags & TAIR_STRING_SET_KEEPTTL)) {
        RedisModule_SetExpire(key, REDISMODULE_NO_EXPIRE);
    }

    if (expire_p) {
        RedisModule_Replicate(ctx, "EXSET", "ssclcl", argv[1], tair_string_obj->value, "ABS", tair_string_obj->version,
                              "PXAT", (milliseconds + RedisModule_Milliseconds()));
    } else {
        RedisModule_Replicate(ctx, "EXSET", "sscl", argv[1], tair_string_obj->value, "ABS", tair_string_obj->version);
    }

    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
    RedisModule_ReplyWithSimpleString(ctx, "OK");
    RedisModule_ReplyWithSimpleString(ctx, "");
    RedisModule_ReplyWithLongLong(ctx, tair_string_obj->version);
    RedisModule_ReplySetArrayLength(ctx, 3);
    return REDISMODULE_OK;
}

/* EXCAD <key> <version> */
int TairStringTypeExCad_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    long long version = 0;

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    int type = RedisModule_KeyType(key);
    if (REDISMODULE_KEYTYPE_EMPTY != type && RedisModule_ModuleTypeGetType(key) != TairStringType) {
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return REDISMODULE_ERR;
    }

    if (RedisModule_StringToLongLong(argv[2], &version) != REDISMODULE_OK) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    TairStringObj *tair_string_obj = NULL;
    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_ReplyWithLongLong(ctx, -1);
        return REDISMODULE_OK;
    } else {
        if (RedisModule_ModuleTypeGetType(key) != TairStringType) {
            return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        }
        tair_string_obj = RedisModule_ModuleTypeGetValue(key);
    }

    if (tair_string_obj->version != version) {
        RedisModule_ReplyWithLongLong(ctx, 0);
        return REDISMODULE_OK;
    }

    RedisModule_Replicate(ctx, "DEL", "s", argv[1]);
    RedisModule_DeleteKey(key);
    RedisModule_ReplyWithLongLong(ctx, 1);
    return REDISMODULE_OK;
}

/* CAD <key> <value> */
int StringTypeCad_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_WRITE);
    int type = RedisModule_KeyType(key);
    if (REDISMODULE_KEYTYPE_EMPTY != type && type != REDISMODULE_KEYTYPE_STRING) {
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return REDISMODULE_ERR;
    }

    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_ReplyWithLongLong(ctx, -1);
        return REDISMODULE_OK;
    } else {
        size_t proto_len, expect_len;
        RedisModuleCallReply *replay = RedisModule_Call(ctx, "GET", "s", argv[1]);
        switch (RedisModule_CallReplyType(replay)) {
            case REDISMODULE_REPLY_STRING:
                break;
            default:
                RedisModule_ReplyWithLongLong(ctx, 0);
                return REDISMODULE_OK;
        }

        const char *proto_ptr = RedisModule_CallReplyStringPtr(replay, &proto_len);
        const char *expect_ptr = RedisModule_StringPtrLen(argv[2], &expect_len);
        if (proto_len != expect_len || memcmp(expect_ptr, proto_ptr, proto_len) != 0) {
            RedisModule_ReplyWithLongLong(ctx, 0);
            return REDISMODULE_OK;
        }
    }

    RedisModule_DeleteKey(key);
    RedisModule_Replicate(ctx, "DEL", "s", argv[1]);
    RedisModule_ReplyWithLongLong(ctx, 1);
    return REDISMODULE_OK;
}

/* CAS <Key> <oldvalue> <newvalue> [EX/EXAT/PX/PXAT time] [KEEPTTL] */
int StringTypeCas_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc < 4) {
        return RedisModule_WrongArity(ctx);
    }

    long long milliseconds = 0, expire = 0;
    RedisModuleString *expire_p = NULL;
    int ex_flags = TAIR_STRING_SET_NO_FLAGS;
    unsigned int allow_flags = TAIR_STRING_SET_EX | TAIR_STRING_SET_PX | TAIR_STRING_SET_ABS_EXPIRE | TAIR_STRING_SET_KEEPTTL;
    if (parseAndGetExFlags(argv, argc, 4, &ex_flags, &expire_p, NULL, NULL, NULL, NULL, NULL, allow_flags) != REDISMODULE_OK) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    if ((NULL != expire_p) && (RedisModule_StringToLongLong(expire_p, &expire) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    if ((expire_p && expire <=0)) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_WRITE);
    int type = RedisModule_KeyType(key);
    if (REDISMODULE_KEYTYPE_EMPTY != type && type != REDISMODULE_KEYTYPE_STRING) {
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return REDISMODULE_ERR;
    }

    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_ReplyWithLongLong(ctx, -1);
        return REDISMODULE_OK;
    } else {
        size_t proto_len, expect_len;
        RedisModuleCallReply *replay = RedisModule_Call(ctx, "GET", "s", argv[1]);
        switch (RedisModule_CallReplyType(replay)) {
            case REDISMODULE_REPLY_STRING:
                break;
            default:
                RedisModule_ReplyWithLongLong(ctx, 0);
                return REDISMODULE_OK;
        }

        const char *proto_ptr = RedisModule_CallReplyStringPtr(replay, &proto_len);
        const char *expect_ptr = RedisModule_StringPtrLen(argv[2], &expect_len);
        if (proto_len != expect_len || memcmp(expect_ptr, proto_ptr, proto_len) != 0) {
            RedisModule_ReplyWithLongLong(ctx, 0);
            return REDISMODULE_OK;
        }
    }

    if (RedisModule_StringSet(key, argv[3]) != REDISMODULE_OK) {
        return REDISMODULE_ERR;
    }

    if (expire_p) {
        if (ex_flags & TAIR_STRING_SET_EX) {
            expire *= 1000;
        }
        if (ex_flags & TAIR_STRING_SET_ABS_EXPIRE) {
            milliseconds = expire - RedisModule_Milliseconds();
            if (milliseconds < 0) {
                milliseconds = 0;
            }
        } else {
            milliseconds = expire;
        }

        RedisModule_SetExpire(key, milliseconds);
    } else if (!(ex_flags & TAIR_STRING_SET_KEEPTTL)) {
        RedisModule_SetExpire(key, REDISMODULE_NO_EXPIRE);
    }

    RedisModule_Replicate(ctx, "SET", "ss", argv[1], argv[3]);
    if (expire_p) {
        RedisModule_Replicate(ctx, "PEXPIREAT", "sl", argv[1], (milliseconds + RedisModule_Milliseconds()));
    }

    RedisModule_ReplyWithLongLong(ctx, 1);
    return REDISMODULE_OK;
}

/* EXPREPEND <key> <value> [NX|XX] [VER/ABS version] */
int TairStringTypeExPrepend_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleString *version_p = NULL;
    long long version = 0;
    int ex_flags = TAIR_STRING_SET_NO_FLAGS;
    unsigned int allow_flags = TAIR_STRING_SET_NX | TAIR_STRING_SET_XX | TAIR_STRING_SET_WITH_VER | TAIR_STRING_SET_WITH_ABS_VER;
    if (parseAndGetExFlags(argv, argc, 3, &ex_flags, NULL, &version_p, NULL, NULL, NULL, NULL, allow_flags) != REDISMODULE_OK) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    if ((NULL != version_p) && (RedisModule_StringToLongLong(version_p, &version) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    if (version < 0) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    size_t originalLength;
    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_WRITE);
    RedisModuleString *newvalue;
    int type = RedisModule_KeyType(key);

    TairStringObj *tair_string_obj = NULL;

    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        /* not exist: result = argv[2] */
        if (ex_flags & TAIR_STRING_SET_XX) {
            RedisModule_ReplyWithNull(ctx);
            return REDISMODULE_ERR;
        }
        tair_string_obj = createTairStringTypeObject();
        tair_string_obj->value = argv[2];
        RedisModule_ModuleTypeSetValue(key, TairStringType, tair_string_obj);
        RedisModule_RetainString(ctx, argv[2]);
    } else {
        /* exist: result = argv[2] + original */
        /* Statements like "tair_string_obj->value = argv[2];
        Append(tair_string_obj->value, original);" would lead to a mistake when
        replicating the command from master to slave, so it's necessary to
        create a copy of argv[2] and use the copy to get the command executed.
      */

        if (ex_flags & TAIR_STRING_SET_NX) {
            RedisModule_ReplyWithNull(ctx);
            return REDISMODULE_ERR;
        }

        if (RedisModule_ModuleTypeGetType(key) != TairStringType) {
            RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
            return REDISMODULE_ERR;
        }
        tair_string_obj = RedisModule_ModuleTypeGetValue(key);

        if (ex_flags & TAIR_STRING_SET_WITH_VER && version != 0 && version != tair_string_obj->version) {
            RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_VERSION);
            return REDISMODULE_ERR;
        }

        /* Convert RedisModuleString to cstring to use StringAppendBuffer() */
        const char *c_string_original = RedisModule_StringPtrLen(tair_string_obj->value, &originalLength);
        newvalue = RedisModule_CreateStringFromString(ctx, argv[2]);

        if (RedisModule_StringAppendBuffer(ctx, newvalue, c_string_original, originalLength) == REDISMODULE_ERR) {
            RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_APPENDBUFFER);
            return REDISMODULE_ERR;
        }

        if (tair_string_obj->value) {
            RedisModule_FreeString(ctx, tair_string_obj->value);
        }

        tair_string_obj->value = newvalue;
        RedisModule_RetainString(NULL, newvalue);
    }

    if (ex_flags & TAIR_STRING_SET_WITH_ABS_VER) {
        tair_string_obj->version = version;
    } else {
        tair_string_obj->version++;
    }

    RedisModule_ReplicateVerbatim(ctx);
    RedisModule_ReplyWithLongLong(ctx, tair_string_obj->version);
    return REDISMODULE_OK;
}

/* EXAPPEND <key> <value> [NX|XX] [VER/ABS version] */
int TairStringTypeExAppend_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleString *version_p = NULL;
    long long version = 0;
    int ex_flags = TAIR_STRING_SET_NO_FLAGS;
    unsigned int allow_flags = TAIR_STRING_SET_NX | TAIR_STRING_SET_XX | TAIR_STRING_SET_WITH_VER | TAIR_STRING_SET_WITH_ABS_VER;
    if (parseAndGetExFlags(argv, argc, 3, &ex_flags, NULL, &version_p, NULL, NULL, NULL, NULL, allow_flags) != REDISMODULE_OK) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    if ((NULL != version_p) && (RedisModule_StringToLongLong(version_p, &version) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    if (version < 0) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    size_t appendLength;
    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_WRITE);
    int type = RedisModule_KeyType(key);

    TairStringObj *tair_string_obj = NULL;
    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        /* not exist: result = argv[2] */
        if (ex_flags & TAIR_STRING_SET_XX) {
            RedisModule_ReplyWithNull(ctx);
            return REDISMODULE_ERR;
        }

        tair_string_obj = createTairStringTypeObject();
        tair_string_obj->value = argv[2];
        RedisModule_ModuleTypeSetValue(key, TairStringType, tair_string_obj);
        RedisModule_RetainString(NULL, argv[2]);

    } else {
        /* exist: result = original + argv[2] */
        if (ex_flags & TAIR_STRING_SET_NX) {
            RedisModule_ReplyWithNull(ctx);
            return REDISMODULE_ERR;
        }
        if (RedisModule_ModuleTypeGetType(key) != TairStringType) {
            RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
            return REDISMODULE_ERR;
        }
        tair_string_obj = RedisModule_ModuleTypeGetValue(key);

        if (ex_flags & TAIR_STRING_SET_WITH_VER && version != 0 && version != tair_string_obj->version) {
            RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_VERSION);
            return REDISMODULE_ERR;
        }

        /* Convert RedisModuleString to cstring to use StringAppendBuffer() */
        const char *c_string_argv = RedisModule_StringPtrLen(argv[2], &appendLength);

        if (RedisModule_StringAppendBuffer(ctx, tair_string_obj->value, c_string_argv, appendLength)
            == REDISMODULE_ERR) {
            RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_APPENDBUFFER);
            return REDISMODULE_ERR;
        }
    }

    if (ex_flags & TAIR_STRING_SET_WITH_ABS_VER) {
        tair_string_obj->version = version;
    } else {
        tair_string_obj->version++;
    }

    RedisModule_ReplicateVerbatim(ctx);
    RedisModule_ReplyWithLongLong(ctx, tair_string_obj->version);
    return REDISMODULE_OK;
}

/* EXGAE <key> <EX time | EXAT time | PX time | PXAT time> */
int TairStringTypeExGAE_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc < 4) {
        return RedisModule_WrongArity(ctx);
    }
 
    RedisModuleString *expire_p;
    long long expire = 0, milliseconds = 0;
    int ex_flags = TAIR_STRING_SET_NO_FLAGS;
    unsigned int allow_flags = TAIR_STRING_SET_EX | TAIR_STRING_SET_PX | TAIR_STRING_SET_ABS_EXPIRE;
    if (parseAndGetExFlags(argv, argc, 2, &ex_flags, &expire_p, NULL, NULL, NULL, NULL, NULL, allow_flags) != REDISMODULE_OK) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    if (RedisModule_StringToLongLong(expire_p, &expire) != REDISMODULE_OK) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    if ((expire_p && expire <=0)) {
        RedisModule_ReplyWithError(ctx, TAIRSTRING_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_WRITE);
    int type = RedisModule_KeyType(key);
    if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != TairStringType) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_ReplyWithNull(ctx);
        return REDISMODULE_OK;
    }

    if (expire_p) {
        if (ex_flags & TAIR_STRING_SET_EX) {
            expire *= 1000;
        }
        if (ex_flags & TAIR_STRING_SET_ABS_EXPIRE) {
            milliseconds = expire - RedisModule_Milliseconds();
            if (milliseconds < 0) {
                milliseconds = 0;
            }
        } else {
            milliseconds = expire;
        }

        RedisModule_SetExpire(key, milliseconds);
    }

    RedisModule_SetExpire(key, expire);

    TairStringObj *o = RedisModule_ModuleTypeGetValue(key);

    RedisModule_ReplicateVerbatim(ctx);

    RedisModule_ReplyWithArray(ctx, 3);
    RedisModule_ReplyWithString(ctx, o->value);
    RedisModule_ReplyWithLongLong(ctx, o->version);
    RedisModule_ReplyWithLongLong(ctx, (long long)o->flags);
    return REDISMODULE_OK;
}

/* ========================== "exstrtype" type methods =======================*/
void *TairStringTypeRdbLoad(RedisModuleIO *rdb, int encver) {
    if (encver != TAIRSTRING_ENCVER_VER_1) {
        return NULL;
    }
    TairStringObj *o = createTairStringTypeObject();
    o->version = RedisModule_LoadUnsigned(rdb);
    o->flags = RedisModule_LoadUnsigned(rdb);
    o->value = RedisModule_LoadString(rdb);
    return o;
}

void TairStringTypeRdbSave(RedisModuleIO *rdb, void *value) {
    const struct TairStringObj *o = value;
    assert(value != NULL);
    RedisModule_SaveUnsigned(rdb, o->version);
    RedisModule_SaveUnsigned(rdb, o->flags);
    RedisModule_SaveString(rdb, o->value);
}

void TairStringTypeAofRewrite(RedisModuleIO *aof, RedisModuleString *key, void *value) {
    const struct TairStringObj *o = value;
    assert(value != NULL);
    RedisModule_EmitAOF(aof, "EXSET", "ssclcl", key, o->value, "ABS", o->version, "FLAGS", (long long)o->flags);
}

size_t TairStringTypeMemUsage(const void *value) {
    const struct TairStringObj *o = value;
    assert(value != NULL);
    size_t len;
    RedisModule_StringPtrLen(o->value, &len);
    return sizeof(*o) + len;
}

void TairStringTypeFree(void *value) { TairStringTypeReleaseObject(value); }

void TairStringTypeDigest(RedisModuleDigest *md, void *value) {
    const struct TairStringObj *o = value;
    assert(value != NULL);
    RedisModule_DigestAddLongLong(md, o->version);
    RedisModule_DigestAddLongLong(md, o->flags);
    size_t len;
    const char *str = RedisModule_StringPtrLen(o->value, &len);
    RedisModule_DigestAddStringBuffer(md, (unsigned char *)str, len);
    RedisModule_DigestEndSequence(md);
}

int Module_CreateCommands(RedisModuleCtx *ctx) {
#define CREATE_CMD(name, tgt, attr)                                                       \
    do {                                                                                  \
        if (RedisModule_CreateCommand(ctx, name, tgt, attr, 1, 1, 1) != REDISMODULE_OK) { \
            return REDISMODULE_ERR;                                                       \
        }                                                                                 \
    } while (0);

#define CREATE_WRCMD(name, tgt) CREATE_CMD(name, tgt, "write deny-oom")
#define CREATE_ROCMD(name, tgt) CREATE_CMD(name, tgt, "readonly fast")

    CREATE_WRCMD("exset", TairStringTypeSet_RedisCommand)
    CREATE_ROCMD("exget", TairStringTypeGet_RedisCommand)
    CREATE_WRCMD("exincrby", TairStringTypeIncrBy_RedisCommand)
    CREATE_WRCMD("exincrbyfloat", TairStringTypeIncrByFloat_RedisCommand)
    CREATE_WRCMD("exsetver", TairStringTypeExSetVer_RedisCommand)
    CREATE_WRCMD("excas", TairStringTypeExCas_RedisCommand)
    CREATE_WRCMD("excad", TairStringTypeExCad_RedisCommand)
    CREATE_WRCMD("exprepend", TairStringTypeExPrepend_RedisCommand)
    CREATE_WRCMD("exappend", TairStringTypeExAppend_RedisCommand)
    CREATE_WRCMD("exgae", TairStringTypeExGAE_RedisCommand)
    /* CAS/CAD cmds for redis string type. */
    CREATE_WRCMD("cas", StringTypeCas_RedisCommand)
    CREATE_WRCMD("cad", StringTypeCad_RedisCommand)

    return REDISMODULE_OK;
}

int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    REDISMODULE_NOT_USED(argv);
    REDISMODULE_NOT_USED(argc);

    if (RedisModule_Init(ctx, "exstrtype", 1, REDISMODULE_APIVER_1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleTypeMethods tm = {.version = REDISMODULE_TYPE_METHOD_VERSION,
                                 .rdb_load = TairStringTypeRdbLoad,
                                 .rdb_save = TairStringTypeRdbSave,
                                 .aof_rewrite = TairStringTypeAofRewrite,
                                 .mem_usage = TairStringTypeMemUsage,
                                 .free = TairStringTypeFree,
                                 .digest = TairStringTypeDigest};

    TairStringType = RedisModule_CreateDataType(ctx, "exstrtype", TAIRSTRING_ENCVER_VER_1, &tm);
    if (TairStringType == NULL) {
        return REDISMODULE_ERR;
    }

    if (REDISMODULE_ERR == Module_CreateCommands(ctx)) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
