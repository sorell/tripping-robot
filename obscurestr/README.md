obscurestr
==========

Let compiler obscure text strings so that they're hard to trace from binary
file. Every text string has a unique 32 bit pseudo random encryption key.
The strings are automatically de-obscured when the application is loaded 
into ram and can be used without any recurring performance hit.

All code is in an include file. No compilation needed.

USAGE (with your application)
-----------------------------

Simply include the header and declare your string:
char const *str = obscureStr("My text");

"My text" will not appear in clear text in the binary file.
