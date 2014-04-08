select e1.ename as "manager", d.dname as "of dept"
from emp e1, manages m, dept d
where 
      e1.eno = m.eno
  and m.dno = d.dno

