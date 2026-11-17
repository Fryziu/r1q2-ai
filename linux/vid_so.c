// Main windowed and fullscreen graphics interface module for the
// SDL OpenGL rendering version of the Quake refresh engine.

#define SO_FILE "/etc/quake2.conf"

#include <assert.h>
#ifdef _WIN32
#include <windows.h>
#define Sys_LoadSymbol(lib, sym) ((void *)GetProcAddress((HMODULE)(lib), (sym)))
#else
#include <dlfcn.h> // ELF dl loader
#include <sys/stat.h>
#include <unistd.h>
#define Sys_LoadSymbol(lib, sym) dlsym((lib), (sym))
#endif
#include <errno.h>

#include <SDL.h>

#include "../client/client.h"

#include "../linux/rw_linux.h"

qboolean reload_video = false;

// Structure containing functions exported from refresh DLL
refexport_t		re;
DrawCrosshair_t	re_DrawCrosshair;

// Console variables that we need to access from this module
cvar_t		*vid_gamma;
cvar_t		*vid_ref;			// Name of Refresh DLL loaded
cvar_t		*vid_xpos;			// X coordinate of window position
cvar_t		*vid_ypos;			// Y coordinate of window position
cvar_t		*vid_fullscreen;
cvar_t		*vid_display;
cvar_t		*vid_width;
cvar_t		*vid_height;

// Global variables used internally by this module
viddef_t	viddef;				// global video state; used by other modules
void		*reflib_library;		// Handle to refresh DLL 
qboolean	reflib_active = 0;

#define VID_NUM_MODES ( sizeof( vid_modes ) / sizeof( vid_modes[0] ) )

/** KEYBOARD **************************************************************/

void Do_Key_Event(int key, qboolean down);

void (*KBD_Update_fp)(void);
void (*KBD_Init_fp)(Key_Event_fp_t fp);
void (*KBD_Close_fp)(void);

int (*RW_VID_GetNumDisplays_fp)(void);
int (*RW_VID_GetDisplayMode_fp)(int display_index, int *width, int *height, const char **name);

/** MOUSE *****************************************************************/

in_state_t in_state;

void (*RW_IN_Init_fp)(in_state_t *in_state_p);
void (*RW_IN_Shutdown_fp)(void);
void (*RW_IN_Activate_fp)(qboolean active);
void (*RW_IN_Commands_fp)(void);
void (*RW_IN_Move_fp)(usercmd_t *cmd);
void (*RW_IN_Frame_fp)(void);

void Real_IN_Init (void);

/*
==========================================================================

DLL GLUE

==========================================================================
*/

#define	MAXPRINTMSG	4096
void EXPORT VID_Printf (int print_level, const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	
	va_start (argptr,fmt);
	vsnprintf (msg, sizeof(msg)-1, fmt,argptr);
	va_end (argptr);

	msg[sizeof(msg)-1] = 0;

	if (print_level == PRINT_ALL)
		Com_Printf ("%s", LOG_CLIENT, msg);
	else
		Com_DPrintf ("%s", msg);
}

void EXPORT VID_Error (int err_level, const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	
	va_start (argptr,fmt);
	vsnprintf (msg, sizeof(msg)-1, fmt,argptr);
	va_end (argptr);

	msg[sizeof(msg)-1] = 0;

	Com_Error (err_level,"%s", msg);
}

//==========================================================================

/*
============
VID_Restart_f

Console command to re-start the video mode and refresh DLL. We do this
simply by setting the modified flag for the vid_ref variable, which will
cause the entire video mode and refresh DLL to be reset on the next frame.
============
*/
void VID_Restart_f (void)
{
	vid_ref->modified = true;
	reload_video = true;
}

/*
** VID_GetModeInfo
*/
typedef struct vidmode_s
{
	const char *description;
	int         width, height;
	int         mode;
} vidmode_t;

