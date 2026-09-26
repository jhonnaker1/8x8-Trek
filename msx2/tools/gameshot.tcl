# Boot the game disk and photograph VRAM at the times in $SHOT_TIMES, typing
# $SHOT_KEYS (time:text pairs) on the way. Pictures come from tools/vram2png.py,
# not openMSX's renderer.
set save_settings_on_exit off
set throttle off
set dir $::env(OUTDIR)
proc dump {t} {
    set f [open "$::dir/vram_$t.bin" wb]; fconfigure $f -translation binary
    puts -nonewline $f [debug read_block VRAM 0 54272]; close $f
    set f [open "$::dir/pal_$t.bin" wb]; fconfigure $f -translation binary
    puts -nonewline $f [debug read_block {VDP palette} 0 32]; close $f
}
foreach p [split $::env(SHOT_KEYS) ","] {
    if {$p eq ""} continue
    lassign [split $p ":"] t k
    # SHOT_KEYS comes through the environment, so "\r" is two characters
    # until subst makes it a RETURN -- the first title-screen RETURN typed a
    # backslash and an r.
    after time $t [list type [subst -nocommands -novariables $k]]
}
set last 0
foreach t [split $::env(SHOT_TIMES) ","] {
    after time $t [list dump $t]
    if {$t > $last} { set last $t }
}
# THE STACK, MEASURED ON THE RUNNING GAME (STACK_FROM set): at the game's own
# entry -- ($0006) = $DB06, which MSXDOS2.SYS's entry is not -- fill
# everything from the image end to the stack top with a sentinel, before
# crt0 runs a single instruction. At the end, the lowest byte no longer the
# sentinel is how deep the stack went. This region is page 3, RAM whatever
# page 0 holds, so poke and peek are safe here as they were not in page 0.
set ::fill 0xE5
proc stack_fill {} {
    if {[peek16 6] != 0xDB06} return
    for {set a $::env(STACK_FROM)} {$a < 0xDB06} {incr a} { poke $a $::fill }
    set ::filled 1
}
proc stack_report {} {
    set f [open "$::dir/stack.txt" w]
    if {![info exists ::filled]} { puts $f "NOT FILLED -- the entry breakpoint never fired"; close $f; return }
    set a $::env(STACK_FROM)
    while {$a < 0xDB06 && [peek $a] == $::fill} { incr a }
    puts $f [format "stack reached \$%04X: %d bytes below \$DB06; %d of the %d between image and stack top never touched" \
        $a [expr {0xDB06 - $a}] [expr {$a - $::env(STACK_FROM)}] [expr {0xDB06 - $::env(STACK_FROM)}]]
    close $f
}
if {[info exists ::env(STACK_FROM)] && $::env(STACK_FROM) ne ""} {
    debug set_bp 0x0100 {} stack_fill
}
after time [expr {$last + 0.5}] {
    if {[info exists ::env(STACK_FROM)] && $::env(STACK_FROM) ne ""} stack_report
    close [open "$::dir/gameshot_done" w]; exit
}
