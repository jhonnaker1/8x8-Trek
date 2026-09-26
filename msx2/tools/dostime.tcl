# Read DOSTIME's screen once it says DONE (text mode throughout).
set save_settings_on_exit off
set throttle off
set dir $::env(OUTDIR)
file delete -force "$dir/dostime.txt"
proc poll {} {
    if {![catch {get_screen} s] && [string first "DONE" $s] >= 0} {
        set f [open "$::dir/dostime.txt" w]; puts $f [string trimright $s]; close $f; exit
    }
    after time 1 poll
}
after time 5 poll
after time 600 { set f [open "$::dir/dostime.txt" w]; puts $f "TIMEOUT\n[get_screen]"; close $f; exit }
