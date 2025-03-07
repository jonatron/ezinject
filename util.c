#define _GNU_SOURCE
#include "util.h"
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "ezinject.h"

#include <sys/sem.h>

void hexdump(void *pAddressIn, long lSize) {
	char szBuf[100];
	long lIndent = 1;
	long lOutLen, lIndex, lIndex2, lOutLen2;
	long lRelPos;
	struct {
		char *pData;
		unsigned long lSize;
	} buf;
	unsigned char *pTmp, ucTmp;
	unsigned char *pAddress = (unsigned char *)pAddressIn;

	buf.pData = (char *)pAddress;
	buf.lSize = lSize;

	while (buf.lSize > 0) {
		pTmp = (unsigned char *)buf.pData;
		lOutLen = (int)buf.lSize;
		if (lOutLen > 16)
			lOutLen = 16;

		// create a 64-character formatted output line:
		sprintf(szBuf, " >                                                      %08zX", pTmp - pAddress);
		lOutLen2 = lOutLen;

		for (lIndex = 1 + lIndent, lIndex2 = 53 - 15 + lIndent, lRelPos = 0; lOutLen2; lOutLen2--, lIndex += 2, lIndex2++) {
			ucTmp = *pTmp++;
			sprintf(szBuf + lIndex, "%02X ", (unsigned short)ucTmp);
			if (!isprint(ucTmp))
				ucTmp = '.';	// nonprintable char
			szBuf[lIndex2] = ucTmp;

			if (!(++lRelPos & 3)) {	// extra blank after 4 bytes
				lIndex++;
				szBuf[lIndex + 2] = ' ';
			}
		}
		if (!(lRelPos & 3))
			lIndex--;
		szBuf[lIndex] = '<';
		szBuf[lIndex + 1] = ' ';
		printf("%s\n", szBuf);
		buf.pData += lOutLen;
		buf.lSize -= lOutLen;
	}
}

int get_stack(pid_t pid, uintptr_t *stack_start, size_t *stack_size){
    char line[256];
    snprintf(line, 256, "/proc/%u/maps", pid);
    FILE *fp = fopen(line, "r");

    void *start = NULL;
    void *end = NULL;
    char path[128];
    while(fgets(line, 256, fp)){
        if(sscanf(line, "%p-%p %*s %*p %*x:%*x %*u %s", &start, &end, path) <= 0){
            continue;
        }
        if(strstr(path, "[stack]")){
            break;
        } else {
			start = NULL;
			end = NULL;
		}
    }

    if(start == NULL || end == NULL){
        return -1;
    }

    *stack_start = (uintptr_t)start;
    *stack_size = (uintptr_t)end - (uintptr_t)start;
    return 0;
}

void *get_base(pid_t pid, char *substr, char **ignores)
{
	char line[256];
	char path[128];
	void *base;
	char perms[8];
	bool found = false;

	int sublen = strlen(substr);
	char *end = (char *)&line + sizeof(line);

	snprintf(line, 256, "/proc/%u/maps", pid);
	FILE *fp = fopen(line, "r");
	int val;
	do
	{
		if(!fgets(line, 256, fp))
			break;
		strcpy(path, "[anonymous]");
		val = sscanf(line, "%p-%*p %s %*p %*x:%*x %*u %s", &base, (char *)&perms, path);
		
		char *sub;
		if((sub=strstr(path, substr)) != NULL){
			bool skip = false;
			if(ignores != NULL){
				char **sptr = ignores;
				while(*sptr != NULL){
					if(strstr(path, *(sptr++))){
						skip = true;
						break;
					}
				}
			}
			
			if(skip){
				break;
			}

			sub += sublen;
			if(sub >= end){
				break;
			}

			switch(*sub){
				case '.': //libc.
				case '-': //libc-
					found = true;
					break;
			}

			if(found){
				break;
			}
		}
	} while(val > 0 && !found);
	fclose(fp);

	return (found) ? base : NULL;
}

uintptr_t get_code_base(pid_t pid){
	char line[256];

	snprintf(line, sizeof(line), "/proc/%u/maps", pid);
	FILE *fp = fopen(line, "r");
	if(fp == NULL){
		PERROR("fopen maps");
		return 0;
	}

	uintptr_t start, end;
	char perms[8];
	memset(perms, 0x00, sizeof(perms));

	int val;
	uintptr_t region_addr = 0;

	while(region_addr == 0){
		if(!fgets(line, sizeof(line), fp)){
			PERROR("fgets");
			break;			
		}
		val = sscanf(line, "%p-%p %s %*p %*x:%*x %*u %*s", (void **)&start, (void **)&end, (char *)&perms);
		if(val == 0){
			break;
		}

		if(strchr(perms, 'x') != NULL){
			region_addr = start;
		}
	}

	fclose(fp);
	return region_addr;
}
