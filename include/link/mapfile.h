#ifndef RGBDS_LINK_MAPFILE_H
#define RGBDS_LINK_MAPFILE_H

#include <stdint.h>

extern void SetMapfileName(char *name);
extern void SetSymfileName(char *name);
extern void CloseMapfile(void);
extern void MapfileWriteSection(struct sSection * pSect);
extern void MapfileInitBank(int32_t bank);
extern void MapfileCloseBank(int32_t slack);

#endif
