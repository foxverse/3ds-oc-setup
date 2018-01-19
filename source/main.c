#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <3ds/services/fs.h>
#include <3ds/services/am.h>
#include <unistd.h>
#include "jsmn.h"


#include <3ds.h>


#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define DEBUG

static const char	*g_version ="v1.0";

enum
{
	USA = 0,
	EUR = 1,
	JAP = 2,
	Cert = 3,
};

char *str_replace(char *haystack, size_t haystacksize,
					const char *, const char *newneedle);

#define SUCCESS (char *)haystack
#define FAILURE (void *)NULL

static bool
locate_forward(char **needle_ptr, char *read_ptr,
		const char *needle, const char *needle_last);
static bool
locate_backward(char **needle_ptr, char *read_ptr,
		const char *needle, const char *needle_last);
		
char *str_replace(char *haystack, size_t haystacksize,
					const char *oldneedle, const char *newneedle)
{   
    size_t oldneedle_len = strlen(oldneedle);
    size_t newneedle_len = strlen(newneedle);
    char *oldneedle_ptr;    
    char *read_ptr;         
    char *write_ptr; 
    const char *oldneedle_last =
        oldneedle +             
        oldneedle_len - 1;      

    if (oldneedle_len == 0)
        return SUCCESS;   

    if (newneedle_len <= oldneedle_len) {       
        for (oldneedle_ptr = (char *)oldneedle,
            read_ptr = haystack, write_ptr = haystack; 
            *read_ptr != '\0';
            read_ptr++, write_ptr++)
        {
            *write_ptr = *read_ptr;         
            bool found = locate_forward(&oldneedle_ptr, read_ptr,
                        oldneedle, oldneedle_last);
            if (found)  {   
                write_ptr -= oldneedle_len;
                memcpy(write_ptr+1, newneedle, newneedle_len);
                write_ptr += newneedle_len;
            }               
        } 
        *write_ptr = '\0';
        return SUCCESS;
    }

    else {
        size_t diff_len =      
            newneedle_len -   
            oldneedle_len;   

        for (oldneedle_ptr = (char *)oldneedle,
            read_ptr = haystack, write_ptr = haystack;
            *read_ptr != '\0';
            read_ptr++, write_ptr++)
        {
            bool found = locate_forward(&oldneedle_ptr, read_ptr, 
                        oldneedle, oldneedle_last);
            if (found) {    
                write_ptr += diff_len;
            }
            if (write_ptr >= haystack+haystacksize)
                return FAILURE;
        }

        for (oldneedle_ptr = (char *)oldneedle_last;
            write_ptr >= haystack;
            write_ptr--, read_ptr--)
        {
            *write_ptr = *read_ptr;
            bool found = locate_backward(&oldneedle_ptr, read_ptr, 
                        oldneedle, oldneedle_last);
            if (found) {    
                write_ptr -= diff_len;
                memcpy(write_ptr, newneedle, newneedle_len);
            }   
        }
        return SUCCESS;
    }
}

static inline bool 
locate_forward(char **needle_ptr, char *read_ptr,
        const char *needle, const char *needle_last)
{
    if (**needle_ptr == *read_ptr) {
        (*needle_ptr)++;
        if (*needle_ptr > needle_last) {
            *needle_ptr = (char *)needle;
            return true;
        }
    }
    else 
        *needle_ptr = (char *)needle;
    return false;
}

static inline bool
locate_backward(char **needle_ptr, char *read_ptr, 
        const char *needle, const char *needle_last)
{
    if (**needle_ptr == *read_ptr) {
        (*needle_ptr)--;
        if (*needle_ptr < needle) {
            *needle_ptr = (char *)needle_last;
            return true;
        }
    }
    else 
        *needle_ptr = (char *)needle_last;
    return false;
}

