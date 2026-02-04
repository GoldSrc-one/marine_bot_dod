///////////////////////////////////////////////////////////////////////////////////////////////
//
//	-- GNU -- open source 
// Please read and agree to the mb_gnu_license.txt file
// (the file is located in the marine_bot source folder)
// before editing or distributing this source code.
// This source code is free for use under the rules of the GNU General Public License.
// For more information goto:: http://www.gnu.org/licenses/
//
// credits to - valve, botman.
//
// Marine Bot - code by Frank McNeil, Kota@, Mav, Shrike.
//
// (http://marinebot.xf.cz)
//
//
// h_export.cpp
// 
////////////////////////////////////////////////////////////////////////////////////////////////

#include "defines.h"

#pragma warning( disable: 4005 91 4996 )

#include "extdll.h"
#include "enginecallback.h"
#include "util.h"
#include "cbase.h"

#pragma warning( default: 4005 91 )

#include "bot.h"
#include "engine.h"
#include "bot_manager.h"
#include "console_output.h"

#ifndef __linux__

HINSTANCE h_Library = NULL;
HGLOBAL h_global_argv = NULL;
void FreeNameFuncGlobals(void);
void LoadSymbols(char *filename);

#else

void *h_Library = NULL;
char h_global_argv[1024];

#endif

enginefuncs_t g_engfuncs;
globalvars_t  *gpGlobals;
char *g_argv;

static FILE *fp, *fl;

// global mod name, it's used to build virtual path inside HL folder
char mod_dir_name[32];
int g_mod_version;
// global bool steam detected
// set to false by default if steam found then set to true
bool is_steam = FALSE;

GETENTITYAPI other_GetEntityAPI = NULL;
GETNEWDLLFUNCTIONS other_GetNewDLLFunctions = NULL;
GIVEFNPTRSTODLL other_GiveFnptrsToDll = NULL;


#ifndef __linux__

// Required DLL entry point
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
   if (fdwReason == DLL_PROCESS_ATTACH)
   {
   }
   else if (fdwReason == DLL_PROCESS_DETACH)
   {
	  FreeNameFuncGlobals();  // Free exported symbol table

      if (h_Library)
         FreeLibrary(h_Library);

      if (h_global_argv)
      {
         GlobalUnlock(h_global_argv);
         GlobalFree(h_global_argv);
      }
   }

   return TRUE;
}

#endif

