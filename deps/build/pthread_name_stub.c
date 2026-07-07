/* Part of the FreeCAD -> WebAssembly port (github.com/magik6k/freecad-web).
   Compile:  emcc -c pthread_name_stub.c -o pthread_name_stub.o
   Then include the .o in the Stage-2 relink (see femrelink-s2.sh / deps/README.md). */
#include <stddef.h>
/* VTK loguru references these glibc thread-naming fns; emscripten lacks them.
   Single-threaded wasm: no-op stubs. */
typedef unsigned long pthread_t;
int pthread_getname_np(pthread_t t, char *name, size_t len){ if(len) name[0]='\0'; return 0; }
int pthread_setname_np(pthread_t t, const char *name){ (void)t;(void)name; return 0; }
