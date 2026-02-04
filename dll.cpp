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
// dll.cpp
// 
////////////////////////////////////////////////////////////////////////////////////////////////

#include "defines.h"

#pragma warning( push )
#pragma warning( disable: 4005 91 4477 26495 26451 )

#include "extdll.h"
#include "enginecallback.h"
#include "util.h"
#include "cbase.h"
#include "entity_state.h"

#pragma warning( pop )

#include "bot.h"
#include "bot_config.h"
#include "bot_func.h"
#include "bot_manager.h"
#include "client_commands.h"
#include "console_output.h"
#include "waypoint.h"


extern "C"
{
#include <stdio.h>
};


// stuff for MS Visual Studio vs. Linux GCC builds
// although max() isn't used
#undef min
#define min(a,b)  (((a) < (b)) ? (a) : (b))

#undef max
#define max(a,b)  (((a) > (b)) ? (a) : (b))


extern GETENTITYAPI other_GetEntityAPI;
extern GETNEWDLLFUNCTIONS other_GetNewDLLFunctions;
extern enginefuncs_t g_engfuncs;
extern int debug_engine;
extern globalvars_t  *gpGlobals;
extern char *g_argv;


extern char wpt_author[32];
extern char wpt_modified[32];


bot_weapon_select_t bot_weapon_select[MAX_WEAPONS];			// array of all weapons the bot can use
bot_fire_delay_t bot_fire_delay[MAX_WEAPONS];				// array of all weapon fire delays (delay between two shots)
bot_target_offset_t bot_target_offset[BOT_SKILL_LEVELS];	// array of body target offsets used when aiming at the enemy

static FILE *fp;

DLL_FUNCTIONS other_gFunctionTable;
DLL_GLOBAL const Vector g_vecZero = Vector(0,0,0);

externals_t externals;
internals_t internals;
botmanager_t botmanager;
botdebugger_t botdebugger;
config_t configFile;
unified_error_messages_system_t errormsgs;		// the fancy HUD welcome messages are part of it too
console_input_t conInput;
console_output_t conOutput;
utils_t util;
development_tools_t devTool;


modTeams_t teamONE;
modTeams_t teamTWO;

int client_t::humans_num;
int client_t::bots_num;
client_t clients[MAX_CLIENTS];

control_point_t ControlPoints[MAX_CAPTUREPOINTS];
dod_control_point_through_capture_area_t dodCaptureArea[MAX_CAPTUREPOINTS];

int m_spriteTexture = 0;
int m_spriteTexturePath1 = 0;
int m_spriteTexturePath2 = 0;
int m_spriteTexturePath3 = 0;

bool is_dedicated_server = TRUE;	// we set that it is a dedicated server by default, because we need it for h_export.cpp stuff, it's set to correct value later on in GameDLLInit

edict_t* pent_info_doddetect = NULL;
edict_t *listenserver_edict = NULL;
edict_t *pRecipient = NULL;		// the one who wrote the ClientCommand

char mb_version_info[32] = "0.97b[DOD]";		// holds MarineBot version string


// Marine Bot doesn't use these yet and most probably will never spam the game with these
char bot_whine[MAX_BOT_WHINE][81];
int whine_count;
int recent_bot_whine[5];

// following variables are used only in this file
edict_t* pent_dispatch_detect = NULL;	// allows reading data from game entities during map load, assigned in DispatchKeyValue and cleared in DispatchSpawn
char captureareas_filename[256]{};
int capturearea_counter = 0;

bool Dedicated_Server_Init = false;		// ensures only one run of DS init
bool g_GameRules = false;
int isFakeClientCommand = 0;
int fake_arg_count;
bool read_whole_cfg = true;				// allows to read the whole configuration file after map change
bool using_default_cfg = true;			// is FALSE when we changed to some map specific .cfg
bool need_to_open_cfg = true;
float bot_cfg_pause_time = 0.0f;
float respawn_time = 0.0f;
bool spawn_time_reset = false;
int num_bots = 0;
int prev_num_bots = 0;
bool override_max_bots = false;
const float wpt_autosave_delay = 90.0f;	// constant delay between two attempts to automatic waypoints save
float wpt_autosave_time = 0.0f;			// holds time of the next attempt to automatic waypoints save

float check_send_info = 0.0f;			// send message checks
const char presentation_const[] = "This server runs Marine Bot in version";
char presentation_msg[96]{};			// make sure the size can handle both strings the presentation constant and mb version info
float presentation_time = 0.0f;			// holds the time of last presentation

// the welcome messages are using the unified error message system now
const char welcome_msg[] = "reporting for duty!\nWrite \"help\" or \"?\" into console to show console help";
const char welcome2_msg[] = "Visit MarineBot web page at:\nhttp://www.marinebot.xf.cz";


// few function prototypes used in this file...

bool AlternativePlayerStartReposition(edict_t& pent, int team, int playerstart_position, const char* startpoints_filename);
bool ModifiedCaptureAreas(char* entry_value, const char* entry_name, int capturearea_id);
bool BuildCaptureAreasFile(int capturearea_id, edict_t* pent);
void GameDLLInit(void);
void UpdateClientData(const struct edict_s *ent, int sendweapons, struct clientdata_s *cd);
void ProcessBotCfgFile(bool only_count_custom_classes = false);
void MBServerCommands(void);		// Dedicated server console commands


/*
* modifies player start position based on data read from external configuration file
*/
bool AlternativePlayerStartReposition(edict_t& pent, int team, int playerstart_position, const char* startpoints_filename)
{
	char point_id[7]{};			// allows up to 99 positions
	char point_id_check[7]{};
	bool read_x, read_y, read_z;
	Vector alt_point_origin;

	if (configFile.OpenConfigFile(startpoints_filename) == false)
		return false;

	configFile.ResetConfigHistory();
	read_x = read_y = read_z = false;

	// see which team start point we need to read from the file
	if (team == teamONE.GetTeamId())
	{
		// and set appropriate scope for it
		sprintf(point_id, "TONE%d", playerstart_position);
		sprintf(point_id_check, "TONE%d", playerstart_position + 1);
	}
	else
	{
		sprintf(point_id, "TTWO%d", playerstart_position);
		sprintf(point_id_check, "TTWO%d", playerstart_position + 1);
	}

	configFile.SetScope(point_id_check);

	// does such point exist in the file?
	if (configFile.FindKeyScope(point_id))
	{
		// then try to read all 3 values for its alternative origin
		alt_point_origin.x = configFile.ReadFloatValue(9999.0f);

		if (configFile.IsReadError() == false)
		{
			read_x = true;

			alt_point_origin.y = configFile.ReadFloatValue(9999.0f);

			if (configFile.IsReadError() == false)
			{
				read_y = true;

				alt_point_origin.z = configFile.ReadFloatValue(9999.0f);

				if (configFile.IsReadError() == false)
					read_z = true;
			}
		}

		// was there any issue with reading the values AND is there a specific error message?
		if (configFile.IsReadError() && configFile.IsErrorMessage())
		{
			// then let the user know what is missing in the config file
			conOutput.Print(NULL, configFile.GetErrorMessage(), MType::msg_error);
			// we don't use the fancy HUD error messages here so we should log this error to MB public errorlog
			util.DebugInFile(configFile.GetErrorMessage());
		}
	}

	configFile.CloseConfigFile();

#ifdef DEBUG
	char ermsg[128]{};
	sprintf(ermsg, "ExternalCFGFile - reading alternative start point #%d origin is: %.1f %.1f %.1f\n", playerstart_position, alt_point_origin.x, alt_point_origin.y, alt_point_origin.z);
	//util.DebugInFile(ermsg);
#endif // DEBUG


	// did we read all 3 values successfully?
	if (read_z)
	{
		// then we can finally change the position for this start point
		pent.v.origin = alt_point_origin;

		return true;
	}

	return false;
}


/*
* reads modified data for the capture area from external configuration file
* skips any reading if user disallowed modifying the capture areas
*/
bool ModifiedCaptureAreas(char* entry_value, const char* entry_name, int capturearea_id)
{
	// do we allow using the alternative data for capture areas?
	if (internals.IsChangeCaptureAreas())
	{
		char caparea_id[6]{};
		char caparea_id_check[6]{};
		char the_entry[32]{};
		bool read_the_value = false;

		if (configFile.OpenConfigFile(captureareas_filename) == false)
			return false;

		configFile.ResetConfigHistory();

		sprintf(caparea_id, "AREA%d", capturearea_id);
		sprintf(caparea_id_check, "AREA%d", capturearea_id + 1);

		configFile.SetScope(caparea_id_check);

		if (configFile.FindKeyScope(caparea_id))
		{
			// keep reading the entries for this capture area
			while (configFile.ReadEntryScope(the_entry))
			{
				// did we find the entry name we were looking for?
				if (strcmp(the_entry, entry_name) == 0)
				{
					// did we read the value for this entry name?
					if (configFile.ReadValueScope(entry_value))
					{
						read_the_value = true;
						break;
					}
				}
			}
		}

		if (configFile.IsReadError() && configFile.IsErrorMessage())
		{
			conOutput.Print(NULL, configFile.GetErrorMessage(), MType::msg_error);
			util.DebugInFile(configFile.GetErrorMessage());
		}

		configFile.CloseConfigFile();

		if (read_the_value)
			return true;
	}

	return false;
}


/*
* creates an external file with capture areas data
*/
bool BuildCaptureAreasFile(int capturearea_id, edict_t* pent)
{
	FILE* fc = NULL;
	
	if (capturearea_id == 0)
		fc = fopen(captureareas_filename, "w");
	else
		fc = fopen(captureareas_filename, "a");

	if (fc)
	{
		if (capturearea_id == 0)
		{
			fprintf(fc, "#\n# Marine Bot capture areas configuration file\n#\n");
			fprintf(fc, "# The data in this file can be used to modify the capture areas values.\n");
			fprintf(fc, "# The code phrase on left side is critical variable. DON'T MESS WITH IT!\n");
			fprintf(fc, "# The values in double quotes are allowed to be modified.\n# Keep the rest entries as they are.\n#\n");
			fprintf(fc, "# The name is NOT being processed. It's here to give you an idea what area it is.\n#\n");
		}

		fprintf(fc, "AREA%d\n", capturearea_id);
		fprintf(fc, "	axis_to_capture \"%d\"\n", dodCaptureArea->GetTeamTwoPlayersToCapture(dodCaptureArea->FindPointInArray(pent)));
		fprintf(fc, "	allies_to_capture \"%d\"\n", dodCaptureArea->GetTeamOnePlayersToCapture(dodCaptureArea->FindPointInArray(pent)));
		fprintf(fc, "	name \"%s\"\n", dodCaptureArea->GetPointName(dodCaptureArea->FindPointInArray(pent)));

		fclose(fc);

		return true;
	}

	return false;
}


void GameDLLInit(void)
{
	int i;
	char filename[128]{};
	char msg[128]{};

	(*g_engfuncs.pfnAddServerCommand) ("m_bot", MBServerCommands);
	
	// the new Windows console (terminal) doesn't allow SHIFT so this is second MB CVAR without the underscore character
	(*g_engfuncs.pfnAddServerCommand) ("mbot", MBServerCommands);

	// is dedicated server
	if (IS_DEDICATED_SERVER())
		is_dedicated_server = true;
	else
		is_dedicated_server = false;

	// make the version string match particular build
#ifdef _DEBUG
	strcat(mb_version_info, "_Dev");
#endif

	// define the teams this mod uses
	teamONE.SetTeamId(1);								// this value matches the position on Select Team menu ... it's used to check entity->v.team 
	teamONE.SetTeamName("allies");						// the name of the team ... it's used instead of the team ID in some commands (eg. recruit) also in various console outputs
	teamONE.SetTeamName2wordsLC("allied team");			// two word version where it is needed, all lowercase ... it's used in balancing and waypoints info console outputs
	teamONE.SetTeamName2wordsFUC("Allied Team");		// two word version where it is needed, with first letters in capitals ... eg. waypoint info command
	teamONE.SetTeamNameAltF1word("AlliedTeam ");		// one word version with alternative formatting (keep the space at the end), it's used in the HUD message for path info
	teamONE.SetTeamNameForGoalAltF1word(" AlliedGoal ");// one word version for team goal with alternative formatting (keep the spaces) ... it's used in console 'misc' output for print all paths command
	// following two may not always be used in the game mod, if so then assign some nonsence like "notavailable", but don't leave it as empty string
	teamONE.SetTeamNameForPlayerModel("unused1");		// name of the player model ... it's used to check entity->v.model if v.team check failed
	teamONE.SetTeamNameNetname("Allied Force");			// netname string ... it's used to check entity->v.netname to determine the team if v.team check failed
	teamONE.SetTeamPathColor(0,0,255);					// the color code of waypoint path for this team, can be set to any color, but there already are some colors
														// for special purpose paths so for two teams there is used red and blue color
	teamTWO.SetTeamId(2);
	teamTWO.SetTeamName("axis");
	teamTWO.SetTeamName2wordsLC("axis team");
	teamTWO.SetTeamName2wordsFUC("Axis Team");
	teamTWO.SetTeamNameAltF1word("AxisTeam ");
	teamTWO.SetTeamNameForGoalAltF1word(" AxisGoal ");
	teamTWO.SetTeamNameForPlayerModel("unused2");		// dunno if this is ever used in DoD so let's set there something
	teamTWO.SetTeamNameNetname("Axis Force");			// dunno if this is ever used in DoD so let's set there something
	teamTWO.SetTeamPathColor(255,0,0);


	util.InitPrivateVars();
	
	// whines aren't used at all ... probably useless and will be removed
	whine_count = 0;
	for (i=0; i < 5; i++)
		recent_bot_whine[i] = -1;

	// initialize the arrays with bot names
	BotNameArraysInit();

	util.MarineBotFileName(filename, "marine_dog-tags.txt", NULL);

	sprintf(msg, "loading bot names: %s\n", filename);
	conOutput.Print(NULL, msg, MType::msg_info);

	// open the file with bot names and read the data from it
	if (BotNamesInit(filename) == false)
	{
		// let the user know if things have failed so..
		
		// first set the error code,
		// this will also ensure that this error message will display through the fancy HUD text once the client joins the game
		errormsgs.AddErrorCode(UEMS_WARN_BNAME);

		// then build associated error and warning messages
		errormsgs.PrepareErrorAndWarning();

		// and print the messages into the console
		conOutput.Print(NULL, errormsgs.GetError(), MType::msg_error);
		conOutput.Print(NULL, errormsgs.GetWarning(), MType::msg_warning);
	}

	// we need to initialize this mod weapon IDs
	if (InitWeaponsForThisMod() == false)
	{
		errormsgs.AddErrorCode(UEMS_ER_WPNLNK);

		errormsgs.PrepareErrorAndWarning();
		conOutput.Print(NULL, errormsgs.GetError(), MType::msg_error);
		conOutput.Print(NULL, errormsgs.GetWarning(), MType::msg_warning);
	}

	// initialize the weapon arrays
	memset(bot_weapon_select, 0, sizeof(bot_weapon_select));
	memset(bot_fire_delay, 0, sizeof(bot_fire_delay));
	// and also the body target offsets array
	memset(bot_target_offset, 0, sizeof(bot_target_offset));

	//kota@ we should read weapon configuration from the file.
	filename[0] = 0;
	char FA_version_string[16]{};

	// we need to use string version here
	if (g_mod_version == DOD_13)
		strcpy(FA_version_string,"1_3");

	util.MarineBotFileName(filename, "weapons", FA_version_string);

	sprintf(msg, "loading weapon definitions: %s\n", filename);
	conOutput.Print(NULL, msg, MType::msg_info);

	// open weapon definitions file and read the data from it
	if (BotWeaponArraysInit(filename) == false)
	{
		errormsgs.AddErrorCode(UEMS_ER_WPNDEF);

		errormsgs.PrepareErrorAndWarning();
		conOutput.Print(NULL, errormsgs.GetError(), MType::msg_error);
		conOutput.Print(NULL, errormsgs.GetWarning(), MType::msg_warning);
	}
	
	util.MarineBotFileName(filename, "weapons", "targetbodyoffsets");

	sprintf(msg, "loading target body offsets: %s\n", filename);
	conOutput.Print(NULL, msg, MType::msg_info);

	// open target body offsets file and read the data from it
	if (BotTargetOffsetsArrayInit(filename) == false)
	{
		errormsgs.AddErrorCode(UEMS_WARN_TARGOFS);

		errormsgs.PrepareErrorAndWarning();
		conOutput.Print(NULL, errormsgs.GetError(), MType::msg_error);
		conOutput.Print(NULL, errormsgs.GetWarning(), MType::msg_warning);
	}

	// initialize the bots array
	bots = new bot_t[MAX_CLIENTS];

	(*other_gFunctionTable.pfnGameInit)();
}

