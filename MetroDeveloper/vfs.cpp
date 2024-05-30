#include <stdio.h>
#include <windows.h>
#define PSAPI_VERSION 1
#include <psapi.h>
#include "MinHook.h"

#include "MetroDeveloper.h"

#ifndef _WIN64

LPVOID vfs_ropen_Orig = nullptr;
LPVOID vfs_ropen_os = nullptr;

typedef char(__cdecl* _method)(void* a1, void** buffer, size_t size);
typedef void(__cdecl* _vfs_rbuffered)(const char* fn, void* a1, _method method);
_vfs_rbuffered vfs_rbuffered_Orig = nullptr;

//static char ropen_msg_format[] = "%s\n";

static __declspec(naked) void vfs_ropen_Hook_2033(/*const char* fn*/)
{
	// 0. check if file exists
	__asm 
	{
		/*
		mov eax, [esp + 4]
		push eax
		mov eax, offset ropen_msg_format
		push eax
		call printf
		add esp, 8
		*/

		mov eax, [esp + 4]
		push eax
		call GetFileAttributesA
		cmp eax, 0xFFFFFFFF
		je to_orig_code
	}

	// 1. if it exists, forward call to ropen_os
	__asm 
	{
		push edi              // save regs...
		push esi

		mov edi, esi          // for vfs::ropen 'this' pointer is passed in EDI, while for ropen_os its passed in EDI
		mov eax, [esp + 0x0C]
		push eax              // filename
		call vfs_ropen_os
		add esp, 4

		pop esi               // restore regs...
		pop edi
		
		ret
	}

	// 2. if it doesn't, forward call to original ropen_function
	__asm
	{
	to_orig_code:
		jmp vfs_ropen_Orig
	}
}

static __declspec(naked) void vfs_ropen_Hook_LastLight(/*const char* fn*/)
{
	// 0. check if file exists
	__asm
	{
		/*
		mov eax, [esp + 4]
		push eax
		mov eax, offset ropen_msg_format
		push eax
		call printf
		add esp, 8
		*/

		mov eax, [esp + 4]
		push eax
		call GetFileAttributesA
		cmp eax, 0xFFFFFFFF
		je to_orig_code
	}

	// 1. if it exists, forward call to ropen_os
	__asm
	{
		mov eax, [esp + 4]
		push eax            // filename
		push esi            // this
		call vfs_ropen_os
		add esp, 8
		ret
	}

	// 2. if it doesn't, forward call to original ropen_function
	__asm
	{
	to_orig_code:
		jmp vfs_ropen_Orig
	}
}

static const size_t rbuffered_buf_size_2033 = 0x20000;
static const size_t rbuffered_buf_size_LastLight = 0x10000;

static void __cdecl vfs_rbuffered_Hook_2033(const char* fn, void* a1, _method method)
{
	//printf("%s\n", fn);

	if (GetFileAttributes(fn) != 0xFFFFFFFF) {
		size_t size;

		size_t buffer_size = (rbuffered_buf_size_2033);
		void* buffer = malloc(buffer_size);

		if (buffer) {
			FILE* f = fopen(fn, "rb");

			if (f) {
				while (size = fread(buffer, 1, buffer_size, f))
					method(a1, (void**)buffer, size);

				fclose(f);
			}

			free(buffer);
			return;
		}
	}

	vfs_rbuffered_Orig(fn, a1, method);
}

static void __cdecl vfs_rbuffered_Hook_LastLight(const char* fn, void* a1, _method method)
{
	//printf("%s\n", fn);

	if (GetFileAttributes(fn) != 0xFFFFFFFF) {
		size_t size;

		size_t buffer_size = (rbuffered_buf_size_LastLight);
		void* buffer = malloc(buffer_size);

		if (buffer) {
			FILE* f = fopen(fn, "rb");

			if (f) {
				while (size = fread(buffer, 1, buffer_size, f))
					method(a1, &buffer, size);

				fclose(f);
			}

			free(buffer);
			return;
		}
	}

	vfs_rbuffered_Orig(fn, a1, method);
}

#else

typedef void* (__fastcall* _vfs_ropen_package)(void* result, void* package, const char* fn, const int force_raw, unsigned int* uncompressed_size);
typedef void* (__fastcall* _vfs_ropen_os)(void* result, const char* fn);
typedef void(__fastcall* _vfs_rbuffered_package)(void* package, const char* fn, void* cb, const int force_raw);
typedef void* (__fastcall* _rblock_init)(const char* fn, unsigned int* f_offset, unsigned int* f_size, unsigned int not_packaged);
typedef bool(__fastcall* _vfs_package_registry_level_downloaded)(void* _this, const char* map_name);

