// Misc. utilities

#ifndef UTILS_H
#define UTILS_H

// Prototypes
// Übersetzt Hex-Text in Bytes; zwei Ziffern pro Byte.
int hex2data(unsigned char *data, const char *hexstring, unsigned int len);
// Kehrt Byte-Reihenfolge im Array um.
void reverseByteArray(unsigned char *data, int len);

#endif
