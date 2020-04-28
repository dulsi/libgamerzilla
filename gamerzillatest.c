#include "gamerzilla.h"
#include <string.h>

int main(int argc, char **argv)
{
	Gamerzilla g;
	GamerzillaTrophy trophy[5];
	g.short_name = strdup("test");
	g.name = strdup("Test");
	g.image = strdup("test.png");
	g.version = 1;
	g.numTrophy = 5;
	g.trophy = trophy;
	trophy[0].name = strdup("Win Game");
	trophy[0].desc = strdup("Win Game");
	trophy[0].max_progress = 0;
	trophy[1].name = strdup("Slayer");
	trophy[1].desc = strdup("Defeat 100 monsters");
	trophy[1].max_progress = 100;
	trophy[2].name = strdup("Game Master");
	trophy[2].desc = strdup("Solve 10 puzzles");
	trophy[2].max_progress = 10;
	trophy[3].name = strdup("Explosive");
	trophy[3].desc = strdup("Kill 5 enemies in one explosion");
	trophy[3].max_progress = 0;
	trophy[4].name = strdup("Untouchable");
	trophy[4].desc = strdup("Defeat boss without taking damage");
	trophy[4].max_progress = 0;
	GamerzillaInit(false);
	if (argc == 4)
		GamerzillaConnect(argv[1], argv[2], argv[3]);
	int game_id = GamerzillaGameInit(&g);
	GamerzillaSetTophy(game_id, "Slayer");
	return 0;
}
