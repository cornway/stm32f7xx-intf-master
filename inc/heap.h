
#ifdef DATA_IN_ExtSDRAM

#define ALIGN(x) __attribute__((aligned(x)))

#define SDRAM __attribute__ ((section ("dram")))
#define DTCM __attribute__ ((section ("dtcm")))
#define IRAM __attribute__ ((section ("iram")))
#define IRAM2 __attribute__ ((section ("iram2")))

#else

#define SDRAM
#define DTCM
#define IRAM
#define IRAM2

#endif

