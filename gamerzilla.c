#define GAMERZILLA_BUILD

#include <gamerzilla.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <jansson.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

#ifndef _MSC_VER

#include <unistd.h>
#include <dirent.h>

#else

#include <windows.h>

#define DIR void

struct dirent
{
	char d_name[256];
};

static HANDLE dirEntry = INVALID_HANDLE_VALUE;
static WIN32_FIND_DATA fileData;
static struct dirent fileData2;

static DIR *opendir(const char *name)
{
	if (dirEntry != INVALID_HANDLE_VALUE)
		FindClose(dirEntry);
	char *path = malloc(strlen(name) + 2);
	strcpy(path, name);
	strcat(path, ".");
	memset(&fileData, 0, sizeof fileData);
	dirEntry = FindFirstFile(path, &fileData);
	if (dirEntry == INVALID_HANDLE_VALUE)
		return 0;
	else
		return &dirEntry;
}

static struct dirent *readdir(DIR *dirp)
{
	strcpy(fileData2.d_name, fileData.cFileName);
	if (fileData2.d_name[0] == 0)
		return 0;
	else
	{
		memset(&fileData, 0, sizeof fileData);
		FindNextFile(dirEntry, &fileData);
		return &fileData2;
	}
}

#endif

#ifdef _WIN32
#define GAMERZILLA_USETCP

#define mkdir(x,y) _mkdir(x)
#define writesocket(a,b,c) send(a,(const char *)b,c,0)
#define readsocket(a,b,c) recv(a,(char *)b,c,0)
#endif

#define GAMERZILLA_TCPPORT 56924

#ifndef _WIN32
#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/socket.h>

#define SOCKET int
#define writesocket(a,b,c) write(a,b,c)
#define readsocket(a,b,c) read(a,b,c)
#define closesocket(x) close(x)
#define INVALID_SOCKET -1
#endif

#define MODE_OFFLINE 0
#define MODE_SERVEROFFLINE 1
#define MODE_CONNECTED 2
#define MODE_STANDALONE 3
#define MODE_SERVERONLINE 4

#define CMD_QUIT 0
#define CMD_GETGAMEINFO 1
#define CMD_SETGAMEINFO 2
#define CMD_SETTROPHY 3
#define CMD_SETGAMEIMAGE 4
#define CMD_SETTROPHYSTAT 5
#define CMD_SETTROPHYIMAGE 6
#define MAX_CMD 7

#define GAMEID_CURRENT 999999
#define GAMEID_ERROR -1

#define GamerzillaLog(l, s1, s2) \
	{ \
		if (logLevel >= l) \
		{ \
			fprintf(logFile, "%s%s\n", (s1), (s2)); \
			fflush(logFile); \
		} \
	}

static const char *cmdName[] = {
	"Quit",
	"Get Game Info",
	"Set Game Info",
	"Set Trophy",
	"Set Game Image",
	"Set Trophy Stat",
	"Set Trophy Image"
};

typedef struct {
	bool achieved;
	int progress;
} TrophyData;

static int mode = MODE_OFFLINE;
static char *burl = NULL;
static char *uname = NULL;
static char *pswd = NULL;
static int game_num = 0;
static int game_sz = 0;
static char **game_list = NULL;
static Gamerzilla **game_info = NULL;
static TrophyData **game_data = NULL;
static Gamerzilla current;
static TrophyData *currentData = NULL;
static int remote_game_id = -1;
static SOCKET server_socket = INVALID_SOCKET;
static SOCKET *client_socket = NULL;
static int num_client = 0;
static int size_client = 0;
static CURL *curl[2] = { NULL, NULL };
static char *save_dir = NULL;
static int logLevel = 0;
static FILE *logFile = NULL;
static GamerzillaAccessGame accessCallback = NULL;
static void *accessData = NULL;

size_t fileSize(const char *filename)
{
	struct stat statbuf;
	if (0 == stat(filename, &statbuf))
	{
		return statbuf.st_size;
	}
	return 0;
}

void *fileOpen(const char *filename)
{
	FILE *f = fopen(filename, "rb");
	return f;
}

size_t fileRead(void *fd, void *buf, size_t count)
{
	return fread(buf, 1, count, (FILE *)fd);
}

void fileClose(void *fd)
{
	fclose((FILE *)fd);
}

static GamerzillaSize game_size = &fileSize;
static GamerzillaOpen game_open = &fileOpen;
static GamerzillaRead game_read = &fileRead;
static GamerzillaClose game_close = &fileClose;

typedef struct {
	size_t size;
	size_t len;
	char *data;
} content;

static void content_init(content *x)
{
	x->size = 0;
	x->len = 0;
	x->data = 0;
}

static void content_destroy(content *x)
{
	x->size = 0;
	x->len = 0;
	if (x->data)
		free(x->data);
	x->data = 0;
}

static void content_size(content *x, ssize_t sz)
{
	if (x->size == 0)
	{
		x->size = 1024;
	}
	while (x->size < sz)
	{
		x->size = x->size * 2;
	}
	char *dataOld = x->data;
	x->data = (char *)malloc(x->size);
	memcpy(x->data, dataOld, x->len);
	free(dataOld);
}

static void content_read(int fd, content *x, ssize_t sz)
{
	content_size(x, sz);
	ssize_t ct;
	for (ct = 0; ct < sz; )
	{
		ct += readsocket(fd, x->data + ct, sz - ct);
	}
	x->len = sz;
}

static void createImagePath(char *name)
{
	char *fullpath = malloc(strlen(save_dir) + strlen(name) + 15);
	strcpy(fullpath, save_dir);
	strcat(fullpath, "image/");
	strcat(fullpath, name);
	struct stat statbuf;
	if (0 != stat(fullpath, &statbuf))
	{
		strcat(fullpath, "/");
		int save_len = strlen(save_dir);
		int len = strlen(fullpath) - save_len;
		for (int i = 0; i < len; i++)
		{
			if (fullpath[save_len + i] == '/')
			{
				fullpath[save_len + i] = 0;
				if (0 != stat(fullpath, &statbuf))
				{
					mkdir(fullpath, S_IRWXU);
				}
				fullpath[save_len + i] = '/';
			}
		} 
	}
}

static size_t curlWriteData(void *buffer, size_t size, size_t nmemb, void *userp)
{
	content *response = (content *)userp;
	if (response->size - response->len < size * nmemb)
	{
		content_size(response, response->len + size * nmemb);
	}
	memcpy(response->data + response->len, buffer, size * nmemb);
	response->len += size * nmemb;
	return nmemb;
}

static size_t curlWriteFile(void *ptr, size_t size, size_t nmemb, void *fd)
{
	size_t written = fwrite(ptr, 1, size * nmemb, ((FILE *)fd));
	return written;
}

size_t jsonRead(void *buffer, size_t buflen, void *data)
{
	return (*game_read)(data, buffer, buflen);
}

static void gamerzillaClear(Gamerzilla *g, bool memFree)
{
	if (memFree)
	{
		if (g->short_name)
			free(g->short_name);
		if (g->name)
			free(g->name);
		if (g->image)
			free(g->image);
		for (int i = 0; i < g->numTrophy; i++)
		{
			if (g->trophy[i].name)
				free(g->trophy[i].name);
			g->trophy[i].name = NULL;
			if (g->trophy[i].desc)
				free(g->trophy[i].desc);
			g->trophy[i].desc = NULL;
			if (g->trophy[i].true_image)
				free(g->trophy[i].true_image);
			g->trophy[i].true_image = NULL;
			if (g->trophy[i].false_image)
				free(g->trophy[i].false_image);
			g->trophy[i].false_image = NULL;
		}
		free(g->trophy);
	}
	g->short_name = NULL;
	g->name = NULL;
	g->image = NULL;
	g->version = 0;
	g->numTrophy = 0;
	g->szTrophy = 0;
	g->trophy = NULL;
}

