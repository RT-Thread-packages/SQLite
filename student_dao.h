/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-03-10     lizhen9880   first version
 */

#ifndef __STUDENT_DAO_H__
#define __STUDENT_DAO_H__

#include <rtthread.h>

struct student
{
    unsigned int id;
    char name[32];
    int score;
    rt_list_t list;
};
typedef struct student student_t;

/**  
 * ASC:Ascending 
 * DESC:Descending 
 * */
enum order_type
{
    ASC = 0,
    DESC = 1,
};
int student_get_by_id(student_t *e, int id);
int student_get_by_score(rt_list_t *h, int ls, int hs, enum order_type order);
int student_get_all(rt_list_t *q);
int student_add(rt_list_t *h);
int student_del(int id);
int student_del_all(void);
int student_update(student_t *e);
void student_free_list(rt_list_t *h);
void student_print_list(rt_list_t *q);

#endif
