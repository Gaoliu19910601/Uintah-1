#undef SCISHARE

#if defined(_WIN32) && !defined(BUILD_STATIC)
#ifdef BUILD_Dataflow_Modules_Visualization
#define SCISHARE __declspec(dllexport)
#else
#define SCISHARE __declspec(dllimport)
#endif
#else
#define SCISHARE
#endif