int DispatchSpawn( edict_t *pent )
{
	if (gpGlobals->deathmatch)
	{
		char *pClassname = (char *)STRING(pent->v.classname);
		static char startpoints_filename[256]{};
		static int teamone_playerstart_position_counter = 0;
		static int teamtwo_playerstart_position_counter = 0;

#ifdef _DEBUG
		if (debug_engine)
		{
			fp=fopen(debug_fname,"a");
			fprintf(fp, "DispatchSpawn: %p %s\n", pent, pClassname);

			if (pent->v.model != 0)
				fprintf(fp, " model=%s\n",STRING(pent->v.model));

			fclose(fp);
		}
#endif

		// this is the first method that's being called on map change so do level initialization stuff here
		if (strcmp(pClassname, "worldspawn") == 0)
		{
			/*/
#ifdef _DEBUG
			fp=fopen("!mb_engine_debug.txt","a");
			fprintf(fp, "\n======================\n\n<dll.cpp> Dispatchspawn() - worldspawn on %s\n", STRING(gpGlobals->mapname));
			fclose(fp);
#endif
			/**/


			// set these internal variables back to defaults when the map changed to a new one or the user restarted actual map
			// normally this should have been at the beginning of Start Frame function, but Start Frame gets called after Dispatch Spawn
			// which means that in Start Frame we would reset variables (like enemy distance limit for example) that we've already set here in Dispatch Spawn so it has to be here
			internals.ResetOnMapChange();

			// doesn't reset all external variables, just those that are dynamically assigned (eg. alternative player start points)
			externals.ResetOnMapChange();

			// we need to reset the team player counters, various update timers etc. on map change 
			util.InitPrivateVars();
			
			// reset both arrays for map objectives in order to fill them with the data for this map
			ControlPoints->ResetArray();
			dodCaptureArea->ResetArray();

			// clear signatures first
			strcpy(wpt_author, "unknown");
			strcpy(wpt_modified, "unknown");

			// initialize all waypoint and path structures including their display times
			wpteditor.InitAll();

			// first reset the waypoint related warning messages in order to...
			errormsgs.DeleteErrorCode(UEMS_WARN_PTH);
			errormsgs.DeleteErrorCode(UEMS_WARN_WPT);

			// reset the whole message system
			errormsgs.ResetMessageSystem();

			// load waypoints for this map
			int result = wpteditor.LoadWaypoints(NULL, NULL);

			// if the waypoint file doesn't exist switch to the other directory and check again
			if (result == -10)
			{
				conOutput.Print(NULL, "Checking the other waypoint directory\n", MType::msg_info);

				if (internals.IsCustomWaypoints())
					internals.ResetIsCustomWaypoints();
				else
					internals.SetIsCustomWaypoints(true);

				result = wpteditor.LoadWaypoints(NULL, NULL);

				// switch back to default waypoints directory if there are no waypoints in custom
				if ((result == -10) && internals.IsCustomWaypoints())
				{
					internals.ResetIsCustomWaypoints();
					conOutput.Print(NULL, "Switching back to default waypoint directory\n", MType::msg_info);
				}
			}

			// if old waypoints were detected try to convert them automatically
			if (result == -1)
				wpteditor.LoadUnsupportedWaypoints(NULL);
			// was there any other problem
			else if ((result == 0) || (result == -10))
			{
				// then add appropriate error message
				errormsgs.AddErrorCode(UEMS_WARN_WPT);

				errormsgs.PrepareErrorAndWarning();
				conOutput.Print(NULL, errormsgs.GetError(), MType::msg_error);
				conOutput.Print(NULL, errormsgs.GetWarning(), MType::msg_warning);
			}
			else if (result == 1)
				conOutput.Print(NULL, "Loading waypoints...\n", MType::msg_info);

			// load waypoint paths for this map
			result = patheditor.LoadPaths(NULL, NULL);
			
			// if old waypoint paths are detected try convert them automatically
			if (result == -1)
				patheditor.LoadUnsupportedPaths(NULL);
			else if (result == 0)
			{
				errormsgs.AddErrorCode(UEMS_WARN_PTH);

				errormsgs.PrepareErrorAndWarning();
				conOutput.Print(NULL, errormsgs.GetError(), MType::msg_error);
				conOutput.Print(NULL, errormsgs.GetWarning(), MType::msg_warning);
			}
			else if (result == 1)
				conOutput.Print(NULL, "Loading paths...\n", MType::msg_info);

			pent_info_doddetect = NULL;


			PRECACHE_SOUND("weapons/xbow_hit1.wav");      // waypoint add
			PRECACHE_SOUND("weapons/mine_activate.wav");  // waypoint delete
			PRECACHE_SOUND("common/wpn_hudoff.wav");      // path add/delete start
			PRECACHE_SOUND("common/wpn_moveselect.wav");  // path add/delete cancel

			PRECACHE_SOUND("plats/elevbell1.wav");		// snd_done
			PRECACHE_SOUND("buttons/button10.wav");		// snd_failed

			m_spriteTexture = PRECACHE_MODEL("sprites/lgtning.spr");	// the waypoint beam
						
			m_spriteTexturePath1 = PRECACHE_MODEL("sprites/zbeam6.spr");// the one-way path beam
			m_spriteTexturePath2 = m_spriteTexture;	// other path types do use same beam as waypoint
			m_spriteTexturePath3 = PRECACHE_MODEL("sprites/rope.spr");// for paths with additional flags (avoid & ignore enemy and such like)
			
			// don't update while the map still loads (estimated time based on couple tests)
			// the first bot will join the game after like 6 seconds on Dedicated server so this needs to be done before that to give him relevant waypoint data
			internals.SetUpdateWaypointDataTime(gpGlobals->time + 5.0f);

			g_GameRules = true;

			char mapname[64]{};
			strcpy(mapname, STRING(gpGlobals->mapname));

			// see if this map is one of maps where the bots can snipe through skybox
			/*/
			if (strcmp(mapname, "ps_island") == 0)
			{
				internals.SetIsEnemyDistanceLimit(true);
				internals.SetEnemyDistanceLimit(3000.0f);
			}
			/**/
#ifdef __linux__
			// we automatically activate the hotfix for a crash of a Linux game client due to overloading the particle manager on map Charlie
			if (strcmp(mapname, "dod_charlie") == 0)
			{
				internals.SetIsFixParticleManagerCrash(true);
			}
#endif
			// check if there is a file with alternative player start positions for this map
			strcat(mapname, "_startpoints.cfg");
			startpoints_filename[0] = 0;
			util.MarineBotFileName(startpoints_filename, "mapcfgs", mapname);

			// does mapname_startpoints.cfg file exist?
			if (UTIL_IsFile(startpoints_filename))
			{
				// then activate the switch to use them
				externals.SetAlternativeStartPositions(true);

				// and reset the counters
				teamone_playerstart_position_counter = 0;
				teamtwo_playerstart_position_counter = 0;

				char msg[128]{};
				sprintf(msg, "loading alternative player start positions: %s\n", startpoints_filename);
				conOutput.Print(NULL, msg, MType::msg_info);
			}

			// check if there is a file with modified capture areas data for this map
			mapname[0] = 0;
			strcpy(mapname, STRING(gpGlobals->mapname));
			strcat(mapname, "_captureareas.cfg");
			captureareas_filename[0] = 0;
			util.MarineBotFileName(captureareas_filename, "mapcfgs", mapname);

			// does mapname_captureareas.cfg file exist?
			if (UTIL_IsFile(captureareas_filename))
			{
				// don't create the file if it already exists
				if (internals.IsBuildCaptureAreasFile())
					internals.ResetBuildCaptureAreasFile();

				externals.SetModifiedCaptureAreas(true);

				char msg[128]{};
				sprintf(msg, "loading modified capture areas data: %s\n", captureareas_filename);
				conOutput.Print(NULL, msg, MType::msg_info);
			}
			capturearea_counter = 0;

			bot_cfg_pause_time = 0.0f;
			respawn_time = 0.0f;
			spawn_time_reset = false;

			prev_num_bots = num_bots;
			num_bots = 0;
			override_max_bots = false;

			botmanager.SetBotCheckTime(gpGlobals->time + 30.0f);
		}

		// is it a player start entity AND is there an external file with modified position for it AND do we allow modifying it?
		if ((strcmp(pClassname, "info_player_allies") == 0) && externals.IsAlternativeStartPositions() && internals.IsChangeStartPosition())
		{
			// then change its position to alternative coordinates
			AlternativePlayerStartReposition(*pent, teamONE.GetTeamId(), teamone_playerstart_position_counter, startpoints_filename);
			teamone_playerstart_position_counter++;


#ifdef DEBUG
			char ermsg[128]{};
			sprintf(ermsg, "Start point origin for ALLIES is: %.1f %.1f %.1f\n", pent->v.origin.x, pent->v.origin.y, pent->v.origin.z);
			//util.DebugInFile(ermsg);
#endif // DEBUG

		}
		else if ((strcmp(pClassname, "info_player_axis") == 0) && externals.IsAlternativeStartPositions() && internals.IsChangeStartPosition())
		{
			AlternativePlayerStartReposition(*pent, teamTWO.GetTeamId(), teamtwo_playerstart_position_counter, startpoints_filename);
			teamtwo_playerstart_position_counter++;


#ifdef DEBUG
			char ermsg[128]{};
			sprintf(ermsg, "Start point origin for AXIS is: %.1f %.1f %.1f\n", pent->v.origin.x, pent->v.origin.y, pent->v.origin.z);
			//util.DebugInFile(ermsg);
#endif // DEBUG

		}

		// is the entity we were processing being spawned now?
		if (pent == pent_dispatch_detect)
		{
			if (util.IsEntityName(pent, "dod_capture_area"))
			{
				// capture area target points to control point targetname, this creates the connection (the link) between these two entities, it's the same string so it can be used to find linked control point
				int valid_array_index = ControlPoints->FindPointByLinkName(STRING(pent->v.target));

				// did we find matching control point?
				if (valid_array_index != CAPTUREPOINTS_ERROR_VAL)
				{
					// then update the capture area array with data from linked control point
					dodCaptureArea->SetOwnedByTeam(pent, ControlPoints->GetOwnedByTeam(valid_array_index));
					dodCaptureArea->SetPointObjListIndex(pent, ControlPoints->GetPointObjListIndex(valid_array_index));
				}

				// are we using modified values for the capture areas on this map? then just update the counter so that we always modify correct area
				if (externals.IsModifiedCaptureAreas())
					capturearea_counter++;
				// or do we need to create the external file with capture areas?
				else if (internals.IsBuildCaptureAreasFile())
				{
					// then create the record for this capture area and update the counter
					if (BuildCaptureAreasFile(capturearea_counter, pent))
						capturearea_counter++;
				}
			}
			else if (util.IsEntityName(pent, "dod_control_point"))
			{
				int valid_array_index = dodCaptureArea->FindPointByName(STRING(pent->v.targetname));
				
				if (valid_array_index != CAPTUREPOINTS_ERROR_VAL)
				{
					dodCaptureArea->SetOwnedByTeam(valid_array_index, ControlPoints->GetOwnedByTeam(pent));
					dodCaptureArea->SetPointObjListIndex(valid_array_index, ControlPoints->GetPointObjListIndex(pent));
				}
			}

			// finally reset the pointer so that we can start working on another entity
			pent_dispatch_detect = NULL;
		}
	}

	return (*other_gFunctionTable.pfnSpawn)(pent);
}

void DispatchThink( edict_t *pent )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"DispatchThink:\n"); fclose(fp); }
	(*other_gFunctionTable.pfnThink)(pent);
}

void DispatchUse( edict_t *pentUsed, edict_t *pentOther )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"DispatchUse:\n"); fclose(fp); }
	(*other_gFunctionTable.pfnUse)(pentUsed, pentOther);
}

void DispatchTouch( edict_t *pentTouched, edict_t *pentOther )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"DispatchTouch:\n"); fclose(fp); }
	(*other_gFunctionTable.pfnTouch)(pentTouched, pentOther);
}

void DispatchBlocked( edict_t *pentBlocked, edict_t *pentOther )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"DispatchBlocked:\n"); fclose(fp); }
	(*other_gFunctionTable.pfnBlocked)(pentBlocked, pentOther);
}

void DispatchKeyValue( edict_t *pentKeyvalue, KeyValueData *pkvd )
{
#ifdef _DEBUG
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "DispatchKeyValue: %p %s=%s\n", pentKeyvalue, pkvd->szKeyName, pkvd->szValue); fclose(fp); }
#endif

	if (pentKeyvalue == pent_info_doddetect)
	{
		//if (debug_engine) { fp = fopen(debug_fname, "a"); fprintf(fp, "DispatchKeyValue: %p %s=%s\n", pentKeyvalue, pkvd->szKeyName, pkvd->szValue); fclose(fp); }

		// see if there are brits instead of americans
		if (strcmp(pkvd->szKeyName, "detect_allies_country") == 0)
			internals.SetIsBritishTeam(atoi(pkvd->szValue));
	}
	else if (pent_info_doddetect == NULL)
	{
		if ((strcmp(pkvd->szKeyName, "classname") == 0) && (strcmp(pkvd->szValue, "info_doddetect") == 0))
		{
			pent_info_doddetect = pentKeyvalue;
		}
	}
	
	if (pentKeyvalue == pent_dispatch_detect)
	{
		if (strcmp(pkvd->szKeyName, "area_object_group") == 0)
		{
			// can this capture area be taken only when player has some object? (ie. satchel charge on map Charlie or the documents on map Jagd)
			dodCaptureArea->SetDodObjectRequired(pentKeyvalue, true);
		}
		else if (strcmp(pkvd->szKeyName, "area_time_to_cap") == 0)
		{
			dodCaptureArea->SetTimeToCapture(pentKeyvalue, strtof(pkvd->szValue, NULL));
		}
		else if (strcmp(pkvd->szKeyName, "area_axis_numcap") == 0)
		{
			// is there an external file with modified capture area data?
			if (externals.IsModifiedCaptureAreas())
			{
				char modifiedValue[2]{};

				// then try to get the new value to modify this capture area
				if (ModifiedCaptureAreas(modifiedValue, "axis_to_capture", capturearea_counter))
					sprintf(pkvd->szValue, "%s", modifiedValue);
			}

			dodCaptureArea->SetTeamTwoPlayersToCapture(pentKeyvalue, atoi(pkvd->szValue));
		}
		else if (strcmp(pkvd->szKeyName, "area_allies_numcap") == 0)
		{
			if (externals.IsModifiedCaptureAreas())
			{
				char modifiedValue[2]{};

				if (ModifiedCaptureAreas(modifiedValue, "allies_to_capture", capturearea_counter))
					sprintf(pkvd->szValue, "%s", modifiedValue);
			}

			dodCaptureArea->SetTeamOnePlayersToCapture(pentKeyvalue, atoi(pkvd->szValue));
		}
		else if (strcmp(pkvd->szKeyName, "area_axis_cancap") == 0)
		{
			dodCaptureArea->SetTeamTwoAllowedToCapture(pentKeyvalue, atoi(pkvd->szValue));
		}
		else if (strcmp(pkvd->szKeyName, "area_allies_cancap") == 0)
		{
			dodCaptureArea->SetTeamOneAllowedToCapture(pentKeyvalue, atoi(pkvd->szValue));
		}
		else if (strcmp(pkvd->szKeyName, "target") == 0)
		{
			dodCaptureArea->SetPointName(pentKeyvalue, pkvd->szValue);
		}

		// these are the data we need to read from control point entity
		else if (strcmp(pkvd->szKeyName, "origin") == 0)
		{
			ControlPoints->SetPointOrigin(pentKeyvalue, pkvd->szValue);
		}
		else if (strcmp(pkvd->szKeyName, "targetname") == 0)
		{
			ControlPoints->SetPointLinkName(pentKeyvalue, pkvd->szValue);// the targetname is the link to capture area data
		}
		else if (strcmp(pkvd->szKeyName, "point_default_owner") == 0)
		{
			ControlPoints->SetOwnedByTeam(pentKeyvalue, atoi(pkvd->szValue));
		}

		// these 2 seem to work the other way around
		else if (strcmp(pkvd->szKeyName, "point_can_allies_touch") == 0)
		{
			ControlPoints->SetTeamTwoAllowedToCapture(pentKeyvalue, atoi(pkvd->szValue));
		}
		else if (strcmp(pkvd->szKeyName, "point_can_axis_touch") == 0)
		{
			ControlPoints->SetTeamOneAllowedToCapture(pentKeyvalue, atoi(pkvd->szValue));
		}

		else if (strcmp(pkvd->szKeyName, "point_index") == 0)
		{
			int point_index = atoi(pkvd->szValue);
			
			// DoD actually assigns values starting from 1 here so we must convert this number to a true index (starts from 0) else it wouldn't work at all, because the engine messages do work
			// with true indexes then, also on some maps this index is totally bugged, because it's either -1 or some other value and it's assigned to all points,
			// but that is handled elsewhere, because here we don't have needed data to fix it
			if (point_index > 0)
				point_index = point_index - 1;

			ControlPoints->SetPointObjListIndex(pentKeyvalue, point_index);
		}
		else if (strcmp(pkvd->szKeyName, "point_name") == 0)
		{
			ControlPoints->SetPointName(pentKeyvalue, pkvd->szValue);
		}
	}
	else if (pent_dispatch_detect == NULL)
	{
		// is the engine about to process one of DoD specific capture areas?
		if ((strcmp(pkvd->szKeyName, "classname") == 0) && (strcmp(pkvd->szValue, "dod_capture_area") == 0))
		{
			// then remember its pointer so that we can get the data through it
			pent_dispatch_detect = pentKeyvalue;

			// also store the pointer to this entity to a free array slot
			dodCaptureArea->AddNewPoint(pentKeyvalue);
		}
		// or one of the control points? (both these are linked to get all the data we need)
		else if ((strcmp(pkvd->szKeyName, "classname") == 0) && (strcmp(pkvd->szValue, "dod_control_point") == 0))
		{
			pent_dispatch_detect = pentKeyvalue;
			ControlPoints->AddNewPoint(pentKeyvalue);
		}
	}


	(*other_gFunctionTable.pfnKeyValue)(pentKeyvalue, pkvd);
}

