10 rem install an app bundle from device 8 onto the c64 os disk (10),
20 rem compare every byte on the disk with the source, and print "ok"
30 rem and the length or the first difference.
50 rem after commodore-uno/c64os-llvm/tools/install.bas.
60 open15,10,15
70 print#15,"cd//os/applications"
80 print#15,"md:{APP}"
90 print#15,"cd//os/applications/{APP}"
100 for f=1 to 3:read n$,t$
110 print#15,"s:"+n$
120 open1,8,2,n$+","+t$+",r":open2,10,2,n$+","+t$+",w"
130 get#1,a$:s=st:if a$="" then a$=chr$(0)
140 print#2,a$;:if s=0 then 130
150 close1:close2
160 open1,8,2,n$+","+t$+",r":open3,10,3,n$+","+t$+",r":m=0:e=-1
170 get#1,a$:s1=st:get#3,b$:s2=st
180 if a$<>b$ and e<0 then e=m
190 m=m+1:if s1=0 and s2=0 then 170
200 if s1<>s2 and e<0 then e=m
210 close1:close3
220 if e<0 then print n$;" ok";m
230 if e>=0 then print n$;" differs at";e
240 next
330 print#15,"cd//":close15
340 print "done"
350 data main.o,p,menu.m,s,about.t,s
