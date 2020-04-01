from building import *

cwd = GetCurrentDir()
src = ['sqlite3.c']
src += ['dbhelper.c']
if GetDepend('PKG_SQLITE_DAO_EXAMPLE'):
    src += Glob('student_dao.c')

CPPPATH = [cwd]
group = DefineGroup('sqlite', src, depend = ['RT_USING_DFS', 'PKG_USING_SQLITE'], CPPPATH = CPPPATH)

Return('group')
