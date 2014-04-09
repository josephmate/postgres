explain select 
  e.ename as "employee",
  d.dname as "in dept",
  me.ename as "manager"
from emp e, emp me, manages m, dept d
where 
      me.eno = m.eno
  and m.dno = d.dno
  and e.dno = m.dno

