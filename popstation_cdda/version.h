#ifndef __VERSION_H
#define __VERSION_H

/*
	I (rck) figured it would be proper to slap on a version
	so we don't have all these "strange" (:-p) builds of popstation
	floating around with no idea of what they are for.

	This should give some clarity.
	If you make any changes, slap in an entry in this comment block
	and up the version!

	Dark_AleX (3.02OE-B) = 1.00  <- start
	Dark_AleX (3.03OE-A) = 1.10  <- update (compression)
	Tinnus               = 1.20  <- embedded maketoc
	Tinnus               = 1.21  <- fixes

	(start versioning)

	rck                  = 1.22  <- cleaned up code
	rck                  = 1.23  <- fixed silly bug

	Z33 (ZrX ;))         = 1.24  <- Replace disclaimer screen with
	                                boot.png if available.
	                                Atleast insert correct disc
	                                length if no ccd/toc available.
	                                Fill remaining toc-entries to
	                                make it more "proper".
*/

#define VERSION_NUMBER	"1.24"
#define VERSION_AUTHOR	"Z33"

#define VERSION_STRING	VERSION_NUMBER "-" VERSION_AUTHOR

#endif
