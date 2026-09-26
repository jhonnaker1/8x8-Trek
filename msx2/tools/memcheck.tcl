# THE MEMORY CHECK'S FAILURE PATH, run for real: a normal boot disk -- the real
# COMMAND2.COM -- with the game as EGATREK.COM, typed at the prompt. COMMAND2
# keeps 1,280 bytes, so crt0's check must refuse, wait for a key, and give
# the prompt back; DIR afterwards proves DOS survived it.
set save_settings_on_exit off
set throttle off
after time 20 { type "EGATREK\r" }
after time 30 { set ::a [get_screen] }
after time 32 { type "x" }
after time 40 { type "DIR /W\r" }
after time 46 { set f [open $::env(OUTDIR)/memcheck.txt w]; puts $f "--- 30s:\n$::a\n--- 46s:\n[get_screen]"; close $f; close [open $::env(OUTDIR)/memcheck_done w]; exit }
