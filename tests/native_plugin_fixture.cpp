#if defined(_WIN32)
#define NENGINE_EXPORT extern "C" __declspec(dllexport)
#else
#define NENGINE_EXPORT extern "C" __attribute__((visibility("default")))
#endif
NENGINE_EXPORT int nengine_plugin_smoke(int x) { return x+7; }
