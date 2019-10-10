#ifdef WIN32
#undef DLL
#define DLL __declspec(dllimport)
#endif