vidmode_t vid_modes[] =
{
	{ "Mode 0: 320x240",   320, 240,   0 },
	{ "Mode 1: 400x300",   400, 300,   1 },
	{ "Mode 2: 512x384",   512, 384,   2 },
	{ "Mode 3: 640x480",   640, 480,   3 },
	{ "Mode 4: 800x600",   800, 600,   4 },
	{ "Mode 5: 960x720",   960, 720,   5 },
	{ "Mode 6: 1024x768",  1024, 768,  6 },
	{ "Mode 7: 1152x864",  1152, 864,  7 },
	{ "Mode 8: 1280x1024",  1280, 1024, 8 },
	{ "Mode 9: 1600x1200", 1600, 1200, 9 },
	{ "Mode 10: 1280x720", 1280, 720, 10 },
	{ "Mode 11: 1366x768", 1366, 768, 11 },
	{ "Mode 12: 1600x900", 1600, 900, 12 },
	{ "Mode 13: 1920x1080", 1920, 1080, 13 },
	{ "Mode 14: 2560x1440", 2560, 1440, 14 },
	{ "Mode 15: 3440x1440", 3440, 1440, 15 },
	{ "Mode 16: 3840x2160", 3840, 2160, 16 }
};

qboolean EXPORT VID_GetModeInfo(unsigned int *width, unsigned int *height, int mode)
{
	SDL_DisplayMode display_mode;
	int display_index;
	int display_count;
	int mode_w, mode_h;

	if (mode == -1)
	{
		*width = Cvar_IntValue ("vid_width");
		*height = Cvar_IntValue ("vid_height");
		if (*width < 800)
			*width = 800;
		if (*height < 600)
			*height = 600;
		if (*width > 16384)
			*width = 16384;
		if (*height > 16384)
			*height = 16384;
		return true;
	}

	if (mode == -2)
	{
		display_count = RW_VID_GetNumDisplays_fp ? RW_VID_GetNumDisplays_fp () : SDL_GetNumVideoDisplays ();
		display_index = vid_display ? vid_display->intvalue : 0;
		if (display_index < 0 || display_index >= display_count)
			display_index = 0;

		if (RW_VID_GetDisplayMode_fp)
		{
			if (RW_VID_GetDisplayMode_fp (display_index, &mode_w, &mode_h, NULL) == 0 && mode_w > 0 && mode_h > 0)
			{
				*width = (unsigned int)mode_w;
				*height = (unsigned int)mode_h;
				return true;
			}
		}
		else if (SDL_GetCurrentDisplayMode (display_index, &display_mode) == 0)
		{
			*width = display_mode.w;
			*height = display_mode.h;
			return true;
		}

		*width = 800;
		*height = 600;
		return true;
	}

	if ( mode < 4 )
		mode = 4;
	if ( mode >= VID_NUM_MODES )
		return false;

	*width  = vid_modes[mode].width;
	*height = vid_modes[mode].height;

	return true;
}

/*
** VID_NewWindow
*/
void EXPORT VID_NewWindow ( int width, int height)
{
	viddef.width  = width;
	viddef.height = height;

	cl.force_refdef = true;		// can't use a paused refdef
}

void VID_FreeReflib (void)
{
	if (reflib_library) {
		if (KBD_Close_fp)
			KBD_Close_fp();
		if (RW_IN_Shutdown_fp)
			RW_IN_Shutdown_fp();
#ifdef _WIN32
		FreeLibrary((HMODULE)reflib_library);
#else
		dlclose(reflib_library);
#endif
	}

	KBD_Init_fp = NULL;
	KBD_Update_fp = NULL;
	KBD_Close_fp = NULL;
	RW_VID_GetNumDisplays_fp = NULL;
	RW_VID_GetDisplayMode_fp = NULL;
	RW_IN_Init_fp = NULL;
	RW_IN_Shutdown_fp = NULL;
	RW_IN_Activate_fp = NULL;
	RW_IN_Commands_fp = NULL;
	RW_IN_Move_fp = NULL;
	RW_IN_Frame_fp = NULL;

	memset (&re, 0, sizeof(re));
	re_DrawCrosshair = NULL;
	reflib_library = NULL;
	reflib_active  = false;
}

