#pragma once

#include <imagine/fs/FS.hh>

void chdirFromFilePath(const char *path);

// used on iOS to allow saves on incorrectly root-owned files/dirs
void fixFilePermissions(const char *path);

static void fixFilePermissions(const FS::PathString &path)
{
	return fixFilePermissions(path.data());
}
