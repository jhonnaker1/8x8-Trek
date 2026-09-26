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
after time [expr {$last + 0.5}] { close [open "$::dir/gameshot_done" w]; exit }
