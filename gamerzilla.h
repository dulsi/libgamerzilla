#ifndef __gammerzilla_h__
#define __gamerzilla_h__

#include <stdbool.h>
#if defined(_WIN32)
#include <winsock2.h>
#else
#include <sys/select.h>
#endif

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

extern bool GamerzillaInit(bool server, const char *savedir);
extern bool GamerzillaConnect(const char *url, const char *username, const char *password);
extern int GamerzillaGameInit(Gamerzilla *g);
extern void GamerzillaGameAddTrophy(Gamerzilla *g, char *name, char *desc, int max_progress, char *true_image, char *false_image);
extern bool GamerzillaGetTrophy(int game_id, const char *name, bool *achieved);
extern bool GamerzillaGetTrophyStat(int game_id, const char *name, int *progress);
extern bool GamerzillaSetTrophy(int game_id, const char *name);
extern bool GamerzillaSetTrophyStat(int game_id, const char *name, int progress);
extern void GamerzillaServerProcess(struct timeval *timeout);
extern void GamerzillaQuit();

#ifdef __cplusplus
}
#endif

#endif
