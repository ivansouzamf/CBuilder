#if defined(_WIN32)
	#undef _CRT_NONSTDC_NO_DEPRECATE
	#define _CRT_NONSTDC_NO_DEPRECATE
	#undef _CRT_SECURE_NO_WARNINGS
	#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define INI_IMPLEMENTATION
#include "ini.h"

typedef struct Str_List {
	char** data;
	size_t size;
} Str_List;

bool IsFileValid(char* path);
bool IsDirValid(char* dir);
size_t IterateDir(size_t startIndex, bool recurse, char** fileList, char* path, char* ext);
char* GetLibsStr(Str_List libs);

typedef struct Process_Data Process_Data;
bool SpawnAsyncProcess(char* cmd, char* workDir, Process_Data* process);
bool WaitForMultipleProcesses(Process_Data* processList, size_t processCount);
size_t GetThreadCount();

#if !defined(_WIN32)
	// HACK: There are a ton of Str macros defined in 'shlwapi.h'
	#define StrLen(str) strlen(str)
	#define StrCpy(dst, src) strcpy(dst, src)
	#define StrCmp(s1, s2) (strcmp(s1, s2) == 0)
#endif
#define MemCpy(dst, src, size) memcpy(dst, src, size)
#define MemZero(dst, size) memset(dst, 0, size)
#define MemCmp(dst, src, size) (memcmp(dst, src, size) == 0)

#if defined(__linux__)
	#include "OS_Linux.c"
#elif defined(_WIN32)
	#include "OS_Win32.c"
#else
	#error "OS not supported"
#endif

#define CBUILDER_VERSION "0.0.1"

#define SEC_MAIN "Program"
#if defined(__linux__)
	#define SEC_OS "Program.Linux"
	#include <alloca.h>
	#define ALLOCA(size) alloca(size)
#elif defined(_WIN32)
	#define SEC_OS "Program.Win32"
	#define ALLOCA(size) _alloca(size)
#endif
#define PROP_MAIN_SRCS "sources "
#define PROP_MAIN_OUT "output "
#define PROP_OS_COMP "compiler "
#define PROP_OS_SYSLIBS "sysLibs "
#define PROP_OS_CFLAGS "compFlags "
#define PROP_OS_LFLAGS "linkFlags "

#if defined(__linux__)
	#define COMP_FLAGS "-c"
	#define COMP_OUT "-o "
	#define COMP_EXE_EXT ""
	#define COMP_DLL_EXT ".so"
	#define COMP_OBJ_SEARCH "%s/*.o"
	// That's hacky but it works
	#define COMP_LINK compiler
#elif defined(_WIN32)
	#define COMP_FLAGS "/c"
	#define COMP_OUT "/OUT:"
	#define COMP_EXE_EXT ".exe"
	#define COMP_DLL_EXT ".dll"
	#define COMP_OBJ_SEARCH "%s/*.obj"
	#define COMP_LINK "link.exe"
#endif

#define CHECK_INI(sec) (sec != INI_NOT_FOUND)
char* GetIniProp(ini_t* ini, int sec, const char* name);
char* GetFilenameFromPath(char* path);
char* GetDirFromPath(char* path);
char* GetFileExtension(char* file);
Str_List SplitStringList(char* strList);
Str_List ParseFileList(char* sources);
size_t FullLenStrList(Str_List list);
void DestroyStrList(Str_List* list);

