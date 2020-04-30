#include <gamerzilla.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <jansson.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/socket.h>

#define MODE_OFFLINE 0
#define MODE_SERVEROFFLINE 1
#define MODE_CONNECTED 2
#define MODE_STANDALONE 3
#define MODE_SERVERONLINE 4

#define CMD_GETGAMEINFO 0
#define CMD_SETGAMEINFO 1
#define CMD_SETTROPHY 2
#define CMD_SETGAMEIMAGE 3

typedef struct {
	bool achieved;
	int progress;
} TrophyData;

static int mode = MODE_OFFLINE;
static char *burl = NULL;
static char *uname = NULL;
static char *pswd = NULL;
static int game_num = 0;
static char **game_list = NULL;
static Gamerzilla current;
static TrophyData *currentData = NULL;
static int remote_game_id = -1;
static int server_socket;
static int *client_socket = NULL;
static int num_client = 0;
static int size_client = 0;
static CURL *curl = NULL;
static char *save_dir = NULL;

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
		ct += read(fd, x->data + ct, sz - ct);
	}
	x->len = sz;
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

static void gamerzillaClear(Gamerzilla *g, bool memFree)
{
	if (memFree)
	{
		free(g->short_name);
		free(g->name);
		free(g->image);
		for (int i = 0; i < g->numTrophy; i++)
		{
			free(g->trophy[i].name);
			g->trophy[i].name = NULL;
			free(g->trophy[i].desc);
			g->trophy[i].desc = NULL;
		}
		free(g->trophy);
	}
	g->short_name = NULL;
	g->name = NULL;
	g->image = NULL;
	g->version = 0;
	g->numTrophy = 0;
	g->trophy = NULL;
}

bool GamerzillaInit(bool bServer, const char *savedir)
{
	current.short_name = NULL;
	save_dir = strdup(savedir);
	if (bServer)
	{
		// Initialize socket
		mode = MODE_SERVEROFFLINE;
		struct sockaddr_un addr;
		memset(&addr, 0, sizeof(struct sockaddr_un));
		addr.sun_family = AF_UNIX;
		strncpy(&addr.sun_path[1], "Gamerzilla", 10);
		server_socket = socket(AF_UNIX, SOCK_STREAM, 0);
		if (server_socket == -1)
			return false;
		if (bind(server_socket, (struct sockaddr *) &addr, sizeof(sa_family_t) + 10 + 1) == -1)
			return false;
		if (listen(server_socket, 5) == -1)
			return false;
		return true;
	}
	else
	{
		// Try connect to server
		struct sockaddr_un addr;
		memset(&addr, 0, sizeof(struct sockaddr_un));
		addr.sun_family = AF_UNIX;
		strncpy(&addr.sun_path[1], "Gamerzilla", 10);
		server_socket = socket(AF_UNIX, SOCK_STREAM, 0);
		if (server_socket == -1)
			return false;
		if (connect(server_socket, (struct sockaddr*)&addr, sizeof(sa_family_t) + 10 + 1) != -1)
		{
			mode = MODE_CONNECTED;
			// TODO: Send user ID and confirm on server
			return true;
		}
	}
	return false;
}

bool GamerzillaConnect(const char *baseurl, const char *username, const char *password)
{
	burl = strdup(baseurl);
	uname = strdup(username);
	pswd = strdup(password);
	curl = curl_easy_init();
	if (mode == MODE_OFFLINE)
		mode = MODE_STANDALONE;
	else if (mode == MODE_SERVEROFFLINE)
		mode = MODE_SERVERONLINE;
	// TODO: Try connecting
	return true;
}

