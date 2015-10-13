#ifndef RGBDS_ASM_CHARMAP_H
#define RGBDS_ASM_CHARMAP_H

#define MAXCHARMAPS	512
#define CHARMAPLENGTH	16

struct Charmap {
	int count;
	char input[MAXCHARMAPS][CHARMAPLENGTH + 1];
	char output[MAXCHARMAPS];
};

int readUTF8Char(char *destination, char *source);
void charmap_Sort();
int charmap_Add(char *input, UBYTE output);
int charmap_Convert(char **input);

#endif
