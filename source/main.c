#include <3ds.h>

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <dirent.h>
#include <stdarg.h>

#include <errno.h>

#include <fcntl.h>

#include <sys/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SOC_ALIGN      0x1000
#define SOC_BUFFERSIZE 0x100000

static u32* SOC_buffer = NULL;
s32 sock = -1, csock = -1;

__attribute__((format(printf,1,2)))
void failExit(const char* fmt, ...) {
	if(sock>0) close(sock);
	if(csock>0) close(csock);

	va_list ap;

	printf(CONSOLE_RED);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf(CONSOLE_RESET);
	printf("\npress B to exit\n");

	while(aptMainLoop()) {
		gspWaitForVBlank();
		hidScanInput();

		u32 kDown = hidKeysDown();
		if(kDown & KEY_B) exit(0);
	}
}

// these have to be in order
// there's probably a much better way of doing this lmao
char* extensions[] = {
	"html",
	"css",
	"js",
	"png",
	"jpg",
};

char* contentTypes[] = {
	"text/html",
	"text/css",
	"text/js",
	"image/png",
	"image/jpg",
};

char* getContentType(const char* extension) {
	char* result = NULL;
	for(uint16_t i = 0; i < sizeof(extensions)/sizeof(const char*); ++i) {
		if(strcmp(extension, extensions[i]) == 0) {
			result = contentTypes[i];
			break;
		}
	}

	return result;
}

const static char http_200[] = "HTTP/1.1 200 OK\r\n";
const static char http_html_hdr[] = "Content-type: text/html\r\n\r\n";
const static char http_get_index[] = "GET / HTTP/1.1\r\n";

void socShutdown() {
	printf("waiting for socExit\n");
	socExit();
}

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

void uninit() {
	romfsExit();
	gfxExit();
}

void handleRequest(char* request) {
	char temp[1024];
	char path[64] = "html/";
	char* p = request+4;
	int i = 4;
	while(*p != ' ') {
		path[i] = *p;
		++i;
		++p;
	}
	path[i] = '\0';
	if(i==5) {
		sprintf(path, "html/index.html");
	}
	printf("%s\n", path);
	void* buffer;
	uint32_t size = 0;
	readRomfsFile(path, &buffer, &size);
	if(buffer == NULL || size == 0) {
		sprintf(temp, "HTTP/1.1 404 Not Found");
		send(csock, temp, strlen(temp), 0);
		send(csock, http_html_hdr, strlen(http_html_hdr),0);
		sprintf(temp, "<html><head><title>404</title></head><body><h1>404</h1></body></html>");
		send(csock, temp, strlen(temp), 0);
	} else {
		char extension[5];
		p=path;
		i=0;
		while(*p != '.' && *p != '\0') {
			++p;
		}
		strcpy(extension, p);
		if(*p != '\0') { 
			send(csock, http_200, strlen(http_200),0);
			sprintf(temp, "Content-type: %s\r\n\r\n", getContentType(extension));
			send(csock, temp, strlen(temp),0);
			send(csock, (void*)buffer, size, 0);
		}
	}
	free(buffer);
	printf("sent!\n");
}

int main(int argc, char** argv) {
	gfxInitDefault();

	consoleInit(GFX_TOP, NULL);

	Result rc = romfsInit();

	int ret;

	if(rc) {
		printf("romfsInit: %08lX\n", rc);
		sleep(3);
		gfxExit();
		exit(0);
	}

	atexit(uninit);

	u32 clientLen;
	struct sockaddr_in client;
	struct sockaddr_in server;
	char temp[1024];

	printf("test\n");

	listDir("html");
	fileInfo("html/index.html");
	printFile("html/index.html");

	SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);

	if(SOC_buffer == NULL) {
		failExit("memalign: failed to allocate\n");
	}

	if((ret = socInit(SOC_buffer, SOC_BUFFERSIZE)) != 0) {
		failExit("socInit: 0x%08X\n", (unsigned int) ret);
	}

	clientLen = sizeof(client);

	atexit(socShutdown);

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

	if(sock < 0) {
		failExit("socket: %d %s\n", errno, strerror(errno));
	}

	memset(&server, 0, sizeof(server));
	memset(&client, 0, sizeof(client));

	server.sin_family = AF_INET;
	server.sin_port = htons(80);
	server.sin_addr.s_addr = gethostid();

	printf("ip: %s\n", inet_ntoa(server.sin_addr));

	if ((ret = bind(sock, (struct sockaddr*)&server, sizeof(server)))) {
		close(sock);
		failExit("bind: %d %s\n", errno, strerror(errno));
	}

	fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);

	if((ret = listen(sock,5))) {
		failExit("listen: %d %s\n", errno, strerror(errno));
	}

	while(aptMainLoop()) {
		hidScanInput();
		u32 kDown = hidKeysDown();

		if(kDown & KEY_START) break;

		csock = accept(sock, (struct sockaddr*)&client, &clientLen);

		if(csock < 0) {
			if(errno != EAGAIN) {
				failExit("accept: %d %s\n", errno, strerror(errno));
			}
		} else {
			fcntl(csock, F_SETFL, fcntl(csock, F_GETFL, 0) & ~O_NONBLOCK);
			printf("connection port %d from %s\n", client.sin_port, inet_ntoa(client.sin_addr));
			memset(temp,0,1024);
			ret = recv(csock,temp,1024,0);
			printf("%s\n",temp);
			if(!strncmp(temp,http_get_index,4)) {
				printf("!\n");
				handleRequest(temp);
			}
			close(csock);
			csock = -1;
		}

		gfxFlushBuffers();
		gfxSwapBuffers();

		gspWaitForVBlank();
	}
	close(sock);

	romfsExit();
	gfxExit();

	return 0;
}
