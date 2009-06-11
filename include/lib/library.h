#ifndef ASMOTOR_LIB_LIBRARY_H
#define ASMOTOR_LIB_LIBRARY_H

#include "lib/libwrap.h"

extern sLibrary *lib_Read(char *filename);
extern BBOOL lib_Write(sLibrary * lib, char *filename);
extern sLibrary *lib_AddReplace(sLibrary * lib, char *filename);
extern void lib_Free(sLibrary * lib);
extern sLibrary *lib_DeleteModule(sLibrary * lib, char *filename);
extern sLibrary *lib_Find(sLibrary * lib, char *filename);

#endif
