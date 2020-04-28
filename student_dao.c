/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-03-10     lizhen9880   first version
 */

#include <rtthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dfs_posix.h>
#include "sqlite3.h"
#include "dbhelper.h"
#include "student_dao.h"

#define DBG_ENABLE
#define DBG_SECTION_NAME "app.student_dao"
#define DBG_LEVEL DBG_INFO
#define DBG_COLOR
#include <rtdbg.h>

static int student_insert_bind(sqlite3_stmt *stmt, int index, void *arg)
{
    int rc;
    rt_list_t *h = arg, *pos, *n;
    student_t *s = RT_NULL;
    rt_list_for_each_safe(pos, n, h)
    {
        s = rt_list_entry(pos, student_t, list);
        sqlite3_reset(stmt);                                        //reset the stmt
        sqlite3_bind_text(stmt, 1, s->name, strlen(s->name), NULL); //bind the 1st data,is a string
        sqlite3_bind_int(stmt, 2, s->score);                        //bind the 1st data,is a int
        rc = sqlite3_step(stmt);                                    //execute the stmt by step
    }

    if (rc != SQLITE_DONE)
        return rc;
    return SQLITE_OK;
}
int student_add(rt_list_t *h)
{
    return db_nonquery_operator("insert into student(name,score) values (?,?);", student_insert_bind, h);
}

int student_del(int id)
{
    return db_nonquery_by_varpara("delete from student where id=?;", "%d", id);
}

int student_del_all(void)
{
    return db_nonquery_operator("delete from student;", 0, 0);
}

