#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>

void fileInfo(const char* path) {
	char fullPath[64];
	sprintf(fullPath, "romfs:/%s", path);
	FILE* f = fopen(fullPath, "r");

	if(!f) {
		printf("could not open file \"%s\"\n", path);
		return;
	}

	uint32_t size;
	fseek(f, 0, SEEK_END);
	size = ftell(f);

	printf("\"%s\": %li b\n", path, size);

	return;
}

void printFile(const char* path) {
	char fullPath[64];
	sprintf(fullPath, "romfs:/%s", path);
	FILE* f = fopen(fullPath, "r");

	if(!f) {
		printf("could not open file \"%s\"\n", path);
		return;
	}

	uint32_t size;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	rewind(f);

	char* buffer = malloc(size * sizeof(char));
	fread(buffer, size, 1, f);
	printf("%s\n", buffer);
	free(buffer);

	return;
}

void listDir(const char* path) {
	DIR* dir;
	struct dirent* ent;

	char fullPath[64];
	sprintf(fullPath, "romfs:/%s", path);
	dir = opendir(fullPath);
	if(dir != NULL) {
		while((ent = readdir(dir)) != NULL) {
			printf("%s\n", ent->d_name);
		}
		closedir(dir);
	} else {
		printf("could not open directory \"%s\"\n", path);
	}
}

void readRomfsFile(const char* path, void** buffer, uint32_t* size) {
	char fullPath[64];
	sprintf(fullPath, "romfs:/%s", path);
	FILE* f = fopen(fullPath, "r");

	if(!f) {
		printf("could not open file\"%s\"\n", path);
		*buffer = NULL;
		return;
	}

	fseek(f, 0, SEEK_END);
	*size = ftell(f);
	rewind(f);

	*buffer = malloc(*size);
	fread(*buffer, *size, 1, f);
	printf("%li bytes\n", *size);
	return;
}
