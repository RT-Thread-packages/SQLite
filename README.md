# SQLite for RT-Thread

## 简介 

SQLite 是一个小型，快速，独立的高可靠性全功能 SQL 数据库引擎。SQLite 是全世界部署最广泛的 SQL 数据库引擎。它内置在几乎所有的移动电话、大多数计算机以及部分嵌入式设备中，并被捆绑在人们每天使用的无数其他应用程序中。

## 文件说明

| 名称                     | 说明                                                             |
| ------------------------ | ---------------------------------------------------------------- |
| sqlite3.c                | sqlite3源文件                                                    |
| sqlite3.h                | sqlite3头文件                                                    |
| sqlite_config_rtthread.h | sqlite3在rt-thread上的配置文件                                   |
| rtthread_io_methods.c    | rt-thread为sqlite提供的底层文件IO接口                            |
| rtthread_mutex.c         | rt-thread为sqlite提供的互斥量操作接口                            |
| rtthread_vfs.c           | rt-thread为sqlite提供的VFS(虚拟文件系统)接口                     |
| dbhelper.c               | sqlite3操作接口封装，简化应用                                    |
| dbhelper.h               | dbhelper头文件，向外部声明封装后的接口，供用户调用               |
| student_dao.c            | 简单的DAO层例程，简单展示了对dbhelper的使用方法                  |
| student_dao.h            | 数据访问对象对外接口声明，线程可通过调用这些接口完成对该表的操作 |

## 获取

在ENV中配置如下

```c
RT-Thread online packages  ---> 
    system packages  ---> 
        --- SQLite: a self-contained, high-reliability, embedded, full-featured, public-domain, SQL database engine.
        (1024) SQL statements max length
        [*]   Enable example
```

配置项说明：

| 名称                      | 说明                                                |
| ------------------------- | --------------------------------------------------- |
| SQL statements max length | SQL语句最大长度，请根据实际业务需求设置。           |
| Enable example            | 选择是否使能DAO层例程，例程是模拟了学生成绩录入查询 |

## 依赖
- RT-Thread 3.X+
- DFS组件
## dbhelp接口说明

dbhelp是对sqlite3操作接口的封装，目的是使用户更加简单地操作sqlite。

### 数据库文件完整路径
数据库文件的默认存放完整路径是"/rt.db"，用户可根据实际需求在dbhelper.h中修改。
```c
#define DB_NAME "/rt.db"
```
### 初始化
dbhelp初始化，其中包含了sqlite的初始化及互斥量创建。用户无需再对数据库及锁初始化。
```c
int db_helper_init(void);
```
| 参数      | 说明       |
| --------- | ---------- |
| void      |            |
| 返回      |            |
| RT_EOK    | 初始化成功 |
| -RT_ERROR | 初始化失败 |

### 数据库创建
```c
int db_create_database(const char *sqlstr);
```
| 参数   | 说明            |
| ------ | --------------- |
| sqlstr | 创建表的SQL语句 |
| 返回   |                 |
| 0      | 成功            |
| 非0    | 失败            |
输入形参sqlstr应当是一条创建表的SQL语句，例：
```c
const char *sql = "CREATE TABLE student(id INTEGER PRIMARY KEY AUTOINCREMENT,name varchar(32) NOT NULL,score INT NOT NULL);";
return db_create_database(sql);
```

### 可进行数据绑定的非查询操作
用于非查询的操作，可通过回调进行数据绑定，db_nonquery_operator中开启了事务功能，支持操作失败后回滚。
```c
int db_nonquery_operator(const char *sqlstr, int (*bind)(sqlite3_stmt *, int index, void *arg), void *param);
```
| 参数   | 说明                                                         |
| ------ | ------------------------------------------------------------ |
| sqlstr | 非查询类SQL语句，如果有多个SQL语句要执行，请用;将SQL语句分开 |
| bind   | 数据绑定回调                                                 |
| param  | 在内部将赋值给bind的输入参数void *arg                        |
| 返回   |                                                              |
| 0      | 成功                                                         |
| 非0    | 失败                                                         |
注：该接口请勿用于查询类操作。
bind需要用户实现：
| 参数                   | 说明                                                                 |
| ---------------------- | -------------------------------------------------------------------- |
| stmt                   | sqlite3_stmt预备语句对象                                             |
| index                  | 如果有多条SQL语句要执行，index表示第index条SQL语句，注意index从1开始 |
| arg                    | 即db_nonquery_operator的四三个形参void *param                        |
| 返回                   |                                                                      |
| SQLITE_OK或SQLITE_DONE | 成功                                                                 |
| 其他                   | 失败(如果用户提供的bind返回失败，db_nonquery_operator将执行回滚操作) |