bool GamerzillaStart(bool server, const char *savedir)
{
	current.short_name = NULL;
	if (savedir[0] == '~')
	{
		char *home = getenv("HOME");
		save_dir = malloc(strlen(home) + strlen(savedir));
		strcpy(save_dir, home);
		strcat(save_dir, savedir + 1);
	}
	else
		save_dir = strdup(savedir);
	int len = strlen(save_dir);
	struct stat statbuf;
	if (0 != stat(save_dir, &statbuf))
	{
		int i;
		for (i = len - 1; i > 0; i--)
		{
			if (save_dir[i] == '/')
			{
				save_dir[i] = 0;
				if (0 == stat(save_dir, &statbuf))
				{
					save_dir[i] = '/';
					break;
				}
			}
		}
		while (i < len)
		{
			if (save_dir[i] == 0)
			{
				mkdir(save_dir, S_IRWXU);
				save_dir[i] = '/';
			}
			i++;
		}
	}
	if (server)
	{
		// Initialize socket
		mode = MODE_SERVEROFFLINE;
#ifdef GAMERZILLA_USETCP
#ifdef _WIN32
		WSADATA wsaData;
		int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
		if (iResult != 0)
			return false;
#endif
		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(GAMERZILLA_TCPPORT);
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		server_socket = socket(AF_INET, SOCK_STREAM, 0);
		if (server_socket == INVALID_SOCKET)
			return false;
		int optval = 1;
		setsockopt(server_socket,SOL_SOCKET,SO_REUSEADDR,(const char*)&optval,sizeof(optval));
		if (bind(server_socket, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)) == -1)
			return false;
#else
		struct sockaddr_un addr;
		memset(&addr, 0, sizeof(struct sockaddr_un));
		addr.sun_family = AF_UNIX;
		int len = sprintf(&addr.sun_path[1], "Gamerzilla%d", geteuid());
		server_socket = socket(AF_UNIX, SOCK_STREAM, 0);
		if (server_socket == INVALID_SOCKET)
		{
			fprintf(stderr, "Invalid socket\n");
			return false;
		}
		if (bind(server_socket, (struct sockaddr *) &addr, sizeof(sa_family_t) + len + 1) == -1)
		{
			fprintf(stderr, "Bind failed\n");
			return false;
		}
#endif
		if (listen(server_socket, 5) == -1)
			return false;
		return true;
	}
	else
	{
		// Try connect to server
#ifdef GAMERZILLA_USETCP
#ifdef _WIN32
		WSADATA wsaData;
		int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
		if (iResult != 0)
			return false;
#endif
		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(GAMERZILLA_TCPPORT);
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		server_socket = socket(AF_INET, SOCK_STREAM, 0);
		if (server_socket == -1)
			return false;
		if (connect(server_socket, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) != -1)
		{
			mode = MODE_CONNECTED;
			return true;
		}
#else
		struct sockaddr_un addr;
		memset(&addr, 0, sizeof(struct sockaddr_un));
		addr.sun_family = AF_UNIX;
		int len = sprintf(&addr.sun_path[1], "Gamerzilla%d", geteuid());
		server_socket = socket(AF_UNIX, SOCK_STREAM, 0);
		if (server_socket == -1)
			return false;
		if (connect(server_socket, (struct sockaddr*)&addr, sizeof(sa_family_t) + len + 1) != -1)
		{
			mode = MODE_CONNECTED;
			return true;
		}
#endif
	}
	return false;
}

static content GamerzillaGetGameInfo_internal(CURL *c, const char *name, bool *success)
{
	content internal_struct;
	content_init(&internal_struct);
	if ((mode == MODE_STANDALONE) || (mode == MODE_SERVERONLINE))
	{
		// Get game info
		char *esc_name = curl_easy_escape(c, name, 0);
		char *postdata = malloc(strlen(esc_name) + 20);
		strcpy(postdata, "game=");
		strcat(postdata, esc_name);
		curl_free(esc_name);
		char *url, *userpwd;
		url = malloc(strlen(burl) + 20);
		strcpy(url, burl);
		strcat(url, "api/gamerzilla/game");
		curl_easy_setopt(c, CURLOPT_URL, url);
		curl_easy_setopt(c, CURLOPT_POSTFIELDS, postdata);
		curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, strlen(postdata));
		userpwd = malloc(strlen(uname) + strlen(pswd) + 2);
		strcpy(userpwd, uname);
		strcat(userpwd, ":");
		strcat(userpwd, pswd);
		curl_easy_setopt(c, CURLOPT_USERPWD, userpwd);
		curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curlWriteData);
		curl_easy_setopt(c, CURLOPT_WRITEDATA, &internal_struct);
		CURLcode res = curl_easy_perform(c);
		if (res != CURLE_OK)
		{
			if (success)
				*success = false;
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		}
		else if (success)
			*success = true;
		free(url);
		free(userpwd);
		free(postdata);
	}
	else if (mode == MODE_CONNECTED)
	{
		uint8_t cmd = CMD_GETGAMEINFO;
		uint32_t hostsz = strlen(name);
		uint32_t sz = htonl(hostsz);
		writesocket(server_socket, &cmd, sizeof(cmd));
		writesocket(server_socket, &sz, sizeof(sz));
		writesocket(server_socket, name, hostsz);
		ssize_t ct = 0;
		while (ct != sizeof(sz))
		{
			ct += readsocket(server_socket, ((char *)&sz) + ct, sizeof(sz) - ct);
		}
		hostsz = ntohl(sz);
		content_read(server_socket, &internal_struct, hostsz);
	}
	return internal_struct;
}

static char *GamerzillaGetGameImage_internal(const char *game_name)
{
	char *filename = malloc(strlen(save_dir) + 30 + strlen(game_name) * 2);
	strcpy(filename, save_dir);
	strcat(filename, "/image/");
	strcat(filename, game_name);
	strcat(filename, "/");
	strcat(filename, game_name);
	strcat(filename, ".png");
	return filename;
}

static char *GamerzillaGetTrophyImage_internal(const char *game_name, const char *name, bool achieved)
{
	char *filename = malloc(strlen(save_dir) + 30 + strlen(game_name) + strlen(name));
	strcpy(filename, save_dir);
	strcat(filename, "/image/");
	strcat(filename, game_name);
	strcat(filename, "/");
	int end = strlen(filename);
	for (int k = 0; name[k]; k++)
	{
		if (isalnum(name[k]))
		{
			filename[end] = name[k];
		}
		else
		{
			filename[end] = '_';
		}
		end++;
	}
	filename[end] = 0;
	if (achieved)
		strcat(filename, "1.png");
	else
		strcat(filename, "0.png");
	return filename;
}

static void GamerzillaUpdateImages(CURL *c, const char *game_name, Gamerzilla *g)
{
	char *postdata;
	char *url, *userpwd;
	url = malloc(strlen(burl) + 40);
	strcpy(url, burl);
	strcat(url, "api/gamerzilla/game/image/show");
	char *esc_name = curl_easy_escape(c, game_name, 0);
	postdata = malloc(strlen(esc_name) + 10);
	strcpy(postdata, "game=");
	strcat(postdata, esc_name);
	curl_easy_setopt(c, CURLOPT_URL, url);
	curl_easy_setopt(c, CURLOPT_POSTFIELDS, postdata);
	curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, strlen(postdata));
	userpwd = malloc(strlen(uname) + strlen(pswd) + 2);
	strcpy(userpwd, uname);
	strcat(userpwd, ":");
	strcat(userpwd, pswd);
	curl_easy_setopt(c, CURLOPT_USERPWD, userpwd);
	createImagePath(g->short_name);
	char *filename = malloc(strlen(save_dir) + 30 + strlen(g->short_name) * 2);
	strcpy(filename, save_dir);
	strcat(filename, "/image/");
	strcat(filename, g->short_name);
	strcat(filename, "/");
	strcat(filename, g->short_name);
	strcat(filename, ".png");
	FILE *f = fopen(filename, "wb");
	if (f)
	{
		curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curlWriteFile);
		curl_easy_setopt(c, CURLOPT_WRITEDATA, f);
		CURLcode res = curl_easy_perform(c);
		if (res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		fclose(f);
	}
	for (int i = 0; i < g->numTrophy; i++)
	{
		strcpy(url, burl);
		strcat(url, "api/gamerzilla/trophy/image/show");
		char *esc_trophy = curl_easy_escape(c, g->trophy[i].name, 0);
		postdata = malloc(strlen(esc_name) + strlen(esc_trophy) + 30);
		strcpy(postdata, "game=");
		strcat(postdata, esc_name);
		strcat(postdata, "&trophy=");
		strcat(postdata, esc_trophy);
		strcat(postdata, "&achieved=1");
		curl_free(esc_trophy);
		curl_easy_setopt(c, CURLOPT_URL, url);
		curl_easy_setopt(c, CURLOPT_POSTFIELDS, postdata);
		curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, strlen(postdata));
		curl_easy_setopt(c, CURLOPT_USERPWD, userpwd);
		free(filename);
		filename = GamerzillaGetTrophyImage_internal(g->short_name, g->trophy[i].name, true);
		f = fopen(filename, "wb");
		if (f)
		{
			curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curlWriteFile);
			curl_easy_setopt(c, CURLOPT_WRITEDATA, f);
			CURLcode res = curl_easy_perform(c);
			if (res != CURLE_OK)
				fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
			fclose(f);
		}
		postdata[strlen(postdata) - 1] = '0';
		curl_easy_setopt(c, CURLOPT_URL, url);
		curl_easy_setopt(c, CURLOPT_POSTFIELDS, postdata);
		curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, strlen(postdata));
		curl_easy_setopt(c, CURLOPT_USERPWD, userpwd);
		filename[strlen(filename) - 5] = '0';
		f = fopen(filename, "wb");
		if (f)
		{
			curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curlWriteFile);
			curl_easy_setopt(c, CURLOPT_WRITEDATA, f);
			CURLcode res = curl_easy_perform(c);
			if (res != CURLE_OK)
				fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
			fclose(f);
		}
	}
	curl_free(esc_name);
	free(url);
	free(filename);
	free(postdata);
	free(userpwd);
}

