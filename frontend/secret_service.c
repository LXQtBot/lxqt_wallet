/*
 * copyright: 2013
 * name : Francis Banyikwa
 * email: mhogomchungu@gmail.com
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <libsecret-1/libsecret/secret.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * Its a bit tricky to use stable part of libsecret API to add our secrets the way that will agree with out API that is consistent with
 * libsecret backend,kwallet backend and the internal back end.
 *
 * The biggest problem is how to get information you do not know about in the wallet.For our usecase,how to get a list of all keys
 * in the wallet.KeyID schema is used to solve this problem.
 *
 * This problem is solved by having two schemas.
 *
 * Schema "s" holds attributes of the wallet.These attributes are:
 * 1. wallet size
 * 2. key and their associated key values
 *
 * "lxqt_wallet_open" is another attributes that is used to try to write to the wallet to check if its open or not
 *
 * Schema "keyID" is used to store a list of keys in the wallet in a way that make it easy to retrieve them.When a key is added
 * in the wallet,a smallest number is seached to attach it to the volume.Seaching is necessary to reuse numbers vacated by removed keys
 */

#define BUFFER_SIZE 32

static char *_get_string_value_0(const SecretSchema *s, const char *key)
{
    return secret_password_lookup_sync(s, NULL, NULL, "string", key, NULL);
}

static int _get_string_value(const SecretSchema *s, const char *key)
{
    char *c = _get_string_value_0(s, key);
    int e;

    if (c == NULL)
    {
        return -1;
    }
    else
    {
        e = atoi(c);
        free(c);
        return e;
    }
}

static char *_get_integer_value_0(const SecretSchema *s, int key)
{
    return secret_password_lookup_sync(s, NULL, NULL, "integer", key, NULL);
}

static int _get_integer_value(const SecretSchema *s, int key)
{
    char *c = _get_integer_value_0(s, key);
    int e;

    if (c == NULL)
    {
        return -1;
    }
    else
    {
        e = atoi(c);
        free(c);
        return e;
    }
}

static int _set_integer_value(const SecretSchema *s, const char *name, const char *key, int value)
{
    return secret_password_store_sync(s, "default", name, key, NULL, NULL, "integer", value, NULL);
}

static int _set_string_value(const SecretSchema *s, const char *name, const char *key, const char *value)
{
    return secret_password_store_sync(s, "default", name, key, NULL, NULL, "string", value, NULL);
}

static int _number_of_entries_in_the_wallet(const SecretSchema *s)
{
    int e = _get_string_value(s, "lxqt_wallet_size");

    if (e == -1)
    {
        return 0;
    }
    else
    {
        return e;
    }
}

static int _clear_integer_value(const SecretSchema *s, int k)
{
    return secret_password_clear_sync(s, NULL, NULL, "integer", k, NULL);
}

static int _clear_string_value(const SecretSchema *s, const char *k)
{
    return secret_password_clear_sync(s, NULL, NULL, "string", k, NULL);
}

/*
 * This function is used to check if a wallet is open or not and force prompt to open it if it is not open.
 *
 * write to the wallet and the operation will succeed if the wallet is open.
 * If the wallet is not open,the operation will block while a user is prompted for a key to unlock it.
 * If the user fail to unlock it,the operation will fail.
 */
int lxqt_secret_service_wallet_is_open(const void *s)
{
    const SecretSchema *e = s;
    return _set_string_value(e, e->name, "lxqt_wallet_open", "lxqt_wallet_open");
}

char *lxqt_secret_service_get_value(const char *key, const void *s)
{
    return _get_string_value_0(s, key);
}

