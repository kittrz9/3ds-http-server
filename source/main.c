#include <3ds.h>

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdarg.h>

#include <errno.h>

#include <fcntl.h>

#include <sys/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "romfs.h"
#include "http.h"

#define SOC_ALIGN      0x1000
#define SOC_BUFFERSIZE 0x100000

const static char http_get_index[] = "GET / HTTP/1.1\r\n";

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


void socShutdown() {
	printf("waiting for socExit\n");
	socExit();
}


void uninit() {
	romfsExit();
	gfxExit();
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
				handleRequest(temp, csock);
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
