#include <stdint.h>

void fileInfo(const char* path);
void printFile(const char* path);
void listDir(const char* path);
void readRomfsFile(const char* path, void** buffer, uint32_t*size);
