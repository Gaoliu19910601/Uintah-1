#undef SCISHARE

#if defined(_WIN32) && !defined(BUILD_STATIC)
#ifdef BUILD_Core_Algorithms_DataIO
#define SCISHARE __declspec(dllexport)
#else
#define SCISHARE __declspec(dllimport)
#endif
#else
#define SCISHARE
#endif