Result http_download(const char *url, u8 **output, u32 *outSize)
{
    Result ret=0;
    httpcContext context;
    char *newurl=NULL;
    u32 statuscode=0;
    u32 contentsize=0, readsize=0, size=0;
    u8 *buf, *lastbuf;

    do {
        ret = httpcOpenContext(&context, HTTPC_METHOD_GET, url, 1);
        #ifdef DEBUG
            printf("return from httpcOpenContext: %"PRId32"\n",ret);
        #endif

        ret = httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);
        #ifdef DEBUG
            printf("return from httpcSetSSLOpt: %"PRId32"\n",ret);
        #endif

        ret = httpcAddRequestHeaderField(&context, "User-Agent", "httpc-example/1.0.0");
        #ifdef DEBUG
            printf("return from httpcAddRequestHeaderField: %"PRId32"\n",ret);
        #endif

        ret = httpcAddRequestHeaderField(&context, "Connection", "Keep-Alive");
        #ifdef DEBUG
            printf("return from httpcAddRequestHeaderField: %"PRId32"\n",ret);
        #endif

        ret = httpcBeginRequest(&context);
        if(ret!=0){
            httpcCloseContext(&context);
            if(newurl!=NULL) free(newurl);
            return ret;
        }

        ret = httpcGetResponseStatusCode(&context, &statuscode);
        if(ret!=0){
            httpcCloseContext(&context);
            if(newurl!=NULL) free(newurl);
            return ret;
        }

        if ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308)) {
            if(newurl==NULL) newurl = malloc(0x1000);
            if (newurl==NULL){
                httpcCloseContext(&context);
                return -1;
            }
            ret = httpcGetResponseHeader(&context, "Location", newurl, 0x1000);
            url = newurl;
            #ifdef DEBUG
                printf("redirecting to url: %s\n",url);
            #endif
            httpcCloseContext(&context);
        }
    } while ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308));

    if(statuscode!=200){
        #ifdef DEBUG
            printf("URL returned status: %"PRId32"\n", statuscode);
        #endif
        httpcCloseContext(&context);
        if(newurl!=NULL) free(newurl);
        return -2;
    }

    ret = httpcGetDownloadSizeState(&context, NULL, &contentsize);
    if(ret!=0){
        httpcCloseContext(&context);
        if(newurl!=NULL) free(newurl);
        return ret;
    }
    #ifdef DEBUG
        printf("reported size: %"PRId32"\n",contentsize);
    #endif

    buf = (u8*)malloc(0x1000);
    if(buf==NULL){
        httpcCloseContext(&context);
        if(newurl!=NULL) free(newurl);
        return -1;
    }

    do {
        ret = httpcDownloadData(&context, buf+size, 0x1000, &readsize);
        size += readsize; 
        if (ret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING){
            lastbuf = buf;
            buf = realloc(buf, size + 0x1000);
            if(buf==NULL){ 
                httpcCloseContext(&context);
                free(lastbuf);
                if(newurl!=NULL) free(newurl);
                return -1;
            }
        }

        if (size > 10000)
        {
        	printf("\33[2K\rDownloading:   [");

	        float   progress = (float)(size) / (float)(contentsize);
	        int     barWidth = 25;
	        int     pos = barWidth * progress;

	        for (int i = 0; i < barWidth; ++i) 
	        {
	            if (i < pos) printf("=");
	            else if (i == pos) printf(">");
	            else printf(" ");
	        }
	        printf("] %d%%", (int)(progress * 100.0f));
    	}
        gfxFlushBuffers();
        gfxSwapBuffers();

    } while (ret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING);

    printf("\n");

    if(ret!=0)
    {
        httpcCloseContext(&context);
        if(newurl!=NULL) free(newurl);
        free(buf);
        return -1;
    }

    lastbuf = buf;
    buf = realloc(buf, size);
    if(buf==NULL){
        httpcCloseContext(&context);
        free(lastbuf);
        if(newurl!=NULL) free(newurl);
        return -1;
    }

    #ifdef DEBUG
        printf("downloaded size: %"PRId32"\n",size);
    #endif

    *output = buf;
    *outSize = size;
    return 0;
}

