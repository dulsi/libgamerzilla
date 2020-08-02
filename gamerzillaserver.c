#include "gamerzilla.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

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

int main(int argc, char **argv)
{
	char *savedir = strdup("./server/");
	char *url = NULL;
	char *name = NULL;
	char *pswd = NULL;
	if (argc == 2)
	{
		char tmp[200];
		FILE *f = fopen(argv[1], "r");
		if ((f) && (NULL != fgets(tmp, 200, f)))
		{
			savedir = strdup(trimend(tmp));
			if (NULL != fgets(tmp, 200, f))
				url = strdup(trimend(tmp));
			if (NULL != fgets(tmp, 200, f))
				name = strdup(trimend(tmp));
			if (NULL != fgets(tmp, 200, f))
				pswd = strdup(trimend(tmp));
		}
	}
	bool init = GamerzillaStart(true, savedir);
	GamerzillaSetLog(1, stdout);
	if (!init)
	{
		fprintf(stderr, "Failed to start server\n");
		return 1;
	}
	if (argc == 4)
	{
		url = strdup(argv[1]);
		name = strdup(argv[2]);
		pswd = strdup(argv[3]);
	}
	if (url != NULL)
		GamerzillaConnect(url, name, pswd);
	GamerzillaServerListen(&PrintAccess, NULL);
	while (true)
	{
		GamerzillaServerProcess(NULL);
	}
	return 0;
}