static void GamerzillaSetTrophy_internal(CURL *c, const char *game_name, const char *name)
{
	if ((mode == MODE_STANDALONE) || (mode == MODE_SERVERONLINE))
	{
		char *esc_game_name = curl_easy_escape(c, game_name, 0);
		char *esc_name = curl_easy_escape(c, name, 0);
		char *postdata = malloc(strlen(esc_game_name) + strlen(esc_name) + 30);
		strcpy(postdata, "game=");
		strcat(postdata, esc_game_name);
		strcat(postdata, "&trophy=");
		strcat(postdata, esc_name);
		curl_free(esc_game_name);
		curl_free(esc_name);
		content internal_struct;
		char *url, *userpwd;
		content_init(&internal_struct);
		url = malloc(strlen(burl) + 40);
		strcpy(url, burl);
		strcat(url, "api/gamerzilla/trophy/set");
		curl_easy_setopt(c, CURLOPT_URL, url);
		curl_easy_setopt(c, CURLOPT_POSTFIELDS, postdata);
		curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, strlen(postdata));
		userpwd = malloc(strlen(uname) + strlen(pswd) + 2);
		strcpy(userpwd, uname);
		strcat(userpwd, ":");
		strcat(userpwd, pswd);
		curl_easy_setopt(c, CURLOPT_USERPWD, userpwd);
		curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curlWriteData);
		curl_easy_setopt(c, CURLOPT_WRITEDATA, &internal_struct);
		CURLcode res = curl_easy_perform(c);
		if (res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		free(url);
		free(userpwd);
		content_destroy(&internal_struct);
		free(postdata);
	}
	else if (mode == MODE_CONNECTED)
	{
		uint8_t cmd = CMD_SETTROPHY;
		uint32_t hostsz = strlen(game_name);
		uint32_t sz = htonl(hostsz);
		writesocket(server_socket, &cmd, sizeof(cmd));
		writesocket(server_socket, &sz, sizeof(sz));
		writesocket(server_socket, game_name, hostsz);
		hostsz = strlen(name);
		sz = htonl(hostsz);
		writesocket(server_socket, &sz, sizeof(sz));
		writesocket(server_socket, name, hostsz);
	}
}

static void GamerzillaSetTrophyStat_internal(CURL *c, const char *game_name, const char *name, int progress)
{
	if ((mode == MODE_STANDALONE) || (mode == MODE_SERVERONLINE))
	{
		char *esc_game_name = curl_easy_escape(c, game_name, 0);
		char *esc_name = curl_easy_escape(c, name, 0);
		char *postdata = malloc(strlen(esc_game_name) + strlen(esc_name) + 100);
		strcpy(postdata, "game=");
		strcat(postdata, esc_game_name);
		strcat(postdata, "&trophy=");
		strcat(postdata, esc_name);
		strcat(postdata, "&progress=");
		sprintf(postdata + strlen(postdata), "%d", progress);
		curl_free(esc_game_name);
		curl_free(esc_name);
		content internal_struct;
		char *url, *userpwd;
		content_init(&internal_struct);
		url = malloc(strlen(burl) + 40);
		strcpy(url, burl);
		strcat(url, "api/gamerzilla/trophy/set/stat");
		curl_easy_setopt(c, CURLOPT_URL, url);
		curl_easy_setopt(c, CURLOPT_POSTFIELDS, postdata);
		curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, strlen(postdata));
		userpwd = malloc(strlen(uname) + strlen(pswd) + 2);
		strcpy(userpwd, uname);
		strcat(userpwd, ":");
		strcat(userpwd, pswd);
		curl_easy_setopt(c, CURLOPT_USERPWD, userpwd);
		curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curlWriteData);
		curl_easy_setopt(c, CURLOPT_WRITEDATA, &internal_struct);
		CURLcode res = curl_easy_perform(c);
		if (res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		free(url);
		free(userpwd);
		content_destroy(&internal_struct);
		free(postdata);
	}
	else if (mode == MODE_CONNECTED)
	{
		uint8_t cmd = CMD_SETTROPHYSTAT;
		uint32_t hostsz = strlen(game_name);
		uint32_t sz = htonl(hostsz);
		writesocket(server_socket, &cmd, sizeof(cmd));
		writesocket(server_socket, &sz, sizeof(sz));
		writesocket(server_socket, game_name, hostsz);
		hostsz = strlen(name);
		sz = htonl(hostsz);
		writesocket(server_socket, &sz, sizeof(sz));
		writesocket(server_socket, name, hostsz);
		hostsz = progress;
		sz = htonl(hostsz);
		writesocket(server_socket, &sz, sizeof(sz));
	}
}

static json_t *gamefile_read(const char *shortname)
{
	char *filename = (char *)malloc(strlen(save_dir) + strlen(shortname) + 10);
	strcpy(filename, save_dir);
	strcat(filename, shortname);
	strcat(filename, ".game");
	json_error_t error;
	json_t *root = json_load_file(filename, 0, &error);
	return root;
}

static json_t *GamerzillaJson(Gamerzilla *g, TrophyData *t, bool incomplete)
{
	char tmp[512];
	json_t *root = json_object();
	json_object_set_new(root, "shortname", json_string(g->short_name));
	json_object_set_new(root, "name", json_string(g->name));
	json_object_set_new(root, "image", json_string(g->image));
	sprintf(tmp, "%d", g->version);
	json_object_set_new(root, "version", json_string(tmp));
	if (incomplete)
	{
		json_object_set_new(root, "incomplete", json_string("1"));
	}
	json_t *trophy = json_array();
	json_object_set_new(root, "trophy", trophy);
	for (int i = 0; i < g->numTrophy; i++)
	{
		json_t *curTrophy = json_object();
		json_array_append_new(trophy, curTrophy);
		json_object_set_new(curTrophy, "trophy_name", json_string(g->trophy[i].name));
		json_object_set_new(curTrophy, "trophy_desc", json_string(g->trophy[i].desc));
		sprintf(tmp, "%d", g->trophy[i].max_progress);
		json_object_set_new(curTrophy, "max_progress", json_string(tmp));
		sprintf(tmp, "%d", t[i].achieved);
		json_object_set_new(curTrophy, "achieved", json_string(tmp));
		sprintf(tmp, "%d", t[i].progress);
		json_object_set_new(curTrophy, "progress", json_string(tmp));
	}
	return root;
}

static void gamefile_write(Gamerzilla *g, TrophyData *t)
{
	char *filename = (char *)malloc(strlen(save_dir) + strlen(g->short_name) + 10);
	strcpy(filename, save_dir);
	strcat(filename, g->short_name);
	strcat(filename, ".game");
	json_t *root = GamerzillaJson(g, t, false);
	json_dump_file(root, filename, 0);
	json_decref(root);
}

static bool GamerzillaMerge(CURL *c, Gamerzilla *g, TrophyData **t, json_t *root)
{
	bool update = false;
	json_t *ver = json_object_get(root, "version");
	if (json_is_string(ver))
	{
		int dbVersion = atol(json_string_value(ver));
		if (dbVersion > g->version)
		{
			g->version = dbVersion;
			update = true;
		}
	}
	if (g->short_name == NULL)
	{
		json_t *tmp = json_object_get(root, "shortname");
		if (json_is_string(tmp))
		{
			g->short_name = strdup(json_string_value(tmp));
		}
		tmp = json_object_get(root, "name");
		if (json_is_string(tmp))
		{
			g->name = strdup(json_string_value(tmp));
		}
		tmp = json_object_get(root, "image");
		if (json_is_string(tmp))
		{
			g->image = strdup(json_string_value(tmp));
		}
		else
		{
			g->image = strdup("image.png");
		}
	}
	json_t *tr = json_object_get(root, "trophy");
	if (json_is_array(tr))
	{
		int num = json_array_size(tr);
		if (update)
		{
			if (num > g->numTrophy)
			{
				TrophyData *newT = calloc(num, sizeof(TrophyData));
				GamerzillaTrophy *newTrophy = calloc(num, sizeof(GamerzillaTrophy));
				for (int i = 0; i <num; i++)
				{
					json_t *item = json_array_get(tr, i);
					json_t *tmp = json_object_get(item, "trophy_name");
					newTrophy[i].name = strdup(json_string_value(tmp));
					tmp = json_object_get(item, "trophy_desc");
					newTrophy[i].desc = strdup(json_string_value(tmp));
					tmp = json_object_get(item, "max_progress");
					newTrophy[i].max_progress = atol(json_string_value(tmp));
					tmp = json_object_get(item, "trueimage");
					if (json_is_string(tmp))
					{
						newTrophy[i].true_image = strdup(json_string_value(tmp));
					}
					else
					{
						newTrophy[i].true_image = strdup("trueimage.png");
					}
					tmp = json_object_get(item, "falseimage");
					if (json_is_string(tmp))
					{
						newTrophy[i].false_image = strdup(json_string_value(tmp));
					}
					else
					{
						newTrophy[i].false_image = strdup("falseimage.png");
					}
					for (int k = 0; k < g->numTrophy; k++)
					{
						if (0 == strcmp(newTrophy[i].name, g->trophy[k].name))
						{
							newT[i].achieved = (*t)[k].achieved;
							newT[i].progress = (*t)[k].progress;
							break;
						}
					}
				}
				if (*t)
					free(*t);
				for (int i = 0; i < g->numTrophy; i++)
				{
					free(g->trophy[i].name);
					g->trophy[i].name = NULL;
					free(g->trophy[i].desc);
					g->trophy[i].desc = NULL;
				}
				if (g->trophy)
					free(g->trophy);
				g->numTrophy = num;
				g->trophy = newTrophy;
				*t = newT;
			}
		}
		update = false; // Determine if any updates are needed
		for (int i = 0; i <num; i++)
		{
			json_t *item = json_array_get(tr, i);
			json_t *tmp = json_object_get(item, "trophy_name");
			const char *trophy_name = json_string_value(tmp);
			for (int k = 0; k < num; k++)
			{
				if (0 == strcmp(trophy_name, g->trophy[k].name))
				{
					tmp = json_object_get(item, "achieved");
					int tmpVal;
					if (tmp)
					{
						tmpVal = atol(json_string_value(tmp));
						if (((*t)[k].achieved == false) && (tmpVal))
						{
							(*t)[k].achieved = tmpVal;
							update = true;
						}
						else if ((tmpVal == false) && ((*t)[k].achieved))
							GamerzillaSetTrophy_internal(c, g->short_name, g->trophy[k].name);
					}
					tmp = json_object_get(item, "progress");
					if (tmp)
					{
						if (json_is_null(tmp))
							tmpVal = 0;
						else
							tmpVal = atol(json_string_value(tmp));
						if (tmpVal > (*t)[k].progress)
						{
							(*t)[k].progress = tmpVal;
							update = true;
						}
						else if (tmpVal < (*t)[k].progress)
							GamerzillaSetTrophyStat_internal(c, g->short_name, g->trophy[k].name, (*t)[k].progress);
					}
				}
			}
		}
	}
	return update;
}

