/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifdef USE_CURL
#include "client.h"

cvar_t	*cl_http_downloads;
cvar_t	*cl_http_filelists;
cvar_t	*cl_http_proxy;
cvar_t	*cl_http_max_connections;

enum
{
	HTTPDL_ABORT_NONE,
	HTTPDL_ABORT_SOFT,
	HTTPDL_ABORT_HARD
};

#define HTTP_MAX_FILELIST_SIZE (1024 * 1024)
#define HTTP_INITIAL_FILELIST_SIZE (128 * 1024)

static CURLM	*multi = NULL;
static int		handleCount = 0;
static int		pendingCount = 0;
static int		abortDownloads = HTTPDL_ABORT_NONE;
static qboolean	downloading_pak = false;
static qboolean	httpDown = false;
/*
===============================
R1Q2 HTTP Downloading Functions
===============================
HTTP downloading is used if the server provides a content
server URL in the connect message. Any missing content the
client needs will then use the HTTP server instead of auto
downloading via UDP. CURL is used to enable multiple files
to be downloaded in parallel to improve performance on high
latency links when small files such as textures are needed.
Since CURL natively supports gzip content encoding, any files
on the HTTP server should ideally be gzipped to conserve
bandwidth.
*/

/*
===============
CL_HTTP_Progress

libcurl callback to update progress info. Mainly just used as
a way to cancel the transfer if required.
===============
*/
static int EXPORT CL_HTTP_Progress (void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
	dlhandle_t *dl;

	dl = (dlhandle_t *)clientp;

	dl->position = (size_t)dlnow;

	//don't care which download shows as long as something does :)
	if (!abortDownloads)
	{
		strcpy (cls.downloadname, dl->queueEntry->quakePath);
		cls.downloadposition = dl->position;

		if (dltotal)
			cls.downloadpercent = (int)(((double)dlnow / (double)dltotal) * 100.0);
		else
			cls.downloadpercent = 0;
	}

	return abortDownloads;
}

/*
===============
CL_HTTP_Header

libcurl callback to update header info.
===============
*/
static size_t EXPORT CL_HTTP_Header (void *ptr, size_t size, size_t nmemb, void *stream)
{
	char	headerBuff[1024];
	size_t	bytes;
	size_t	len;

	bytes = size * nmemb;

	if (bytes <= 16)
		return bytes;

	//memset (headerBuff, 0, sizeof(headerBuff));
	//memcpy (headerBuff, ptr, min(bytes, sizeof(headerBuff)-1));
	if (bytes < sizeof(headerBuff)-1)
		len = bytes;
	else
		len = sizeof(headerBuff)-1;

	Q_strncpy (headerBuff, ptr, len);

	if (!Q_strncasecmp (headerBuff, "Content-Length: ", 16))
	{
		dlhandle_t	*dl;

		dl = (dlhandle_t *)stream;

		if (dl->file)
			dl->fileSize = strtoul (headerBuff + 16, NULL, 10);
	}

	return bytes;
}

/*void CL_RemoveHTTPDownload (const char *quakePath)
{

}*/

/*
===============
CL_EscapeHTTPPath

Properly escapes a path with HTTP %encoding. libcurl's function
seems to treat '/' and such as illegal chars and encodes almost
the entire URL...
===============
*/
static void CL_EscapeHTTPPath (const char *filePath, char *escaped)
{
	static const char hex[] = "0123456789abcdef";
	byte c;

	while ((c = (byte)*filePath++) != 0)
	{
		if (!isalnum (c) && !strchr ("/-_.~", c))
		{
			escaped[0] = '%';
			escaped[1] = hex[c >> 4];
			escaped[2] = hex[c & 15];
			escaped += 3;
		}
		else
		{
			*escaped++ = c;
		}
	}
	*escaped = 0;
}

// Q2Pro uses the logical game directory, never the local filesystem path.
static const char *CL_HTTPGamedir (void)
{
	if (cl.gamedir[0])
		return cl.gamedir;

	return BASEDIRNAME;
}