void DispatchSave( edict_t *pent, SAVERESTOREDATA *pSaveData )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"DispatchSave:\n"); fclose(fp); }
	(*other_gFunctionTable.pfnSave)(pent, pSaveData);
}

int DispatchRestore( edict_t *pent, SAVERESTOREDATA *pSaveData, int globalEntity )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"DispatchRestore:\n"); fclose(fp); }
	return (*other_gFunctionTable.pfnRestore)(pent, pSaveData, globalEntity);
}

void DispatchObjectCollsionBox( edict_t *pent )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"DispatchObjectCollsionBox:\n"); fclose(fp); }
	(*other_gFunctionTable.pfnSetAbsBox)(pent);
}

void SaveWriteFields( SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"SaveWriteFields:\n"); fclose(fp); }
	(*other_gFunctionTable.pfnSaveWriteFields)(pSaveData, pname, pBaseData, pFields, fieldCount);
}

void SaveReadFields( SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"SaveReadFields:\n"); fclose(fp); }
	(*other_gFunctionTable.pfnSaveReadFields)(pSaveData, pname, pBaseData, pFields, fieldCount);
}

void SaveGlobalState( SAVERESTOREDATA *pSaveData )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"SaveGlobalState:\n"); fclose(fp); }
	(*other_gFunctionTable.pfnSaveGlobalState)(pSaveData);
}

void RestoreGlobalState( SAVERESTOREDATA *pSaveData )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"RestoreGlobalState:\n"); fclose(fp); }
	(*other_gFunctionTable.pfnRestoreGlobalState)(pSaveData);
}

void ResetGlobalState( void )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"ResetGlobalState:\n"); fclose(fp); }
	(*other_gFunctionTable.pfnResetGlobalState)();
}

BOOL ClientConnect( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ]  )
{
	if (gpGlobals->deathmatch)
	{
		int i;

#ifdef _DEBUG
		if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "ClientConnect: pent=%p name=%s address=%s\n", pEntity, pszName, pszAddress); fclose(fp); }
#endif

		// check if this client is the listen server client
		if (strcmp(pszAddress, "loopback") == 0)
		{
			// save the edict of the listen server client...
			listenserver_edict = pEntity;
		}

		// check if this is NOT a bot joining the server...
		if (strcmp(pszAddress, "127.0.0.1") != 0)
		{
			// don't try to add bots for 60 seconds, give client time to get added
			botmanager.SetBotCheckTime(gpGlobals->time + 60.0f);

			// if there are currently more than the minimum number of bots running AND there's also more than max_bots clients on the server
			// then kick one of the bots off the server, but do this only on dedicated server
			if ((is_dedicated_server) && (clients[0].BotCount() > 0) && (clients[0].BotCount() > externals.GetMinBots()) && (externals.GetMinBots() != -1) &&
				(clients[0].ClientCount() > externals.GetMaxBots()) && (externals.GetMaxBots() != -1))
			{
				for (i=0; i < MAX_CLIENTS; i++)
				{
					// is this slot used?
					if (bots[i].is_used)
					{
						char cmd[80]{};

						sprintf(cmd, "kick \"%s\"\n", bots[i].name);

						SERVER_COMMAND(cmd);  // kick the bot using (kick "name")

						break;
					}
				}
			}
		}
	}

	return (*other_gFunctionTable.pfnClientConnect)(pEntity, pszName, pszAddress, szRejectReason);
}

void ClientDisconnect( edict_t *pEntity )
{
	if (gpGlobals->deathmatch)
	{
		int i;

#ifdef _DEBUG
		if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "ClientDisconnect: %p\n", pEntity); fclose(fp); }
#endif

		i = 0;
		while ((i < MAX_CLIENTS) && (clients[i].pEntity != pEntity))
			i++;

		if (i < MAX_CLIENTS)
		{
			if (clients[i].pEntity->v.flags & FL_FAKECLIENT)
				clients[i].substr_bot();
			else
				clients[i].substr_human();

			clients[i].pEntity = NULL;
			clients[i].SetHuman(false);
			clients[i].SetBleeding(false);
		}

		/*
#ifdef _DEBUG
		///@@@@@@@@@@@@@@@@@@@@22
		char msg[128];
		sprintf(msg, "***dll.cpp|ClientDiconnect() - total number of clients: %d\n", clients[0].ClientCount());
		conOutput.Print(NULL, msg, msg_null);
#endif
		/**/


		for (i = 0; i < MAX_CLIENTS; i++)
		{
			if (bots[i].pEdict == pEntity)
			{
				// someone kicked this bot off of the server...

				bots[i].is_used = false;  // this slot is now free to use

				bots[i].kick_time = gpGlobals->time;  // save the kicked time

				// try to find the name this bot used and sign it free..
				FreeBotName(bots[i].name, bots[i].GetBotTeam());

				break;
			}
		}

		// check if any other bot is aiming at this one, if so clear it
		for (i = 0; i < MAX_CLIENTS; i++)
		{
			if (bots[i].is_used == false)
				continue;

			if (bots[i].pEdict == pEntity)
				continue;

			// so NULL its enemy
			if (bots[i].pBotEnemy == pEntity)
			{				
				bots[i].BotForgetEnemy();
			}
		}

		// clear the pointer to the client who called a console command if it is this one
		if (conInput.GetCommandInvoker() == pEntity)
			conInput.ResetCommandInvoker();
	}

	// remove the fakeclient bit before kicking the bot
	if (pEntity->v.flags & FL_FAKECLIENT)
	{
		pEntity->v.flags &= ~FL_FAKECLIENT;

		(*other_gFunctionTable.pfnClientDisconnect)(pEntity);
	}
	else
		(*other_gFunctionTable.pfnClientDisconnect)(pEntity);
}

void ClientKill( edict_t *pEntity )
{
#ifdef _DEBUG
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "ClientKill: %p\n", pEntity); fclose(fp); }
#endif

	(*other_gFunctionTable.pfnClientKill)(pEntity);
}

void ClientPutInServer( edict_t *pEntity )
{
#ifdef _DEBUG
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "ClientPutInServer: %p\n", pEntity); fclose(fp); }
#endif

	int i = 0;

	while ((i < MAX_CLIENTS) && (clients[i].pEntity != NULL))
		i++;

	if (i < MAX_CLIENTS)
	{
		clients[i].pEntity = pEntity;  // store this clients edict in the clients array

		if (!(pEntity->v.flags & FL_FAKECLIENT))
		{
			clients[i].SetHuman(true);
			clients[i].add_human();
		}
		else
		{
			clients[i].SetHuman(false);
			clients[i].add_bot();
		}
	}




	/*/
#ifdef _DEBUG
	//@@@@@@@@@@@@@@@@@@@@22
	char msg[128];
	sprintf(msg, "***dll.cpp|ClientPutInServer() - adding client %s | total number of clients: %d\n",
		STRING(pEntity->v.netname), clients[0].ClientCount());
	conOutput.Notify(msg, true);
	//conOutput.Print(NULL, msg, msg_null);
	//util.DebugInFile(msg);
#endif
	/**/


	(*other_gFunctionTable.pfnClientPutInServer)(pEntity);
}

void ClientCommand( edict_t *pEntity )
{
	const char *pcmd = Cmd_Argv(0);
	const char *arg1 = Cmd_Argv(1);
	const char *arg2 = Cmd_Argv(2);
	const char *arg3 = Cmd_Argv(3);
	const char *arg4 = Cmd_Argv(4);
	const char *arg5 = Cmd_Argv(5);

	// save the ClCommand author if it is not a bot
	if (!(pEntity->v.flags & FL_FAKECLIENT))
		pRecipient = pEntity;



	/*/
	//@@@@@@@@@@@@@@@
	if (pEntity != listenserver_edict)
		ALERT(at_console, "ClientCommand:%s (arg1:%s)(arg2:%s)(arg3:%s)(arg4:%s)(arg5:%s)\n",
		pcmd,arg1,arg2,arg3,arg4,arg5);
	/**/



	if (debug_engine)
	{
		char edict_name[32]{};

		strcpy(edict_name, STRING(pEntity->v.netname));

		fp=fopen(debug_fname,"a"); fprintf(fp,"%s's ClientCommand: %s ",edict_name,pcmd);
		if ((arg1 != NULL) && (*arg1 != 0))
			fprintf(fp," %s", arg1);
		if ((arg2 != NULL) && (*arg2 != 0))
			fprintf(fp," %s", arg2);
		if ((arg3 != NULL) && (*arg3 != 0))
			fprintf(fp," %s", arg3);
		if ((arg4 != NULL) && (*arg4 != 0))
			fprintf(fp," %s", arg4);
		if ((arg5 != NULL) && (*arg5 != 0))
			fprintf(fp," %s", arg5);

		fprintf(fp, " (gametime=%.3f)\n", gpGlobals->time);
		fclose(fp);
	}

	// only allow custom commands in deathmatch mode AND NOT on dedicated server AND the invoker is a listen server client...
	if ((gpGlobals->deathmatch) && (is_dedicated_server == false) && (pEntity == listenserver_edict))
	{
		// if this is NOT a bot then remember who called the command
		if ((pEntity->v.flags & FL_FAKECLIENT) == false)
			conInput.SetCommandInvoker(pEntity);

		if (CustomClientCommands(pEntity, pcmd, arg1, arg2, arg3, arg4, arg5))
			return;
	}

	(*other_gFunctionTable.pfnClientCommand)(pEntity);
}

void ClientUserInfoChanged( edict_t *pEntity, char *infobuffer )
{
#ifdef _DEBUG
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "ClientUserInfoChanged: pEntity=%p infobuffer=%s\n", pEntity, infobuffer); fclose(fp); }
#endif
	
	/*/
#ifdef _DEBUG
	//@@@@@@@@@@@@@@@
	ALERT(at_console, "ClientUserInfoChanged: pEntity=%x infobuffer=%s\n", pEntity, infobuffer);
#endif
	/**/

	(*other_gFunctionTable.pfnClientUserInfoChanged)(pEntity, infobuffer);
}

void ServerActivate( edict_t *pEdictList, int edictCount, int clientMax )
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"ServerActivate: edictCount%d clientMax%d\n", edictCount, clientMax); fclose(fp); }

	(*other_gFunctionTable.pfnServerActivate)(pEdictList, edictCount, clientMax);

	// this function is called after all entities are created, ie. when the game can finally start, so we can reset the creation of external file with capture areas data here
	internals.ResetBuildCaptureAreasFile();

	// next we have to reset the marker telling us whether this map requires explosives charges to capture at least one of the map objectives or not
	internals.ResetMapGoalBasedOnExplosives();

	// also we have to to put some sense to the 'allowed to be capture' data for each Control Point, because there's a mess with this in DoD that would otherwise prevent the automated path tags work well
	// next we have to update the Control Points array with data from Capture Area array about which team is allowed to capture which point,
	// because the Control Points usually dispatch with both teams allowed, but later in the game the Capture Areas manage that
	for (int conpoint = 0; conpoint < MAX_CAPTUREPOINTS; conpoint++)
	{
		ControlPoints->NormalizeAllowedToCapture(conpoint);

		int valid_array_index = dodCaptureArea->FindPointByName(ControlPoints->GetPointLinkName(conpoint));

		if (valid_array_index != CAPTUREPOINTS_ERROR_VAL)
		{
			ControlPoints->SetTeamOneAllowedToCapture(conpoint, dodCaptureArea->GetTeamOneAllowedToCapture(valid_array_index));
			ControlPoints->SetTeamTwoAllowedToCapture(conpoint, dodCaptureArea->GetTeamTwoAllowedToCapture(valid_array_index));

			// is there at least one of the map objectives that requires explosives charge to capture? (then mark it so that there can be assigned proper path and waypoint values in navigation system) 
			if ((dodCaptureArea->GetPointObjListIndex(valid_array_index) > 0) && dodCaptureArea->GetDodObjectRequired(valid_array_index))
				internals.SetMapGoalBasedOnExplosives(true);
		}
	}

	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"ServerActivate(engine-returned): edictCount%d clientMax%d\n", edictCount, clientMax); fclose(fp); }
}

void ServerDeactivate( void )
{
	(*other_gFunctionTable.pfnServerDeactivate)();
}

void PlayerPreThink( edict_t *pEntity )
{
   (*other_gFunctionTable.pfnPlayerPreThink)(pEntity);
}

void PlayerPostThink( edict_t *pEntity )
{
	(*other_gFunctionTable.pfnPlayerPostThink)(pEntity);
}