static void GamerzillaSetGameInfo_internal(CURL *c, Gamerzilla *g)
{
	char *postdata;
	char tmp[512];
	json_t *root = json_object();
	json_object_set_new(root, "shortname", json_string(g->short_name));
	json_object_set_new(root, "name", json_string(g->name));
	sprintf(tmp, "%d", g->version);
	json_object_set_new(root, "version", json_string(tmp));
	json_t *trophy = json_array();
	json_object_set_new(root, "trophy", trophy);
	for (int i = 0; i < g->numTrophy; i++)
	{
		json_t *curTrophy = json_object();
		json_array_append_new(trophy, curTrophy);
		json_object_set_new(curTrophy, "trophy_name", json_string(g->trophy[i].name));
		json_object_set_new(curTrophy, "trophy_desc", json_string(g->trophy[i].desc));
		sprintf(tmp, "%d", g->trophy[i].max_progress);
		json_object_set_new(curTrophy, "max_progress", json_string(tmp));
	}
	char *response = json_dumps(root, 0);
	json_decref(root);
	if ((mode == MODE_STANDALONE) || (mode == MODE_SERVERONLINE))
	{
		char *esc_response = curl_easy_escape(c, response, 0);
		postdata = malloc(strlen(esc_response) + 20);
		strcpy(postdata, "game=");
		strcat(postdata, esc_response);
		curl_free(esc_response);
		content internal_struct;
		char *url, *userpwd;
		content_init(&internal_struct);
		url = malloc(strlen(burl) + 30);
		strcpy(url, burl);
		strcat(url, "api/gamerzilla/game/add");
		curl_easy_setopt(c, CURLOPT_URL, url);
		curl_easy_setopt(c, CURLOPT_POSTFIELDS, postdata);
		curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, strlen(postdata));
		userpwd = malloc(strlen(uname) + strlen(pswd) + 2);
		strcpy(userpwd, uname);
		strcat(userpwd, ":");
		strcat(userpwd, pswd);
		curl_easy_setopt(c, CURLOPT_USERPWD, userpwd);
		curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curlWriteData);
		curl_easy_setopt(c, CURLOPT_WRITEDATA, &internal_struct);
		CURLcode res = curl_easy_perform(c);
		if (res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		free(url);
		free(userpwd);
		free(postdata);
		content_destroy(&internal_struct);
	}
	else if (mode == MODE_CONNECTED)
	{
		uint8_t cmd = CMD_SETGAMEINFO;
		uint32_t hostsz = strlen(response);
		uint32_t sz = htonl(hostsz);
		writesocket(server_socket, &cmd, sizeof(cmd));
		writesocket(server_socket, &sz, sizeof(sz));
		writesocket(server_socket, response, hostsz);
	}
	free(response);
	if ((mode == MODE_STANDALONE) || (mode == MODE_SERVERONLINE))
	{
		content internal_struct;
		char *url, *userpwd;
		curl_mime *form = NULL;
		curl_mimepart *field = NULL;
		content_init(&internal_struct);
		url = malloc(strlen(burl) + 40);
		strcpy(url, burl);
		strcat(url, "api/gamerzilla/game/image");
		curl_easy_setopt(c, CURLOPT_URL, url);
		form = curl_mime_init(c);
		field = curl_mime_addpart(form);
		curl_mime_name(field, "imagefile");
		if (mode == MODE_SERVERONLINE)
		{
			char *filename = GamerzillaGetGameImage_internal(g->short_name);
			curl_mime_filedata(field, filename);
			free(filename);
		}
		else
			curl_mime_filedata(field, g->image);
		field = curl_mime_addpart(form);
		curl_mime_name(field, "game");
		curl_mime_data(field, g->short_name, CURL_ZERO_TERMINATED);
		curl_easy_setopt(c, CURLOPT_MIMEPOST, form);
		userpwd = malloc(strlen(uname) + strlen(pswd) + 2);
		strcpy(userpwd, uname);
		strcat(userpwd, ":");
		strcat(userpwd, pswd);
		curl_easy_setopt(c, CURLOPT_USERPWD, userpwd);
		curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curlWriteData);
		curl_easy_setopt(c, CURLOPT_WRITEDATA, &internal_struct);
		CURLcode res = curl_easy_perform(c);
		if (res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		curl_mime_free(form);
		for (int i = 0; i < g->numTrophy; i++)
		{
			internal_struct.len = 0;
			strcpy(url, burl);
			strcat(url, "api/gamerzilla/trophy/image");
			curl_easy_setopt(c, CURLOPT_URL, url);
			form = curl_mime_init(c);
			field = curl_mime_addpart(form);
			curl_mime_name(field, "trueimagefile");
			if (mode == MODE_SERVERONLINE)
			{
				char *filename = GamerzillaGetTrophyImage_internal(g->short_name, g->trophy[i].name, true);
				curl_mime_filedata(field, filename);
				free(filename);
			}
			else
				curl_mime_filedata(field, g->trophy[i].true_image);
			field = curl_mime_addpart(form);
			curl_mime_name(field, "falseimagefile");
			if (mode == MODE_SERVERONLINE)
			{
				char *filename = GamerzillaGetTrophyImage_internal(g->short_name, g->trophy[i].name, false);
				curl_mime_filedata(field, filename);
				free(filename);
			}
			else
				curl_mime_filedata(field, g->trophy[i].false_image);
			field = curl_mime_addpart(form);
			curl_mime_name(field, "game");
			curl_mime_data(field, g->short_name, CURL_ZERO_TERMINATED);
			field = curl_mime_addpart(form);
			curl_mime_name(field, "trophy");
			curl_mime_data(field, g->trophy[i].name, CURL_ZERO_TERMINATED);
			curl_easy_setopt(c, CURLOPT_MIMEPOST, form);
			curl_easy_setopt(c, CURLOPT_USERPWD, userpwd);
			curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curlWriteData);
			curl_easy_setopt(c, CURLOPT_WRITEDATA, &internal_struct);
			CURLcode res = curl_easy_perform(c);
			if (res != CURLE_OK)
				fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
			curl_mime_free(form);
		}
		free(url);
		free(userpwd);
		content_destroy(&internal_struct);
	}
	else if (mode == MODE_CONNECTED)
	{
		uint8_t cmd = CMD_SETGAMEIMAGE;
		uint32_t hostsz = strlen(g->short_name);
		uint32_t sz = htonl(hostsz);
		size_t st_size[2];
		st_size[0] = (*game_size)(g->image);
		char data[1024];
		if (0 != st_size[0])
		{
			writesocket(server_socket, &cmd, sizeof(cmd));
			writesocket(server_socket, &sz, sizeof(sz));
			writesocket(server_socket, g->short_name, hostsz);
			hostsz = st_size[0];
			sz = htonl(hostsz);
			writesocket(server_socket, &sz, sizeof(sz));
			void *f = (*game_open)(g->image);
			while (hostsz > 0)
			{
				size_t ct = (*game_read)(f, data, 1024);
				if (ct > 0)
				{
					hostsz -= ct;
					writesocket(server_socket, data, ct);
				}
			}
			(*game_close)(f);
		}
		for (int i = 0; i < g->numTrophy; i++)
		{
			cmd = CMD_SETTROPHYIMAGE;
			hostsz = strlen(g->short_name);
			sz = htonl(hostsz);
			st_size[0] = (*game_size)(g->trophy[i].true_image);
			st_size[1] = (*game_size)(g->trophy[i].false_image);
			if ((0 != st_size[0]) && (0 != st_size[1]))
			{
				writesocket(server_socket, &cmd, sizeof(cmd));
				writesocket(server_socket, &sz, sizeof(sz));
				writesocket(server_socket, g->short_name, hostsz);
				hostsz = strlen(g->trophy[i].name);
				sz = htonl(hostsz);
				writesocket(server_socket, &sz, sizeof(sz));
				writesocket(server_socket, g->trophy[i].name, hostsz);
				hostsz = st_size[0];
				sz = htonl(hostsz);
				writesocket(server_socket, &sz, sizeof(sz));
				void *f = (*game_open)(g->trophy[i].true_image);
				while (hostsz > 0)
				{
					size_t ct = (*game_read)(f, data, 1024);
					if (ct > 0)
					{
						hostsz -= ct;
						writesocket(server_socket, data, ct);
					}
				}
				(*game_close)(f);
				hostsz = st_size[1];
				sz = htonl(hostsz);
				writesocket(server_socket, &sz, sizeof(sz));
				f = (*game_open)(g->trophy[i].false_image);
				while (hostsz > 0)
				{
					size_t ct = (*game_read)(f, data, 1024);
					if (ct > 0)
					{
						hostsz -= ct;
						writesocket(server_socket, data, ct);
					}
				}
				(*game_close)(f);
			}
		}
	}
}