static int student_update_bind(sqlite3_stmt *stmt, int index, void *arg)
{
    int rc;
    student_t *s = arg;
    sqlite3_bind_text(stmt, 1, s->name, strlen(s->name), NULL);
    sqlite3_bind_int(stmt, 2, s->score);
    sqlite3_bind_int(stmt, 3, s->id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
        return rc;
    return SQLITE_OK;
}
int student_update(student_t *s)
{
    return db_nonquery_operator("update student set name=?,score=? where id=?;", student_update_bind, s);
}

static int student_create(sqlite3_stmt *stmt, void *arg)
{
    student_t *s = arg;
    int ret = sqlite3_step(stmt);
    if (ret != SQLITE_ROW)
    {
        return 0;
    }
    else
    {
        s->id = db_stmt_get_int(stmt, 0);
        db_stmt_get_text(stmt, 1, s->name);
        s->score = db_stmt_get_int(stmt, 2);
    }
    return ret;
}

int student_get_by_id(student_t *s, int id)
{
    int res = db_query_by_varpara("select * from student where id=?;", student_create, s, "%d", id);
    return res;
}

void student_free_list(rt_list_t *h)
{
    rt_list_t *head = h, *pos, *n;
    student_t *p = RT_NULL;
    rt_list_for_each_safe(pos, n, head)
    {
        p = rt_list_entry(pos, student_t, list);
        rt_free(p);
    }
    rt_free(head);
}

void student_print_list(rt_list_t *q)
{
    student_t *s = NULL;
    for (s = rt_list_entry((q)->next, student_t, list);
         &s->list != (q);
         s = rt_list_entry(s->list.next, student_t, list))
    {
        rt_kprintf("id:%d\tname:%s\tscore:%d\n", s->id, s->name, s->score);
    }
}

static int student_create_queue(sqlite3_stmt *stmt, void *arg)
{
    rt_list_t *q = arg;
    student_t *s;
    int ret, count = 0;
    ret = sqlite3_step(stmt);
    if (ret != SQLITE_ROW)
    {
        return 0;
    }
    do
    {
        s = rt_calloc(sizeof(student_t), 1);
        if (!s)
        {
            LOG_E("No enough memory!");
            goto __create_student_fail;
        }
        s->id = db_stmt_get_int(stmt, 0);
        db_stmt_get_text(stmt, 1, s->name);
        s->score = db_stmt_get_int(stmt, 2);
        rt_list_insert_before(q, &(s->list));
        count++;
    } while ((ret = sqlite3_step(stmt)) == SQLITE_ROW);
    return count;
__create_student_fail:
    return -1;
}

int student_get_all(rt_list_t *q)
{
    return db_query_by_varpara("select * from student;", student_create_queue, q, RT_NULL);
}

static void list_all(void)
{
    rt_kprintf("test get all students\n");
    rt_list_t *h = rt_calloc(sizeof(student_t), 1);
    rt_list_init(h);
    int ret = student_get_all(h);
    if (ret >= 0)
    {
        student_print_list(h);
        rt_kprintf("record(s):%d\n", ret);
    }
    else
    {
        rt_kprintf("Get students information failed");
    }
    student_free_list(h);
    return;
}

int student_get_by_score(rt_list_t *h, int ls, int hs, enum order_type order)
{
    char sql[128];

    rt_snprintf(sql, 128, "select * from student where score between %d and %d ORDER BY score %s;", ls, hs, order == ASC ? "ASC" : "DESC");
    return db_query_by_varpara(sql, student_create_queue, h, RT_NULL);
}

static void list_by_score(int ls, int hs, enum order_type order)
{
    rt_list_t *h = rt_calloc(sizeof(rt_list_t), 1);
    rt_list_init(h);
    rt_kprintf("the student list of score between %d and %d:\n", ls, hs);
    int ret = student_get_by_score(h, ls, hs, order);
    if (ret >= 0)
    {
        student_print_list(h);
        rt_kprintf("record(s):%d\n", ret);
    }
    else
    {
        LOG_E("Get students information failed!");
    }
    student_free_list(h);
    return;
}

static void stu(uint8_t argc, char **argv)
{
    if (argc < 2)
    {
        list_all();
        return;
    }
    else
    {
        char *cmd = argv[1];
        int rand = 0;

        if (rt_strcmp(cmd, "add") == 0)
        {
            int i = 0, count = 0;
            if (argc >= 3)
            {
                count = atol(argv[2]);
            }
            if (count == 0)
            {
                count = 1;
            }
            rt_tick_t ticks = rt_tick_get();
            rand = ticks;
            rt_list_t *h = (rt_list_t *)rt_calloc(1, sizeof(rt_list_t));
            rt_list_init(h);
            for (i = 0; i < count; i++)
            {
                student_t *s = (student_t *)rt_calloc(1, sizeof(student_t));
                rand += i;
                rand %= 99999;
                s->score = (rand % 81) + 20;
                sprintf(s->name, "Student%d", rand);
                rt_list_insert_before(h, &(s->list));
            }
            int res = student_add(h);
            student_free_list(h);
            if (res != SQLITE_OK)
            {
                LOG_E("add failed!");
            }
            else
            {
                ticks = rt_tick_get() - ticks;
                rt_kprintf("Insert %d record(s): %dms, speed: %dms/record\n", count,
                           ticks * 1000 / RT_TICK_PER_SECOND, ticks * 1000 / RT_TICK_PER_SECOND / count);
            }
        }
        else if (rt_strcmp(cmd, "del") == 0)
        {
            if (argc == 2)
            {
                if (student_del_all() == SQLITE_OK)
                {
                    rt_kprintf("Del all record success!\n");
                }
                else
                {
                    rt_kprintf("Del all record failed!\n");
                }
            }
            else
            {
                rt_uint32_t id = atol(argv[2]);
                if (student_del(id) == SQLITE_OK)
                {
                    rt_kprintf("Del record success with id:%d\n", id);
                }
                else
                {
                    rt_kprintf("Del record failed with id:%d\n", id);
                }
            }
        }
        else if (rt_strcmp(cmd, "update") == 0)
        {
            /* update student record by id */
            if (argc >= 5)
            {
                student_t *s = rt_calloc(sizeof(student_t), 1);
                s->id = atol(argv[2]);
                rt_strncpy(s->name, argv[3], rt_strlen(argv[3]));
                s->score = atol(argv[4]);
                if (student_update(s) == SQLITE_OK)
                {
                    rt_kprintf("update record success!\n");
                }
                else
                {
                    rt_kprintf("update record failed!\n");
                }
                rt_free(s);
            }
            else
            {
                rt_kprintf("usage: stu update id name score\n");
            }
        }
        else if (rt_strcmp(cmd, "score") == 0)
        {
            /* query student's score between LOW and HIGH. */
            if (argc >= 4)
            {
                enum order_type order = ASC;
                int ls = atol(argv[2]);
                int hs = atol(argv[3]);
                if (rt_strcmp(argv[4], "-d") == 0)
                {
                    order = DESC;
                }
                list_by_score(ls, hs, order);
            }
            else
            {
                rt_kprintf("usage: stu score LOW HIGH [OPTION]\n"
                           "desc:query student's score between LOW and HIGH.\n"
                           "OPTION(default ascending):\n    -a:ascending\n    -d:descending\n"
                           "e.g: stu score 60 100 or stu score -d 60 100\n");
            }
        }
        else
        {
            student_t *s = rt_calloc(sizeof(student_t), 1);
            rt_uint32_t id = atol(argv[1]);
            if (student_get_by_id(s, id) > 0)
            {
                rt_kprintf("id:%d\t\tname:%s\tscore:%d\n", s->id, s->name, s->score);
            }
            else
            {
                rt_kprintf("no record with id:%d\n", id);
            }
            rt_free(s);
        }
    }
}
MSH_CMD_EXPORT(stu, student add del update query);

static int create_student_tbl(void)
{
    int fd = 0;
    db_set_name("/stu_info.db");
    fd = open(db_get_name(), O_RDONLY);
    rt_kprintf(db_get_name());
    if (fd < 0)
    {
        /* there is not the .db file.create db and table */
        const char *sql = "CREATE TABLE student(id INTEGER PRIMARY KEY AUTOINCREMENT,name varchar(32) NOT NULL,score INT NOT NULL);";
        return db_create_database(sql);
    }
    else if (db_table_is_exist("student") > 0)
    {
        /* there is the table int db.close the db. */
        close(fd);
        LOG_I("The table has already existed!\n");
        return RT_EOK;
    }
    else
    {
        /* there is not the table int db.create the table */
        const char *sql = "CREATE TABLE student(id INTEGER PRIMARY KEY AUTOINCREMENT,name varchar(32) NOT NULL,score INT NOT NULL);";
        return db_create_database(sql);
    }
}
MSH_CMD_EXPORT(create_student_tbl, create sqlite db);
