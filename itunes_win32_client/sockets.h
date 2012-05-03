void init_sockets();
BOOL ipconnect(char *ip);
BOOL datasend(void *data, unsigned len);
void disconnect();

void sendSongTitle(char *ip, char *title, char *artist, char *album);
void sendAlbumArt(char *ip, char *data, unsigned len);