void StartFrame( void )
{
	if (gpGlobals->deathmatch)
	{
		edict_t* pPlayer;
		static int i, index, player_index, bot_index;
		static float previous_time = -1.0f;
		static float client_update_time = 0.0f;
		clientdata_s cd;
		char msg[256]{};
		int count;

		// if a new map has started then (MUST BE FIRST IN StartFrame)...
		// this statement will be called on 2nd map load or after the user used commands like 'restart' to load your current map again or 'map <mapname>' to change map
		// the 1st map (after 'create a game' in the main menu) doesn't call this statement, constructor defaults are used there
		if ((gpGlobals->time + 0.1f) < previous_time)
		{
			char filename[256];
			char mapname[64];

			bot_t::harakiri_moment = 0.0;

			// if automatic teams balancing is enabled reset its time at start of new map
			// changed by kota@
			if (botmanager.IsTeamsBalanceNeeded() && is_dedicated_server)
				botmanager.SetTimeOfTeamsBalanceCheck(gpGlobals->time + externals.GetBalanceTime());

			// turn off teams balance override (ie do teams balance checks again if it is enabled)
			botmanager.ResetOverrideTeamsBalance();

			// if info message autosending is enabled reset its time at start of new map
			if ((check_send_info != -1.0f) && (is_dedicated_server))
				check_send_info = gpGlobals->time + externals.GetInfoTime();

			// turn off all waypoints show & auto adding commands to prevent engine overloading
			wptser.ResetOnMapChange();

			pRecipient = NULL;

			// reset the whole message system
			errormsgs.ResetMessageSystem();

			// show the presentation after some time from map change
			presentation_time = gpGlobals->time + 90.0f;

			// check if mapname_marine.cfg file exists ie are there any specific settings & classes for this map
			strcpy(mapname, STRING(gpGlobals->mapname));
			strcat(mapname, "_marine.cfg");

			util.MarineBotFileName(filename, "mapcfgs", mapname);

			// check if the map specific .cfg exists
			if (UTIL_IsFile(filename))
			{
				// forces opening configuration file
				need_to_open_cfg = true;

				// mark the bots "fully kicked" ie. no auto respawn at the start on new map
				for (index = 0; index < MAX_CLIENTS; index++)
				{
					bots[index].is_used = false;
					bots[index].respawn_state = 0;
					bots[index].kick_time = 0.0f;
				}
			}
			// otherwise we are using default .cfg file ("marine.cfg")
			else
			{
				// there was map specific .cfg for the previous map, but there's none for this map so we have to read the default .cfg again
				if (using_default_cfg == false)
				{
					need_to_open_cfg = true;

					for (index = 0; index < MAX_CLIENTS; index++)
					{
						bots[index].is_used = false;
						bots[index].respawn_state = 0;
						bots[index].kick_time = 0.0f;
					}
				}
				// otherwise we are still using the same default .cfg so we have to just respawn existing bots
				else
				{
					// but first we have to check if the configuration file is still open, because using the alternative player start positions clears the pointer to any configuration file
					// and since we do not read the configuration file again in this case then we must do this manual check here and reopen the configuration file in order to fix this specific case
					if (configFile.IsConfigFile() == false)
					{
						util.MarineBotFileName(filename, "marine.cfg", NULL);

						if (configFile.OpenConfigFile(filename) == false)
						{
							errormsgs.AddErrorCode(UEMS_ER_CFG);

							errormsgs.PrepareErrorAndWarning();
							conOutput.Print(NULL, errormsgs.GetError(), MType::msg_error);
							conOutput.Print(NULL, errormsgs.GetWarning(), MType::msg_warning);
						}
						else
						{
							sprintf(msg, "reopening configuration file: %s\n", filename);
							conOutput.Print(NULL, msg, MType::msg_info);
						}
					}

					count = 0;

					// mark the bots as needing to be respawned...
					for (index = 0; index < MAX_CLIENTS; index++)
					{
						if (count >= prev_num_bots)
						{
							bots[index].is_used = false;
							bots[index].respawn_state = 0;
							bots[index].kick_time = 0.0f;
						}

						if (bots[index].is_used)  // is this slot used?
						{
							bots[index].respawn_state = RESPAWN_NEED_TO_RESPAWN;
							count++;
						}

						// check for any bots that were very recently kicked...
						if ((bots[index].kick_time + 5.0f) > previous_time)
						{
							bots[index].respawn_state = RESPAWN_NEED_TO_RESPAWN;
							count++;
						}
						else
							bots[index].kick_time = 0.0f;  // reset to prevent false spawns later
					}
				}
			}

			// set the respawn time
			if (is_dedicated_server)
				respawn_time = gpGlobals->time + 5.0f;
			else
				respawn_time = gpGlobals->time + 20.0f;

			// start updating client data again
			client_update_time = gpGlobals->time + 10.0f;

			botmanager.SetBotCheckTime(gpGlobals->time + 30.0f);
		}
		// NEW MAP STARTED - Initialization END


		// the fancy HUD messages on listen server
		if (!is_dedicated_server)
		{
			// is the player already in the game?
			if ((listenserver_edict != NULL) && util.IsAlive(listenserver_edict) && (errormsgs.GetMessageTime() < 1.0f))
			{
				// then initialize welcome messages time so that the first message will display 2 seconds after the player spawned
				errormsgs.SetMessageTime(gpGlobals->time + 2.0f);

				// if there was any serious error detected then skip the welcome messages
				if (errormsgs.IsAnyErrorMessage())
					errormsgs.AddErrorCode(UEMS_ALLWSENT);
			}

			// everything has already been sent so we have nothing else to do here
			if (errormsgs.IsErrorCode(UEMS_ALLWSENT))
				;
			// otherwise there are still some welcome messages left to be displayed
			else
			{
				// is it time to display one message?
				if ((errormsgs.GetMessageTime() > 0.0f) && (errormsgs.GetMessageTime() < gpGlobals->time))
				{
					char fancy_msg[256];
					int msg_duration = 8;

					if (errormsgs.IsErrorCode(UEMS_WELCOME1))
					{
						sprintf(fancy_msg, "MarineBot %s %s", mb_version_info, welcome_msg);

						// let's send a welcome message to client
						Vector color1 = Vector(200, 50, 0);
						Vector color2 = Vector(0, 250, 0);
						CustHudMessageToAll(fancy_msg, color1, color2, 2, msg_duration);

						errormsgs.SetNextWelcome();

						// next welcome message will be displayed 5 seconds after this one disappeared
						errormsgs.SetMessageTime(gpGlobals->time + float(msg_duration + 5));
					}
					else if (errormsgs.IsErrorCode(UEMS_WELCOME2))
					{
						Vector color1 = Vector(200, 50, 0);
						Vector color2 = Vector(0, 250, 0);
						CustHudMessageToAll(welcome2_msg, color1, color2, 2, msg_duration);

						errormsgs.SetNextWelcome();
						errormsgs.SetMessageTime(gpGlobals->time + float(msg_duration + 5));
					}
					else if (errormsgs.IsErrorCode(UEMS_WELCOME3))
					{
						bool print_this = false;

						// get waypoints author
						if ((wpt_author[0] != 0) && (strcmp(wpt_author, "unknown") != 0))
						{
							if (util.IsOfficialWaypoints(wpt_author))
								sprintf(fancy_msg, "Official MarineBot waypoints by %s", wpt_author);
							else if ((wpt_modified[0] != 0) && (util.IsOfficialWaypoints(wpt_modified)))
								sprintf(fancy_msg, "Official MarineBot waypoints by %s", wpt_author);
							else
								sprintf(fancy_msg, "Waypoints by %s", wpt_author);

							// we have something to be displayed so do it
							print_this = true;
						}

						if ((wpt_modified[0] != 0) && (strcmp(wpt_modified, "unknown") != 0))
						{
							char temp[128];

							sprintf(temp, "were modified by %s", wpt_modified);

							sprintf(fancy_msg, "%s\n%s", fancy_msg, temp);

							print_this = true;
						}

						// do we have anything to display on screen?
						if (print_this)
						{
							Vector color1 = Vector(200, 50, 0);
							Vector color2 = Vector(0, 250, 0);
							CustHudMessageToAll(fancy_msg, color1, color2, 2, msg_duration);

							errormsgs.SetMessageTime(gpGlobals->time + float(msg_duration + 6));
						}

						// this will actually stop the sequence of welcome messages
						errormsgs.SetNextWelcome();
					}
				}
			}

			// is there any error or warning AND aren't we displaying any message right now?
			if ((errormsgs.IsAnyErrorMessage() || errormsgs.IsAnyWarningMessage()) && (errormsgs.GetMessageTime() > 0.0f) && (errormsgs.GetMessageTime() < gpGlobals->time))
			{
				char hud_er_msg[256];
				int msg_duration = 10;

				// then find the error that wasn't displayed yet
				if (errormsgs.GetHUDErrorMessage(hud_er_msg))
				{
					// and send it to clients
					Vector color1 = Vector(250, 50, 0);
					Vector color2 = Vector(255, 0, 20);
					CustHudMessageToAll(hud_er_msg, color1, color2, 2, msg_duration);

					// repeat the error message or display next error message
					// couple seconds after current message faded
					errormsgs.SetMessageTime(gpGlobals->time + float(msg_duration + 30));
				}
			}
		}
		// dedicated server
		else
		{
			if (errormsgs.IsErrorCode(UEMS_WELCOME1) && (errormsgs.GetMessageTime() < gpGlobals->time))
			{
				// was there a map change?
				if (errormsgs.GetMessageTime() < 1.0f)
					// then give the server some time to deal with stuff before sending the message
					errormsgs.SetMessageTime(gpGlobals->time + 1.0f);
				else
				{
					conOutput.Print(NULL, "\n", MType::msg_null);		// we'll make one empty line before the message itself
					conOutput.Print(NULL, "write [m_bot help] or [mbot help] into your console for command help\n\n", MType::msg_info);	// we'll also make one empty line after the message

					errormsgs.DeleteErrorCode(UEMS_WELCOME1);	// print it only once
				}
			}
		}

		// do DS initialization only once
		if ((Dedicated_Server_Init == false) && (is_dedicated_server))
		{
			Dedicated_Server_Init = true;		// run only once

			// init automatic teams balance checks time
			// changed by kota@
			if (botmanager.IsTeamsBalanceNeeded())
				botmanager.SetTimeOfTeamsBalanceCheck(gpGlobals->time + externals.GetBalanceTime());

			// sent info about console help into DS console some time after start (this timing should hit the moment right before loading external .cfg variables)
			if (errormsgs.GetMessageTime() == 0.0f)
				//welcome_msg_time = gpGlobals->time + 6.5;
				errormsgs.SetMessageTime(gpGlobals->time + 3.5f);

			// init info msg time
			if (check_send_info == 0.0f)
				check_send_info = gpGlobals->time + externals.GetInfoTime();

			// show the presentation message after some time from server intialization
			presentation_time = gpGlobals->time + 90.0f;

			presentation_msg[0] = 0;

			// use current bot version for this message
			sprintf(presentation_msg, "%s %s", presentation_const, mb_version_info);
		}

		// is it time to update weapons & equipment
		if (client_update_time <= gpGlobals->time)
		{
			client_update_time = gpGlobals->time + 1.0f;

			for (i = 0; i < MAX_CLIENTS; i++)
			{
				if (bots[i].is_used)
				{
					memset(&cd, 0, sizeof(cd));

					UpdateClientData(bots[i].pEdict, 1, &cd);

					// see if a weapon was dropped...
					if (bots[i].bot_weapons != cd.weapons)
					{
						bots[i].bot_weapons = cd.weapons;
					}
				}
			}
		}

		count = 0;

		// run bot think method for each bot (ie. play for each bot)
		for (bot_index = 0; bot_index < gpGlobals->maxClients; bot_index++)
		{
			if ((bots[bot_index].is_used) &&  // is this slot used AND
				(bots[bot_index].respawn_state == RESPAWN_IDLE))  // not respawning
			{
				bots[bot_index].BotThink();

				count++;
			}
		}

		if (count > num_bots)
			num_bots = count;

		// are the waypoints turned on?
		if (wptser.IsShowWaypoints())
		{
			for (player_index = 1; player_index <= gpGlobals->maxClients; player_index++)
			{
				pPlayer = INDEXENT(player_index);

				// is this client a human AND is he alive? (this should prevent overload on netchan error)
				if (pPlayer && !pPlayer->free && FBitSet(pPlayer->v.flags, FL_CLIENT) && !FBitSet(pPlayer->v.flags, FL_FAKECLIENT) && util.IsAlive(pPlayer))
				{
					// show/update waypoints, paths & stuff for player
					WaypointThink(pPlayer);
				}
			}
		}

		// check if we can update waypoint data yet (regular check)
		if (internals.GetUpdateWaypointDataTime() < gpGlobals->time)
		{


#ifdef DEBUG
			//ALERT(at_console, ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>UPDATE WPT DATA (globTime=%.2f)<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n", gpGlobals->time);
			//util.DebugInFile("UPDATE WPT DATA\n");
#endif // DEBUG


			if (internals.IsWaypointConversionUnfinished())
			{


#ifdef DEBUG
				//util.DebugInFile("FINISHING WAYPOINT CONVERSION\n");
#endif // DEBUG

				wpteditor.FinalizeWaypointConversion();

				// do this just once
				internals.ResetIsWaypointConversionUnfinished();
			}

			UpdateWaypointData();

			internals.SetUpdateWaypointDataTime(gpGlobals->time + 2.0f);
		}

		// print the internal message to appropriate client console
		if (internals.IsInternalMessage())
		{
			ClientPrint(conInput.GetCommandInvoker(), HUD_PRINTCONSOLE, internals.GetInternalMessage());
			internals.ResetInternalMessage();


#ifdef DEBUG

			//@@@@@@@@@@
			ALERT(at_console, "The internal message has been printed and cleared right away!\n");

#endif // DEBUG


		}

		// look if team balance is needed and is it time for it
		if ((botmanager.GetTeamsBalanceValue() > 0) && (respawn_time <= gpGlobals->time))
		{
			int balance_in_progress;

			balance_in_progress = util.ExecuteTeamsBalance();

			// is still balancing teams in progress
			if (balance_in_progress == 0)
				respawn_time = gpGlobals->time + 2.0f;
			else
				respawn_time = 0.0f;
		}

		// are we currently respawning bots and is it time to spawn one yet?
		if ((respawn_time > 1.0f) && (respawn_time <= gpGlobals->time))
		{
			int index = 0;

			// find bot needing to be respawned...
			while ((index < MAX_CLIENTS) && (bots[index].respawn_state != RESPAWN_NEED_TO_RESPAWN))
				index++;

			// respawn 1 bot then wait a while (otherwise engine might crash)
			if (index < MAX_CLIENTS)
			{
				bots[index].respawn_state = RESPAWN_IS_RESPAWNING;
				bots[index].is_used = false;      // free up this slot

				// these will temporary hold some settings
				char c_team[2];
				char c_class[3];
				char c_skill[2];
				char c_name[BOT_NAME_LEN + 1];
				int temp_bot_skill = externals.GetSpawnSkill();
				int aim_skill = externals.GetSpawnSkill();

				// store the settings
				sprintf(c_team, "%d", bots[index].GetBotTeam());
				sprintf(c_class, "%d", bots[index].GetBotClass());
				sprintf(c_skill, "%d", bots[index].GetBotSkill());
				strncpy(c_name, bots[index].name, BOT_NAME_LEN);
				c_name[BOT_NAME_LEN] = 0;  // make sure c_name is null terminated
				aim_skill = bots[index].GetAimSkill();

				// fix the right skill value (due array based style)
				temp_bot_skill = atoi(c_skill);
				temp_bot_skill++;
				sprintf(c_skill, "%d", temp_bot_skill);

				BotCreate(NULL, c_team, c_class, c_skill, bots[index].name, NULL);

				// set back stored settings
				bots[index].SetAimSkill(aim_skill);

				respawn_time = gpGlobals->time + 0.5f;		// set next respawn time
				botmanager.SetBotCheckTime(gpGlobals->time + 0.5f);		// time to next adding
			}
			else
			{
				respawn_time = 0.0f;
			}
		}

		if (g_GameRules)
		{
			if (need_to_open_cfg)  // have we open marine.cfg file yet?
			{
				char filename[256];
				char mapname[64];

				need_to_open_cfg = false;  // only do this once!!!

				// we must first close current configuration file if it is open
				if (configFile.IsConfigFile())
					configFile.CloseConfigFile();

				// and reset configuration history so we can start fresh
				configFile.ResetConfigHistory();

				// there are no configuration data now so we must also delete this error message
				errormsgs.DeleteErrorCode(UEMS_ER_CFG);

				// allows us to read the whole configuration file
				read_whole_cfg = true;

				// check if mapname_marine.cfg file exists
				strcpy(mapname, STRING(gpGlobals->mapname));
				strcat(mapname, "_marine.cfg");

				util.MarineBotFileName(filename, "mapcfgs", mapname);

				if (configFile.OpenConfigFile(filename))
				{
					sprintf(msg, "loading configuration file: %s\n", filename);
					conOutput.Print(NULL, msg, MType::msg_info);

					// to know that we changed to map specific .cfg
					using_default_cfg = false;
				}
				// there is no map specific configuration file so we must revert to default configuration file
				else
				{
					util.MarineBotFileName(filename, "marine.cfg", NULL);

					sprintf(msg, "loading configuration file: %s\n", filename);
					conOutput.Print(NULL, msg, MType::msg_info);

					using_default_cfg = true;

					if (configFile.OpenConfigFile(filename) == false)
					{
						errormsgs.AddErrorCode(UEMS_ER_CFG);

						errormsgs.PrepareErrorAndWarning();
						conOutput.Print(NULL, errormsgs.GetError(), MType::msg_error);
						conOutput.Print(NULL, errormsgs.GetWarning(), MType::msg_warning);
					}
				}

				// does configuration file exist?
				if (configFile.IsConfigFile())
				{
					// we'll count the custom classes right away, because this is quite intensive workload that would freeze the game for a few seconds when the client is already in game
					ProcessBotCfgFile(true);

					configFile.ResetConfigHistory();
				}

				// set the respawn time again, just for sure
				if (is_dedicated_server)
					bot_cfg_pause_time = gpGlobals->time + 5.0f;
				else
					bot_cfg_pause_time = gpGlobals->time + 20.0f;
			}

			if (!is_dedicated_server && !spawn_time_reset)
			{
				if (listenserver_edict != NULL)
				{
					if (util.IsAlive(listenserver_edict))
					{
						spawn_time_reset = true;

						if (respawn_time >= 1.0f)
							respawn_time = min(respawn_time, gpGlobals->time + 1.0f);

						if (bot_cfg_pause_time >= 1.0f)
							bot_cfg_pause_time = min(bot_cfg_pause_time, gpGlobals->time + 1.0f);
					}
				}
			}

			// does configuration file exist and can we start reading it?
			if (configFile.IsConfigFile() && (bot_cfg_pause_time >= 1.0f) && (bot_cfg_pause_time <= gpGlobals->time))
			{
				// then read and process it
				ProcessBotCfgFile();
			}
		}

		// check if it is DS and if it is time to see if a bot needs to be created or kicked
		if ((is_dedicated_server) && (botmanager.GetBotCheckTime() < gpGlobals->time))
		{
			botmanager.SetBotCheckTime(gpGlobals->time + 1.5f);	// time to next check

			// if there are currently LESS than the maximum number of "players" then add another bot using the default skill level...
			// changed by kota@
			if ((clients[0].ClientCount() < externals.GetMaxBots()) && (externals.GetMaxBots() != -1))
			{
				// if automatic teams balancing is ENABLED do team balancing right on join
				if (botmanager.IsTeamsBalanceNeeded())
				{
					// create a bot (allies or axis) based on team member count
					if (util.GetTeamOnePlayerCount() <= util.GetTeamTwoPlayerCount())
						BotCreate(NULL, teamONE.GetTeamIdAsString(), NULL, NULL, NULL, NULL);
					else
						BotCreate(NULL, teamTWO.GetTeamIdAsString(), NULL, NULL, NULL, NULL);
				}
				// otherwise do random join
				else
					BotCreate(NULL, NULL, NULL, NULL, NULL, NULL);
			}

			// if there are currently MORE than the maximum number of "players" and we can kick a bot then kick one
			if ((clients[0].ClientCount() > externals.GetMaxBots()) && (externals.GetMaxBots() != -1) &&
				(externals.GetMinBots() != -1) && (clients[0].BotCount() > externals.GetMinBots()) && (override_max_bots == false))
			{
				// if automatic teams balance is ENABLED then try to kick bot from "stronger" team
				if (botmanager.IsTeamsBalanceNeeded())
				{
					// if the allied team is stronger then try to kick allied bot
					if (util.GetTeamOnePlayerCount() < util.GetTeamTwoPlayerCount())
					{
						// wasn't the try to kick one blue bot successful?
						if (util.KickBot(100 + teamTWO.GetTeamId()) == false)
							// then kick random bot
							util.KickBot(-100);
					}
					// otherwise try to kick axis bot
					else
					{
						if (util.KickBot(100 + teamONE.GetTeamId()) == false)
							util.KickBot(-100);
					}
				}
				// otherwise kick a random bot
				else
					util.KickBot(-100);
			}
		}

		// is time to automatically check teams balance AND we are allowed to do teams balancing
		// changed by kota@
		else if ((botmanager.GetTimeOfTeamsBalanceCheck() < gpGlobals->time) && botmanager.IsTeamsBalanceNeeded() && (botmanager.IsOverrideTeamsBalance() == false))
		{
			if (externals.GetBalanceTime() == 0.0)
			{
				botmanager.ResetTimeOfTeamsBalanceCheck();
				botmanager.ResetTeamsBalanceNeeded();		// added by kota@

				conOutput.Print(NULL, "auto balance DISABLED!\n", MType::msg_default);
			}
			else
			{
				botmanager.SetTimeOfTeamsBalanceCheck(gpGlobals->time + externals.GetBalanceTime());
				botmanager.SetTeamsBalanceValue(util.TeamsBalanceCheck());

				// we don't want to print the message into listen server console
				if (is_dedicated_server)
				{
					conOutput.Print(NULL, "\n", MType::msg_null);	// first seperate this message

					if (botmanager.GetTeamsBalanceValue() == -2)
						conOutput.Print(NULL, "server is empty!\n", MType::msg_warning);
					else if (botmanager.GetTeamsBalanceValue() == -1)
						conOutput.Print(NULL, "there are no bots!\n", MType::msg_warning);
					else if (botmanager.GetTeamsBalanceValue() == 0)
						conOutput.Print(NULL, "teams are balanced\n", MType::msg_info);
					else if (botmanager.GetTeamsBalanceValue() > 100)
					{
						sprintf(msg, "balancing in progress... (moving %d bots from %s to %s)\n", botmanager.GetTeamsBalanceValue() - 100, teamONE.GetTeamName2wordsFUC(), teamTWO.GetTeamName2wordsFUC());
						conOutput.Print(NULL, msg, MType::msg_info);
					}
					else if ((botmanager.GetTeamsBalanceValue() > 0) && (botmanager.GetTeamsBalanceValue() < 100))
					{
						sprintf(msg, "balancing in progress... (moving %d bots from %s to %s)\n", botmanager.GetTeamsBalanceValue(), teamTWO.GetTeamName2wordsFUC(), teamONE.GetTeamName2wordsFUC());
						conOutput.Print(NULL, msg, MType::msg_info);
					}
					else if (botmanager.GetTeamsBalanceValue() < -2)
						conOutput.Print(NULL, "internal error\n", MType::msg_error);
				}
			}
		}

		// is time to automatically send information message AND can we do it
		else if ((check_send_info < gpGlobals->time) && (check_send_info > 1.0) && (is_dedicated_server))
		{
			bool more_details = true;
			int clients_info = util.TeamsBalanceCheck();

			conOutput.Print(NULL, "\n", MType::msg_null);	// separate this msg

			if (clients_info == -2)
			{
				conOutput.Print(NULL, "there are NO players or bots (server is EMPTY)!\n", MType::msg_warning);

				more_details = false;
			}
			else if (clients_info == -1)
				conOutput.Print(NULL, "there are NO bots!\n", MType::msg_warning);
			else if (clients_info > 0)
				conOutput.Print(NULL, "teams are NOT balanced!\n", MType::msg_warning);

			if (more_details)
			{
				// print correct message
				if (clients_info == -1)
				{
					sprintf(msg, "there are %d clients on %s (no bots)\n", clients[0].ClientCount(), STRING(gpGlobals->mapname));
					conOutput.Print(NULL, msg, MType::msg_info);
				}
				else
				{
					sprintf(msg, "there are %d clients on %s (%d humans and %d bots)\n", clients[0].ClientCount(), STRING(gpGlobals->mapname), clients[0].HumanCount(), clients[0].BotCount());
					conOutput.Print(NULL, msg, MType::msg_info);

					int j, k;
					char client_name[BOT_NAME_LEN + 1];

					conOutput.Print(NULL, "--------------MB skill values----------------------\n", MType::msg_null);

					// print bot names and their skill levels
					for (j = 0; j < MAX_CLIENTS; j++)
					{
						if (bots[j].is_used == false)
							continue;

						for (k = 0; k < MAX_CLIENTS; k++)
						{
							if (bots[j].pEdict == clients[k].pEntity)
								break;
						}

						strcpy(client_name, STRING(clients[k].pEntity->v.netname));

						sprintf(msg, "%s: botskill:%d and aimskill:%d\n", client_name, bots[j].GetBotSkill() + 1, bots[j].GetAimSkill() + 1);
						conOutput.Print(NULL, msg, MType::msg_null);
					}

					conOutput.Print(NULL, "---------------------------------------------------\n", MType::msg_null);
				}
			}

			check_send_info = gpGlobals->time + externals.GetInfoTime();
		}

		// do we need to fill listen server and is it time to add next bot?
		else if (botmanager.IsListenServerFilling() && (botmanager.GetBotCheckTime() < gpGlobals->time))
		{
			bool bot_added = false;
			int total_clients;
			int yet_to_fill = 0;		// if amount of bots was specified (i.e. "arg filling")

			// count clients in both teams
			total_clients = util.GetTeamOnePlayerCount() + util.GetTeamTwoPlayerCount();

			// is specified the amount of bots (i.e. "arg filling")
			if (botmanager.GetBotsToBeAdded() > 0)
			{
				yet_to_fill = botmanager.GetBotsToBeAdded();
			}

			// first check if there are some free slots on the server
			if (total_clients >= gpGlobals->maxClients)
				botmanager.ResetListenServerFilling();
			// create bot (allied or axis) based on teams member count
			else if (util.GetTeamOnePlayerCount() <= util.GetTeamTwoPlayerCount())
				bot_added = BotCreate(NULL, teamONE.GetTeamIdAsString(), NULL, NULL, NULL, NULL);
			else
				bot_added = BotCreate(NULL, teamTWO.GetTeamIdAsString(), NULL, NULL, NULL, NULL);
			
			if (bot_added)
			{
				botmanager.SetBotCheckTime(gpGlobals->time + 0.5f);	// time to next adding

				// decrease the amount of bots to be added (only if is "arg filling")
				if (yet_to_fill != 0)
				{
					botmanager.DecreaseBotsToBeAdded();

					// end filling if there is no bot to be added
					if (botmanager.GetBotsToBeAdded() == 0)
						botmanager.ResetListenServerFilling();
				}
			}
		}

		// send presentation message to all clients of the server
		else if ((is_dedicated_server) && (presentation_time < gpGlobals->time) && (presentation_time > 1.0))
		{
			presentation_time = gpGlobals->time + externals.GetPresentationTime();

			Vector color1 = Vector(200, 50, 0);
			Vector color2 = Vector(0, 250, 0);
			CustHudMessageToAll(presentation_msg, color1, color2, 2, 15);
		}

		// do save waypoints & paths automatically after set time period
		else if (internals.IsWaypointsAutoSave() && (wpt_autosave_time < gpGlobals->time))
		{
			// we don't want this message on DS
			if (is_dedicated_server == false)
				conOutput.Print(NULL, "***autosaving waypoints and paths***\n", MType::msg_null);

			// check if all went fine
			if (wpteditor.AutoSaveWaypoints())
				wpt_autosave_time = gpGlobals->time + wpt_autosave_delay;
			// otherwise try it again sooner (in other words postpone current try)
			else
			{
				wpt_autosave_time = gpGlobals->time + 30.0f;

				if (is_dedicated_server == false)
					conOutput.Print(NULL, "***waypoints and paths weren't saved***\n", MType::msg_null);
			}
		}

		previous_time = gpGlobals->time;
	} // is deathmatch END

	(*other_gFunctionTable.pfnStartFrame)();
}