static content GamerzillaGetGameInfo_internal(const char *name)
{
	content internal_struct;
	content_init(&internal_struct);
	if ((mode == MODE_STANDALONE) || (mode == MODE_SERVERONLINE))
	{
		// Get game info
		char *postdata = malloc(strlen(name) + 20);
		strcpy(postdata, "game=");
		strcat(postdata, name);
		char *url, *userpwd;
		url = malloc(strlen(burl) + 20);
		strcpy(url, burl);
		strcat(url, "api/gamerzilla/game");
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postdata);
		userpwd = malloc(strlen(uname) + strlen(pswd) + 2);
		strcpy(userpwd, uname);
		strcat(userpwd, ":");
		strcat(userpwd, pswd);
		curl_easy_setopt(curl, CURLOPT_USERPWD, userpwd);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteData);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &internal_struct);
		CURLcode res = curl_easy_perform(curl);
		if (res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		free(url);
		free(userpwd);
		free(postdata);
	}
	else if (mode == MODE_CONNECTED)
	{
		uint8_t cmd = CMD_GETGAMEINFO;
		uint32_t hostsz = strlen(name);
		uint32_t sz = htonl(hostsz);
		write(server_socket, &cmd, sizeof(cmd));
		write(server_socket, &sz, sizeof(sz));
		write(server_socket, name, hostsz);
		ssize_t ct = 0;
		while (ct != sizeof(sz))
		{
			ct += read(server_socket, ((char *)&sz) + ct, sizeof(sz) - ct);
		}
		hostsz = ntohl(sz);
		content_read(server_socket, &internal_struct, hostsz);
	}
	return internal_struct;
}

static void GamerzillaSetTrophy_internal(const char *game_name, const char *name)
{
	if (mode == MODE_STANDALONE)
	{
		char *postdata = malloc(strlen(game_name) + strlen(name) + 30);
		strcpy(postdata, "game=");
		strcat(postdata, game_name);
		strcat(postdata, "&trophy=");
		strcat(postdata, name);
		content internal_struct;
		char *url, *userpwd;
		content_init(&internal_struct);
		url = malloc(strlen(burl) + 20);
		strcpy(url, burl);
		strcat(url, "api/gamerzilla/trophy/set");
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postdata);
		userpwd = malloc(strlen(uname) + strlen(pswd) + 2);
		strcpy(userpwd, uname);
		strcat(userpwd, ":");
		strcat(userpwd, pswd);
		curl_easy_setopt(curl, CURLOPT_USERPWD, userpwd);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteData);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &internal_struct);
		CURLcode res = curl_easy_perform(curl);
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
		write(server_socket, &cmd, sizeof(cmd));
		write(server_socket, &sz, sizeof(sz));
		write(server_socket, game_name, hostsz);
		hostsz = strlen(name);
		sz = htonl(hostsz);
		write(server_socket, &sz, sizeof(sz));
		write(server_socket, name, hostsz);
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

static json_t *GamerzillaJson(Gamerzilla *g, TrophyData *t)
{
	char tmp[512];
	json_t *root = json_object();
	json_object_set_new(root, "shortname", json_string(g->short_name));
	json_object_set_new(root, "name", json_string(g->name));
	json_object_set_new(root, "image", json_string(g->image));
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
	json_t *root = GamerzillaJson(g, t);
	json_dump_file(root, filename, 0);
	json_decref(root);
}

static bool GamerzillaMerge(Gamerzilla *g, TrophyData **t, json_t *root)
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
		if (json_is_string(ver))
		{
			g->short_name = strdup(json_string_value(tmp));
		}
		tmp = json_object_get(root, "name");
		if (json_is_string(ver))
		{
			g->name = strdup(json_string_value(tmp));
		}
		tmp = json_object_get(root, "image");
		if (json_is_string(ver))
		{
			g->image = strdup(json_string_value(tmp));
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
					newTrophy[i].max_progress = atol(json_string_value(ver));
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
					int tmpVal = atol(json_string_value(tmp));
					if (((*t)[k].achieved == false) && (tmpVal))
					{
						(*t)[k].achieved = tmpVal;
						update = true;
					}
					else if ((tmpVal == false) && ((*t)[k].achieved))
						GamerzillaSetTrophy_internal(g->short_name, g->trophy[k].name);
					tmp = json_object_get(item, "progress");
					if (json_is_null(tmp))
						tmpVal = 0;
					else
						tmpVal = atol(json_string_value(tmp));
					if (tmpVal > (*t)[k].progress)
					{
						(*t)[k].progress = tmpVal;
						update = true;
					}
				}
			}
		}
	}
	return update;
}

int GamerzillaGameInit(Gamerzilla *g)
{
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
	}
	currentData = (TrophyData *)calloc(current.numTrophy, sizeof(TrophyData));
	// Read save file
	json_t *root = gamefile_read(current.short_name);
	if (root)
	{
		GamerzillaMerge(&current, &currentData, root);
		json_decref(root);
	}
	if (mode != MODE_OFFLINE)
	{
		// Get online data
		content internal_struct = GamerzillaGetGameInfo_internal(current.short_name);
		json_error_t error;
		root = json_loadb(internal_struct.data, internal_struct.len, 0, &error);
		bool update = GamerzillaMerge(&current, &currentData, root);
		json_decref(root);
		if (update)
		{
			gamefile_write(&current, currentData);
		}
	}
	return 9999;
}

