# A STATISTICAL PROFILE of the running game: every PROF_STEP seconds of
# emulated time from PROF_FROM to PROF_TO, record the PC and which primary
# slot page 0 is on (port $A8 bits 0-1). The slot is not decoration: our code
# lives in page 0 too, and a PC there is the BIOS ROM or the game depending
# on it (#62). tools/profile.py maps the samples to functions.
set save_settings_on_exit off
set throttle off
set dir $::env(OUTDIR)
set ::out [open "$dir/profile.txt" w]
foreach p [split $::env(SHOT_KEYS) ","] {
    if {$p eq ""} continue
    lassign [split $p ":"] t k
    after time $t [list type [subst -nocommands -novariables $k]]
}
proc sample {} {
    puts $::out [format "%.4f %04X %d" [machine_info time] [reg PC] [expr {[debug read ioports 0xA8] & 3}]]
    if {[machine_info time] < $::env(PROF_TO)} { after time $::env(PROF_STEP) sample } else {
        close $::out; close [open "$::dir/profile_done" w]; exit
    }
}
after time $::env(PROF_FROM) sample