int main(int argc, char* argv[])
{
	const char* cmdUsage =
		"	CBuilder <options> <build_file.ini>\n"
		"	Options:\n"
		"		--version: Show version\n"
		"		--help: Show this message\n"
	;

	if (argc < 2) {
		fprintf(stderr, "Invalid arguments! Usage:\n");
		fprintf(stderr, "%s\n", cmdUsage);
		return -1;
	}

	char* arg = argv[1];
	if (StrCmp(arg, "--version")) {
		printf("CBuilder version %s\n", CBUILDER_VERSION);
		return 0;
	} else if (StrCmp(arg, "--help")) {
		printf("Usage:\n");
		printf("%s", cmdUsage);
		return 0;
	} else {
		if (!IsFileValid(arg)) {
			fprintf(stderr, "Invalid path!\n");
			return -1;
		}

		FILE* file = fopen(arg, "r");

		fseek(file, 0, SEEK_END);
		size_t fileSize = ftell(file);
		fseek(file, 0, SEEK_SET);

		char* fileData = (char*) malloc(fileSize + 1);
		MemZero(fileData, fileSize + 1);
		fread(fileData, 1, fileSize, file);

		ini_t* config = ini_load(fileData, NULL);

		int mainSec = ini_find_section(config, SEC_MAIN, 0);
		int osSec = ini_find_section(config, SEC_OS, 0);
		if (!CHECK_INI(mainSec) || !CHECK_INI(osSec)) {
			fprintf(stderr, "Invalid build config!\n");
			return -1;
		}

		char* sources 	= GetIniProp(config, mainSec, PROP_MAIN_SRCS);
		char* output 	= GetIniProp(config, mainSec, PROP_MAIN_OUT);
		char* compiler 	= GetIniProp(config, osSec, PROP_OS_COMP);
		char* sysLibs 	= GetIniProp(config, osSec, PROP_OS_SYSLIBS);
		char* compFlags = GetIniProp(config, osSec, PROP_OS_CFLAGS);
		char* linkFlags = GetIniProp(config, osSec, PROP_OS_LFLAGS);

		char* outputDir  = GetDirFromPath(output);
		char* outputFile = GetFilenameFromPath(output);

		Str_List sourcesSplitted = SplitStringList(sources);
		Str_List sysLibsSplitted = SplitStringList(sysLibs);

		size_t thrdCount = GetThreadCount();

		for (size_t i = 0; i < sourcesSplitted.size; i += 1) {
			Str_List sourceFiles = ParseFileList(sourcesSplitted.data[i]);
			for (size_t j = 0; j < sourceFiles.size; j += thrdCount) {
				Process_Data* processes = ALLOCA(sizeof(Process_Data) * thrdCount);
				for (size_t k = 0; (k < thrdCount) && (k + j < sourceFiles.size); k += 1) {
					size_t srcIdx = j + k;

					const char* cmdFmt = "%s %s %s %s";
					size_t cmdLen = 1 + snprintf(NULL, 0, cmdFmt, compiler, COMP_FLAGS, compFlags, sourceFiles.data[srcIdx]);
					char* cmd = (char*) malloc(cmdLen);
					snprintf(cmd, cmdLen, cmdFmt, compiler, COMP_FLAGS, compFlags, sourceFiles.data[srcIdx]);

					bool ok = SpawnAsyncProcess(cmd, outputDir, &processes[k]);
					if (!ok) {
						fprintf(stderr, "Error trying to compile file '%s'\n", sourceFiles.data[srcIdx]);
						return -1;
					}

					free(cmd);
				}

				// TODO: Maybe we should destroy process data as well
				WaitForMultipleProcesses(processes, thrdCount);
				//Sleep(1000 * 5); // HACK: Should be removed when 'WaitForMultipleProcesses()' got fixed
			}

			DestroyStrList(&sourceFiles);
		}

		// Linking stage
		{
			size_t objPathLen = 1 + snprintf(NULL, 0, COMP_OBJ_SEARCH, outputDir);
			char* objPath = (char*) malloc(objPathLen);
			snprintf(objPath, objPathLen, COMP_OBJ_SEARCH, outputDir);

			Str_List objFiles = ParseFileList(objPath);

			char* objFilesStr = (char*) malloc(FullLenStrList(objFiles));
			size_t objFilesStrOffset = 0;
			for (size_t i = 0; i < objFiles.size; i += 1) {
				size_t fileLen = StrLen(objFiles.data[i]);
				MemCpy(&objFilesStr[objFilesStrOffset], objFiles.data[i], fileLen + 1);
				if (i < objFiles.size - 1)
					objFilesStr[objFilesStrOffset + fileLen] = ' ';

				objFilesStrOffset += fileLen + 1;
			}

			char* libsStr = GetLibsStr(sysLibsSplitted);

			const char* cmdFmt = "%s %s %s%s%s %s %s";
			size_t cmdLen = 1 + snprintf(NULL, 0, cmdFmt, COMP_LINK, linkFlags, COMP_OUT, outputFile, COMP_EXE_EXT, libsStr, objFilesStr);
			char* cmd = (char*) malloc(cmdLen);
			snprintf(cmd, cmdLen, cmdFmt, COMP_LINK, linkFlags, COMP_OUT, outputFile, COMP_EXE_EXT, libsStr, objFilesStr);

			Process_Data linkerProcess = {0};
			bool ok = SpawnAsyncProcess(cmd, outputDir, &linkerProcess);
			if (!ok) {
				fprintf(stderr, "Error trying to link '%s%s'\n", outputFile, COMP_EXE_EXT);
				return -1;
			}

			WaitForMultipleProcesses(&linkerProcess, 1);

			//free(cmd);
			//free(libsStr);
			//free(objFilesStr);
			//DestroyStrList(&objFiles);
			//free(objPath);
		}

		//DestroyStrList(&sourcesSplitted);
		//DestroyStrList(&sysLibsSplitted);
		//free(outputFile);
		//free(outputDir);
		//ini_destroy(config);
		//free(fileData);
	}

	return 0;
}

