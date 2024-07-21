#include <windows.h>
#include <stdio.h>
#include <vector>
#include "i_pathengine.h"
#include "model.hpp"

#include "MetroDeveloper.h"

struct NavmapGenOptions
{
	string256 navmapFormat;
	string256 navmapFilename;
	string256 navmapResult;
	bool navmapBinMode;
	bool navmapDebugInfoBin;
	bool navmapExitWhenDone;
};

NavmapGenOptions g_opts;

#ifndef _WIN64
extern void convert_tok_to_bin(const void* tok_data, size_t tok_size, void** bin_data, size_t* bin_size, int _debug);

// nav_map part
class MemoryStreamImpl : public iOutputStream
{
public:
	std::vector<char> m_data;

	bool save(const char* filename, bool isLL)
	{
		bool result;

		FILE* out = fopen(filename, "wb");
		if (!out)
			return false;

		if (!isLL)
		{
			size_t written = fwrite(&m_data.front(), 1, m_data.size(), out);
			result = (written == m_data.size());
		}
		else
		{
			void* bin_data;
			size_t bin_size;
			convert_tok_to_bin(&m_data[0], m_data.size(), &bin_data, &bin_size, g_opts.navmapDebugInfoBin);
			size_t written = fwrite(bin_data, bin_size, 1, out);
			result = (written == 1);
			free(bin_data);
		}

		fclose(out);

		return result;
	}

	const char* ptr()
	{
		return &m_data.front();
	}

	size_t size()
	{
		return m_data.size();
	}

	virtual void put(const char* data, tUnsigned32 dataSize)
	{
		size_t pos = m_data.size();
		m_data.resize(m_data.size() + dataSize);
		memcpy(&m_data[pos], data, dataSize);
	}

	void putInt(int value)
	{
		put((char*)&value, sizeof(value));
	}

	void putFloat(float value)
	{
		put((char*)&value, sizeof(value));
	}
};

iMesh* load_raw(iPathEngine* pathengine, const char* filename)
{
	FaceVertexMeshImpl m;

	if (!m.load_raw(filename))
	{
		printf("raw mesh '%s' load failed\n", filename);
		return NULL;
	}

	iFaceVertexMesh const* const pm = &m;
	iMesh* real_mesh = pathengine->buildMeshFromContent(&pm, 1, 0);

	return real_mesh;
}

iMesh* load_4a(iPathEngine* pathengine, const char* filename)
{
	FaceVertexMeshImpl m;

	if (!m.load_4a(filename))
	{
		printf("4A mesh '%s' load failed\n", filename);
		return NULL;
	}

	iFaceVertexMesh const* const pm = &m;
	iMesh* real_mesh = pathengine->buildMeshFromContent(&pm, 1, 0);

	return real_mesh;
}

iMesh* load_xml(iPathEngine* pathengine, const char* filename)
{
	FILE* f = fopen(filename, "rb");
	if (!f)
		return NULL;

	fseek(f, 0, SEEK_END);
	size_t length = ftell(f);
	fseek(f, 0, SEEK_SET);

	void* buffer = malloc(length);
	fread(buffer, 1, length, f);

	fclose(f);

	iMesh* real_mesh = pathengine->loadMeshFromBuffer("xml", (char*)buffer, length, 0);
	free(buffer);

	return real_mesh;
}

void create_shapes_2033(iPathEngine* pathengine, iShape** shape1, iShape** shape2, iShape** shape3)
{
	tSigned32 shape1_data[] = {
		-300, 0,
		-150, 259,
		150, 259,
		300, 0,
		150, -259,
		-150, -259
	};

	*shape1 = pathengine->newShape(6, shape1_data);

	tSigned32 shape2_data[] = {
		-600, 0,
		-300, 519,
		300, 519,
		600, 0,
		300, -519,
		-300, -519
	};

	*shape2 = pathengine->newShape(6, shape2_data);

	tSigned32 shape3_data[] = {
		-100, 0,
		-50, 86,
		50, 86,
		100, 0,
		50, -86,
		-50, -86
	};

	*shape3 = pathengine->newShape(6, shape3_data);
}

void create_shapes_ll_and_redux(iPathEngine* pathengine, iShape** shape1, iShape** shape2, iShape** shape3)
{
	tSigned32 shape1_data_ll[] = {
		-350, 0,
		-247, 247,
		0, 350,
		247, 247,
		350, 0,
		247, -247,
		0, -350,
		-247, -247
	};

	*shape1 = pathengine->newShape(8, shape1_data_ll);

	tSigned32 shape2_data_ll[] = {
		-600, 0,
		-424, 424,
		0, 600,
		424, 424,
		600, 0,
		424, -424,
		0, -600,
		-424, -424
	};

	*shape2 = pathengine->newShape(8, shape2_data_ll);

	tSigned32 shape3_data_ll[] = {
		-100, 0,
		-70, 70,
		0, 100,
		70, 70,
		100, 0,
		70, -70,
		0, -100,
		-70, -70
	};

	*shape3 = pathengine->newShape(8, shape3_data_ll);
}