普通SQL语句在执行时是需要解析、编译、执行的，而sqlite3_stmt结构是已经通过sqlite3_prepare函数对sql语句解析和编译了的，而在SQL语句中在要绑定数据的位置放置?作为占位符，即可通过数据绑定接口sqlite3_bind_*(此处*为通配符，取值为int，double，text等，详情请见sqlite文档)。这种在批量操作时，由于绕开了解释编译的过程，直接在sqlite3_stmt的基础上对绑定值进行修改，因而会使整体操作效率势大大提升。具体应用请见[student_dao.c](./student_dao.c)

### 带可变参数的非查询操作

```c
int db_nonquery_by_varpara(const char *sql, const char *fmt, ...);
```
| 参数 | 说明                                                              |
| ---- | ----------------------------------------------------------------- |
| sql  | 非查询类SQL语句                                                   |
| fmt  | 格式符，配合后面的可变参数使用，用法类似printf，只支持%d,%f,%s,%x |
| ...  | 可变形参                                                          |
| 返回 |                                                                   |
| 0    | 成功                                                              |
| 非0  | 失败                                                              |
注：该接口请勿用于查询类操作。
例：
```c
int student_del(int id)
{
    return db_nonquery_by_varpara("delete from student where id=?;", "%d", id);
}
```
SQL语句"delete from student where id=?;"中，“?”为占位符，id将会按照%d的格式被绑定到通过对前述SQL语句解释编译后的sqlite3_stmt中。

### 开启事务的非查询操作
db_nonquery_transaction开启了事务功能，支持操作失败后回滚。
```c
int db_nonquery_transaction(int (*exec_sqls)(sqlite3 *db, void *arg), void *arg);
```
| 参数      | 说明                                           |
| --------- | ---------------------------------------------- |
| exec_sqls | SQL执行回调                                    |
| arg       | 内部赋值给exec_sqls的void *arg形参作为输入参数 |
| 返回      |                                                |
| 0         | 成功                                           |
| 非0       | 失败                                           |

exec_sqls：
| 参数                   | 说明                                                                         |
| ---------------------- | ---------------------------------------------------------------------------- |
| db                     | 数据库描述符                                                                 |
| arg                    | 来自于db_nonquery_transaction的形参void *arg                                 |
| 返回                   |                                                                              |
| SQLITE_OK或SQLITE_DONE | 成功                                                                         |
| 其他                   | 失败(如果用户提供的exec_sqls返回失败，db_nonquery_transaction将执行回滚操作) |
注：该接口请勿用于查询类操作。
db_nonquery_transaction的自由度比较大，内部开启事务后就开始执行exec_sqls，用户可在exec_sqls通过调用SQLite的接口进行相关的数据库操作。执行完exec_sqls后提交事务。如果有失败情况会回滚。

### 带变参的查询操作
用户可通过sql语句及变参进行查询操作
```c
int db_query_by_varpara(const char *sql, int (*create)(sqlite3_stmt *stmt, void *arg), void *arg, const char *fmt, ...);
```
| 参数                   | 说明                                                              |
| ---------------------- | ----------------------------------------------------------------- |
| sql                    | SQL语句                                                           |
| create                 | 创建用来接收查询结果的数据对象                                    |
| arg                    | 将赋值给create的void *arg作为输入参数                             |
| fmt                    | 格式符，配合后面的可变参数使用，用法类似printf，只支持%d,%f,%s,%x |
| ...                    | 变参                                                              |
| 返回（creat!=NULL）    |                                                                   |
| 返回create的返回值     |                                                                   |
| 返回（create==NULL时） |                                                                   |
| 0                      |                                                                   |

