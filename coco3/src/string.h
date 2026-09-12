/* string.h for cmoc, which does not ship one.
 *
 * cmoc declares memcpy, memset, strlen, strcpy, strncpy and strcmp in
 * <cmoc.h> instead, and provides them in its own runtime. Without this shim
 * the shared sources' `#include <string.h>` resolves to the HOST's -- on this
 * Mac, Xcode's libc++ header, which fails with "Your compiler doesn't seem to
 * define __BYTE_ORDER__" and "Unsupported architecture" fifty lines into a
 * cascade that never mentions cmoc.
 *
 * That error is worth recognising rather than debugging: a cross compiler
 * reaching a MacOSX.sdk path means a standard header it does not have, and
 * the answer is a shim here, not a flag.
 */
#ifndef COCO3_STRING_H
#define COCO3_STRING_H

#include <cmoc.h>

#endif