void ParmsNewLevel( void )
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "ParmsNewLevel\n"); fclose(fp); }

	(*other_gFunctionTable.pfnParmsNewLevel)();
}

void ParmsChangeLevel( void )
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "ParmsChangeLevel\n"); fclose(fp); }

	(*other_gFunctionTable.pfnParmsChangeLevel)();
}

const char *GetGameDescription( void )
{
	// prepare development debugging file
	if (debug_fname[0] == '\0')
	{
		util.MarineBotFileName(debug_fname, "!mb_devdebug.txt", NULL);
	}

	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "GetGameDescription\n"); fclose(fp); }

	return (*other_gFunctionTable.pfnGetGameDescription)();
}

void PlayerCustomization( edict_t *pEntity, customization_t *pCust )
{
#ifdef _DEBUG
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "PlayerCustomization: %p\n", pEntity); fclose(fp); }
#endif

	(*other_gFunctionTable.pfnPlayerCustomization)(pEntity, pCust);
}

void SpectatorConnect( edict_t *pEntity )
{
#ifdef _DEBUG
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "SpectatorConnect: %p\n", pEntity); fclose(fp); }
#endif

	(*other_gFunctionTable.pfnSpectatorConnect)(pEntity);
}

void SpectatorDisconnect( edict_t *pEntity )
{
#ifdef _DEBUG
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "SpectatorDisconnect: %p\n", pEntity); fclose(fp); }
#endif

	(*other_gFunctionTable.pfnSpectatorDisconnect)(pEntity);
}

void SpectatorThink( edict_t *pEntity )
{
#ifdef _DEBUG
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "SpectatorThink: %p\n", pEntity); fclose(fp); }
#endif

	(*other_gFunctionTable.pfnSpectatorThink)(pEntity);
}

void Sys_Error( const char *error_string )
{
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "Sys_Error: %s\n",error_string); fclose(fp); }

#ifdef _DEBUG
	fp=fopen(debug_fname,"a");
	fprintf(fp, "Sys_Error: %s\n",error_string);
	fclose(fp);
#endif

	// dump the error in error log ... useful when there is missing some model on map load etc.
	util.DebugInFile(error_string);

	(*other_gFunctionTable.pfnSys_Error)(error_string);
}

void PM_Move ( struct playermove_s *ppmove, int server )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "PM_Move:\n"); fclose(fp); }

	(*other_gFunctionTable.pfnPM_Move)(ppmove, server);
}

void PM_Init ( struct playermove_s *ppmove )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "PM_Init:\n"); fclose(fp); }

	(*other_gFunctionTable.pfnPM_Init)(ppmove);
}

#ifndef NEWSDKAM
// if you're getting errors here then go to defines.h and change the NEWSDKAM setting
char PM_FindTextureType( char *name )
#else
char PM_FindTextureType( const char* name)
#endif
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "PM_FindTextureType: %s\n", name); fclose(fp); }

	return (*other_gFunctionTable.pfnPM_FindTextureType)(name);
}

void SetupVisibility( edict_t *pViewEntity, edict_t *pClient, unsigned char **pvs, unsigned char **pas )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "SetupVisibility: \n"); fclose(fp); }

	(*other_gFunctionTable.pfnSetupVisibility)(pViewEntity, pClient, pvs, pas);
}

void UpdateClientData ( const struct edict_s *ent, int sendweapons, struct clientdata_s *cd )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "UpdateClientData: \n"); fclose(fp); }

	(*other_gFunctionTable.pfnUpdateClientData)(ent, sendweapons, cd);
}

int AddToFullPack( struct entity_state_s *state, int e, edict_t *ent, edict_t *host, int hostflags, int player, unsigned char *pSet )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "AddToFullPack: \n"); fclose(fp); }

	return (*other_gFunctionTable.pfnAddToFullPack)(state, e, ent, host, hostflags, player, pSet);
}

void CreateBaseline( int player, int eindex, struct entity_state_s *baseline, struct edict_s *entity, int playermodelindex, vec3_t player_mins, vec3_t player_maxs )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "CreateBaseline: \n"); fclose(fp); }

	(*other_gFunctionTable.pfnCreateBaseline)(player, eindex, baseline, entity, playermodelindex, player_mins, player_maxs);
}

void RegisterEncoders( void )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "RegisterEncoders: \n"); fclose(fp); }

	(*other_gFunctionTable.pfnRegisterEncoders)();
}

int GetWeaponData( struct edict_s *player, struct weapon_data_s *info )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "GetWeaponData: \n"); fclose(fp); }

	return (*other_gFunctionTable.pfnGetWeaponData)(player, info);
}

void CmdStart( const edict_t *player, const struct usercmd_s *cmd, unsigned int random_seed )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "CmdStart: \n"); fclose(fp); }

	(*other_gFunctionTable.pfnCmdStart)(player, cmd, random_seed);
}

void CmdEnd ( const edict_t *player )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "CmdEnd: \n"); fclose(fp); }

	(*other_gFunctionTable.pfnCmdEnd)(player);
}

int ConnectionlessPacket( const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "ConnectionlessPacket: \n"); fclose(fp); }

	return (*other_gFunctionTable.pfnConnectionlessPacket)(net_from, args, response_buffer, response_buffer_size);
}

int GetHullBounds( int hullnumber, float *mins, float *maxs )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "GetHullBounds: \n"); fclose(fp); }

	return (*other_gFunctionTable.pfnGetHullBounds)(hullnumber, mins, maxs);
}

void CreateInstancedBaselines( void )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "CreateInstancedBaselines: \n"); fclose(fp); }

	(*other_gFunctionTable.pfnCreateInstancedBaselines)();
}

int InconsistentFile( const edict_t *player, const char *filename, char *disconnect_message )
{
#ifdef _DEBUG
	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "InconsistentFile: %p filename=%s discon_message=%s\n", player, filename, disconnect_message); fclose(fp); }
#endif

	return (*other_gFunctionTable.pfnInconsistentFile)(player, filename, disconnect_message);
}

int AllowLagCompensation( void )
{
	//if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp, "AllowLagCompensation: \n"); fclose(fp); }

	return (*other_gFunctionTable.pfnAllowLagCompensation)();
}


DLL_FUNCTIONS gFunctionTable =
{
	GameDLLInit,               //pfnGameInit
	DispatchSpawn,             //pfnSpawn
	DispatchThink,             //pfnThink
	DispatchUse,               //pfnUse
	DispatchTouch,             //pfnTouch
	DispatchBlocked,           //pfnBlocked
	DispatchKeyValue,          //pfnKeyValue
	DispatchSave,              //pfnSave
	DispatchRestore,           //pfnRestore
	DispatchObjectCollsionBox, //pfnAbsBox

	SaveWriteFields,           //pfnSaveWriteFields
	SaveReadFields,            //pfnSaveReadFields

	SaveGlobalState,           //pfnSaveGlobalState
	RestoreGlobalState,        //pfnRestoreGlobalState
	ResetGlobalState,          //pfnResetGlobalState

	ClientConnect,             //pfnClientConnect
	ClientDisconnect,          //pfnClientDisconnect
	ClientKill,                //pfnClientKill
	ClientPutInServer,         //pfnClientPutInServer
	ClientCommand,             //pfnClientCommand
	ClientUserInfoChanged,     //pfnClientUserInfoChanged
	ServerActivate,            //pfnServerActivate
	ServerDeactivate,          //pfnServerDeactivate

	PlayerPreThink,            //pfnPlayerPreThink
	PlayerPostThink,           //pfnPlayerPostThink

	StartFrame,                //pfnStartFrame
	ParmsNewLevel,             //pfnParmsNewLevel
	ParmsChangeLevel,          //pfnParmsChangeLevel

	GetGameDescription,        //pfnGetGameDescription    Returns string describing current .dll game.
	PlayerCustomization,       //pfnPlayerCustomization   Notifies .dll of new customization for player.

	SpectatorConnect,          //pfnSpectatorConnect      Called when spectator joins server
	SpectatorDisconnect,       //pfnSpectatorDisconnect   Called when spectator leaves the server
	SpectatorThink,            //pfnSpectatorThink        Called when spectator sends a command packet (usercmd_t)

	Sys_Error,                 //pfnSys_Error          Called when engine has encountered an error

	PM_Move,                   //pfnPM_Move
	PM_Init,                   //pfnPM_Init            Server version of player movement initialization
	PM_FindTextureType,        //pfnPM_FindTextureType

	SetupVisibility,           //pfnSetupVisibility        Set up PVS and PAS for networking for this client
	UpdateClientData,          //pfnUpdateClientData       Set up data sent only to specific client
	AddToFullPack,             //pfnAddToFullPack
	CreateBaseline,            //pfnCreateBaseline        Tweak entity baseline for network encoding, allows setup of player baselines, too.
	RegisterEncoders,          //pfnRegisterEncoders      Callbacks for network encoding
	GetWeaponData,             //pfnGetWeaponData
	CmdStart,                  //pfnCmdStart
	CmdEnd,                    //pfnCmdEnd
	ConnectionlessPacket,      //pfnConnectionlessPacket
	GetHullBounds,             //pfnGetHullBounds
	CreateInstancedBaselines,  //pfnCreateInstancedBaselines
	InconsistentFile,          //pfnInconsistentFile
	AllowLagCompensation,      //pfnAllowLagCompensation
};