int CreateFiles(void *buffer, u32 size)
{
	struct stat st = {0};
	
	FILE *patchUSA;
	FILE *patchEUR;
	FILE *patchJAP;
	FILE *cert;
	
	if (stat("sdmc:/luma/titles/000400300000BE02", &st) == -1)
	{
		mkdir("sdmc:/luma/titles/000400300000BE02", 0700);
	} else {
		remove("sdmc:/luma/titles/000400300000BE02");
		mkdir("sdmc:/luma/titles/000400300000BE02", 0700);
	}
	
	if (stat("sdmc:/luma/titles/000400300000BD02", &st) == -1)
	{
		mkdir("sdmc:/luma/titles/000400300000BD02", 0700);
	} else {
		remove("sdmc:/luma/titles/000400300000BD02");
		mkdir("sdmc:/luma/titles/000400300000BD02", 0700);
	}
	
	if (stat("sdmc:/luma/titles/000400300000BC02", &st) == -1)
	{
		mkdir("sdmc:/luma/titles/000400300000BC02", 0700);
	} else {
		remove("sdmc:/luma/titles/000400300000BC02");
		mkdir("sdmc:/luma/titles/000400300000BC02", 0700);
	}
	
	if (stat("sdmc:/olv", &st) == -1)
	{
		mkdir("sdmc:/olv", 0700);
	} else {
		remove("sdmc:/olv");
		mkdir("sdmc/olv", 0700);
	}
	
	if (!buffer)
		return (-1);
	
	patchUSA = fopen("sdmc:/luma/titles/000400300000BE02/code.ips", "w+");
	fwrite(buffer, 1, size, patchUSA);
	fclose(patchUSA);
	
	patchEUR = fopen("sdmc:/luma/titles/000400300000BD02/code.ips", "w+");
	fwrite(buffer, 1, size, patchEUR);
	fclose(patchEUR);
	
	patchJAP = fopen("sdmc:/luma/titles/000400300000BC02/code.ips", "w+");
	fwrite(buffer, 1, size, patchJAP);
	fclose(patchJAP);
	
	cert = fopen("sdmc:/olv/cave.pem", "w+");
	fwrite(buffer, 1, size, cert);
	fclose(cert);
	
	free(buffer);
	
	return(0);
}

int DownloadFiles(int version)
{
	static const char *urls[4] =
	{
		"https://github.com/foxverse/3ds/tree/master/web/oc-setup/usa/code.ips?raw=true",
		"https://github.com/foxverse/3ds/tree/master/web/oc-setup/eur/code.ips?raw=true",
		"https://github.com/foxverse/3ds/tree/master/web/oc-setup/jap/code.ips?raw=true",
		"https://github.com/foxverse/3ds/tree/master/web/oc-setup/cert/cave.pem?raw=true"
	};
	static const char *downloadVersion[4] =
	{
		"Downloading foxverse patches...\n\n",
	};
	
	u8		*buffer = NULL;
	u32		size = 0;
	
	printf(downloadVersion[version]);
	
	if (!http_download(urls[version], &buffer, &size))
	{
		if (!CreateFiles(buffer, size))
		{
			printf("foxverse has been installed.\n\n");
			return(0);
		} else {
			printf("An error occured while installing foxverse!\n");
		}
	} else {
		printf("Installation failed!\n");
	}
	return(-1);
}

/*static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
	if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
			strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
		return 0;
	}
	return -1;
}

char* readFile(char* filename)
{
    FILE* file = fopen(filename,"r");
    if(file == NULL)
    {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long int size = ftell(file);
    rewind(file);

    char* content = calloc(size + 1, 1);

    fread(content,1,size,file);

    return content;
}*/

/*static Result startInstall(u32 *handle)
{
    return (AM_StartCiaInstall(MEDIATYPE_SD, handle));
}

static Result cancelInstall(u32 handle)
{
    return (AM_CancelCIAInstall(handle));
}

static Result endInstall(u32 handle)
{
    return (AM_FinishCiaInstall(handle));
}*/

/*Result installUpdate(const char *url)
{
    int userChoice = 0;

    u8      *buffer = NULL;
    u32     size = 0;
    u32     res;
    Handle  ciaHandle;

    while (userChoice == 0)
    {
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown == KEY_A)
        {
            if (!http_download(url, &buffer, &size))
            {
				#ifdef DEBUG
				printf("Size: %i\n", size);
				#endif
				
            	if (R_SUCCEEDED(res = amInit())) {
            		AM_QueryAvailableExternalTitleDatabase(NULL);
            		
            		res = AM_StartCiaInstall(MEDIATYPE_SD, &ciaHandle);
                	
                	u32 bytes;
                	u8* cia_buffer = (u8*)(malloc(size * sizeof (u8)));
                	FSFILE_Write(ciaHandle, &bytes, 0, buffer, size, 0);
                } else {
                	printf("Unknown error occured (amInit): 0x%08lX\n", res);
                    	userChoice = 1;
                    	return -3;
                }
                
                if (res == 0)
                {
                    if (R_SUCCEEDED(res = AM_FinishCiaInstall(buffer))) {
                        printf("Update installed!\n");
                        userChoice = 1;
                        return 0;
                    } else {
                        printf("Unknown error occured (FinishCiaInstall): 0x%08lX\n", res);
                        userChoice = 1;
                        return -4;
                    }
                }
                else
                {
                    printf("Unknown error occured: 0x%08lX\n", res);
                    userChoice = 1;
                    return -2;
                }
                printf("ok");
            }
            else
            {
                printf("Error downloading updates!\n");
                return -1;
            }

        }
        if (kDown == KEY_B)
        {
            printf("Aborted!\n");
            userChoice = 1;
            return 1;
        }
    }
    return 0;      
}*/

