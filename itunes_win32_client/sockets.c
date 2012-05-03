//
//this is all just some glue code to implement
//tcp socket connections with a 5 sec timeout
//
#include <winsock2.h>
#include <stdio.h>
#define PORT 54321
#define CHUNKSIZE 1000
#define TIMEOUT 5

#define TIMEOUT_MS (TIMEOUT*1000)
static SOCKET sock = INVALID_SOCKET;
static struct sockaddr_in addr;
//FILE *fp;

DWORD WINAPI connectThread( LPVOID lpParam )
{
	char *ip = (char *)lpParam;
	char tmp[256];
	DWORD ticks = GetTickCount();

	if(sock != -1)
	    return TRUE;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	//sock = socket(AF_INET, SOCK_DGRAM, 0);
	

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = htons(PORT);

	

	
	if(connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0)
	{
 
		unsigned long val = TIMEOUT_MS;
		setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO, (const char *)&val, sizeof(val));

	    return TRUE;	    
	}
	sprintf(tmp,"failed to connect to %s:%d (error: %d)", ip, PORT, WSAGetLastError());
	MessageBoxA(0, tmp, "", MB_OK);
	closesocket(sock);
	sock = -1;
	return FALSE;
}

//XXX this is pretty boneheaded, i could've just used non-blocking connect with a select() timeout
BOOL ipconnect(char *ip)
{

	HANDLE h = CreateThread(NULL,0,connectThread,ip,0,NULL);
	if(WaitForSingleObject(h,TIMEOUT_MS) == WAIT_OBJECT_0)
		return TRUE;
	else
	{
        //MessageBox(NULL,"connect() ok","",MB_OK);
        closesocket(sock);
        return FALSE;
	}
}

void disconnect()
{
    //MessageBox(NULL,"disconnect()","",MB_OK);
	closesocket(sock);
	sock = -1;
}

BOOL datasend(void *data, unsigned len)
{
	unsigned char *bytes=data;
	unsigned int pos;
	int s;
	for(pos=0; pos<len; pos+=CHUNKSIZE)
	{
		int sendlen = (pos+CHUNKSIZE) > len ? len-pos : CHUNKSIZE;
        //fprintf(fp,"sending %d(+%d) of %d\n",pos,sendlen,len);
        //fflush(fp);
		if((s=send/*to*/(sock,&(bytes[pos]),sendlen,0)/*, (struct sockaddr *)&addr, sizeof(addr))*/) != sendlen)
		{          
			char tmp[1024];
			sprintf(tmp,"failed to send %d bytes, sent %d (error: %d)",sendlen,s, WSAGetLastError());
			MessageBoxA(NULL,tmp,"",MB_OK);
			closesocket(sock);
			sock = -1;
			return FALSE;
		}
	}
	
	return TRUE;
}

void sendSongTitle(char *ip, char *title, char *artist, char *album)
{
	DWORD ticks = GetTickCount();
	static SOCKET sock;
	static struct sockaddr_in addr;
	unsigned len = strlen(title) + 1 + strlen(artist) + 1 + strlen(album);

	sock = socket(AF_INET, SOCK_STREAM, 0);
	//sock = socket(AF_INET, SOCK_DGRAM, 0);
	

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = htons(PORT+1);

	

	
	if(connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0)
	{
 
		unsigned long val = TIMEOUT_MS;
		int s;
		setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO, (const char *)&val, sizeof(val));
		s = 'T';
		send(sock, (char *)&s, 1 ,0);
		len = htonl(len);
		send(sock, (char *)&len, sizeof(len) ,0);
		len = ntohl(len);
		send(sock, title, strlen(title), 0);
		s = '\n';
		send(sock, (char *)&s, 1 ,0);
		send(sock, artist, strlen(artist), 0);
		send(sock, (char *)&s, 1 ,0);
		send(sock, album, strlen(album), 0);

	    
	}
		
	closesocket(sock);
}


void sendAlbumArt(char *ip, char *data, unsigned len)
{
	DWORD ticks = GetTickCount();
	static SOCKET sock;
	static struct sockaddr_in addr;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	//sock = socket(AF_INET, SOCK_DGRAM, 0);
	

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = htons(PORT+1);

	

	
	if(connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0)
	{
 
		unsigned long val = TIMEOUT_MS;
		unsigned pos,s;
		setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO, (const char *)&val, sizeof(val));
		s = 'A';
		send(sock, (char *)&s, 1 ,0);
		len = htonl(len);
		send(sock, (char *)&len, sizeof(len) ,0);
		len = ntohl(len);
		
		for(pos=0; pos<len; pos+=CHUNKSIZE)
		{
			int sendlen = (pos+CHUNKSIZE) > len ? len-pos : CHUNKSIZE;

			if((s=send(sock,&(data[pos]),sendlen,0)) != sendlen)
				break;
		}
	    
	}
		
	closesocket(sock);
}

/*
void logit(char *msg)
{
        fprintf(fp,"%s\n",msg);
        fflush(fp);    
}
*/