#ifdef __BORLANDC__
int EXPORT GetEntityAPI( DLL_FUNCTIONS *pFunctionTable, int interfaceVersion )
#else
extern "C" EXPORT int GetEntityAPI( DLL_FUNCTIONS *pFunctionTable, int interfaceVersion )
#endif
{
   // check if engine's pointer is valid and version is correct...

   if ( !pFunctionTable || interfaceVersion != INTERFACE_VERSION )
      return FALSE;

   // pass engine callback function table to engine...
   memcpy( pFunctionTable, &gFunctionTable, sizeof( DLL_FUNCTIONS ) );

   
   // pass other DLLs engine callbacks to function table...
   if (!(*other_GetEntityAPI)(&other_gFunctionTable, INTERFACE_VERSION))
   {
      return FALSE;  // error initializing function table!!!
   }

   return TRUE;
}


#ifdef __BORLANDC__
int EXPORT GetNewDLLFunctions( NEW_DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion )
#else
extern "C" EXPORT int GetNewDLLFunctions( NEW_DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion )
#endif
{
   if (other_GetNewDLLFunctions == NULL)
      return FALSE;

   // pass other DLLs engine callbacks to function table...
   if (!(*other_GetNewDLLFunctions)(pFunctionTable, interfaceVersion))
   {
      return FALSE;  // error initializing function table!!!
   }

   return TRUE;
}


void FakeClientCommand(edict_t *pBot, const char *arg1, const char *arg2, const char *arg3)
{
	int length;

	memset(g_argv, 0, 1024);

	isFakeClientCommand = 1;

	if ((arg1 == NULL) || (*arg1 == 0))
		return;

	if ((arg2 == NULL) || (*arg2 == 0))
	{
		length = sprintf(&g_argv[0], "%s", arg1);
		fake_arg_count = 1;
	}
	else if ((arg3 == NULL) || (*arg3 == 0))
	{
		length = sprintf(&g_argv[0], "%s %s", arg1, arg2);
		fake_arg_count = 2;
	}
	else
	{	
		length = sprintf(&g_argv[0], "%s %s %s", arg1, arg2, arg3);
		fake_arg_count = 3;
	}

	g_argv[length] = 0;  // null terminate just in case

	strcpy(&g_argv[64], arg1);

	if (arg2)
		strcpy(&g_argv[128], arg2);

	if (arg3)
		strcpy(&g_argv[192], arg3);

	if (debug_engine) { fp=fopen(debug_fname,"a"); fprintf(fp,"FakeClientCommand=%s\n",g_argv); fclose(fp); }

	// allow the MOD DLL to execute the ClientCommand...
	ClientCommand(pBot);

	isFakeClientCommand = 0;
}

const char *Cmd_Args( void )
{
	if (isFakeClientCommand)
	{
		// a fix by Pierre-Marie Baty
		// is it a "say" or "say_team" client command ?
		if (strncmp ("say ", g_argv, 4) == 0)
			return (&g_argv[0] + 4); // skip the "say" bot client command (bug in HL engine)
		else if (strncmp ("say_team ", g_argv, 9) == 0)
			return (&g_argv[0] + 9); // skip the "say_team" bot client command (bug in HL engine)
		// a fix by Pierre-Marie Baty end

		return &g_argv[0];
	}
	else
	{
		return (*g_engfuncs.pfnCmd_Args)();
	}
}


const char *Cmd_Argv( int argc )
{
	if (isFakeClientCommand)
	{
		if (argc == 0)
		{
			return &g_argv[64];
		}
		else if (argc == 1)
		{
			return &g_argv[128];
		}
		else if (argc == 2)
		{
			return &g_argv[192];
		}
		else
		{
			return NULL;
		}
	}
	else
	{
		return (*g_engfuncs.pfnCmd_Argv)(argc);
	}
}


int Cmd_Argc( void )
{
	if (isFakeClientCommand)
	{
		return fake_arg_count;
	}
	else
	{
		return (*g_engfuncs.pfnCmd_Argc)();
	}
}


