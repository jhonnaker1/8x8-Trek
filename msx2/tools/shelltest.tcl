# Read the screen once SHELLTEST, installed as COMMAND2.COM, has spoken.
set save_settings_on_exit off
set throttle off
set dir $::env(OUTDIR)
file delete -force "$dir/shelltest.txt"
proc poll {} {
    if {![catch {get_screen} s] && [regexp {SHELL (OK|FAILED)|NOT THE SHELL} $s]} {
        set f [open "$::dir/shelltest.txt" w]; puts $f [string trimright $s]; close $f; exit
    }
    after time 1 poll
}
after time 5 poll
after time 60 { set f [open "$::dir/shelltest.txt" w]; puts $f "TIMEOUT"; close $f; exit }