static bool GamerzillaSync_internal(CURL *c, const char *short_name, Gamerzilla *g, TrophyData **t)
{
	GamerzillaLog(1, "Sync: ", short_name);
	// Read save file
	json_t *root = gamefile_read(short_name);
	if (root)
	{
		GamerzillaMerge(c, g, t, root);
		int version = g->version;
		json_decref(root);
		if (mode != MODE_OFFLINE)
		{
			// Get online data
			bool success = false;
			content internal_struct = GamerzillaGetGameInfo_internal(c, short_name, &success);
			if (!success)
				return false;
			else
			{
				json_error_t error;
				int jsonVersion = 0;
				root = json_loadb(internal_struct.data, internal_struct.len, 0, &error);
				bool update = GamerzillaMerge(c, g, t, root);
				json_t *ver = json_object_get(root, "version");
				if (json_is_string(ver))
				{
					jsonVersion = atol(json_string_value(ver));
				}
				json_decref(root);
				if (update)
				{
					gamefile_write(g, *t);
					if (version != g->version)
					{
						GamerzillaUpdateImages(c, short_name, g);
					}
				}
				else
				{
					if (jsonVersion < g->version)
					{
						GamerzillaSetGameInfo_internal(c, g);
						for (int k = 0; k < g->numTrophy; k++)
						{
							if ((*t)[k].achieved)
								GamerzillaSetTrophy_internal(c, g->short_name, g->trophy[k].name);
							else if ((*t)[k].progress > 0)
								GamerzillaSetTrophyStat_internal(c, g->short_name, g->trophy[k].name, (*t)[k].progress);
						}
					}
				}
			}
		}
		return true;
	}
	return false;
}

bool GamerzillaConnect(const char *baseurl, const char *username, const char *password)
{
	burl = strdup(baseurl);
	uname = strdup(username);
	pswd = strdup(password);
	curl[0] = curl_easy_init();
	if (mode == MODE_OFFLINE)
	{
		// TODO: Try connecting
		mode = MODE_STANDALONE;
	}
	else if (mode == MODE_SERVEROFFLINE)
	{
		mode = MODE_SERVERONLINE;
		curl[1] = curl_easy_init();
		DIR *d = NULL;
		struct dirent *dir;
		d = opendir(save_dir);
		if (d)
		{
			while ((dir = readdir(d)) != NULL)
			{
				int l = strlen(dir->d_name);
				if ((l > 5) && (strcmp(dir->d_name + l - 5, ".game") == 0))
				{
					char *dname = strdup(dir->d_name);
					dname[l - 5] = 0;
					GamerzillaGetGame(dname);
					Gamerzilla g;
					TrophyData *t = NULL;
					gamerzillaClear(&g, false);
					bool success = GamerzillaSync_internal(curl[1], dname, &g, &t);
					free(dname);
					if (success == false)
					{
						mode = MODE_SERVEROFFLINE;
						curl_easy_cleanup(curl[0]);
						curl[0] = NULL;
						curl_easy_cleanup(curl[1]);
						curl[1] = NULL;
						return false;
					}
				}
			}
		}
		content internal_struct;
		char *url, *userpwd;
		content_init(&internal_struct);
		url = malloc(strlen(burl) + 30);
		strcpy(url, burl);
		strcat(url, "api/gamerzilla/games");
		curl_easy_setopt(curl[1], CURLOPT_URL, url);
		curl_easy_setopt(curl[1], CURLOPT_POST, 1);
		userpwd = malloc(strlen(uname) + strlen(pswd) + 2);
		strcpy(userpwd, uname);
		strcat(userpwd, ":");
		strcat(userpwd, pswd);
		curl_easy_setopt(curl[1], CURLOPT_USERPWD, userpwd);
		curl_easy_setopt(curl[1], CURLOPT_WRITEFUNCTION, curlWriteData);
		curl_easy_setopt(curl[1], CURLOPT_WRITEDATA, &internal_struct);
		CURLcode res = curl_easy_perform(curl[1]);
		if (res != CURLE_OK)
		{
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
			mode = MODE_SERVEROFFLINE;
			curl_easy_cleanup(curl[0]);
			curl[0] = NULL;
			curl_easy_cleanup(curl[1]);
			curl[1] = NULL;
		}
		else
		{
			json_error_t error;
			json_t *root = json_loadb(internal_struct.data, internal_struct.len, 0, &error);
			if (json_is_array(root))
			{
				int num = json_array_size(root);
				for (int i = 0; i < num; i++)
				{
					json_t *item = json_array_get(root, i);
					json_t *tmp = json_object_get(item, "shortname");
					const char *game_name = json_string_value(tmp);
					bool found = false;
					for (int k = 0; k < game_num; k++)
					{
						if (strcmp(game_name, game_list[k]) == 0)
						{
							found = true;
							break;
						}
					}
					if (!found)
					{
						GamerzillaLog(1, "Online Data: ", game_name);
						GamerzillaGetGame(game_name);
						// Get online data
						content internal_struct = GamerzillaGetGameInfo_internal(curl[1], game_name, NULL);
						json_error_t error;
						json_t *game_info = json_loadb(internal_struct.data, internal_struct.len, 0, &error);
						Gamerzilla g;
						TrophyData *t = NULL;
						gamerzillaClear(&g, false);
						bool update = GamerzillaMerge(curl[1], &g, &t, game_info);
						json_decref(game_info);
						gamefile_write(&g, t);
						GamerzillaUpdateImages(curl[1], game_name, &g);
					}
				}
			}
			json_decref(root);
		}
		free(url);
		free(userpwd);
		content_destroy(&internal_struct);
	}
	return true;
}

void GamerzillaInitGame(Gamerzilla *g)
{
	gamerzillaClear(g, false);
}

void GamerzillaClearGame(Gamerzilla *g)
{
	gamerzillaClear(g, true);
}

int GamerzillaSetGame_common()
{
	// Read save file
	json_t *root = gamefile_read(current.short_name);
	if (root)
	{
		GamerzillaMerge(curl[0], &current, &currentData, root);
		json_decref(root);
	}
	if (mode != MODE_OFFLINE)
	{
		// Get online data
		bool success = false;
		content internal_struct = GamerzillaGetGameInfo_internal(curl[0], current.short_name, &success);
		json_error_t error;
		root = json_loadb(internal_struct.data, internal_struct.len, 0, &error);
		bool needSend = false;
		if (root)
		{
			bool update = GamerzillaMerge(curl[0], &current, &currentData, root);
			json_t *ver = json_object_get(root, "version");
			if (json_is_string(ver))
			{
				int dbVersion = atol(json_string_value(ver));
				// Should compare everything but trust version for now.
				if (dbVersion < current.version)
					needSend = true;
			}
			else
			{
				needSend = true;
			}
			json_t *incplt = json_object_get(root, "incomplete");
			if ((incplt) && (json_is_string(incplt)) && (1 == atol(json_string_value(incplt))))
			{
				needSend = true;
			}
			json_decref(root);
			if (update)
			{
				gamefile_write(&current, currentData);
			}
		}
		else if (success)
			needSend = true;
		if (needSend)
		{
			GamerzillaSetGameInfo_internal(curl[0], &current);
			for (int k = 0; k < current.numTrophy; k++)
			{
				if (currentData[k].achieved)
					GamerzillaSetTrophy_internal(curl[0], current.short_name, current.trophy[k].name);
				else if (currentData[k].progress > 0)
					GamerzillaSetTrophyStat_internal(curl[0], current.short_name, current.trophy[k].name, currentData[k].progress);
			}
		}
		remote_game_id = 0;
	}
	return GAMEID_CURRENT;
}

bool GamerzillaValidateGame(Gamerzilla *g)
{
	if ((g->short_name == NULL) || (g->name == NULL) || (g->image == NULL))
		return false;
	for (int i = 0; i < current.numTrophy; i++)
	{
		if ((g->trophy[i].name == NULL) || (g->trophy[i].desc == NULL) ||
			(g->trophy[i].true_image == NULL) || (g->trophy[i].false_image == NULL))
			return false;
	}
	return true;
}

int GamerzillaSetGame(Gamerzilla *g)
{
	if (!GamerzillaValidateGame(g))
		return GAMEID_ERROR;
	for (int i = 0; i < current.numTrophy; i++)
	{
		if ((g->trophy[i].name == NULL) || (g->trophy[i].desc == NULL) ||
			(g->trophy[i].true_image == NULL) || (g->trophy[i].false_image == NULL))
			return GAMEID_ERROR;
	}
	current.short_name = strdup(g->short_name);
	current.name = strdup(g->name);
	current.image = strdup(g->image);
	current.version = g->version;
	current.numTrophy = g->numTrophy;
	current.trophy = (GamerzillaTrophy*)malloc(sizeof(GamerzillaTrophy) * current.numTrophy);
	for (int i = 0; i < current.numTrophy; i++)
	{
		current.trophy[i].name = strdup(g->trophy[i].name);
		current.trophy[i].desc = strdup(g->trophy[i].desc);
		current.trophy[i].max_progress = g->trophy[i].max_progress;
		current.trophy[i].true_image = strdup(g->trophy[i].true_image);
		current.trophy[i].false_image = strdup(g->trophy[i].false_image);
	}
	currentData = (TrophyData *)calloc(current.numTrophy, sizeof(TrophyData));
	return GamerzillaSetGame_common();
}

