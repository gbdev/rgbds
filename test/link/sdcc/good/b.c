// sdcc -c -msm83 -o b.rel b.c

#ifdef __SDCC_sm83
const int sm83 = 1;
#else
const int sm83 = 0;
#endif

int function0(int de) {
	return de | sm83;
}
