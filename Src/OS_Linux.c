#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <spawn.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/sysinfo.h>

char *realpath (const char *__restrict, char *__restrict);

bool IsFileValid(char* path)
{
    struct stat fileInfo = {0};
    int res = stat(path, &fileInfo);
    return (res == 0) && S_ISREG(fileInfo.st_mode);
}

bool IsDirValid(char* path)
{
    struct stat fileInfo = {0};
    int res = stat(path, &fileInfo);
    return (res == 0) && S_ISDIR(fileInfo.st_mode);
}

// Forward declaration
char* GetFilenameFromPath(char* path);
char* GetDirFromPath(char* path);
char* GetFileExtension(char* file);
size_t FullLenStrList(Str_List list);

static char* _PathJoin(char* path1, char* path2)
{
    size_t path1Len = StrLen(path1);
    size_t path2Len = StrLen(path2);
    size_t finalLen = path1Len + path2Len + 2;

    char* finalPath = (char*) malloc(finalLen);
    MemCpy(&finalPath[0], path1, path1Len);
    MemCpy(&finalPath[path1Len + 1], path2, path2Len + 1);
    finalPath[path1Len] = '/';

    return finalPath;
}

size_t IterateDir(size_t startIndex, bool recurse, char** fileList, char* path, char* ext)
{
    size_t entryIndex = startIndex;
    DIR* dir = opendir(path);

    struct dirent* entry = NULL;
    while ((entry = readdir(dir))) {
        if (entry->d_type == DT_DIR) {
            // We want to skip the '.' and '..' directories
            if (recurse && !StrCmp(entry->d_name, ".") && !StrCmp(entry->d_name, "..")) {
                char* subDir = _PathJoin(path, entry->d_name);
                entryIndex = IterateDir(entryIndex, recurse, fileList, subDir, ext);
                free(subDir);
            }

            continue;
        }

        char* currExt = GetFileExtension(entry->d_name);
        if (currExt != NULL) {
            if (StrCmp(currExt, ext)) {
                if (fileList != NULL) {
                    char* relativePath = _PathJoin(path, entry->d_name);
                    fileList[entryIndex] = malloc(PATH_MAX);

                    realpath(relativePath, &fileList[entryIndex][1]);
                    fileList[entryIndex][0] = '\"';
                    fileList[entryIndex][StrLen(fileList[entryIndex])] = '\"';

                    free(relativePath);
                }

                entryIndex += 1;
            }
        }

        free(currExt);
    }

    closedir(dir);

    return entryIndex;
}

char* GetLibsStr(Str_List libs)
{
    const char* prefix = "-l";
    const size_t prefixLen = sizeof("-l") - 1;

	char* libsStr = (char*) malloc(libs.size * (prefixLen + 1) + FullLenStrList(libs));
	size_t libsStrOffset = 0;
	for (size_t i = 0; i < libs.size; i += 1) {
	    size_t strLen = StrLen(libs.data[i]);
	    MemCpy(&libsStr[libsStrOffset], prefix, prefixLen);
		MemCpy(&libsStr[libsStrOffset + prefixLen], libs.data[i], strLen + 1);
		if (i < libs.size - 1)
		    libsStr[libsStrOffset + prefixLen + strLen] = ' ';

		libsStrOffset += prefixLen + strLen + 1;
	}

	return libsStr;
}

typedef struct Process_Data {
    char** argv;
    pid_t pid;
} Process_Data;

bool SpawnAsyncProcess(char* cmd, char* workDir, Process_Data* process)
{
    char* program = NULL;
    for (size_t i = 0; cmd[i] != '\0'; i += 1) {
        if (cmd[i] == ' ') {
            program = (char*) malloc(i);
            program[i] = '\0';
            MemCpy(program, cmd, i);

            size_t numOfArgs = 0;
            for (size_t j = i; cmd[j] != '\0'; j += 1)
                if (cmd[j] == ' ')
                    numOfArgs += 1;

            process->argv = malloc(sizeof(char*) * (numOfArgs + 1));
            process->argv[0] = program;
            process->argv[numOfArgs] = NULL;

            bool insideBlock = false;
            size_t lastSplit = i;
            size_t argIdx = 1;
            for (size_t j = i + 1; cmd[j] != '\0'; j += 1) {
                size_t argLen = j - lastSplit - 1;

                #define _AppendArg()\
                    process->argv[argIdx] = malloc(argLen);\
                    MemCpy(process->argv[argIdx], &cmd[j - argLen], argLen);\
                    argIdx += 1

                if (cmd[j] == '\"') {
                    if (insideBlock) {
                        _AppendArg();
                        j += 1;
                    }

                    insideBlock = !insideBlock;
                    lastSplit = j;
                } else if (cmd[j] == ' ' && !insideBlock) {
                    _AppendArg();
                    lastSplit = j;
                }
            }

            break;
        }
    }

    char* currDir = (char*) malloc(PATH_MAX);
    currDir = getcwd(currDir, PATH_MAX);

    // Since the child process inherits the CWD, we need to chage it temporarily
    chdir(workDir);
    int res = posix_spawnp(&process->pid, program, NULL, NULL, process->argv, environ);
    chdir(currDir);

    free(currDir);
    free(program);

    return res == 0;
}

void DestroyProcess(Process_Data* process)
{
    for (size_t i = 0; process->argv[i] != NULL; i += 1)
        free(process->argv[i]);

    free(process->argv);
    process->argv = NULL;
    process->pid  = 0;
}

bool WaitForMultipleProcesses(Process_Data* processList, size_t processCount)
{
    for (size_t i = 0; i < processCount; i += 1)
        if (waitpid(processList[i].pid, NULL, 0) == -1)
            return false;

    return true;
}

size_t GetThreadCount()
{
    return (size_t) get_nprocs();
}