_vfs_ropen_package vfs_ropen_package_Orig = nullptr;
_vfs_ropen_os vfs_ropen_os = nullptr;
_vfs_rbuffered_package vfs_rbuffered_package_Orig = nullptr;
_rblock_init rblock_init_Orig = nullptr;
_vfs_package_registry_level_downloaded vfs_package_registry_level_downloaded_Orig = nullptr;

void* __fastcall vfs_ropen_package_Hook(void* result, void* package, const char* fn, const int force_raw, unsigned int* uncompressed_size)
{
	//printf("%s\n", fn);

	if (GetFileAttributes(fn) != 0xFFFFFFFF)
	{
		return vfs_ropen_os(result, fn);
	}

	return vfs_ropen_package_Orig(result, package, fn, force_raw, uncompressed_size);
}

struct fastdelegate
{
	void* object;
	bool (*method)(void* object, LPVOID& buffer, size_t size);
};

void __fastcall vfs_rbuffered_package_Hook(void* package, const char* fn, fastdelegate* cb, const int force_raw)
{
	//printf("%s\n", fn);

	if (GetFileAttributes(fn) != 0xFFFFFFFF)
	{
		size_t size;
		void* buffer = malloc(0x30000);
		if (buffer)
		{
			FILE* f = fopen(fn, "rb");
			if (f)
			{
				while (size = fread(buffer, 1, 0x30000, f))
					cb->method(cb->object, buffer, size);

				fclose(f);
			}
			free(buffer);
			return;
		}
	}

	vfs_rbuffered_package_Orig(package, fn, cb, force_raw);
}