/*
* reads and processes external configuration file, all data are checked for validity
* the switch allows calling the function during map load to count custom classes
* it's done this way to move the excessive reading operation to a moment when it won't affect client gaming experience that much
*/
void ProcessBotCfgFile(bool only_count_custom_classes)
{
	static bool cfg_variables_initialized = false;
	static bool cfg_commands_initialized = false;
	static int processed_commands = 0;			// counter of commands that were already either added or marked as invalid
	static bool error_in_recruiting = false;	// keeps a record whether there was an error while processing the recruit commands as a whole
	static int added_recruit = 0;				// counter of already added bots from the recruit section

	bool skip_recruiting = false;				// it's used to prevent reading the custom recruiting section when we are on Dedicated Server and we reached the max bots amount of bots in game
	char msg[96];

	// stop right away if the configuration file isn't open
	if (configFile.IsConfigFile() == false)
		return;

	if (read_whole_cfg)
	{
#ifdef _DEBUG
		//@@@@@@@@@@
		conOutput.Print(NULL, "** ProcessCfgFile() - reading whole .cfg\n", MType::msg_null);
#endif
		// run the initialization just once
		read_whole_cfg = false;

		// reset these to allow us to read them again
		cfg_variables_initialized = false;
		cfg_commands_initialized = false;
		processed_commands = 0;
		error_in_recruiting = false;
		added_recruit = 0;

		// also reset the error codes related to reading configuration data
		errormsgs.DeleteErrorCode(UEMS_WARN_CFG_CMDS);
		errormsgs.DeleteErrorCode(UEMS_WARN_CFG_CVARS);
		errormsgs.DeleteErrorCode(UEMS_WARN_CFG_REC);
	}

	if (cfg_variables_initialized == false)
	{
		// set the external configuration variable to the value read from the configuration file
		externals.SetIsLogging(configFile.GetBooleanCVar("mb_log", false));

		// warn the user if there was syntax error in the configuration file
		if (configFile.IsReadError())
		{
			// is there a specific error message?
			if (configFile.IsErrorMessage())
			{
				// then let the user know what is missing in the config file
				conOutput.Print(NULL, configFile.GetErrorMessage(), MType::msg_error);

				// clean the error message
				configFile.ResetErrorMessage();
			}

			// reset it for next configuration file reading
			configFile.ResetReadError();

			// finally print general error message telling the user that the CVAR was reset to default value
			conOutput.Print(NULL, "logging reset to DISABLED!\n", MType::msg_cfg_failed);
		}
		// otherwise print the CVAR value
		else
		{
			if (externals.GetIsLogging())
				conOutput.Print(NULL, "logging ENABLED!\n", MType::msg_cfg_passed);
			else
				conOutput.Print(NULL, "logging DISABLED!\n", MType::msg_cfg_passed);
		}


		// except for invalid value we are also checking the minimum and maximum allowed value here and
		// reset the variable to the default value if those are exceeded
		externals.SetSpawnSkill(configFile.GetIntegerCVar("spawn_skill", 3, 1, 5));

		if (configFile.IsReadError())
		{
			if (configFile.IsErrorMessage())
			{
				conOutput.Print(NULL, configFile.GetErrorMessage(), MType::msg_error);
				configFile.ResetErrorMessage();
			}
			configFile.ResetReadError();

			sprintf(msg, "default %s reset to %d\n", configFile.GetCVarName(), externals.GetSpawnSkill());
			conOutput.Print(NULL, msg, MType::msg_cfg_failed);
		}
		else
		{
			sprintf(msg, "default %s set to %d\n", configFile.GetCVarName(), externals.GetSpawnSkill());
			conOutput.Print(NULL, msg, MType::msg_cfg_passed);
		}


		externals.SetRandomSkill(configFile.GetBooleanCVar("random_skill", false));

		if (configFile.IsReadError())
		{
			if (configFile.IsErrorMessage())
			{
				conOutput.Print(NULL, configFile.GetErrorMessage(), MType::msg_error);
				configFile.ResetErrorMessage();
			}
			configFile.ResetReadError();

			conOutput.Print(NULL, "reset to DISABLED!\n", configFile.GetCVarName(), MType::msg_cfg_failed);
		}
		else
		{
			if (externals.GetRandomSkill())
				conOutput.Print(NULL, "ENABLED!\n", configFile.GetCVarName(), MType::msg_cfg_passed);
			else
				conOutput.Print(NULL, "DISABLED!\n", configFile.GetCVarName(), MType::msg_cfg_passed);
		}


		externals.SetReactionTime(configFile.GetFloatCVar("reaction_time", 0.5f, 0.0f, 50.0f));

		if (configFile.IsReadError())
		{
			if (configFile.IsErrorMessage())
			{
				conOutput.Print(NULL, configFile.GetErrorMessage(), MType::msg_error);
				configFile.ResetErrorMessage();
			}
			configFile.ResetReadError();

			sprintf(msg, "%s reset to %.1fs\n", configFile.GetCVarName(), externals.GetReactionTime());
			conOutput.Print(NULL, msg, MType::msg_cfg_failed);
		}
		else
		{
			sprintf(msg, "bots %s set to %.1fs\n", configFile.GetCVarName(), externals.GetReactionTime());
			conOutput.Print(NULL, msg, MType::msg_cfg_passed);
		}


		externals.SetBalanceTime(configFile.GetFloatCVar("auto_balance", 30.0f, 30.0f, 3600.0f, 0.0f));

		if (configFile.IsReadError())
		{
			if (configFile.IsErrorMessage())
			{
				conOutput.Print(NULL, configFile.GetErrorMessage(), MType::msg_error);
				configFile.ResetErrorMessage();
			}
			configFile.ResetReadError();

			botmanager.SetTeamsBalanceNeeded(true);

			sprintf(msg, "%s time reset to %.1fs\n", configFile.GetCVarName(), externals.GetBalanceTime());
			conOutput.Print(NULL, msg, MType::msg_cfg_failed);
		}
		else
		{
			if (externals.GetBalanceTime() == 0.0)
			{
				botmanager.ResetTimeOfTeamsBalanceCheck();
				botmanager.ResetTeamsBalanceNeeded();	// added by kota@

				conOutput.Print(NULL, "DISABLED!\n", configFile.GetCVarName(), MType::msg_cfg_passed);
			}
			else
			{
				botmanager.SetTeamsBalanceNeeded(true);		// added by kota@

				sprintf(msg, "%s time set to %.1fs\n", configFile.GetCVarName(), externals.GetBalanceTime());
				conOutput.Print(NULL, msg, MType::msg_cfg_passed);
			}
		}


		externals.SetMinBots(configFile.GetIntegerCVar("min_bots", 2, 0, 31));

		if (configFile.IsReadError())
		{
			if (configFile.IsErrorMessage())
			{
				conOutput.Print(NULL, configFile.GetErrorMessage(), MType::msg_error);
				configFile.ResetErrorMessage();
			}
			configFile.ResetReadError();

			sprintf(msg, "%s reset to %d\n", configFile.GetCVarName(), externals.GetMinBots());
			conOutput.Print(NULL, msg, MType::msg_cfg_failed);
		}
		else
		{
			if (externals.GetMinBots() == 31)
			{
				externals.SetMinBots(-1);
				conOutput.Print(NULL, "DISABLED!\n", configFile.GetCVarName(), MType::msg_cfg_passed);
			}
			else
			{
				sprintf(msg, "%s set to %d\n", configFile.GetCVarName(), externals.GetMinBots());
				conOutput.Print(NULL, msg, MType::msg_cfg_passed);
			}
		}


		externals.SetMaxBots(configFile.GetIntegerCVar("max_bots", 6, 0, 31));

		if (configFile.IsReadError())
		{
			if (configFile.IsErrorMessage())
			{
				conOutput.Print(NULL, configFile.GetErrorMessage(), MType::msg_error);
				configFile.ResetErrorMessage();
			}
			configFile.ResetReadError();

			sprintf(msg, "%s reset to %d\n", configFile.GetCVarName(), externals.GetMaxBots());
			conOutput.Print(NULL, msg, MType::msg_cfg_failed);
		}
		else
		{
			if (externals.GetMaxBots() == 0)
			{
				externals.SetMaxBots(-1);
				conOutput.Print(NULL, "DISABLED!\n", configFile.GetCVarName(), MType::msg_cfg_passed);
			}
			else
			{
				sprintf(msg, "%s set to %d\n", configFile.GetCVarName(), externals.GetMaxBots());
				conOutput.Print(NULL, msg, MType::msg_cfg_passed);
			}
		}


		externals.SetInfoTime(configFile.GetFloatCVar("send_info", 150.0f, 30.0f, 3600.0f, 0.0f));

		if (configFile.IsReadError())
		{
			if (configFile.IsErrorMessage())
			{
				conOutput.Print(NULL, configFile.GetErrorMessage(), MType::msg_error);
				configFile.ResetErrorMessage();
			}
			configFile.ResetReadError();

			sprintf(msg, "%s time reset to %.1fs\n", configFile.GetCVarName(), externals.GetInfoTime());
			conOutput.Print(NULL, msg, MType::msg_cfg_failed);
		}
		else
		{
			if (externals.GetInfoTime() == 0.0)
			{
				check_send_info = -1.0;
				conOutput.Print(NULL, "DISABLED!\n", configFile.GetCVarName(), MType::msg_cfg_passed);
			}
			else
			{
				sprintf(msg, "%s time set to %.1fs\n", configFile.GetCVarName(), externals.GetInfoTime());
				conOutput.Print(NULL, msg, MType::msg_cfg_passed);
			}
		}


		externals.SetPresentationTime(configFile.GetFloatCVar("send_presentation", 210.0f, 30.0f, 3600.0f, 0.0f));

		if (configFile.IsReadError())
		{
			if (configFile.IsErrorMessage())
			{
				conOutput.Print(NULL, configFile.GetErrorMessage(), MType::msg_error);
				configFile.ResetErrorMessage();
			}
			configFile.ResetReadError();

			sprintf(msg, "%s time reset to %.1fs\n", configFile.GetCVarName(), externals.GetPresentationTime());
			conOutput.Print(NULL, msg, MType::msg_cfg_failed);
		}
		else
		{
			if (externals.GetPresentationTime() == 0.0)
			{
				presentation_time = 0.0;
				conOutput.Print(NULL, "DISABLED!\n", configFile.GetCVarName(), MType::msg_cfg_passed);
			}
			else
			{
				sprintf(msg, "%s time set to %.1fs\n", configFile.GetCVarName(), externals.GetPresentationTime());
				conOutput.Print(NULL, msg, MType::msg_cfg_passed);
			}
		}


		externals.SetHUDTextLineLength(configFile.GetIntegerCVar("hudtext_line_length", 80, 60, 140));

		if (configFile.IsReadError())
		{
			if (configFile.IsErrorMessage())
			{
				conOutput.Print(NULL, configFile.GetErrorMessage(), MType::msg_error);
				configFile.ResetErrorMessage();
			}
			configFile.ResetReadError();

			sprintf(msg, "%s reset to %d\n", configFile.GetCVarName(), externals.GetHUDTextLineLength());
			conOutput.Print(NULL, msg, MType::msg_cfg_failed);
		}
		else
		{
			sprintf(msg, "%s set to %d\n", configFile.GetCVarName(), externals.GetHUDTextLineLength());
			conOutput.Print(NULL, msg, MType::msg_cfg_passed);
		}


		externals.SetDontSpeak(configFile.GetBooleanCVar("dont_speak", false));

		if (configFile.IsReadError())
		{
			if (configFile.IsErrorMessage())
			{
				conOutput.Print(NULL, configFile.GetErrorMessage(), MType::msg_error);
				configFile.ResetErrorMessage();
			}
			configFile.ResetReadError();

			conOutput.Print(NULL, "reset to DISABLED!\n", configFile.GetCVarName(), MType::msg_cfg_failed);
		}
		else
		{
			if (externals.GetDontSpeak())
				conOutput.Print(NULL, "ENABLED!\n", configFile.GetCVarName(), MType::msg_cfg_passed);
			else
				conOutput.Print(NULL, "DISABLED!\n", configFile.GetCVarName(), MType::msg_cfg_passed);
		}


		externals.SetDontChat(configFile.GetBooleanCVar("dont_chat", false));

		if (configFile.IsReadError())
		{
			if (configFile.IsErrorMessage())
			{
				conOutput.Print(NULL, configFile.GetErrorMessage(), MType::msg_error);
				configFile.ResetErrorMessage();
			}
			configFile.ResetReadError();

			conOutput.Print(NULL, "reset to DISABLED!\n", configFile.GetCVarName(), MType::msg_cfg_failed);
		}
		else
		{
			if (externals.GetDontChat())
				conOutput.Print(NULL, "ENABLED!\n", configFile.GetCVarName(), MType::msg_cfg_passed);
			else
				conOutput.Print(NULL, "DISABLED!\n", configFile.GetCVarName(), MType::msg_cfg_passed);
		}


		externals.SetDontChatToBots(configFile.GetBooleanCVar("dont_chat_tobots", false));

		if (configFile.IsReadError())
		{
			if (configFile.IsErrorMessage())
			{
				conOutput.Print(NULL, configFile.GetErrorMessage(), MType::msg_error);
				configFile.ResetErrorMessage();
			}
			configFile.ResetReadError();

			conOutput.Print(NULL, "reset to DISABLED!\n", configFile.GetCVarName(), MType::msg_cfg_failed);
		}
		else
		{
			if (externals.GetDontChatToBots())
				conOutput.Print(NULL, "ENABLED!\n", configFile.GetCVarName(), MType::msg_cfg_passed);
			else
				conOutput.Print(NULL, "DISABLED!\n", configFile.GetCVarName(), MType::msg_cfg_passed);
		}


		externals.SetRichNames(configFile.GetBooleanCVar("rich_names", true));

		if (configFile.IsReadError())
		{
			if (configFile.IsErrorMessage())
			{
				conOutput.Print(NULL, configFile.GetErrorMessage(), MType::msg_error);
				configFile.ResetErrorMessage();
			}
			configFile.ResetReadError();

			conOutput.Print(NULL, "reset to ENABLED!\n", configFile.GetCVarName(), MType::msg_cfg_failed);
		}
		else
		{
			if (externals.GetRichNames())
				conOutput.Print(NULL, "ENABLED!\n", configFile.GetCVarName(), MType::msg_cfg_passed);
			else
				conOutput.Print(NULL, "DISABLED!\n", configFile.GetCVarName(), MType::msg_cfg_passed);
		}


		externals.SetCustomHeadshotPercentage(configFile.GetIntegerCVar("custom_headshot_percentage", -1, -1, 100));

		if (configFile.IsReadError())
		{
			if (configFile.IsErrorMessage())
			{
				conOutput.Print(NULL, configFile.GetErrorMessage(), MType::msg_error);
				configFile.ResetErrorMessage();
			}
			configFile.ResetReadError();

			conOutput.Print(NULL, "reset to DISABLED!\n", configFile.GetCVarName(), MType::msg_cfg_failed);
		}
		else
		{
			if (externals.GetCustomHeadshotPercentage() == -1)
			{
				conOutput.Print(NULL, "DISABLED!\n", configFile.GetCVarName(), MType::msg_cfg_passed);
			}
			else
			{
				sprintf(msg, "%s set to %d\n", configFile.GetCVarName(), externals.GetCustomHeadshotPercentage());
				conOutput.Print(NULL, msg, MType::msg_cfg_passed);
			}
		}


		externals.SetGrenadeUsePercentage(configFile.GetIntegerCVar("grenade_use_percentage", 25, -1, 100));

		if (configFile.IsReadError())
		{
			if (configFile.IsErrorMessage())
			{
				conOutput.Print(NULL, configFile.GetErrorMessage(), MType::msg_error);
				configFile.ResetErrorMessage();
			}
			configFile.ResetReadError();

			sprintf(msg, "%s reset to %d\n", configFile.GetCVarName(), externals.GetGrenadeUsePercentage());
			conOutput.Print(NULL, msg, MType::msg_cfg_failed);
		}
		else
		{
			if (externals.GetGrenadeUsePercentage() == -1)
			{
				conOutput.Print(NULL, "DISABLED!\n", configFile.GetCVarName(), MType::msg_cfg_passed);
			}
			else
			{
				sprintf(msg, "%s set to %d\n", configFile.GetCVarName(), externals.GetGrenadeUsePercentage());
				conOutput.Print(NULL, msg, MType::msg_cfg_passed);
			}
		}

		// now we have all external variables from the configuration file read and initialized
		cfg_variables_initialized = true;

		// was there any error while reading this section of configuration file?
		if (configFile.IsSectionError())
		{
			// set its error code
			// (it will also display this error message through HUD once client joins the game)
			errormsgs.AddErrorCode(UEMS_WARN_CFG_CVARS);

			// if we are on DS then make one empty line for better readability
			if (is_dedicated_server)
				conOutput.Print(NULL, "\n", MType::msg_null);

			// build error and warning messages
			errormsgs.PrepareErrorAndWarning();

			// print the messages directly into console
			conOutput.Print(NULL, errormsgs.GetError(), MType::msg_error);
			conOutput.Print(NULL, errormsgs.GetWarning(), MType::msg_warning);

			// also we'll reset configuration error history from this section
			configFile.ResetConfigHistory();
		}
	}


	if (bot_cfg_pause_time > gpGlobals->time)
	{
		return;
	}

	if (cfg_commands_initialized == false)
	{
		// always start at the beginning of the configuration file
		configFile.Rewind();

		// first check whether there's a correct syntax by
		// looking for valid end for this section
		if (configFile.FindKey("commands-end"))
		{
			// we must rewind the file again, because we moved past the beginning of this section
			// when we were checking for correct syntax
			configFile.Rewind();

			// find the commands section
			if (configFile.FindKey("commands"))
			{
				char the_entry[evSize]{};		// using the value size here, because the command could be long
				int this_command;			// counter of currently read commands

				the_entry[0] = 0;
				this_command = 0;

				// set the end of this section as the end of the scope that
				// we are going to work with
				configFile.SetScope("commands-end");

				// keep reading the entries till we reach the end of the scope
				while (configFile.ReadEntryScope(the_entry))
				{
					// did we find a new command?
					if (strcmp(the_entry, "addcommand") == 0)
					{
						// first we must check whether this command wasn't already processed
						if (this_command == processed_commands)
						{
							// then we can read and process it
							if (configFile.ReadValueWithSpace(the_entry, evSize - 1))
							{
								int length = strlen(the_entry);

								// replace the brackets with double quotes that the HL engine
								// uses for multi word strings
								// (using brackets in the configuration file is easier for the
								// end user than forcing him/her to use the escape character + double quotes)
								for (int pos = 0; pos < length; pos++)
								{
									if ((the_entry[pos] == '(') || (the_entry[pos] == ')'))
										the_entry[pos] = '"';
								}

								char the_command[evSize]{};
								the_command[0] = 0;

								// server command must be ended properly else it won't be accepted
								sprintf(the_command, "%s\n", the_entry);

#ifdef _DEBUG
								//@@@@@@@@@@
								sprintf(msg, "** ProcessCfgFile() - a try to add command no. %d\n", this_command + 1);
								conOutput.Print(NULL, msg, MType::msg_null);

								sprintf(msg, "** ProcessCfgFile() - the command is <%s>\n", the_command);
								conOutput.Print(NULL, msg, MType::msg_null);
#endif

								// send our command to the engine
								SERVER_COMMAND(the_command);

								// now we've added this command so increase the counter
								// to prevent working with this command again
								processed_commands++;

								// we must always process only one command at one call
								return;
							}

							// while processing this command we've found it's invalid so we cannot add it, but
							// the counter must be increased to prevent working with this command again
							processed_commands++;
						}

						// we've read one addcommand entry so increase the counter
						this_command++;
					}
				}

				if (configFile.IsErrorMessage())
				{
					conOutput.Print(NULL, configFile.GetErrorMessage(), MType::msg_error);
					configFile.ResetErrorMessage();
				}
			}
		}

		// was there any error while reading this configuration section?
		if (configFile.IsSectionError())
		{
			errormsgs.AddErrorCode(UEMS_WARN_CFG_CMDS);

			if (is_dedicated_server)
				conOutput.Print(NULL, "\n", MType::msg_null);

			errormsgs.PrepareErrorAndWarning();
			conOutput.Print(NULL, errormsgs.GetError(), MType::msg_error);
			conOutput.Print(NULL, errormsgs.GetWarning(), MType::msg_warning);

			configFile.ResetConfigHistory();
		}

		// we are done here so set this static flag to prevent reading this section over and over again,
		// it doesn't matter whether there was any syntax error in this section or not,
		// because we won't be able to get better data than what do we have now
		cfg_commands_initialized = true;

		return;
	}

	if (is_dedicated_server)
	{
		if ((externals.GetMaxBots() == -1) || (externals.GetMaxBots() == 0))
		{
#ifdef _DEBUG
			//@@@@@@@@@@
			conOutput.Print(NULL, "** ProcessCfgFile() - no bots to be auto spawned - skipping recruit section\n", MType::msg_null);
#endif
			// don't read the recruiting section
			skip_recruiting = true;
		}

		if (clients[0].ClientCount() >= externals.GetMaxBots())
		{
#ifdef _DEBUG
			//@@@@@@@@@@
			conOutput.Print(NULL, "** ProcessCfgFile() - number of clients on the server >= max_bots - skipping recruit section\n", MType::msg_info);
#endif
			skip_recruiting = true;
		}
	}

	// go back to the beginning of the configuration file...
	configFile.Rewind();

	// first check for correct syntax
	if (configFile.FindKey("recruit-end"))
	{
		// then we must rewind the file again
		configFile.Rewind();

		// now we can finally try to find the recruit section
		if (configFile.FindKey("recruit"))
		{
			char the_entry[evSize]{};
			char nr_team[evSize]{};
			char nr_class[evSize]{};
			char nr_skill[evSize]{};
			char nr_name[evSize]{};
			int this_recruit;				// counter of currently read recruit commands

			the_entry[0] = 0;
			nr_team[0] = 0;
			nr_class[0] = 0;
			nr_skill[0] = 0;
			nr_name[0] = 0;
			this_recruit = 0;

			// set the end of this section
			configFile.SetScope("recruit-end");

			// can we recruit more bots to the game?
			if (skip_recruiting == false)
			{
				// keep reading till we reach the end of this section
				while (configFile.ReadEntryScope(the_entry))
				{
					// we have found a recruit command...
					if (strcmp(the_entry, "addmarine") == 0)
					{
						// can we add this bot or was it already added before?
						if (this_recruit == added_recruit)
						{
							// if so then try to read all recruit data
							while (configFile.ReadEntryScope(the_entry))
							{
								if (strcmp(the_entry, "team") == 0)
								{
									if (configFile.ReadTeam(nr_team, teamONE.GetTeamName(), teamTWO.GetTeamName()))
									{
										// convert the team name to number, because bot create function just "atois" it to interger value
										// such conversion could have been done in the ReadTeam function, but that would complicate porting Marine Bot
										// to different HL mod (modTeams class isn't known there)
										// so it's better to keep the config functions as general as possible and do it here with the help of modTeams class
										if (strcmp(nr_team, teamONE.GetTeamName()) == 0)
											sprintf(nr_team, "%d", teamONE.GetTeamId());

										if (strcmp(nr_team, teamTWO.GetTeamName()) == 0)
											sprintf(nr_team, "%d", teamTWO.GetTeamId());
									}
								}
								else if (strcmp(the_entry, "class") == 0)
								{
									configFile.ReadClass(nr_class, 1, 10);//									TODO:	Find a way to check for correct class limit - axis/para or us/brits!!!
								}
								else if (strcmp(the_entry, "skill") == 0)
								{
									configFile.ReadSkill(nr_skill, 1, BOT_SKILL_LEVELS);
								}
								else if (strcmp(the_entry, "name") == 0)
								{
									// if we are using tagged names then we must limit the name length
									if (externals.GetRichNames())
									{
										char temp[BOT_NAME_LEN + 1];

										if (configFile.ReadValueWithSpace(temp, BOT_NAME_LEN - 4))
											sprintf(nr_name, "[MB]%s", temp);
										// the name is invalid (too long) or missing so use default bot name
										else
											sprintf(nr_name, "[MB]marine");
									}
									else
									{
										if (configFile.ReadValueWithSpace(nr_name, BOT_NAME_LEN) == false)
										{
											// if the reading failed for whatever reason then use default bot name
											nr_name[0] = 0;
											sprintf(nr_name, "marine");
										}
									}
								}
								// we've reached the next recruit command so stop reading
								else if (strcmp(the_entry, "addmarine") == 0)
									break;
							}

#ifdef _DEBUG
							//@@@@@@@@@@
							sprintf(msg, "** ProcessCfgFile() - a try to add bot number %d\n", this_recruit + 1);
							conOutput.Print(NULL, msg, MType::msg_null);

							sprintf(msg, "** ProcessCfgFile() - bot params <%s> <%s> <%s> <%s>\n", nr_team, nr_class, nr_skill, nr_name);
							conOutput.Print(NULL, msg, MType::msg_null);
#endif

							// we're going to add one bot to the game
							// if there were missing values in this recruitment command then they will be generated
							BotCreate(NULL, nr_team, nr_class, nr_skill, nr_name, NULL);

							bot_cfg_pause_time = gpGlobals->time + 2.0f;
							botmanager.SetBotCheckTime(gpGlobals->time + 2.5f);

							// we are adding this bot so increase the counter to
							// prevent adding this bot again in next calling
							added_recruit++;

							// we can add only one bot at a time so we must stop now
							// and wait till 'bot cfg pause time' allows next recruiting
							break;
						}

						// we have read one recruit command, but this bot was already recruited
						// so increase the counter to read the next recruiting command
						this_recruit++;
					}
				}

				if (configFile.IsOutOfScope())
				{
					// if we reached the end of the recruit section then
					// we must have recruited all bots and we can stop reading
					bot_cfg_pause_time = 0.0;
				}
			}
			else
			{
				// we can't recruit more bots so stop reading the configuration file
				bot_cfg_pause_time = 0.0;
			}
		}
		else
			// recruiting is the last section we are reading from the configuration file so
			// if it is missing then stop reading the configuration file
			bot_cfg_pause_time = 0.0;
	}
	else
		bot_cfg_pause_time = 0.0;

	// was there any error while reading this recruit command?
	if (configFile.IsReadError())
	{
		if (configFile.IsErrorMessage())
		{
			conOutput.Print(NULL, configFile.GetErrorMessage(), MType::msg_error);
		}

		// we must reset the whole history after the addition of each bot,
		// because bot create function will call new configuration reading for the custom class
		configFile.ResetConfigHistory();

		// therefore we are using this static error record
		error_in_recruiting = true;
	}

	// are we done with the recruit section AND was there any error in the whole section?
	if ((bot_cfg_pause_time == 0.0) && error_in_recruiting)
	{
		errormsgs.AddErrorCode(UEMS_WARN_CFG_REC);

		if (is_dedicated_server)
			conOutput.Print(NULL, "\n", MType::msg_null);

		errormsgs.PrepareErrorAndWarning();
		conOutput.Print(NULL, errormsgs.GetError(), MType::msg_error);
		conOutput.Print(NULL, errormsgs.GetWarning(), MType::msg_warning);
	}

	return;
}