static char *strprefix(const char *prefix, char *orig)
{
	char *tmp = malloc(strlen(prefix) + strlen(orig) + 1);
	strcpy(tmp, prefix);
	strcat(tmp, orig);
	free(orig);
	return tmp;
}

int GamerzillaSetGameFromFile(const char *filename, const char *datadir)
{
	GamerzillaInitGame(&current);
	json_error_t error;
	json_t *root = NULL;
	void *fd = (*game_open)(filename);
	root = json_load_callback(&jsonRead, fd, 0, &error);
	(*game_close)(fd);
	if (root)
	{
		GamerzillaMerge(curl[0], &current, &currentData, root);
		current.image = strprefix(datadir, current.image);
		for (int i = 0; i < current.numTrophy; i++)
		{
			current.trophy[i].true_image = strprefix(datadir, current.trophy[i].true_image);
			current.trophy[i].false_image = strprefix(datadir, current.trophy[i].false_image);
		}
		json_decref(root);
	}
	if (!GamerzillaValidateGame(&current))
	{
		gamerzillaClear(&current, true);
		return GAMEID_ERROR;
	}
	return GamerzillaSetGame_common();
}

int GamerzillaGetGame(const char *short_name)
{
	for (int i = 0; i < game_num; i++)
	{
		if (strcmp(game_list[i], short_name) == 0)
			return i;
	}
	if (game_sz == 0)
	{
		game_sz = 100;
		game_list = malloc(sizeof(char *) * game_sz);
		game_info = malloc(sizeof(Gamerzilla *) * game_sz);
		game_data = malloc(sizeof(GamerzillaTrophy **) * game_sz);
	}
	else if (game_sz == game_num)
	{
		game_sz *= 2;
		char **new_game_list = malloc(sizeof(char *) * game_sz);
		Gamerzilla **new_game_info = malloc(sizeof(Gamerzilla *) * game_sz);
		TrophyData **new_game_data = malloc(sizeof(TrophyData *) * game_sz);
		for (int i = 0; i < game_num; i++)
		{
			new_game_list[i] = game_list[i];
			new_game_info[i] = game_info[i];
			new_game_data[i] = game_data[i];
		}
		free(game_list);
		game_list = new_game_list;
		free(game_info);
		game_info = new_game_info;
		free(game_data);
		game_data = new_game_data;
	}
	game_list[game_num] = strdup(short_name);
	game_info[game_num] = NULL;
	game_data[game_num] = NULL;
	game_num++;
	return game_num - 1;
}

void GamerzillaFreeGame(int game_id)
{
	if (game_id < game_num)
	{
		if (game_info[game_id])
		{
			gamerzillaClear(game_info[game_id], true);
			free(game_info[game_id]);
			game_info[game_id] = NULL;
			free(game_data[game_id]);
			game_data[game_id] = NULL;
		}
	}
}

void GamerzillaGameAddTrophy(Gamerzilla *g, const char *name, const char *desc, int max_progress, const char *true_image, const char *false_image)
{
	if (g->szTrophy == 0)
	{
		g->trophy = calloc(16, sizeof(GamerzillaTrophy));
		g->szTrophy = 16;
	}
	else if (g->numTrophy == g->szTrophy)
	{
		g->szTrophy *= 2;
		GamerzillaTrophy *newTrophy = calloc(g->szTrophy, sizeof(GamerzillaTrophy));
		for (int i = 0; i < g->numTrophy; i++)
		{
			newTrophy[i].name = g->trophy[i].name;
			newTrophy[i].desc = g->trophy[i].desc;
			newTrophy[i].max_progress = g->trophy[i].max_progress;
			newTrophy[i].true_image = g->trophy[i].true_image;
			newTrophy[i].false_image = g->trophy[i].false_image;
		}
		free(g->trophy);
		g->trophy = newTrophy;
	}
	g->trophy[g->numTrophy].name = strdup(name);
	g->trophy[g->numTrophy].desc = strdup(desc);
	g->trophy[g->numTrophy].max_progress = max_progress;
	g->trophy[g->numTrophy].true_image = strdup(true_image);
	g->trophy[g->numTrophy].false_image = strdup(false_image);
	g->numTrophy++;
}

bool GamerzillaCheckGameInfo(CURL *c, int game_id, Gamerzilla **info, TrophyData **data)
{
	if ((mode == MODE_SERVEROFFLINE) || (mode == MODE_SERVERONLINE))
	{
		if (game_id < game_num)
		{
			if (game_info[game_id] == NULL)
			{
				game_info[game_id] = malloc(sizeof(Gamerzilla));
				gamerzillaClear(game_info[game_id], false);
				// Read save file
				json_t *root = gamefile_read(game_list[game_id]);
				if (root)
				{
					GamerzillaMerge(c, game_info[game_id], &game_data[game_id], root);
				}
			}
			if (game_info[game_id] == NULL)
				return false;
			*info = game_info[game_id];
			*data = game_data[game_id];
			return true;
		}
	}
	else if (game_id == GAMEID_CURRENT)
	{
		*info = &current;
		*data = currentData;
		return true;
	}
	return false;
}

char *GamerzillaGetGameImage(int game_id)
{
	const char *game_name = NULL;
	if (game_id == GAMEID_CURRENT)
		game_name = current.short_name;
	else if (game_id < game_num)
		game_name = game_list[game_id];
	else
		return NULL;
	return GamerzillaGetGameImage_internal(game_name);
}

int GamerzillaGetTrophyNum(int game_id)
{
	Gamerzilla *info;
	TrophyData *data;
	if (GamerzillaCheckGameInfo((((mode == MODE_SERVERONLINE) || (mode == MODE_SERVEROFFLINE)) ? curl[1] : curl[0]), game_id, &info, &data))
	{
		return info->numTrophy;
	}
	return 0;
}

void GamerzillaGetTrophyByIndex(int game_id, int indx, char **name, char **desc)
{
	Gamerzilla *info;
	TrophyData *data;
	if (GamerzillaCheckGameInfo((((mode == MODE_SERVERONLINE) || (mode == MODE_SERVEROFFLINE)) ? curl[1] : curl[0]), game_id, &info, &data))
	{
		*name = info->trophy[indx].name;
		*desc = info->trophy[indx].desc;
	}
}

bool GamerzillaGetTrophy(int game_id, const char *name, bool *achieved)
{
	Gamerzilla *info;
	TrophyData *data;
	GamerzillaCheckGameInfo((((mode == MODE_SERVERONLINE) || (mode == MODE_SERVEROFFLINE)) ? curl[1] : curl[0]), game_id, &info, &data);
	for (int i = 0; i < info->numTrophy; i++)
	{
		if (0 == strcmp(name, info->trophy[i].name))
		{
			*achieved = data[i].achieved;
			return true;
		}
	}
	return false;
}

bool GamerzillaGetTrophyStat(int game_id, const char *name, int *progress)
{
	Gamerzilla *info;
	TrophyData *data;
	GamerzillaCheckGameInfo((((mode == MODE_SERVERONLINE) || (mode == MODE_SERVEROFFLINE)) ? curl[1] : curl[0]), game_id, &info, &data);
	for (int i = 0; i < info->numTrophy; i++)
	{
		if (0 == strcmp(name, info->trophy[i].name))
		{
			*progress = data->progress;
			return true;
		}
	}
	return false;
}

char *GamerzillaGetTrophyImage(int game_id, const char *name, bool achieved)
{
	const char *game_name = NULL;
	if (game_id == GAMEID_CURRENT)
		game_name = current.short_name;
	else if (game_id < game_num)
		game_name = game_list[game_id];
	else
		return NULL;
	return GamerzillaGetTrophyImage_internal(game_name, name, achieved);
}

bool GamerzillaSetTrophy(int game_id, const char *name)
{
	Gamerzilla *info;
	TrophyData *data;
	if (!GamerzillaCheckGameInfo(curl[0], game_id, &info, &data))
		return false;
	for (int i = 0; i < info->numTrophy; i++)
	{
		if (0 == strcmp(info->trophy[i].name, name))
		{
			data[i].achieved = true;
			gamefile_write(info, data);
			break;
		}
	}
	GamerzillaSetTrophy_internal(curl[0], info->short_name, name);
	return true;
}

bool GamerzillaSetTrophyStat(int game_id, const char *name, int progress)
{
	Gamerzilla *info;
	TrophyData *data;
	bool achieved = false;
	if (!GamerzillaCheckGameInfo(curl[0], game_id, &info, &data))
		return false;
	for (int i = 0; i < info->numTrophy; i++)
	{
		if (0 == strcmp(info->trophy[i].name, name))
		{
			data[i].progress = progress;
			gamefile_write(info, data);
			if (progress == info->trophy[i].max_progress)
				achieved = true;
			break;
		}
	}
	GamerzillaSetTrophyStat_internal(curl[0], info->short_name, name, progress);
	if (achieved)
		GamerzillaSetTrophy(game_id, name);
	return true;
}