//typedef iwriterObj* (__cdecl* _vfs__wopen_os)(iwriterObj* result, const char* _fname);
//_vfs__wopen_os vfs__wopen_os = nullptr;

void savemesh(iPathEngine* pathengine)
{
	iMesh* real_mesh;

	if (strcmp(g_opts.navmapFormat, "4a") == 0)
		real_mesh = load_4a(pathengine, g_opts.navmapFilename);
	else if (strcmp(g_opts.navmapFormat, "raw") == 0)
		real_mesh = load_raw(pathengine, g_opts.navmapFilename);
	else if (strcmp(g_opts.navmapFormat, "xml") == 0)
		real_mesh = load_xml(pathengine, g_opts.navmapFilename);
	else
		real_mesh = NULL;

	printf("real_mesh = %pX\n", real_mesh);
	if (!real_mesh)
		return;

	iCollisionContext* ctx = real_mesh->newContext();
	printf("ctx = %p\n", ctx);
	if (!ctx)
	{
		delete real_mesh;
		return;
	}

	ctx->setSurfaceTypeTraverseCost(1, 0.1000f);
	real_mesh->burnContextIntoMesh(ctx);

	iShape* shape1, * shape2, * shape3;
	if (!g_opts.navmapBinMode) // isLL
	{
		create_shapes_2033(pathengine, &shape1, &shape2, &shape3);
	}
	else
	{
		create_shapes_ll_and_redux(pathengine, &shape1, &shape2, &shape3);
	}

	printf("shape1 = %pX\nshape2 = %pX\nshape3 = %p\n", shape1, shape2, shape3);

	if (!shape1 || !shape2 || !shape3)
	{
		delete shape3;
		delete shape2;
		delete shape1;
		delete ctx;
		delete real_mesh;
		return;
	}

	const char* cp_options[] = {
		//"connectOverlappingShapeExpansions", "true",
		//"enableConnectedRegionQueries", "true",
		0
	};

	const char* pfp_options[] = {
		"enableConnectedRegionQueries", "true",
		0
	};

	real_mesh->generateCollisionPreprocessFor(shape1, cp_options);
	real_mesh->generateCollisionPreprocessFor(shape2, cp_options);
	real_mesh->generateCollisionPreprocessFor(shape3, cp_options);
	real_mesh->generatePathfindPreprocessFor(shape1, pfp_options);
	real_mesh->generatePathfindPreprocessFor(shape2, pfp_options);
	real_mesh->generatePathfindPreprocessFor(shape3, pfp_options);

	MemoryStreamImpl ms_mesh, ms_cp[3], ms_pfp[3];

	real_mesh->saveGround("tok", true, &ms_mesh);
	real_mesh->saveCollisionPreprocessFor(shape1, &ms_cp[0]);
	real_mesh->saveCollisionPreprocessFor(shape2, &ms_cp[1]);
	real_mesh->saveCollisionPreprocessFor(shape3, &ms_cp[2]);
	real_mesh->savePathfindPreprocessFor(shape1, &ms_pfp[0]);
	real_mesh->savePathfindPreprocessFor(shape2, &ms_pfp[1]);
	real_mesh->savePathfindPreprocessFor(shape3, &ms_pfp[2]);

	// 8B 44 24 08 FF 05
	/*vfs__wopen_os = (_vfs__wopen_os)FindPattern(
		(DWORD)mi.lpBaseOfDll,
		mi.SizeOfImage,
		(BYTE*)"\x8B\x44\x24\x08\xFF\x05",
		"xxxxxx");

	iwriterObj wo;
	vfs__wopen_os(&wo, "nav_map.bin");

	printf("_object = %08X\n", wo._object);
	printf("__vftable = %08X\n", wo._object->__vftable);
	printf("iwriter_dtor_0 = %08X\n", wo._object->__vftable->iwriter_dtor_0);*/

	//real_mesh->saveGround(wo._object);
	//real_mesh->saveCollisionPreprocessFor(shape1, wo._object);
	//real_mesh->savePathfindPreprocessFor(shape1, wo._object);

	// try to get position for covers...
	tSigned32 pos0[3] = { -46.201 * 1000.f, -3.192 * 1000.f, -0.0 * 1000.f };
	tSigned32 pos1[3] = { -26.127 * 1000.f, 6.644 * 1000.f, -0.0 * 1000.f };
	tSigned32 pos2[3] = {-40.054 * 1000.f, -6.504 * 1000.f, -0.0 * 1000.f};

	cPosition p0 = real_mesh->positionNear3DPoint(pos0, 100, 100);
	printf("position0 x=%d y=%d cell=%d\n", p0.x, p0.y, p0.cell);
	tSigned32 conn_reg0 = real_mesh->getConnectedRegionFor(shape3, p0);
	printf("conn_reg0 = %d\n", conn_reg0);

	cPosition p1 = real_mesh->positionNear3DPoint(pos1, 100, 100);
	printf("position1 x=%d y=%d cell=%d\n", p1.x, p1.y, p1.cell);
	tSigned32 conn_reg1 = real_mesh->getConnectedRegionFor(shape3, p1);
	printf("conn_reg1 = %d\n", conn_reg1);

	cPosition p2 = real_mesh->positionNear3DPoint(pos2, 100, 100);
	printf("position2 x=%d y=%d cell=%d\n", p2.x, p2.y, p2.cell);
	tSigned32 conn_reg2 = real_mesh->getConnectedRegionFor(shape3, p2);
	printf("conn_reg2 = %d\n", conn_reg2);

	// compile together ;)
	MemoryStreamImpl result;

	result.putInt(ms_mesh.size());
	result.put(ms_mesh.ptr(), ms_mesh.size());

	result.putInt(3); // unknown, probably preprocess count

	result.putFloat(g_opts.navmapBinMode /*isLL*/ ? 0.35f : 0.3f); // unknwown1
	result.putInt(ms_cp[0].size());
	result.put(ms_cp[0].ptr(), ms_cp[0].size());
	result.putInt(ms_pfp[0].size());
	result.put(ms_pfp[0].ptr(), ms_pfp[0].size());

	result.putFloat(0.6f); // unknwown2
	result.putInt(ms_cp[1].size());
	result.put(ms_cp[1].ptr(), ms_cp[1].size());
	result.putInt(ms_pfp[1].size());
	result.put(ms_pfp[1].ptr(), ms_pfp[1].size());

	result.putFloat(0.1f); // unknwown3
	result.putInt(ms_cp[2].size());
	result.put(ms_cp[2].ptr(), ms_cp[2].size());
	result.putInt(ms_pfp[2].size());
	result.put(ms_pfp[2].ptr(), ms_pfp[2].size());

	result.save(g_opts.navmapResult, g_opts.navmapBinMode /*isLL*/);

	// free resources and exit
	delete shape3;
	delete shape2;
	delete shape1;

	delete ctx;

	delete real_mesh;

	return;
}

