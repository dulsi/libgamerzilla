#include <gamerzilla.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <jansson.h>

#include <sys/un.h>
#include <sys/socket.h>

#define MODE_OFFLINE 0
#define MODE_SERVER 1
#define MODE_CONNECTED 2
#define MODE_STANDALONE 0

static int mode = MODE_OFFLINE;
static char *burl = NULL;
static char *uname = NULL;
static char *pswd = NULL;
static int game_num = 0;
static char **game_list = NULL;
static Gamerzilla current;
static int remote_game_id = -1;
static int server_socket;
static int *client_socket = NULL;
static int num_client = 0;
static int size_client = 0;
CURL *curl = NULL;

typedef struct {
	size_t size;
	size_t len;
	char *data;
} content;

void content_init(content *x)
{
	x->size = 0;
	x->len = 0;
	x->data = 0;
}

void content_destroy(content *x)
{
	x->size = 0;
	x->len = 0;
	free(x->data);
	x->data = 0;
}

static size_t curlWriteData(void *buffer, size_t size, size_t nmemb, void *userp)
{
	content *response = (content *)userp;
	if (response->size - response->len < size * nmemb)
	{
		if (response->size == 0)
			response->size = 1024;
		while (response->size - response->len < size * nmemb)
		{
			response->size = response->size * 2;
		}
		char *dataOld = response->data;
		response->data = (char *)malloc(response->size);
		memcpy(response->data, dataOld, response->len);
		free(dataOld);
	}
	memcpy(response->data + response->len, buffer, size * nmemb);
	response->len += size * nmemb;
	return nmemb;
}

bool GamerzillaInit(bool bServer)
{
	if (bServer)
	{
		// Initialize socket
		mode = MODE_SERVER;
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
	// TODO: Try connecting
	return true;
}

int GamerzillaGameInit(Gamerzilla *g)
{
	current.short_name = strdup(g->short_name);
	current.name = strdup(g->name);
	current.version = g->version;
	current.numTrophy = g->numTrophy;
	current.trophy = (GamerzillaTrophy*)malloc(sizeof(GamerzillaTrophy) * current.numTrophy);
	for (int i = 0; i < current.numTrophy; i++)
	{
		current.trophy[i].name = strdup(g->trophy[i].name);
		current.trophy[i].desc = strdup(g->trophy[i].desc);
		current.trophy[i].max_progress = g->trophy[i].max_progress;
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

content GamerzillaGetGameInfo_internal(int game_id, const char *name)
{
	content internal_struct;
	if (mode == MODE_STANDALONE)
	{
		// Get game info
		char *url, *postdata, *userpwd;
		content_init(&internal_struct);
		url = malloc(strlen(burl) + 20);
		strcpy(url, burl);
		strcat(url, "api/gamerzilla/game");
		curl_easy_setopt(curl, CURLOPT_URL, url);
		postdata = malloc(strlen(current.short_name) + 20);
		strcpy(postdata, "game=");
		strcat(postdata, current.short_name);
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
		free(postdata);
		free(userpwd);
	}
	else if (mode == MODE_CONNECTED)
	{
		content_init(&internal_struct);
	}
	return internal_struct;
}

bool GamerzillaSetTophy(int game_id, const char *name)
{
	if (mode == MODE_STANDALONE)
	{
		if (remote_game_id == -1)
		{
			// Get game info
			content internal_struct = GamerzillaGetGameInfo_internal(game_id, name);
			json_t *root;
			json_error_t error;
			root = json_loads(internal_struct.data, 0, &error);
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
				char tmp[512];
				char *url, *postdata, *userpwd;
				root = json_object();
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
				internal_struct.len = 0;
				url = malloc(strlen(burl) + 30);
				strcpy(url, burl);
				strcat(url, "api/gamerzilla/game/add");
				curl_easy_setopt(curl, CURLOPT_URL, url);
				postdata = malloc(strlen(response) + 20);
				strcpy(postdata, "game=");
				// TODO: Post encode the response
				strcat(postdata, response);
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
				free(postdata);
				free(userpwd);
				free(response);
			}
			content_destroy(&internal_struct);
			remote_game_id = 0;
		}
		// Set trophy
		int trophy_num = 0;
		while (trophy_num < current.numTrophy)
		{
			if (strcmp(current.trophy[trophy_num].name, name) == 0)
				break;
			trophy_num++;
		}
		if (trophy_num == current.numTrophy)
		{
			fprintf(stderr, "Failed to find trophy %s\n", name);
			return false; // Should not get here.
		}
		char *url, *postdata, *userpwd;
		content internal_struct;
		content_init(&internal_struct);
		url = malloc(strlen(burl) + 30);
		strcpy(url, burl);
		strcat(url, "api/gamerzilla/trophy/set");
		curl_easy_setopt(curl, CURLOPT_URL, url);
		postdata = malloc(strlen(current.short_name) + strlen(current.trophy[trophy_num].name) + 30);
		strcpy(postdata, "game=");
		strcat(postdata, current.short_name);
		strcat(postdata, "&trophy=");
		strcat(postdata, current.trophy[trophy_num].name);
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
		free(postdata);
		free(userpwd);
		content_destroy(&internal_struct);
		return true;
	}
	return false;
}

bool GamerzillaSetTophyStat(int game_id, const char *name, int progress)
{
	return false;
}

void GamerzillaServerProcess(struct timeval *timeout)
{
	fd_set readfds;
	int nfds = server_socket;
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
			}
		}
	}
}