/*
===============
CL_HTTP_Recv

libcurl callback for filelists.
===============
*/
static size_t EXPORT CL_HTTP_Recv (void *ptr, size_t size, size_t nmemb, void *stream)
{
	dlhandle_t	*dl;
	size_t		bytes;
	size_t		newSize;
	size_t		required;

	dl = (dlhandle_t *)stream;

	if (!size || !nmemb)
		return 0;

	bytes = size * nmemb;
	if (bytes / size != nmemb)
		return 0;

	if (dl->position >= HTTP_MAX_FILELIST_SIZE ||
		bytes >= HTTP_MAX_FILELIST_SIZE - dl->position)
		return 0;

	required = dl->position + bytes + 1;
	newSize = dl->fileSize ? dl->fileSize : HTTP_INITIAL_FILELIST_SIZE;
	while (newSize < required)
		newSize *= 2;

	if (newSize > dl->fileSize)
	{
		char *tmp;

		tmp = Z_TagMalloc ((int)newSize, TAGMALLOC_CLIENT_DOWNLOAD);
		if (dl->tempBuffer)
		{
			memcpy (tmp, dl->tempBuffer, dl->position);
			Z_Free (dl->tempBuffer);
		}
		dl->tempBuffer = tmp;
		dl->fileSize = newSize;
	}

	memcpy (dl->tempBuffer + dl->position, ptr, bytes);
	dl->position += bytes;
	dl->tempBuffer[dl->position] = 0;

	return bytes;
}

int EXPORT CL_CURL_Debug (CURL *c, curl_infotype type, char *data, size_t size, void * ptr)
{
	if (type == CURLINFO_TEXT)
	{
		char	buff[4096];
		if (size > sizeof(buff)-1)
			size = sizeof(buff)-1;
		Q_strncpy (buff, data, size);
		Com_Printf ("DEBUG: %s\n", LOG_GENERAL, buff);
	}

	return 0;
}

/*
===============
CL_StartHTTPDownload

Actually starts a download by adding it to the curl multi
handle.
===============
*/
static void CL_HTTPDownloadFailed (dlqueue_t *entry, dlhandle_t *dl)
{
	if (dl->file)
	{
		fclose (dl->file);
		remove (dl->filePath);
		dl->file = NULL;
	}

	entry->state = DLQ_STATE_DONE;
	if (pendingCount)
		pendingCount--;

	if (!CL_PendingHTTPDownloads() && cls.state == ca_connected)
		CL_RequestNextDownload();
}