static bool GamerzillaCheckImages(Gamerzilla *g)
{
	char *shortname = g->short_name;
	char *filename = (char *)malloc(strlen(save_dir) + strlen(shortname) * 2 + 20);
	createImagePath(shortname);
	strcpy(filename, save_dir);
	strcat(filename, "image/");
	strcat(filename, shortname);
	strcat(filename, "/");
	strcat(filename, shortname);
	strcat(filename, ".png");
	FILE *f = fopen(filename, "rb");
	free(filename);
	if (f)
	{
		fclose(f);
	}
	else
		return false;
	for (int i = 0; i < g->numTrophy; i++)
	{
		char *trophy_name = g->trophy[i].name;
		filename = (char *)malloc(strlen(save_dir) + strlen(shortname) * 2 + strlen(trophy_name) + 20);
		strcpy(filename, save_dir);
		strcat(filename, "image/");
		strcat(filename, shortname);
		strcat(filename, "/");
		int end = strlen(filename);
		for (int i = 0; trophy_name[i]; i++)
		{
			if (isalnum(trophy_name[i]))
			{
				filename[end] = trophy_name[i];
			}
			else
			{
				filename[end] = '_';
			}
			end++;
		}
		filename[end] = 0;
		strcat(filename, "1.png");
		f = fopen(filename, "rb");
		if (f)
		{
			fclose(f);
		}
		else
		{
			free(filename);
			return false;
		}
		filename[end] = '0';
		f = fopen(filename, "rb");
		free(filename);
		if (f)
		{
			fclose(f);
		}
		else
		{
			return false;
		}
	}
	return true;
}

static bool GamerzillaServerProcessClient(SOCKET fd)
{
	uint8_t cmd;
	ssize_t res = readsocket(fd, &cmd, sizeof(cmd));
	if (res != 1)
	{
		return false;
	}
	GamerzillaLog(1, "Command: ", ((cmd <= MAX_CMD) ? cmdName[cmd] : "Unknown Command"));
	switch (cmd)
	{
		case CMD_QUIT:
			return false;
		case CMD_GETGAMEINFO:
		{
			uint32_t sz;
			uint32_t hostsz;
			content client_content;
			ssize_t ct = 0;
			readsocket(fd, &sz, sizeof(sz));
			hostsz = ntohl(sz);
			content_init(&client_content);
			content_read(fd, &client_content, hostsz);
			char *name = malloc(client_content.len + 1);
			memcpy(name, client_content.data, client_content.len);
			name[client_content.len] = 0;
			Gamerzilla g;
			TrophyData *t = NULL;
			gamerzillaClear(&g, false);
			// Read save file
			json_t *root = gamefile_read(name);
			bool complete = true;
			if (root)
			{
				GamerzillaMerge(curl[0], &g, &t, root);
				json_decref(root);
				complete = GamerzillaCheckImages(&g);
			}
			if (mode != MODE_SERVEROFFLINE)
			{
				// Get online data
				content internal_struct = GamerzillaGetGameInfo_internal(curl[0], name, NULL);
				json_error_t error;
				root = json_loadb(internal_struct.data, internal_struct.len, 0, &error);
				if (root)
				{
					bool update = GamerzillaMerge(curl[0], &g, &t, root);
					json_decref(root);
					if (update)
					{
						gamefile_write(&g, t);
						GamerzillaUpdateImages(curl[0], name, &g);
					}
				}
				content_destroy(&internal_struct);
			}
			if ((g.short_name != NULL) && (accessCallback != NULL))
			{
				(*accessCallback)(g.short_name, g.name, accessData);
			}
			root = GamerzillaJson(&g, t, !complete);
			char *response = json_dumps(root, 0);
			hostsz = strlen(response);
			sz = htonl(hostsz);
			writesocket(fd, &sz, sizeof(sz));
			writesocket(fd, response, hostsz);
			json_decref(root);
			content_destroy(&client_content);
			break;
		}
		case CMD_SETGAMEINFO:
		{
			uint32_t sz;
			uint32_t hostsz;
			content client_content;
			ssize_t ct = 0;
			readsocket(fd, &sz, sizeof(sz));
			hostsz = ntohl(sz);
			content_init(&client_content);
			content_read(fd, &client_content, hostsz);
			Gamerzilla g;
			TrophyData *t = NULL;
			gamerzillaClear(&g, false);
			json_error_t error;
			json_t *root = json_loadb(client_content.data, client_content.len, 0, &error);
			if (root)
			{
				GamerzillaMerge(curl[0], &g, &t, root);
				json_decref(root);
				gamefile_write(&g, t);
				if (accessCallback != NULL)
				{
					(*accessCallback)(g.short_name, g.name, accessData);
				}
			}
			if (mode == MODE_SERVERONLINE)
			{
				content internal_struct;
				content_init(&internal_struct);
				char *esc_content = curl_easy_escape(curl[0], client_content.data, client_content.len);
				char *postdata = malloc(strlen(esc_content) + 30);
				strcpy(postdata, "game=");
				strcat(postdata, esc_content);
				curl_free(esc_content);
				char *url, *userpwd;
				url = malloc(strlen(burl) + 30);
				strcpy(url, burl);
				strcat(url, "api/gamerzilla/game/add");
				curl_easy_setopt(curl[0], CURLOPT_URL, url);
				curl_easy_setopt(curl[0], CURLOPT_POSTFIELDS, postdata);
				curl_easy_setopt(curl[0], CURLOPT_POSTFIELDSIZE, strlen(postdata));
				userpwd = malloc(strlen(uname) + strlen(pswd) + 2);
				strcpy(userpwd, uname);
				strcat(userpwd, ":");
				strcat(userpwd, pswd);
				curl_easy_setopt(curl[0], CURLOPT_USERPWD, userpwd);
				curl_easy_setopt(curl[0], CURLOPT_WRITEFUNCTION, curlWriteData);
				curl_easy_setopt(curl[0], CURLOPT_WRITEDATA, &internal_struct);
				CURLcode res = curl_easy_perform(curl[0]);
				if (res != CURLE_OK)
					fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
				free(url);
				free(userpwd);
				free(postdata);
				content_destroy(&internal_struct);
			}
			content_destroy(&client_content);
			break;
		}
		case CMD_SETTROPHY:
		{
			uint32_t sz;
			uint32_t hostsz;
			content client_content;
			ssize_t ct = 0;
			readsocket(fd, &sz, sizeof(sz));
			hostsz = ntohl(sz);
			content_init(&client_content);
			content_read(fd, &client_content, hostsz);
			char *game_name = malloc(client_content.len + 1);
			memcpy(game_name, client_content.data, client_content.len);
			game_name[client_content.len] = 0;
			client_content.len = 0;
			readsocket(fd, &sz, sizeof(sz));
			hostsz = ntohl(sz);
			content_read(fd, &client_content, hostsz);
			char *name = malloc(client_content.len + 1);
			memcpy(name, client_content.data, client_content.len);
			name[client_content.len] = 0;
			Gamerzilla g;
			TrophyData *t = NULL;
			gamerzillaClear(&g, false);
			// Read save file
			json_t *root = gamefile_read(game_name);
			if (root)
			{
				GamerzillaMerge(curl[0], &g, &t, root);
				json_decref(root);
				for (int i = 0; i < g.numTrophy; i++)
				{
					if (0 == strcmp(g.trophy[i].name, name))
					{
						t[i].achieved = true;
						gamefile_write(&g, t);
						break;
					}
				}
			}
			if (mode == MODE_SERVERONLINE)
			{
				GamerzillaSetTrophy_internal(curl[0], game_name, name);
			}
			free(game_name);
			free(name);
			content_destroy(&client_content);
			break;
		}
		case CMD_SETGAMEIMAGE:
		{
			uint32_t sz;
			uint32_t hostsz;
			content client_content;
			char *shortname;
			readsocket(fd, &sz, sizeof(sz));
			hostsz = ntohl(sz);
			shortname = (char *)malloc(hostsz + 1);
			readsocket(fd, shortname, hostsz);
			shortname[hostsz] = 0;
			content_init(&client_content);
			readsocket(fd, &sz, sizeof(sz));
			hostsz = ntohl(sz);
			content_read(fd, &client_content, hostsz);
			char *filename = (char *)malloc(strlen(save_dir) + strlen(shortname) * 2 + 20);
			createImagePath(shortname);
			strcpy(filename, save_dir);
			strcat(filename, "image/");
			strcat(filename, shortname);
			strcat(filename, "/");
			strcat(filename, shortname);
			strcat(filename, ".png");
			FILE *f = fopen(filename, "wb");
			if (f)
			{
				fwrite(client_content.data, 1, client_content.len, f);
				fclose(f);
			}
			free (filename);
			if (mode == MODE_SERVERONLINE)
			{
				content internal_struct;
				char *url, *userpwd;
				curl_mime *form = NULL;
				curl_mimepart *field = NULL;
				content_init(&internal_struct);
				url = malloc(strlen(burl) + 30);
				strcpy(url, burl);
				strcat(url, "api/gamerzilla/game/image");
				curl_easy_setopt(curl[0], CURLOPT_URL, url);
				form = curl_mime_init(curl[0]);
				field = curl_mime_addpart(form);
				curl_mime_data(field, client_content.data, client_content.len);
				curl_mime_filename(field, "image.png");
				curl_mime_name(field, "imagefile");
				field = curl_mime_addpart(form);
				curl_mime_name(field, "game");
				curl_mime_data(field, shortname, CURL_ZERO_TERMINATED);
				curl_easy_setopt(curl[0], CURLOPT_MIMEPOST, form);
				userpwd = malloc(strlen(uname) + strlen(pswd) + 2);
				strcpy(userpwd, uname);
				strcat(userpwd, ":");
				strcat(userpwd, pswd);
				curl_easy_setopt(curl[0], CURLOPT_USERPWD, userpwd);
				curl_easy_setopt(curl[0], CURLOPT_WRITEFUNCTION, curlWriteData);
				curl_easy_setopt(curl[0], CURLOPT_WRITEDATA, &internal_struct);
				CURLcode res = curl_easy_perform(curl[0]);
				if (res != CURLE_OK)
					fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
				curl_mime_free(form);
				free(url);
				free(userpwd);
				content_destroy(&internal_struct);
			}
			free(shortname);
			content_destroy(&client_content);
			break;
		}
		case CMD_SETTROPHYSTAT:
		{
			int progress;
			uint32_t sz;
			uint32_t hostsz;
			content client_content;
			content internal_struct;
			ssize_t ct = 0;
			readsocket(fd, &sz, sizeof(sz));
			hostsz = ntohl(sz);
			content_init(&client_content);
			content_init(&internal_struct);
			content_read(fd, &client_content, hostsz);
			char *game_name = malloc(client_content.len + 1);
			memcpy(game_name, client_content.data, client_content.len);
			game_name[client_content.len] = 0;
			client_content.len = 0;
			readsocket(fd, &sz, sizeof(sz));
			hostsz = ntohl(sz);
			content_read(fd, &client_content, hostsz);
			char *name = malloc(client_content.len + 1);
			memcpy(name, client_content.data, client_content.len);
			name[client_content.len] = 0;
			readsocket(fd, &sz, sizeof(sz));
			progress = ntohl(sz);
			Gamerzilla g;
			TrophyData *t = NULL;
			gamerzillaClear(&g, false);
			// Read save file
			json_t *root = gamefile_read(game_name);
			if (root)
			{
				GamerzillaMerge(curl[0], &g, &t, root);
				json_decref(root);
				for (int i = 0; i < g.numTrophy; i++)
				{
					if (0 == strcmp(g.trophy[i].name, name))
					{
						t[i].progress = progress;
						gamefile_write(&g, t);
						break;
					}
				}
			}
			if (mode == MODE_SERVERONLINE)
			{
				GamerzillaSetTrophyStat_internal(curl[0], game_name, name, progress);
			}
			free(game_name);
			free(name);
			content_destroy(&client_content);
			break;
		}
		case CMD_SETTROPHYIMAGE:
		{
			uint32_t sz;
			uint32_t hostsz;
			content true_content;
			content false_content;
			char *shortname;
			char *trophy_name;
			readsocket(fd, &sz, sizeof(sz));
			hostsz = ntohl(sz);
			shortname = (char *)malloc(hostsz + 1);
			readsocket(fd, shortname, hostsz);
			shortname[hostsz] = 0;
			readsocket(fd, &sz, sizeof(sz));
			hostsz = ntohl(sz);
			trophy_name = (char *)malloc(hostsz + 1);
			readsocket(fd, trophy_name, hostsz);
			trophy_name[hostsz] = 0;
			content_init(&true_content);
			content_init(&false_content);
			readsocket(fd, &sz, sizeof(sz));
			hostsz = ntohl(sz);
			content_read(fd, &true_content, hostsz);
			readsocket(fd, &sz, sizeof(sz));
			hostsz = ntohl(sz);
			content_read(fd, &false_content, hostsz);
			createImagePath(shortname);
			char *filename = (char *)malloc(strlen(save_dir) + strlen(shortname) * 2 + strlen(trophy_name) + 20);
			strcpy(filename, save_dir);
			strcat(filename, "image/");
			strcat(filename, shortname);
			strcat(filename, "/");
			int end = strlen(filename);
			for (int i = 0; trophy_name[i]; i++)
			{
				if (isalnum(trophy_name[i]))
				{
					filename[end] = trophy_name[i];
				}
				else
				{
					filename[end] = '_';
				}
				end++;
			}
			filename[end] = 0;
			strcat(filename, "1.png");
			FILE *f = fopen(filename, "wb");
			if (f)
			{
				fwrite(true_content.data, 1, true_content.len, f);
				fclose(f);
			}
			filename[end] = '0';
			f = fopen(filename, "wb");
			if (f)
			{
				fwrite(false_content.data, 1, false_content.len, f);
				fclose(f);
			}
			free(filename);
			if (mode == MODE_SERVERONLINE)
			{
				content internal_struct;
				char *url, *userpwd;
				curl_mime *form = NULL;
				curl_mimepart *field = NULL;
				content_init(&internal_struct);
				url = malloc(strlen(burl) + 30);
				strcpy(url, burl);
				strcat(url, "api/gamerzilla/trophy/image");
				curl_easy_setopt(curl[0], CURLOPT_URL, url);
				form = curl_mime_init(curl[0]);
				field = curl_mime_addpart(form);
				curl_mime_data(field, true_content.data, true_content.len);
				curl_mime_filename(field, "trueimage.png");
				curl_mime_name(field, "trueimagefile");
				field = curl_mime_addpart(form);
				curl_mime_data(field, false_content.data, false_content.len);
				curl_mime_filename(field, "falseimage.png");
				curl_mime_name(field, "falseimagefile");
				field = curl_mime_addpart(form);
				curl_mime_name(field, "game");
				curl_mime_data(field, shortname, CURL_ZERO_TERMINATED);
				field = curl_mime_addpart(form);
				curl_mime_name(field, "trophy");
				curl_mime_data(field, trophy_name, CURL_ZERO_TERMINATED);
				curl_easy_setopt(curl[0], CURLOPT_MIMEPOST, form);
				userpwd = malloc(strlen(uname) + strlen(pswd) + 2);
				strcpy(userpwd, uname);
				strcat(userpwd, ":");
				strcat(userpwd, pswd);
				curl_easy_setopt(curl[0], CURLOPT_USERPWD, userpwd);
				curl_easy_setopt(curl[0], CURLOPT_WRITEFUNCTION, curlWriteData);
				curl_easy_setopt(curl[0], CURLOPT_WRITEDATA, &internal_struct);
				CURLcode res = curl_easy_perform(curl[0]);
				if (res != CURLE_OK)
					fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
				curl_mime_free(form);
				free(url);
				free(userpwd);
				content_destroy(&internal_struct);
			}
			free(shortname);
			free(trophy_name);
			content_destroy(&true_content);
			content_destroy(&false_content);
			break;
		}
		default:
			break;
	}
	return true;
}