void *lxqt_secret_service_create_schema(const char *schemaName, const char *type)
{
    SecretSchema *s = malloc(sizeof(SecretSchema));

    memset(s, '\0', sizeof(SecretSchema));

    s->name  = schemaName;
    s->flags = SECRET_SCHEMA_NONE;

    s->attributes[0].name = type;

    if (strcmp(type, "string") == 0)
    {
        s->attributes[0].type = SECRET_SCHEMA_ATTRIBUTE_STRING;
    }
    else
    {
        s->attributes[0].type = SECRET_SCHEMA_ATTRIBUTE_INTEGER;
    }

    s->attributes[1].name = "NULL";
    s->attributes[1].type = 0;

    return s;
}

gboolean lxqt_secret_service_password_store_sync(const char *key,
        const char *value,
        const void *p, const void *q)
{
    const SecretSchema *keyValues = p;
    const SecretSchema *keyID     = q;

    int i = 0;
    int k;
    int j;

    const char *walletLabel = keyValues->name;
    char wallet_size[ BUFFER_SIZE ];

    if (!lxqt_secret_service_wallet_is_open(keyValues))
    {
        return FALSE;
    }

    j = _get_string_value(keyValues, "lxqt_wallet_size");

    if (j == -1)
    {
        _set_string_value(keyValues, walletLabel, "1", "lxqt_wallet_size");
        _set_integer_value(keyID, walletLabel, key, 0);
    }
    else
    {
        j = j + 1;

        snprintf(wallet_size, BUFFER_SIZE, "%d", j);

        _set_string_value(keyValues, walletLabel, wallet_size, "lxqt_wallet_size");

        while (i < j)
        {
            k = _get_integer_value(keyID, i);

            if (k == -1)
            {
                if (_set_integer_value(keyID, walletLabel, key, i))
                {
                    if (_set_string_value(keyValues, walletLabel, value, key))
                    {
                        return TRUE;
                    }
                    else
                    {
                        _clear_integer_value(keyID, i);
                        return FALSE;
                    }
                }
                else
                {
                    return FALSE;
                }
            }
            else
            {
                i++;
            }
        }
    }

    return FALSE;
}

gboolean lxqt_secret_service_clear_sync(const char *key, const void *p, const void *q)
{
    const SecretSchema *keyValues = p;
    const SecretSchema *keyID     = q;

    int i = 0;
    int k = 0;
    int e;

    char *c;
    char wallet_size[ BUFFER_SIZE ];
    const char *walletLabel = keyValues->name;

    int j = _number_of_entries_in_the_wallet(keyValues);

    if (!lxqt_secret_service_wallet_is_open(keyValues))
    {
        return FALSE;
    }

    while (i <= j)
    {
        c = _get_integer_value_0(keyID, k);

        if (c != NULL)
        {
            e = strcmp(c, key);

            free(c);

            if (e == 0)
            {
                _clear_integer_value(keyID, k);

                e = _get_string_value(keyValues, "lxqt_wallet_size");
                snprintf(wallet_size, BUFFER_SIZE, "%d", e - 1);

                _set_string_value(keyValues, walletLabel, wallet_size, "lxqt_wallet_size");

                _clear_string_value(keyValues, key);

                return TRUE;
            }
            else
            {
                i++;
                k++;
            }
        }
        else
        {
            k++;
        }
    }

    return FALSE;
}

char **lxqt_secret_get_all_keys(const void *p, const void *q, int *count)
{
    const SecretSchema *keyValues = p;
    const SecretSchema *keyID     = q;

    int k = 0;
    int i = 0;
    int j;

    char **c = NULL ;
    char *e  = NULL;

    *count = 0;

    if (lxqt_secret_service_wallet_is_open(keyValues))
    {
        j = _number_of_entries_in_the_wallet(keyValues);
        c = malloc(sizeof(char *) * j);

        if (c != NULL)
        {
            while (i < j)
            {
                e = _get_integer_value_0(keyID, k);

                if (e != NULL)
                {
                    *(c + i) = e;
                    *count += 1;
                    i++;
                    k++;
                }
                else
                {
                    k++;
                }
            }
        }
    }

    return c;
}

int lxqt_secret_service_wallet_size(const void *s)
{
    return _number_of_entries_in_the_wallet(s);
}