/*
==============
VID_LoadRefresh
==============
*/
qboolean VID_LoadRefresh( char *name )
{
	refimport_t		ri;
	refimportnew_t  rx;

	GetRefAPI_t		GetRefAPI;
	GetExtraAPI_t	GetExtraAPI;

	char	fn[MAX_OSPATH];
	
	if ( reflib_active )
	{
		if (KBD_Close_fp)
			KBD_Close_fp();
		if (RW_IN_Shutdown_fp)
			RW_IN_Shutdown_fp();
		KBD_Close_fp = NULL;
		RW_IN_Shutdown_fp = NULL;
		re.Shutdown();
		VID_FreeReflib ();
	}

	Com_Printf( "------- Loading %s -------\n", LOG_CLIENT, name);


	/*if ((fp = fopen(SO_FILE, "r")) == NULL) {
		Com_Printf( "LoadLibrary(\"%s\") failed: can't open " SO_FILE " (required for location of ref libraries)\n", name);
		return false;
	}
	fgets(fn, sizeof(fn), fp);
	fclose(fp);
	if (*fn && fn[strlen(fn) - 1] == '\n')
		fn[strlen(fn) - 1] = 0;
	*/

#ifdef _WIN32
	Q_strncpy (fn, name, sizeof(fn)-1);
	if ( ( reflib_library = (void *)LoadLibrary( fn ) ) == 0 )
	{
		Com_Printf( "LoadLibrary(\"%s\") failed (%lu)\n", LOG_CLIENT, name, (unsigned long)GetLastError());
		return false;
	}
#else
	strcpy (fn, ".");
	strcat(fn, "/");
	strcat(fn, name);

	// permission checking
/*	if (strstr(fn, "softx") == NULL) { // softx doesn't require root
		if (stat(fn, &st) == -1) {
			Com_Printf( "LoadLibrary(\"%s\") failed: %s\n", name, strerror(errno));
			return false;
		}
		if (st.st_uid != 0) {
			Com_Printf( "LoadLibrary(\"%s\") failed: ref is not owned by root\n", name);
			return false;
		}
#if 0
		if ((st.st_mode & 0777) & ~0700) {
			Com_Printf( "LoadLibrary(\"%s\") failed: invalid permissions, must be 700 for security considerations\n", name);
			return false;
		}
#endif
	} else {
		// softx requires we give up root now
		setreuid(getuid(), getuid());
		setegid(getgid());
	}
*/

	if ( ( reflib_library = dlopen( fn, RTLD_NOW ) ) == 0 )
	{
		Com_Printf( "LoadLibrary(\"%s\") failed: %s\n", LOG_CLIENT, name , dlerror());
		return false;
	}
#endif

	ri.Cmd_AddCommand = Cmd_AddCommand;
	ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
	ri.Cmd_Argc = Cmd_Argc;
	ri.Cmd_Argv = Cmd_Argv;
	ri.Cmd_ExecuteText = Cbuf_ExecuteText;
	ri.Con_Printf = VID_Printf;
	ri.Sys_Error = VID_Error;
	ri.FS_LoadFile = FS_LoadFile;
	ri.FS_FreeFile = FS_FreeFile;
	ri.FS_Gamedir = FS_Gamedir;
	ri.Cvar_Get = Cvar_Get;
	ri.Cvar_Set = Cvar_Set;
	ri.Cvar_SetValue = Cvar_SetValue;
	ri.Vid_GetModeInfo = VID_GetModeInfo;
	ri.Vid_MenuInit = VID_MenuInit;
	ri.Vid_NewWindow = VID_NewWindow;

	//EXTENDED FUNCTIONS
	rx.FS_FOpenFile = FS_FOpenFile;
	rx.FS_FCloseFile = FS_FCloseFile;
	rx.FS_Read = FS_Read;
	
	rx.APIVersion = EXTENDED_API_VERSION;

	if ( ( GetRefAPI = (GetRefAPI_t) Sys_LoadSymbol( reflib_library, "GetRefAPI" ) ) == 0 )
		Com_Error( ERR_FATAL, "dlsym failed on %s", name );

	if ( ( GetExtraAPI = (GetExtraAPI_t) Sys_LoadSymbol( reflib_library, "GetExtraAPI" ) ) == 0 )
	{
		Com_DPrintf ("No ExtraAPI found.\n");
	} else {
		Com_DPrintf ("Initializing ExtraAPI...");
		GetExtraAPI (rx);
		Com_DPrintf ("done.\n");
	}

	re = GetRefAPI( ri );
	re_DrawCrosshair = (DrawCrosshair_t) Sys_LoadSymbol( reflib_library, "Draw_Crosshair" );
	RW_VID_GetNumDisplays_fp = (int (*)(void)) Sys_LoadSymbol( reflib_library, "RW_VID_GetNumDisplays" );
	RW_VID_GetDisplayMode_fp = (int (*)(int, int *, int *, const char **)) Sys_LoadSymbol( reflib_library, "RW_VID_GetDisplayMode" );

	if (re.api_version != API_VERSION)
	{
		VID_FreeReflib ();
		Com_Error (ERR_FATAL, "%s has incompatible api_version", name);
	}

	/* Init IN (Mouse) */
	in_state.IN_CenterView_fp = IN_CenterView;
	in_state.Key_Event_fp = Do_Key_Event;
	in_state.viewangles = cl.viewangles;
	in_state.in_strafe_state = &in_strafe.state;

	if ((RW_IN_Init_fp = Sys_LoadSymbol(reflib_library, "RW_IN_Init")) == NULL ||
		(RW_IN_Shutdown_fp = Sys_LoadSymbol(reflib_library, "RW_IN_Shutdown")) == NULL ||
		(RW_IN_Activate_fp = Sys_LoadSymbol(reflib_library, "RW_IN_Activate")) == NULL ||
		(RW_IN_Commands_fp = Sys_LoadSymbol(reflib_library, "RW_IN_Commands")) == NULL ||
		(RW_IN_Move_fp = Sys_LoadSymbol(reflib_library, "RW_IN_Move")) == NULL ||
		(RW_IN_Frame_fp = Sys_LoadSymbol(reflib_library, "RW_IN_Frame")) == NULL)
		Sys_Error("No RW_IN functions in REF.\n");

	Real_IN_Init();

	if ( re.Init( 0, 0 ) == -1 )
	{
		re.Shutdown();
		VID_FreeReflib ();
		return false;
	}

	/* Init KBD */
#if 1
	if ((KBD_Init_fp = Sys_LoadSymbol(reflib_library, "KBD_Init")) == NULL ||
		(KBD_Update_fp = Sys_LoadSymbol(reflib_library, "KBD_Update")) == NULL ||
		(KBD_Close_fp = Sys_LoadSymbol(reflib_library, "KBD_Close")) == NULL)
		Sys_Error("No KBD functions in REF.\n");
#else
	{
		void KBD_Init(void);
		void KBD_Update(void);
		void KBD_Close(void);

		KBD_Init_fp = KBD_Init;
		KBD_Update_fp = KBD_Update;
		KBD_Close_fp = KBD_Close;
	}
#endif
	KBD_Init_fp(Do_Key_Event);

#ifndef _WIN32
	// give up root now
	setreuid(getuid(), getuid());
	setegid(getgid());
#endif

	Com_Printf( "------------------------------------\n", LOG_CLIENT);
	reflib_active = true;
	return true;
}

