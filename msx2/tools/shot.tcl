# Screenshot the booted disk -- and log VRAM alongside, because under
# `throttle off` THE SCREENSHOT LAGS VRAM. Measured 2026-09-24: VRAM finished
# drawing by t=25s, yet `screenshot -raw` stayed black at 25/35/45s and showed
# the picture only by 60s -- the renderer updates the displayed frame rarely
# when unthrottled, and -raw captures the last RENDERED frame. VRAM is the
# truth; the PNG is a lagging picture of it.
set save_settings_on_exit off
set throttle off
set dir $::env(OUTDIR)
after time 60 {
    set nz 0
    for {set a 0} {$a < 54272} {incr a 97} { if {[debug read VRAM $a] != 0} { incr nz } }
    set log [open "$dir/shot.txt" w]
    puts $log [format "R0=%02X  VRAM nonzero %d/560 sampled" [debug read {VDP regs} 0] $nz]
    close $log
    screenshot -raw "$dir/shot.png"
    exit
}
