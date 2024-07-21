#ifndef __METRO_DEVELOPER_H__
#define __METRO_DEVELOPER_H__

// settings
typedef char string256[256];

#ifndef _WIN64
DWORD FindPattern(DWORD start_address, DWORD length, BYTE* pattern, char* mask);
DWORD FindPatternInEXE(BYTE* pattern, char* mask);
#else
DWORD64 FindPattern(DWORD64 start_address, DWORD64 length, BYTE* pattern, char* mask);
DWORD64 FindPatternInEXE(BYTE* pattern, char* mask);
#endif

void ASMWrite(void* address, BYTE* code, size_t size);

void getString(const char* section_name, const char* str_name, const char* default_str, char* result, DWORD size);
bool getBool(const char* section_name, const char* bool_name, bool default_bool);
float getFloat(const char* section_name, const char* param_name, float param_default);

void install_vfs_hooks(bool isLL);

void install_vs_signals();
bool process_vs_signal(int key, int action);

void StartNavmapThread();

#endif