void* __fastcall rblock_init_Hook(const char* fn, unsigned int* f_offset, unsigned int* f_size, unsigned int not_packaged)
{
	printf("%s\n", fn);

	if (GetFileAttributes(fn) != 0xFFFFFFFF)
	{
		//return rblock_init_Orig(fn, f_offset, f_size, 1);

		// 0F 84 ? ? ? ? 48 8B D1 48 8D 4C 24
		// \x0F\x84\x00\x00\x00\x00\x48\x8B\xD1\x48\x8D\x4C\x24 xx????xxxxxxx
		void* je = (void*)FindPattern(
			(DWORD64)mi.lpBaseOfDll,
			mi.SizeOfImage,
			(BYTE*)"\x0F\x84\x00\x00\x00\x00\x48\x8B\xD1\x48\x8D\x4C\x24",
			"xx????xxxxxxx");
		BYTE nop[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
		ASMWrite(je, nop, sizeof(nop));

		//BYTE nop1[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
		// 0f 84 ? ? ? ? 48 89 bc 24 ? ? ? ? e8
		//ASMWrite((void*)je, nop1, sizeof(nop1));

		void* ttt = rblock_init_Orig(fn, f_offset, f_size, 1);
		if (ttt == 0)
		{
			printf("test\n");
		}
		return ttt;
	}

	return rblock_init_Orig(fn, f_offset, f_size, not_packaged);
}

bool __fastcall vfs_package_registry_level_downloaded_Hook(void* _this, const char* map_name)
{
	bool ret = vfs_package_registry_level_downloaded_Orig(_this, map_name);

	if (!ret)
	{
		char map_path[256];
		strcpy(map_path, "content\\maps\\");
		strcat(map_path, map_name);
		strcat(map_path, "\\level");
		if (FileExists(map_path))
			ret = true;
	}

	return ret;
}

#endif

void install_vfs_hooks(MODULEINFO& mi, bool isLL)
{
#ifndef _WIN64
	// 55 8B EC 83 E4 ? 83 EC ? 53 57 8D 44 24
	LPVOID vfs_ropen_Address = (LPVOID)FindPattern(
		(DWORD)mi.lpBaseOfDll,
		mi.SizeOfImage,
		(BYTE*)"\x55\x8B\xEC\x83\xE4\x00\x83\xEC\x00\x53\x57\x8D\x44\x24",
		"xxxxx?xx?xxxxx");

	LPVOID vfs_rbuffered_Address;

	if (!isLL)
	{
		// 55 8B EC 83 E4 ? 81 EC ? ? ? ? 53 8B 1D ? ? ? ? 56 8D 44 24
		vfs_ropen_os = (LPVOID)FindPattern(
			(DWORD)mi.lpBaseOfDll,
			mi.SizeOfImage,
			(BYTE*)"\x55\x8B\xEC\x83\xE4\x00\x81\xEC\x00\x00\x00\x00\x53\x8B\x1D\x00\x00\x00\x00\x56\x8D\x44\x24",
			"xxxxx?xx????xxx????xxxx");

		// 55 8B EC 83 E4 ? 83 EC ? 53 56 57 8D 44 24 ? 50
		vfs_rbuffered_Address = (LPVOID)FindPattern(
			(DWORD)mi.lpBaseOfDll,
			mi.SizeOfImage,
			(BYTE*)"\x55\x8B\xEC\x83\xE4\x00\x83\xEC\x00\x53\x56\x57\x8D\x44\x24\x00\x50",
			"xxxxx?xx?xxxxxx?x");
	}
	else
	{
		// 55 8B EC 83 E4 ? 81 EC ? ? ? ? 56 57 8B 3D ? ? ? ? 8D 44 24
		vfs_ropen_os = (LPVOID)FindPattern(
			(DWORD)mi.lpBaseOfDll,
			mi.SizeOfImage,
			(BYTE*)"\x55\x8B\xEC\x83\xE4\x00\x81\xEC\x00\x00\x00\x00\x56\x57\x8B\x3D\x00\x00\x00\x00\x8D\x44\x24",
			"xxxxx?xx????xxxx????xxx");

		// 83 EC ? 53 55 56 57 8D 44 24 ? 50
		vfs_rbuffered_Address = (LPVOID)FindPattern(
			(DWORD)mi.lpBaseOfDll,
			mi.SizeOfImage,
			(BYTE*)"\x83\xEC\x00\x53\x55\x56\x57\x8D\x44\x24\x00\x50",
			"xx?xxxxxxx?x");
	}

	if (MH_CreateHook(vfs_ropen_Address, isLL ? &vfs_ropen_Hook_LastLight : &vfs_ropen_Hook_2033, reinterpret_cast<LPVOID*>(&vfs_ropen_Orig)) == MH_OK) {
		if (MH_EnableHook(vfs_ropen_Address) != MH_OK) {
			MessageBox(NULL, "MH_EnableHook() != MH_OK", "vfs_ropen Hook", MB_OK | MB_ICONERROR);
		}
	}
	else {
		MessageBox(NULL, "MH_CreateHook() != MH_OK", "vfs_ropen Hook", MB_OK | MB_ICONERROR);
	}

	if (MH_CreateHook(vfs_rbuffered_Address, isLL ? &vfs_rbuffered_Hook_LastLight : &vfs_rbuffered_Hook_2033, reinterpret_cast<LPVOID*>(&vfs_rbuffered_Orig)) == MH_OK) {
		if (MH_EnableHook(vfs_rbuffered_Address) != MH_OK) {
			MessageBox(NULL, "MH_EnableHook() != MH_OK", "vfs_rbuffered Hook", MB_OK | MB_ICONERROR);
		}
	}
	else {
		MessageBox(NULL, "MH_CreateHook() != MH_OK", "vfs_rbuffered Hook", MB_OK | MB_ICONERROR);
	}
#else
	// 48 89 5C 24 ? 48 89 6C 24 ? 56 57 41 54 41 56 41 57 48 83 EC ? 45 33 E4 45 8B F9
	LPVOID vfs_ropen_package_Address = (LPVOID)FindPattern(
		(DWORD64)mi.lpBaseOfDll,
		mi.SizeOfImage,
		(BYTE*)"\x48\x89\x5C\x24\x00\x48\x89\x6C\x24\x00\x56\x57\x41\x54\x41\x56\x41\x57\x48\x83\xEC\x00\x45\x33\xE4\x45\x8B\xF9",
		"xxxx?xxxx?xxxxxxxxxxx?xxxxxx");

	// 48 8B C4 53 55 57 41 56 41 57 48 81 EC
	vfs_ropen_os = (_vfs_ropen_os)FindPattern(
		(DWORD64)mi.lpBaseOfDll,
		mi.SizeOfImage,
		(BYTE*)"\x48\x8B\xC4\x53\x55\x57\x41\x56\x41\x57\x48\x81\xEC",
		"xxxxxxxxxxxxx");

	// 48 89 5C 24 ? 48 89 74 24 ? 41 56 48 83 EC ? 83 79
	LPVOID vfs_rbuffered_package_Address = (LPVOID)FindPattern(
		(DWORD64)mi.lpBaseOfDll,
		mi.SizeOfImage,
		(BYTE*)"\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x41\x56\x48\x83\xEC\x00\x83\x79",
		"xxxx?xxxx?xxxxx?xx");

	// 48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 41 56 48 81 EC ? ? ? ? 33 ED
	LPVOID rblock_init_Address = (LPVOID)FindPattern(
		(DWORD64)mi.lpBaseOfDll,
		mi.SizeOfImage,
		(BYTE*)"\x48\x89\x5C\x24\x00\x48\x89\x6C\x24\x00\x48\x89\x74\x24\x00\x41\x56\x48\x81\xEC\x00\x00\x00\x00\x33\xED",
		"xxxx?xxxx?xxxx?xxxxx????xx");

	// 48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 41 54 41 56 41 57 48 83 EC 20 44 0F B7 B9 ? ? ? ? 33 DB 4C 8B F2 4C 8B E1 45 85 FF 74 6E
	LPVOID vfs_package_registry_level_downloaded_Address = (LPVOID)FindPattern(
		(DWORD64)mi.lpBaseOfDll,
		mi.SizeOfImage,
		(BYTE*)"\x48\x89\x5C\x24\x00\x48\x89\x6C\x24\x00\x48\x89\x74\x24\x00\x48\x89\x7C\x24\x00\x41\x54\x41\x56\x41\x57\x48\x83\xEC\x20\x44\x0F\xB7\xB9\x00\x00\x00\x00\x33\xDB\x4C\x8B\xF2\x4C\x8B\xE1\x45\x85\xFF\x74\x6E",
		"xxxx?xxxx?xxxx?xxxx?xxxxxxxxxxxxxx????xxxxxxxxxxxxx");

	if (MH_CreateHook(vfs_ropen_package_Address, &vfs_ropen_package_Hook, reinterpret_cast<LPVOID*>(&vfs_ropen_package_Orig)) == MH_OK) {
		if (MH_EnableHook(vfs_ropen_package_Address) != MH_OK) {
			MessageBox(NULL, "MH_EnableHook() != MH_OK", "vfs_ropen_package Hook", MB_OK | MB_ICONERROR);
		}
	}
	else {
		MessageBox(NULL, "MH_CreateHook() != MH_OK", "vfs_ropen_package Hook", MB_OK | MB_ICONERROR);
	}

	if (MH_CreateHook(vfs_rbuffered_package_Address, &vfs_rbuffered_package_Hook, reinterpret_cast<LPVOID*>(&vfs_rbuffered_package_Orig)) == MH_OK) {
		if (MH_EnableHook(vfs_rbuffered_package_Address) != MH_OK) {
			MessageBox(NULL, "MH_EnableHook() != MH_OK", "vfs_rbuffered_package Hook", MB_OK | MB_ICONERROR);
		}
	}
	else {
		MessageBox(NULL, "MH_CreateHook() != MH_OK", "vfs_rbuffered_package Hook", MB_OK | MB_ICONERROR);
	}

	/*if (MH_CreateHook(rblock_init_Address, &rblock_init_Hook, reinterpret_cast<LPVOID*>(&rblock_init_Orig)) == MH_OK) {
		if (MH_EnableHook(rblock_init_Address) != MH_OK) {
			MessageBox(NULL, "MH_EnableHook() != MH_OK", "rblock_init Hook", MB_OK | MB_ICONERROR);
		}
	} else {
		MessageBox(NULL, "MH_CreateHook() != MH_OK", "rblock_init Hook", MB_OK | MB_ICONERROR);
	}*/

	if (MH_CreateHook(vfs_package_registry_level_downloaded_Address, &vfs_package_registry_level_downloaded_Hook, reinterpret_cast<LPVOID*>(&vfs_package_registry_level_downloaded_Orig)) == MH_OK) {
		if (MH_EnableHook(vfs_package_registry_level_downloaded_Address) != MH_OK) {
			MessageBox(NULL, "MH_EnableHook() != MH_OK", "vfs_rbuffered_package Hook", MB_OK | MB_ICONERROR);
		}
	}
	else {
		MessageBox(NULL, "MH_CreateHook() != MH_OK", "vfs_rbuffered_package Hook", MB_OK | MB_ICONERROR);
	}
#endif
}