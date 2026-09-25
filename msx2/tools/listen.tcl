# Record what SNDTEST.COM plays, for tools/listen.py. Sourced over the
# control channel BEFORE power-on (see the Makefile's `listen` rule), so the
# recording starts at t=0 and catches the whole script whatever boot costs.
#
# The recording is in EMULATED time, which is the point: throttle off makes
# the run fast without changing a single sample.
set save_settings_on_exit off
set throttle off
set dir $::env(OUTDIR)
file delete -force "$dir/listen.wav" "$dir/listen.txt"
record start -audioonly -mono "$dir/listen.wav"
after time 60 {
    record stop
    set f [open "$dir/listen.txt" w]
    puts $f [string trimright [get_screen]]
    close $f
    exit
}
after realtime 200 { exit }
