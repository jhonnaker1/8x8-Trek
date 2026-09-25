# Drive STKTEST.COM, installed as the shell (it starts ~10.2s): type keys
# while its interrupt phase runs, so the BIOS handler is measured doing
# keyboard work, then keep the screen once DONE.
# Sourced over the control channel before power-on, as tools/listen.tcl is.
set save_settings_on_exit off
set throttle off
set dir $::env(OUTDIR)
file delete -force "$dir/stktest.txt"
# Phase 1 (five seconds of interrupts) runs from ~10.5s; these land in it.
# The one at 30s releases the blocking CHGET phase, which waits for it.
foreach t {11.5 12.5 13.5 14.5} { after time $t { type "ab" } }
after time 30 { type "z" }
proc poll {} {
    # get_screen FAILS outside a text mode, and the test spends its first
    # phases in SCREEN 7 -- an uncaught error here ended the polling once.
    if {![catch {get_screen} scr] && [string first "DONE" $scr] >= 0} {
        set f [open "$::dir/stktest.txt" w]
        puts $f [string trimright [get_screen]]
        close $f
        exit
    }
    after time 1 poll
}
after time 10 poll
after time 120 { set f [open "$::dir/stktest.txt" w]; puts $f "TIMEOUT\n[get_screen]"; close $f; exit }