create回调：
| 参数                              | 说明                                                 |
| --------------------------------- | ---------------------------------------------------- |
| stmt                              | sqlite3_stmt预备语句对象                             |
| arg                               | 接收来由db_query_by_varpara的void *arg传递进来的形参 |
| 返回                              |                                                      |
| 返回值将被db_query_by_varpara返回 |                                                      |

用户可通过调用在通过调用db_stmt_get_int，db_stmt_get_text等接口配合sqlite3_step获取查询结果，回调接收不定数量的条目时请注意内存用量，或在查询前先通过db_query_count_result确定条符合条件的条目数量。

### 获取符合条件的结果数量
调用此接口返回结果条目数量，注意，如的SQL语句应为SELECT COUNT为前缀的查询数量的语句。接口中只会获取第一行第一列。
```c
int db_query_count_result(const char *sql);
```
| 参数   | 说明                                                              |
| ------ | ----------------------------------------------------------------- |
| sql    | SQL语句，仅限查询数量语句，如：select count(*) from sqlite_master |
| 返回   |                                                                   |
| 非负值 | 符合条件的条目的数量                                              |
| 负数   | 查询失败                                                          |
例如，查询数据库中表的数量：
```c
nums = db_query_count_result("select count(*) from sqlite_master where type = 'table';");
```

### 在查询结果中获取blob型数据
在查询时获取blob类型数据，blob类型在SQLite 中指二进制大对象，完全根据其输入值存储。
```c
int db_stmt_get_blob(sqlite3_stmt *stmt, int index, unsigned char *out)
```
| 参数   | 说明                                           |
| ------ | ---------------------------------------------- |
| stmt   | sqlite3_stmt预备语句对象                       |
| index  | 数据索引                                       |
| out    | 输出参数，获取到的数据将存入该指针指向的地址中 |
| 返回   |                                                |
| 非负值 | 获取的数据长度                                 |
| 负数   | 查询失败                                       |  |

### 在查询结果中获取text型数据
在查询时获取文本类型数据，text类型值是一个文本字符串。
```c
int db_stmt_get_text(sqlite3_stmt *stmt, int index, char *out)
```
| 参数   | 说明                                           |
| ------ | ---------------------------------------------- |
| stmt   | sqlite3_stmt预备语句对象                       |
| index  | 数据索引                                       |
| out    | 输出参数，获取到的数据将存入该指针指向的地址中 |
| 返回   |                                                |
| 非负值 | 获取的数据长度                                 |
| 负数   | 查询失败                                       |

### 在查询结果中获取int型数据
在查询时获取整型数据
```c
int db_stmt_get_int(sqlite3_stmt *stmt, int index);
```
| 参数      | 说明                     |
| --------- | ------------------------ |
| stmt      | sqlite3_stmt预备语句对象 |
| index     | 数据索引                 |
| 返回      |                          |
| int型变量 | 查询结果                 |

### 在查询结果中获取double型数据
在查询时获取双精度数据
```c
double db_stmt_get_double(sqlite3_stmt *stmt, int index)
```
| 参数         | 说明                     |
| ------------ | ------------------------ |
| stmt         | sqlite3_stmt预备语句对象 |
| index        | 数据索引                 |
| 返回         |                          |
| double型变量 | 查询结果                 |

### 通过表面查询表是否存在
可通过输入表名称来确定该表是否存在。
```c
int db_table_is_exist(const char *tbl_name)
```
| 参数     | 说明     |
| -------- | -------- |
| tbl_name | 表名称   |
| 返回     |          |
| 正值     | 已存在   |
| 0        | 不存在   |
| 负值     | 查询错误 |


### 设置数据库文件名
该文件名应为绝对路径，请使用.db作为扩展名。在对单数据库的使用场景中可用来设置数据库名称。多线程多数据库请使用int db_connect(char *name)和int db_connect(char *name)。
```c
int db_set_name(char *name)
```
| 参数   | 说明                   |
| ------ | ---------------------- |
| name   | 文件名（完整绝对路径） |
| 返回   |                        |
| RT_EOK | 设置成功               |
| <0     | 设置失败               |

