#ifndef __gammerzilla_h__
#define __gamerzilla_h__

#include <stdbool.h>
#if defined(_WIN32)
#include <winsock2.h>
#else
#include <sys/select.h>
#endif
#include <stdio.h>

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
	int szTrophy;
	GamerzillaTrophy *trophy;
} Gamerzilla;

extern bool GamerzillaStart(bool server, const char *savedir);
extern bool GamerzillaConnect(const char *url, const char *username, const char *password);
extern void GamerzillaInitGame(Gamerzilla *g);
extern int GamerzillaSetGame(Gamerzilla *g);
extern int GamerzillaSetGameFromFile(const char *filename, const char *datadir);
extern int GamerzillaGetGame(const char *short_name);
extern void GamerzillaFreeGame(int game_id);
extern void GamerzillaGameAddTrophy(Gamerzilla *g, char *name, char *desc, int max_progress, char *true_image, char *false_image);
extern char *GamerzillaGetGameImage(int game_id);
extern int GamerzillaGetTrophyNum(int game_id);
extern void GamerzillaGetTrophyByIndex(int game_id, int indx, char **name, char **desc);
extern bool GamerzillaGetTrophy(int game_id, const char *name, bool *achieved);
extern bool GamerzillaGetTrophyStat(int game_id, const char *name, int *progress);
extern char *GamerzillaGetTrophyImage(int game_id, const char *name, bool achieved);
extern bool GamerzillaSetTrophy(int game_id, const char *name);
extern bool GamerzillaSetTrophyStat(int game_id, const char *name, int progress);
extern void GamerzillaServerProcess(struct timeval *timeout);
extern void GamerzillaQuit();

extern void GamerzillaSetLog(int level, FILE *f);

#ifdef __cplusplus
}
#endif

#endif