// handles MB Dedicated Server Commands
void MBServerCommands(void)
{
	const char *cmd, *arg1, *arg2, *arg3, *arg4, *arg5;
	char msg[256];

	cmd = CMD_ARGV (1);
	arg1 = CMD_ARGV (2);
	arg2 = CMD_ARGV (3);
	arg3 = CMD_ARGV (4);
	arg4 = CMD_ARGV (5);
	arg5 = CMD_ARGV (6);

	if ((strcmp(cmd, "help") == 0) || (strcmp(cmd, "?") == 0))
	{
		// beause of the steam dedicated console string formating system we are forced to use only printable characters
		// (ie. no \n or \t in the string, \n will automatically end the string so anything that's behind it won't be printed to the console)
		
		conOutput.Print(NULL, "\n", MType::msg_null);
		conOutput.Print(NULL, "----------------------------------------------------\n", MType::msg_null);
		conOutput.Print(NULL, "Marine Bot dedicated server commands help\n", MType::msg_null);
		conOutput.Print(NULL, "----------------------------------------------------\n", MType::msg_null);
		conOutput.Print(NULL, "CVAR for MarineBot is [m_bot] or [mbot] so all commands must start with m_bot or mbot\n", MType::msg_null);
		conOutput.Print(NULL, "Also you can omit the underscore in any other command (e.g. max_bots -> maxbots)\n", MType::msg_null);
		conOutput.Print(NULL, "[addmarine]  adds bot with random team, class, name and default skill\n", MType::msg_null);
		sprintf(msg, "[addmarine1] adds bot to %s with random class, name and default skill\n", teamONE.GetTeamName2wordsLC());
		conOutput.Print(NULL, msg, MType::msg_null);
		sprintf(msg, "[addmarine2] adds bot to %s with random class, name and default skill\n", teamTWO.GetTeamName2wordsLC());
		conOutput.Print(NULL, msg, MType::msg_null);
		conOutput.Print(NULL, "[addmarine <team> <class> <skill> <name>] adds fully customized bot\n", MType::msg_null);
		conOutput.Print(NULL, "[min_bots <number>] specifies minimum of bots on server (31=disabled - no bot is kicked)\n", MType::msg_null);
		conOutput.Print(NULL, "[max_bots <number>] specifies maximum of bots on server (0=disabled)\n", MType::msg_null);
		sprintf(msg, "[kick_bot <argument>] kicks bot based on the argument (all, %s, %s, <his name>), if no arg then random bot gets kicked\n",
			teamONE.GetTeamName(), teamTWO.GetTeamName());
		conOutput.Print(NULL, msg, MType::msg_null);
		sprintf(msg, "[kill_bot <argument>] kills bot based on the argument (all, %s, %s, <his name>), %s or %s means the whole team\n",
			teamONE.GetTeamName(), teamTWO.GetTeamName(), teamONE.GetTeamName(), teamTWO.GetTeamName());
		conOutput.Print(NULL, msg, MType::msg_null);
		conOutput.Print(NULL, "[random_skill <current>] toggles between using default bot skill and generating the skill randomly for each bot, \"current\" returns actual state\n", MType::msg_null);
		conOutput.Print(NULL, "[spawn_skill <number>] sets default bot skill on join (1-5 where 1=best)\n", MType::msg_null);
		conOutput.Print(NULL, "[set_botskill <number>] sets bot skill level for all bots already in game\n", MType::msg_null);
		conOutput.Print(NULL, "[botskill_up] increases bot skill level for all bots already in game\n", MType::msg_null);
		conOutput.Print(NULL, "[botskill_down] decreases bot skill level for all bots already in game\n", MType::msg_null);
		conOutput.Print(NULL, "[set_aimskill <number>] sets aim skill level for all bots already in game\n", MType::msg_null);
		conOutput.Print(NULL, "[reaction_time <number>] sets bot reaction time (0-50 where 1 will be converted to 0.1s and 50 to 5.0s)\n", MType::msg_null);
		conOutput.Print(NULL, "[range_limit <number>] sets the max distance of enemy the bot can see & attack (500-7500 units)\n", MType::msg_null);
		conOutput.Print(NULL, "[balance_teams] tries to balance teams on server (moves bots to weaker team)\n", MType::msg_null);
		conOutput.Print(NULL, "[auto_balance <number>] sets time for team balance checks i.e. autobalancing (30-3600 seconds where 3600 is 1hour, setting it to 0 means never do team balance)\n", MType::msg_null);
		conOutput.Print(NULL, "[send_info <number>] sets time for info message gets sent i.e. a list of bots and their skills (30-3600s; 0=off)\n", MType::msg_null);
		conOutput.Print(NULL, "[send_presentation <number>] sets time for presentation message gets sent i.e. a HUD message to clients (30-3600s; 0=off)\n", MType::msg_null);
		conOutput.Print(NULL, "[clients] prints clients/bots count currently on server\n", MType::msg_null);
		conOutput.Print(NULL, "[version_info] prints MarineBot version\n", MType::msg_null);
		conOutput.Print(NULL, "[load_unsupported] converts & saves older waypoint file\n", MType::msg_null);
		conOutput.Print(NULL, "[directory <current>] toggles through waypoint directories, \"current\" returns name of current directory\n", MType::msg_null);
		conOutput.Print(NULL, "[dont_speak <current>] bot will not use Voice&Radio commands, \"current\" returns actual state\n", MType::msg_null);
		conOutput.Print(NULL, "[dont_chat <current>] bot will not use say&say_team commands, \"current\" returns actual state\n", MType::msg_null);
		conOutput.Print(NULL, "[dont_chat_tobots <current>] bot will not use say&say_team commands to other bots, \"current\" returns actual state\n", MType::msg_null);
		conOutput.Print(NULL, "[melee_only <current>] bot will use only melee weapons, \"current\" returns actual state\n", MType::msg_null);
		conOutput.Print(NULL, "[change_start_positions <current>] allows using the alternative start points from files in 'mapcfgs', \"current\" returns actual state\n", MType::msg_null);
		conOutput.Print(NULL, "[change_capture_areas <current>] allows using the alternative data for capture areas from files in 'mapcfgs', \"current\" returns actual state\n", MType::msg_null);
		conOutput.Print(NULL, "------------------------------------------\n", MType::msg_null);
	}
	// spawn random team bot
	else if (strcmp(cmd, "addmarine") == 0)
	{
		// allow more than maxbots bots on the server for the rest of current map
		override_max_bots = TRUE;
		
		BotCreate( NULL, arg1, arg2, arg3, arg4, NULL );

		// wait for a while before allowing the next addition
		botmanager.SetBotCheckTime(gpGlobals->time + 2.5f);
	}
	// spawn red bot
	else if (strcmp(cmd, "addmarine1") == 0)
	{
		override_max_bots = TRUE;
		BotCreate( NULL, teamONE.GetTeamIdAsString(), NULL, NULL, NULL, NULL);
		botmanager.SetBotCheckTime(gpGlobals->time + 2.5f);
	}
	// spawn blue bot
	else if (strcmp(cmd, "addmarine2") == 0)
	{
		override_max_bots = TRUE;
		BotCreate( NULL, teamTWO.GetTeamIdAsString(), NULL, NULL, NULL, NULL);
		botmanager.SetBotCheckTime(gpGlobals->time + 2.5f);
	}
	else if (conInput.IsCommand(cmd, "min_bots", "minbots"))
	{
		// first check for the validity (the type of this variable as well as allowed range)
		if (conInput.IsValidIntegerValue(arg1, 0, 31))
		{
			// then we can assign the value
			externals.SetMinBots(atoi(arg1));

			// and handle exceptions
			if (externals.GetMinBots() == 31)
			{
				externals.SetMinBots(-1);
				conOutput.Print(NULL, "DISABLED!\n", conInput.GetCmdName(), MType::msg_default);
			}
			else
			{
				sprintf(msg, "%s set to %d\n", conInput.GetCmdName(), externals.GetMinBots());
				conOutput.Print(NULL, msg, MType::msg_default);
			}
		}
		// if the value wasn't valid then print error message (including its current value)
		else
			conOutput.PrintErrorMessage(externals.GetMinBots());
	}
	else if (conInput.IsCommand(cmd, "max_bots", "maxbots"))
	{
		if (conInput.IsValidIntegerValue(arg1, 0, 31))
		{
			externals.SetMaxBots(atoi(arg1));

			if (externals.GetMaxBots() == 0)
			{
				externals.SetMaxBots(-1);
				conOutput.Print(NULL, "DISABLED!\n", conInput.GetCmdName(), MType::msg_default);
			}
			else
			{
				sprintf(msg, "%s set to %d\n", conInput.GetCmdName(), externals.GetMaxBots());
				conOutput.Print(NULL, msg, MType::msg_default);
			}
		}
		else
			conOutput.PrintErrorMessage(externals.GetMaxBots());
	}
	else if (conInput.IsCommand(cmd, "kick_bot", "kickbot"))
	{
		KickBotCommand(NULL, arg1);
	}
	else if (conInput.IsCommand(cmd, "kill_bot", "killbot"))
	{
		KillBotCommand(NULL, arg1);
	}
	else if (conInput.IsCommand(cmd, "random_skill", "randomskill"))
	{
		RandomSkillCommand(NULL, arg1);
	}
	else if (conInput.IsCommand(cmd, "spawn_skill", "spawnskill"))
	{
		SpawnSkillCommand(NULL, arg1);
	}
	else if (conInput.IsCommand(cmd, "set_botskill", "setbotskill"))
	{
		SetBotSkillCommand(NULL, arg1);
	}				
	else if (conInput.IsCommand(cmd, "botskill_up", "botskillup"))
	{
		BotSkillUpCommand(NULL, arg1);
	}
	else if (conInput.IsCommand(cmd, "botskill_down", "botskilldown"))
	{
		BotSkillDownCommand(NULL, arg1);
	}
	else if (conInput.IsCommand(cmd, "set_aimskill", "setaimskill"))
	{
		SetAimSkillCommand(NULL, arg1);
	}
	else if (conInput.IsCommand(cmd, "reaction_time", "reactiontime"))
	{
		SetReactionTimeCommand(NULL, arg1);
	}
	else if (conInput.IsCommand(cmd, "range_limit", "rangelimit"))
	{
		RangeLimitCommand(NULL, arg1);
	}
	// set the time for teams balance checking
	else if (conInput.IsCommand(cmd, "auto_balance", "autobalance"))
	{
		if (conInput.IsValidFloatValue(arg1, 30.0f, 3600.0f, 0.0f))
		{
			externals.SetBalanceTime(strtof(arg1, NULL));

			if (externals.GetBalanceTime() == 0.0f)
			{
				botmanager.ResetTeamsBalanceNeeded();	// added by kota@
				// needed to properly deactivate automatic teams balancing
				botmanager.ResetTimeOfTeamsBalanceCheck();

				conOutput.Print(NULL, "DISABLED!\n", conInput.GetCmdName(), MType::msg_default);
			}
			else
			{
				botmanager.SetTeamsBalanceNeeded(true);	// added by kota@
				botmanager.SetTimeOfTeamsBalanceCheck(gpGlobals->time + externals.GetBalanceTime());

				sprintf(msg, "%s time set to %.1fs\n", conInput.GetCmdName(), externals.GetBalanceTime());
				conOutput.Print(NULL, msg, MType::msg_default);
			}
		}
		else
			conOutput.PrintErrorMessage(externals.GetBalanceTime());
	}
	// do teams balancing manually
	else if (conInput.IsCommand(cmd, "balance_teams", "balanceteams"))
	{
		botmanager.SetTeamsBalanceValue(util.TeamsBalanceCheck());
		
		if (botmanager.GetTeamsBalanceValue() == -2)
			conOutput.Print(NULL, "server is empty!\n", MType::msg_error);
		else if (botmanager.GetTeamsBalanceValue() == -1)
			conOutput.Print(NULL, "there are no bots!\n", MType::msg_error);
		else if (botmanager.GetTeamsBalanceValue() == 0)
			conOutput.Print(NULL, "teams are balanced\n", MType::msg_default);
		else if (botmanager.GetTeamsBalanceValue() > 100)
		{
			sprintf(msg, "balancing in progress... (moving %d bots from %s to %s)\n",
				botmanager.GetTeamsBalanceValue() - 100, teamONE.GetTeamName2wordsFUC(), teamTWO.GetTeamName2wordsFUC());
			conOutput.Print(NULL, msg, MType::msg_default);
		}
		else if ((botmanager.GetTeamsBalanceValue() > 0) && (botmanager.GetTeamsBalanceValue() < 100))
		{
			sprintf(msg, "balancing in progress... (moving %d bots from %s to %s)\n",
				botmanager.GetTeamsBalanceValue(), teamTWO.GetTeamName2wordsFUC(), teamONE.GetTeamName2wordsFUC());
			conOutput.Print(NULL, msg, MType::msg_default);
		}
		else if (botmanager.GetTeamsBalanceValue() < -2)
			conOutput.Print(NULL, "internal error\n", MType::msg_error);
	}
	// set the time between two info notifications
	else if (conInput.IsCommand(cmd, "send_info", "sendinfo"))
	{
		if (conInput.IsValidFloatValue(arg1, 30.0f, 3600.0f, 0.0f))
		{
			externals.SetInfoTime(strtof(arg1, NULL));

			if (externals.GetInfoTime() == 0.0f)
			{
				check_send_info = -1.0f;
				conOutput.Print(NULL, "DISABLED!\n", conInput.GetCmdName(), MType::msg_default);
			}
			else
			{
				check_send_info = gpGlobals->time + externals.GetInfoTime();

				sprintf(msg, "%s time set to %.1fs\n", conInput.GetCmdName(), externals.GetInfoTime());
				conOutput.Print(NULL, msg, MType::msg_default);
			}
		}
		else
			conOutput.PrintErrorMessage(externals.GetInfoTime());
	}
	// set the time between two presentation messages
	else if (conInput.IsCommand(cmd, "send_presentation", "sendpresentation"))
	{
		if (conInput.IsValidFloatValue(arg1, 30.0f, 3600.0f, 0.0f))
		{
			externals.SetPresentationTime(strtof(arg1, NULL));

			if (externals.GetPresentationTime() == 0.0f)
			{
				presentation_time = 0.0;
				conOutput.Print(NULL, "DISABLED!\n", conInput.GetCmdName(), MType::msg_default);
			}
			else
			{
				presentation_time = gpGlobals->time + externals.GetPresentationTime();

				sprintf(msg, "%s time set to %.1fs\n", conInput.GetCmdName(), externals.GetPresentationTime());
				conOutput.Print(NULL, msg, MType::msg_default);
			}
		}
		else
			conOutput.PrintErrorMessage(externals.GetPresentationTime());
	}
	// get some info about all clients on the server
	else if (strcmp(cmd, "clients") == 0)
	{
		int actual_pl, red_clients, red_bots, blue_clients, blue_bots;
		
		actual_pl = red_clients = red_bots = blue_clients = blue_bots = 0;

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (clients[i].pEntity != NULL)
			{
				actual_pl++;

				if (clients[i].pEntity->v.team == 1)
				{
					red_clients++;

					if (clients[i].pEntity->v.flags & FL_FAKECLIENT)
						red_bots++;
				}
				else if (clients[i].pEntity->v.team == 2)
				{
					blue_clients++;

					if (clients[i].pEntity->v.flags & FL_FAKECLIENT)
						blue_bots++;
				}
			}
		}
		
		if (actual_pl == 0)
			conOutput.Print(NULL, "the server is empty\n", MType::msg_warning);
		else
		{
			if ((red_bots == 0) && (blue_bots == 0))
			{
				conOutput.Print(NULL, "there are no bots!\n", MType::msg_warning);
				conOutput.Print(NULL, "clients analyzed\n", MType::msg_null);
				conOutput.Print(NULL, "------------------------------------\n", MType::msg_null);
				sprintf(msg, "total clients on server: %d\n", actual_pl);
				conOutput.Print(NULL, msg, MType::msg_null);
				sprintf(msg, "%s clients: %d\n", teamONE.GetTeamName(), red_clients);
				conOutput.Print(NULL, msg, MType::msg_null);
				sprintf(msg, "%s clients: %d\n", teamTWO.GetTeamName(), blue_clients);
				conOutput.Print(NULL, msg, MType::msg_null);
				conOutput.Print(NULL, "------------------------------------\n", MType::msg_null);
			}
			else
			{
				conOutput.Print(NULL, "clients analyzed\n", MType::msg_default);
				conOutput.Print(NULL, "------------------------------------\n", MType::msg_null);
				sprintf(msg, "total clients on server: %d\n", actual_pl);
				conOutput.Print(NULL, msg, MType::msg_null);
				sprintf(msg, "%s bots: %d out of %d %s clients\n", teamONE.GetTeamName2wordsLC(), red_bots, red_clients, teamONE.GetTeamName());
				conOutput.Print(NULL, msg, MType::msg_null);
				sprintf(msg, "%s bots: %d out of %d %s clients\n", teamTWO.GetTeamName2wordsLC(), blue_bots, blue_clients, teamTWO.GetTeamName());
				conOutput.Print(NULL, msg, MType::msg_null);
				conOutput.Print(NULL, "------------------------------------\n", MType::msg_null);
			}
		}
	}
	else if (conInput.IsCommand(cmd, "version_info", "versioninfo"))
	{
		sprintf(msg, "You are using MarineBot version: %s\n", mb_version_info);
		conOutput.Print(NULL, msg, MType::msg_default);
	}
	else if (conInput.IsCommand(cmd, "load_unsupported", "loadunsupported"))
	{
		conOutput.Print(NULL, "starting conversion...\n", MType::msg_default);
		
		if (wpteditor.LoadUnsupportedWaypoints(NULL))
		{
			if (patheditor.LoadUnsupportedPaths(NULL))
			{
				wpteditor.SaveWaypoints(NULL);
				patheditor.SavePaths(NULL);
				
				conOutput.Print(NULL, "older waypoints and paths converted and saved\n", MType::msg_info);
			}
		}
	}
	else if (strcmp(cmd, "directory") == 0)
	{
		SetWaypointDirectoryCommand(NULL, arg1);
	}
	else if (conInput.IsCommand(cmd, "dont_speak", "dontspeak"))
	{
		DontSpeakCommand(NULL, arg1);
	}
	else if (conInput.IsCommand(cmd, "dont_chat", "dontchat"))
	{
		DontChatCommand(NULL, arg1);
	}
	else if (conInput.IsCommand(cmd, "dont_chat_tobots", "dontchattobots"))
	{
		DontChatToBotsCommand(NULL, arg1);
	}
	else if (conInput.IsCommand(cmd, "melee_only", "meleeonly"))
	{
		MeleeOnlyCommand(NULL, arg1);
	}
	else if (conInput.IsCommand(cmd, "change_start_positions", "changestartpositions"))
	{
		ChangeStarPositionsCommand(NULL, arg1);
	}
	else if (conInput.IsCommand(cmd, "change_capture_areas", "changecaptureareas"))
	{
		ChangeCaptureAreasCommand(NULL, arg1);
	}
	else
	{
		conOutput.Print(NULL, "invalid or unknown command!\n", MType::msg_error);
		conOutput.Print(NULL, "write [m_bot help] or [m_bot ?] into your console for command help\n", MType::msg_info);
	}
}