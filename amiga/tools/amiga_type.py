#!/usr/bin/env python3
"""Type at the Amiga's shell through Amiberry's IPC socket, and screenshot.

TWO THINGS NOTHING DOCUMENTS. The protocol is TAB-SEPARATED -- a
space-separated command comes back `ERROR Unknown command`, which reads
exactly like an unsupported feature rather than a syntax error. And SEND_KEY
takes RAW AMIGA KEYCODES with a separate press and release, not characters,
so a string has to be spelled out through the table below.
"""
import socket, sys, time

SOCK = "/tmp/amiberry.sock"

# Raw keycodes, the Amiga keyboard matrix. Only what a build/run session types.
KEY = {
    '`':0x00,'1':0x01,'2':0x02,'3':0x03,'4':0x04,'5':0x05,'6':0x06,'7':0x07,
    '8':0x08,'9':0x09,'0':0x0A,'-':0x0B,'=':0x0C,'\\':0x0D,
    'q':0x10,'w':0x11,'e':0x12,'r':0x13,'t':0x14,'y':0x15,'u':0x16,'i':0x17,
    'o':0x18,'p':0x19,'[':0x1A,']':0x1B,
    'a':0x20,'s':0x21,'d':0x22,'f':0x23,'g':0x24,'h':0x25,'j':0x26,'k':0x27,
    'l':0x28,';':0x29,"'":0x2A,
    'z':0x31,'x':0x32,'c':0x33,'v':0x34,'b':0x35,'n':0x36,'m':0x37,
    ',':0x38,'.':0x39,'/':0x3A,
    ' ':0x40,'\b':0x41,'\t':0x42,'\n':0x44,
}
SHIFTED = {':':';','*':'8','?':'/','"':"'",'+':'=','_':'-','(':'9',')':'0',
           '!':'1','<':',','>':'.'}
LSHIFT = 0x60


def cmd(text, wait=0.25):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(5); s.connect(SOCK)
    s.sendall((text + "\n").encode())
    time.sleep(wait)
    out = b""
    try:
        while True:
            b = s.recv(65536)
            if not b:
                break
            out += b
    except socket.timeout:
        pass
    s.close()
    return out.decode(errors="replace").strip()


def key(code, state):
    cmd("SEND_KEY\t%d\t%d" % (code, state), wait=0.04)


def type_text(text):
    for ch in text:
        low = ch.lower()
        shift = ch.isupper() or ch in SHIFTED
        if ch in SHIFTED:
            low = SHIFTED[ch]
        if low not in KEY:
            sys.exit("amiga_type: no keycode for %r" % ch)
        if shift:
            key(LSHIFT, 1)
        key(KEY[low], 1)
        key(KEY[low], 0)
        if shift:
            key(LSHIFT, 0)
        time.sleep(0.03)


if __name__ == "__main__":
    if sys.argv[1] == "shot":
        print(cmd("SCREENSHOT\t" + sys.argv[2]))
    else:
        type_text(" ".join(sys.argv[1:]) + "\n")
        print("typed")
