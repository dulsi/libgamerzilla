#include "gamerzilla.h"
#include <string.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	bool init = GamerzillaInit(true);
	if (!init)
	{
		fprintf(stderr, "Failed to start server\n");
		return 1;
	}
	GamerzillaConnect(argv[1], argv[2], argv[3]);
	while (true)
	{
		GamerzillaServerProcess(NULL);
	}
	return 0;
}
