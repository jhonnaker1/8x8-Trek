/* EGA Trek's resources, compiled by Retro68's Rez and merged with the code
   (the Makefile passes the linked code with --copy). Replaces Retro68's
   default Retro68APPL.r so the app carries its own SIZE, version and data.

   The shape is commodore-uno/mac/src/uno.r's. */
#include "Processes.r"
#include "Finder.r"
#include "Icons.r"

/* MultiFinder partition, MEASURED (2026-11-26, QEMU's Quadra 800, System
   7.6.1): a probe build put FreeMem's lowest value in the window title, and
   in a 640K partition it never fell below 113,184 bytes -- through the
   title, the setup screen and the console. So about 527K is in use.

   What is known of that: 84K of code, 44K of data and statics (the 32K far
   store is most of it), and the 128K offscreen console. That is 256K; the
   other ~270K is NOT YET ACCOUNTED FOR (Retro68 keeping a relocated copy of
   the CODE segment is the first suspect). Until it is, the minimum is the
   partition the measurement was taken in, and the preferred size adds room.
   Retro68's default asks for a whole megabyte. */
resource 'SIZE' (-1) {
	reserved,
	ignoreSuspendResumeEvents,
	reserved,
	cannotBackground,
	needsActivateOnFGSwitch,
	backgroundAndForeground,
	dontGetFrontClicks,
	ignoreChildDiedEvents,
	is32BitCompatible,
	notHighLevelEventAware,
	onlyLocalHLEvents,
	notStationeryAware,
	dontUseTextEditServices,
	reserved,
	reserved,
	reserved,
	768 * 1024,
	640 * 1024
};

resource 'vers' (1) {
	0x00, 0x01, development, 0x01, verUS,
	"0.1",
	"0.1, EGA Trek by Nels Anderson -- the Macintosh port"
};

/* THE DATA, INSIDE THE PROGRAM. Named after the files every other port
   ships, so storage.h's callers ask for "STRINGS.DAT" here as everywhere
   and src/macstorage.c finds a resource of that name first. Paths are
   relative to mac/, where the Makefile runs Rez. Retro68's Rez has no
   `read` statement (the token is declared, no rule uses it), so the files
   come in through the $$read() function inside a `data` body. */
data 'EGAd' (128, "STRINGS.DAT") { $$read("build/STRINGS.DAT") };
data 'EGAd' (129, "MUSIC.DAT")   { $$read("build/MUSIC.DAT") };
data 'EGAd' (130, "BRIEF.TXT")   { $$read("build/BRIEF.TXT") };

/* THE FINDER ICON: drawn by tools/mkicon.py into build/icon.r (ICN# and a
   16-colour icl4), and a bundle that says the creator 'EGAt' owns it. The
   Finder only reads a bundle when the file's hasBundle flag is set, which
   Rez does not do -- tools/setbundle.py sets it in the MacBinary. The shape
   is commodore-uno/mac/src/uno.r's. */
#include "icon.r"

resource 'FREF' (128) {
	'APPL', 0, ""
};

resource 'BNDL' (128) {
	'EGAt', 0,
	{
		'ICN#', { 0, 128 };
		'FREF', { 0, 128 }
	}
};

/* The owner resource the bundle points at: by convention a Pascal string. */
data 'EGAt' (0) {
	$"2A" "EGA Trek, by Nels Anderson -- the Mac port"
};
