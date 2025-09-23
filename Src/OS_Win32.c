#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>

#if defined(StrLen) || defined(StrCpy) || defined(StrCmp)
	// HACK: There are a ton of Str macros defined in 'shlwapi.h'
	#undef StrLen
	#undef StrCpy
	#undef StrCmp
	#define StrLen(str) strlen(str)
	#define StrCpy(dst, src) strcpy(dst, src)
	#define StrCmp(s1, s2) (strcmp(s1, s2) == 0)
#endif

bool IsFileValid(char* path)
{
	// FIXME: It doesn't fail if it's a valid directory but not a file
	return PathFileExistsA(path);
}

bool IsDirValid(char* dir)
{
	return PathIsDirectoryA(dir);
}

// Forward declaration
char* GetFilenameFromPath(char* path);
char* GetDirFromPath(char* path);
char* GetFileExtension(char* file);
size_t FullLenStrList(Str_List list);

size_t IterateDir(size_t startIndex, bool recurse, char** fileList, char* path, char* ext)
{
	const char* idk = "*";
	char* findPath = (char*) malloc(StrLen(path) + 2);
	findPath = PathCombineA(findPath, path, idk);

	size_t entryIndex = startIndex;
	WIN32_FIND_DATAA fileData = {};
	HANDLE find = FindFirstFileA(findPath, &fileData);
	do {
		if (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			// We want to skip the '.' and '..' directories
			if (recurse && !StrCmp(fileData.cFileName, ".") && !StrCmp(fileData.cFileName, "..")) {
				char* subDir = (char*) malloc(MAX_PATH + 1);
				subDir = PathCombineA(subDir, path, fileData.cFileName);
				entryIndex = IterateDir(entryIndex, recurse, fileList, subDir, ext);
				free(subDir);
			}

			continue;
		}

		char* currExt = GetFileExtension(fileData.cFileName);
		if (StrCmp(currExt, ext)) {
			if (fileList != NULL) {
				fileList[entryIndex] = (char*) malloc(MAX_PATH + 1);
				fileList[entryIndex] = PathCombineA(fileList[entryIndex], path, fileData.cFileName);
			}

			entryIndex += 1;
		}

		free(currExt);
	} while (FindNextFileA(find, &fileData));

	FindClose(find);
	free(findPath);

	return entryIndex;
}

char* GetLibsStr(Str_List libs)
{
	char* libsStr = (char*) malloc(FullLenStrList(libs));
	size_t libsStrOffset = 0;
	for (size_t i = 0; i < libs.size; i += 1) {
		size_t strLen = StrLen(libs.data[i]);
		MemCpy(&libsStr[libsStrOffset], libs.data[i], strLen + 1);
		if (i < libs.size - 1)
			libsStr[libsStrOffset + strLen] = ' ';

		libsStrOffset += strLen + 1;
	}

	return libsStr;
}

typedef struct Process_Data {
	STARTUPINFO startInfo; // TODO: Probably doesn't need to live here
	PROCESS_INFORMATION processInfo;
} Process_Data;

bool SpawnAsyncProcess(char* cmd, char* workDir, Process_Data* process)
{
	char* workDirAbs = (char*) malloc(MAX_PATH + 1);
	GetFullPathNameA(workDir, MAX_PATH, workDirAbs, NULL);

	MemZero(process, sizeof(Process_Data));
	BOOL res = CreateProcessA(
		NULL, cmd,
		NULL, NULL,
		// TODO: Maybe 'bInheritHandles' should be true in order to share StdOutput
		FALSE, NORMAL_PRIORITY_CLASS,
		NULL, workDirAbs,
		&process->startInfo, &process->processInfo
	);

	// TODO: Idk if that's safe
	free(workDirAbs);

	return res == TRUE;
}

// FIXME: This should return only when all processes had already been finished
bool WaitForMultipleProcesses(Process_Data* processList, size_t processCount)
{
	HANDLE* handles = _alloca(sizeof(HANDLE) * processCount);
	for (size_t i = 0; i < processCount; i += 1)
		handles[i] = processList[i].processInfo.hProcess;

	DWORD res = WaitForMultipleObjects(
		(DWORD) processCount, handles,
		TRUE, INFINITE
	);

	return res != WAIT_FAILED;
}

size_t GetThreadCount()
{
	SYSTEM_INFO info = {0};
	GetSystemInfo(&info);

	return (size_t) info.dwNumberOfProcessors;
}