char* GetIniProp(ini_t* ini, int sec, const char* name)
{
	int prop = ini_find_property(ini, sec, name, 0);
	if (!CHECK_INI(prop)) {
		char* secName = (char*) ini_section_name(ini, sec);
		fprintf(stderr, "Missing property '%s' in section '%s'\n", name, secName);
		exit(-1);
	}

	return (char*) ini_property_value(ini, sec, prop);
}


// TODO: No memory allocation
char* GetFilenameFromPath(char* path)
{
	size_t lastSlash = 0;
	size_t lenght = 0;
	for (size_t i = 0; path[i] != '\0'; i += 1) {
		lenght += 1;
		if (path[i] == '/')
			lastSlash = i;
	}

	size_t size = lenght - lastSlash;
	char* filename = (char*) malloc(size);
	MemZero(filename, size);
	MemCpy(filename, &path[lastSlash + 1], size);

	return filename;
}

char* GetDirFromPath(char* path)
{
	size_t lastSlash = 0;
	for (size_t i = 0; path[i] != '\0'; i += 1) {
		if (path[i] == '/')
			lastSlash = i;
	}

	char* dir = (char*) malloc(lastSlash + 1);
	MemZero(dir, lastSlash + 1);
	MemCpy(dir, path, lastSlash);

	return dir;
}

// TODO: No memory allocation
char* GetFileExtension(char* file)
{
	char* ext = NULL;
	size_t fileLen = StrLen(file);
	for (size_t i = fileLen; i > 0; i -= 1) {
		if (file[i - 1] == '.') {
			size_t extLen = fileLen - i;
			ext = (char*) malloc(extLen);
			StrCpy(ext, &file[fileLen - extLen]);
			break;
		}
	}

	return ext;
}

Str_List SplitStringList(char* strList)
{
	size_t splits = 0;
	for (size_t i = 0; strList[i] != '\0'; i += 1) {
		if (strList[i] == ',')
			splits += 1;
	}

	Str_List finalList = {
		.data = (char**) malloc(sizeof(char*) * (splits + 1)),
		.size = splits + 1,
	};

	size_t current = 0;
	size_t offset = 0;
	for (size_t i = 0; ; i += 1) {
		if (strList[i] == ' ')
			offset += 1;

		if (strList[i] == ',' || strList[i] == '\0') {
			size_t size = i - offset;
			finalList.data[current] = (char*) malloc(size + 1);
			MemZero(finalList.data[current], size + 1);
			MemCpy(finalList.data[current], &strList[i - size], size);
			offset = i + 1;
			current += 1;

			if (strList[i] == '\0')
				break;
		}
	}

	return finalList;
}

Str_List ParseFileList(char* sources)
{
    Str_List fileList = {};
    char* file = GetFilenameFromPath(sources);

    if (file[0] != '*') {
		fileList.data = (char**) malloc(sizeof(char*) * 1);
		fileList.data[0] = file;
		fileList.size = 1;

		return fileList;
	}

    bool recurse = MemCmp(file, "**", 2);

    char* dir = GetDirFromPath(sources);
   	char* ext = GetFileExtension(file);
	fileList.size = IterateDir(0, recurse, NULL, dir, ext);
	if (fileList.size != 0) {
		fileList.data = (char**) malloc(sizeof(char*) * fileList.size);
		IterateDir(0, recurse, fileList.data, dir, ext);
	}

	free(ext);
    free(dir);
    free(file);

    return fileList;
}

size_t FullLenStrList(Str_List list)
{
    size_t listLen = 0;
        for (size_t i = 0; i < list.size; i += 1)
            listLen += StrLen(list.data[i]);

    return listLen;
}

void DestroyStrList(Str_List* list)
{
	for (size_t i = 0; i < list->size; i += 1) {
		free(list->data[i]);
	}

	free(list->data);
	list->data = NULL;
	list->size = 0;
}
