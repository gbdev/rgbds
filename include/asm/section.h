
#include <stdint.h>

#include "linkdefs.h"

struct Expression;

struct Section {
	char *pzName;
	enum SectionType nType;
	uint32_t nPC;
	uint32_t nOrg;
	uint32_t nBank;
	uint32_t nAlign;
	struct Section *pNext;
	struct Patch *pPatches;
	uint8_t *tData;
};

struct SectionSpec {
	int32_t bank;
	int32_t alignment;
};

struct Section *out_FindSectionByName(const char *pzName);
void out_NewSection(char const *pzName, uint32_t secttype, int32_t org,
		    struct SectionSpec const *attributes);

void out_AbsByte(int32_t b);
void out_AbsByteGroup(char const *s, int32_t length);
void out_Skip(int32_t skip);
void out_String(char const *s);
void out_RelByte(struct Expression *expr);
void out_RelWord(struct Expression *expr);
void out_RelLong(struct Expression *expr);
void out_PCRelByte(struct Expression *expr);
void out_BinaryFile(char const *s);
void out_BinaryFileSlice(char const *s, int32_t start_pos, int32_t length);

void out_PushSection(void);
void out_PopSection(void);
