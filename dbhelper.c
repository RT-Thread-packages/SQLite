/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-03-06     lizhen9880   first version
 */

#include <stdio.h>
#include <string.h>
#include <rtthread.h>
#include <ctype.h>
#include "dbhelper.h"

#define DBG_ENABLE
#define DBG_SECTION_NAME "app.dbhelper"
#define DBG_LEVEL DBG_INFO
#define DBG_COLOR
#include <rtdbg.h>

#if PKG_SQLITE_DB_NAME_MAX_LEN < 8
#error "the database name length is too short"
#endif
#define DEFAULT_DB_NAME "/rt.db"

static rt_mutex_t db_mutex_lock = RT_NULL;
static char db_name[PKG_SQLITE_DB_NAME_MAX_LEN + 1] = DEFAULT_DB_NAME;

/**
 * This function will initialize SQLite3 create a mutex as a lock.
 */
int db_helper_init(void)
{
    sqlite3_initialize();
    if (db_mutex_lock == RT_NULL)
    {
        db_mutex_lock = rt_mutex_create("dbmtx", RT_IPC_FLAG_FIFO);
    }
    if (db_mutex_lock == RT_NULL)
    {
        LOG_E("rt_mutex_create dbmtx failed!\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}
INIT_APP_EXPORT(db_helper_init);

/**
 * This function will create a database.
 *
 * @param sqlstr should be a SQL CREATE TABLE statements.
 * @return the result of sql execution.
 */
int db_create_database(const char *sqlstr)
{
    return db_nonquery_operator(sqlstr, 0, 0);
}

static int db_bind_by_var(sqlite3_stmt *stmt, const char *fmt, va_list args)
{
    int len, npara = 1;
    int ret = SQLITE_OK;
    if (fmt == NULL)
    {
        return ret;
    }

    for (; *fmt; ++fmt)
    {
        if (*fmt != '%')
        {
            continue;
        }
        ++fmt;
        /* get length */
        len = 0;
        while (isdigit(*fmt))
        {
            len = len * 10 + (*fmt - '0');
            ++fmt;
        }
        switch (*fmt)
        {
        case 'd':
            ret = sqlite3_bind_int(stmt, npara, va_arg(args, int));
            break;
        case 'f':
            ret = sqlite3_bind_double(stmt, npara, va_arg(args, double));
            break;
        case 's':
        {
            char *str = va_arg(args, char *);
            ret = sqlite3_bind_text(stmt, npara, str, strlen(str), NULL);
        }
        break;
        case 'x':
        {
            char *pdata;
            pdata = va_arg(args, char *);
            ret = sqlite3_bind_blob(stmt, npara, pdata, len, NULL);
        }
        break;
        default:
            ret = SQLITE_ERROR;
            break;
        }
        ++npara;
        if (ret)
            return ret;
    }
    return ret;
}

/**
 * This function will be used for the SELECT operating.The additional arguments
 * following format are formatted and inserted in the resulting string replacing
 * their respective specifiers.
 *
 * @param sql the SQL statements.
 * @param create the callback function supported by user.
 *              create@param stmt the SQL statement after preparing.
 *              create@param arg the input parameter from 'db_query_by_varpara' arg.
 *              create@return rule:SQLITE_OK:success,others:fail
 * @param arg the parameter for the callback "create".
 * @param fmt the args format.such as %s string,%d int.
 * @param ... the additional arguments
 * @return  =SQLITE_OK:success, others:fail.
 */
int db_query_by_varpara(const char *sql, int (*create)(sqlite3_stmt *stmt, void *arg), void *arg, const char *fmt, ...)
{
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    if (sql == NULL)
    {
        return SQLITE_ERROR;
    }
    rt_mutex_take(db_mutex_lock, RT_WAITING_FOREVER);
    int rc = sqlite3_open(db_name, &db);
    if (rc != SQLITE_OK)
    {
        LOG_E("open database failed,rc=%d", rc);
        rt_mutex_release(db_mutex_lock);
        return rc;
    }

    rc = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        LOG_E("database prepare fail,rc=%d", rc);
        goto __db_exec_fail;
    }

    if (fmt)
    {
        va_list args;
        va_start(args, fmt);
        rc = db_bind_by_var(stmt, fmt, args);
        va_end(args);
        if (rc)
        {
            LOG_E("database bind fail,rc=%d", rc);
            goto __db_exec_fail;
        }
    }

    if (create)
    {
        rc = (*create)(stmt, arg);
    }
    else
    {
        rc = (sqlite3_step(stmt), 0);
    }
    sqlite3_finalize(stmt);
    goto __db_exec_ok;
__db_exec_fail:
    LOG_E("db operator failed,rc=%d", rc);
__db_exec_ok:
    sqlite3_close(db);
    rt_mutex_release(db_mutex_lock);
    return rc;
}

/**
 * This function will be used for the operating that is not SELECT.It support executing multiple
 * SQL statements.
 *
 * @param sqlstr the SQL statements strings.if there are more than one
 *               statements in the sqlstr to execute,separate them by a semicolon(;).
 * @param bind the callback function supported by user.bind data and call the sqlite3_step function.
 *             bind@param stmt the SQL statement after preparing.
 *             bind@param index the index of SQL statements strings.
 *             bind@param param the parameter from 'db_nonquery_operator' arg.
 *             bind@return SQLITE_OK or SQLITE_DONE:success,others:fail
 * @param param the parameter for the callback "bind".
 * @return  =SQLITE_OK:success, others:fail.
 */
int db_nonquery_operator(const char *sqlstr, int (*bind)(sqlite3_stmt *stmt, int index, void *param), void *param)
{
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    int index = 0, offset = 0, n = 0;

    if (sqlstr == NULL)
    {
        return SQLITE_ERROR;
    }
    rt_mutex_take(db_mutex_lock, RT_WAITING_FOREVER);
    int rc = sqlite3_open(db_name, &db);
    if (rc != SQLITE_OK)
    {
        LOG_E("open database failed,rc=%d", rc);
        rt_mutex_release(db_mutex_lock);
        return rc;
    }
    rc = sqlite3_exec(db, "begin transaction", 0, 0, NULL);
    if (rc != SQLITE_OK)
    {
        LOG_E("begin transaction:ret=%d", rc);
        goto __db_begin_fail;
    }
    char sql[DB_SQL_MAX_LEN];
    while (sqlstr[index] != 0)
    {
        offset = 0;
        do
        {
            if (offset >= DB_SQL_MAX_LEN)
            {
                LOG_E("sql is too long,(%d)", offset);
                rc = SQLITE_ERROR;
                goto __db_exec_fail;
            }
            if ((sqlstr[index] != ';') && (sqlstr[index] != 0))
            {
                sql[offset++] = sqlstr[index++];
            }
            else
            {
                sql[offset] = '\0';
                if (sqlstr[index] == ';')
                {
                    index++;
                }
                n++;
                break;
            }
        } while (1);
        rc = sqlite3_prepare(db, sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK)
        {
            LOG_E("prepare error,rc=%d", rc);
            goto __db_exec_fail;
        }
        if (bind)
        {
            rc = (*bind)(stmt, n, param);
        }
        else
        {
            rc = sqlite3_step(stmt);
        }
        sqlite3_finalize(stmt);
        if ((rc != SQLITE_OK) && (rc != SQLITE_DONE))
        {
            LOG_E("bind failed");
            goto __db_exec_fail;
        }
    }
    rc = sqlite3_exec(db, "commit transaction", 0, 0, NULL);
    if (rc)
    {
        LOG_E("commit transaction:%d", rc);
        goto __db_exec_fail;
    }
    goto __db_exec_ok;

__db_exec_fail:
    if (sqlite3_exec(db, "rollback transaction", 0, 0, NULL))
    {
        LOG_E("rollback transaction error");
    }

__db_begin_fail:
    LOG_E("db operator failed,rc=%d", rc);

__db_exec_ok:
    sqlite3_close(db);
    rt_mutex_release(db_mutex_lock);
    return rc;
}

/**
 * This function will be used for the operating that is not SELECT.The additional
 * arguments following format are formatted and inserted in the resulting string
 * replacing their respective specifiers.
 *
 * @param sql the SQL statement.
 * @param fmt the args format.such as %s string,%d int.
 * @param ... the additional arguments
 * @return  =SQLITE_OK:success, others:fail.
 */
int db_nonquery_by_varpara(const char *sql, const char *fmt, ...)
{
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    if (sql == NULL)
    {
        return SQLITE_ERROR;
    }
    rt_mutex_take(db_mutex_lock, RT_WAITING_FOREVER);
    int rc = sqlite3_open(db_name, &db);
    if (rc != SQLITE_OK)
    {
        LOG_E("open database failed,rc=%d\n", rc);
        rt_mutex_release(db_mutex_lock);
        return rc;
    }
    LOG_D("sql:%s", sql);
    rc = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        LOG_E("prepare error,rc=%d", rc);
        goto __db_exec_fail;
    }
    if (fmt)
    {
        va_list args;
        va_start(args, fmt);
        rc = db_bind_by_var(stmt, fmt, args);
        va_end(args);
        if (rc)
        {
            goto __db_exec_fail;
        }
    }
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if ((rc != SQLITE_OK) && (rc != SQLITE_DONE))
    {
        LOG_E("bind error,rc=%d", rc);
        goto __db_exec_fail;
    }
    rc = SQLITE_OK;
    goto __db_exec_ok;

__db_exec_fail:
    LOG_E("db operator failed,rc=%d", rc);

__db_exec_ok:
    sqlite3_close(db);
    rt_mutex_release(db_mutex_lock);
    return rc;
}

/**
 * This function will be used for the transaction that is not SELECT.
 *
 * @param exec_sqls the callback function of executing SQL statements.
 *                  exec_sqls@param db the database connection handle
 *                  exec_sqls@param arg the input parameter from 'db_nonquery_transaction' function parameter 'arg'.
 *                  exec_sqls@return =SQLITE_OK or =SQLITE_DONE:success,others:fail
 * @param arg the parameter for the callback "exec_sqls".
 * @return  =SQLITE_OK:success, others:fail.
 */
int db_nonquery_transaction(int (*exec_sqls)(sqlite3 *db, void *arg), void *arg)
{
    sqlite3 *db = NULL;

    rt_mutex_take(db_mutex_lock, RT_WAITING_FOREVER);
    int rc = sqlite3_open(db_name, &db);
    if (rc != SQLITE_OK)
    {
        LOG_E("open database failed,rc=%d", rc);
        rt_mutex_release(db_mutex_lock);
        return rc;
    }
    rc = sqlite3_exec(db, "begin transaction", 0, 0, NULL);
    if (rc != SQLITE_OK)
    {
        LOG_E("begin transaction:%d", rc);
        goto __db_begin_fail;
    }
    if (exec_sqls)
    {
        rc = (*exec_sqls)(db, arg);
    }
    else
    {
        rc = SQLITE_ERROR;
    }
    if ((rc != SQLITE_OK) && (rc != SQLITE_DONE))
    {
        LOG_E("prepare error,rc=%d", rc);
        goto __db_exec_fail;
    }

    rc = sqlite3_exec(db, "commit transaction", 0, 0, NULL);
    if (rc)
    {
        LOG_E("commit transaction:%d", rc);
        goto __db_exec_fail;
    }
    goto __db_exec_ok;

__db_exec_fail:
    if (sqlite3_exec(db, "rollback transaction", 0, 0, NULL))
    {
        LOG_E("rollback transaction:error");
    }

__db_begin_fail:
    LOG_E("db operator failed,rc=%d", rc);

__db_exec_ok:
    sqlite3_close(db);
    rt_mutex_release(db_mutex_lock);
    return rc;
}

static int db_get_count(sqlite3_stmt *stmt, void *arg)
{
    int ret, *count = arg;
    ret = sqlite3_step(stmt);
    if (ret != SQLITE_ROW)
    {
        return SQLITE_EMPTY;
    }
    *count = db_stmt_get_int(stmt, 0);
    return SQLITE_OK;
}

/**
 * This function will return the number of records returned by a select query.
 * This function only gets the 1st row of the 1st column.
 *
 * @param sql the SQL statement SELECT COUNT() FROM .
 * @return  >=0:the count ,<0: fail.
 */
int db_query_count_result(const char *sql)
{
    int ret, count = 0;
    ret = db_query_by_varpara(sql, db_get_count, &count, NULL);
    if (ret == SQLITE_OK)
    {
        return count;
    }
    return -RT_ERROR;
}

/**
 * This function will get the blob from the "index" colum.
 *
 * @param stmt the SQL statement returned by the function sqlite3_step().
 * @param index the colum index.the first colum's index value is 0.
 * @param out the output buffer.the result will put in this buffer.
 * @return  >=0:the result length ,<0: fail.
 */
int db_stmt_get_blob(sqlite3_stmt *stmt, int index, unsigned char *out)
{
    const char *pdata = sqlite3_column_blob(stmt, index);
    int len = sqlite3_column_bytes(stmt, index);
    if (pdata)
    {
        memcpy(out, pdata, len);
        return len;
    }
    return -RT_ERROR;
}

/**
 * This function will get the text from the "index" colum.
 *
 * @param stmt the SQL statement returned by the function sqlite3_step().
 * @param index the colum index.the first colum's index value is 0.
 * @param out the output buffer.the result will put in this buffer.
 * @return  >=0:the result length ,<0: fail.
 */
int db_stmt_get_text(sqlite3_stmt *stmt, int index, char *out)
{
    const unsigned char *pdata = sqlite3_column_text(stmt, index);
    if (pdata)
    {
        int len = strlen((char *)pdata);
        strncpy(out, (char *)pdata, len);
        return len;
    }
    return -RT_ERROR;
}

/**
 * This function will get a integer from the "index" colum.
 *
 * @param stmt the SQL statement returned by the function sqlite3_step().
 * @param index the colum index.the first colum's index value is 0.
 * @return  the result.
 */
int db_stmt_get_int(sqlite3_stmt *stmt, int index)
{
    return sqlite3_column_int(stmt, index);
}

/**
 * This function will get a double precision value from the "index" colum.
 *
 * @param stmt the SQL statement returned by the function sqlite3_step().
 * @param index the colum index.the first colum's index value is 0.
 * @return  the result.
 */
double db_stmt_get_double(sqlite3_stmt *stmt, int index)
{
    return sqlite3_column_double(stmt, index);
}

/**
 * This function will check a table exist or not by table name.
 *
 * @param tbl_name the table name.
 * @return >0:existed; ==0:not existed; <0:ERROR
 */
int db_table_is_exist(const char *tbl_name)
{
    char sqlstr[DB_SQL_MAX_LEN];
    int cnt = 0;
    if (tbl_name == RT_NULL)
    {
        return -RT_ERROR;
    }
    rt_snprintf(sqlstr, DB_SQL_MAX_LEN, "select count(*) from sqlite_master where type = 'table' and name = '%s';", tbl_name);
    cnt = db_query_count_result(sqlstr);
    if (cnt > 0)
    {
        return cnt;
    }
    return -RT_ERROR;
}

/**
 * This function will connect DB
 *
 * @param name the DB filename.
 * @return RT_EOK:success
 *         -RT_ERROR:the input name is too long
 */
int db_connect(char *name)
{
    int32_t len = 0;
    rt_mutex_take(db_mutex_lock, RT_WAITING_FOREVER);
    len = rt_strnlen(name, PKG_SQLITE_DB_NAME_MAX_LEN + 1);
    if (len >= PKG_SQLITE_DB_NAME_MAX_LEN + 1)
    {
        LOG_E("the database name '(%s)' lengh is too long(max:%d).", name, PKG_SQLITE_DB_NAME_MAX_LEN);
        rt_mutex_release(db_mutex_lock);
        return -RT_ERROR;
    }
    rt_strncpy(db_name, name, len);
    db_name[len] = '\0';
    return RT_EOK;
}
/**
 * This function will disconnect DB
 *
 * @param name the DB filename.
 * @return RT_EOK:success
 *         -RT_ERROR:the input name is too long
 */
int db_disconnect(char *name)
{
    int32_t len = 0;
    rt_mutex_release(db_mutex_lock);
    rt_strncpy(db_name, DEFAULT_DB_NAME, strlen(DEFAULT_DB_NAME));
    db_name[len] = '\0';
    return RT_EOK;
}

/**
 * This function will connect DB
 *
 * @param name the DB filename.
 * @return RT_EOK:success
 *         -RT_ERROR:the input name is too long
 */
int db_set_name(char *name)
{
    int32_t len = 0;
    rt_mutex_take(db_mutex_lock, RT_WAITING_FOREVER);
    len = rt_strnlen(name, PKG_SQLITE_DB_NAME_MAX_LEN + 1);
    if (len >= PKG_SQLITE_DB_NAME_MAX_LEN + 1)
    {
        LOG_E("the database name '(%s)' lengh is too long(max:%d).", name, PKG_SQLITE_DB_NAME_MAX_LEN);
        rt_mutex_release(db_mutex_lock);
        return -RT_ERROR;
    }
    rt_strncpy(db_name, name, len);
    db_name[len] = '\0';
    rt_mutex_release(db_mutex_lock);
    return RT_EOK;
}

/**
 * This function will get the current DB filename
 *
 * @return the current DB filename
 *
 */
char *db_get_name(void)
{
    static char name[PKG_SQLITE_DB_NAME_MAX_LEN + 1];
    size_t len = rt_strlen(db_name);
    rt_strncpy(name, db_name, len);
    name[len] = '\0';
    return name;
}
