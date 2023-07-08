#include "../include/util.h"
#include "../include/debug.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>


int
WriteFile(
	const char* Path,
	uint64_t Length,
	uint8_t* Buffer
	)
{
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
}


int
ReadFile(
	const char* Path,
	uint64_t* Length,
	uint8_t** Buffer
	)
{
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
}
