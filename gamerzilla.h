#ifndef __gammerzilla_h__
#define __gamerzilla_h__

#include <stdbool.h>
#include <sys/select.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	char *name;
	char *desc;
	int max_progress;
	char *true_image;
	char *false_image;
} GamerzillaTrophy;

typedef struct
{
	char *short_name;
	char *name;
	char *image;
	int version;
	int numTrophy;
	GamerzillaTrophy *trophy;
} Gamerzilla;

bool GamerzillaInit(bool server, const char *savedir);
bool GamerzillaConnect(const char *url, const char *username, const char *password);
int GamerzillaGameInit(Gamerzilla *g);
bool GamerzillaGetTrophy(int game_id, const char *name, bool *achieved);
bool GamerzillaGetTrophyStat(int game_id, const char *name, int *progress);
bool GamerzillaSetTophy(int game_id, const char *name);
bool GamerzillaSetTophyStat(int game_id, const char *name, int progress);
void GamerzillaServerProcess(struct timeval *timeout);
void GamerzillaQuit();

#ifdef __cplusplus
}
#endif

#endif
