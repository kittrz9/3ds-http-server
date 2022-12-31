#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/socket.h>

#include <3ds.h>

#include "romfs.h"

const static char http_200[] = "HTTP/1.1 200 OK\r\n";
const static char http_html_hdr[] = "Content-type: text/html\r\n\r\n";

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

void handleRequest(char* request, s32 csock) {
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
