#include "../include/util.h"
#include "../include/debug.h"

#include <stdlib.h>
#ifdef _WIN32
	#include <windows.h>
#else
	#include <unistd.h>
	#include <fcntl.h>
	#include <sys/stat.h>
#endif


int
UtilWriteFile(
	const char* Path,
	uint64_t Length,
	uint8_t* Buffer
	)
{
#ifdef _WIN32
	HANDLE File = CreateFile(Path, GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(File == INVALID_HANDLE_VALUE)
	{
		return -1;
	}

	DWORD Bytes;
	WriteFile(File, Buffer, Length, &Bytes, NULL);
	CloseHandle(File);
	return Bytes == Length ? 0 : -1;
#else
	int File = open(Path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	if(File < 0)
	{
		return -1;
	}

	if(ftruncate(File, Length) == -1)
	{
		return -1;
	}

	ssize_t Bytes = write(File, Buffer, Length);
	close(File);
	return Bytes == Length ? 0 : -1;
#endif
}


int
UtilReadFile(
	const char* Path,
	uint64_t* Length,
	uint8_t** Buffer
	)
{
#ifdef _WIN32
	HANDLE File = CreateFile(Path, GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(File == INVALID_HANDLE_VALUE)
	{
		return -1;
	}

	DWORD fileSize = GetFileSize(File, NULL);
	*Length = fileSize;
	*Buffer = malloc(*Length);
	AssertNEQ(*Buffer, NULL);

	DWORD Bytes;
	ReadFile(File, *Buffer, *Length, &Bytes, NULL);
	CloseHandle(File);
	return Bytes == *Length ? 0 : -1;
#else
	int File = open(Path, O_RDONLY);
	if(File < 0)
	{
		return -1;
	}

	struct stat Stat;
	if(fstat(File, &Stat) == -1)
	{
		return -1;
	}

	*Length = Stat.st_size;
	*Buffer = malloc(*Length);
	AssertNEQ(*Buffer, NULL);

	ssize_t Bytes = read(File, *Buffer, *Length);
	close(File);
	return Bytes == *Length ? 0 : -1;
#endif
}