static void CL_StartHTTPDownload (dlqueue_t *entry, dlhandle_t *dl)
{
	int			len;
	char		tempFile[MAX_QPATH];
	char		escapedFilePath[MAX_QPATH*4];
	
	//yet another hack to accomodate filelists, how i wish i could push :(
	//NULL file handle indicates filelist.
	len = strlen (entry->quakePath);
	if (len > 9 && !strcmp (entry->quakePath + len - 9, ".filelist"))
	{
		dl->file = NULL;
		dl->filePath[0] = 0;
		CL_EscapeHTTPPath (entry->quakePath, escapedFilePath);
	}
	else
	{
		len = snprintf (dl->filePath, sizeof(dl->filePath), "%s/%s.tmp", FS_Gamedir(), entry->quakePath);
		if (len < 0 || len >= (int)sizeof(dl->filePath))
		{
			Com_Printf ("CL_StartHTTPDownload: Refusing oversize temporary file path.\n", LOG_CLIENT | LOG_WARNING);
			goto fail;
		}

		len = snprintf (tempFile, sizeof(tempFile), "%s/%s", CL_HTTPGamedir(), entry->quakePath);
		if (len < 0 || len >= (int)sizeof(tempFile))
		{
			Com_Printf ("CL_StartHTTPDownload: Refusing oversize server file path.\n", LOG_CLIENT | LOG_WARNING);
			goto fail;
		}
		CL_EscapeHTTPPath (tempFile, escapedFilePath);

		FS_CreatePath (dl->filePath);

		//don't bother with http resume... too annoying if server doesn't support it.
		dl->file = fopen (dl->filePath, "wb");
		if (!dl->file)
		{
			Com_Printf ("CL_StartHTTPDownload: Couldn't open %s for writing.\n", LOG_CLIENT | LOG_WARNING, dl->filePath);
			goto fail;
		}
	}

	dl->tempBuffer = NULL;
	dl->speed = 0;
	dl->fileSize = 0;
	dl->position = 0;
	dl->queueEntry = entry;

	if (!dl->curl)
		dl->curl = curl_easy_init ();
	if (!dl->curl)
	{
		Com_Printf ("CL_StartHTTPDownload: curl_easy_init failed.\n", LOG_CLIENT | LOG_WARNING);
		goto fail;
	}

	len = snprintf (dl->URL, sizeof(dl->URL), "%s%s", cls.downloadServer, escapedFilePath);
	if (len < 0 || len >= (int)sizeof(dl->URL))
	{
		Com_Printf ("CL_StartHTTPDownload: Refusing oversize download URL.\n", LOG_CLIENT | LOG_WARNING);
		goto fail;
	}

	curl_easy_setopt (dl->curl, CURLOPT_ACCEPT_ENCODING, "");
	//curl_easy_setopt (dl->curl, CURLOPT_DEBUGFUNCTION, CL_CURL_Debug);
	//curl_easy_setopt (dl->curl, CURLOPT_VERBOSE, 1);
	curl_easy_setopt (dl->curl, CURLOPT_NOPROGRESS, 0L);
	if (dl->file)
	{
		curl_easy_setopt (dl->curl, CURLOPT_WRITEDATA, dl->file);
		curl_easy_setopt (dl->curl, CURLOPT_WRITEFUNCTION, NULL);
		curl_easy_setopt (dl->curl, CURLOPT_MAXFILESIZE, 0L);
	}
	else
	{
		curl_easy_setopt (dl->curl, CURLOPT_WRITEDATA, dl);
		curl_easy_setopt (dl->curl, CURLOPT_WRITEFUNCTION, CL_HTTP_Recv);
		curl_easy_setopt (dl->curl, CURLOPT_MAXFILESIZE, HTTP_MAX_FILELIST_SIZE - 1L);
	}
	curl_easy_setopt (dl->curl, CURLOPT_PROXY, cl_http_proxy->string);
	curl_easy_setopt (dl->curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt (dl->curl, CURLOPT_MAXREDIRS, 5L);
	curl_easy_setopt (dl->curl, CURLOPT_CONNECTTIMEOUT, 10L);
	curl_easy_setopt (dl->curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
	curl_easy_setopt (dl->curl, CURLOPT_LOW_SPEED_TIME, 30L);
	curl_easy_setopt (dl->curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt (dl->curl, CURLOPT_PROTOCOLS_STR, "http,https");
	curl_easy_setopt (dl->curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
	curl_easy_setopt (dl->curl, CURLOPT_WRITEHEADER, dl);
	curl_easy_setopt (dl->curl, CURLOPT_HEADERFUNCTION, CL_HTTP_Header);
	curl_easy_setopt (dl->curl, CURLOPT_XFERINFOFUNCTION, CL_HTTP_Progress);
	curl_easy_setopt (dl->curl, CURLOPT_XFERINFODATA, dl);
	curl_easy_setopt (dl->curl, CURLOPT_USERAGENT, Cvar_VariableString ("version"));
	curl_easy_setopt (dl->curl, CURLOPT_REFERER, cls.downloadReferer);
	curl_easy_setopt (dl->curl, CURLOPT_URL, dl->URL);

	if (curl_multi_add_handle (multi, dl->curl) != CURLM_OK)
	{
		Com_Printf ("curl_multi_add_handle: error\n", LOG_ERROR|LOG_CLIENT);
		goto fail;
	}

	handleCount++;
	//Com_Printf ("started dl: hc = %d\n", LOG_GENERAL, handleCount);
	Com_DPrintf  ("CL_StartHTTPDownload: Fetching %s...\n", dl->URL);
	dl->queueEntry->state = DLQ_STATE_RUNNING;
	return;

fail:
	CL_HTTPDownloadFailed (entry, dl);
}

/*
===============
CL_InitHTTPDownloads

Init libcurl and multi handle.
===============
*/
void CL_InitHTTPDownloads (void)
{
	if (curl_global_init (CURL_GLOBAL_DEFAULT) != CURLE_OK)
	{
		httpDown = true;
		Com_Printf ("curl_global_init failed; HTTP downloading disabled.\n", LOG_CLIENT | LOG_WARNING);
		return;
	}

	//Com_Printf ("%s initialized.\n", LOG_CLIENT, curl_version());
}

/*
===============
CL_SetHTTPServer

A new server is specified, so we nuke all our state.
===============
*/
void CL_SetHTTPServer (const char *URL)
{
	dlqueue_t	*q, *last;
	size_t		urlLen;

	CL_HTTP_Cleanup (false);

	q = &cls.downloadQueue;

	last = NULL;

	while (q->next)
	{
		q = q->next;

		if (last)
			Z_Free (last);

		last = q;
	}

	if (last)
		Z_Free (last);

	if (multi)
		Com_Error (ERR_DROP, "CL_SetHTTPServer: Still have old handle");

	multi = NULL;
	memset (&cls.downloadQueue, 0, sizeof(cls.downloadQueue));

	abortDownloads = HTTPDL_ABORT_NONE;
	handleCount = pendingCount = 0;
	cls.downloadServer[0] = 0;

	if (httpDown || !URL || !URL[0] || !cl_http_downloads->intvalue)
		return;

	if (strncmp (URL, "http://", 7) && strncmp (URL, "https://", 8))
	{
		Com_Printf ("Ignoring download server URL with non-HTTP schema.\n", LOG_CLIENT | LOG_WARNING);
		return;
	}

	urlLen = strlen (URL);
	if (urlLen >= sizeof(cls.downloadServer))
	{
		Com_Printf ("Ignoring oversize download server URL.\n", LOG_CLIENT | LOG_WARNING);
		return;
	}

	multi = curl_multi_init ();
	if (!multi)
	{
		Com_Printf ("curl_multi_init failed; HTTP downloading disabled.\n", LOG_CLIENT | LOG_WARNING);
		return;
	}

	memcpy (cls.downloadServer, URL, urlLen + 1);
}
/*
===============
CL_CancelHTTPDownloads

Cancel all downloads and nuke the queue.
===============
*/
void CL_ResetPrecacheCheck (void);
void CL_CancelHTTPDownloads (qboolean permKill)
{
	dlqueue_t	*q;

	if (permKill)
	{
		CL_ResetPrecacheCheck ();
		abortDownloads = HTTPDL_ABORT_HARD;
	}
	else
		abortDownloads = HTTPDL_ABORT_SOFT;

	q = &cls.downloadQueue;

	while (q->next)
	{
		q = q->next;
		if (q->state == DLQ_STATE_NOT_STARTED)
			q->state = DLQ_STATE_DONE;
	}

	if (!pendingCount && !handleCount)
	{
		if (abortDownloads == HTTPDL_ABORT_HARD)
			cls.downloadServer[0] = 0;
		else if (abortDownloads == HTTPDL_ABORT_SOFT)
			abortDownloads = HTTPDL_ABORT_NONE;
	}

	pendingCount = 0;
}

/*
===============
CL_ResetHTTPQueue

Reset the HTTP download queue between map changes (soft reconnects)
while keeping the current downloadServer URL active.
===============
*/
void CL_ResetHTTPQueue (void)
{
	dlqueue_t	*q, *last;
	int			i;

	if (!cls.downloadServer[0])
		return;

	for (i = 0; i < 4; i++)
	{
		dlhandle_t *dl = &cls.HTTPHandles[i];

		if (dl->file)
		{
			fclose (dl->file);
			remove (dl->filePath);
			dl->file = NULL;
		}

		if (dl->tempBuffer)
		{
			Z_Free (dl->tempBuffer);
			dl->tempBuffer = NULL;
		}

		if (dl->curl && multi)
			curl_multi_remove_handle (multi, dl->curl);

		dl->queueEntry = NULL;
	}

	q = &cls.downloadQueue;
	last = NULL;
	while (q->next)
	{
		q = q->next;
		if (last)
			Z_Free (last);
		last = q;
	}
	if (last)
		Z_Free (last);

	memset (&cls.downloadQueue, 0, sizeof(cls.downloadQueue));
	abortDownloads = HTTPDL_ABORT_NONE;
	handleCount = 0;
	pendingCount = 0;
	downloading_pak = false;
}

/*
===============
CL_QueueHTTPDownload

Called from the precache check to queue a download. Return value of
false will cause standard UDP downloading to be used instead.
===============
*/
qboolean CL_QueueHTTPDownload (const char *quakePath)
{
	int			len;
	dlqueue_t	*q;
	qboolean	needList;
	char		listPath[MAX_QPATH];

	// no http server (or we got booted)
	if (!cls.downloadServer[0] || abortDownloads || !cl_http_downloads->intvalue)
		return false;

	len = strlen (quakePath);
	if (len < 1 || len >= MAX_QPATH)
		return false;

	needList = false;

	// first download queued, so we want the mod filelist
	if (!cls.downloadQueue.next && cl_http_filelists->intvalue)
		needList = true;

	q = &cls.downloadQueue;

	while (q->next)
	{
		q = q->next;

		//avoid sending duplicate requests
		if (!strcmp (quakePath, q->quakePath))
		{
			if (q->state == DLQ_STATE_DONE)
				return false;
			return true;
		}
	}

	q->next = Z_TagMalloc (sizeof(*q), TAGMALLOC_CLIENT_DOWNLOAD);
	q = q->next;

	q->next = NULL;
	q->state = DLQ_STATE_NOT_STARTED;
	Q_strncpy (q->quakePath, quakePath, sizeof(q->quakePath)-1);

	if (needList)
	{
		//grab the filelist
		len = snprintf (listPath, sizeof(listPath), "%s.filelist", CL_HTTPGamedir());
		if (len >= 0 && len < (int)sizeof(listPath))
			CL_QueueHTTPDownload (listPath);

		//this is a nasty hack to let the server know what we're doing so admins don't
		//get confused by a ton of people stuck in CNCT state. it's assumed the server
		//is running r1q2 if we're even able to do http downloading so hopefully this
		//won't spew an error msg.
		MSG_BeginWriting (clc_stringcmd);
		MSG_WriteString ("download http\n");
		MSG_EndWriting (&cls.netchan.message);
	}

	//special case for map file lists, i really wanted a server-push mechanism for this, but oh well
	len = strlen (quakePath);
	if (cl_http_filelists->intvalue && len > 4 && !Q_stricmp (quakePath + len - 4, ".bsp"))
	{
		len = snprintf (listPath, sizeof(listPath), "%s/%.*s.filelist", CL_HTTPGamedir(), len - 4, quakePath);
		if (len >= 0 && len < (int)sizeof(listPath))
			CL_QueueHTTPDownload (listPath);
	}

	//if a download entry has made it this far, CL_FinishHTTPDownload is guaranteed to be called.
	pendingCount++;

	return true;
}

/*
===============
CL_PendingHTTPDownloads

See if we're still busy with some downloads. Called by precacher just
before it loads the map since we could be downloading the map. If we're
busy still, it'll wait and CL_FinishHTTPDownload will pick up from where
it left.
===============
*/
qboolean CL_PendingHTTPDownloads (void)
{
	dlqueue_t	*q;

	if (!cls.downloadServer[0])
		return false;

	return pendingCount + handleCount;

	q = &cls.downloadQueue;

	while (q->next)
	{
		q = q->next;
		if (q->state != DLQ_STATE_DONE)
			return true;
	}

	return false;
}

/*
===============
CL_ParseFileList

Validate a path supplied by a filelist.
===============
*/
static void CL_CheckAndQueueDownload (char *path)
{
	size_t		length;
	char		*ext;
	qboolean	pak;
	qboolean	gameLocal;

	StripHighBits (path, 1);

	length = strlen(path);

	if (length >= MAX_QPATH)
		return;

	ext = strrchr (path, '.');

	if (!ext)
		return;

	ext++;

	if (!ext[0])
		return;

	Q_strlwr (ext);

	if (!strcmp (ext, "pak"))
	{
		Com_Printf ("NOTICE: Filelist is requesting a .pak file (%s)\n", LOG_CLIENT, path);
		pak = true;
	}
	else
		pak = false;

	if (!pak && strcmp (ext, "pcx") && strcmp (ext, "wal") && strcmp (ext, "wav") && strcmp (ext, "md2") &&
		strcmp (ext, "sp2") && strcmp (ext, "tga") && strcmp (ext, "png") && strcmp (ext, "jpg") &&
		strcmp (ext, "bsp") && strcmp (ext, "ent") && strcmp (ext, "txt") && strcmp (ext, "dm2") &&
		strcmp (ext, "loc"))
	{
		Com_Printf ("WARNING: Illegal file type '%s' in filelist.\n", LOG_CLIENT|LOG_WARNING, MakePrintable(path, length));
		return;
	}

	if (path[0] == '@')
	{
		if (pak)
		{
			Com_Printf ("WARNING: @ prefix used on a pak file (%s) in filelist.\n", LOG_CLIENT|LOG_WARNING, MakePrintable(path, length));
			return;
		}
		gameLocal = true;
		path++;
		length--;
	}
	else
		gameLocal = false;

	if (strstr (path, "..") || !isvalidchar (path[0]) || !isvalidchar (path[length-1]) || strstr(path, "//") ||
		strchr (path, '\\') || (!pak && !strchr (path, '/')) || (pak && strchr(path, '/')))
	{
		Com_Printf ("WARNING: Illegal path '%s' in filelist.\n", LOG_CLIENT|LOG_WARNING, MakePrintable(path, length));
		return;
	}

	//by definition paks are game-local
	if (gameLocal || pak)
	{
		qboolean	exists;
		FILE		*f;
		char		gamePath[MAX_OSPATH];

		if (pak)
		{
			Com_sprintf (gamePath, sizeof(gamePath),"%s/%s",FS_Gamedir(), path);
			f = fopen (gamePath, "rb");
			if (!f)
			{
				exists = false;;
			}
			else
			{
				exists = true;
				fclose (f);
			}
		}
		else
		{
			exists = FS_ExistsInGameDir (path);
		}

		if (!exists)
		{
			if (CL_QueueHTTPDownload (path))
			{
				//paks get bumped to the top and HTTP switches to single downloading.
				//this prevents someone on 28k dialup trying to do both the main .pak
				//and referenced configstrings data at once.
				if (pak)
				{
					dlqueue_t	*q, *last;

					last = q = &cls.downloadQueue;

					while (q->next)
					{
						last = q;
						q = q->next;
					}

					last->next = NULL;
					q->next = cls.downloadQueue.next;
					cls.downloadQueue.next = q;
				}
			}
		}
	}
	else
	{
		CL_CheckOrDownloadFile (path);
	}
}

/*
===============
CL_ParseFileList

A filelist is in memory, scan and validate it and queue up the files.
===============
*/
static void CL_ParseFileList (dlhandle_t *dl)
{
	char	 *list;
	char	*p;

	if (!dl->tempBuffer)
		return;

	if (!cl_http_filelists->intvalue)
	{
		Z_Free (dl->tempBuffer);
		dl->tempBuffer = NULL;
		return;
	}

	list = dl->tempBuffer;

	for (;;)
	{
		p = strchr (list, '\n');
		if (p)
		{
			p[0] = 0;
			if (list[0])
				CL_CheckAndQueueDownload (list);
			list = p + 1;
		}
		else
		{
			if (list[0])
				CL_CheckAndQueueDownload (list);
			break;
		}
	}

	Z_Free (dl->tempBuffer);
	dl->tempBuffer = NULL;
}

/*
===============
CL_ReVerifyHTTPQueue

A pak file just downloaded, let's see if we can remove some stuff from
the queue which is in the .pak.
===============
*/
static void CL_ReVerifyHTTPQueue (void)
{
	dlqueue_t	*q;

	q = &cls.downloadQueue;

	pendingCount = 0;

	while (q->next)
	{
		q = q->next;
		if (q->state == DLQ_STATE_NOT_STARTED)
		{
			if (FS_LoadFile (q->quakePath, NULL) != -1)
				q->state = DLQ_STATE_DONE;
			else
				pendingCount++;
		}
	}
}

/*
===============
CL_HTTP_Cleanup

Quake II is exiting or we're changing servers. Clean up.
===============
*/
void CL_HTTP_Cleanup (qboolean fullShutdown)
{
	dlhandle_t	*dl;
	int			i;

	if (fullShutdown && httpDown)
		return;

	for (i = 0; i < 4; i++)
	{
		dl = &cls.HTTPHandles[i];

		if (dl->file)
		{
			fclose (dl->file);
			remove (dl->filePath);
			dl->file = NULL;
		}

		if (dl->tempBuffer)
		{
			Z_Free (dl->tempBuffer);
			dl->tempBuffer = NULL;
		}

		if (dl->curl)
		{
			if (multi)
				curl_multi_remove_handle (multi, dl->curl);
			curl_easy_cleanup (dl->curl);
			dl->curl = NULL;
		}

		dl->queueEntry = NULL;
	}

	if (multi)
	{
		curl_multi_cleanup (multi);
		multi = NULL;
	}

	if (fullShutdown)
	{
		curl_global_cleanup ();
		httpDown = true;
	}
}

/*
===============
CL_FinishHTTPDownload

A download finished, find out what it was, whether there were any errors and
if so, how severe. If none, rename file and other such stuff.
===============
*/
static void CL_FinishHTTPDownload (void)
{
	size_t		i;
	int			msgs_in_queue;
	CURLMsg		*msg;
	CURLcode	result;
	dlhandle_t	*dl = NULL;
	CURL		*curl;
	long		responseCode;
	double		timeTaken;
	curl_off_t	fileSize;
	char		tempName[MAX_OSPATH];
	qboolean	isFile;

	do
	{
		msg = curl_multi_info_read (multi, &msgs_in_queue);

		if (!msg)
		{
			Com_Printf ("CL_FinishHTTPDownload: Odd, no message for us...\n", LOG_CLIENT);
			return;
		}

		if (msg->msg != CURLMSG_DONE)
		{
			Com_Printf ("CL_FinishHTTPDownload: Got some weird message...\n", LOG_CLIENT);
			continue;
		}

		curl = msg->easy_handle;

		// curl doesn't provide reverse-lookup of the void * ptr, so search for it
		for (i = 0; i < 4; i++)
		{
			if (cls.HTTPHandles[i].curl == curl)
			{
				dl = &cls.HTTPHandles[i];
				break;
			}
		}

		if (i == 4)
		{
			Com_Error (ERR_DROP, "CL_FinishHTTPDownload: Handle not found");
			continue;
		}

		//we mark everything as done even if it errored to prevent multiple
		//attempts.
		dl->queueEntry->state = DLQ_STATE_DONE;

		//filelist processing is done on read
		if (dl->file)
			isFile = true;
		else
			isFile = false;

		if (isFile)
		{
			fclose (dl->file);
			dl->file = NULL;
		}

		//might be aborted
		if (pendingCount)
			pendingCount--;
		handleCount--;
		//Com_Printf ("finished dl: hc = %d\n", LOG_GENERAL, handleCount);
		cls.downloadname[0] = 0;
		cls.downloadposition = 0;

		result = msg->data.result;

		switch (result)
		{
			//for some reason curl returns CURLE_OK for a 404...
			case CURLE_HTTP_RETURNED_ERROR:
			case CURLE_OK:
			
				curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &responseCode);
				if (responseCode == 404)
				{
					i = strlen (dl->queueEntry->quakePath);
					if (i >= 4 && !strcmp (dl->queueEntry->quakePath + i - 4, ".pak"))
						downloading_pak = false;

					if (isFile)
						remove (dl->filePath);
					Com_Printf ("HTTP(%s): 404 File Not Found [%d remaining files]\n", LOG_CLIENT, dl->queueEntry->quakePath, pendingCount);
					curl_easy_getinfo (curl, CURLINFO_SIZE_DOWNLOAD_T, &fileSize);
					if (fileSize > 512)
					{
						//ick
						isFile = false;
						result = CURLE_FILESIZE_EXCEEDED;
						Com_Printf ("Oversized 404 body received (%d bytes), aborting HTTP downloading.\n", LOG_CLIENT, (int)fileSize);
					}
					else
					{
						curl_multi_remove_handle (multi, dl->curl);
						continue;
					}
				}
				else if (responseCode == 200)
				{
					if (!isFile && !abortDownloads)
						CL_ParseFileList (dl);
					break;
				}

				//every other code is treated as fatal, fallthrough here
				Com_Printf ("Bad HTTP response code %ld for %s, aborting HTTP downloading.\n", LOG_CLIENT, responseCode, dl->queueEntry->quakePath);

			//fatal error, disable http
			case CURLE_COULDNT_RESOLVE_HOST:
			case CURLE_COULDNT_CONNECT:
			case CURLE_COULDNT_RESOLVE_PROXY:
				if (isFile)
					remove (dl->filePath);
				Com_Printf ("Fatal HTTP error: %s\n", LOG_CLIENT|LOG_WARNING, curl_easy_strerror (result));
				curl_multi_remove_handle (multi, dl->curl);
				if (abortDownloads)
					continue;
				CL_CancelHTTPDownloads (true);
				continue;
			default:
				i = strlen (dl->queueEntry->quakePath);
				if (i >= 4 && !strcmp (dl->queueEntry->quakePath + i - 4, ".pak"))
					downloading_pak = false;
				if (isFile)
					remove (dl->filePath);
				Com_Printf ("HTTP download failed: %s\n", LOG_CLIENT|LOG_WARNING, curl_easy_strerror (result));
				curl_multi_remove_handle (multi, dl->curl);
				continue;
		}

		if (isFile)
		{
			//rename the temp file
			Com_sprintf (tempName, sizeof(tempName), "%s/%s", FS_Gamedir(), dl->queueEntry->quakePath);

			remove (tempName);
			if (rename (dl->filePath, tempName))
				Com_Printf ("Failed to rename %s for some odd reason...", LOG_CLIENT|LOG_ERROR, dl->filePath);

			//a pak file is very special...
			i = strlen (tempName);
			if (!strcmp (tempName + i - 4, ".pak"))
			{
				FS_FlushCache ();
				FS_ReloadPAKs ();
				CL_ReVerifyHTTPQueue ();
				downloading_pak = false;
			}
		}

		//show some stats
		curl_easy_getinfo (curl, CURLINFO_TOTAL_TIME, &timeTaken);
		curl_easy_getinfo (curl, CURLINFO_SIZE_DOWNLOAD_T, &fileSize);

		//FIXME:
		//technically i shouldn't need to do this as curl will auto reuse the
		//existing handle when you change the URL. however, the handleCount goes
		//all weird when reusing a download slot in this way. if you can figure
		//out why, please let me know.
		curl_multi_remove_handle (multi, dl->curl);

		Com_Printf ("HTTP(%s): %.f bytes, %.2fkB/sec [%d remaining files]\n", LOG_CLIENT, dl->queueEntry->quakePath, (double)fileSize, timeTaken > 0.0 ? ((double)fileSize / 1024.0) / timeTaken : 0.0, pendingCount);
	} while (msgs_in_queue > 0);

	FS_FlushCache ();

	if (handleCount == 0)
	{
		if (abortDownloads == HTTPDL_ABORT_SOFT)
			abortDownloads = HTTPDL_ABORT_NONE;
		else if (abortDownloads == HTTPDL_ABORT_HARD)
			cls.downloadServer[0] = 0;
	}

	// done current batch, see if we have more to dl - maybe a .bsp needs downloaded
	if (cls.state == ca_connected && !CL_PendingHTTPDownloads())
		CL_RequestNextDownload ();
}

/*
===============
CL_GetFreeDLHandle

Find a free download handle to start another queue entry on.
===============
*/
static dlhandle_t *CL_GetFreeDLHandle (void)
{
	dlhandle_t	*dl;
	int			i;

	for (i = 0; i < 4; i++)
	{
		dl = &cls.HTTPHandles[i];
		if (!dl->queueEntry || dl->queueEntry->state == DLQ_STATE_DONE)
			return dl;
	}

	return NULL;
}

/*
===============
CL_StartNextHTTPDownload

Start another HTTP download if possible.
===============
*/
static void CL_StartNextHTTPDownload (void)
{
	dlqueue_t	*q;

	q = &cls.downloadQueue;

	while (q->next)
	{
		q = q->next;
		if (q->state == DLQ_STATE_NOT_STARTED)
		{
			size_t		len;

			dlhandle_t	*dl;

			dl = CL_GetFreeDLHandle();

			if (!dl)
				return;

			CL_StartHTTPDownload (q, dl);

			//ugly hack for pak file single downloading
			len = strlen (q->quakePath);
			if (q->state == DLQ_STATE_RUNNING && len > 4 && !Q_stricmp (q->quakePath + len - 4, ".pak"))
				downloading_pak = true;

			break;
		}
	}
}

/*
===============
CL_RunHTTPDownloads

This calls curl_multi_perform do actually do stuff. Called every frame while
connecting to minimise latency. Also starts new downloads if we're not doing
the maximum already.
===============
*/
void CL_RunHTTPDownloads (void)
{
	int			newHandleCount;
	CURLMcode	ret;

	if (!cls.downloadServer[0])
		return;

	//Com_Printf ("handle %d, pending %d\n", LOG_GENERAL, handleCount, pendingCount);

	//not enough downloads running, queue some more!
	if (pendingCount && abortDownloads == HTTPDL_ABORT_NONE &&
		!downloading_pak && handleCount < cl_http_max_connections->intvalue)
		CL_StartNextHTTPDownload ();

	do
	{
		ret = curl_multi_perform (multi, &newHandleCount);
		if (newHandleCount < handleCount)
		{
			//Com_Printf ("runnd dl: hc = %d, nc = %d\n", LOG_GENERAL, handleCount, newHandleCount);
			//hmm, something either finished or errored out.
			CL_FinishHTTPDownload ();
			handleCount = newHandleCount;
		}
	}
	while (ret == CURLM_CALL_MULTI_PERFORM);

	if (ret != CURLM_OK)
	{
		Com_Printf ("curl_multi_perform error. Aborting HTTP downloads.\n", LOG_ERROR|LOG_CLIENT);
		CL_CancelHTTPDownloads (true);
	}

	//not enough downloads running, queue some more!
	if (pendingCount && abortDownloads == HTTPDL_ABORT_NONE &&
		!downloading_pak && handleCount < cl_http_max_connections->intvalue)
		CL_StartNextHTTPDownload ();
}

#endif
