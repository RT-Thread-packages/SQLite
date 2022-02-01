/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-03-06     lizhen9880   first version
 */

#ifndef __DBHELPER_H__
#define __DBHELPER_H__

#include <sqlite3.h>
#include <rtthread.h>

#define DB_SQL_MAX_LEN PKG_SQLITE_SQL_MAX_LEN

int db_helper_init(void);
int db_create_database(const char *sqlstr);
/**
 * This function will be used for the operating that is not SELECT.It support executing multiple
 * SQL statements.
 *
 * @param sqlstr the SQL statements strings.if there are more than one
 *               statements in the sqlstr to execute,separate them by a semicolon(;).
 * @param bind the callback function supported by user.bind data and call the sqlite3_step function.
 * @param param the parameter for the callback "bind".
 * @return  success or fail.
 */
int db_nonquery_operator(const char *sqlstr, int (*bind)(sqlite3_stmt *, int index, void *arg), void *param);

/**
 * This function will be used for the operating that is not SELECT.The additional
 * arguments following format are formatted and inserted in the resulting string
 * replacing their respective specifiers.
 *
 * @param sql the SQL statement.
 * @param fmt the args format.such as %s string,%d int.
 * @param ... the additional arguments
 * @return  success or fail.
 */
int db_nonquery_by_varpara(const char *sql, const char *fmt, ...);

/**
 * This function will be used for the transaction that is not SELECT.
 *
 * @param exec_sqls the callback function of executing SQL statements.
 * @param arg the parameter for the callback "exec_sqls".
 * @return  success or fail.
 */
int db_nonquery_transaction(int (*exec_sqls)(sqlite3 *db, void *arg), void *arg);

/**
 * This function will be used for the SELECT operating.The additional arguments
 * following format are formatted and inserted in the resulting string replacing
 * their respective specifiers.
 *
 * @param sql the SQL statements.
 * @param create the callback function supported by user.
 * @param arg the parameter for the callback "create".
 * @param fmt the args format.such as %s string,%d int.
 * @param ... the additional arguments
 * @return  success or fail.
 */
int db_query_by_varpara(const char *sql, int (*create)(sqlite3_stmt *stmt, void *arg), void *arg, const char *fmt, ...);

/**
 * This function will return the number of records returned by a select query.
 * This function only gets the 1st row of the 1st column.
 *
 * @param sql the SQL statement SELECT COUNT() FROM .
 * @return  the count or fail.
 */
int db_query_count_result(const char *sql);

/**
 * This function will get the blob from the "index" colum.
 *
 * @param stmt the SQL statement returned by the function sqlite3_step().
 * @param index the colum index.the first colum's index value is 0.
 * @param out the output buffer.the result will put in this buffer.
 * @return  the result length or fail.
 */
int db_stmt_get_blob(sqlite3_stmt *stmt, int index, unsigned char *out);

/**
 * This function will get the text from the "index" colum.
 *
 * @param stmt the SQL statement returned by the function sqlite3_step().
 * @param index the colum index.the first colum's index value is 0.
 * @param out the output buffer.the result will put in this buffer.
 * @return  the result length or fail.
 */
int db_stmt_get_text(sqlite3_stmt *stmt, int index, char *out);

/**
 * This function will get a integer from the "index" colum.
 *
 * @param stmt the SQL statement returned by the function sqlite3_step().
 * @param index the colum index.the first colum's index value is 0.
 * @return  the result.
 */
int db_stmt_get_int(sqlite3_stmt *stmt, int index);

/**
 * This function will get a double precision value from the "index" colum.
 *
 * @param stmt the SQL statement returned by the function sqlite3_step().
 * @param index the colum index.the first colum's index value is 0.
 * @return  the result.
 */
double db_stmt_get_double(sqlite3_stmt *stmt, int index);

/**
 * This function will check a table exist or not by table name.
 *
 * @param tbl_name the table name.
 * @return >0:existed; ==0:not existed; <0:ERROR
 */
int db_table_is_exist(const char *tbl_name);

/**
 * This function will connect DB
 *
 * @param name the DB filename.
 * @return RT_EOK:success
 *         -RT_ERROR:the input name is too long
 */
int db_connect(char *name);

/**
 * This function will disconnect DB
 *
 * @param name the DB filename.
 * @return RT_EOK:success
 *         -RT_ERROR:the input name is too long
 */
int db_disconnect(char *name);

/**
 * This function will get the current DB filename
 *
 * @return the current DB filename
 *
 */
char *db_get_name(void);
#endif