bool GamerzillaGetTrophy(int game_id, const char *name, bool *acheived)
{
	return false;
}

bool GamerzillaGetTrophyStat(int game_id, const char *name, int *progress)
{
	return false;
}

static void GamerzillaSetGameInfo_internal(int game_id)
{
	char *postdata;
	char tmp[512];
	json_t *root = json_object();
	json_object_set_new(root, "shortname", json_string(current.short_name));
	json_object_set_new(root, "name", json_string(current.name));
	sprintf(tmp, "%d", current.version);
	json_object_set_new(root, "version", json_string(tmp));
	json_t *trophy = json_array();
	json_object_set_new(root, "trophy", trophy);
	for (int i = 0; i < current.numTrophy; i++)
	{
		json_t *curTrophy = json_object();
		json_array_append_new(trophy, curTrophy);
		json_object_set_new(curTrophy, "trophy_name", json_string(current.trophy[i].name));
		json_object_set_new(curTrophy, "trophy_desc", json_string(current.trophy[i].desc));
		sprintf(tmp, "%d", current.trophy[i].max_progress);
		json_object_set_new(curTrophy, "max_progress", json_string(tmp));
	}
	char *response = json_dumps(root, 0);
	json_decref(root);
	if (mode == MODE_STANDALONE)
	{
		postdata = malloc(strlen(response) + 20);
		strcpy(postdata, "game=");
		// TODO: Post encode the response
		strcat(postdata, response);
		content internal_struct;
		char *url, *userpwd;
		content_init(&internal_struct);
		url = malloc(strlen(burl) + 30);
		strcpy(url, burl);
		strcat(url, "api/gamerzilla/game/add");
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postdata);
		userpwd = malloc(strlen(uname) + strlen(pswd) + 2);
		strcpy(userpwd, uname);
		strcat(userpwd, ":");
		strcat(userpwd, pswd);
		curl_easy_setopt(curl, CURLOPT_USERPWD, userpwd);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteData);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &internal_struct);
		CURLcode res = curl_easy_perform(curl);
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
		write(server_socket, &cmd, sizeof(cmd));
		write(server_socket, &sz, sizeof(sz));
		write(server_socket, response, hostsz);
	}
	free(response);
	if (mode == MODE_STANDALONE)
	{
		content internal_struct;
		char *url, *userpwd;
		curl_mime *form = NULL;
		curl_mimepart *field = NULL;
		content_init(&internal_struct);
		url = malloc(strlen(burl) + 30);
		strcpy(url, burl);
		strcat(url, "api/gamerzilla/game/image");
		curl_easy_setopt(curl, CURLOPT_URL, url);
		form = curl_mime_init(curl);
		field = curl_mime_addpart(form);
		curl_mime_name(field, "imagefile");
		curl_mime_filedata(field, current.image);
		field = curl_mime_addpart(form);
		curl_mime_name(field, "game");
		curl_mime_data(field, current.short_name, CURL_ZERO_TERMINATED);
		curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
		userpwd = malloc(strlen(uname) + strlen(pswd) + 2);
		strcpy(userpwd, uname);
		strcat(userpwd, ":");
		strcat(userpwd, pswd);
		curl_easy_setopt(curl, CURLOPT_USERPWD, userpwd);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteData);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &internal_struct);
		CURLcode res = curl_easy_perform(curl);
		if (res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		curl_mime_free(form);
		free(url);
		free(userpwd);
		content_destroy(&internal_struct);
	}
	else if (mode == MODE_CONNECTED)
	{
		uint8_t cmd = CMD_SETGAMEIMAGE;
		uint32_t hostsz = strlen(current.short_name);
		uint32_t sz = htonl(hostsz);
		struct stat statbuf;
		if (0 == stat(current.image, &statbuf))
		{
			write(server_socket, &cmd, sizeof(cmd));
			write(server_socket, &sz, sizeof(sz));
			write(server_socket, current.short_name, hostsz);
			hostsz = statbuf.st_size;
			sz = htonl(hostsz);
			write(server_socket, &sz, sizeof(sz));
			char data[1024];
			int fd = open(current.image, O_RDONLY);
			while (hostsz > 0)
			{
				size_t ct = read(fd, data, 1024);
				if (ct > 0)
				{
					hostsz -= ct;
					write(server_socket, data, ct);
				}
			}
		}
	}
}