#ifndef __linux__
#ifdef __BORLANDC__
extern "C" DLLEXPORT void EXPORT GiveFnptrsToDll(enginefuncs_t* pengfuncsFromEngine, globalvars_t *pGlobals)
#else
void DLLEXPORT GiveFnptrsToDll( enginefuncs_t* pengfuncsFromEngine, globalvars_t *pGlobals )
#endif
#else
extern "C" void DLLEXPORT GiveFnptrsToDll( enginefuncs_t* pengfuncsFromEngine, globalvars_t *pGlobals )
#endif
{
	int pos;
	char game_dir[256];						// mod root folder name
	char game_dll_filename[256];			// path to mod game_dll library
	char the_game[80];
	char temp_filename[256];
	char liblist_filename[256];				// holds path to liblist.gam file for current mod
	char the_entry[32];						// used to read the key entries in liblist.gam file
	char line_buffer[32];					// used to read the string values in liblist.gam file
	char mbfoldername[32];					// used to process MB custom folder name before we can store it
	bool read_mb_folder_name = false;		// do we need to read custom MB folder name from liblist.gam file
	char message[256];						// used to build console output

	// get the engine functions from the engine...
	
	memcpy(&g_engfuncs, pengfuncsFromEngine, sizeof(enginefuncs_t));
	gpGlobals = pGlobals;
	
	// find the directory name of the currently running MOD...
	(*g_engfuncs.pfnGetGameDir)(game_dir);

#ifdef _DEBUG
	extern int debug_engine;

	if (debug_engine)
	{
		fp=fopen("!mb_engine_debug.txt","a");
		fprintf(fp, "\n<h_export.cpp>Current virtual path returned by GetGameDir(): \"%s\"\n", game_dir);
		fclose(fp);
	}
#endif

	// STEAM detector is based on the fact that STEAM engine returns only mod directory name
	// (eg. "firearms") while original WON HL returns whole path to HL (eg. "C:\Half-Life/firearms")
	// therefore we have to scan the string backwards till first directory separator
	pos = strlen(game_dir) - 1;
	while ((pos) && (game_dir[pos] != '/'))
		pos--;

	// there was no directory separator which means that we are running STEAM
	if (pos == 0)
	{
		is_steam = TRUE;	// STEAM found
	}
	
	// code by *** Shrike ***
	if (is_steam)
	{
		pos = 0;
 
		if (strstr(game_dir, "/") != NULL)
		{
			// scan backwards till first directory separator...
			while ((pos) && (game_dir[pos] != '/'))
				pos--;

			if (pos == 0)
			{
				// Error getting directory name!
				conOutput.Print(NULL, "failed to determine MOD directory name (under STEAM)\n", MType::msg_error);
			}
			pos++;
		}
	}
	else
	{
		pos = strlen(game_dir) - 1;
		// scan backwards till first directory separator...
		while ((pos) && (game_dir[pos] != '/'))
			pos--;

		if (pos == 0)
		{
			// Error getting directory name!
			conOutput.Print(NULL, "failed to determine MOD directory name\n", MType::msg_error);
		}
		pos++;
	}
	
	// store the mod folder name
	strcpy(mod_dir_name, &game_dir[pos]);

#ifdef _DEBUG
	extern int debug_engine;

	if (debug_engine)
	{
		fp=fopen("!mb_engine_debug.txt","a");
		fprintf(fp, "\n<h_export.cpp>Mod directory name: \"%s\"\n", mod_dir_name);
		fclose(fp);
	}
#endif

	game_dll_filename[0] = 0;
	the_game[0] = 0;

	// we need to know which mod do we play so check mod directory name first
	if (strcmpi(mod_dir_name, "dod") == 0)
		strcpy(the_game, mod_dir_name);
	// mod directory hasn't default name so get "game" title from liblist.gam
	else
	{
		// find liblist.gam file located in mod root directory
		UTIL_BuildFileName(liblist_filename, mod_dir_name, "liblist.gam", false);

		fl = fopen(liblist_filename,"r");

		if (fl != NULL)
		{
			int c;

			the_entry[0] = 0;

			// scan strings one by one
			while (fscanf(fl, "%s", the_entry) != EOF)
			{
				// terminate it
				the_entry[sizeof(the_entry) - 1] = '\0';

				// when we find a comment then skip the rest of that line
				if (strstr(the_entry, "//") != NULL)
				{
					while (((c = getc(fl)) != '\n') && (c != EOF))
						;
				}

				// find game(mod) title
				if (strcmp(the_entry, "game") == 0)
				{
					line_buffer[0] = 0;

					// get the next string following the "game" entry
					// and test whether there was any string or not
					if (fscanf(fl, "%s", line_buffer) < 1)
					{
						conOutput.Print(NULL, "Missing value for \"game\" entry in \"liblist.gam\" file\n", MType::msg_error);
						break;
					}

					// terminate it
					line_buffer[sizeof(line_buffer) - 1] = '\0';

					// is there a phrase "Day of Defeat" in the string following the "game" entry?
					if (strstr(line_buffer, "Day of Defeat") != NULL)
					{
						strcpy(the_game, "dod");
						break;
					}
					else
					{
						conOutput.Print(NULL, "Failed to detect \"Day of Defeat\" string in \"game\" entry in \"liblist.gam\" file\n", MType::msg_error);
						break;
					}
				}
			}

			fclose(fl);
		}
		else
		{
			// Not sure if this can happen under normal circumstances, because
			// without liblist.gam file the bot library won't even load
			// so only as a fail-safe if there was a sudden IO issue right when we called for this file
			sprintf(message, "Failed to open: \"%s\"\n", temp_filename);
			conOutput.Print(NULL, message, MType::msg_critical_error);
		}
	}

	// we finally can check which mod do we play
	if (strcmpi(the_game, "dod") == 0)
	{
		g_mod_version = DOD_13;	// set the latest available version by default

		UTIL_BuildFileName(liblist_filename, mod_dir_name, "liblist.gam", false);

		fl = fopen(liblist_filename,"r");
		
		// code by *** Shrike ***
		if (fl != NULL)
		{
			int c;
			char compare[16];													// TODO:	Rework this code local variable to use my the_entry, like I did with line_buffer
			char ver_com[16];

			while (fscanf (fl, "%s", compare) != EOF)
			{
				compare[sizeof(compare) - 1] = '\0';

				if (strstr(compare, "//") != NULL)
				{
					while (((c = getc(fl)) != '\n') && (c != EOF))
						;
				}

				if (strcmp(compare, "version") == 0)	// find version
				{
					if (fscanf(fl, "%s", line_buffer) < 1)
					{
						conOutput.Print(NULL, "Missing value for \"version\" entry in \"liblist.gam\" file\n", MType::msg_error);
						break;
					}

					line_buffer[sizeof(line_buffer) - 1] = '\0';
					
					// we need only the chars between the double quotes
					UTIL_StringFromBuffer(ver_com, line_buffer, '"', '"');

					if (strncmp(ver_com, "1.3", 3) == 0)	// scan for 1.3
					{
						g_mod_version = DOD_13;
						break;
					}
					else
					{
						sprintf(message,"Failed to detect supported version number in \"liblist.gam\" file (found version value is: \"%s\")\n", ver_com);
						conOutput.Print(NULL, message, MType::msg_error);
					}
				}
			}

			fclose(fl);
		}
		else
		{
			sprintf(message, "Failed to open: \"%s\"\n", temp_filename);
			conOutput.Print(NULL, message, MType::msg_critical_error);
		}


		temp_filename[0] = 0;

		UTIL_BuildFileName(temp_filename, mod_dir_name, "dlls", false);

#ifndef __linux__
		UTIL_BuildFileName(game_dll_filename, temp_filename, "dod.dll", false);
#else
		UTIL_BuildFileName(game_dll_filename, temp_filename, "dod_i386.so", false);
#endif
	}
	
	// now we can try to load the mod library
	if (game_dll_filename[0])
	{
		sprintf(message, "loading MOD game library: %s\n", game_dll_filename);
		conOutput.Print(NULL, message, MType::msg_info);

#ifndef __linux__

		h_Library = LoadLibrary(game_dll_filename);

		if (h_Library == NULL)
		{
			conOutput.Print(NULL, "Default Day of Defeat game library failed to load\n", MType::msg_error);

			game_dll_filename[0] = 0;

			UTIL_BuildFileName(game_dll_filename, temp_filename, "dod.dll", false);
			
			sprintf(message, "loading Day of Defeat game library: %s\n", game_dll_filename);
			conOutput.Print(NULL, message, MType::msg_info);

			if (game_dll_filename[0])
				h_Library = LoadLibrary(game_dll_filename);
		}
#else
		h_Library = dlopen(game_dll_filename, RTLD_NOW);

		// if there is no classic dod Linux library then check for the renamed library
		// this is needed for cases when someone removed the _i386 suffix from library name to deal with Linux HLDS changes reflecting GNU/Linux naming convention
		if (h_Library == NULL)
		{
			conOutput.Print(NULL, "Default Day of Defeat game library failed to load\n", MType::msg_error);

			// reset it
			game_dll_filename[0] = 0;

			// build the filename using the other library name
			UTIL_BuildFileName(game_dll_filename, temp_filename, "dod.so", false);

			sprintf(message, "loading Day of Defeat game library: %s\n", game_dll_filename);
			conOutput.Print(NULL, message, MType::msg_info);

			// and if the filename is valid try to load it
			if (game_dll_filename[0])
				h_Library = dlopen(game_dll_filename, RTLD_NOW);
		}
#endif
	}

	if (h_Library == NULL)
	{
		// Directory error or Unsupported MOD!
		
		conOutput.Print(NULL, "MOD game library not found (or unsupported MOD)\n", MType::msg_critical_error);		
		return;
	}

	// now we need to check how is Marine Bot folder named
	UTIL_BuildFileName(temp_filename, mod_dir_name, "marine_bot", "marine.cfg");

	// MB doesn't use default folder name, because marine_bot\marine.cfg file cannot be accessed
	if (UTIL_IsFile(temp_filename) == FALSE)
	{
		conOutput.Print(NULL, "checking for custom Marine Bot folder name...\n", MType::msg_info);

		// let's check if just the underscore character has been omitted
		// (i.e. the easiest way to deal with Linux HLDS crashing issue due to this character in the filename/path)
		UTIL_BuildFileName(temp_filename, mod_dir_name, "marinebot", "marine.cfg");

		// if the file can be accessed then set new Marine Bot folder name
		if (UTIL_IsFile(temp_filename))
		{
			conOutput.Print(NULL, "Found custom folder name: \"marinebot\"\n", MType::msg_info);

			internals.SetMBFolderName("marinebot");
		}
		// non existing filename?
		else
		{
			// then the user must have used custom folder name
			// and we need to read it from liblist.gam file
			read_mb_folder_name = true;

			int c;

			// open liblist.gam file again
			fl = fopen(liblist_filename, "r");

			the_entry[0] = 0;
			line_buffer[0] = 0;

			while (fscanf(fl, "%s", the_entry) != EOF)
			{
				the_entry[sizeof(the_entry) - 1] = '\0';

				if (strcmp(the_entry, "//") == 0)
				{
					while (((c = getc(fl)) != '\n') && (c != EOF))
						;
				}

				// find gamedll entry string based on the OS we are running
#ifndef __linux__
				if (strcmp(the_entry, "gamedll") == 0)
#else
				if (strcmp(the_entry, "gamedll_linux") == 0)
#endif
				{
					if (fscanf(fl, "%s", line_buffer) < 1)
					{
						//TODO: Maybe add some error message here, although this case should never happen

						break;
					}

					line_buffer[sizeof(line_buffer) - 1] = '\0';

					// allows checking for validity
					mbfoldername[0] = 0;

					// read the custom folder name value based on the OS we are running
#ifndef __linux__
					UTIL_StringFromBuffer(mbfoldername, line_buffer, '"', '\\');
#else
					UTIL_StringFromBuffer(mbfoldername, line_buffer, '"', '/');
#endif

					// we need to find out if this is Marine Bot custom folder name
					if (mbfoldername[0])
					{
						// so we will build the path to MB configuration file using the found folder name ...
						UTIL_BuildFileName(temp_filename, mod_dir_name, mbfoldername, "marine.cfg");

						// and test whether gamedll entry points to Marine Bot folder or not
						// (ie. could be used to load Metamod)
						if (UTIL_IsFile(temp_filename))
						{
							// if so then we'll set the new Marine Bot folder name
							internals.SetMBFolderName(mbfoldername);

							// check whether the custom name doesn't exceed allowed size
							if (strcmp(internals.GetMBFolderName(), mbfoldername) < 0)
							{
								conOutput.Print(NULL, "custom folder name is too long! Shorten it\n", MType::msg_error);
							}
							else
							{
								sprintf(message, "Found custom folder name: \"%s\"\n", mbfoldername);
								conOutput.Print(NULL, message, MType::msg_info);
							}

							// we know the custom folder name now so
							// we must reset this flag to disable further searching
							read_mb_folder_name = false;
						}
					}

					// no point reading further we got all we needed
					break;
				}
			}

			// we still don't know Marine Bot folder name?
			if (read_mb_folder_name)
			{
				// return at the beginning of liblist.gam file ...
				rewind(fl);

				the_entry[0] = 0;
				line_buffer[0] = 0;

				while (fscanf(fl, "%s", the_entry) != EOF)
				{
					the_entry[sizeof(the_entry) - 1] = '\0';

					if (strcmp(the_entry, "//") == 0)
					{
						while (((c = getc(fl)) != '\n') && (c != EOF))
							;
					}

					// and try to find Marine Bot custom folder name entry string
					if (strcmp(the_entry, "marinebot_folder_name") == 0)
					{
						if (fscanf(fl, "%s", line_buffer) < 1)
						{
							conOutput.Print(NULL, "Missing value for \"marinebot_folder_name\" entry in \"liblist.gam\" file\n", MType::msg_error);
							break;
						}

						line_buffer[sizeof(line_buffer) - 1] = '\0';
						mbfoldername[0] = 0;

						UTIL_StringFromBuffer(mbfoldername, line_buffer, '"', '"');

						// if there was correct Marine Bot custom folder name entry then
						if (mbfoldername[0])
						{
							UTIL_BuildFileName(temp_filename, mod_dir_name, mbfoldername, "marine.cfg");

							if (UTIL_IsFile(temp_filename))
							{
								// set the new Marine Bot folder name
								internals.SetMBFolderName(mbfoldername);

								if (strcmp(internals.GetMBFolderName(), mbfoldername) < 0)
								{
									conOutput.Print(NULL, "custom folder name is too long! Shorten it\n", MType::msg_error);
								}
								else
								{
									sprintf(message, "Found custom folder name: \"%s\"\n", mbfoldername);
									conOutput.Print(NULL, message, MType::msg_info);
								}

								read_mb_folder_name = false;
							}
						}

						break;
					}
				}
			}

			fclose(fl);

			if (read_mb_folder_name)
			{
				conOutput.Print(NULL, "Failed to determine custom Marine Bot folder name\n", MType::msg_error);
			}
		}
	}

#ifndef __linux__
   h_global_argv = GlobalAlloc(GMEM_SHARE, 1024);
   g_argv = (char *)GlobalLock(h_global_argv);
#else
   g_argv = (char *)h_global_argv;
#endif

   other_GetEntityAPI = (GETENTITYAPI)GetProcAddress(h_Library, "GetEntityAPI");

   if (other_GetEntityAPI == NULL)
   {
      // Can't find GetEntityAPI!

		ALERT( at_error, "MarineBot - Can't get MOD's GetEntityAPI!" );

		/**/
#ifdef _DEBUG
		// debuging
		fp=fopen("!mb_engine_debug.txt","a");
		fprintf(fp,"Can't get MOD's GetEntityAPI!\n");
		fclose(fp);
#endif
		/**/
   }

   other_GetNewDLLFunctions = (GETNEWDLLFUNCTIONS)GetProcAddress(h_Library, "GetNewDLLFunctions");

   /**/
   // NOTE by Frank: This is always NULL in Firearms so I think we can ignore it
   if (other_GetNewDLLFunctions == NULL)
   {
	   // Can't find GetNewDLLFunctions!
	   
	   ALERT( at_error, "MarineBot - Can't get MOD's GetNewDLLFunctions!" );
	   
	   fp=fopen("!mb_engine_debug.txt","a");
	   fprintf(fp,"Can't get MOD's GetNewDLLFunctions!\n");
	   fclose(fp);
   }
   /**/

   other_GiveFnptrsToDll = (GIVEFNPTRSTODLL)GetProcAddress(h_Library, "GiveFnptrsToDll");

   if (other_GiveFnptrsToDll == NULL)
   {
      // Can't find GiveFnptrsToDll!

      ALERT( at_error, "MarineBot - Can't get MOD's GiveFnptrsToDll!" );

	  /**/
#ifdef _DEBUG
	  // debuging
	  fp=fopen("!mb_engine_debug.txt","a");
	  fprintf(fp,"Can't get MOD's GiveFnptrsToDll!\n");
	  fclose(fp);
#endif
	  /**/
   }


#ifndef __linux__
   LoadSymbols(game_dll_filename);  // Load exported symbol table
#endif

   pengfuncsFromEngine->pfnCmd_Args = Cmd_Args;
   pengfuncsFromEngine->pfnCmd_Argv = Cmd_Argv;
   pengfuncsFromEngine->pfnCmd_Argc = Cmd_Argc;

   pengfuncsFromEngine->pfnPrecacheModel = pfnPrecacheModel;
   pengfuncsFromEngine->pfnPrecacheSound = pfnPrecacheSound;
   pengfuncsFromEngine->pfnSetModel = pfnSetModel;
   pengfuncsFromEngine->pfnModelIndex = pfnModelIndex;
   pengfuncsFromEngine->pfnModelFrames = pfnModelFrames;
   pengfuncsFromEngine->pfnSetSize = pfnSetSize;
   pengfuncsFromEngine->pfnChangeLevel = pfnChangeLevel;
   pengfuncsFromEngine->pfnGetSpawnParms = pfnGetSpawnParms;
   pengfuncsFromEngine->pfnSaveSpawnParms = pfnSaveSpawnParms;
   pengfuncsFromEngine->pfnVecToYaw = pfnVecToYaw;
   pengfuncsFromEngine->pfnVecToAngles = pfnVecToAngles;
   pengfuncsFromEngine->pfnMoveToOrigin = pfnMoveToOrigin;
   pengfuncsFromEngine->pfnChangeYaw = pfnChangeYaw;
   pengfuncsFromEngine->pfnChangePitch = pfnChangePitch;
   pengfuncsFromEngine->pfnFindEntityByString = pfnFindEntityByString;
   pengfuncsFromEngine->pfnGetEntityIllum = pfnGetEntityIllum;
   pengfuncsFromEngine->pfnFindEntityInSphere = pfnFindEntityInSphere;
   pengfuncsFromEngine->pfnFindClientInPVS = pfnFindClientInPVS;
   pengfuncsFromEngine->pfnEntitiesInPVS = pfnEntitiesInPVS;
   pengfuncsFromEngine->pfnMakeVectors = pfnMakeVectors;
   pengfuncsFromEngine->pfnAngleVectors = pfnAngleVectors;
   pengfuncsFromEngine->pfnCreateEntity = pfnCreateEntity;
   pengfuncsFromEngine->pfnRemoveEntity = pfnRemoveEntity;
   pengfuncsFromEngine->pfnCreateNamedEntity = pfnCreateNamedEntity;
   pengfuncsFromEngine->pfnMakeStatic = pfnMakeStatic;
   pengfuncsFromEngine->pfnEntIsOnFloor = pfnEntIsOnFloor;
   pengfuncsFromEngine->pfnDropToFloor = pfnDropToFloor;
   pengfuncsFromEngine->pfnWalkMove = pfnWalkMove;
   pengfuncsFromEngine->pfnSetOrigin = pfnSetOrigin;
   pengfuncsFromEngine->pfnEmitSound = pfnEmitSound;
   pengfuncsFromEngine->pfnEmitAmbientSound = pfnEmitAmbientSound;
   pengfuncsFromEngine->pfnTraceLine = pfnTraceLine;
   pengfuncsFromEngine->pfnTraceToss = pfnTraceToss;
   pengfuncsFromEngine->pfnTraceMonsterHull = pfnTraceMonsterHull;
   pengfuncsFromEngine->pfnTraceHull = pfnTraceHull;
   pengfuncsFromEngine->pfnTraceModel = pfnTraceModel;
   pengfuncsFromEngine->pfnTraceTexture = pfnTraceTexture;
   pengfuncsFromEngine->pfnTraceSphere = pfnTraceSphere;
   pengfuncsFromEngine->pfnGetAimVector = pfnGetAimVector;
   pengfuncsFromEngine->pfnServerCommand = pfnServerCommand;
   pengfuncsFromEngine->pfnServerExecute = pfnServerExecute;
   pengfuncsFromEngine->pfnClientCommand = pfnClientCommand;
   pengfuncsFromEngine->pfnParticleEffect = pfnParticleEffect;
   pengfuncsFromEngine->pfnLightStyle = pfnLightStyle;
   pengfuncsFromEngine->pfnDecalIndex = pfnDecalIndex;
   pengfuncsFromEngine->pfnPointContents = pfnPointContents;
   pengfuncsFromEngine->pfnMessageBegin = pfnMessageBegin;
   pengfuncsFromEngine->pfnMessageEnd = pfnMessageEnd;
   pengfuncsFromEngine->pfnWriteByte = pfnWriteByte;
   pengfuncsFromEngine->pfnWriteChar = pfnWriteChar;
   pengfuncsFromEngine->pfnWriteShort = pfnWriteShort;
   pengfuncsFromEngine->pfnWriteLong = pfnWriteLong;
   pengfuncsFromEngine->pfnWriteAngle = pfnWriteAngle;
   pengfuncsFromEngine->pfnWriteCoord = pfnWriteCoord;
   pengfuncsFromEngine->pfnWriteString = pfnWriteString;
   pengfuncsFromEngine->pfnWriteEntity = pfnWriteEntity;
   pengfuncsFromEngine->pfnCVarRegister = pfnCVarRegister;
   pengfuncsFromEngine->pfnCVarGetFloat = pfnCVarGetFloat;
   pengfuncsFromEngine->pfnCVarGetString = pfnCVarGetString;
   pengfuncsFromEngine->pfnCVarSetFloat = pfnCVarSetFloat;
   pengfuncsFromEngine->pfnCVarSetString = pfnCVarSetString;
   // if you're getting errors here then go to 'defines.h' and set the correct flag
   // matching your HL SDK version
   pengfuncsFromEngine->pfnPvAllocEntPrivateData = pfnPvAllocEntPrivateData;
   pengfuncsFromEngine->pfnPvEntPrivateData = pfnPvEntPrivateData;
   pengfuncsFromEngine->pfnFreeEntPrivateData = pfnFreeEntPrivateData;
   pengfuncsFromEngine->pfnSzFromIndex = pfnSzFromIndex;
   pengfuncsFromEngine->pfnAllocString = pfnAllocString;
   pengfuncsFromEngine->pfnGetVarsOfEnt = pfnGetVarsOfEnt;
   pengfuncsFromEngine->pfnPEntityOfEntOffset = pfnPEntityOfEntOffset;
   pengfuncsFromEngine->pfnEntOffsetOfPEntity = pfnEntOffsetOfPEntity;
   pengfuncsFromEngine->pfnIndexOfEdict = pfnIndexOfEdict;
   pengfuncsFromEngine->pfnPEntityOfEntIndex = pfnPEntityOfEntIndex;
   pengfuncsFromEngine->pfnFindEntityByVars = pfnFindEntityByVars;
   pengfuncsFromEngine->pfnGetModelPtr = pfnGetModelPtr;
   pengfuncsFromEngine->pfnRegUserMsg = pfnRegUserMsg;
   pengfuncsFromEngine->pfnAnimationAutomove = pfnAnimationAutomove;
   pengfuncsFromEngine->pfnGetBonePosition = pfnGetBonePosition;
   pengfuncsFromEngine->pfnFunctionFromName = pfnFunctionFromName;
   pengfuncsFromEngine->pfnNameForFunction = pfnNameForFunction;
   pengfuncsFromEngine->pfnClientPrintf = pfnClientPrintf;
   pengfuncsFromEngine->pfnServerPrint = pfnServerPrint;
   pengfuncsFromEngine->pfnGetAttachment = pfnGetAttachment;
   pengfuncsFromEngine->pfnCRC32_Init = pfnCRC32_Init;
   pengfuncsFromEngine->pfnCRC32_ProcessBuffer = pfnCRC32_ProcessBuffer;
   pengfuncsFromEngine->pfnCRC32_ProcessByte = pfnCRC32_ProcessByte;
   pengfuncsFromEngine->pfnCRC32_Final = pfnCRC32_Final;
   pengfuncsFromEngine->pfnRandomLong = pfnRandomLong;
   pengfuncsFromEngine->pfnRandomFloat = pfnRandomFloat;
   pengfuncsFromEngine->pfnSetView = pfnSetView;
   pengfuncsFromEngine->pfnTime = pfnTime;
   pengfuncsFromEngine->pfnCrosshairAngle = pfnCrosshairAngle;
   pengfuncsFromEngine->pfnLoadFileForMe = pfnLoadFileForMe;
   pengfuncsFromEngine->pfnFreeFile = pfnFreeFile;
   pengfuncsFromEngine->pfnEndSection = pfnEndSection;
   pengfuncsFromEngine->pfnCompareFileTime = pfnCompareFileTime;
   pengfuncsFromEngine->pfnGetGameDir = pfnGetGameDir;
   pengfuncsFromEngine->pfnCvar_RegisterVariable = pfnCvar_RegisterVariable;
   pengfuncsFromEngine->pfnFadeClientVolume = pfnFadeClientVolume;
   pengfuncsFromEngine->pfnSetClientMaxspeed = pfnSetClientMaxspeed;
   pengfuncsFromEngine->pfnCreateFakeClient = pfnCreateFakeClient;
   pengfuncsFromEngine->pfnRunPlayerMove = pfnRunPlayerMove;
   pengfuncsFromEngine->pfnNumberOfEntities = pfnNumberOfEntities;
   pengfuncsFromEngine->pfnGetInfoKeyBuffer = pfnGetInfoKeyBuffer;
   pengfuncsFromEngine->pfnInfoKeyValue = pfnInfoKeyValue;
   pengfuncsFromEngine->pfnSetKeyValue = pfnSetKeyValue;
   pengfuncsFromEngine->pfnSetClientKeyValue = pfnSetClientKeyValue;
   pengfuncsFromEngine->pfnIsMapValid = pfnIsMapValid;
   pengfuncsFromEngine->pfnStaticDecal = pfnStaticDecal;
   pengfuncsFromEngine->pfnPrecacheGeneric = pfnPrecacheGeneric;
   pengfuncsFromEngine->pfnGetPlayerUserId = pfnGetPlayerUserId;
   pengfuncsFromEngine->pfnGetPlayerAuthId = pfnGetPlayerAuthId;
   pengfuncsFromEngine->pfnBuildSoundMsg = pfnBuildSoundMsg;
   pengfuncsFromEngine->pfnIsDedicatedServer = pfnIsDedicatedServer;
   pengfuncsFromEngine->pfnCVarGetPointer = pfnCVarGetPointer;
   pengfuncsFromEngine->pfnGetPlayerWONId = pfnGetPlayerWONId;

   // SDK 2.0 additions...
   pengfuncsFromEngine->pfnInfo_RemoveKey = pfnInfo_RemoveKey;
   pengfuncsFromEngine->pfnGetPhysicsKeyValue = pfnGetPhysicsKeyValue;
   pengfuncsFromEngine->pfnSetPhysicsKeyValue = pfnSetPhysicsKeyValue;
   pengfuncsFromEngine->pfnGetPhysicsInfoString = pfnGetPhysicsInfoString;
   pengfuncsFromEngine->pfnPrecacheEvent = pfnPrecacheEvent;
   pengfuncsFromEngine->pfnPlaybackEvent = pfnPlaybackEvent;
   pengfuncsFromEngine->pfnSetFatPVS = pfnSetFatPVS;
   pengfuncsFromEngine->pfnSetFatPAS = pfnSetFatPAS;
   pengfuncsFromEngine->pfnCheckVisibility = pfnCheckVisibility;
   pengfuncsFromEngine->pfnDeltaSetField = pfnDeltaSetField;
   pengfuncsFromEngine->pfnDeltaUnsetField = pfnDeltaUnsetField;
   pengfuncsFromEngine->pfnDeltaAddEncoder = pfnDeltaAddEncoder;
   pengfuncsFromEngine->pfnGetCurrentPlayer = pfnGetCurrentPlayer;
   pengfuncsFromEngine->pfnCanSkipPlayer = pfnCanSkipPlayer;
   pengfuncsFromEngine->pfnDeltaFindField = pfnDeltaFindField;
   pengfuncsFromEngine->pfnDeltaSetFieldByIndex = pfnDeltaSetFieldByIndex;
   pengfuncsFromEngine->pfnDeltaUnsetFieldByIndex = pfnDeltaUnsetFieldByIndex;
   pengfuncsFromEngine->pfnSetGroupMask = pfnSetGroupMask;
   pengfuncsFromEngine->pfnCreateInstancedBaseline = pfnCreateInstancedBaseline;
   pengfuncsFromEngine->pfnCvar_DirectSet = pfnCvar_DirectSet;
   pengfuncsFromEngine->pfnForceUnmodified = pfnForceUnmodified;
   pengfuncsFromEngine->pfnGetPlayerStats = pfnGetPlayerStats;
   pengfuncsFromEngine->pfnAddServerCommand = pfnAddServerCommand;

   // give the engine functions to the other DLL...
   (*other_GiveFnptrsToDll)(pengfuncsFromEngine, pGlobals);
}

