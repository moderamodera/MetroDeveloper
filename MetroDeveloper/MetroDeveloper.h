#ifndef __METRO_DEVELOPER_H__
#define __METRO_DEVELOPER_H__

DWORD FindPattern(DWORD start_address, DWORD length, BYTE* pattern, char* mask);

void getString(const char* section_name, const char* str_name, const char* default_str, char* result, DWORD size);
bool getBool(const char* section_name, const char* bool_name, bool default_bool);

void install_vfs_hooks(MODULEINFO &mi, bool isLL);

void install_vs_signals(MODULEINFO& mi);
bool process_vs_signal(int key, int action);

#endif