### 读取当前数据库文件名
```c
const char *db_get_name(void)
```
| 参数   | 说明     |
| ------ | -------- |
| 返回   |          |
| 字符串 | 设置成功 |
| <0     | 设置失败 |

### 连接数据库
该文件名应为绝对路径，请使用.db作为扩展名，该方法将会设置要打开的数据库文件名并加锁，操作完毕后请调用 int db_disconnect(char *name) 解锁，以便以其他线程操作。适用于多线程操作多数据库场景。如果是多线程，单数据库场景请使用 int db_set_name(char *name) 来修改数据库名称或直接操作数据库（即使用默认数据库名称）。
```c
int db_connect(char *name)
```
| 参数   | 说明                   |
| ------ | ---------------------- |
| name   | 文件名（完整绝对路径） |
| 返回   |                        |
| RT_EOK | 设置成功               |
| <0     | 设置失败               |

### 断开数据库连接
断开数据库连接，数据库名称将恢复为默认，并释放互斥量。

```c
int db_connect(char *name)
```
| 参数   | 说明                   |
| ------ | ---------------------- |
| name   | 文件名（完整绝对路径） |
| 返回   |                        |
| RT_EOK | 设置成功               |
| <0     | 设置失败               |


## DAO层实例
这是一个学生成绩录入查询的DAO(Data Access Object)层示例，可在menuconfig中配置使能。通过此例程可更加详细的了解dbhelper的使用方法。例程配置使能后，可通过命令行实现对student表的操作，具体命令如下：

说明：以下命令中被[]包裹的为必填参数，<>包裹的为非必填参数

### stu add \<num\>
增加\<num\>个学生信息。学生名称和成绩为一定范围内的生成的随机值。如果仅输入```stu add```则默认插入一条学生信息。

### stu del <id>
删除主键为\<id\>的学生成绩信息，如果仅输入```stu del```则删除全部学生信息。例如：
```
stu del 1
```
删除主键为1的学生成绩信息。

### stu update [id] [name] [score]
将主键为[id]的学生的姓名及分数分别修改为[name]和[score]。例如：
```
stu update 1 lizhen9880 100
```
表示将主键为1的条目中学生姓名修改为lizhen9880，分数修改为100。

### stu \<id\>
根据id查询学生成绩并打印，id为该表自增主键，具备唯一性。根据主键查询速度是非常快的。
仅输入 ```stu``` 会查询出所有学生名称成绩并打印,例：
```
stu 3
```
将查询出主键为3的学生成绩信息并打印。

### stu score [ls] [hs] \<-d|-a\>
查询并打印分数在[ls]到[hs]之间的学生成绩单。<br/>
[ls]: 最低分 <br/>
[hs]: 最高分 <br/>
\<-d|-a\>: -d 降序；-a 升序；默认升序
例如：
```
stu score 60 100 -d
```
按照```降序```打印出成绩及格学生的成绩单。

```
stu score 60 100
```
按照```升序```打印出成绩及格学生的成绩单。

## 注意事项
- SQLite资源占用：RAM:250KB+,ROM:310KB+，所以需要有较充足的硬件资源。
- 根据应用场景创建合理的表结构，会提高操作效率。
- 根据应用场合理使用SQL语句，如查询条件，插入方式等。
- 如涉及到多表操作或联表查询，最好使用PowerDesigner等工具合理设计表。

## 相关链接
Env工具获取：[https://www.rt-thread.org/page/download.html](https://www.rt-thread.org/page/download.html)

Env工具使用：[https://www.rt-thread.org/document/site/zh/5chapters/01-chapter_env_manual/#env](https://www.rt-thread.org/document/site/zh/5chapters/01-chapter_env_manual/#env)

SQLite官方文档：[https://www.sqlite.org/docs.html](https://www.sqlite.org/docs.html)
## Q&F
联系人： RT-Thread官方 | [lizhen9880](https://github.com/lizhen9880)<br/>
邮箱：[package_team@rt-thread.com](package_team@rt-thread.com) | [lizhen9880@126.com](lizhen9880@126.com)