static DWORD WINAPI NavMapThread(LPVOID)
{
	typedef iPathEngine* (__stdcall* _getPathEngine)();

	// B8 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? B8 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? B8 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? B8 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? B8
	_getPathEngine getPathEngine = (_getPathEngine)FindPatternInEXE(
		(BYTE*)"\xB8\x00\x00\x00\x00\xA3\x00\x00\x00\x00\xA3\x00\x00\x00\x00\xB8\x00\x00\x00\x00\xA3\x00\x00\x00\x00\xA3\x00\x00\x00\x00\xB8\x00\x00\x00\x00\xA3\x00\x00\x00\x00\xA3\x00\x00\x00\x00\xB8\x00\x00\x00\x00\xA3\x00\x00\x00\x00\xA3\x00\x00\x00\x00\xB8",
		"x????x????x????x????x????x????x????x????x????x????x????x????x");

	iPathEngine* pathEngine = getPathEngine();

	printf("pPathEngine = %p\n", pathEngine);
	printf("InterfaceMajorVersion %d\n", pathEngine->getInterfaceMajorVersion());
	printf("InterfaceMinorVersion %d\n", pathEngine->getInterfaceMinorVersion());

	tSigned32 i, j, k;
	pathEngine->getReleaseNumbers(i, j, k);
	printf("ReleaseNumbers %d %d %d\n", i, j, k);

	savemesh(pathEngine);

	if (g_opts.navmapExitWhenDone)
	{
		/*
		uconsole_server** console = (uconsole_server**)getConsole();
		(*console)->execute_deferred(console, "quit");
		*/
		ExitProcess(0);
	}

	return 0;
}

void StartNavmapThread()
{
	// read settings for navmap generation
	getString("nav_map", "format", "raw", g_opts.navmapFormat, sizeof(g_opts.navmapFormat));
	getString("nav_map", "filename", "nav_map.raw", g_opts.navmapFilename, sizeof(g_opts.navmapFilename));
	g_opts.navmapBinMode = getBool("nav_map", "bin_mode", false);
	g_opts.navmapDebugInfoBin = getBool("nav_map", "supply_debug_info_bin", false);
	if (!g_opts.navmapBinMode) {
		getString("nav_map", "result_pe", "nav_map.pe", g_opts.navmapResult, sizeof(g_opts.navmapResult));
	}
	else {
		getString("nav_map", "result_bin", "nav_map.bin", g_opts.navmapResult, sizeof(g_opts.navmapResult));
	}
	g_opts.navmapExitWhenDone = getBool("nav_map", "exitwhendone", false);

	// start thread
	DWORD thread_id;
	HANDLE thread = CreateThread(NULL, 0, NavMapThread, NULL, 0, &thread_id);
	CloseHandle(thread);
}

#endif