bool GamerzillaSetTophy(int game_id, const char *name)
{
	if ((mode == MODE_STANDALONE) || (mode == MODE_CONNECTED))
	{
		if (remote_game_id == -1)
		{
			// Get game info
			content internal_struct = GamerzillaGetGameInfo_internal(current.short_name);
			json_t *root;
			json_error_t error;
			root = json_loadb(internal_struct.data, internal_struct.len, 0, &error);
			bool needSend = false;
			// Send if old
			if (root)
			{
				json_t *ver = json_object_get(root, "version");
				if (json_is_string(ver))
				{
					int dbVersion = atol(json_string_value(ver));
					// Should compare everything but trust version for now.
					if (dbVersion < current.version)
						needSend = true;
					json_decref(root);
				}
				else
				{
					needSend = true;
				}
			}
			else
			{
				needSend = true;
			}
			if (needSend)
			{
				GamerzillaSetGameInfo_internal(game_id);
			}
			content_destroy(&internal_struct);
			remote_game_id = 0;
		}
		// Set trophy
		for (int i = 0; i < current.numTrophy; i++)
		{
			if (0 == strcmp(current.trophy[i].name, name))
			{
				currentData[i].achieved = true;
				gamefile_write(&current, currentData);
				break;
			}
		}
		GamerzillaSetTrophy_internal(current.short_name, name);
		return true;
	}
	return false;
}

bool GamerzillaSetTophyStat(int game_id, const char *name, int progress)
{
	return false;
}