/*
============
VID_CheckChanges

This function gets called once just before drawing each frame, and it's sole purpose in life
is to check to see if any of the video mode parameters have changed, and if they have to 
update the rendering DLL and/or video mode to match.
============
*/
void VID_ReloadRefresh (void)
{
	char name[100];

	if ( Q_stricmp( vid_ref->string, "gl" ) != 0 &&
		 Q_stricmp( vid_ref->string, "r1gl" ) != 0 &&
		 Q_stricmp( vid_ref->string, "r1q2gl" ) != 0 )
	{
		Com_Printf( "Only the OpenGL renderer is supported; using vid_ref gl.\n", LOG_CLIENT );
		Cvar_Set( "vid_ref", "gl" );
	}

	if ( vid_ref->modified )
	{
		S_StopAllSounds();
	}

	while (vid_ref->modified)
	{
		/*
		** refresh has changed
		*/
		vid_ref->modified = false;
		//vid_fullscreen->modified = true;
		cl.refresh_prepped = false;
		cl.frame.valid = false;
		cls.disable_screen = true;

#ifdef _WIN32
		strcpy( name, "ref_r1q2gl.dll" );
#else
		strcpy( name, "ref_r1q2gl.so" );
#endif
		if ( !VID_LoadRefresh( name ) )
			Com_Error (ERR_FATAL, "Couldn't load OpenGL renderer %s", name);

		cls.disable_screen = false;
	}

}

