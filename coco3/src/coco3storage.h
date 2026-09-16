#ifndef COCO3STORAGE_H
#define COCO3STORAGE_H

/* CoCo 3 filesystem operations that are NOT part of the portable seam.
 *
 * core/storage.h is the contract every port implements and it is deliberately
 * sequential -- nothing in the shared code seeks. These two are a CoCo 3
 * Disk BASIC filesystem talking to a CoCo 3 far store, and they live here
 * rather than in core/ for exactly the reason that header gives: the core and
 * the UI must never see a path, a device number or a granule.
 */

/* Opens by name for random access. Returns the first granule, or 0xFF, and
   writes the file's length through `len` if that is not null. */
unsigned char plat_raw_open(const char *name, unsigned long *len);

/* Reads the `index`th 256-byte sector of that file into `dst`, which must be
   at least 256 bytes. Non-zero on success. Walking the chain costs no disk
   access; the FAT is already in RAM. */
unsigned char plat_raw_sector(unsigned char first, unsigned int index,
                              unsigned char *dst);


/* Set by a port whose sound is interrupt-driven; see coco3storage.c. Null by
   default, so nothing that links this file has to provide anything. */
extern void (*plat_disk_quiet)(void);

#endif
