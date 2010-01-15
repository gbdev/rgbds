#ifndef ASMOTOR_LINK_MAIN_H
#define ASMOTOR_LINK_MAIN_H

#include "link/types.h"

extern void usage(void);
extern SLONG fillchar;
extern char smartlinkstartsymbol[256];

enum eOutputType {
	OUTPUT_GBROM,
	OUTPUT_PSION2
};

extern enum eOutputType outputtype;

#endif
