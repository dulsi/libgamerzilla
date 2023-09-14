#include "gamerzilla.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

char *trimend(char *s)
{
	int i = strlen(s);
	for (; (i > 0) && (isspace(s[i - 1])); i--)
		s[i - 1] = 0;
	return s;
}

void PrintAccess(const char *short_name, const char *name, void *user_data)
{
	printf("Access: %s\n", name);
}

char *savePath[3] = {".local", "share", "gamerzillaserver"};

int main(int argc, char **argv)
{
	char *savedir = NULL;
#ifdef __unix__
	char *home = getenv("XDG_DATA_HOME");
	if (!home)
	{
		home = getenv("HOME");
		if (home)
		{
			int len = strlen(home);
			savedir = (char *)malloc(len + 100);
			strcpy(savedir, home);
			if (savedir[len - 1] != '/')
			{
				strcat(savedir, "/");
			}
			for (int i = 0; i < 3; i++)
			{
				strcat(savedir, savePath[i]);
				int err = mkdir(savedir, 0700);
				if ((-1 == err) && (EEXIST != errno))
				{
					fprintf(stderr, "Error creating directory %s\n", savedir);
					exit(2);
				}
				strcat(savedir, "/");
			}
		}
	}
	if (!home)
	{
		savedir = (char *)malloc(100);
		strcpy(savedir, "./gamerzillaserver/");
	}
#else
	savedir = strdup("./gamerzillaserver/");
#endif
	char *url = NULL;
	char *name = NULL;
	char *pswd = NULL;
	bool useConfig = true;
	bool useConnect = true;
	char *config  = "server.cfg";
	for (int i = 1; i < argc; i++)
	{
		if (0 == strcmp(argv[i], "--noconfig"))
			useConfig = false;
		else if (0 == strcmp(argv[i], "--noconnect"))
			useConnect = false;
		else if (0 == strcmp(argv[i], "--savepath"))
		{
			if (i + 1 < argc)
			{
				i++;
				savedir = strdup(argv[i]);
			}
			else
			{
				fprintf(stderr, "Failed to include path argument.\n");
				exit(3);
			}
		}
		else
			config = argv[i];
	}
	char *fullconfig = (char *)malloc(strlen(savedir) + strlen(config) + 1);
	strcpy(fullconfig, savedir);
	strcat(fullconfig, config);
	if (useConfig && useConnect)
	{
		char tmp[201];
		FILE *f = fopen(fullconfig, "r");
		if ((f) && (NULL != fgets(tmp, 200, f)))
		{
			url = strdup(trimend(tmp));
			if (NULL != fgets(tmp, 200, f))
				name = strdup(trimend(tmp));
			if (NULL != fgets(tmp, 200, f))
				pswd = strdup(trimend(tmp));
		}
	}
	if (useConnect && (url == NULL))
	{
		char tmp[201];
		printf("Hubzilla URL including trailing '/': ");
		fgets(tmp, 200, stdin);
		url = strdup(trimend(tmp));
		printf("Username: ");
		fgets(tmp, 200, stdin);
		name = strdup(trimend(tmp));
		printf("Password: ");
		fgets(tmp, 200, stdin);
		pswd = strdup(trimend(tmp));
		if (useConfig)
		{
			FILE *f = fopen(fullconfig, "w");
			fputs(url, f);
			fputs("\n", f);
			fputs(name, f);
			fputs("\n", f);
			fputs(pswd, f);
			fputs("\n", f);
			fclose(f);
		}
	}
	bool init = GamerzillaStart(true, savedir);
	GamerzillaSetLog(3, stdout);
	if (!init)
	{
		fprintf(stderr, "Failed to start server\n");
		return 1;
	}
	if ((useConnect) && (url != NULL))
		GamerzillaConnect(url, name, pswd);
	GamerzillaServerListen(&PrintAccess, NULL);
	while (true)
	{
		GamerzillaServerProcess(NULL);
	}
	return 0;
}