static bool GamerzillaServerProcessClient(int fd)
{
	uint8_t cmd;
	ssize_t res = read(fd, &cmd, sizeof(cmd));
	if (res != 1)
	{
		return false;
	}
	switch (cmd)
	{
		case CMD_GETGAMEINFO:
		{
			uint32_t sz;
			uint32_t hostsz;
			content client_content;
			content internal_struct;
			ssize_t ct = 0;
			read(fd, &sz, sizeof(sz));
			hostsz = ntohl(sz);
			content_init(&client_content);
			content_init(&internal_struct);
			content_read(fd, &client_content, hostsz);
			char *name = malloc(client_content.len + 1);
			memcpy(name, client_content.data, client_content.len);
			name[client_content.len] = 0;
			Gamerzilla g;
			TrophyData *t = NULL;
			gamerzillaClear(&g, false);
			// Read save file
			json_t *root = gamefile_read(name);
			if (root)
			{
				GamerzillaMerge(&g, &t, root);
				json_decref(root);
			}
			if (mode != MODE_SERVEROFFLINE)
			{
				// Get online data
				content internal_struct = GamerzillaGetGameInfo_internal(name);
				json_error_t error;
				root = json_loadb(internal_struct.data, internal_struct.len, 0, &error);
				if (root)
				{
					bool update = GamerzillaMerge(&g, &t, root);
					json_decref(root);
					if (update)
					{
						gamefile_write(&g, t);
					}
				}
			}
			root = GamerzillaJson(&g, t);
			char *response = json_dumps(root, 0);
			hostsz = strlen(response);
			sz = htonl(hostsz);
			write(fd, &sz, sizeof(sz));
			write(fd, response, hostsz);
			json_decref(root);
			break;
		}
		case CMD_SETGAMEINFO:
		{
			uint32_t sz;
			uint32_t hostsz;
			content client_content;
			content internal_struct;
			ssize_t ct = 0;
			read(fd, &sz, sizeof(sz));
			hostsz = ntohl(sz);
			content_init(&client_content);
			content_init(&internal_struct);
			content_read(fd, &client_content, hostsz);
			Gamerzilla g;
			TrophyData *t = NULL;
			gamerzillaClear(&g, false);
			json_error_t error;
			json_t *root = json_loadb(client_content.data, client_content.len, 0, &error);
			if (root)
			{
				GamerzillaMerge(&g, &t, root);
				json_decref(root);
				gamefile_write(&g, t);
			}
			if (mode == MODE_SERVERONLINE)
			{
				char *postdata = malloc(client_content.len + 20);
				strcpy(postdata, "game=");
				// TODO: Post encode the response
				memcpy(postdata, client_content.data, client_content.len);
				postdata[client_content.len] = 0;
				char *url, *userpwd;
				url = malloc(strlen(burl) + 20);
				strcpy(url, burl);
				strcat(url, "api/gamerzilla/game/add");
				curl_easy_setopt(curl, CURLOPT_URL, url);
				curl_easy_setopt(curl, CURLOPT_POSTFIELDS, client_content.data);
				curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)client_content.len);
				userpwd = malloc(strlen(uname) + strlen(pswd) + 2);
				strcpy(userpwd, uname);
				strcat(userpwd, ":");
				strcat(userpwd, pswd);
				curl_easy_setopt(curl, CURLOPT_USERPWD, userpwd);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteData);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &internal_struct);
				CURLcode res = curl_easy_perform(curl);
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
			content internal_struct;
			ssize_t ct = 0;
			read(fd, &sz, sizeof(sz));
			hostsz = ntohl(sz);
			content_init(&client_content);
			content_init(&internal_struct);
			content_read(fd, &client_content, hostsz);
			char *game_name = malloc(client_content.len + 1);
			memcpy(game_name, client_content.data, client_content.len);
			game_name[client_content.len] = 0;
			client_content.len = 0;
			read(fd, &sz, sizeof(sz));
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
				GamerzillaMerge(&g, &t, root);
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
				char *postdata = malloc(strlen(game_name) + strlen(name) + 30);
				strcpy(postdata, "game=");
				strcat(postdata, game_name);
				strcat(postdata, "&trophy=");
				strcat(postdata, name);
				char *url, *userpwd;
				url = malloc(strlen(burl) + 20);
				strcpy(url, burl);
				strcat(url, "api/gamerzilla/trophy/set");
				curl_easy_setopt(curl, CURLOPT_URL, url);
				curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postdata);
				curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(postdata));
				userpwd = malloc(strlen(uname) + strlen(pswd) + 2);
				strcpy(userpwd, uname);
				strcat(userpwd, ":");
				strcat(userpwd, pswd);
				curl_easy_setopt(curl, CURLOPT_USERPWD, userpwd);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteData);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &internal_struct);
				CURLcode res = curl_easy_perform(curl);
				if (res != CURLE_OK)
					fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
				free(url);
				free(userpwd);
				content_destroy(&internal_struct);
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
			content internal_struct;
			char *shortname;
			read(fd, &sz, sizeof(sz));
			hostsz = ntohl(sz);
			shortname = (char *)malloc(hostsz + 1);
			read(fd, shortname, hostsz);
			shortname[hostsz] = 0;
			content_init(&client_content);
			content_init(&internal_struct);
			read(fd, &sz, sizeof(sz));
			hostsz = ntohl(sz);
			content_read(fd, &client_content, hostsz);
			char *url, *userpwd;
			curl_mime *form = NULL;
			curl_mimepart *field = NULL;
			url = malloc(strlen(burl) + 30);
			strcpy(url, burl);
			strcat(url, "api/gamerzilla/game/image");
			curl_easy_setopt(curl, CURLOPT_URL, url);
			form = curl_mime_init(curl);
			field = curl_mime_addpart(form);
			curl_mime_data(field, client_content.data, client_content.len);
			curl_mime_filename(field, "image.png");
			curl_mime_name(field, "imagefile");
			field = curl_mime_addpart(form);
			curl_mime_name(field, "game");
			curl_mime_data(field, shortname, CURL_ZERO_TERMINATED);
			curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
			userpwd = malloc(strlen(uname) + strlen(pswd) + 2);
			strcpy(userpwd, uname);
			strcat(userpwd, ":");
			strcat(userpwd, pswd);
			curl_easy_setopt(curl, CURLOPT_USERPWD, userpwd);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteData);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &internal_struct);
			CURLcode res = curl_easy_perform(curl);
			if (res != CURLE_OK)
				fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
			curl_mime_free(form);
			free(url);
			free(userpwd);
			free(shortname);
			content_destroy(&client_content);
			content_destroy(&internal_struct);
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
						client_socket = (int*)malloc(sizeof(int) * size_client);
					}
					else
					{
						int *old_client = client_socket;
						size_client *= 2;
						client_socket = (int*)malloc(sizeof(int) * size_client);
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
					close(client_socket[i]);
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
}
