5 rem the copier at $c000 moves whole blocks (tools/copier.s): loaded once,
6 rem and a jmp at $c000 is how this line knows it has been.
10 if peek(49152)<>76 then load"copier",8,1
15 poke 55,0:poke 56,64:clr
20 rem install an app bundle from device 8 onto the c64 os disk (10),
30 rem read each back, and report its length and sum for the mac to
40 rem compare with the source (tools/rig.py).
50 rem after commodore-uno/c64os-llvm/tools/install.bas.
55 open4,8,4,"result,s,w"
60 open15,10,15
70 print#15,"cd//os/applications"
80 print#15,"md:{APP}"
90 print#15,"cd//os/applications/{APP}"
100 read nf:for f=1 to nf:read n$,t$,xl,xs:a=0
105 rem each file is written, read back and checked against the length and
106 rem sum the mac computed, and written again if they differ: the cmd hd in
107 rem vice now and then reads a file back one byte long with an error.
110 a=a+1:print#15,"s:"+n$
120 open1,8,2,n$+","+t$+",r":open2,10,2,n$+","+t$+",w"
125 input#15,d,d$,d1,d2:if d then print#4,n$;" open for write: ";d;d$
130 sys 49152:if peek(50165) then print#4,n$;" copy: channel";peek(50165)
150 close1:close2
160 open3,10,3,n$+","+t$+",r"
165 input#15,d,d$,d1,d2:if d then print#4,n$;" open to verify: ";d;d$
170 sys 49155
190 m=peek(50160)+256*peek(50161)+65536*peek(50166):e=peek(50162):c=peek(50163)+256*peek(50164)
210 close3
215 if (m<>xl or c<>xs or e) and a<3 then 110
220 print#4,n$;" len";m;"sum";c;"err";e;"tries";a
240 next
330 print#15,"cd//":close15
335 print#4,"done":close4
340 print "done"
350 data {FILES}
