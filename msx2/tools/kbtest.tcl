# Type into KBTEST.COM through the emulated key matrix. It is installed as
# the shell and starts ~10.2s; it waits three seconds, then calls kb_init.
set save_settings_on_exit off
set throttle off
set dir $::env(OUTDIR)
file delete -force "$dir/kbtest.txt"
# Row/bit of the keys `type` cannot spell, from the MSX matrix: row 7 bit 2
# ESC, bit 5 BS, bit 7 RETURN; row 8 bit 5 UP, bit 6 DOWN; row 1 bit 2 minus.
proc press {row bit at} {
    after time $at "keymatrixdown $row [expr {1 << $bit}]"
    after time [expr {$at + 0.1}] "keymatrixup $row [expr {1 << $bit}]"
}
after time 11.5 { type "x" }
set t 14.0
foreach k {a Z 1 , . q " "} { after time $t [list type $k]; set t [expr {$t + 0.5}] }
# NOT `type -`: a leading "-" is where `type` looks for its OWN options, and
# the first run typed nothing at all there -- the ring went straight from
# SPACE to RETURN. Row 1 bit 2 is the minus key.
press 1 2 $t; set t [expr {$t + 0.5}]
press 7 7 $t; set t [expr {$t + 0.5}]
press 7 2 $t; set t [expr {$t + 0.5}]
press 7 5 $t; set t [expr {$t + 0.5}]
press 8 5 $t; set t [expr {$t + 0.5}]
press 8 6 $t
proc poll {} {
    if {![catch {get_screen} s] && [string first "DONE" $s] >= 0} {
        set f [open "$::dir/kbtest.txt" w]; puts $f [string trimright $s]; close $f; exit
    }
    after time 1 poll
}
after time 12 poll
# ON A TIMEOUT, SAY WHERE IT STOPPED: the keys it has, whether the loop is
# running, and what the ring holds. Addresses from build/kbtest.map, which
# the Makefile passes in.
after time 60 {
    set f [open "$::dir/kbtest.txt" w]
    puts $f "TIMEOUT  PC=[format %04X [reg PC]]"
    set g {}; for {set i 0} {$i < 13} {incr i} { lappend g [peek [expr {$::env(KB_GOT) + $i}]] }
    puts $f "got      $g"
    puts $f "entropy  [peek16 $::env(KB_ENT)]"
    puts $f [format "GETPNT %04X PUTPNT %04X" [peek16 0xF3FA] [peek16 0xF3F8]]
    set r {}; for {set a 0xFBF0} {$a < 0xFC18} {incr a} { lappend r [format %02X [peek $a]] }
    puts $f "KEYBUF   $r"
    if {![catch {get_screen} s]} { puts $f $s }
    close $f; exit
}