/*
============
VID_Init
============
*/
void VID_Init (void)
{
	/* The Linux build supports only the SDL OpenGL renderer. */
	vid_ref = Cvar_Get ("vid_ref", "gl", CVAR_ARCHIVE);
	vid_xpos = Cvar_Get ("vid_xpos", "3", CVAR_ARCHIVE);
	vid_ypos = Cvar_Get ("vid_ypos", "22", CVAR_ARCHIVE);
	vid_fullscreen = Cvar_Get ("vid_fullscreen", "0", CVAR_ARCHIVE);
	vid_display = Cvar_Get ("vid_display", "0", CVAR_ARCHIVE);
	vid_width = Cvar_Get ("vid_width", "800", CVAR_ARCHIVE);
	vid_height = Cvar_Get ("vid_height", "600", CVAR_ARCHIVE);
	vid_gamma = Cvar_Get( "vid_gamma", "1", CVAR_ARCHIVE );

	/* Add some console commands that we want to handle */
	Cmd_AddCommand ("vid_restart", VID_Restart_f);

	/* Disable the 3Dfx splash screen */
	putenv("FX_GLIDE_NO_SPLASH=0");
		
	/* Start the graphics mode and load refresh DLL */
	//VID_CheckChanges();
	VID_ReloadRefresh();
}

/*
============
VID_Shutdown
============
*/
void VID_Shutdown (void)
{
	if ( reflib_active )
	{
		if (KBD_Close_fp)
			KBD_Close_fp();
		if (RW_IN_Shutdown_fp)
			RW_IN_Shutdown_fp();
		KBD_Close_fp = NULL;
		RW_IN_Shutdown_fp = NULL;
		re.Shutdown ();
		VID_FreeReflib ();
	}
}


/*****************************************************************************/
/* INPUT                                                                     */
/*****************************************************************************/

cvar_t	*in_joystick;

// This if fake, it's acutally done by the Refresh load
void IN_Init (void)
{
#ifdef JOYSTICK
	in_joystick	= Cvar_Get ("in_joystick", "0", CVAR_ARCHIVE);
#endif
}

void Real_IN_Init (void)
{
	if (RW_IN_Init_fp)
		RW_IN_Init_fp(&in_state);
}

void IN_Shutdown (void)
{
	if (RW_IN_Shutdown_fp)
		RW_IN_Shutdown_fp();
}

void IN_Commands (void)
{
	if (RW_IN_Commands_fp)
		RW_IN_Commands_fp();
}

void IN_Move (usercmd_t *cmd)
{
	if (RW_IN_Move_fp)
		RW_IN_Move_fp(cmd);
}

void IN_Frame (void)
{
	if (RW_IN_Activate_fp)
	{
		if ( !cl.refresh_prepped || cls.key_dest == key_console || cls.key_dest == key_menu)
			RW_IN_Activate_fp(false);
		else
			RW_IN_Activate_fp(true);
	}

	if (RW_IN_Frame_fp)
		RW_IN_Frame_fp();
}

void IN_Activate (qboolean active)
{
	if (RW_IN_Activate_fp)
		RW_IN_Activate_fp(active);
}

void IN_Restart_f (void)
{
	IN_Shutdown ();
	Real_IN_Init ();
}

void Do_Key_Event(int key, qboolean down)
{
	Key_Event(key, down, Sys_Milliseconds());
}