/*int     downloadUpdate(void)
{
    char            *json = NULL;
    char            *changeLog;
    static const char  *urlDownload[89];
    u32             size = 0;
    int             i;
    int             r;
    int 			ret = 0;
    jsmn_parser     jParser;
    jsmntok_t       tokens[128];


    if (!http_download("https://api.github.com/repos/foxverse/3ds/web/oc-setup/releases/latest", (u8 *)&json, &size))
    {
        jsmn_init(&jParser);
        r = jsmn_parse(&jParser, json, size, tokens, sizeof(tokens)/sizeof(tokens[0]));
        if (r < 0) 
        {
            printf(ANSI_COLOR_RED "Failed to parse JSON: %d\n" ANSI_COLOR_RESET, r);
            return 1;
        }
        
        if (r < 1 || tokens[0].type != JSMN_OBJECT) 
        {
            printf(ANSI_COLOR_RED "Object expected\n" ANSI_COLOR_RESET);
            return 1;
        }

        // Loop over all keys of the root object 
        for (i = 1; i < r; i++) 
        {
            if (jsoneq(json, &tokens[i], "tag_name") == 0) 
            {
            	ret = strcmp(json + tokens[i + 1].start, g_version);

                if (ret < 0)
                {
                    printf(ANSI_COLOR_GREEN "New update! Version: %.*s\n" ANSI_COLOR_RESET, tokens[i + 1].end-tokens[i + 1].start,
                        json + tokens[i + 1].start);
                    i++;
                }
            }
            if (jsoneq(json, &tokens[i], "browser_download_url") == 0) 
            {
                strncpy(urlDownload, json + tokens[i + 1].start, 89);
                str_replace(urlDownload, 89, "\"}", "");
                i++;
            }
            if (jsoneq(json, &tokens[i], "body") == 0) 
            {
                if (ret < 0)
                {
                    changeLog = json + tokens[i + 1].start;

                    str_replace(changeLog, tokens[i + 1].end-tokens[i + 1].start, "# ", "");
                    str_replace(changeLog, tokens[i + 1].end-tokens[i + 1].start, "What's New\\r\\n* ", "\n\n* ");
                    str_replace(changeLog, tokens[i + 1].end-tokens[i + 1].start, "\\n", "\n");
                    str_replace(changeLog, tokens[i + 1].end-tokens[i + 1].start, "\\r", "\n");
                    str_replace(changeLog, tokens[i + 1].end-tokens[i + 1].start, ".\"}", "");
                    printf("What's new: %s\n\n", changeLog);
                    printf("Press A to install\n");
                    printf("Press B to abort\n");
                    installUpdate(urlDownload);
                }
            }
        }
    }
    else
    {
        printf("An error occured while checking for an update !\n");
        return (-1);
    }
    return 0;
}*/

void kys()
{
	amAppInit();
	AM_DeleteTitle(MEDIATYPE_SD, 0x0000F0C5);
	amExit();
}

void rename()
{
       FSUSER_RenameFile(MEDIATYPE_SD, sdmc:/luma/titles/000400300000BE02/usa.ips, MEDIATYPE_SD, sdmc:/luma/titles/000400300000BE02/code.ips); 

int main()
{
	bool	isRunning = true;
	
	gfxInitDefault();
	httpcInit(0);
	
	consoleInit(GFX_TOP,NULL);
	
	printf("- foxverse OneClick-Setup %s -\n", g_version);
	printf("Press A to install foxverse\n");
	//printf("Press Y to check for updates.\n");
	printf("Press Start to exit.\n\n");
	
	gfxFlushBuffers();
	
	while (isRunning && aptMainLoop())
	{
		gspWaitForVBlank();
		hidScanInput();
		
		u32 kDown = hidKeysDown();
		
		/*if(kDown == KEY_Y)
		{
			printf("Checking for an update...\n");
			downloadUpdate();
		}*/
		
		if(kDown == KEY_A)
		{
			if(!DownloadFiles(USA && EUR && JAP && Cert))
			{
				printf("Killing Installer...\n");
				isRunning = false;
			}
		}
		
		if(kDown & KEY_START)
			break;
		
		gfxFlushBuffers();
		gfxSwapBuffers();
	}
	
	if(!isRunning)
	{
		svcSleepThread(2000000000);
		httpcExit();
		kys();
		gfxExit();
		return 0;
	}
}
