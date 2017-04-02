#if __STDC_VERSION__ >= 201112L
    /* C11 or newer */
    #define noreturn _Noreturn
#elif __cplusplus >= 201103L
    /* C++11 or newer */
    #define noreturn [[noreturn]]
#elif __GNUC__ > 2 || (__GNUC__ == 2 && (__GNUC_MINOR__ >= 5))
    /* GCC 2.5 or newer */
    #define noreturn __attribute__ ((noreturn))
#elif _MSC_VER >= 1310
    /* MS Visual Studio 2003/.NET Framework 1.1 or newer */
    #define noreturn _declspec( noreturn)
#else
    /* unsupported, but no need to throw a fit */
    #define noreturn
#endif