#ifdef __BORLANDC__
int EXPORT Server_GetBlendingInterface(int version, struct sv_blending_interface_s** ppinterface, struct engine_studio_api_s* pstudio, float(*rotationmatrix)[3][4], float(*bonetransform)[MAXSTUDIOBONES][3][4])
#else
extern "C" EXPORT int Server_GetBlendingInterface(int version, struct sv_blending_interface_s** ppinterface, struct engine_studio_api_s* pstudio, float(*rotationmatrix)[3][4], float(*bonetransform)[MAXSTUDIOBONES][3][4])
#endif
{
	static SERVER_GETBLENDINGINTERFACE other_Server_GetBlendingInterface = NULL;
	static bool missing = FALSE;

	// if the blending interface has been formerly reported as missing, give up
	if (missing)
		return (FALSE);

	// do we NOT know if the blending interface is provided ? if so, look for its address
	if (other_Server_GetBlendingInterface == NULL)
		other_Server_GetBlendingInterface = (SERVER_GETBLENDINGINTERFACE)GetProcAddress(h_Library, "Server_GetBlendingInterface");

	// have we NOT found it ?
	if (!other_Server_GetBlendingInterface)
	{
		missing = TRUE; // then mark it as missing, no use to look for it again in the future
		return (FALSE); // and give up
	}

	// else call the function that provides the blending interface on request
	return ((other_Server_GetBlendingInterface)(version, ppinterface, pstudio, rotationmatrix, bonetransform));
}