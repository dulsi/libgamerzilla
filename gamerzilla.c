#include <gamerzilla.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <jansson.h>

#define MODE_STANDALONE 0
#define MODE_SERVER 1
#define MODE_CONNECTED 2

static int mode = MODE_STANDALONE;
static char *burl = NULL;
static char *uname = NULL;
static char *pswd = NULL;
static int game_num = 0;
static char **game_list = NULL;
static Gamerzilla current;
static int remote_game_id = -1;
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
	}
	else
	{
		// Try connect to server
	}
	return false;
}

bool GamerzillaConnect(const char *baseurl, const char *username, const char *password)
{
	burl = strdup(baseurl);
	uname = strdup(username);
	pswd = strdup(password);
	curl = curl_easy_init();
	return false;
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

bool GamerzillaSetTophy(int game_id, const char *name)
{
	if (mode == MODE_STANDALONE)
	{
		if (burl)
		{
			if (remote_game_id == -1)
			{
				// Get game info
				char *url, *postdata, *userpwd;
				content internal_struct;
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
			}
			// Set trophy
		}
	}
	return false;
}

bool GamerzillaSetTophyStat(int game_id, const char *name, int progress)
{
	return false;
}
