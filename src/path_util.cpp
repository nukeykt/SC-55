#include "path_util.h"
#include <string_view>
#include <cstdio>

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

std::filesystem::path P_GetProcessPath()
{
#if defined(_WIN32)
    char path[MAX_PATH];
    DWORD actual_size = GetModuleFileNameA(NULL, path, sizeof(path));
    if (actual_size == 0)
    {
        // TODO: handle error
        fprintf(stderr, "fatal: P_GetProcessPath failed\n");
        exit(1);
    }
#elif defined(__APPLE__)
    char path[1024];
    uint32_t actual_size = sizeof(path);
    if (_NSGetExecutablePath(path, &actual_size) != 0)
    {
        // TODO: handle error
        fprintf(stderr, "fatal: P_GetProcessPath failed\n");
        exit(1);
    }
#else
    char path[PATH_MAX];
    ssize_t actual_size = readlink("/proc/self/exe", path, sizeof(path));
    if (actual_size == -1)
    {
        // TODO: handle error
        fprintf(stderr, "fatal: P_GetProcessPath failed\n");
        exit(1);
    }
#endif
    return std::filesystem::path(std::string_view(path, (size_t)actual_size));
}