void GamerzillaServerProcess(struct timeval *timeout)
{
	fd_set readfds;
	int nfds = server_socket + 1;
	FD_ZERO(&readfds);
	FD_SET(server_socket, &readfds);
	for (int i = 0; i < num_client; i++)
	{
		FD_SET(client_socket[i], &readfds);
		if (nfds <= client_socket[i])
		{
			nfds = client_socket[i] + 1;
		}
	}
	int rc = select(nfds, &readfds, NULL, NULL, timeout);
	if (rc == -1)
	{
	}
	else if (rc > 0)
	{
		if (FD_ISSET(server_socket, &readfds))
		{
			int newfd = accept(server_socket, NULL, NULL);
			if (newfd != -1)
			{
				if (num_client >= size_client)
				{
					if (size_client == 0)
					{
						size_client = 100;
						client_socket = (SOCKET *)malloc(sizeof(SOCKET) * size_client);
					}
					else
					{
						SOCKET *old_client = client_socket;
						size_client *= 2;
						client_socket = (SOCKET *)malloc(sizeof(int) * size_client);
						for (int i = 0; i < num_client; i++)
							client_socket[i] = old_client[i];
						free(old_client);
					}
				}
				client_socket[num_client] = newfd;
				num_client++;
			}
		}
		for (int i = 0; i < num_client; i++)
		{
			if (FD_ISSET(client_socket[i], &readfds))
			{
				bool good = GamerzillaServerProcessClient(client_socket[i]);
				if (!good)
				{
					closesocket(client_socket[i]);
					num_client = num_client - 1;
					while (i < num_client)
					{
						client_socket[i] = client_socket[i + 1];
					}
				}
			}
		}
	}
}

void GamerzillaServerListen(GamerzillaAccessGame callback, void *user_data)
{
	accessCallback = callback;
	accessData = user_data;
}

void GamerzillaQuit()
{
	if (current.short_name)
		gamerzillaClear(&current, true);
	if (currentData)
		free(currentData);
	if (save_dir)
		free(save_dir);
	if (burl)
		free(burl);
	if (uname)
		free(uname);
	if (pswd)
		free(pswd);
	if (server_socket != INVALID_SOCKET)
	{
		closesocket(server_socket);
		server_socket = INVALID_SOCKET;
#ifdef _WIN32
		WSACleanup();
#endif
	}
	if (curl[0])
	{
		curl_easy_cleanup(curl[0]);
		curl[0] = NULL;
	}
	if (curl[1])
	{
		curl_easy_cleanup(curl[1]);
		curl[1] = NULL;
	}
	if (client_socket)
	{
		for (int i = 0; i < num_client; i++)
		{
			closesocket(client_socket[i]);
		}
		free(client_socket);
	}
}

void GamerzillaSetRead(GamerzillaSize gameSize, GamerzillaOpen gameOpen, GamerzillaRead gameRead, GamerzillaClose gameClose)
{
	game_size = gameSize;
	game_open = gameOpen;
	game_read = gameRead;
	game_close = gameClose;
}

void GamerzillaSetLog(int level, FILE *f)
{
	logLevel = level;
	logFile = f;
}
