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
// bot.cpp
// 
////////////////////////////////////////////////////////////////////////////////////////////////

#include "defines.h"

#pragma warning( disable: 4005 91 )

#include "extdll.h"
#include "util.h"
#include "cbase.h"

#pragma warning( default: 4005 91 )

#include "bot.h"
#include "bot_config.h"
#include "bot_func.h"
#include "bot_manager.h"
#include "bot_weapons.h"
#include "client_commands.h"
#include "console_output.h"
#include "waypoint.h"

#include <sys/types.h>
#include <sys/stat.h>


#ifndef __linux__
extern HINSTANCE h_Library;
#else
extern void *h_Library;
#endif

extern bool is_dedicated_server;

//extern int team_class_limits[4];		// this might be useful
extern char bot_whine[MAX_BOT_WHINE][81];
extern int whine_count;

static FILE *fp;

bot_t *bots = NULL;   // no bots in a game yet

float bot_t::harakiri_moment = 0.0;

extern int debug_engine;
extern int recent_bot_whine[5];

botname_t bot_names_american[MAX_BOT_NAMES];		// array of bot names read from external file
botname_t bot_names_british[MAX_BOT_NAMES];
botname_t bot_names_german[MAX_BOT_NAMES];

// used to make some difference between bots in the "ability to hear" a sound, based on their skill level
float g_sound_sensitivity[BOT_SKILL_LEVELS] = { 2.0f, 1.8f, 1.6f, 1.4f, 1.0f };



// few function prototypes used in this file
int FindBotName(const char* bot_name, botname_t* bot_names_array, int array_size);
bool FindFreeBotName(char* bot_name, botname_t* bot_names_array, int array_size, int array_index);
void BotPickName(char* name_buffer, const char* team_value_as_string);
void BotChangeNameToMatchNation(bot_t* pBot, const char* nation);
bool IsEntityInSphere(const char* entity_name, edict_t *pEdict, float radius);
bool IsEntityInSphere(const char* entity_name, edict_t* pEdict, float radius, edict_t* pIgnoreEntity);
//int BotInFieldOfView(bot_t *pBot, Vector dest);
bool BotEntityIsVisible( bot_t *pBot, Vector dest );
void BotFindItem( bot_t *pBot );
bool IsBleeding(edict_t *pPatient);
void BotUseClaymoreMine(bot_t* pBot);
void BotCommunicateWithOthers(bot_t* pBot);
bool DealWithWeaponManipulation(bot_t* pBot, const char* loc);
bool BotManageWeaponUse(bot_t* pBot, const char* loc);


inline edict_t *CREATE_FAKE_CLIENT( const char *netname )
{
	return (*g_engfuncs.pfnCreateFakeClient)( netname );
}

inline char *GET_INFOBUFFER( edict_t *e )
{
	return (*g_engfuncs.pfnGetInfoKeyBuffer)( e );
}

inline char *GET_INFO_KEY_VALUE( char *infobuffer, char *key )
{
	return (g_engfuncs.pfnInfoKeyValue( infobuffer, key ));
}

inline void SET_CLIENT_KEY_VALUE( int clientIndex, char *infobuffer, char *key, char *value )
{
	(*g_engfuncs.pfnSetClientKeyValue)( clientIndex, infobuffer, key, value );
}


// this is the LINK_ENTITY_TO_CLASS function that creates a player (bot)
void player( entvars_t *pev )
{
	static LINK_ENTITY_FUNC otherClassName = NULL;

	if (otherClassName == NULL)
		otherClassName = (LINK_ENTITY_FUNC)GetProcAddress(h_Library, "player");

	if (otherClassName != NULL)
	{
		(*otherClassName)(pev);
	}
}


bot_t::bot_t()
{
		is_used			= false;
		respawn_state	=0;
		pEdict			= NULL;
		name[0]			= '\0';
		bot_flags		= 0;
		start_action	= 0;		// not needed for non-team MODs
		kick_time		= 0.0f;

		in_team			= teamNULL;
		bot_class		= NO_VAL;
		face_skin		= NO_VAL;
		bot_skill		= 0;
		aiming_skill	= 0;
		bot_behaviour	= 0;
		bot_fa_skills	= 0;

		bot_spawn_time = 0.0f;

//		killer_edict	= NULL;
		main_weapon		= NO_VAL;
		backup_weapon	= NO_VAL;
		melee_weapon	= NO_VAL;
		used_weapon		= uWeapon::none;
		grenade_slot	= NO_VAL;
		claymore_slot	= NO_VAL;

		BotSpawnInit();
		prev_wpt_index.print();
}


/*
* sets nearly all bot variables to initial/default value
* needed after the bot has been killed or when the bot is joining the game
*/
void bot_t::BotSpawnInit()
{
#ifdef _DEBUG
	
	//																				NEW CODE 094 (remove it)
	if (strlen(name) > 1)
	{
		if (devTool.IsBotDebugging(pEdict))
		{
			char dm[256]{};
			sprintf(dm, "%s is respawning\n", name);
			util.DebugInFile(dm);
		}
	}
	//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

#endif
	
	ResetPosture();
	ResetStance();
	bot_tasks = 0;
	bot_subtasks = 0;
	bot_needs = 0;
	SetNeed(NEED_POSTSPAWN_DECISIONS);		// set this need to make bot decide soon after (re)spawning (back) to game
	
	current_health = 0;
	UpdatePrevHealth();
	amount_of_bandages = 0;
	bandage_time = 0.0f;
	medic_treat_time = 0.0f;

	paused_time = 0.0f;
	time_to_look_for_ground_items = 0.0f;
	ground_item_pos = g_vecZero;

	f_max_speed = CVAR_GET_FLOAT("sv_maxspeed");
	prev_movespeed = MoveSpeed::max;				// fake movement since bot is NOT stuck
	current_movespeed = MoveSpeed::stop;
	moved_distance_check_time = gpGlobals->time;
	prev_origin = Vector(9999.0f, 9999.0f, 9999.0f);
	dont_move_time = 0.0f;
	// pick a wander direction (50% of the time to the left, 50% to the right)
	if (RANDOM_LONG(1, 100) <= 50)
		wander_direction = SIDE_LEFT;
	else
		wander_direction = SIDE_RIGHT;
	strafe_time = 0.0f;
	strafe_direction = 0.0f;
	SetDontCheckStuck();				// prevents an invalid try to unstuck when bot spawns
	got_stuck_time = 0.0f;
	unstuck_attempts = 0;
	check_deathfall_time = 0.0f;

	dont_look_for_waypoint_time = 0.0f;
	curr_wpt_index = NO_VAL;
	prev_wpt_index.clear();
	curr_wpt_fake_position = g_vecZero;
	prev_distance_to_curr_wpt = 9999.0f;
	RemoveBehaviour(BOT_PRECISION);
	time_to_reach_curr_wpt = gpGlobals->time;
	time_to_face_waypoint = 0.0f;
	wpt_action_time = 0.0f;
	bot_wait_time = 0.0f;
	pGameEntity = NULL;
	time_to_face_game_entity = 0.0f;
	point_in_space = g_vecZero;
	Aims.Clear();
	curr_aim_index = NO_VAL;
	time_to_keep_current_aim = 0.0f;
	time_of_adjusting_the_aim = 0.0f;
	targeting_the_aim_stop = 0;
	duckjump_time = 0.0f;
	ladder_use_direction = LadderDir::unknown;
	start_ladder_time = 0.0f;
	reached_end_of_ladder = false;
	parachute_use_time = 0.0f;
	go_prone_time = 0.0f;


	//pBot->waypoint_goal = -1;
	//pBot->f_waypoint_goal_time = 0.0;
	//pBot->waypoint_near_flag = FALSE;
	//pBot->waypoint_flag_origin = Vector(0, 0, 0);

	curr_path_index = NO_VAL;
	prev_path_index = NO_VAL;
	patrol_path_waypoint = NO_VAL;

	msecnum = 0;
	msecdel = 0.0;
	msecval = 0.0;

	bot_armor = 0;
	bot_weapons = 0;
	
	f_wall_on_right = 0.0;
	f_wall_on_left = 0.0;
	f_dont_avoid_wall_time = 0.0;

	pTeamLeader = NULL;
	bot_see_team_leader_time = 0.0f;

	pBotEnemy = NULL;
	pBotPrevEnemy = NULL;
	bot_see_enemy_time = 0.0f;
	dont_look_for_enemy_time = 0.0f;
	check_for_closer_enemy_time = 0.0f;
	last_known_enemy_position = g_vecZero;
	wait_for_enemy_time = 0.0f;
	prev_distance_to_enemy = 0.0f;
	bot_hide_time = 0.0f;
	bot_reaction_time = 0.0f;
	
	weapon_action = W_READY;
	weapon_status = 0;

	claymore_plant_time = 0.0f;
	grenade_use_time = 0.0f;
	check_ammunition_time = gpGlobals->time;
	SetTask(TASK_CHECKAMMO);		// set this task to make bot check it right after spawn
	take_ammo_for_main_weapon = 0;
	take_ammo_for_backup_weapon = 0;

	f_shoot_time = gpGlobals->time;
	full_auto_fire_time = 0.0f;
	weapon_reload_time = 0.0f;
	bipod_deploy_time = 0.0f;
	bipod_yaw_angle = 0.0f;
	snipe_time = 0.0f;
	advance_toward_enemy_time = 0.0f;
	override_advance_time = 0.0f;
	check_stance_time = 0.0f;
	stance_change_time = 0.0f;

	speak_time = 0.0f;
	voice_command_to_use = voiceCmd::nothing;
	text_message_time = 0.0f;
	previous_message = botSay::nothing;


//	b_bot_say_killed = FALSE;
//	f_bot_say_killed = 0.0;

	//pBot->b_use_button = FALSE;
	//pBot->f_use_button_time = 0;
	//pBot->b_lift_moving = FALSE;

	//pBot->b_use_capture = FALSE;
	//pBot->f_use_capture_time = 0.0;
	//pBot->pCaptureEdict = NULL;

	memset(&(current_weapon), 0, sizeof(current_weapon));
	memset(&(curr_rgAmmo), 0, sizeof(curr_rgAmmo));// array of current amount of mags

	harakiri = false;



	SetBlindedTime(0.0f);
	SetTimeOfNextSoundsCheck(0.0f);

	prev_globals_time = gpGlobals->time;


#ifdef _DEBUG
	is_forced = false;
	forced_stance = BOT_STANDING;
#endif
}


void bot_t::InitializeAtCreation(void)
{
	is_used = true;
	respawn_state = RESPAWN_IDLE;
	name[0] = 0;					// name not set by server yet
	start_action = MSG_VGUI_IDLE;			

	in_team = teamNULL;
	bot_class = NO_VAL;
	face_skin = NO_VAL;
	bot_skill = 0;
	aiming_skill = 0;
	bot_behaviour = 0;
	bot_fa_skills = 0;
	bot_flags = 0;

	SetBotFlag(BF_NOT_JOINED_GAME);		// hasn't joined game yet

	main_weapon = NO_VAL;
	backup_weapon = NO_VAL;
	melee_weapon = NO_VAL;
	grenade_slot = NO_VAL;
	claymore_slot = NO_VAL;
}


float bot_t::CalculateMovedDistanceSinceLastCheck(void)
{
	float moved_by = 2.0f;

	if (moved_distance_check_time <= gpGlobals->time)
	{
		// see how far bot has moved since the previous position
		moved_by = (prev_origin - pEdict->v.origin).Length();

		// save current position as previous
		prev_origin = pEdict->v.origin;
		moved_distance_check_time = gpGlobals->time + 0.2f;
	}
	
	return moved_by;
}


/*
* sets the correct float value for RunPlayerMove() based on the move speed flag from bot class
*/
float bot_t::ConvertMoveSpeedToRealValue()
{
	// but first we need to fix DoD strange maxspeed values else bot would move ridiculously fast ... so let's tweak it to reasonable value if it is something insane
	// the value is taken from tests, bots are made like 10 units faster than human player when it comes to standard move speed, still you can catch them
	if (IsTask(TASK_SPRINT))
		;
	else if (GetMaxSpeed() > 230.0f)
		SetMaxSpeed(230.0f);

	// then we can finally use it
	float tmp_move_speed = GetMaxSpeed();

	switch (current_movespeed)
	{
		case MoveSpeed::stop:
		{
			tmp_move_speed = 0.0f;
			break;
		}
		case MoveSpeed::slowest:
		{
			// don't change the speed if bot is crouched or is lying prone
			if ((IsCrouched() == false) && (IsProne() == false))
				tmp_move_speed = GetMaxSpeed() / 4.0f;

			break;
		}
		case MoveSpeed::slow:
		{
			if ((IsCrouched() == false) && (IsProne() == false))
				tmp_move_speed = GetMaxSpeed() / 2.0f;

			break;
		}
		default:
			break;
	}

	return tmp_move_speed;
}


/*
* resets global arrays with bot names before filling
*/
void BotNameArraysInit(void)
{
	for (int i = 0; i < MAX_BOT_NAMES; i++)
	{
		bot_names_american[i].name[0] = 0;
		bot_names_american[i].is_used = false;

		bot_names_british[i].name[0] = 0;
		bot_names_british[i].is_used = false;

		bot_names_german[i].name[0] = 0;
		bot_names_german[i].is_used = false;
	}
}


/*
* reads names from external file and puts them to global array of bot names
*/
bool BotNamesInit(const char* bot_names_filename)
{
	FILE* bot_name_fp;
	const int buffer_size = 80;
	char name_buffer[buffer_size];
	int already_used_names = 0;
	bool amer_names_found = false;
	bool brit_names_found = false;
	bool ger_names_found = false;

	bot_name_fp = fopen(bot_names_filename, "r");

	if (bot_name_fp != NULL)
	{
		// read american names
		if (internals.GetAmerNamesCount() < MAX_BOT_NAMES)
		{
			// just in case someone changed order of the sections with names
			rewind(bot_name_fp);

			// search for the american name starting tag
			while ((amer_names_found == false) && (fgets(name_buffer, buffer_size, bot_name_fp) != NULL))
			{
				if (strstr(name_buffer, "american_names"))
				{
					amer_names_found = true;
					break;
				}
			}

			// we've found the american names we can read them into the array now
			if (amer_names_found)
			{
				while ((already_used_names < MAX_BOT_NAMES) && (fgets(name_buffer, buffer_size, bot_name_fp) != NULL) && (strstr(name_buffer, "american_names-end") == NULL))
				{
					// ignore empty buffer and commented out or empty lines in the file
					if ((name_buffer[0] != 0) && (name_buffer[0] != '#') && (name_buffer[0] != '\n') && (name_buffer[0] != '\r'))
					{
						// make sure the name is properly ternimated i.e. we care only about the 31 characters of the name, as is said in the external file
						// actually this works as a solution to deal with those who ignore what is written in the external file, the buffer is quite large to hold even really long name,
						// but we use only the first 31 characters from it, if the buffer was just for the 31 characters and someone ignored the guidelines and used long name then
						// we would have read the other part of the long name as a new name and that would be confusing at least
						name_buffer[BOT_NAME_LEN] = 0;

						// first get rid of the newline character/s at the end of the name
						util.RemoveNewlineCharsFromBufferEnd(name_buffer);

						// then remove any illegal characters from the name
						util.RemoveIllegalCharsFromBuffer(name_buffer);
						
						already_used_names = internals.GetAmerNamesCount();

						// copy the name that was read from the file to the array
						strcpy(bot_names_american[already_used_names].name, name_buffer);

						internals.SetAmerNamesCount(++already_used_names);
					}
				}
			}
		}

		// read british names
		if (internals.GetBritNamesCount() < MAX_BOT_NAMES)
		{
			rewind(bot_name_fp);

			while ((brit_names_found == false) && (fgets(name_buffer, buffer_size, bot_name_fp) != NULL))
			{
				if (strstr(name_buffer, "british_names"))
				{
					brit_names_found = true;
					break;
				}
			}

			if (brit_names_found)
			{
				while ((already_used_names < MAX_BOT_NAMES) && (fgets(name_buffer, buffer_size, bot_name_fp) != NULL) && (strstr(name_buffer, "british_names-end") == NULL))
				{
					if ((name_buffer[0] != 0) && (name_buffer[0] != '#') && (name_buffer[0] != '\n') && (name_buffer[0] != '\r'))
					{
						name_buffer[BOT_NAME_LEN] = 0;
						util.RemoveNewlineCharsFromBufferEnd(name_buffer);
						util.RemoveIllegalCharsFromBuffer(name_buffer);

						already_used_names = internals.GetBritNamesCount();
						strcpy(bot_names_british[already_used_names].name, name_buffer);
						internals.SetBritNamesCount(++already_used_names);
					}
				}
			}
		}

		// read german names
		if (internals.GetGerNamesCount() < MAX_BOT_NAMES)
		{
			rewind(bot_name_fp);

			while ((ger_names_found == false) && (fgets(name_buffer, buffer_size, bot_name_fp) != NULL))
			{
				if (strstr(name_buffer, "german_names"))
				{
					ger_names_found = true;
					break;
				}
			}

			if (ger_names_found)
			{
				while ((already_used_names < MAX_BOT_NAMES) && (fgets(name_buffer, buffer_size, bot_name_fp) != NULL) && (strstr(name_buffer, "german_names-end") == NULL))
				{
					if ((name_buffer[0] != 0) && (name_buffer[0] != '#') && (name_buffer[0] != '\n') && (name_buffer[0] != '\r'))
					{
						name_buffer[BOT_NAME_LEN] = 0;
						util.RemoveNewlineCharsFromBufferEnd(name_buffer);
						util.RemoveIllegalCharsFromBuffer(name_buffer);

						already_used_names = internals.GetGerNamesCount();
						strcpy(bot_names_german[already_used_names].name, name_buffer);
						internals.SetGerNamesCount(++already_used_names);
					}
				}
			}
		}

		fclose(bot_name_fp);
		return true;
	}

	return false;
}


/*
* returns the nation based on the bot name passed in
*/
char* GetNationFromBotName(const char* bot_name)
{
	if (FindBotName(bot_name, &bot_names_german[0], internals.GetGerNamesCount()) > NO_VAL)
	{
		return "german";
	}

	if (FindBotName(bot_name, &bot_names_american[0], internals.GetAmerNamesCount()) > NO_VAL)
	{
		return "american";
	}

	if (FindBotName(bot_name, &bot_names_british[0], internals.GetBritNamesCount()) > NO_VAL)
	{
		return "british";
	}

	return "unknown";
}


/*
* searches arrays with bot names trying to find specified name in order to mark it free
*/
bool FreeBotName(const char* bot_name, int bot_team)
{
	int name_index;

	if (bot_team == teamONE.GetTeamId())
	{
		// see if there are brits as the allied team
		if (internals.IsBritishTeam())
		{
			name_index = FindBotName(bot_name, &bot_names_british[0], internals.GetBritNamesCount());

			if (name_index > NO_VAL)
			{
				// this bot name is free again
				bot_names_british[name_index].is_used = false;
				return true;
			}
		}

		// well then it must be americans
		name_index = FindBotName(bot_name, &bot_names_american[0], internals.GetAmerNamesCount());

		if (name_index > NO_VAL)
		{
			bot_names_american[name_index].is_used = false;
			return true;
		}
	}
	
	if (bot_team == teamTWO.GetTeamId())
	{
		name_index = FindBotName(bot_name, &bot_names_german[0], internals.GetGerNamesCount());

		if (name_index > NO_VAL)
		{
			bot_names_german[name_index].is_used = false;
			return true;
		}
	}

	return false;
}


/*
* searches given array of bot names for specified bot name and returns its index if found
* otherwise returns -1
*/
int FindBotName(const char* bot_name, botname_t* bot_names_array, int array_size)
{
	for (int name_index = 0; name_index < array_size; name_index++)
	{
		if (externals.GetRichNames())
			bot_names_array[name_index].name[BOT_NAME_LEN - 4] = 0;

		if (strstr(bot_name, bot_names_array[name_index].name))
		{
			return name_index;
		}
	}

	return NO_VAL;
}


/*
* checks whether the name in given slot of given array of names is free to use, if so then that bot name is marked and used
*/
bool FindFreeBotName(char* bot_name, botname_t *bot_names_array, int array_size, int array_index)
{
	bool used = true;
	int attempts = 0;
	char stripped_name[BOT_NAME_LEN + 1]{};

	while (used)
	{
		// is there another bot using this name?
		if (bot_names_array[array_index].is_used)
		{
			// try next name
			array_index++;

			// if we reached the end of the list of available names then return back at the start of it
			if (array_index == array_size)
				array_index = 0;

			// prevents infinite loops
			attempts++;
		}
		// otherwise this name should be free, but we need to test all existing clients
		else
		{
			// go through all clients
			for (int index = 1; index <= gpGlobals->maxClients; index++)
			{
				edict_t* pPlayer = INDEXENT(index);

				// we have to process only clients who have name, otherwise we would test even non-existing clients with empty names
				if (pPlayer && strlen(STRING(pPlayer->v.netname)) > 0)
				{
					stripped_name[0] = 0;

					util.RemoveTagsFromBotname(STRING(pPlayer->v.netname), stripped_name);

					if (stripped_name[0] != 0)
					{
						if (strstr(bot_names_array[array_index].name, stripped_name))
						{
							// we found that another bot uses this name so set correct flag, ie. mark this name as used
							bot_names_array[array_index].is_used = true;
						}
					}
				}
			}

			// this name must really be free so...
			if (bot_names_array[array_index].is_used == false)
			{
				// mark this name as used, because...
				bot_names_array[array_index].is_used = true;
				// we are going to use it
				strcpy(bot_name, bot_names_array[array_index].name);

				return true;
			}
		}

		// break out of loop even if this name is already used
		if (attempts == array_size)
			used = false;
	}

	return false;
}


/*
* pick any free (not used) name from global array of bot names
* adds Marine Bot sign/tag if needed (allowed by .cfg variable)
*/
void BotPickName(char* name_buffer, const char* team_value_as_string)
{
	int team = teamNULL;
	int name_index = 0;
	int number_of_names = 0;
	bool found_one = false;
	char free_bot_name[BOT_NAME_LEN + 1]{};

	// see if the team was specified
	if (conInput.IsValidTeam(team_value_as_string, true, false))
	{
		team = conInput.GetIntegerValue();
	}
	// otherwise pick team with less players
	else
	{
		if (util.GetTeamOnePlayerCount() <= util.GetTeamTwoPlayerCount())
			team = teamONE.GetTeamId();
		else
			team = teamTWO.GetTeamId();
	}

	// allies
	if (team == teamONE.GetTeamId())
	{
		if (internals.IsBritishTeam())
		{
			number_of_names = internals.GetBritNamesCount();
			name_index = RANDOM_LONG(1, number_of_names) - 1;  // zero based

			found_one = FindFreeBotName(free_bot_name, &bot_names_british[0], number_of_names, name_index);
		}
		// if it is NOT brits then it must be americans
		else
		{
			number_of_names = internals.GetAmerNamesCount();
			name_index = RANDOM_LONG(1, number_of_names) - 1;

			found_one = FindFreeBotName(free_bot_name, &bot_names_american[0], number_of_names, name_index);
		}

	}
	// axis
	else if (team == teamTWO.GetTeamId())
	{
		number_of_names = internals.GetGerNamesCount();
		name_index = RANDOM_LONG(1, number_of_names) - 1;

		found_one = FindFreeBotName(free_bot_name, &bot_names_german[0], number_of_names, name_index);
	}

	// we didn't find any free bot name so let's make some random-ish name
	if (found_one == false)
	{
		sprintf(free_bot_name, "marine%d", RANDOM_LONG(1, 100));
	}

	// if needed insert the '[MB]' tag right before the name
	if (externals.GetRichNames())
	{
		// first we should cut off 4 characters from the name to make space for the tag so that following function stays within the array boundaries
		free_bot_name[BOT_NAME_LEN - 4] = 0;

		// then add the processed name behind the tag
		sprintf(name_buffer, "[MB]%s", free_bot_name);
	}
	// otherwise use only the name
	else
	{
		strcpy(name_buffer, free_bot_name);
	}

	// make sure name is null terminated
	name_buffer[BOT_NAME_LEN] = 0;
}


/*
* puts bot in the game
* sets all values if corresponding arguments are valid otherwise are left untouched
* for further proccessing (methods in bot_start.cpp generates them)
* arg1 is team
* arg2 is class
* arg3 is skill level
* arg4 is name
* arg5 is unused in DoD
*/
bool BotCreate( edict_t *pPlayer, const char *arg1, const char *arg2, const char *arg3, const char *arg4, const char *arg5)
{
	edict_t *BotEnt;
	bot_t *pBot;
	char c_name[BOT_NAME_LEN + 1]{};
	int skill;

	// general initialization
	c_name[0] = 0;
	skill = 1;

	if (conInput.IsValidIntegerValue(arg3, 1, BOT_SKILL_LEVELS))
		skill = conInput.GetIntegerValue();
	else
	{
		// if there is a random skill request then generate the skill number
		if (externals.GetRandomSkill())
			skill = RANDOM_LONG(1, BOT_SKILL_LEVELS);
		// otherwise use default skill
		else
			skill = externals.GetSpawnSkill();
	}

	if ((arg4 != NULL) && (*arg4 != 0))
	{
		strncpy(c_name, arg4, BOT_NAME_LEN);
		c_name[BOT_NAME_LEN] = 0;  // make sure c_name is null terminated
	}
	else
	{
		int number_of_names_in_external_file = internals.GetAmerNamesCount() + internals.GetBritNamesCount() + internals.GetGerNamesCount();

		if (number_of_names_in_external_file > 0)
			BotPickName(c_name, arg1);
		else
		{
			if (externals.GetRichNames())
				sprintf(c_name, "[MB]marine%d", RANDOM_LONG(1, 100));
			else
				sprintf(c_name, "marine%d", RANDOM_LONG(1, 100));
		}
	}

	// remove any illegal characters from name
	util.RemoveIllegalCharsFromBuffer(c_name);

	BotEnt = CREATE_FAKE_CLIENT( c_name );

	if (FNullEnt( BotEnt ))
	{
		if (pPlayer)
		{
			ClientPrint( pPlayer, HUD_PRINTNOTIFY, "Max. Players reached.  Can't create bot!\n");

			return false;
		}
	}
	else
	{
		char ptr[128];  // allocate space for message from ClientConnect
		char *infobuffer;
		int clientIndex;
		int index;

		conOutput.Print(pPlayer, "Creating MarineBot...\n", MType::msg_null);

		index = 0;
		while ((bots[index].is_used) && (index < MAX_CLIENTS))
			index++;

		if (index == MAX_CLIENTS)
		{
			conOutput.Print(pPlayer, "Can't create MarineBot server is full!\n", MType::msg_null);

			return false;
		}

		// create the player entity by calling MOD's player function
		// (from LINK_ENTITY_TO_CLASS for player object)

		// kick & rejoin bug - a fix by Pierre-Marie Baty
		FREE_PRIVATE (BotEnt);
		BotEnt->pvPrivateData = NULL;
		BotEnt->v.frags = 0;
		// a fix by Pierre-Marie Baty end

		player( VARS(BotEnt) );

		infobuffer = GET_INFOBUFFER( BotEnt );
		clientIndex = ENTINDEX( BotEnt );

		SET_CLIENT_KEY_VALUE( clientIndex, infobuffer, "model", "gina" );

		ClientConnect( BotEnt, c_name, "127.0.0.1", ptr );

		BotEnt->v.flags |= FL_FAKECLIENT;

		// Pieter van Dijk - use instead of DispatchSpawn() - Hip Hip Hurray!
		ClientPutInServer( BotEnt );

		// original position, but I've moved it above the client put in server, because we need to know if it is bot or not right in that function
		//BotEnt->v.flags |= FL_FAKECLIENT;

		// initialize all the variables for this bot

		pBot = &bots[index];

		pBot->pEdict = BotEnt;

		BotEnt->v.idealpitch = BotEnt->v.v_angle.x;
		BotEnt->v.ideal_yaw = BotEnt->v.v_angle.y;
		BotEnt->v.pitch_speed = 20.0f;
		BotEnt->v.yaw_speed = 20.0f;

		pBot->InitializeAtCreation();
		pBot->BotSpawnInit();

		pBot->SetBotSkill(skill - 1);	// zero based array index
		pBot->SetAimSkill(skill - 1);	// by default it uses the same value we use for botskill

		if (conInput.IsValidTeam(arg1, true, false))
		{
			pBot->SetBotTeam(conInput.GetIntegerValue());

			// the max allowed value for the class argument is set based on the count of hardcoded classes
			if (conInput.IsValidIntegerValue(arg2, 1, 10))
			{
				pBot->SetBotClass(conInput.GetIntegerValue());
			}
		}
	}

	return true;
}


/*
* finds current bot name in array of names matching the passed nation variable and marks this name free, then randomly picks a new name for the bot and changes the name for this bot by calling engine functions
*/
void BotChangeNameToMatchNation(bot_t* pBot, const char* nation)
{
	int name_index;
	char change_name[BOT_NAME_LEN + 1]{};
	char team_id_str[2]{};
	char* infobuffer;
	int clientIndex;

	// first make current bot name free again, because we aren't going to use it from now on
	if (strcmp(nation, "american") == 0)
	{
		name_index = FindBotName(pBot->name, &bot_names_american[0], internals.GetAmerNamesCount());

		if (name_index > NO_VAL)
			bot_names_american[name_index].is_used = false;
	}
	else if (strcmp(nation, "british") == 0)
	{
		name_index = FindBotName(pBot->name, &bot_names_british[0], internals.GetBritNamesCount());

		if (name_index > NO_VAL)
			bot_names_british[name_index].is_used = false;
	}
	else if (strcmp(nation, "german") == 0)
	{
		name_index = FindBotName(pBot->name, &bot_names_german[0], internals.GetGerNamesCount());

		if (name_index > NO_VAL)
			bot_names_german[name_index].is_used = false;
	}

	sprintf(team_id_str, "%d", pBot->GetBotTeam());

	// now we can pick a new name for this bot
	BotPickName(change_name, team_id_str);

	// call to the engine to change the name of this client ie. the name for this bot
	player(VARS(pBot->pEdict));
	infobuffer = GET_INFOBUFFER(pBot->pEdict);
	clientIndex = ENTINDEX(pBot->pEdict);

	SET_CLIENT_KEY_VALUE(clientIndex, infobuffer, "name", change_name);

	// finally we have to update the name for this bot with new name
	strcpy(pBot->name, change_name);
}


/*
* returns true is there's the entity in given range we've search for
*/
bool IsEntityInSphere(const char* entity_name, edict_t *pEdict, float radius)
{
	edict_t *pent = NULL;

	while ((pent = util.FindEntityInSphere(pent, pEdict->v.origin, radius )) != NULL)
	{
		char item_name[64]{};
		strcpy(item_name, STRING(pent->v.classname));

		if (strcmp(entity_name, item_name) == 0)
			return true;
	}

	return false;
}


/*
* Overloaded to add ignore entity pointer
*/
bool IsEntityInSphere(const char* entity_name, edict_t* pEdict, float radius, edict_t* pIgnoreEntity)
{
	edict_t* pent = NULL;

	while ((pent = util.FindEntityInSphere(pent, pEdict->v.origin, radius)) != NULL)
	{
		if (pent == pIgnoreEntity)
			continue;

		char item_name[64]{};
		strcpy(item_name, STRING(pent->v.classname));

		if (strcmp(entity_name, item_name) == 0)
			return true;
	}

	return false;
}


/*
* returns true if the target object is fully visible
*/
bool BotEntityIsVisible( bot_t *pBot, Vector dest )
{
	TraceResult tr;
	
	// trace a line from bot's eyes to destination...
	UTIL_TraceLine( pBot->pEdict->v.origin + pBot->pEdict->v.view_ofs, dest, ignore_monsters, pBot->pEdict, &tr );
	
	// check if line of sight to object is not blocked (i.e. visible)
	if (tr.flFraction >= 1.0f)
		return true;
		
	return false;
}


/*
* scans the surrounding for certain items/objects
*/
void BotFindItem( bot_t *pBot )
{
	edict_t *pent = NULL;
	Vector vecStart, vecEnd;
	int angle_to_entity;
	edict_t *pEdict = pBot->pEdict;
	
	while ((pent = util.FindEntityInSphere( pent, pEdict->v.origin, FIND_ITEM_RADIUS )) != NULL)
	{
		// handle thrown grenades
		if (util.IsEntityName(pent, "grenade") || util.IsEntityName(pent, "grenade2"))
		{
			// ignore grenades thrown by your teammate or unknown grenades even if it might save lifes in certain cases (ie. TKs)
			if(util.AreTeammates(pent, pEdict))
				continue;

			vecStart = pEdict->v.origin + pEdict->v.view_ofs;
			vecEnd = pent->v.origin;

			angle_to_entity = util.InFieldOfView(pEdict, vecEnd - vecStart);

			if (angle_to_entity > 45)
				continue;

			if (BotEntityIsVisible(pBot, vecEnd))
			{
				if (RANDOM_FLOAT(1, 100) > 50)
					pBot->UseTextMessage(botSay::grenade_in);
				else
					pBot->BotSpeak(voiceCmd::grenade);

				break;
			}
		}
	} // end the while
}


/*
* clears all posture bits from bot behaviour
*/
void bot_t::ResetPosture(void)
{
	RemoveBehaviour(BOT_STANDING);
	RemoveBehaviour(BOT_CROUCHED);
	RemoveBehaviour(BOT_PRONED);
}


/*
* clears all goto stance bits from bot behaviour
*/
void bot_t::ResetStance(void)
{
	RemoveBehaviour(GOTO_STANDING);
	RemoveBehaviour(GOTO_CROUCH);
	RemoveBehaviour(GOTO_PRONE);
}


/*
* checks if bot can go/resume prone, if so then sets the appropriate task
*/
bool bot_t::GoProne(const char* loc)
{
	if (IsNotGoingProne() && (IsBehaviour(BOT_DONTGOPRONE) == false) && (IsSubTask(ST_CANTPRONE) == false) && (IsTask(TASK_BIPOD) == false) && IsWeaponReady())
	{
		SetTask(TASK_GOPRONE);


#ifdef DEBUG
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@														// NEW CODE 094 (remove it)
		if ((loc != NULL) && (botdebugger.IsDebugStance() || botdebugger.IsDebugStuck()))
		{
			char dm[128]{};
			sprintf(dm, "called Go Prone() @ %s\n", loc);
			conOutput.Notify(dm, this);
		}
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#endif // DEBUG


		return true;
	}

	return false;
}


/*
* clear all Stance flags first and then set the new one
*/
void bot_t::SetStance(int flag, const char* loc)
{
	return SetStance(flag, false, loc);
}


/*
* overloaded to allow forcing stance right after we just set one
*/
void bot_t::SetStance(int flag, bool is_forced_stance, const char* loc)
{
	// is bot already in this Stance OR using bipod OR deploying/folding it right now OR still changing the Stance, don't set new one
	if ((IsBehaviour(flag)) || IsTask(TASK_BIPOD) || (IsNotDeployingBipod() == false) || ((IsNotChangingStance() == false) && !is_forced_stance))
		return;

	if (flag & (BOT_STANDING | BOT_CROUCHED | BOT_PRONED))
	{
#ifdef DEBUG
		util.DebugInFile("Set Stance() -> INVALID stance bit was sent !!!\n");
#endif // DEBUG

		return;
	}


#ifdef DEBUG
	//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@														// NEW CODE 094 (remove it)
	if ((loc != NULL) && (botdebugger.IsDebugStance() || botdebugger.IsDebugStuck()))
	{
		char dm[128]{};
		sprintf(dm, "called Set Stance() @ %s\n", loc);
		conOutput.Notify(dm, this);
	}
	//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#endif // DEBUG




	// if we want the bot to go prone, but he cannot go prone at his current location ...
	if ((flag & GOTO_PRONE) && IsSubTask(ST_CANTPRONE))
	{



#ifdef DEBUG
		if (botdebugger.IsDebugStance())
		{
			// @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@							NEW CODE 094 (remove it)
			char dm[128]{};
			sprintf(dm, "Set Stance() GOTO PRONE && SubTask CANTPRONE -> Stance set to GOTO STANDING !!!\n");
			conOutput.Notify(dm, this);
		}
#endif // DEBUG




		// then reset the stance back to standing
		flag = GOTO_STANDING;
	}


	if (IsNotGoingProne() == false)
	{



#ifdef DEBUG
		char dm[128]{};
		sprintf(dm, "Set Stance() -> called while going to/resume from prone !!!\n");
		conOutput.Notify(dm, this);
#endif // DEBUG



	}

	// clear all flags
	ResetStance();

#ifdef _DEBUG
	if (is_forced)
		flag = forced_stance;
#endif

	// and set new flag
	SetBehaviour(flag);

	// we need to store the time when we changed the stance ... adding short delay there
	SetCheckStanceTime(0.5f);

	return;
}


/*
* assigns the radio or voice or hand signal the bot has to use, together with the task to speak and the moment when will he use this command
* set delay allows to postpone speaking by given number of seconds (eg. in case of a reply to some command) instead of random delay that is used normally
*/
void bot_t::BotSpeak(voiceCmd use_this_command, float set_delay)
{
	// has the bot already something to say then don't add anything new
	if (IsTask(TASK_SPEAK))
		return;

	// remember the command
	voice_command_to_use = use_this_command;

	// do we need to postpone the speaking by set amount of seconds? (this is specifically needed for the cases when bot has to reply to some command someone else used at him)
	if (set_delay > 0.0f)
		SetSpeakTime(set_delay);
	// otherwise generate the moment when will the bot use this command randomly
	else
		SetSpeakTime(RANDOM_FLOAT(0.4f, 1.0f));

	// and set the task to speak
	SetTask(TASK_SPEAK);
}


/*
* picks the best message for current situation passed by message type argument
*/
void bot_t::UseTextMessage(botSay text_message, edict_t *pRecipient, float since_last_msg)
{
	// use say command only if allowed to do so
	if (externals.GetDontChat())
		return;

	// is this message for a specific client AND can we chat to human clients only AND is that client a bot?
	if (pRecipient && externals.GetDontChatToBots() && (util.GetBotIndex(pRecipient) != -1))
		return;

	// we don't want to spam the game
	if ((text_message_time + since_last_msg > gpGlobals->time) || (speak_time + 1.0f > gpGlobals->time))
		return;

	// if the bot said exactly this message in last 3 seconds then don't say it again
	if ((previous_message == text_message) && (text_message_time + 3.0f > gpGlobals->time))
		return;

	// store the time bot said something
	text_message_time = gpGlobals->time;

	int choice = RANDOM_LONG(1, 100);
	char msg[128]{};
	char name[BOT_NAME_LEN + 1]{};
	name[0] = '\0';

	switch (text_message)
	{
		case botSay::grenade_in:
			if (choice < 50)
				util.TeamSay(pEdict, "GRENADE!");
			else
				util.TeamSay(pEdict, "GRENADE! Take cover");
			break;
		case botSay::grenade_out:
			if (choice < 50)
				util.TeamSay(pEdict, "Fire in the hole!");
			else
				util.TeamSay(pEdict, "Frag out!");
			break;
		case botSay::claymore_found:
			if (choice < 50)
				util.TeamSay(pEdict, "Claymore spotted! Watch your steps");
			else
				util.TeamSay(pEdict, "There's a mine at my position. Watch out!");
			// to prevent spamming the game
			text_message_time = gpGlobals->time + 1.0f;
			break;
		case botSay::medic_help_you:
			// there must be a recipient for this message type
			if (pRecipient)
			{
				util.HumanizeTheName(STRING(pRecipient->v.netname), name);
				sprintf(msg, "Hey, %s, stay still and I'll treat you", name);
				util.TeamSay(pEdict, msg);
			}
			text_message_time = gpGlobals->time + 1.0f;
			break;
		case botSay::medic_cant_help:
			if (pRecipient)
			{
				util.HumanizeTheName(STRING(pRecipient->v.netname), name);
				if (choice < 50)
					sprintf(msg, "Sorry, %s, I can't help you", name);
				else
					sprintf(msg, "I can't help you %s", name);
				util.TeamSay(pEdict, msg);
			}
			text_message_time = gpGlobals->time + 1.0f;
			break;
		case botSay::cease_fire:
			if (pRecipient)
			{
				util.HumanizeTheName(STRING(pRecipient->v.netname), name);
				if (choice < 50)
					sprintf(msg, "Cease fire, %s", name);
				else
					sprintf(msg, "Hey, %s, cease fire!", name);
				util.TeamSay(pEdict, msg);
			}
			text_message_time = gpGlobals->time + 1.0f;
			break;
		case botSay::enemy_spotted:
			if (choice < 50)
				util.TeamSay(pEdict, "Enemy spotted!");
			else
				util.TeamSay(pEdict, "Get down!");
			break;
		default:
			break;
	}

	// remember the message that has to be said, to prevent spamming the game
	previous_message = text_message;
}


/*
* checks if the patient still bleeds
*/
bool IsBleeding(edict_t *pPatient)
{
	int index = util.GetBotIndex(pPatient);

	// is the patient a bot
	if (index != -1)
	{
		if (bots[index].IsTask(TASK_BLEEDING))
			return true;
	}
	// or is it a human player
	else
	{
		// search this patient in clients array to see if he still bleeds
		for (index = 0; index < MAX_CLIENTS; index++)
		{
			if ((clients[index].pEntity == pPatient) && clients[index].IsBleeding())
			{
				return true;
			}
		}
	}

	return false;
}


/*
* checks if claymore mine is available and the bot can change weapons now
*/
void BotUseClaymoreMine(bot_t* pBot)
{
	return;
}


/*
* handles various ways of communication with other clients in the game
*/
void BotCommunicateWithOthers(bot_t* pBot)
{
	// can the bot use this form of communication at all?
	if (externals.GetDontSpeak() == false)
	{
		switch (pBot->GetVoiceCommandToUse())
		{
			case voiceCmd::area_clear:
				util.Voice(pBot->pEdict, voiceCmd::area_clear);
				break;
			case voiceCmd::coverme:
				util.Voice(pBot->pEdict, voiceCmd::coverme);
				break;
			case voiceCmd::enemy_ahead:
				util.Voice(pBot->pEdict, voiceCmd::enemy_ahead);		// not using it anymore, because it doesn't seem to work correctly - the hand signaling randomly points to wrong directions
				break;
			case voiceCmd::fire_in_the_hole:
				util.Voice(pBot->pEdict, voiceCmd::fire_in_the_hole);
				break;
			case voiceCmd::grenade:
				util.Voice(pBot->pEdict, voiceCmd::grenade);
				break;
			case voiceCmd::handsig_negative:
				util.HandSignal(pBot->pEdict, "signal_no");
				break;
			case voiceCmd::handsig_yes_sir:
				util.HandSignal(pBot->pEdict, "signal_yes");
				break;
			case voiceCmd::negative:
				util.Voice(pBot->pEdict, voiceCmd::negative);
				break;
			case voiceCmd::yes_sir:
				util.Voice(pBot->pEdict, voiceCmd::yes_sir);
				break;
		}
	}

	// make bot remember the moment when he spoke last
	pBot->SetSpeakTime();
	// and remove the task, because the bot has just spoke or used the signal
	pBot->RemoveTask(TASK_SPEAK);

	return;
}


/*
* bot checks if the sound is made by his teammate if not then turns to that direction
*/
bool bot_t::UpdateSounds(edict_t* pPlayer)
{
	float distance;
	static bool check_footstep_sounds = TRUE;
	static float footstep_sounds_on;
	float volume;
	Vector v_sound;

	// ignore sounds when on ladder (just for sure)
	if (pEdict->v.movetype == MOVETYPE_FLY)
		return false;

	// update sounds made by this player, alert bots if they are nearby
	if (check_footstep_sounds)
	{
		check_footstep_sounds = FALSE;
		footstep_sounds_on = CVAR_GET_FLOAT("mp_footsteps");
	}

	if (footstep_sounds_on > 0.0f)
	{
		// check if this player is moving fast enough to make sounds
		if (pPlayer->v.velocity.Length2D() > 180.0f)
		{
			volume = 500.0f;  // volume of sound being made (just pick something)

			// is possible enemy really an enemy (ie not in same team)
			if (!util.AreTeammates(pEdict, pPlayer))
			{
				v_sound = pPlayer->v.origin - pEdict->v.origin;

				distance = v_sound.Length();
			}
			// otherwise it's your teammate so set max distance which will prevent facing him
			else
			{
				distance = 9999.0f;
			}

			// is the bot close enough to hear this sound?
			if (distance < (volume * g_sound_sensitivity[GetBotSkill()]))
			{
				// limit facing the source of the noice while the bot is moving
				// we don't want the bot to turn back and/or sides while he's advancing forward
				if (pEdict->v.velocity.Length2D() > 50.0f)
				{
					bool HeadVisible = util.IsVisible(pPlayer->v.origin + pPlayer->v.view_ofs, pEdict);
					bool BodyVisible = util.IsVisible(pPlayer->v.origin, pEdict);

					if (!HeadVisible && !BodyVisible)
					{
						//@@@@@@@@@@@@@@@@
						//#ifdef _DEBUG
						//ALERT(at_console, "%s can hear enemy, but can't see it\n", name);
						//#endif

						return false;
					}

				}

				// is bot using bipod AND the sound is NOT in direction bot is looking
				if (IsTask(TASK_BIPOD) && (util.IsInViewCone(&v_sound, pEdict) == false))
					return false;

				Vector bot_angles = UTIL_VecToAngles(v_sound);

				pEdict->v.ideal_yaw = bot_angles.y;

				BotFixIdealYaw(pEdict);

				return true;
			}
		}
	}

	return false;
}


/*
* returns true if the bot doesn't have to aim at important point/entity and so we can allow the bot to look straight forward ie. set his pitch angle to zero
*/
bool bot_t::CanResetPitch(void)
{
	return ((IsTask(TASK_IGNOREAIMWPTS) == false) && (IsSubTask(ST_FACEGENT_DONE) == false) && (IsSubTask(ST_FACEPOINTIS_DONE) == false) && (IsTurningToFaceGEnt() == false) &&
		(wptmanager.IsWaypointTypeTeamPriority(GetCurrentAimWaypoint(), WptT::aim, 1, GetBotTeam()) == false));
}


/*
* makes the bot face the pGameEntity entity
*/
bool bot_t::FaceGameEntity(void)
{
	// called in mistake? so don't continue
	if (IsTask(TASK_IGNOREAIMWPTS) == false)
		return false;

	// bot has no GameEntity yet?
	if (HasNoGEnt())
	{
		// has the bot valid coordinates of some point in the game world? then use them
		if (GetPositionOfPointInSpace() != g_vecZero)
			return false;

		// otherwise try to look if there is any breakable object around
		util.CheckForBreakableAround(this, EXTENDED_SEARCH_RADIUS);

		// still nothing?
		if (HasNoGEnt())
		{
#ifdef _DEBUG
			char msg[TEXT_MSG_SIZE]{};
			sprintf(msg, "FaceGameEntity() - pointer to GEnt (GameEntity) is NULL\n");
			conOutput.Notify(msg, this);
			util.DebugDev(msg, curr_wpt_index, curr_path_index);
#endif
			RemoveTask(TASK_IGNOREAIMWPTS);

			return false;
		}
	}

	Vector v_entity;
	Vector entity_origin = util.GetEntityOrigin(GetPointerToGEnt());
	float view_cone = 0.95f;

	// some Capture Areas had problems with the default very narrow view cone, bot couldn't get them in it, so we will make the check a bit loose for them
	// also some breakable objects (eg. window on upper level or sewer/vent cover straight up above the bot) needs to loosen the limits
	if (IsPointerToGEntThisEntity("dod_capture_area") || (IsPointerToGEntThisEntity("func_breakable") && ((pEdict->v.origin.z + (pEdict->v.size.z * 2.0f)) < entity_origin.z)))
		view_cone = 0.85f;

	// the GameEntity is currently in FOV (very tight cone here) so don't try to face it
	if (util.IsInNarrowViewCone(&entity_origin, pEdict, view_cone))
		SetTimeToFaceGEnt(-0.1f);
	// otherwise set time to start facing it
	else
		SetTimeToFaceGEnt(0.1f);
			
	// don't do anything when the bot isn't facing it yet, we just need to prevent the bot to act (eg. start shooting)
	if (IsTurningToFaceGEnt())
	{
		;
	}
	// otherwise the bot is facing it so he can act now (eg. start shooting)
	else
	{
		RemoveTask(TASK_IGNOREAIMWPTS);
		SetSubTask(ST_FACEGENT_DONE);
	}

	// we must keep the object in sight and looking at it at correct angle...

	if (IsSubTask(ST_RANDOMCENTRE))
	{
		// if the object is really close then offset quite a lot to deal with the hole in the centre of the object
		if ((entity_origin - pEdict->v.origin).Length() < 50.0f)
			entity_origin = entity_origin + Vector((RANDOM_LONG(1, 30) - 15), (RANDOM_LONG(1, 30) - 15), (RANDOM_LONG(1, 30) - 15));
		else
			entity_origin = entity_origin + Vector((RANDOM_LONG(1, 10) - 5), (RANDOM_LONG(1, 10) - 5), (RANDOM_LONG(1, 10) - 5));
	}

	if (IsSubTask(ST_USEEYESORIGIN))
		v_entity = entity_origin - (pEdict->v.origin + pEdict->v.view_ofs);
	else
		v_entity = entity_origin - pEdict->v.origin;

	Vector bot_angles = UTIL_VecToAngles(v_entity);

	pEdict->v.idealpitch = -bot_angles.x;
	BotFixIdealPitch(pEdict);
	pEdict->v.ideal_yaw = bot_angles.y;
	BotFixIdealYaw(pEdict);

	return true;
}


/*
* returns the distance to pGameEntity
* handles both the game entity with zero origin as well as null pointer to game entity
*/
float bot_t::GetDistanceToGEnt(void)
{
	if (pGameEntity != NULL)
	{
		Vector ge_origin;

		if (pGameEntity->v.origin == g_vecZero)
			ge_origin = util.VecBModelOrigin(pGameEntity);
		else
			ge_origin = pGameEntity->v.origin;

		return (ge_origin - pEdict->v.origin).Length();
	}

	return 9999.0f;
}


/*
* makes the bot face the coordinates given by Point In Space
*/
void bot_t::FacePointInSpace(void)
{
	// called in mistake? so don't continue
	if (IsTask(TASK_IGNOREAIMWPTS) == false)
		return;

	// bot doesn't have any coordinates?
	if (GetPositionOfPointInSpace() == g_vecZero)
	{
#ifdef _DEBUG
		char msg[TEXT_MSG_SIZE]{};
		sprintf(msg, "FacePointInSpace() - no coordinates!\n");
		conOutput.Notify(msg, this);
		util.DebugDev(msg, curr_wpt_index, curr_path_index);
#endif
		RemoveTask(TASK_IGNOREAIMWPTS);

		return;
	}

	Vector v_entity;
	float view_cone = 0.95f;

	// in certain cases (ie. bot is quite close and the angle to the coords is high) the limits have to be loosen a bit otherwise the bot wouldn't be able to get the coordinates into the original view cone
	if (targeting_the_aim_stop > 250)
	{
		// needed to prevent getting the bot stuck in targeting loop forever
		RemoveTask(TASK_IGNOREAIMWPTS);
		SetSubTask(ST_FACEPOINTIS_DONE);

#ifdef DEBUG
		conOutput.Notify("FacePointInSpace() -> Number of tries to target it EXCEEDED 250 -->> BREAK!!!\n", this);
#endif // DEBUG

		return;
	}
	else if (targeting_the_aim_stop > 150)
		view_cone = 0.75f;
	else if (targeting_the_aim_stop > 75)
		view_cone = 0.85f;

	// are the coordinates already in quite narrow FOV?
	if (util.IsInNarrowViewCone(GetPointerToPositionOfPointInSpace(), pEdict, view_cone))
		SetTimeToFaceGEnt(-0.1f);
	// otherwise set time to start facing them
	else
		SetTimeToFaceGEnt(0.1f);

	// don't do anything when the bot isn't facing the coordinates yet, we just need to prevent the bot to act (eg. start shooting)
	if (IsTurningToFaceGEnt())
	{
		// safety stop for the cases when the bot cannot face the coords (ie. get that point in a narrow view cone)
		targeting_the_aim_stop++;
	}
	// otherwise the bot is facing them so he can act now (eg. start shooting)
	else
	{
		RemoveTask(TASK_IGNOREAIMWPTS);
		SetSubTask(ST_FACEPOINTIS_DONE);
	}

	if (IsSubTask(ST_USEEYESORIGIN))
		v_entity = GetPositionOfPointInSpace() - (pEdict->v.origin + pEdict->v.view_ofs);
	else
		v_entity = GetPositionOfPointInSpace() - pEdict->v.origin;

	Vector bot_angles = UTIL_VecToAngles(v_entity);

	pEdict->v.idealpitch = -bot_angles.x;
	BotFixIdealPitch(pEdict);
	pEdict->v.ideal_yaw = bot_angles.y;
	BotFixIdealYaw(pEdict);
}


/*
* resets all aim waypoint slots as well as current aiming stage
*/
void bot_t::ResetAims(const char* loc)
{
	Aims.Clear();
	curr_aim_index = NO_VAL;
	time_to_keep_current_aim = 0.0f;
	time_of_adjusting_the_aim = 0.0f;
	targeting_the_aim_stop = 0;

	RemoveSubTask(ST_AIM_DONE);
	RemoveSubTask(ST_FACEGENT_DONE);
	RemoveSubTask(ST_FACEPOINTIS_DONE);
	RemoveSubTask(ST_USEEYESORIGIN);
	RemoveTask(TASK_IGNOREAIMWPTS);
	RemoveTask(TASK_PRECISEAIM);

	// DoD specific
	RemoveSubTask(ST_MEDEVAC_ST);
	RemoveSubTask(ST_MEDEVAC_H);
	RemoveSubTask(ST_MEDEVAC_F);

#ifdef _DEBUG
	if (botdebugger.IsDebugActions() || botdebugger.IsDebugAims())
	{
		if (loc != NULL)
		{
			char msg[256]{};
			sprintf(msg, "ResetAims() called @ %s\n", loc);
			conOutput.Notify(msg, this);
		}

		char msg[256]{};
		sprintf(msg, "*** aiming marks were cleared (curr wpt index %d)\n", curr_wpt_index + 1);
		conOutput.Notify(msg, this);
	}
#endif
}


/*
* manages targeting aim waypoints
*/
void bot_t::TargetAimWaypoint(const char* loc)
{
	// is bot tasked to ignore any aim waypoint OR currently using OR deploying/folding a bipod? then break it right here
	if (IsTask(TASK_IGNOREAIMWPTS) || IsSubTask(ST_FACEGENT_DONE) || IsSubTask(ST_FACEPOINTIS_DONE) || IsTask(TASK_BIPOD) || (IsNotDeployingBipod() == false))
	{


#ifdef DEBUG																	// NEW CODE 094 Test
		//if (loc != NULL)
		{
			char msg[TEXT_MSG_SIZE]{};
			sprintf(msg, "TargetingTheAim @ AimWpt_index=%d -> IgnoreAims OR FaceItemDone OR TaskBipod OR HandlingBipod -->> BREAK!!!\n", Aims.Print());
			conOutput.Notify(msg, this);
		}
#endif // DEBUG


		RemoveSubTask(ST_AIM_FACEAIMWPT);
		return;
	}


	int local_wpt = curr_wpt_index;

	if (local_wpt == NO_VAL)
	{
		// we have to prevent false error printing in cases where the bot has been given the order to hold a position via the voice command
		if (IsTask(TASK_IGNOREWPTNAV) == false)
		{
			// public debugging
			char msg[256]{};
			sprintf(msg, "TargetAimWaypoint() - local/current wpt is -1 (previous wpt is #%d, path is #%d)\n Possible reason: there's no goback wpt at the end of the path and the path doesn't end at cross (ie. wrongly set camping spot)\n",
				prev_wpt_index.get(), curr_path_index);
			util.DebugInFile(msg);

			if (botdebugger.IsDebugActions() || botdebugger.IsDebugAims() || botdebugger.IsDebugWaypoints())
				conOutput.Notify(msg);
		}

		// set it so like we succeeded even if there aren't any waypoints/targets
		RemoveSubTask(ST_AIM_FACEAIMWPT);
		SetSubTask(ST_AIM_DONE);

		return;
	}



#ifdef DEBUG																	// NEW CODE 094 Test
	if (loc != NULL)
	{
		char msg[TEXT_MSG_SIZE]{};
		sprintf(msg, "TargetingTheAim() for wpt=%d called @ %s\n", local_wpt + 1, loc);
		conOutput.Notify(msg, this);
	}
#endif // DEBUG



	if (botdebugger.IsDebugAims())
	{
		static char prev_msg[TEXT_MSG_SIZE]{};
		char msg[TEXT_MSG_SIZE]{};

		sprintf(msg, "TargetingTheAim - CurrentTarget=%d || AimWpt1=%d | AimWpt2=%d | AimWpt3=%d | AimWpt4=%d\n", GetCurrentAimWaypoint() + 1, Aims.Print(0), Aims.Print(1), Aims.Print(2), Aims.Print(3));

		// don't print the message every frame
		if (strcmp(prev_msg, msg) != 0)
		{
			conOutput.Notify(msg, this);
			strcpy(prev_msg, msg);
		}

		// but once in a while allow repeating this message
		if (RANDOM_LONG(1, 100) > 99)
			prev_msg[0] = 0;
	}

	// if the array of aims is empty then fill it
	if (Aims.IsEmpty())
		wptmanager.FindAimingWaypointsForBot(this, local_wpt);

	// are there no aim waypoints around?
	if (Aims.IsEmpty())
	{
		// then use next waypoint from the path as the aim target
		Aims.AddNewAimWpt(GetNextWaypointOnPath());

		// if even that failed then there really is nothing to target...
		if (Aims.IsEmpty())
		{
			// so reset things and quit
			RemoveSubTask(ST_AIM_FACEAIMWPT);
			SetSubTask(ST_AIM_DONE);

			if (botdebugger.IsDebugAims())
			{
				char msg[TEXT_MSG_SIZE]{};
				sprintf(msg, "TargetingTheAim for waypoint #%d -> NOTHING to aim at (AimWpt1=%d) -->> STOP IT!!!\n", local_wpt + 1, Aims.Print());
				conOutput.Notify(msg, this);
			}

			return;
		}

		if (botdebugger.IsDebugAims() || botdebugger.IsDebugWaypoints())
		{
			char msg[TEXT_MSG_SIZE]{};
			sprintf(msg, "<<WARNING>> There are no aim waypoints around! Using the next path waypoint #%d as the target\n", Aims.Print());
			conOutput.Notify(msg, this);
		}
	}

	// the bot has no aim "vector" yet, this statement must be first
	if (IsSubTask(ST_AIM_GETAIMWPT))
	{
		int new_target;

		// is there only one aim waypoint around?
		// using this simple check for validity is faster than counting them
		if (Aims.Get(1) == NO_VAL)
			new_target = Aims.Get(0);
		else
		{
			// prepare new target
			new_target = GetCurrentAimWaypoint();

			// randomly pick a new aim waypoint than isn't the one the bot currently aims at
			while (new_target == GetCurrentAimWaypoint())
			{
				new_target = Aims.GetRandom();
			}
		}

		// set the new aim waypoint as current target
		SetCurrentAimWaypoint(new_target);

		// is there a priority 1 set on this aim waypoint? then always look through the eyes
		if (wptmanager.IsWaypointTypeTeamPriority(new_target, WptT::aim, 1, GetBotTeam()))
			SetSubTask(ST_USEEYESORIGIN);

		RemoveSubTask(ST_AIM_GETAIMWPT);
	}

	// face the bot towards current aim waypoint
	if (IsSubTask(ST_AIM_FACEAIMWPT))
	{
		// no aim "vector" yet?
		if (GetCurrentAimWaypoint() == NO_VAL)
		{
			// go get some
			SetSubTask(ST_AIM_GETAIMWPT);
			// but we'll do it in next game frame
			return;
		}

		Vector v_aim;
		Vector aim_angles;

		// if the bot isn't able to precisely target this aim waypoint then just reset it back to where it should be and break it
		if (targeting_the_aim_stop >= 250)
		{
			v_aim = waypoints[GetCurrentAimWaypoint()].origin - waypoints[curr_wpt_index].origin;
			aim_angles = UTIL_VecToAngles(v_aim);
			pEdict->v.ideal_yaw = aim_angles.y;
			BotFixIdealYaw(pEdict);

			RemoveSubTask(ST_AIM_FACEAIMWPT);
			SetSubTask(ST_AIM_DONE);

			// remove this one as well, bot isn't able to handle it
			RemoveTask(TASK_PRECISEAIM);

			// send this problem to debug file
			char msg[TEXT_MSG_SIZE]{};
			sprintf(msg, "(WAYPOINT BUG) Bot isn't able to target aim waypoint #%d from waypoint #%d! Try reducing the range on \"master\" waypoint.\n", GetCurrentAimWaypoint() + 1, curr_wpt_index + 1);
			util.DebugInFile(msg);

			if (botdebugger.IsDebugAims() || botdebugger.IsDebugWaypoints())
				conOutput.Notify(msg);


#ifdef DEBUG																	// NEW CODE 094 Test
			char tmsg[TEXT_MSG_SIZE]{};
			sprintf(tmsg, "TargetAimWpt() for AIM wpt #%d -> Number of tries to target aim EXCEEDED 250 -->> BREAK!!!\n", GetCurrentAimWaypoint() + 1);
			conOutput.Notify(tmsg, this);
#endif // DEBUG


			return;
		}

		// the bot is forced to face the aim waypoint before setting the done flag
		if (IsTask(TASK_PRECISEAIM))
		{
			// keep facing current aim waypoint
			if (IsTimeToKeepCurrentAim())
			{
				Vector target_origin = waypoints[GetCurrentAimWaypoint()].origin;

				// does the bot need to look through his eyes on this target? then we have to simulate the position of the eyes on the waypoint to get correct aiming vector
				if (IsSubTask(ST_USEEYESORIGIN))
					v_aim = target_origin - (waypoints[curr_wpt_index].origin + Vector(0, 0, 22));
				else
					v_aim = target_origin - waypoints[curr_wpt_index].origin;
				
				aim_angles = UTIL_VecToAngles(v_aim);

				pEdict->v.ideal_yaw = aim_angles.y;
				BotFixIdealYaw(pEdict);

				if (IsSubTask(ST_USEEYESORIGIN))
				{
					// in this case use also its height ie. bot has to aim up or down based on the position of current aim waypoint
					pEdict->v.idealpitch = -aim_angles.x;
					BotFixIdealPitch(pEdict);
				}

				// has the bot trouble targeting the aim waypoint?
				if (targeting_the_aim_stop > 50)
				{
					float waypoint_range = waypoints[curr_wpt_index].range;

					if (waypoint_range < WPT_RANGE)
						waypoint_range = WPT_RANGE;

					// then virtually shift the aim waypoint farther away from the master waypoint (ie. current waypoint the bot is standing by)
					// this virtual target will be in the same direction (same aim vector) as the aim waypoint that the bot cannot target correctly,
					// but the distance between this virtual target and current waypoint will be 3 times the range of current waypoint (ie. quite far away),
					// this should be far enough to allow the bot see this virtual aim waypoint in order to confirm he is facing the right direction
					target_origin = waypoints[curr_wpt_index].origin + (v_aim.Normalize() * (waypoint_range * 3.0f));
					// keep the virtual target on the same height level as master (current) waypoint
					target_origin.z = waypoints[curr_wpt_index].origin.z;

					if (botdebugger.IsDebugAims())
					{
						char msg[TEXT_MSG_SIZE]{};
						sprintf(msg, "TargetingTheAim for AIM wpt #%d -> Task PRECISE AIM -> readjusting the target position\n", GetCurrentAimWaypoint() + 1);
						conOutput.Notify(msg, this);
					}
				}

				// safety stop for the cases when the bot cannot face current aim waypoint (get it in a narrow view cone)
				targeting_the_aim_stop++;

				// we know that the time of adjusting the aim holds the moment when we started facing this (current) aim waypoint
				// so if this statement gets true we know the bot has already been adjusting his yaw angles for over half a second (turn speed is limited so it takes some time)
				// also we can't go much higher, because 1 second is the minimal time people can set as wait time and if we exceed it, the bot won't be able to shoot at such waypoint or act at all
				if (GetTimeOfAdjustingAim() + 0.5f < gpGlobals->time)
				{
					// so now we can check if the bot is really facing the aim waypoint by checking if it is in his quite narrow view cone
					// if there's a combination of waypoint with large range and an aim waypoint that is really close to it then bot standing on the edge of the range may not be able to get the aim waypoint
					// into this narrow view cone, the virtual shift of the aim waypoint should deal with such case, but if that fail too then the safety stop will break the loop and bot will be able to act
					if (util.IsInNarrowViewCone(&target_origin, pEdict, 0.7f))
					{
						RemoveSubTask(ST_AIM_FACEAIMWPT);
						SetSubTask(ST_AIM_DONE);

						if (botdebugger.IsDebugAims())
						{
							char msg[TEXT_MSG_SIZE]{};
							sprintf(msg, "TargetingTheAim for AIM wpt #%d -> Task PRECISE AIM -> in narrow view cone DONE\n", GetCurrentAimWaypoint() + 1);
							conOutput.Notify(msg, this);
						}

						return;
					}
				}
			}
			// otherwise set time to allow the bot to face the aim target
			else
			{
				SetTimeToKeepCurrentAim(RANDOM_FLOAT(2.0f, 4.0f));
				SetTimeOfAdjustingAim();

				return;
			}
		}
		// otherwise just turn towards it
		else
		{
			if (IsSubTask(ST_USEEYESORIGIN))
				v_aim = waypoints[GetCurrentAimWaypoint()].origin - (waypoints[curr_wpt_index].origin + Vector(0, 0, 22));
			else
				v_aim = waypoints[GetCurrentAimWaypoint()].origin - waypoints[curr_wpt_index].origin;
			
			aim_angles = UTIL_VecToAngles(v_aim);

			// some off-set
			aim_angles.y += RANDOM_LONG(0, 10) - 5;

			pEdict->v.ideal_yaw = aim_angles.y;
			BotFixIdealYaw(pEdict);

			if (IsSubTask(ST_USEEYESORIGIN))
			{
				pEdict->v.idealpitch = -aim_angles.x;
				BotFixIdealPitch(pEdict);
			}

			RemoveSubTask(ST_AIM_FACEAIMWPT);
			SetSubTask(ST_AIM_DONE);

			if (botdebugger.IsDebugAims())
			{
				char msg[TEXT_MSG_SIZE]{};
				sprintf(msg, "TargetingTheAim for AIM wpt #%d -> face it DONE\n", GetCurrentAimWaypoint() + 1);
				conOutput.Notify(msg, this);
			}
		}
	}

	// set the watch time
	if (IsSubTask(ST_AIM_SETTIME))
	{
		int aim_count = Aims.Count();

		// if there are more aim waypoints divide the whole wait time between them
		if (aim_count > 1)
		{
			float the_time = wptmanager.GetWaypointWaitTime(local_wpt, GetBotTeam());

			// if there is no wait time on the waypoint (bot is waiting for some special command such as wait until something respawns) then generate some wait time
			if (the_time == 0.0f)
				the_time = RANDOM_FLOAT(2.0f, 5.0f);

			// see how many times the bot should change aim waypoint (i.e. the longer the wait time is the more changes are there)
			if ((the_time > 25.0f) && (the_time < 50.0f))
				aim_count = RANDOM_LONG(4, 8);
			else if ((the_time > 50.0f) && (the_time < 120.0f))
				aim_count = RANDOM_LONG(8, 12);
			else if (the_time > 120.0f)
				aim_count = RANDOM_LONG(12, 24);

			the_time /= (float)aim_count;

			SetTimeToKeepCurrentAim(the_time);
			SetTimeOfAdjustingAim();
		}
		// otherwise target the only aim waypoint and keep it for the whole wait time
		else if (aim_count == 1)
		{
			SetTimeToKeepCurrentAim(GetWaitTime());
			SetTimeOfAdjustingAim();
		}
		// there are no aim waypoints around
		else if (aim_count == 0)
		{


			/// TODO: Change this, probably use the ignoreaim task and the bot will then forget about any aim wpts targeting and use the lft, right... facing idea

			// NOTE: this should "somehow" be changed to fill bot->aim_indexes with all four directions (forward, left, backward, right) and sniper time = waittime / 4



			// for now do target previous waypoint

			Aims.AddNewAimWpt(prev_wpt_index.get());

			SetTimeToKeepCurrentAim(GetWaitTime());
			SetTimeOfAdjustingAim();
		}

		RemoveSubTask(ST_AIM_SETTIME);

		return;
	}

	// make sure the bot is really watching current aim
	if (IsSubTask(ST_AIM_ADJUSTAIM))
	{
		// check it again in 2.5 sec
		SetTimeOfAdjustingAim(2.5f);

		SetSubTask(ST_AIM_FACEAIMWPT);
		RemoveSubTask(ST_AIM_ADJUSTAIM);
	}
}


/*
* handles several things when the bot gets to waypoint with some wait time
*/
void bot_t::BotWaitHere()
{
	// is bot following a team leader?
	if (pTeamLeader != NULL)
	{
		// then prevent him to look for any waypoint till the moment given order is finished and gets reset
		// we do check for 0.5 seconds delay after wait time is over before anything gets reset in the clearing section so setting 1.0 second is safe value
		SetDontLookForWaypoint(1.0f);
	}
	else
	{
		SetDontLookForWaypoint();

		// is the bot going to wait at door waypoint? then don't update the waypoint reach time, because the bot may get stuck at door and wouldn't be able to get free at all
		if (wptmanager.IsWaypoint(curr_wpt_index, WptT::door, WptT::dooruse) == false)
			SetTimeToReachCurrWaypoint();
	}

	// if the bot is moving sideways then he is still in process of finding a free place to wait on this waypoint (i.e. someone else is blocking him)
	if (DoesBotStrafeNow())
	{
		// so we must update the wait time to postpone the wait action for the moment when the bot is finally positioned correctly
		SetWaitTime( GetWaitTime() - GetPreviousGlobalsTime() );
		return;
	}
	
	SetMoveSpeed(MoveSpeed::stop);

#ifdef DEBUG
	SetDontCheckStuck();
	//SetDontCheckStuck("Bot WaitHere()");
#else
	SetDontCheckStuck();
#endif // DEBUG
	
	// DoD specific waiting so that the bot can drop the box with extra ammo for his teammate
	if (IsTask(TASK_MEDEVAC))
	{
		// didn't bot drop the ammo yet AND is he facing his teammate?
		if ((IsWeaponStatus(WS_DROPAMMO) == false) && IsSubTask(ST_FACEGENT_DONE))
		{
			// then make him drop the box with extra ammo
			FakeClientCommand(pEdict, "dropammo", NULL, NULL);

			// and set the weapon status so we know this bot doesn't have any extra ammo anymore
			SetWeaponStatus(WS_DROPAMMO);
		}

		// otherwise do nothing, just wait for a while so the teammate can pick up the box in cases when the box was in a direction in which this bot needs to move
	}
	
	// is the bot waiting until door opens
	else if (IsSubTask(ST_DOOR_OPEN) && wptmanager.IsWaypoint(curr_wpt_index, WptT::door, WptT::dooruse))
	{
		// there are no doors at all ... forget about it
		if (GetPositionOfPointInSpace() == g_vecZero)
		{

#ifdef DEBUG
			//@@@@@@@@@@@@@@@@
			conOutput.Notify("***Waiting at the door - !!! NO DOORS (doorposition is vecZero) !!! -> leaving\n", this);
#endif

			RemoveSubTask(ST_DOOR_OPEN);
			SetWaitTime(-0.2f);

			return;
		}

		// is the door open? then break the waiting and continue in navigation
		if ((HasNoGEnt() == false) && util.IsDoorOpen(GetPointerToGEnt(), IsCrouched()))
		{
			RemoveSubTask(ST_DOOR_OPEN);
			SetWaitTime(-0.2f);

			if (botdebugger.IsDebugActions())
				conOutput.Notify("***Waiting at the door -> leaving, it's open now\n", this);
		}
		else
		{
			// is current waypoint unreachable? then don't increase the wait time anymore and let the bot try to get free
			if (NotReachedCurrWaypointFor(15.0f))
				;
			else
			{
				// do increase the wait time so the bot can't run away
				SetWaitTime(1.0f);
				f_dont_avoid_wall_time = gpGlobals->time + 2.0f;
			}

			// is the bot facing the door yet?
			if (util.IsInNarrowViewCone(GetPointerToPositionOfPointInSpace(), pEdict, 0.95f))
			{
				// no pointer to door or the door isn't moving now AND is the right moment? then try touching the door to open it
				if ((HasNoGEnt() || (GetPointerToGEnt()->v.nextthink <= 0.0f)) && (RANDOM_LONG(1, 100) <= 25))	// was 5 originally
				{
					SetMoveSpeed(MoveSpeed::slow);

					if (wptmanager.IsWaypoint(curr_wpt_index, WptT::dooruse))
						pEdict->v.button |= IN_USE;

					if (botdebugger.IsDebugActions())
						conOutput.Notify("***Waiting at the door -> going to use or touch to open it\n", this);
				}

				// no pointer to door entiry OR is the door type that isn't defined in door open function? 
				if (HasNoGEnt() || util.IsEntityName(GetPointerToGEnt(), "momentary_door"))
				{
					// then use traceline to determine passability
					UTIL_MakeVectors(pEdict->v.v_angle);

					Vector v_src = pEdict->v.origin + pEdict->v.view_ofs;
					Vector v_dest = v_src + gpGlobals->v_forward * STANDARD_SEARCH_RADIUS;

					TraceResult tr;
					UTIL_TraceLine(v_src, v_dest, ignore_monsters, pEdict->v.pContainingEntity, &tr);

					// did the traceline hit a solid object (eg. wall behind the door) OR did it reach its destination? (ie. there's nothing blocking the way)
					if (util.IsEntityName(tr.pHit, "worldspawn") || (tr.flFraction == 1.0f))
					{
						RemoveSubTask(ST_DOOR_OPEN);
						SetWaitTime(-0.2f);

						if (botdebugger.IsDebugActions())
							conOutput.Notify("***Waiting at the door -> going to pass through as it opened\n", this);
					}
				}
			}
			// otherwise do face it
			else
			{
				Vector door_angle = GetPositionOfPointInSpace() - pEdict->v.origin;
				Vector bot_angles = UTIL_VecToAngles(door_angle);

				pEdict->v.ideal_yaw = bot_angles.y;
				BotFixIdealYaw(pEdict);
			}
		}
	}
	
	// is bot waiting at claymore waypoint (including the case when he already got a new waypoint in order to get away from the blast)?
	else if (wptmanager.IsWaypoint(curr_wpt_index, WptT::claymore) || (IsTask(TASK_CLAY_EVADE) && (IsTask(TASK_PARACHUTE) == false)))
	{
		// has bot any explosives charge?
		if (IsEquippedWithExplosiveCharge())
		{
			// we better update the wait time in case the bot needed to turn to the breakable object (doing so can take some time)
			SetWaitTime(5.0f);

			// now we must handle cases when the claymore waypoint is wrongly positioned and bot isn't touching the breakable object enough to plant the explosives charge so...
			// is there intact func_breakable based object near bot AND isn't he touching it yet?
			if (util.CheckForClaymoreOnlySDObjectAround(this) && (IsSubTask(ST_INAREA) == false))
			{
				// is he already facing the object and is chance?
				if (IsSubTask(ST_FACEGENT_DONE) && (RANDOM_LONG(1, 100) < 20))
				{
					// then try to move closer to plant the explosives charge
					SetMoveSpeed(MoveSpeed::slow);
				}
				// not facing the object yet AND do we have a valid pointer to this breakable entity AND is the right moment? 
				if ((IsSubTask(ST_FACEGENT_DONE) == false) && (HasNoGEnt() == false) && (RANDOM_LONG(1, 100) < 5))
				{
					// then try to face it
					SetTask(TASK_IGNOREAIMWPTS);
				}
			}
		}
		// otherwise the explosives charge must have been just planted so there's nothing to be done here anymore, bot should ignore this waypoint and move away else he may die in the blast
		else
		{
			// also we should clear everything we may have set during this action
			SetWaitTime(0.0f);


			//@@@@@@@@@@@@@@@@@@@@@@@
#ifdef _DEBUG
			if (botdebugger.IsDebugActions())
			{
				if (IsEquippedWithExplosiveCharge())
					conOutput.Notify("WaitHere()|ClaymoreWpt -> NO OBJECT at all or already been DESTROYED\n", this);
				else
					conOutput.Notify("WaitHere()|ClaymoreWpt -> explosives charge been just PLANTED or have NO CHARGE at all\n", this);
			}
#endif

		}
	}

	// is the bot waiting at shoot waypoint where is he facing the aim waypoint or the game entity he's about to shoot at OR has the bot been given order to use the bazooka via the voice command?
	else if ((wptmanager.IsWaypoint(curr_wpt_index, WptT::shoot) && (IsSubTask(ST_AIM_DONE) || IsSubTask(ST_FACEGENT_DONE))) ||
		(((IsSubTask(ST_FACEGENT_DONE) || IsSubTask(ST_FACEPOINTIS_DONE)) && IsSubTask(ST_MEDEVAC_F)) || (IsSubTask(ST_MEDEVAC_H) && (IsSubTask(ST_MEDEVAC_F) == false))))
	{
		bool dont_shoot_yet = true;

		// no ammo for either weapon?
		if (IsNoAmmoForMainWeapon() && IsNoAmmoForBackupWeapon())
		{
			// then reset wait time to return the bot back to normal navigation
			SetWaitTime(0.0f);

			if (botdebugger.IsDebugActions())
				conOutput.Notify("***Ran completely OUT OF AMMO! Breaking the wait to shoot\n", this);
		}
		// can the bot use weapon by now?
		// we have to also check whether he recovered from all reloading actions where the additional delay means including a full return back to standing stance, because
		// if we allowed calling the functions right after he finished only the reloading then he may still be in crouch and we would have had false results
		// if the shoot waypoint is also a crouch one or a prone one then this will only add a little delay so it's pointless to branch the code here just for that
		else if (IsAllowedToHandleWeapon() && ((GetWeaponReloadTime() + 0.5f) < gpGlobals->time))
		{
			// when the bot reacts to the voice command Use the bazooka then we must skip the checks for waypoint priority else we would get false results
			// if the priority is set to no priority then don't shoot
			if ((IsSubTask(ST_MEDEVAC_H) == false) && (wptmanager.GetWaypointPriority(curr_wpt_index, GetBotTeam()) == 0))
				dont_shoot_yet = true;
			// if the waypoint has priority == 1 then don't check for object existence ie. the bot will always fire
			else if ((IsSubTask(ST_MEDEVAC_H) == false) && (wptmanager.GetWaypointPriority(curr_wpt_index, GetBotTeam()) == 1))
				dont_shoot_yet = false;
			else
			{
				bool is_there_anything_to_destroy = false;

				// initialize it to 'Yes, let's shoot', but go check...
				dont_shoot_yet = false;

				// is there any breakable object in front of the bot?
				if (util.CheckForwardForBreakable(this, true))
				{
					is_there_anything_to_destroy = true;

					// we also need to set some wait time so that the bot won't run away before he destroyed all available objects
					SetWaitTime(10.0f);

					// the pointer to game entity gets assigned inside Check Forward For Breakable function only if the subsequent check finds something in the vicinity of the original target
					// (ie. the destination of the aim vector)
					if (IsPointerToGEntThisEntity("func_breakable"))
					{
						// bot has to ignore aim waypoints to successfully target the object
						SetTask(TASK_IGNOREAIMWPTS);

						// don't let the bot fire until he's facing the object
						if (IsSubTask(ST_FACEGENT_DONE) == false)
							dont_shoot_yet = true;
					}
					// also don't shoot until the bot is properly facing the coordinates given by teammate
					else if (IsSubTask(ST_MEDEVAC_F) && (IsSubTask(ST_FACEPOINTIS_DONE) == false))
						dont_shoot_yet = true;
				}
				// wasn't the first check successful AND is there no aim waypoint around AND bot doesn't have any coordinates to shoot at from teammate?
				// (ie. there's nothing in front of him and bot is free to look around) then let's check the space around the bot for any breakable object
				else if ((wptmanager.IsWaypoint(curr_aim_index, WptT::aim) == false) && (IsSubTask(ST_MEDEVAC_F) == false) && util.CheckForBreakableAround(this, EXTENDED_SEARCH_RADIUS))
				{
					is_there_anything_to_destroy = true;
					SetWaitTime(10.0f);
					SetTask(TASK_IGNOREAIMWPTS);

					if (IsSubTask(ST_FACEGENT_DONE) == false)
						dont_shoot_yet = true;
				}

				if (is_there_anything_to_destroy == false)
				{
					// there's completely nothing so set it to NOT shoot and leave
					dont_shoot_yet = true;
					SetWaitTime(0.0f);
					// also clear the stuff related to "Use the bazooka" command if current actions are based on it because there is nothing to use the bazooka to (this's needed when bot follows team leader)
					if (IsSubTask(ST_MEDEVAC_H))
					{
						ResetAims("WaitHere()|UseBazookaCmd -> nothing to shoot at");
					}

					if (botdebugger.IsDebugActions())
						conOutput.Notify("***NO breakable object! Breaking the wait to shoot\n", this);
				}
			}
		}
		
		// can the bot shoot at the breakable AND is ready to to use weapon now?
		if ((dont_shoot_yet == false) && IsAllowedToHandleWeapon() && IsNotGoingProne())
		{
			// check if the weapon is empty
			if (IsCurrentWeaponEmpty())
			{
				dont_shoot_yet = true;

				// if the bot has any magazines for this weapon then reload it
				if (current_weapon.iAmmo1 != 0)
					ReloadWeapon("WaitHere() to shoot -> weapon is empty");
				else
					DecideNextWeapon("WaitHere() to shoot -> weapon is empty");
			}

			// if the bot is sniper he should zoom in, because unscoped accuracy of sniper rifle is really awful
			else if (IsSniperRifle(current_weapon.iId, pEdict->v.playerclass) && (pEdict->v.fov != ZOOM_1X))
			{
				pEdict->v.button |= IN_ATTACK2;
				f_shoot_time = gpGlobals->time + 1.0f;
				dont_shoot_yet = true;

				if (botdebugger.IsDebugActions())
				{
					conOutput.Notify("***Sniper rifle zoom in when at shoot waypoint\n", this);
				}
			}

			// can the breakable object be destroyed only by explosives? ... means this is a bot with a rocket/grenade launcher and this shoot waypoint is kind of a goal for him
			else if (IsSubTask(ST_MEDEVAC_ST))
			{
				// but bot has no ammo for the rocket/grenade launcher so break the action, because he can't do anything here
				// this has to be here else the bot will switch to pistol and will try to break it with pistol which won't work on Cromwell tanks at all for example
				if (IsNoAmmoForMainWeapon())
				{
					dont_shoot_yet = true;
					SetWaitTime(0.0f);

					if (botdebugger.IsDebugActions())
						conOutput.Notify("***NO rockets/grenades! Breaking the wait to shoot\n", this);
				}

				// but rocket/grenade launcher isn't currently selected weapon?
				else if (IsUsedWeaponMain() == false)
				{
					SetWeaponStatus(WS_DONTSWITCHTOOTHER);
					dont_shoot_yet = true;
				}

				// but RPG isn't in ready to fire mode (ie. not on shoulder) yet?
				else if (IsWeaponSecondaryModeActive() == false)
				{
					BotSwitchGrenadeLauncherFireMode(this, "WaitHere() to shoot -> launcher not ready to fire");
					dont_shoot_yet = true;
				}
			}

			// is it time to fire the weapon now?
			if (dont_shoot_yet == false)
			{
				if (IsUsedWeaponKnife())
				{
					SetWaitTime(0.0f);

					if (botdebugger.IsDebugActions())
						conOutput.Notify("***Have just a knife! Breaking the wait to shoot\n", this);
				}
				// makes bot switch to "safe" weapon first in case he is holding dangerous explosives right now and the object is quite close (a rare occurrence in DoD, but still better to check)
				else if (BotSelectWeaponToDestroyThisBreakable(this, true) == false)
					BotFireWeaponOutOfCombat(this);
			}
		}
	}
	// is it DoD specific Area capture of control point?
	else if (IsTask(TASK_PARACHUTE))
	{
		bool can_capture = false;

		// bot should be within the area so do a simple check first
		if (util.IsCaptureAreaNearby(this, STANDARD_SEARCH_RADIUS) && util.CanBotCaptureTheArea(this))
			can_capture = true;
		// if things failed try larger area, but then a direct visibility is tested
		else if (util.IsCaptureAreaNearby(this, TEAMMATE_SEARCH_RADIUS) && util.CanBotCaptureTheArea(this))
			can_capture = true;

		if (can_capture)
		{
			// then keep increasing the wait time
			SetWaitTime(3.0f);


#ifdef _DEBUG
			//conOutput.Notify("Wait() -> WaitToCap\n",this);
#endif



			// is bot currently not inside the control point area? (was moving too fast and/or teammate showed late and bot already got past the area border)
			if (IsSubTask(ST_INAREA) == false)
			{
				// is bot facing the centre of this Area capture control point?
				if (IsSubTask(ST_FACEGENT_DONE))
				{
					// then try to move back inside
					if (IsProne() || IsCrouched())
						SetMoveSpeed(MoveSpeed::max);
					else
						SetMoveSpeed(MoveSpeed::slow);
				}
				// otherwise turn towards it
				else
				{
					SetTask(TASK_IGNOREAIMWPTS);
				}



#ifdef _DEBUG
				//conOutput.Notify("Wait() -> WaitToCap -> NOT IN AREA\n", this);
#endif



			}
		}
		else
		{
			SetWaitTime(0.0f);
			RemoveTask(TASK_PARACHUTE);



#ifdef _DEBUG
			//conOutput.Notify("Wait() -> BREAKING WaitToCap\n");
#endif


		}
	}
	// is it DoD specific Area capture of control point where is bot trying to wait for any teammate to show up?
	else if (IsSubTask(ST_PARACHUTE_USED))
	{

#ifdef _DEBUG
		//conOutput.Notify("Wait() -> WaitForTeammate\n", this);
#endif

		// keep looking around for any teammate
		if (util.IsTeammateNearby(this, TEAMMATE_SEARCH_RADIUS))
		{
			// if there is one found then stop this 'waiting if teammate shows' action
			RemoveSubTask(ST_PARACHUTE_USED);
			// and switch to 'let's capture this Area capture control point together' action
			// ie. this will prevent bot from leaving the area due to running out of wait time just when teammate finally came
			SetTask(TASK_PARACHUTE);



#ifdef _DEBUG
			//conOutput.Notify("Wait() -> WaitForTeammate -> SWITCHING to WaitToCap\n");
#endif


		}
	}
	// the bot must be waiting on standard wait command (ie. waypoint with some wait time) so we need to search for some aim waypoint in this case
	else
	{
		// is the time to watch current aim waypoint over?
		if (IsTimeToKeepCurrentAim() == false)
		{
			// force the bot to pick one of aim waypoints from his aim array
			SetSubTask(ST_AIM_GETAIMWPT);
			// reset look through eyes flag in case the new target won't be a priority 1 aiming waypoint
			RemoveSubTask(ST_USEEYESORIGIN);
			// force the bot to face it
			SetSubTask(ST_AIM_FACEAIMWPT);
			// tell the bot how long he should watch it
			SetSubTask(ST_AIM_SETTIME);
			// reset the safety counter
			targeting_the_aim_stop = 0;
		}

		// is it time to adjust aiming to aim waypoint
		if (IsTimeToKeepCurrentAim() && IsTimeToAdjustTheAim())
			SetSubTask(ST_AIM_ADJUSTAIM);

		// handle the aim waypoint targeting
		TargetAimWaypoint();

		// also check if the weapon needs to be reloaded
		if (IsNotGoingProne() && ShouldReload("WaitHere() -> wait-timed waypoint"))
		{
			ReloadWeapon("WaitHere() -> weapon is empty while waiting at wait-timed waypoint");
		}
		// is the bot already facing his current aim waypoint AND does he have a weapon with bipod attachement AND can he use it at this spot AND
		// there's quite long wait time on this waypoint AND there is still enough wait time left (we don't want to deploy it and fold it right away, because
		// the wait time is about to end and the bot must be able to leave the waypoint)
		else if (IsBipodWeapon(current_weapon.iId, pEdict->v.playerclass) && IsSubTask(ST_AIM_DONE) && (IsTask(TASK_BIPOD) == false) && (IsWeaponStatus(WS_CANTBIPOD) == false) &&
			CanDeployBipod(pEdict) && (wptmanager.GetWaypointWaitTime(curr_wpt_index, GetBotTeam()) >= 20.0f) && (gpGlobals->time + 10.0f < GetWaitTime()))
		{
			// then try deploying the bipod
			BotUseBipod(this, FALSE, "WaitHere() -> DEPLOY IT");
		}

		// break the waiting if no ammo for main weapon
		// we don't want bots (especially snipers or machine gunners) to guard some spot if they can't kill the enemy
		// do NOT break the wait time if this waypoint is either shoot or claymore or parachute or roadblock where the bot must do the action
		if (IsNoAmmoForMainWeapon() && (main_weapon != NO_VAL) && (wptmanager.IsWaypoint(curr_wpt_index, WptT::shoot, WptT::claymore, WptT::parachute) == false) &&
			(wptmanager.IsWaypoint(curr_wpt_index, WptT::roadblock) == false))
		{
			SetWaitTime(0.0f);

			if (botdebugger.IsDebugActions())
			{
				char msg[128]{};
				sprintf(msg, "***Ran OUT OF AMMO for main weapon! Breaking the wait at my current waypoint (index=%d)!\n", curr_wpt_index + 1);
				conOutput.Notify(msg);
			}
		}
	}

	// does bot have no waypoint at all?
	if (curr_wpt_index == NO_VAL)
		SetTask(TASK_IGNOREWPTNAV);// then we have to make him ignore the stance management based on waypoints
		
	// bot has to ignore stance management based on waypoints when he's tasked to ignore waypoint based navigation
	if (IsTask(TASK_IGNOREWPTNAV))
		;
	// otherwise if the bot waits at crouch waypoint then keep "crouch key" pressed down
	else if (wptmanager.IsWaypoint(curr_wpt_index, WptT::crouch))
		SetStance(GOTO_CROUCH, "WaitHere()|Waiting at crouch wpt -> GOTO crouch");
	// or is the bot waiting at prone waypoint AND is NOT lying prone yet? then go prone
	else if (wptmanager.IsWaypoint(curr_wpt_index, WptT::prone) && (IsBehaviour(BOT_PRONED) == false))
	{
		// is the bot about to go prone
		if (GoProne("WaitHere()|Waiting at prone wpt -> NOT in prone yet"))
		{
			// then we must reset don't check for stuck otherwise the bot won't be able to
			// deal with any obstacle that may block this action (e.g. someone else already laying there)
			SetDontCheckStuck("WaitHere()|Waiting at prone wpt|NOT in prone yet -> RESETTING DontCheck for STUCK!!!", -1.0f);
		}
	}
	// is bot waiting at a pushpoint waypoint to help a teammate capture it OR waiting at pushpoint whether any teammate shows around?
	// then this statement will prevent him to stand up due to not being at a crouch waypoint
	else if (IsTask(TASK_PARACHUTE) || IsSubTask(ST_PARACHUTE_USED))
		;
	else if (IsBehaviour(BOT_CROUCHED) && (wptmanager.IsWaypoint(curr_wpt_index, WptT::crouch) == false))
		SetStance(GOTO_STANDING, "WaitHere()|Bot in crouch but NOT at crouch wpt -> GOTO standing");
	
	if (botdebugger.IsDebugActions())
	{
		static char prev_msg[256] = "";
		char dmsg[256]{};

		sprintf(dmsg, "%s is waiting at waypoint #%d | prev_wpt[0] was #%d and prev_wpt[1] was #%d\n", name, curr_wpt_index + 1, prev_wpt_index.get() + 1, prev_wpt_index.get(1) + 1);

		// don't print the message every frame
		if (strcmp(prev_msg, dmsg) != 0)
		{
			ALERT(at_console, dmsg);
			strcpy(prev_msg, dmsg);
		}

		// but once in a while allow repeating this message
		if (RANDOM_LONG(1, 100) > 99)
			prev_msg[0] = '\0';
	}
	
	// is it time to check ammunition to know how many of additional mags we need
	if (NotCheckedAmmoReservesFor(5.0f))
	{
		SetTask(TASK_CHECKAMMO);
	}

	// check if we need to switch to desired weapon...
	// rocket/grenade launcher classes should keep using backup weapon
	if (IsRPG(main_weapon))
	{
		// unless they are at shoot waypoint on their class based path (ie. they are trying to destroy some special object like a tank or wall)
		if (IsWeaponStatus(WS_DONTSWITCHTOOTHER))
			UseMainWeapon("WaitHere()");
	}
	// bot gunners should switch to machinegun only if the wait time is long enough
	else if (IsMachinegun(main_weapon))
	{
		if (wptmanager.GetWaypointWaitTime(curr_wpt_index, GetBotTeam()) >= 10.0f)
			UseMainWeapon("WaitHere()");
	}
	// other classes should keep using main weapon if it is available (ie. has enough ammo for it)
	else
		UseMainWeapon("WaitHere()");
	
	UseBackupWeapon("WaitHere()");
	UseKnife("WaitHere()");
}


/*
* generates a wait time for the Hold this position command, the duration is based on bot behaviour and the weapon he uses
*/
float bot_t::GenerateHoldPositionTime(void)
{
	float hold_position_time = 0.0f;

	if (IsBehaviour(MGUNNER) || IsBehaviour(SNIPER))
	{
		if (IsBehaviour(DEFENDER))
			hold_position_time = RANDOM_FLOAT(40.0f, 90.0f);
		else
			hold_position_time = RANDOM_FLOAT(25.0f, 40.0f);
	}
	else
	{
		if (IsBehaviour(ATTACKER))
			hold_position_time = RANDOM_FLOAT(5.0f, 15.0f);
		else if (IsBehaviour(DEFENDER))
			hold_position_time = RANDOM_FLOAT(15.0f, 30.0f);
		else
			hold_position_time = RANDOM_FLOAT(10.0f, 20.0f);
	}

	return hold_position_time;
}


/*
* sets time period for which the bot won't be checking for being stuck
* if no value is set then default 1.0s is used
* 0.0 means setting it to current game time
* and using -1.0 will set the value to 0.0
*/
void bot_t::SetDontCheckStuck(const char* loc, float time_in_seconds)
{
	// we can't allow setting it if the bot is obviously in problems
	// the bot must be able to deal with any obstacle that may block going/resume from prone (e.g. someone else already laying there)
	if (IsSubTask(ST_CANTPRONE))
	{
		dont_check_stuck_time = 0.0f;

		return;
	}

	if (time_in_seconds == -1.0f)
		dont_check_stuck_time = 0.0f;
	else
		dont_check_stuck_time = gpGlobals->time + time_in_seconds;



#ifdef DEBUG

	if ((loc != NULL) && (botdebugger.IsDebugActions() || botdebugger.IsDebugStuck()))
	{
		static char last_csmsg[256] = "";

		// print it only once
		if (strcmp(loc, last_csmsg) != 0)
		{
			char msg[128]{};
			sprintf(msg, "SetDontCheckStuck() called @ %s\n", loc);
			conOutput.Notify(msg, true, this);

			strcpy(last_csmsg, loc);
		}
	}
#endif // DEBUG


}


/*
* 
*/
bool DealWithWeaponManipulation(bot_t* pBot, const char* loc)
{
	// is the bot going to check his current weapon? (do it only if the weapon is ready)
	if (pBot->IsWeaponStatus(WS_CHECKWEAPON) && pBot->IsAllowedToHandleWeapon())//															DOES NOTHING IN DOD - Leftover from FA
	{
		// bot already checked his current weapon so remove this bit in order to prevent checking the weapon over and over again
		pBot->RemoveWeaponStatus(WS_CHECKWEAPON);

		//if (botdebugger.IsDebugWeapons())
		//	conOutput.Notify("***Current weapon has been checked\n", this);

		return true;
	}

	// is some time after we've reloaded the weapon so it's time to check ammo reserves
	if ((pBot->GetWeaponReloadTime() + 1.0f > gpGlobals->time) && pBot->IsNotReloadingWeapon() && pBot->NotCheckedAmmoReservesFor(2.0f) && (pBot->IsTask(TASK_CHECKAMMO) == false))
	{
		pBot->SetTask(TASK_CHECKAMMO);

		if (botdebugger.IsDebugActions())
			conOutput.Notify("***Going to check ammunition after weapon reload\n", pBot);

		return true;
	}

	// or is some time after last ammo reserves checking
	// we must set this task just once, if the bot already has it then he'll do the check in next free moment
	// (ie. this cannot be run several frames in row otherwise the standard navigation would NOT be called at all and the bot would move strange and/or get stuck)
	if (pBot->NotCheckedAmmoReservesFor(25.0f) && (pBot->IsTask(TASK_CHECKAMMO) == false))
	{
		pBot->SetTask(TASK_CHECKAMMO);

		if (botdebugger.IsDebugActions())
			conOutput.Notify("***Regular ammunition check is needed\n", pBot);

		return true;
	}

	// for these few weapon based actions the bot needs to be allowed to manipulate with it and NOT go to/resume from prone...
	if (pBot->IsAllowedToHandleWeapon() && pBot->IsNotGoingProne())
	{
		// see if it is the right moment to reload weapon
		if (pBot->ShouldReload(loc))
		{
			pBot->ReloadWeapon(loc);

			if (botdebugger.IsDebugWeapons())
				conOutput.Notify("***Going to reload current weapon\n", pBot);

			return true;
		}

		// is bot looking through weapon optics now?
		if ((pBot->pEdict->v.fov == ZOOM_1X) && pBot->HasNoGEnt())
		{
			if (IsWeaponWithOptics(pBot->current_weapon.iId, pBot->IsBehaviour(SNIPER)))
			{
				pBot->pEdict->v.button |= IN_ATTACK2;			// scope off
				pBot->f_shoot_time = gpGlobals->time + 0.6f;	// time to take effect

				if (botdebugger.IsDebugWeapons())
				{
					conOutput.Notify("***Stopping the use of the scope on weapon with optics (no target nor combat)\n", pBot);
				}
			}

			return true;
		}

		// has bot shouldered grenade launcher?
		if (IsRPG(pBot->current_weapon.iId) && pBot->IsWeaponSecondaryModeActive())
		{
			BotSwitchGrenadeLauncherFireMode(pBot, loc);

			if (botdebugger.IsDebugWeapons())
				conOutput.Notify("***Taking the launcher off shoulder (no target nor combat)\n", pBot);

			return true;
		}
	}

	return false;
}


/*
* 
*/
bool BotManageWeaponUse(bot_t* pBot, const char* loc)
{
	// let's update ammo reserves for main weapon first
	if (pBot->CheckMainWeaponOutOfAmmo(loc))
		;
	// bot gunners and anti-armor specialists should always use backup weapon ... unless it's empty else they have to use melee
	else if (IsMachinegun(pBot->main_weapon) || IsRPG(pBot->main_weapon))
	{
		if (pBot->IsAllowedToHandleWeapon())
		{
			if (pBot->IsNoAmmoForBackupWeapon() == false)
				pBot->UseWeapon(uWeapon::backup);
			else
				pBot->UseWeapon(uWeapon::knife);
		}
	}
	// everyone else should use main weapon at all cost
	else
	{
		pBot->UseMainWeapon(loc);
	}

	// let's update ammo reserves for backup weapon
	if (pBot->CheckBackupWeaponOutOfAmmo(loc))
		;
	// is main weapon unavailable OR are we in need to switch to backup weapon then do so
	else
	{
		pBot->UseBackupWeapon(loc);
	}

	// are both main as well as backup weapons unavailable OR do we need to switch to knife then do so
	pBot->UseKnife(loc);

	return false;
}


/*
* main bot function
* all ...well almost all is directed from here
*/
void bot_t::BotThink()
{
	int index = 0;
	float pitch_degrees;
	float yaw_degrees;
	float moved_distance;      // difference between previous and current location (distance bot moved)
	TraceResult tr;
	bool found_waypoint;
	float f_strafe_speed;		// handle strafe moves

	pEdict->v.flags |= FL_FAKECLIENT;

	if (name[0] == 0)  // name filled in yet?
		strcpy(name, STRING(pEdict->v.netname));


// TheFatal - START from Advanced Bot Framework (Thanks Rich!)

	// adjust the millisecond delay based on the frame rate interval...
	if (msecdel <= gpGlobals->time)
	{
		msecdel = gpGlobals->time + 0.5f;
		if (msecnum > 0)
			msecval = 450.0f/msecnum;
		msecnum = 0;
	}
	else
		msecnum++;

	if (msecval < 1.0f)    // don't allow msec to be less than 1...
		msecval = 1.0f;

	if (msecval > 100.0f)  // ...or greater than 100
		msecval = 100.0f;
// TheFatal - END

	pEdict->v.button = 0;
	SetMoveSpeed(MoveSpeed::stop);

	f_strafe_speed = 0.0f;

	// if the bot hasn't selected stuff to start the game yet, go do that...
	if (IsBotFlag(BF_NOT_JOINED_GAME))
	{
		BotStartGame();

		g_engfuncs.pfnRunPlayerMove(pEdict, pEdict->v.v_angle, 0.0f, 0.0f, 0.0f, pEdict->v.button, 0, msecval);

		return;
	}

	// was there a problem while trying to enter/join the game with hitting the class limit? then repeat the process again
	if (IsBotFlag(BF_RESPAWN_TRY_IT_AGAIN))
	{
		SetBotFlag(BF_NOT_JOINED_GAME);
		RemoveBotFlag(BF_RESPAWN_TRY_IT_AGAIN);

		g_engfuncs.pfnRunPlayerMove(pEdict, pEdict->v.v_angle, 0.0f, 0.0f, 0.0f, pEdict->v.button, 0, msecval);

		return;
	}

	// is bot dead OR the round ended (ie all flags were captured etc.)?
	if (((pEdict->v.health < 1.0f) || (pEdict->v.deadflag != DEAD_NO)) || IsBotFlag(BF_RESPAWN_AT_ROUND_END))
	{
		if (IsBotFlag(BF_MUST_BE_INITIALIZED))
		{
			BotSpawnInit();

			// initialize stuff just once
			RemoveBotFlag(BF_MUST_BE_INITIALIZED);
		}

		if (internals.GetRoundState() == 1)
			RemoveBotFlag(BF_RESPAWN_AT_ROUND_END);

		g_engfuncs.pfnRunPlayerMove(pEdict, pEdict->v.v_angle, 0.0f, 0.0f, 0.0f, pEdict->v.button, 0, msecval);

		return;
	}

	// if the round isn't started then don't do anything ... just wait till game allows moving/doing stuff again
	// also just wait till the respawn wave is called (in case the "wait as a  spectator for next respawn" mode wasn't caught by previous 'if' statement)
	if ((internals.GetRoundState() != 1) || (pEdict->v.movetype == MOVETYPE_NONE))
	{
		g_engfuncs.pfnRunPlayerMove(pEdict, pEdict->v.v_angle, 0.0f, 0.0f, 0.0f, pEdict->v.button, 0, msecval);

		return;
	}

	// has the bot check whether his name matches the nation setting of the map yet? (also fixes wrong name when bot had to switch team)
	if (IsBotFlag(BF_NAMECHECK_DONE) == false)
	{
		// this bit gets unset automatically at bot creation which happens on a map change so we don't have to worry about it at all,
		// in other words the bot name check gets done automatically just once at the start of every map
		SetBotFlag(BF_NAMECHECK_DONE);

		char nation[16]{};

		strcpy(nation, GetNationFromBotName(name));

		// is it allied bot?
		if (GetBotTeam() == teamONE.GetTeamId())
		{
			// are we on map where are British Troops and this bot has american name OR are we on map where is US Army and this bot has british name OR did bot switch team?
			if ((internals.IsBritishTeam() && (strcmp(nation, "american") == 0)) || ((internals.IsBritishTeam() == false) && (strcmp(nation, "british") == 0)) || (strcmp(nation, "german") == 0))
			{
				BotChangeNameToMatchNation(this, nation);
			}
		}
		else if (GetBotTeam() == teamTWO.GetTeamId())
		{
			if ((strcmp(nation, "american") == 0) || (strcmp(nation, "british") == 0))
			{
				BotChangeNameToMatchNation(this, nation);
			}
		}
	}

	// set this for the next time the bot dies so it will initialize stuff
	if (IsBotFlag(BF_MUST_BE_INITIALIZED) == false)
	{
		SetBotFlag(BF_MUST_BE_INITIALIZED);
		SetBotSpawnTime();

		// see which weapons are available to the bot and use the best one
		BotSetWeaponsUsage(this);
	}

	// is the bot blinded by concussion grenade
	if (IsBlindedTime())
	{
		// if the bot is NOT proned then crouch
		if (IsProne() == false)
			pEdict->v.button |= IN_DUCK;

		// if the bot is heading to a waypoint then keep updating waypoint reach time to prevent false navigation reset
		if (curr_wpt_index != NO_VAL)
			SetTimeToReachCurrWaypoint();

		// turn towards ideal_yaw by yaw_speed degrees (slower than normal)
		BotChangeYaw( this, pEdict->v.yaw_speed / 2.0f );

		// basically don't do anything while blinded
		g_engfuncs.pfnRunPlayerMove(pEdict, pEdict->v.v_angle, ConvertMoveSpeedToRealValue(), 0.0f, 0.0f, pEdict->v.button, 0, msecval);

		return;
	}

	// is it time to check for player sounds? (only when bot is NOT on ladder AND when bot either doesn't have any enemy OR is waiting for current enemy to become visible again)
	// ignoreall completely skips this part -> we're ignoring everything
	if (IsTimeToSoundsCheck() && (botdebugger.IsIgnoreAll() == false) && (pEdict->v.movetype != MOVETYPE_FLY) && ((pBotEnemy == NULL) || (IsNotWaitingForEnemy() == false)))
	{
		int ind;
		edict_t *pPlayer;

		SetTimeOfNextSoundsCheck(1.0f);

		for (ind = 1; ind <= gpGlobals->maxClients; ind++)
		{
			pPlayer = INDEXENT(ind);

			// if this player slot is valid and it's not this bot
			if ((pPlayer) && (!pPlayer->free) && (pPlayer != pEdict))
			{
				// if observer mode enabled, don't listen to this player
				if (botdebugger.IsObserverMode() && !(pPlayer->v.flags & FL_FAKECLIENT))
					continue;

				if (util.IsAlive(pPlayer) && (FBitSet(pPlayer->v.flags, FL_CLIENT) || FBitSet(pPlayer->v.flags, FL_FAKECLIENT)))
				{
					// check for sounds being made by other players
					if (UpdateSounds(pPlayer))
					{
						// don't check for sounds for another 1.5 seconds
						SetTimeOfNextSoundsCheck(1.5f);

						// the bot needs some time to face the direction of possible enemy
						SetDontLookForWaypoint(0.5f);//												was 0.2

						// kota@ no reason to listen all players if we can turn to only one
						break;
					}
				}
			}
		}
	}

	// set to max speed
	SetMoveSpeed(MoveSpeed::max);

	moved_distance = CalculateMovedDistanceSinceLastCheck();
	
	// turn towards ideal_pitch by pitch_speed degrees
	pitch_degrees = BotChangePitch( this, pEdict->v.pitch_speed );

	// turn towards ideal_yaw by yaw_speed degrees
	yaw_degrees = BotChangeYaw( this, pEdict->v.yaw_speed );

	// do any speed adjustments only if bot is NOT crouched or lying prone (ie bot is moving fast) or NOT evading mine or NOT using bipod
	if ((IsProne() == false) && (IsCrouched() == false) && (IsTask(TASK_CLAY_EVADE) == false))
	{
		// slow down if pitch is positive ie looking down (for example going down the hill) and not sprinting otherwise don't slow down
		// (20.0 is max value, it's set at creation as max speed for both axis)

		// don't move while turning a lot
		if (yaw_degrees >= 20.0f)
		{
			SetMoveSpeed(MoveSpeed::stop);
			// also don't try to run unstuck code
			// 0.2 is duration of one movement check so we will double this value for next stuck check
			SetDontCheckStuck("BThink()|adjust speed due to turning alot -> speed no", 0.4f);
		}
		// slow down a lot if the bot does a moderate turn
		else if ((yaw_degrees >= 14.0f) || ((pitch_degrees >= 20.0f) && (pEdict->v.v_angle.x > 0) && (IsTask(TASK_SPRINT) == false) && IsBehaviour(BOT_PRECISION)))
		{
			SetMoveSpeed(MoveSpeed::slowest);
		}
		// if the bot turns only a bit then move at about walk speed
		else if ((yaw_degrees >= 7.0f) || ((pitch_degrees >= 15.0f) && (pEdict->v.v_angle.x > 0) && (IsTask(TASK_SPRINT) == false) && IsBehaviour(BOT_PRECISION)))
		{
			SetMoveSpeed(MoveSpeed::slow);
		}
	}

#ifdef _DEBUG
	if (devTool.IsBotDebugging(pEdict))
	{
		/*/
		if (IsSubTask(ST_FACEENEMY))
		{
			char msg[128];
			sprintf(msg, "This bot is turning at yaw of <%.2f> and pitch of <%.2f> | moving at mSpeedVal %d with velocity <%.2f>\n",
				yaw_degrees, pitch_degrees, GetMoveSpeed(), pEdict->v.velocity.Length2D());
			conOutput.Notify(msg, this);
		}
		/**/
	}
#endif

	// before we start any actions we must find out in which stance the bot currently is...
	if (IsDoingDuckJumpNow())
	{
		// don't change current stance while the bot is in duckjump
		;





#ifdef DEBUG
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
		if (botdebugger.IsDebugStuck() || botdebugger.IsDebugActions())
		{
			char djmsg[128]{};
			sprintf(djmsg, "I'm doing DUCKJUMP right NOW!\n");
			conOutput.Notify(djmsg, this);

			if (duckjump_time == gpGlobals->time)
				conOutput.Notify("DUCKJUMP ends in this frame!\n", this);
		}
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#endif // DEBUG




	}
	else
	{
		if (IsProne())
		{
			// clear all flags because there can only be one correct stance at a time
			ResetPosture();

			// and set it for this whole frame
			SetBehaviour(BOT_PRONED);

			// we must check for case when bot is proned, called 'prone' command in last frame, but engine doesn't allow him
			// to stand up, because someone or something is blocking him (e.g. another bot standing on him at wait-timed wpt)
			if (IsBehaviour(GOTO_STANDING) && (GetGoProneTime() + 0.6f < gpGlobals->time))
			{
				if (moved_distance < 2.0f)
				{
					// just set this bit and the clearing section at the bottom of BotThink() will fix it
					SetSubTask(ST_CANTPRONE);


#ifdef DEBUG
					//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@														// NEW CODE 094 (remove it)
					if (botdebugger.IsDebugStance())
					{
						char dm[128]{};
						sprintf(dm, "SET CANTPRONE subtask @ BotThink()|INIT stance sect -> moved_dist < 2.0 (moved_dist=%.2f)\n", moved_distance);
						conOutput.Notify(dm, this);
					}
					//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#endif // DEBUG
				}
				else if (moved_distance == 2.0f)
				{
					;		// do nothing
				}
				else
				{





#ifdef DEBUG
					//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@	 													// NEW CODE 094 (remove it)
					if (botdebugger.IsDebugStance())
					{
						char dm[128]{};
						sprintf(dm, "GOTO_STANDING and BOT_PRONED @ INIT stance sect (moved_dist=%.2f) !! SEE WHEN THIS HAPPENED\n", moved_distance);
						conOutput.Notify(dm, this);
					}
					//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#endif // DEBUG



				}
			}
		}
		else if (IsCrouched())
		{
			ResetPosture();

			SetBehaviour(BOT_CROUCHED);
		}
		// if NOT going to/resume from prone then the bot is standing
		else if (IsNotGoingProne())
		{
			ResetPosture();

			SetBehaviour(BOT_STANDING);
		}
		// bot can only go/resume from prone now
		// (this statement also gets called when the bot tried to go prone, but engine didn't allow it,
		// for example due to being to close to wall or other player)
		// going prone seems to be almost instant, but resume from prone takes several game frames so...
		else
		{
			// don't check for stuck for a short while to prevent bot running false unstuck codes (the bot would have most probably started to strafe)
			if (IsSubTask(ST_CANTPRONE) == false)
			{
				SetDontCheckStuck("BThink()|INIT stance sect -> going/resume from prone (set dont check for stuck to 0.4s)", 0.4);
			}
			else
			{



#ifdef DEBUG
				//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
				if (botdebugger.IsDebugStuck() || botdebugger.IsDebugActions() || botdebugger.IsDebugStance())
					conOutput.Notify("Am I really going to/resume from prone RIGHT NOW???\n", this);
				//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#endif // DEBUG


			}
		}
	}

	// is the bot frozen?
	if (botdebugger.IsFreezeMode())
	{
		// don't move
		SetMoveSpeed(MoveSpeed::stop);
		// prevents false running of unstuck code
		SetDontCheckStuck();
		//SetDontCheckStuck("bot think()->freeze mode enabled");

		// prevents running any waypoint based navigation
		SetDontLookForWaypoint();
		// update current waypoint time to prevent false waypoint lost/stuck
		SetTimeToReachCurrWaypoint();

		// is doing any waypoint action then extend its time
		if (IsActionTime())
		{
			SetActionTime(GetActionTime() - GetPreviousGlobalsTime());
		}

		// is bot waiting somewhere then keep increasing the wait time together with increasing global time
		if (IsWaitTime())
		{
			SetWaitTime( GetWaitTime() - GetPreviousGlobalsTime() );
		}
	}

	// else handle movement related actions
	else
	{
		// handle DoD stamina
		if (pEdict->v.fuser4 <= 4.0f)
		{
			RemoveTask(TASK_SPRINT);	// no sprint is allowed now


#ifdef _DEBUG
			//@@@@@@@@@@@@@@@@@
			//conOutput.Notify("***BThink() -> REMOVED the task SPRINT due to no stamina!!!\n", this);
#endif


		}
		else if (pEdict->v.fuser4 <= 15.0f)
		{
			SetTask(TASK_NOJUMP);		// no jump is allowed


#ifdef _DEBUG
			//@@@@@@@@@@@@@@@@@
			//conOutput.Notify("***CANT JUMP due to no stamina!!!\n", this);
#endif

		}
		else if (pEdict->v.fuser4 >= 35.0f)
		{
			RemoveTask(TASK_NOJUMP);	// bot can jump again
		}

		// is the melee only game mode activated?
		if (internals.IsMeleeOnlyMode())
		{
			// then tell the bot he depleted all ammunition including greanades which will force him to switch to and use only the knife/spade
			SetWeaponStatus(WS_NOAMMOFORMAIN);
			SetWeaponStatus(WS_NOAMMOFORBACKUP);
			SetGrenadesDepleted();
		}

		// is it time to check for placed claymores and other ground items or thrown items (ie. grenades) yet?
		if (time_to_look_for_ground_items <= gpGlobals->time)
		{
			// set time for the next check
			time_to_look_for_ground_items = gpGlobals->time + 0.5f;

			BotFindItem(this);
		}

		if (botdebugger.IsIgnoreAll() == false)
		{
			// did bot already plant the claymore mine at map goal?
			if (IsWeaponStatus(WS_CLAYMOREGOAL))
			{
				// can bot look for an enemy now?
				if (CanLookForEnemy())
				{
					// then prevent him to look for enemy for next couple seconds
					SetDontLookForEnemyTime(5.0f);

					//if (RANDOM_LONG(1, 100) <= 45)
					//	pBotEnemy = BotFindEnemy();
					//else
					//	pBotEnemy = NULL;

					// has bot an enemy?
					if (pBotEnemy != NULL)
					{
						// in most of the time detonate the mine even if bot isn't in safe distance yet
						if (RANDOM_LONG(1, 100) <= 75)
						{
							// pressing it detonates the claymore
							pEdict->v.button |= IN_ATTACK;

							// bot already used the mine
							RemoveWeaponStatus(WS_CLAYMOREGOAL);
						}
					}
				}
			}
			else
			{
				pBotEnemy = BotFindEnemy();
			}
		}
		else
		{
			pBotEnemy = NULL;  // clear enemy pointer (no ememy for you!)
		}

		// check whether the bot still has any grenade left
		if ((IsGrenadesDepleted() == false) && (grenade_slot != NO_VAL) && ((bot_weapons & (1 << grenade_slot)) == false) && (IsGrenadeUseTime() == false))
		{
			SetGrenadesDepleted();


#ifdef _DEBUG
			//@@@@@@@@@@@@@@@@@@@@@@
			if (botdebugger.IsDebugWeapons())
				conOutput.Notify("BThink() -> SET GRENADES DEPLETED !!!!!\n", this);
#endif

		}

		// bot manipulated with the bipod so let's check if the engine set the variables yet
		if (IsWeaponStatus(WS_BIPODMANIPULATION))
		{
			if ((pEdict->v.iuser3 == USR3_BIPOD_USED) || (pEdict->v.vuser1.x == VUSR1_BIPOD_USED))
			{
				SetTask(TASK_BIPOD);
				RemoveWeaponStatus(WS_BIPODMANIPULATION);

#ifdef _DEBUG
				//@@@@@@@@@@@@@@@@@@@@@@
				if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
				{
					conOutput.Notify("BThink()|check BIPOD -> SET task bipod\n", this);
				}
#endif

			}

			if (IsTask(TASK_BIPOD) && (pEdict->v.iuser3 != USR3_BIPOD_USED) && (pEdict->v.vuser1.x != VUSR1_BIPOD_USED))
			{
				RemoveTask(TASK_BIPOD);
				RemoveWeaponStatus(WS_BIPODMANIPULATION);

#ifdef _DEBUG
				//@@@@@@@@@@@@@@@@@@@@@@
				if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
				{
					conOutput.Notify("BThink()|check BIPOD -> REMOVED task bipod\n", this);
				}
#endif

			}
		}

		// look if the bot is doing bandage treatment
		// this statement must be before battle actions, because FA doesn't allow combat while bandaging (it even hides the weapon)
		if (IsBandagingNow())
		{
			SetMoveSpeed(MoveSpeed::stop);
			SetDontCheckStuck("BThink() -> bandage time");

			SetDontLookForWaypoint();
			SetTimeToReachCurrWaypoint();

			// if NOT proned or currently doing so then crouch for cover
			if ((IsBehaviour(BOT_PRONED) == false) && IsNotGoingProne())
				SetStance(GOTO_CROUCH, "BThink()|BandageTime -> GOTO crouch");
		}

		// is the bot paused (being under medical treatment from teammate or he's merging magazines) AND it is NOT a bot medic trying to give a treatment to someone else
		// this also must be before combat actions, because there's no weapon to shoot from
		else if ((IsNotPaused() == false) && (IsTask(TASK_HEALHIM) == false))
		{
			SetMoveSpeed(MoveSpeed::stop);
			SetDontCheckStuck("BThink() -> pause time");

			SetDontLookForWaypoint();
			SetTimeToReachCurrWaypoint();

			// if NOT proned or currently doing so then crouch for cover
			if ((IsBehaviour(BOT_PRONED) == false) && IsNotGoingProne())
				SetStance(GOTO_CROUCH, "BThink()|PauseTime -> GOTO crouch");
		}

		// is bot stuck AND trying to go crouch to get free AND NOT fully crouched yet?
		else if ((NotBeenStuckFor(1.0f) == false) && IsBehaviour(GOTO_CROUCH) && (IsCrouched() == false))
		{
			// then wait for a while to get fully crouched and keep updating the times
			SetMoveSpeed(MoveSpeed::stop);
			
			SetDontLookForWaypoint(0.4f);
			SetTimeToReachCurrWaypoint();

			f_dont_avoid_wall_time = gpGlobals->time + 0.4f;

#ifdef DEBUG
			if (botdebugger.IsDebugStuck())
				conOutput.Notify("***STUCK*** -> waiting for engine to set the fully ducking bit\n", this);
#endif // DEBUG

		}

		// does an enemy exist? (ignore enemy when on ladder, because DoD blocks weapon use on ladder so bot cannot shoot at all)
		else if ((pBotEnemy != NULL) && (pEdict->v.movetype != MOVETYPE_FLY))
		{
			// shoot at the enemy? (going to/resume from prone will block weapon use so don't try to shoot at that moment)
			if (IsNotGoingProne())
			{
				BotShootAtEnemy( this );
			}

			// update the time to reach current waypoint if the bot has any waypoint
			if (curr_wpt_index != NO_VAL)
				SetTimeToReachCurrWaypoint();
		}

		// is bot being "used" and can still follow team leader AND NOT ordered to use rocket/grenade launcher AND NOT ordered to drop the extra ammo?
		else if ((pTeamLeader != NULL) && (IsSubTask(ST_MEDEVAC_H) == false) && (IsTask(TASK_MEDEVAC) == false))
		{
			if (BotFollowTeamLeader(this))
			{
				// make sure to completely forget about standard navigation
				if (curr_wpt_index != NO_VAL)
				{
					ResetWaypointBasedNavigation();


#ifdef DEBUG
					//@@@@@@@@@@@@@@@@@@@@
					if (botdebugger.IsDebugActions())
						conOutput.Notify("WPT NAVIG been fully reset due to following TeamLeader\n");
#endif // DEBUG


				}

				// is team leader crouched go crouch as well
				if (util.IsEdictCrouched(pTeamLeader))
					SetStance(GOTO_CROUCH,"BThink()|FollowingTL -> Matching leader's crouch stance");
				// is bot crouched, but team leader isn't so stand up
				else if (IsBehaviour(BOT_CROUCHED) && (util.IsEdictCrouched(pTeamLeader) == false))
					SetStance(GOTO_STANDING, "BThink()|FollowingTL -> Matching leader's standing stance");
				// is team leader in prone AND bot isn't so go prone 
				else if (util.IsEdictProne(pTeamLeader) && (IsBehaviour(BOT_PRONED) == false))
					GoProne("BThink()|FollowingTL -> Matching leader's prone stance");
				// is bot in prone, but team leader isn't so stand up
				else if (IsBehaviour(BOT_PRONED) && (util.IsEdictProne(pTeamLeader) == false))
					GoProne("BThink()|FollowingTL -> Matching leader's standing stance");

				// got the bot on ladder while following his team leader
				if (pEdict->v.movetype == MOVETYPE_FLY)
				{
					if (LadderDirectionNotDecidedYet())
					{
						// team leader is under the bot so climb down
						if (pTeamLeader->v.origin.z < pEdict->v.origin.z)
							SetLadderUseDirection(LadderDir::climb_down);
						// otherwise climb up
						else
							SetLadderUseDirection(LadderDir::climb_up);
					}

					if (IsClimbLadderUp())
					{
						pEdict->v.v_angle.x = -80;	// look upwards
						pEdict->v.button |= IN_FORWARD;	// and move forward
					}
					else if (IsClimbLadderDown())
					{
						pEdict->v.v_angle.x = 80;	// look downwards
						pEdict->v.button |= IN_FORWARD;	// and move forward
					}
				}
				else if (DealWithWeaponManipulation(this, "BThink() -> following TeamLeader"))
				{
					// we don't need to do anything here, all is handled inside the function in the statement, we just need to branch the code here this way else some things would fail
				}
				else if (BotManageWeaponUse(this, "BThink() -> following TeamLeader"))
				{
				}

				SetDontLookForWaypoint();
			}
		}

		// is the bot in process of deploying or folding the bipod...
		else if (IsNotDeployingBipod() == false)
		{
			// then just continue updating these to prevent calling "I must be stuck" code
			SetMoveSpeed(MoveSpeed::stop);
			SetDontCheckStuck("BThink() -> deploying or folding bipod");

			SetDontLookForWaypoint();
			SetTimeToReachCurrWaypoint();
		}

		// is the bot using bipod while not being in combat
		else if (IsTask(TASK_BIPOD))
		{
			// stop using bipod if snipe time is over, this one handles only sudden firefights when the bot isn't at camper spot (i.e. NOT at wait-timed waypoint)
			if ((IsWaitTime() == false) && IsNotSnipeTime())
			{
				BotUseBipod(this, FALSE, "BThink()|TaskBipod -> battle SnipeTime is over so FOLD IT");
			}
			// did the bot ran out of ammo?
			else if (IsNoAmmoForMainWeapon() && (main_weapon != NO_VAL))
			{
				// stop using bipod immediately
				BotUseBipod(this, TRUE, "BThink()|TaskBipod -> NOAMMO so FOLD IT");
			}
			// is the weapon empty or almost empty?
			else if (ShouldReload("BThink() -> TaskBipod"))
			{
				ReloadWeapon("BThink() -> TaskBipod");
			}
			// does the bot need to target another aim waypoint while waiting at wait-timed wpt...
			else if ((IsTimeToKeepCurrentAim() == false) && IsWaitTime())
			{
				// stop using bipod immediately
				BotUseBipod(this, TRUE, "BThink()|TaskBipod -> Need to FACE new AIM wpt so FOLD IT");
			}
			// if the wait time is about to end we must fold bipod so the bot can leave the waypoint
			else if (IsWaitTime() && ((gpGlobals->time + GetBipodHandlingTime(current_weapon.iId, pEdict->v.playerclass, false)) > GetWaitTime()))
			{
				// stop using bipod immediately
				BotUseBipod(this, TRUE, "BThink()|TaskBipod -> WAIT time is about to end so FOLD IT");
			}
			// is bot at shoot waypoint with deployed bipod for some reason? (he needed to reload or encountered enemy and started battle upon reaching this waypoint)
			else if (wptmanager.IsWaypoint(curr_wpt_index, WptT::shoot))
			{

				// TODO: This needs to be fixed somehow, because calling task bipod statement prevents calling either the task fire or wait-time statements
				// which means bot won't start shooting at all. For now we'll just fold the bipod to make bot finish the shoot action at this waypoint.
				
				BotUseBipod(this, TRUE, "BThink()|TaskBipod -> At SHOOT wpt and need to shoot so FOLD IT");
			}
			
			// otherwise keep using it so...
			SetMoveSpeed(MoveSpeed::stop);
			SetDontCheckStuck("BThink()|TaskBipod -> keep using it");
			
			SetDontLookForWaypoint();
			SetTimeToReachCurrWaypoint();
		}

		// is the bot forced to turn to aim waypoint or turn to some item stored as a pointer to entity
		else if ((IsSubTask(ST_AIM_FACEAIMWPT)) || (IsTask(TASK_IGNOREAIMWPTS)))
		{
			SetMoveSpeed(MoveSpeed::stop);
			SetDontCheckStuck("BThink() -> facing AimWpt or GEnt or PointISp");

			SetDontLookForWaypoint();
			SetTimeToReachCurrWaypoint();

			if (IsTask(TASK_IGNOREAIMWPTS))
			{
				if (FaceGameEntity() == false)
					FacePointInSpace();
			}
			else
				TargetAimWaypoint();
		}

		// is the bot in process of reloading his current weapon?
		// we need this statement here to prevent calling any other movement based actions that are below this one where the bot can eventually use his weapon
		else if (IsNotReloadingWeapon() == false)
		{
			;	// we do nothing here, because everything is handled below ... inside weapon action == inreload section
		}

		// is bot tasked to use primary fire outside combat?
		else if (IsTask(TASK_FIRE))
		{
			if (IsSubTask(ST_AIM_DONE) || IsSubTask(ST_FACEGENT_DONE))
			{
				SetMoveSpeed(MoveSpeed::stop);
				SetDontCheckStuck("BThink() -> TaskFire");

				SetDontLookForWaypoint();
				SetTimeToReachCurrWaypoint();

				if (IsNotGoingProne() && IsAllowedToHandleWeapon())
				{
					// is it a shoot waypoit on a class based path, specifically a path for rocket/grenade launcher class, AND there is any breakable object in front of the bot?
					if (wptmanager.IsPath(curr_path_index, PathT::antiarmor_class) && util.CheckForwardForBreakable(this))
					{
						// then make bot handle the object through wait time
						SetWaitTime(15.0f);

						// so we have to reset this task
						RemoveTask(TASK_FIRE);
					}
					// check if the weapon is empty
					else if (IsCurrentWeaponEmpty())
					{
						if (current_weapon.iAmmo1 != 0)
							ReloadWeapon("BThink()|TaskFire -> empty weapon");
						else
							DecideNextWeapon("BThink()|TaskFire -> empty weapon");
					}
					/*/
					// is bot still crouched after reloading his weapon? then make him stand up before he starts shooting at the object
					else if (IsCrouched() && (wptmanager.IsWaypoint(curr_wpt_index, WptT::crouch) == false))
					{
						SetStance(GOTO_STANDING, "BThink()|TaskFire Bot in crouch but NOT at crouch wpt -> GOTO standing");
					}
					/**/
					else
					{
						// has this waypoint highest priority? then just shoot once, waypoint creator set it up so, so do it (could be just for the fun)
						if (wptmanager.GetWaypointPriority(curr_wpt_index, GetBotTeam()) == 1)
						{
							BotFireWeaponOutOfCombat(this);

							// and reset all tasks and subtasks once we are done here
							RemoveTask(TASK_FIRE);
							ResetAims("BThink()|TaskFire -> done & leaving");
						}
						// otherwise...
						else
						{
							// is the bot facing a breakable game entity? ... this happens when the bot got stuck on it
							if (IsSubTask(ST_FACEGENT_DONE) && IsPointerToGEntThisEntity("func_breakable"))
							{
								// make the bot keep adjusting the aim vector in order to deal with breakable object that has multiple parts and holes 
								RemoveSubTask(ST_FACEGENT_DONE);
								SetTask(TASK_IGNOREAIMWPTS);

								// is the breakable object or current part of it not fully destroyed?
								if (GetPointerToGEnt()->v.health > 0.0f)	// not checking the gent being a null is safe as long as it is locked inside ispointertogent...(), because that one checks it
								{
									// makes bot switch to "safe" weapon first in case he is holding dangerous explosives right now (in DoD the chances for this are slim, but still)
									if (BotSelectWeaponToDestroyThisBreakable(this) == false)
									{
										BotFireWeaponOutOfCombat(this);		// keep shooting at it

										// is the bot having trouble destroying it (eg. using knife and can't reach it from current position)? then try to move a little
										if ((GetUnstuckAttempts() > 0) && (NotBeenStuckFor(5.0f) == false))
										{
											SetMoveSpeed(MoveSpeed::slow);
											// also update the "I got stuck" variables
											IncUnstuckAttempts();
											SetGotStuckTime();

											// still having troubles? then start randomizing the hit point
											if (GetUnstuckAttempts() > 5)
												SetSubTask(ST_RANDOMCENTRE);
										}
									}
								}
								else
								{
									// check whether there is another part of the breakable obstacle
									if (util.CheckForBreakableAround(this, STANDARD_SEARCH_RADIUS / 2.0f))
										;
									else
									{										
										// there isn't any other breakable object and current object is already broken (ie. has no health) so break the action by clearing the pointer and removing the task
										// which will lead to a try to find an aim waypoint around and to check for breakable in front and after failing both the action will be canceled due to nothing to shoot at
										SetPointerToGEnt(NULL);
										RemoveTask(TASK_IGNOREAIMWPTS);
									}
								}
							}
							// otherwise see if there's a breakable object in front of the bot ... including such that is built of multiple parts
							else if (util.CheckForwardForBreakable(this, true))
							{
								// can the breakable object be destroyed only by explosives? ... means this is a bot with a rocket/grenade launcher and this shoot waypoint is kind of a goal for him
								if (IsSubTask(ST_MEDEVAC_ST))
								{
									// so make bot handle the object through wait time
									SetWaitTime(15.0f);
									RemoveTask(TASK_FIRE);
								}
								else
								{
									// is knife/spade the only usable weapon now? then break the action, because with the knife/spade the bot may get stuck here unable to destroy distant breakable object
									if (IsUsedWeaponKnife())
									{
										RemoveTask(TASK_FIRE);
										ResetAims("BThink()|TaskFire -> have just melee thus leaving");
										RemoveSubTask(ST_RANDOMCENTRE);

										if (botdebugger.IsDebugActions())
											conOutput.Notify("***Have just a knife/spade! Leaving\n", this);
									}
									// did we find a object built of multiple parts? then we'll deal with it via bot wait here function
									else if (IsPointerToGEntThisEntity("func_breakable"))
									{
										RemoveTask(TASK_FIRE);
										SetWaitTime(5.0f);
									}
									else if (BotSelectWeaponToDestroyThisBreakable(this, true) == false)
										BotFireWeaponOutOfCombat(this);
								}
							}
							else
							{
								// otherwise break the action, because there's nothing to shoot at here
								RemoveTask(TASK_FIRE);
								ResetAims("BThink()|TaskFire -> nothing to shoot at");
								RemoveSubTask(ST_RANDOMCENTRE);

								if (botdebugger.IsDebugActions())
									conOutput.Notify("***Nothing to shoot at! Leaving now\n", this);
							}
						}
					}
				}
			}
			else if (IsTask(TASK_IGNOREAIMWPTS) == false)
				SetSubTask(ST_AIM_FACEAIMWPT);
		}

		// is the bot doing waypoint action?
		else if (IsActionTime())
		{
			bool do_action = false;

			if (IsTask(TASK_CLAY_IGNORE) == false)
			{
				SetMoveSpeed(MoveSpeed::stop);
				SetDontCheckStuck("BThink() -> wpt action time");
			}

			SetDontLookForWaypoint();
			SetTimeToReachCurrWaypoint();

			// don't have any item yet? then see which entities are around
			if (HasNoGEnt())
			{
				if (botdebugger.IsDebugActions())
					conOutput.Notify("***Has no item yet. Going to check for it now...\n", this);

				if (IsTask(TASK_CLAY_IGNORE) && (curr_wpt_index != NO_VAL))
					do_action = util.IsExplosivesChargeNearby(this, waypoints[curr_wpt_index].range, waypoints[curr_wpt_index].origin);
				else
					do_action = util.CheckForUsablesAround(this);

				// nothing useful has been found?
				if (do_action == false)
				{
					if (IsTask(TASK_CLAY_IGNORE))
					{
						SetActionTime(-0.2f);

						if (botdebugger.IsDebugActions() || botdebugger.IsDebugWaypoints())
							conOutput.Notify("***NO item has been found so leaving this waypoint!\n", this);
					}
					// so press the "use" just in case and clear action time to prevent false behavior
					else
					{
						pEdict->v.button |= IN_USE;
						SetActionTime(-0.2f);

						if (botdebugger.IsDebugActions())
							conOutput.Notify("***NO item has been found, but hitting the use key anyway and leaving this waypoint!\n", this);
					}
				}
				// otherwise we found something usable so we'll set this task to make the bot face the item/entity
				else
				{
					ResetAims("BThink() -> WptActionTime");
					SetTask(TASK_IGNOREAIMWPTS);
				}
			}

			// bot must be already turned to the entity/item so use it
			if (IsSubTask(ST_FACEGENT_DONE))
			{
				// handle eventual crash
				if (HasNoGEnt())
				{
					// log it
					char ermsg[256]{};
					sprintf(ermsg, "BThink()|WptActionTime -> GEnt is NULL | on map %s (wpt=%d | path=%d)\n", STRING(gpGlobals->mapname), curr_wpt_index + 1, curr_path_index + 1);
					util.DebugInFile(ermsg);

					// clear the time
					SetActionTime(-0.2f);

					// and finish this frame
					g_engfuncs.pfnRunPlayerMove(pEdict, pEdict->v.v_angle, ConvertMoveSpeedToRealValue(), 0.0f, 0.0f, pEdict->v.button, 0, msecval);
					return;
				}

				if (IsTask(TASK_CLAY_IGNORE))
				{
					// did the bot get to this explosives charge yet or did he take a different one while he was heading to this one?
					if (IsEquippedWithExplosiveCharge())
					{
						// then break the whole action, because the bot got what he wanted
						SetActionTime(-0.2f);

						if (botdebugger.IsDebugActions())
							conOutput.Notify("***Took the explosives charge\n");
					}
					// did someone else take the explosives charge while the bot was moving towards it?
					else if (IsPointerToGEntThisEntity("dod_object") && (GetPointerToGEnt()->v.effects & EF_NODRAW))
					{
						// then forget about this explosives charge ... bot will try to find different explosives charge around
						SetPointerToGEnt(NULL);
						RemoveSubTask(ST_FACEGENT_DONE);
						// update the action time so that the bot won't leave
						SetActionTime(10.0f);
					}
					// otherwise move towards this explosives charge
					else
					{
						if (GetDistanceToGEnt() > 20.0f)
							SetMoveSpeed(MoveSpeed::max);
						else
							SetMoveSpeed(MoveSpeed::slow);

						Vector bot_angles = UTIL_VecToAngles(GetPointerToGEnt()->v.origin - pEdict->v.origin);

						pEdict->v.idealpitch = -bot_angles.x;
						BotFixIdealPitch(pEdict);
						pEdict->v.ideal_yaw = bot_angles.y;
						BotFixIdealYaw(pEdict);

						SetActionTime(10.0f);
					}
				}
				// don't try to use anything else if already using a TANK (mounted gun)
				else if (IsTask(TASK_USETANK) == false)
				{
					// by default do press the "use" button
					do_action = true;
					bool wait_for_status = false;
					char item_name[64]{};
					strcpy(item_name, STRING(GetPointerToGEnt()->v.classname));

					// first get priority of this waypoint
					int wpt_priority = wptmanager.GetWaypointPriority(curr_wpt_index, GetBotTeam());

					// keep the "use" button down for these entities
					if ((strcmp("ammobox", item_name) == 0) || (strcmp("momentary_rot_button", item_name) == 0))
					{
						pEdict->v.button |= IN_USE;
						SetSubTask(ST_BUTTON_USED);
					}
					// otherwise press the "use" button only once ... for mounted gun controls let this statement run only once so the bot can press "use" to gain the control
					else if ((strcmp("func_tankcontrols", item_name) != 0) || ((IsSubTask(ST_BUTTON_USED) == false) && (strcmp("func_tankcontrols", item_name) == 0)))
					{
						// is this a button with specific priority setting?
						if (((strcmp("func_button", item_name) == 0) || (strcmp("button_target", item_name) == 0)) && ((wpt_priority == 1) || (wpt_priority == 2)))
						{
							// then check if it controls any door
							edict_t* door_entity = util.FindEntityByTargetname(NULL, STRING(GetPointerToGEnt()->v.target));

							if (util.IsDoorEntity(door_entity))
							{
								// is the door moving right now?
								if (door_entity->v.nextthink > 0.0f)
								{
									// is there a wait time set on this waypoint? then try waiting till the door stop moving to check its status (ie. open or closed)
									if (wptmanager.GetWaypointWaitTime(curr_wpt_index, GetBotTeam()) > 0.0f)
										wait_for_status = true;
									// otherwise ignore this button and leave
									else
										do_action = false;
								}
								// or is the door in state matching the priority setting? (ie. 1 = open and 2 = closed) then don't do anything and leave
								else if (((wpt_priority == 1) && util.IsDoorOpen(door_entity, false)) || ((wpt_priority == 2) && (util.IsDoorOpen(door_entity, false) == false)))
								{
									do_action = false;

									if (botdebugger.IsDebugActions())
										conOutput.Notify("***Door in desired state -> leaving\n");
								}
							}
						}

						// does the bot need to wait for the door to stop moving to see whether its open or closed? then don't do anything
						if (wait_for_status)
						{
							if (botdebugger.IsDebugActions())
								conOutput.Notify("***Waiting for status\n", this);
						}
						// can the bot hit the use button?
						else if (do_action && (IsSubTask(ST_BUTTON_USED) == false))
						{
							pEdict->v.button |= IN_USE;
							SetSubTask(ST_BUTTON_USED);

							// button will be pressed so we must break the action time in order to return the bot back to standard behaviour
							// unless there's a 'wait' time on this waypoint to force the bot stay there for a moment (e.g. to let the doors fully open before moving forward)
							// or the bot is trying to use TANK (ie. mounted gun) for unlimited period of time (ie. without wait time on the use waypoint)
							if ((wptmanager.GetWaypointWaitTime(curr_wpt_index, GetBotTeam()) == 0.0f) && (strcmp("func_tankcontrols", item_name) != 0))
								SetActionTime(0.0f);

							if (botdebugger.IsDebugActions())
								conOutput.Notify("***Button used only once\n");


							//@@@@@@@@@@@@@@@
#ifdef _DEBUG
							if (botdebugger.IsDebugStance())
							{
								char dm[128]{};
								sprintf(dm, "BotThink()|WptActionTime -> USE button PRESSED at wpt=%d (ActTime=%.2f | CurrTime=%.2f)\n", curr_wpt_index + 1, GetActionTime(), gpGlobals->time);
								conOutput.Notify(dm, this);
							}
#endif


						}
						else
							SetActionTime(0.0f);
					}
					// see if bot found mounted gun (actually the entity that allows using it)
					else if (strcmp("func_tankcontrols", item_name) == 0)
					{
						// when the game engine removes weapon model while bot is alive then this bot is really "tied" to the gun
						if ((pEdict->v.viewmodel == 0) && (pEdict->v.health > 0.0f))
						{
							// so mark him as "now I'm using mounted gun"
							SetTask(TASK_USETANK);

							// try to find any aim waypoint around
							wptmanager.FindAimingWaypointsForBot(this, curr_wpt_index);

							// is the bot is going to use it for unlimited time? 
							if (IsSubTask(ST_TANK_SHORT) == false)
							{
								// reset the action time because we don't need it anymore
								SetActionTime(0.0f);

								// to prevent turning away from wall where the gun is mounted on
								f_dont_avoid_wall_time = gpGlobals->time + 1.0f;
							}

							if (botdebugger.IsDebugActions())
								conOutput.Notify("***Started using TANK\n");
						}

					}
				}
			}
		} // END waypoint action time

		// is it waypoint wait time so stop at current waypoint and wait
		else if (IsWaitTime())
		{
			BotWaitHere();

			// if we use standard aim waypoints then...
			if (CanResetPitch())
			{
				// look directly forward (ie ignore pitch angles obtained from vector to aim waypoint conversion)
				pEdict->v.idealpitch = 0.0f;
				BotFixIdealPitch(pEdict);
			}
		}

		else
		{
			// no enemy, let's just wander around

			// is bot NOT under water?
			if ((pEdict->v.waterlevel != 2) && (pEdict->v.waterlevel != 3))
			{
				// reset pitch to 0 (level horizontally)
				pEdict->v.idealpitch = 0.0f;
				pEdict->v.v_angle.x = 0;
			}

			// see if bot needs to handle important weapon manipulation actions
			if (DealWithWeaponManipulation(this, "BThink() -> NO enemy so just wander around"))
			{
				;
			}
			// otherwise try some navigation
			else
			{
				BotManageWeaponUse(this, "BThink() -> while navigating");

				// does this bot need to form a fireteam? (the not spoke check is mainly used to gain more time after bot respawned)
				if (IsNeed(NEED_FIRETEAM) && NotSpokeFor(1.5f))
				{
					// is there free spot in his fireteam AND is there at least one teammate around and isn't in this fireteam yet?
					if ((util.CountFireTeamMembers(pEdict) < (FIRETEAM_SIZE - 1)) && util.IsTeammateNearby(this, TEAMMATE_SEARCH_RADIUS, 1, true))
					{
						// then try to form the fireteam by calling the command and also do pause for a moment so that the team can gather
						BotSpeak(voiceCmd::coverme);
						SetPausedTime(RANDOM_FLOAT(1.0f, 2.0f));
					}

					// we don't want to keep trying ... once is enough ... bot will try it again after next battle or respawn
					RemoveNeed(NEED_FIRETEAM);
				}
				
				// was the bot on PATROL path before combat?
				if (IsTask(TASK_BACKTOPATROL) && IsPatrolPathWaypoint())
				{
					// run this only once
					RemoveTask(TASK_BACKTOPATROL);

					// is last PATROL path waypoint still reachable?
					if (wptmanager.IsPatrolWaypointReachableForThisBot(this))
					{
						// set it as current waypoint to head to (ie return the bot on PATROL path)
						SetCurrentWaypoint(GetPatrolPathWaypoint());

						if (botdebugger.IsDebugPaths())
							conOutput.Notify("PATROL path return flag cleared (Bot gets back to patrol)\n", this);
					}
				}

				// check if the bot is underwater
				if (pEdict->v.waterlevel == 3)
				{
					BotUnderWater( this );
				}
				
				found_waypoint = FALSE;

				// it is time to look for a waypoint if there are some waypoints for this map
				if (CanLookForWaypoint() && (num_waypoints != 0))
				{
					found_waypoint = BotHeadTowardWaypoint(this);



#ifdef _DEBUG
					if (found_waypoint == FALSE)//						TEMPORARY for tests
					{
						//@@@@@@@@@@@@@@@@@@@@@@@@@@@@
						if (botdebugger.IsDebugStuck())
						{
							char dbgmsg[256]{};
							sprintf(dbgmsg, "BThink() -> HeadTowardWaypoint() RETURNED FALSE (currWtp=%d currPth=%d) !!!\n", curr_wpt_index + 1, curr_path_index + 1);
							conOutput.Notify(dbgmsg, this);
						}
					}
#endif




				}

				// is the bot on ladder
				if (pEdict->v.movetype == MOVETYPE_FLY)
				{
					f_dont_avoid_wall_time = gpGlobals->time + 2.0f;

					// then do all the ladder navigation
					BotHandleLadder(this, moved_distance);
				}

				// if the bot isn't headed toward a waypoint...
				if (found_waypoint == FALSE)
				{
					TraceResult tr;

					// check if we should be avoiding walls
					if (f_dont_avoid_wall_time <= gpGlobals->time)
					{
						// let's just randomly wander around
						if (BotStuckInCorner( this ))
						{
							pEdict->v.ideal_yaw += 180;  // turn 180 degrees

							BotFixIdealYaw(pEdict);

							SetMoveSpeed(MoveSpeed::stop);  // don't move while turning
							f_dont_avoid_wall_time = gpGlobals->time + 1.0;

							moved_distance = 2.0;  // dont use bot stuck code
#ifdef DEBUG
							if (botdebugger.IsDebugStuck())
								conOutput.Notify("***FREE ROAM*** Bot StuckInCorner!\n", this);
#endif // DEBUG
						}
						else
						{
							// check if there is a wall on the left...
							if (!BotCheckWallOnLeft( this ))
							{
#ifdef DEBUG
								if (botdebugger.IsDebugStuck())
									conOutput.Notify("***FREE ROAM*** Bot CheckWallOn LEFT!\n", this);
#endif // DEBUG
								// if there was a wall on the left over 1/2 a second ago then 20% of the time randomly turn between 45 and 60 degrees
								if ((f_wall_on_left != 0) && (f_wall_on_left <= gpGlobals->time - 0.5) && (RANDOM_LONG(1, 100) <= 20))
								{
									pEdict->v.ideal_yaw += RANDOM_LONG(45, 60);

									BotFixIdealYaw(pEdict);

									SetMoveSpeed(MoveSpeed::stop);  // don't move while turning
									f_dont_avoid_wall_time = gpGlobals->time + 1.0;
#ifdef DEBUG
									if (botdebugger.IsDebugStuck())
										conOutput.Notify("***FREE ROAM*** Bot CheckWallOnLeft -> Random Turn 45 to 60 degrees!\n", this);
#endif // DEBUG
								}

								f_wall_on_left = 0;  // reset wall detect time
							}
							else if (!BotCheckWallOnRight( this ))
							{
#ifdef DEBUG
								if (botdebugger.IsDebugStuck())
									conOutput.Notify("***FREE ROAM*** Bot CheckWallOn RIGHT!\n", this);
#endif // DEBUG
								// if there was a wall on the right over 1/2 a second ago then 20% of the time randomly turn between 45 and 60 degrees
								if ((f_wall_on_right != 0) && (f_wall_on_right <= gpGlobals->time - 0.5) && (RANDOM_LONG(1, 100) <= 20))
								{
									pEdict->v.ideal_yaw -= RANDOM_LONG(45, 60);

									BotFixIdealYaw(pEdict);

									SetMoveSpeed(MoveSpeed::stop);  // don't move while turning
									f_dont_avoid_wall_time = gpGlobals->time + 1.0;
#ifdef DEBUG
									if (botdebugger.IsDebugStuck())
										conOutput.Notify("***FREE ROAM*** Bot CheckWallOnRight -> Random Turn 45 to 60 degrees!\n", this);
#endif // DEBUG
								}

								f_wall_on_right = 0;  // reset wall detect time
							}
						}
					}

					// check if bot is about to hit a wall.  TraceResult gets returned
					if ((f_dont_avoid_wall_time <= gpGlobals->time) && BotCantMoveForward( this, &tr ))
					{
						// ADD LATER
						// need to check if bot can jump up or duck under here...
						// ADD LATER


						BotTurnAtWall( this, &tr );
#ifdef DEBUG
						if (botdebugger.IsDebugStuck())
							conOutput.Notify("***FREE ROAM*** Bot TURN AT WALL!\n", this);
#endif // DEBUG
					}
				}
			}
		}
	}

	//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	//
	// section of actions that are done no matter whether the bot has an enemy or not
	// (for example weapon reload that needs to be done in battle as well as during standard navigation)
	//
	//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	// is it short after bot spawned/respawned? (ie. we should give the mod some time to finish spawning stuff ... like current weapon in Firearms 2.5 and below)
	if (IsTimeSinceBotSpawned(1.5f))//	orig value was 2.5
	{
		// is the weapon locked (ie. not available for any weapon specific action)?
		if (weapon_action == W_LOCKED)
		{
			// then unlock it
			weapon_action = W_READY;

			// also tell the bot to check his current weapon as soon as possible
			SetWeaponStatus(WS_CHECKWEAPON);

#ifdef DEBUG
			if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
				conOutput.Notify("***Weapon has been UNLOCKED\n", this);
#endif // DEBUG
		}

		// is bot in need to make post spawn decisions?
		if (IsNeed(NEED_POSTSPAWN_DECISIONS))
		{

			// here we force bot make a decision how to play the game for the duration of one "game life", these aren't static decisions like some sort of personality or behaviour,
			// these can change every time bot respawns to the game after being killed, say this is some sort of changing mood ... something like ...
			// "Okay now I'm in mood to reach the goal of this map" or "I'll pay attention to my bandages count, perhaps I'll stay alive longer this way"

			int chance = RANDOM_LONG(1, 100);

			// the need to reach map goals is based on behaviour and generated chance
			if (IsBehaviour(DEFENDER) && (chance > 45))
				SetNeed(NEED_GOAL);
			else if (IsBehaviour(STANDARD) && (chance > 66))
				SetNeed(NEED_GOAL);
			else if (IsBehaviour(ATTACKER) && (chance > 80))
				SetNeed(NEED_GOAL);

			// we generate new chance value in order to...
			chance = RANDOM_LONG(1, 100);

			// give Staff Sergeant and Master Sergeant, their counterparts on Axis side and Sergeant Major in British Troops much higher chance to go for map goals in cases when they didn't already pick it above
			if ((IsNeed(NEED_GOAL) == false) && (chance > 20) && (IsBotTeam(teamTWO.GetTeamId()) && ((GetBotClass() == 2) || (GetBotClass() == 3))) ||
				(IsBotTeam(teamONE.GetTeamId()) && (GetBotClass() == 2)) || (IsBotTeam(teamONE.GetTeamId()) && (GetBotClass() == 3) && (internals.IsBritishTeam() == false)))
				SetNeed(NEED_GOAL);

			// print these messages for the waypointers so that they have an idea how will this bot behave on junctions (cross waypoints), it's purely for waypoint testing/debugging
			if (botdebugger.IsDebugCross() || botdebugger.IsDebugPaths() || botdebugger.IsDebugWaypoints())
			{
				if (IsNeed(NEED_GOAL))
					conOutput.Notify("***decided to go for map objectives (pushpoints or s&d objects)\n", this);
				else
					conOutput.Notify("***decided NOT to go for map objectives (pushpoints or s&d objects)\n", this);
			}

			if ((IsEquippedWithExplosiveCharge() == false) && (IsNeed(NEED_GOAL) || ((IsNeed(NEED_GOAL) == false) && (RANDOM_LONG(1, 100) > 80))))
				SetNeed(NEED_EXLOSIVESCHARGE);

			// is the bot leader? then almost always try to form a fireteam
			if (util.CanBeFireTeamLeader(this) && (chance > 5))
			{
				SetNeed(NEED_FIRETEAM);
				// fake speak time allows postponing the fireteam gathering which is needed to get enough data about the spawn point such as going to parachute in
				SetSpeakTime();
			}

			// decide just once
			RemoveNeed(NEED_POSTSPAWN_DECISIONS);
		}
	}

	// does the bot have a waypoint?
	if ((curr_wpt_index != NO_VAL) && CanLookForWaypoint())
	{
		// is next waypoint the bot is heading towards to...
		
		// a crouch waypoint then duck down while moving forward
		// this one must be here else the bot would NOT go crouch while heading towards this waypoint he would crouch only for a short moment after passing through the waypoint
		if (wptmanager.IsWaypoint(curr_wpt_index, WptT::crouch))
		{
			if (IsBehaviour(BOT_PRONED))
				GoProne("BThink()|curr_wpt not -1 -> At CROUCH WPT but in prone!");
			else
				SetStance(GOTO_CROUCH, "BThink()|curr_wpt not -1 -> GOTO crouch at CROUCH WPT");
		}

		// a sniper waypoint then set don't move flag
		//if (waypoints[curr_wpt_index].flags & W_FL_SNIPER)
		//{
			//SetTask(TASK_DEATHFALL);
		//}
	}

	// the bot has to scan forward direction to detect dangerous depths ie. danger of a death fall
	if (IsTask(TASK_DEATHFALL) && (check_deathfall_time < gpGlobals->time))
	{
		// set next check
		check_deathfall_time = gpGlobals->time + 0.2f;

		if (IsDeathFall(pEdict) || IsForwardBlocked(this))
		{
			// if the bot has enemy and there's a dangerous depth in front of the bot then stop
			if (pBotEnemy != NULL)
			{
				SetMoveSpeed(MoveSpeed::stop);
				SetDontMoveTime(1.0f);
				SetDontCheckStuck("BThink() -> TaskDeathFall", 1.5f);
			}
			// no enemy ie normal navigation
			//else
			//{
			//		TODO: turn the bot back (something like with parachute wpt
			//			when the bot has no chute
			//}
		}
	}

	// is bot currently using one of the rocket launchers?
	if (IsRPG(current_weapon.iId) && IsWeaponReady())
	{
		// is RPG in ready to fire state?
		if (IsGrenadeLauncherReadyToFire(pEdict, current_weapon.iId))
		{
			ActivateWeaponSecondaryMode();// then let him know that he's shouldered the rocket launcher (ie. it's ready to fire it)


			//@@@@@@@@@@@@@
#ifdef _DEBUG
			if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
			{
				if (IsWeaponReady())
					conOutput.Notify("BThink()|check RPG -> is SHOULDERED now (weapon sec mode state: activated)\n", this);
			}
#endif

		}
		
		if (IsGrenadeLauncherOffShoulder(pEdict))
		{
			DeactivateWeaponSecondaryMode();// otherwise secondary fire mode isn't active (ie. the launcher isn't shouldered)


			//@@@@@@@@@@@@@
#ifdef _DEBUG
			if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
			{
				if (IsWeaponReady())
					conOutput.Notify("BThink()|check RPG -> is taken OFF shoulder now (weapon sec mode state: deactivated)\n", this);
			}
#endif


		}
	}

	// is the bot in process of changing his current weapon?
	if ((weapon_action == W_TAKEOTHER) || (weapon_action == W_INCHANGE) || (weapon_action == W_INHANDS))
	{
		// we can't change current weapon if the bot is paused, because that means
		// he's under medical treatment from his teammate and FA doesn't allow any weapon based action in such cases
		if (IsNotPaused())
		{
			

			//@@@@@@@@@@@@@22
#ifdef _DEBUG
			if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
			{
				conOutput.Notify("BThink() -> weaponaction == takeother or inchange\n", this);
			}
#endif



			ChangeWeapon();
		}
	}

	// or is the bot reloading his current weapon?
	// we must check for pause time, because teammate giving a medical treatment invalidates the reload action and the bot must wait till it's over
	// then he can reset the reloading and start it anew
	else if ((weapon_action == W_INRELOAD) && IsNotPaused())
	{
		SetMoveSpeed(MoveSpeed::stop);
		SetDontCheckStuck("BThink() -> reloading weapon");

		SetDontLookForWaypoint();
		SetTimeToReachCurrWaypoint();

		// did bot decide to reload his weapon while waiting somewhere?
		if (IsWaitTime())
		{
			// then keep increasing the wait time together with increasing global time so that he won't leave that waypoint before finishing the waiting duties
			SetWaitTime(GetWaitTime() - GetPreviousGlobalsTime());
		}

		// to prevent bot going prone while reloading weapon, because doing so in FA will invalidate whole reload proccess and you end up with empty gun
		// (although you can see weapon reloading animation)
		SetBehaviour(BOT_DONTGOPRONE);

		// didn't the bot press reload button yet?
		if (IsNotReloadingWeapon() && (IsWeaponStatus(WS_PRESSRELOAD) == false) && IsNotGoingProne() && IsNotDeployingBipod())
		{
			// then do so now
			pEdict->v.button |= IN_RELOAD;

			// to know the bot did it
			SetWeaponStatus(WS_PRESSRELOAD);

			// we need correct time to finish reloading the weapon
			DefineReloadTimeForCurrentWeapon(this);



			//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@			NEW CODE 094 (remove it)
#ifdef DEBUG
			if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons() || devTool.IsBotDebugging(pEdict))
			{
				char dp[128]{};
				sprintf(dp, "%s's weapon reloading will be finished at %.3f at most\n", name, GetWeaponReloadTime());
				util.DebugInFile(dp);

			}
#endif // DEBUG




			// see if are reloading only partly used magazine
			if (IsWeaponStatus(WS_NOTEMPTYMAG))
			{
				// reaching two partly used magazines will call a mergeclips command
				// what we don't handle here is fact that each partly used magazine may be from different weapon (main or backup),
				// however merging magazines doesn't cause any fatal problems in Firearms so if the bot is going to use it at wrong moment isn't big issue

				RemoveWeaponStatus(WS_NOTEMPTYMAG);
			}

			if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
				conOutput.Notify("***Reload button pressed\n", this);
		}
		// is weapon still NOT reloaded?
		else if (IsCurrentWeaponEmpty() && IsNotReloadingWeapon())
		{
			// if the reloading was invalidated ...
			if (IsWeaponStatus(WS_INVALID))
			{
				// clear the "button pressed" bit
				RemoveWeaponStatus(WS_PRESSRELOAD);

				// clear "reload attached grenade launcher" bit
				RemoveWeaponStatus(WS_RELOADSECONDARY);

				// clear the reset bit to prevent running this again
				RemoveWeaponStatus(WS_INVALID);

				// prevent the bot to go crouch, because there's no reloading now
				SetStanceChangeTime(0.2f);

				// and reset the action
				SetWeaponReloadTime(0.0f);
				weapon_action = W_READY;

				/*/
				// seems like these weapons don't want to work standard way so we'll force a switching to knife and
				// let the weapon management system return back to these and reload them properly
				if ((current_weapon.iId == fa_weapon_g36e) || (current_weapon.iId == fa_weapon_pkm))
				{
					UseWeapon(uWeapon::knife);
					weapon_action = W_TAKEOTHER;
				}
				/**/

				if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
					conOutput.Notify("***Reloading was invalidated -> going to do it again", this);
			}
			else
			{
				// something must have gone wrong so we'll invalidate this try and try it anew
				SetWeaponStatus(WS_INVALID);
				// to keep the bot in reload "action"
				SetWeaponReloadTime(0.1f);

#ifdef _DEBUG

				if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
				{
					if (IsWeaponStatus(WS_PRESSRELOAD))
					{
						char dbgmsg[256]{};
						sprintf(dbgmsg, "(BThink()|in reload) weaponID=%d still not loaded -> reload was invalidated (FAver=%d)\n", current_weapon.iId, g_mod_version);
						conOutput.Notify(dbgmsg, this);
					}
					else
						conOutput.Notify("(BThink()|in reload) -> button wasn't pressed yet! -> THIS IS BUG !!!", this);
				}
#endif
			}

		}
		// weapon or the chamber on attached GL isn't empty anymore AND reload time is over? then we must have successfully reloaded this weapon...
		else if ((IsCurrentWeaponEmpty() == false) && IsNotReloadingWeapon())
		{
			// so mark the weapon ready for action again and clear all related variables
			weapon_action = W_READY;
			RemoveWeaponStatus(WS_PRESSRELOAD);
			RemoveWeaponStatus(WS_RELOADSECONDARY);
			RemoveWeaponStatus(WS_INVALID);
			SetWeaponReloadTime(0.0f);

			if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
				conOutput.Notify("***Weapon fully loaded -> WEAPON is READY\n", this);

			
			
			// NEW CODE 094 (remove it)
#ifdef _DEBUG
			//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
			//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
			char dbgmsg[256]{};
			sprintf(dbgmsg, "(BThink()|in reload) weapon fully loaded (iclip=%d) -> weapon action set to ready\n", current_weapon.iClip);
			//conOutput.Notify(dbgmsg, true, this);
#endif


		}

		// if NOT proned then crouch
		if ((IsBehaviour(BOT_PRONED) == false) && (IsTask(TASK_BIPOD) == false))
			SetStance(GOTO_CROUCH, "BThink()|InReload -> GOTO crouch");
	}

	// or do we need to check weapons for ammunition that should be taken from ammobox AND is it time to do it yet?
	else if (IsTask(TASK_CHECKAMMO) && IsTimeToCheckAmmoReserves())
		CheckAmmoReserves("BThink() -> is task AND is time for it");

	// show main_weapon, backup_weapon, grenade_slot and claymore_slot weapon names and which of them is currently used
	if (botdebugger.IsDebugWeapons())
		util.PrintAvailableWeapons(this);

	// the bot is being hit by his teammate
	if (IsSubTask(ST_SAY_CEASEFIRE))
	{
		// check if the damage inflictor didn't change yet and if it is really a teammate
		if ((pEdict->v.dmg_inflictor != pEdict) && (pEdict->v.dmg_inflictor->v.netname != NULL) && util.AreTeammates(pEdict->v.dmg_inflictor, pEdict))
			UseTextMessage(botSay::cease_fire, pEdict->v.dmg_inflictor);

		RemoveSubTask(ST_SAY_CEASEFIRE);
	}

	// is the bot tasked to use one of radio or voice commands or a hand signal AND can he do so now?
	if (IsTask(TASK_SPEAK) && NotSpokeFor(0.0f))
	{
		BotCommunicateWithOthers(this);
	}

	// check whether the bot is stuck (in other words he didn't move much since the last location)
	// but don't check stuck if bot is on a ladder (since ladder stuck is handled elsewhere) or if bot is turning a lot at waypoint
	if ((moved_distance <= 1.0f) && (GetMoveSpeed() > MoveSpeed::stop) && IsTimeToCheckStuck() && (pEdict->v.movetype != MOVETYPE_FLY) && IsNotTurningToFaceWaypoint())
	{
		bool found_solution = false;
		float max_strafe_time = 1.5f;	// used to set the duration of moving sideways as well as a check time since last time the bot was stuck

		SetDontLookForWaypoint(0.4f);

		if (GetUnstuckAttempts() < 1)
			ResetAims("Got STUCK");

		if (botdebugger.IsDebugStuck())
		{
#ifdef DEBUG
			char smsg[128]{};
			sprintf(smsg, "***STUCK*** has been called!!! (prevspeed=%d|movespeed=%d|moveddist=%.2f|isCrouch=%d)\n", GetPrevMoveSpeed(), GetMoveSpeed(), moved_distance, IsCrouched());
			conOutput.Notify(smsg, this);

			sprintf(smsg, "***STUCK*** part 2 (isStanceGotoStanding=%d|isStanceGotoCrouch=%d|isStanceGotoProne=%d)\n",
				IsBehaviour(GOTO_STANDING), IsBehaviour(GOTO_CROUCH), IsBehaviour(GOTO_PRONE));
			conOutput.Notify(smsg, this);
#else
			conOutput.Notify("***STUCK*** seems to be stuck ...\n", this);
#endif // DEBUG

		}

		// don't start any new action until the bot finishes current try to free self
		if (DoesBotStrafeNow() == false)
		{
			// the bot is in front of a breakable object
			if (util.CheckForBreakableAround(this, STANDARD_SEARCH_RADIUS / 2.0f))
			{
				// can it be broken at all?
				if ((util.NotBreakableByGunfire(GetPointerToGEnt()) == false) && (util.IsEntityBreakableByExplosivesOnly(GetPointerToGEnt()) == false))
				{
					SetMoveSpeed(MoveSpeed::stop);
					SetTask(TASK_FIRE);
					SetTask(TASK_IGNOREAIMWPTS);

					if (botdebugger.IsDebugStuck())
						conOutput.Notify("***STUCK*** on breakable object -> shoot it\n", this);
				}
				// if it cannot be broken then try moving away from this spot
				else if (ReturnBackToPathStart("***STUCK*** on breakable object"))
				{
					// current waypoint is on the other side of the unbreakable obstacle so it's unreachable and standard navigation cannot handle it,
					// therefore we are going to call for a new waypoint outside of standard navigation, we've already changed the direction bot moves on his current path,
					// so calling for next waypoint will return bot to previously visited waypoint which must be on his side of the obstacle so it must be fully reachable
					SetCurrentWaypoint(GetNextWaypointOnPath());

					if (botdebugger.IsDebugStuck())
						conOutput.Notify("***STUCK*** on breakable object, but cannot break it -> returning back\n", this);
				}

				IncUnstuckAttempts();
				found_solution = true;
			}
			// is bot being "used" AND is very close to team leader? then just stop
			else if ((pTeamLeader != NULL) && ((pEdict->v.origin - pTeamLeader->v.origin).Length() < 150.0f))
			{
				SetMoveSpeed(MoveSpeed::stop);

				if (botdebugger.IsDebugStuck())
					conOutput.Notify("***STUCK*** while following TeamLeader, is quite close so just stop\n", this);
			}
			// can the bot jump (ie has enough stamina to jump and not lying or going to or resume from prone) and can jump onto something?
			else if ((IsTask(TASK_NOJUMP) == false) && (IsBehaviour(BOT_PRONED) == false) && IsNotGoingProne() && BotCanJumpUp(this))
			{
				pEdict->v.button |= IN_JUMP; // jump up and move forward

				if (IsCrouched())
				{
					pEdict->v.button |= IN_DUCK;	// duck is a must in this case to stay in crouch stance
					SetDuckJumpTime(1.0f);			// and keep the "crouch" key pressed for a while
				}

				// don't turn away from the object you are trying to jump up
				f_dont_avoid_wall_time = gpGlobals->time + 1.0f;

				IncUnstuckAttempts();
				found_solution = true;

				if (botdebugger.IsDebugStuck())
					conOutput.Notify("***STUCK*** can jump over it\n", this);
			}
			// can the bot get onto or over the object by a duck jump? (eg. barrel, sandbag, anything that is too high for standard jump) 
			else if ((IsTask(TASK_NOJUMP) == false) && (IsBehaviour(BOT_PRONED) == false) && IsNotGoingProne() && BotCanDuckJumpUp(this))
			{
				pEdict->v.button |= IN_JUMP;
				pEdict->v.button |= IN_DUCK;
				SetDuckJumpTime(1.0f);
				f_dont_avoid_wall_time = gpGlobals->time + 1.0f;
				IncUnstuckAttempts();
				found_solution = true;

				if (botdebugger.IsDebugStuck())
					conOutput.Notify("***STUCK*** can DUCKJUMP over it\n", this);
			}
			// can the bot jump into or through the object by a duck jump? (eg. some small window, ventilation shaft or a manhole in general)
			else if ((IsTask(TASK_NOJUMP) == false) && (IsBehaviour(BOT_PRONED) == false) && IsNotGoingProne() && BotCanDuckJumpInto(this))
			{
				// is already crouched? then duck jump
				if (IsCrouched())
				{
					pEdict->v.button |= IN_JUMP;
					pEdict->v.button |= IN_DUCK;
					SetDuckJumpTime(1.0f);
				}
				// otherwise go crouch first
				else
					SetStance(GOTO_CROUCH, "BThink()|UnStuck|CanDuckJumpInto() -> GOTO crouch");
				
				f_dont_avoid_wall_time = gpGlobals->time + 1.0f;
				IncUnstuckAttempts();
				found_solution = true;

				if (botdebugger.IsDebugStuck())
					conOutput.Notify("***STUCK*** can DUCKJUMP through it\n", this);
			}
			// can the bot duck under something?
			else if (BotCanDuckUnder(this))
			{
				f_dont_avoid_wall_time = gpGlobals->time + 1.0f;
				IncUnstuckAttempts();
				found_solution = true;

				// if NOT proned AND NOT going to/resume from prone...
				if ((IsBehaviour(BOT_PRONED) == false) && IsNotGoingProne())
					SetStance(GOTO_CROUCH, "BThink()|UnStuck|CanDuckUnder() -> GOTO crouch");

				if (botdebugger.IsDebugStuck())
					conOutput.Notify("***STUCK*** can crouch under it\n", this);
			}
			else if (BotCantStrafeLeft(pEdict) == false)
			{
				StrafeLeftFor(RANDOM_FLOAT(0.5f, max_strafe_time));//					test this not sure about it (if bugged then lower range top value)
				IncUnstuckAttempts();

				if (botdebugger.IsDebugStuck())
					conOutput.Notify("***STUCK*** can strafe LEFT\n", this);
			}
			else if (BotCantStrafeRight(pEdict) == false)
			{
				StrafeRightFor(RANDOM_FLOAT(0.5f, max_strafe_time));
				IncUnstuckAttempts();

				if (botdebugger.IsDebugStuck())
					conOutput.Notify("***STUCK*** can strafe RIGHT\n", this);
			}
			else
			{
				MakeRandomTurn();
				IncUnstuckAttempts();
				found_solution = true;

				if (botdebugger.IsDebugStuck())
					conOutput.Notify("***STUCK*** doing a random turn\n", this);
			}

			// is the bot stuck again in a quite short time? ...
			// has he finished his try to sidestep the obstacle AND is NOT trying some solution to get free right now OR already tried to get free for too many times?
			if ((NotBeenStuckFor(max_strafe_time + 0.2f) == false) && ((found_solution == false) || (GetUnstuckAttempts() > 5)))
			{
				// is the bot fully proned so stand up from prone, because prone can easily get the bot stuck
				if (IsBehaviour(BOT_PRONED))
				{
					GoProne("BThink()|UnStuck -> STUCK in prone stance");
					SetStance(GOTO_STANDING, "BThink()|UnStuck->STUCK in prone -> GOTO standing");

					if (botdebugger.IsDebugStuck())
						conOutput.Notify("***STUCK*** in PRONE -> standing up\n", this);
				}
				// is the bot stuck while crouched then stand up
				else if (IsBehaviour(BOT_CROUCHED))
				{
					SetStance(GOTO_STANDING, "BThink()|UnStuck->STUCK in crouch -> GOTO standing");

					if (botdebugger.IsDebugStuck())
						conOutput.Notify("***STUCK*** in CROUCH -> standing up\n", this);
				}
				// is the bot stuck on dead player? then just wait for a while till the body disappears
				else if (IsEntityInSphere("bodyque", pEdict, 50.0f))
				{
					SetMoveSpeed(MoveSpeed::stop);
					SetDontMoveTime(1.0f);

					if (botdebugger.IsDebugStuck())
					{
						conOutput.Notify("***STUCK*** on DEAD BODY --> waiting till it disappers\n", this);
					}
				}
				// is bot stuck inside another player? (them both respawned at the same playerstart entity) we are looking for a entity named player in close vicinity while ignoring self
				else if (IsEntityInSphere("player", pEdict, 35.0f, pEdict))
				{
					// this will give the bot 3 tries before killing self
					IncUnstuckAttempts(7);

					if (botdebugger.IsDebugStuck())
					{
						conOutput.Notify("***STUCK*** INSIDE ANOTHER BODY --> about to commit suicide\n", this);
					}
				}
				else
				{
					// try turning to any direction, it might be free
					MakeRandomTurn();
					IncUnstuckAttempts();

					if (botdebugger.IsDebugStuck())
					{
						char dsm[256]{};
						sprintf(dsm, "***STUCK*** AGAIN - doing a random turn (# of tries %d)\n", GetUnstuckAttempts());
						conOutput.Notify(dsm, this);
					}
				}

				// did bot try getting free a few times already and is he still unable to do so? 
				if (GetUnstuckAttempts() > 20)
				{
					// is the only option left to commit a suicide?
					if (IsNeed(NEED_COMMITSUICIDE))
					{
						if (botdebugger.IsDebugStuck())
							conOutput.Notify("***STUCK*** NO WAY TO UNSTUCK SELF -> will commit SUICIDE\n", this);

						ClientKill(pEdict);
					}
					else
					{
						if (ReturnBackToPathStart("***STUCK*** COMPLETELY -> trying to return back"))
						{
							// current waypoint is obviously unreachable and standard navigation cannot handle it, reason why is bot stuck now, therefore we are going to call for
							// a new waypoint outside of standard navigation, we've already changed the direction bot moves on his current path, so calling for next waypoint on that path
							// will return bot to previously visited waypoint which could still be reachable and may allow bot leave this problem spot
							SetCurrentWaypoint(GetNextWaypointOnPath());

							if (botdebugger.IsDebugStuck())
								conOutput.Notify("***STUCK*** COMPLETELY -> trying to return back\n", this);
						}

						// also we must set a need to kill self in case bot won't be able to reach previous waypoint (ie. got stuck so that he cannot even move) or is on one-way path
						SetNeed(NEED_COMMITSUICIDE);
					}
				}
			}
			// otherwise store the time of current stuck
			else
			{
				SetGotStuckTime();



#ifdef DEBUG
				if (botdebugger.IsDebugStuck())
				{
					char stmsg[128]{};
					sprintf(stmsg, "***STUCK*** Set GotStuckTime (%.2f)\n", GetGotStuckTime());
					conOutput.Notify(stmsg);
				}
#endif // DEBUG



			}
		}
	}

	// does the bot need air because he is drowning now? then make him keep the jump key pressed to get out of water
	else if (IsNeed(NEED_AIR))
	{
		pEdict->v.button |= IN_JUMP;


		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@										NEW CODE 094 (remove it)
#ifdef _DEBUG
		conOutput.Notify("BThink()|NEED AIR (drowning) -> JUMP!\n", true, this);
#endif


	}

	// is bot tasked to sprint AND has no enemy AND is not reloading weapon?
	// (ie. we can't shoot while sprinting so when the bot has an enemy we can't set these two otherwise the bot won't be able to shoot his weapon, and we can't reload as well)
	else if (IsTask(TASK_SPRINT) && (pBotEnemy == NULL) && IsNotReloadingWeapon())// && (current_weapon.iId != fa_weapon_claymore))
	{
		// both must be set, because bot doesn't use in_forward normally
		pEdict->v.button |= IN_FORWARD;
		pEdict->v.button |= IN_RUN;


#ifdef _DEBUG
		//@@@@@@@@@@@@@@@@@
		//conOutput.Notify("***BThink() -> Keeping the SPRINT key pressed since now!\n", this);
#endif


	}

	// did the bot decide to go prone AND NOT already in prone?
	else if (IsBehaviour(GOTO_PRONE) && (IsBehaviour(BOT_PRONED) == false))
	{
		GoProne("BThink() -> Behaviour is GOTO prone");
	}

	//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	//
	// various flags clearing section
	//
	//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	// is some time after waypoint action AND TANK is NOT used
	if (IsTask(TASK_WPTACTION) && NotDoneActionFor(0.5f) && (IsTask(TASK_USETANK) == false))
	{
		SetPointerToGEnt(NULL);			// no entity anymore
		RemoveTask(TASK_WPTACTION);		// clear it
		RemoveSubTask(ST_FACEGENT_DONE);
		RemoveSubTask(ST_BUTTON_USED);
		RemoveSubTask(ST_TANK_SHORT);	// there's no mounted gun at all so this must be removed
		RemoveTask(TASK_CLAY_IGNORE);	// bot finished the action of moving towards the explosives charge spot to get it

		if (botdebugger.IsDebugActions())
			conOutput.Notify("***Waypoint action time is over - (actions cleared)\n", this);



		//@@@@@@@@@@@@@@@
#ifdef _DEBUG
		if (botdebugger.IsDebugActions())
		{
			char dm[256]{};
			sprintf(dm, "BThink()|Clearing -> (curr wpt #%d) Task WptAction RESET (wpt action Time is OVER)\n", curr_wpt_index + 1);
			conOutput.Notify(dm, this);
		}
#endif
	}

	// clear the Game Entity pointer once the bot doesn't need it
	if ((HasNoGEnt() == false) && (IsSubTask(ST_INAREA) == false) && (IsTask(TASK_WPTACTION) == false) && (IsTask(TASK_USETANK) == false) && (IsTask(TASK_FIRE) == false) && NotBeenWaitingFor(0.5f))
	{
		SetPointerToGEnt(NULL);			// no entity anymore
		RemoveSubTask(ST_RANDOMCENTRE);
		ResetAims("BThink()|Clearing -> Waiting is over");

#ifdef DEBUG
		if (botdebugger.IsDebugActions() || botdebugger.IsDebugWaypoints())
			conOutput.Notify("***Cleared pointer to GameEntity\n", this);
#else
		if (botdebugger.IsDebugActions())
			conOutput.Notify("***Cleared pointer to GameEntity\n", this);
#endif // DEBUG
	}

	// does the bot still have a Point in Space coordinates and the wait time is over (ie. is the action he needed it for finished)? then clear it
	if ((GetPositionOfPointInSpace() != g_vecZero) && NotBeenWaitingFor(0.5f))
	{
		SetPositionOfPointInSpace(g_vecZero);
		// just in case
		RemoveSubTask(ST_DOOR_OPEN);
		RemoveSubTask(ST_MEDEVAC_F);

		if (botdebugger.IsDebugActions())
			conOutput.Notify("***Cleared point position related actions\n", this);
	}

	if (IsTask(TASK_IGNOREWPTNAV) && NotBeenWaitingFor(0.5f) && CanLookForWaypoint())
	{
		RemoveTask(TASK_IGNOREWPTNAV);

		if (botdebugger.IsDebugActions())
			conOutput.Notify("***Cleared ignore waypoints task\n", this);
	}

	// wait time is over so bot can return back to standard weapon use management
	if (IsWeaponStatus(WS_DONTSWITCHTOOTHER) && NotBeenWaitingFor(0.5f))
	{
		RemoveWeaponStatus(WS_DONTSWITCHTOOTHER);

#ifdef DEBUG
		if (botdebugger.IsDebugWeapons() || botdebugger.IsDebugActions())
		{
			char dm[128]{};
			sprintf(dm, "BThink()|Clearing -> cleared DONT SWITCH TO OTHER weapon bit, because waiting is over\n");
			conOutput.Notify(dm, this);
		}
#endif // DEBUG
	}

	// bot didn't find any teammate near Area capture control point and the wait time is over then clear it or any new waiting would get messed up by it
	if (IsSubTask(ST_PARACHUTE_USED) && NotBeenWaitingFor(0.5f))
	{
		RemoveSubTask(ST_PARACHUTE_USED);

#ifdef DEBUG
		if (botdebugger.IsDebugActions())
		{
			char dm[128]{};
			sprintf(dm, "BThink()|Clearing -> cleared 'wait for teammate to show at cappoint', because waiting is over\n");
			conOutput.Notify(dm, this);
		}
#endif // DEBUG
	}

	if (IsTask(TASK_MEDEVAC) && NotBeenWaitingFor(0.5f))
	{
		RemoveTask(TASK_MEDEVAC);

#ifdef DEBUG
		if (botdebugger.IsDebugActions())
		{
			char dm[128]{};
			sprintf(dm, "BThink()|Clearing -> cleared 'wait for teammate to pick up the extra ammo', because waiting is over\n");
			conOutput.Notify(dm, this);
		}
#endif // DEBUG
	}

	// is the bot NOT on ladder but he still has set ladder variables so clear them all
	if ((GetLadderUseDirection() > LadderDir::unknown) && (pEdict->v.movetype != MOVETYPE_FLY) && (pEdict->v.flags & FL_ONGROUND) && IsTimeSinceStartOfUsingLadder(2.0f))
	{
		SetLadderUseDirection(LadderDir::unknown);
		ResetReachedEndOfLadder();

		if (botdebugger.IsDebugActions())
			conOutput.Notify("***Cleared ladder actions\n", this);
	}

	// is the need of air still set AND the bot is no more under water? then clear it
	if (IsNeed(NEED_AIR) && (pEdict->v.waterlevel < 3))
	{
		RemoveNeed(NEED_AIR);

		if (botdebugger.IsDebugActions())
			conOutput.Notify("***Bot is no more drowning (need air flag cleared)\n", this);
	}

	// is stuck time over and the suicide timer/counter isn't cleared yet so do it now
	if ((GetUnstuckAttempts() > 0) && (moved_distance > 2.0f) && NotBeenStuckFor(2.0f))
	{
		ResetUnstuckAttempts();
		RemoveNeed(NEED_COMMITSUICIDE);

		if (botdebugger.IsDebugActions() || botdebugger.IsDebugStuck())
			conOutput.Notify("***Bot is no more stuck (unstuck tries cleared)\n", this);
	}

	// if the bot moved then reset cannot bipod bit
	if (IsWeaponStatus(WS_CANTBIPOD) && (moved_distance > 2.0))
	{
		RemoveWeaponStatus(WS_CANTBIPOD);



#ifdef DEBUG
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@														// NEW CODE 094 (remove it)
		if (botdebugger.IsDebugWeapons())
		{
			char dm[128]{};
			sprintf(dm, "cleared CANTBIPOD bit @ BThink()|moved distance > 2.0\n");
			conOutput.Notify(dm, this);
		}
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#endif // DEBUG



	}

	// did the bot finished mergining magazines?
	if ((weapon_action == W_INMERGEMAGS) && IsNotPaused())
	{
		// then set the weapon back to ready for action
		weapon_action = W_READY;

		if (botdebugger.IsDebugActions())
			conOutput.Notify("***Merging magazines has been finished -> WEAPON is READY again\n", this);
	}

	// the bot failed to use claymore mine so the whole action needs to be reset
	if (IsNeed(NEED_RESETCLAYMORE))
	{
		if (botdebugger.IsDebugActions())
			conOutput.Notify("***Resetting claymore mine action\n", this);
	}

	// tried the bot go prone, but engine sent him 'cannot prone here' message...
	if (IsSubTask(ST_CANTPRONE) && (IsNotGoingProne() == false))
	{

#ifdef DEBUG
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@														// NEW CODE 094 (remove it)
		if (botdebugger.IsDebugStance())
		{
			char dm[128]{};
			sprintf(dm, "reset goproneTIME(=%.3f) and cleared GOTO standing @ BThink()|Clearing section. CHECK why does it happen!\n", GetGoProneTime() + 1.2f);
			conOutput.Notify(dm, this);
		}
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#endif // DEBUG




		// then reset going prone time so the bot can act normally in next game frame
		ResetGoProneTime();

		// and reset 'stand up' bit as well if the bot was trying to stand up, because it cannot be done now
		RemoveBehaviour(GOTO_STANDING);
	}

	// bot moved a little so see if we can reset the cannot go prone subtask
	if (IsSubTask(ST_CANTPRONE))
	{
		if (IsWeaponReady() && (IsTask(TASK_BIPOD) == false) && IsNotDeployingBipod() && IsNotGoingProne() && IsNotPaused())
		{
			if (moved_distance > 20.0f)
			{
				RemoveSubTask(ST_CANTPRONE);




#ifdef DEBUG
				//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@											// NEW CODE 094 (remove it)
				if (botdebugger.IsDebugStance())
				{
					char dm[128]{};
					sprintf(dm, "BThink()|CANTPRONE subtask -> cleared CANTPRONE subtask (moved_dist=%.2f)\n", moved_distance);
					conOutput.Notify(dm, this);
				}
				//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#endif // DEBUG
			}
			else if ((moved_distance < 20.0f) && (DoesBotStrafeNow() == false))
			{

				RemoveSubTask(ST_CANTPRONE);


#ifdef DEBUG
				//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@											// NEW CODE 094 (remove it)
				if (botdebugger.IsDebugStance())
				{
					char dm[128]{};
					sprintf(dm, "BThink()|CANTPRONE subtask -> cleared CANTPRONE subtask (moved_dist=%.2f)\n", moved_distance);
					conOutput.Notify(dm, this);
				}
				//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#endif // DEBUG





				if (IsTimeToCheckStuck())
				{
#ifdef DEBUG
					//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@											// NEW CODE 094 (remove it)
					if (botdebugger.IsDebugStance())
					{
						char dm[128]{};
						sprintf(dm, "BThink()|CANTPRONE subtask -> a try to STRAFE (moved_dist=%.2f)\n", moved_distance);
						conOutput.Notify(dm, this);
					}
					//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#endif // DEBUG

					if (BotCantStrafeLeft(pEdict) == false)
					{
						StrafeLeftFor( RANDOM_FLOAT(0.5f, 1.0f) );
					}
					else if (BotCantStrafeRight(pEdict) == false)
					{
						StrafeRightFor( RANDOM_FLOAT(0.5f, 1.0f) );
					}
				}
			}
		}
	}

	// if the bot is tasked to go/resume prone, but something else is blocking this action...
	if (IsTask(TASK_GOPRONE) && IsSubTask(ST_CANTPRONE))
	{
		// then prevent calling the command
		RemoveTask(TASK_GOPRONE);


#ifdef DEBUG
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@														// NEW CODE 094 (remove it)
		if (botdebugger.IsDebugStance())
		{
			char dm[128]{};
			sprintf(dm, "REMOVED TASK_GOPRONE @ BThink()|Clearing -> IsSubtask(cantprone)!\n");
			conOutput.Notify(dm, this);
		}
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#endif // DEBUG


	}

	if (IsTask(TASK_GOPRONE) && IsBehaviour(BOT_DONTGOPRONE))
	{
		// then prevent calling the command
		RemoveTask(TASK_GOPRONE);


#ifdef DEBUG
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@														// NEW CODE 094 (remove it)
		if (botdebugger.IsDebugStance())
		{
			char dm[128]{};
			sprintf(dm, "REMOVED TASK_GOPRONE @ BThink()|Clearing -> Isbehaviour(bot dontgoprone)!\n");
			conOutput.Notify(dm, this);
		}
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#endif // DEBUG


	}

	// is there still behaviour don't go prone AND the bot already finished reloading his weapon
	// AND doesn't use bipod anymore AND finished merging magazines (ie. is not paused)
	// AND the engine allows going prone on current location
	// AND bot has no enemy or his enemy is quite far from him
	if (IsBehaviour(BOT_DONTGOPRONE) && IsWeaponReady() && (IsTask(TASK_BIPOD) == false) && IsNotDeployingBipod() && IsNotPaused() &&
		IsNotGoingProne() && (IsSubTask(ST_CANTPRONE) == false) && ((pBotEnemy == NULL) || (pBotEnemy && ((pBotEnemy->v.origin - pEdict->v.origin).Length() > 1000.0f))))
	{
		// then it's time to remove such behaviour
		RemoveBehaviour(BOT_DONTGOPRONE);


#ifdef DEBUG
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@														// NEW CODE 094 (remove it)
		if (botdebugger.IsDebugStance())
		{
			char dm[128]{};
			sprintf(dm, "REMOVED behaviour BOT DONTGOPRONE @ BThink() -> clearing section\n");
			conOutput.Notify(dm, this);
		}
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#endif // DEBUG


	}

	// if the bot is proned
	if (IsBehaviour(BOT_PRONED))
	{
		//  then he CANNOT crouch
		if (IsBehaviour(GOTO_CROUCH))
			RemoveBehaviour(GOTO_CROUCH);

		// and he CANNOT go prone again
		if (IsBehaviour(GOTO_PRONE))
			RemoveBehaviour(GOTO_PRONE);
	}

	// if the bot is crouched
	if (IsBehaviour(BOT_CROUCHED))
	{
		// he CANNOT go prone
		if (IsBehaviour(GOTO_PRONE))
		{
			RemoveBehaviour(GOTO_PRONE);


#ifdef DEBUG
			//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@														// NEW CODE 094 (remove it)
			if (botdebugger.IsDebugStance())
			{
				char dm[128]{};
				sprintf(dm, "REMOVED GOTO prone @ BThink()|Clearing -> Behaviour is BOT CROUCHED\n");
				conOutput.Notify(dm, this);
			}
			//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#endif // DEBUG
		}

		// he CANNOT go prone
		if (IsTask(TASK_GOPRONE))
		{
			RemoveTask(TASK_GOPRONE);


#ifdef DEBUG
			//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@														// NEW CODE 094 (remove it)
			if (botdebugger.IsDebugStance())
			{
				char dm[128]{};
				sprintf(dm, "REMOVED TASK GoProne @ BThink()|Clearing -> Behaviour is BOT CROUCHED\n");
				conOutput.Notify(dm, this);
			}
			//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#endif // DEBUG
		}
	}

	// if the bot is standing
	if (IsBehaviour(BOT_STANDING))
	{
		// then he CANNOT be even more standing
		if (IsBehaviour(GOTO_STANDING))
		{
			RemoveBehaviour(GOTO_STANDING);



#ifdef DEBUG
			//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@														// NEW CODE 094 (remove it)
			if (botdebugger.IsDebugStance())
			{
				char dm[128]{};
				sprintf(dm, "REMOVED GOTO standing @ BThink()|Clearing -> Behaviour is BOT STANDING\n");
				//conOutput.Notify(dm, this);
			}
			//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#endif // DEBUG
		}
	}

	// reset the strafe direction if bot doesn't move sideways
	if ((strafe_direction != 0.0f) && (DoesBotStrafeNow() == false))
	{
		strafe_direction = 0.0f;
	}

	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
	//
	// END - various flags clearing section
	//
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

	// see if the bot is trying to resume from prone, but is missing the task to do so
	// This happens when the bot was stuck in prone, the obstacle blocking him is gone now and he is free again
	if (IsBehaviour(GOTO_STANDING) && IsBehaviour(BOT_PRONED) && (IsTask(TASK_GOPRONE) == false))
		GoProne("BThink() -> Behaviour is Proned + gotoStanding but NO TASK GoProne!");

	// see if bot has to go/resume prone AND NOT already doing so
	if (IsTask(TASK_GOPRONE) && IsNotGoingProne())
	{
		// if the bot is lying prone then don't check for stuck for a little while, because FireArms stops the player (no forward speed so no position change)
		// when calling the prone command therefore we must disable unstuck routines for next few frames otherwise the bot would incorrectly start to strafe
		if (IsProne())
		{
			SetDontCheckStuck("BThink()|Task GOPRONE -> standing up", 0.5f);

#ifdef DEBUG
			//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@														// NEW CODE 094 (remove it)
			if (botdebugger.IsDebugStance())
			{
				char dm[128]{};
				sprintf(dm, "called cmd 'prone' -> SET goproneTIME, cleared TASK GOPRONE and SET DontCheckSTUCK %0.1fs\n", dont_check_stuck_time - gpGlobals->time);
				conOutput.Notify(dm, this);
			}
			//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#endif // DEBUG




		}
		else
		{
			// is bot moving towards the enemy in battle?
			if (IsNotAdvancingTowardEnemy() == false)
			{
				// then stop moving so that we can start shooting at him or deploy the bipod right after fully lying prone
				SetAdvanceTowardEnemyTime(0.0f);


#ifdef DEBUG
				//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@														// NEW CODE 094 (remove it)
				if (botdebugger.IsDebugStance())
				{
					char dm[128]{};
					sprintf(dm, "BThink()|Task GOPRONE -> going prone now so resetting AdvanceTowardsEnemy time\n");
					conOutput.Notify(dm, this);
				}
				//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#endif // DEBUG


			}



#ifdef DEBUG
			//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@														// NEW CODE 094 (remove it)
			if (botdebugger.IsDebugStance())
			{
				char dm[128]{};
				sprintf(dm, "called cmd 'prone' -> SET goproneTIME and cleared TASK GOPRONE\n");
				conOutput.Notify(dm, this);
			}
			//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#endif // DEBUG



		}



		// call the client command
		FakeClientCommand(pEdict, "sprone", NULL, NULL);

		// store the moment when the bot called the command
		SetGoProneTime();

		// finally we have to reset the task
		RemoveTask(TASK_GOPRONE);
	}

	// is the bot still in duckjump OR has to be crouched?
	if (IsDoingDuckJumpNow() || IsBehaviour(GOTO_CROUCH) || (IsBehaviour(BOT_CROUCHED) && !IsBehaviour(GOTO_STANDING)))
	{
		pEdict->v.button |= IN_DUCK;		// then press the crouch key
	}

	// is the bot in vicinity of small range waypoints AND the bot doesn't sprint at this moment AND he is still quite fast AND is not heading towards crouch or prone waypoint
	if (IsInCrampedSpace() && (IsTask(TASK_SPRINT) == false) && (GetMoveSpeed() > MoveSpeed::slow) && (wptmanager.IsWaypoint(curr_wpt_index, WptT::crouch, WptT::prone) == false))
	{
		// then slow down
		SetMoveSpeed(MoveSpeed::slow);


#ifdef _DEBUG
		//@@@@@@@@@@@@@
		//ALERT(at_console, "***botThink() - cramped space behaviour -> speed slow!!!\n");
#endif


	}

	// if the bot is trying to duckjump over something then we must keep full movement speed therefore we must have this statement here to override all previous 'slow down' commands
	if (IsDoingDuckJumpNow())
	{
		SetMoveSpeed(MoveSpeed::max);

		// we must also override all previously set times that disabled checking for possible stuck because jumping can lead to a 'I'm stuck' situation
		SetDontCheckStuck("BThink()|is DuckJump time", 0.3f);


#ifdef _DEBUG
		//@@@@@@@@@@@@@
		//ALERT(at_console, "***botThink() - it's duckjump time -> max speed!!!\n");
#endif



	}

	// if the bot cannot move then his speed must be no speed
	if (IsDontMoveTime())
	{
		SetMoveSpeed(MoveSpeed::stop);
		SetDontCheckStuck("BThink()|is DontMove time", 0.3f);
	}

	// determine sideway moves
	if (DoesBotStrafeNow())
	{
		f_dont_avoid_wall_time = gpGlobals->time + 1.0f;
		SetMoveSpeed(MoveSpeed::stop);
		SetDontLookForWaypoint(0.1f);

		if (strafe_direction == -1.0f)
		{
			pEdict->v.button |= IN_MOVELEFT;
		}
		else
		{
			pEdict->v.button |= IN_MOVERIGHT;
		}

		f_strafe_speed = strafe_direction * (GetMaxSpeed() / 2.0f);


#ifdef DEBUG

		if (botdebugger.IsDebugStuck())
			conOutput.Notify("***Strafe TIME NOW\n", this);

#endif // DEBUG



	}

	// save current move speed as previous move speed (for checking if stuck)
	UpdatePrevMoveSpeed();

	// save current health as previous health (for bleeding checks)
	UpdatePrevHealth();

	// save current time as previous
	UpdatePreviousGlobalsTime();

#ifdef DEBUG

	// print some debugging info only about one bot
	if (devTool.IsBotDebugging(pEdict))
	{
		/*/
		char msg[255];

		if (pEdict->v.movetype == MOVETYPE_FLY)
		{
			sprintf(msg, "The bot is on ladder (Dir<1-up,2-down,0-uknown> %d | LadderStartT %.1f | GlobT %.1f)\n", GetLadderUseDirection(), start_ladder_time, gpGlobals->time);
			conOutput.Notify(msg);
		}

		if ((f_shoot_time > gpGlobals->time) && ((weapon_action == W_TAKEOTHER) || (weapon_action == W_INCHANGE)))
		{
			sprintf(msg, "(GlobTime:%.2f)The bot can't shoot because of weapon change\n", gpGlobals->time);
			conOutput.Notify(msg);
			sprintf(msg, "NOTE If this message is printed over and over again for a long time then there seems to be a bug somewhere in weapon change code\n");
			conOutput.Notify(msg);
			sprintf(msg, "The bot class is %d. Current weapn ID is %d. Forced weapon is %d. Report it on forums\n", GetBotClass(), current_weapon.iId, used_weapon);
			conOutput.Notify(msg);
		}
		/**/
		
		/*/
		sprintf(msg, "CurrZoom=%.0f | IsSecModeActive=%d | iuser3(Scope)=%d | WeaponSilencer=%d | FireMode=%d\n",
			pEdict->v.fov, IsWeaponSecondaryModeActive(), pEdict->v.iuser3, current_weapon.iAttachment, current_weapon.iFireMode);
		/**/

		/*/
		sprintf(msg, "CurrRealSpeed=%.0f | MoveSpeed=%d | PrevSpeed=%d | 2DDistToCurrWpt=%.0f\n",
			pEdict->v.velocity.Length2D(), GetMoveSpeed(), GetPrevMoveSpeed(), (GetCurrWptPostion() - pEdict->v.origin).Length2D());
		/**/
		//conOutput.Notify(msg);

		/*/
		if (pBotEnemy)
		{
			sprintf(msg, "Current enemy is %s\n", STRING(pBotEnemy->v.netname));
			conOutput.Notify(msg);
		}
		/**/

		/*/
		if (botdebugger.IsDebugWeapons())
			util.PrintAvailableWeapons(this);
		/**/
	}

	// just for debugging
	if (IsWeaponStatus(WS_TEST_INATTACK))
	{
		pEdict->v.button |= IN_ATTACK;
		RemoveWeaponStatus(WS_TEST_INATTACK);
	}
	else if (IsWeaponStatus(WS_TEST_INATTACK2))
	{
		pEdict->v.button |= IN_ATTACK2;
		RemoveWeaponStatus(WS_TEST_INATTACK2);
	}

#endif // DEBUG


	// is the bot fully crouched AND doesn't do any action where we need him to use the pitch angle?
	if (IsCrouched() && (IsTask(TASK_HEALHIM) == false) && (wptmanager.IsWaypoint(curr_wpt_index, WptT::ladder) == false) && CanResetPitch())
	{
		// then look/aim directly forward
		pEdict->v.idealpitch = 0.0f;

		BotFixIdealPitch(pEdict);
	}

	// did bot deploy the bipod AND not having an enemy?
	if (IsTask(TASK_BIPOD) && (pBotEnemy == NULL))
	{
		// then prevent him from trying to turn around at will which would result in weird shivers
		pEdict->v.ideal_yaw = GetBipodYawAngle();

		BotFixIdealYaw(pEdict);
	}

	pEdict->v.v_angle.z = 0;  // reset roll to 0 (straight up and down)

	// set the body angles same as the bot head angles are (ie to the direction bot is looking/aiming)
	pEdict->v.angles.x = -pEdict->v.v_angle.x / 3;
	pEdict->v.angles.y = pEdict->v.v_angle.y;
	pEdict->v.angles.z = pEdict->v.v_angle.z;

	g_engfuncs.pfnRunPlayerMove(pEdict, pEdict->v.v_angle, ConvertMoveSpeedToRealValue(), f_strafe_speed, 0.0f, pEdict->v.button, 0, msecval);

	return;
}


AimWptIndex_t::AimWptIndex_t()
{
	Clear();
}

void AimWptIndex_t::AddNewAimWpt(int newVal)
{
	for (int i = 0; i < size; i++)
	{
		if (aim_index[i] == NO_VAL)
		{
			aim_index[i] = newVal;
			return;
		}
	}
}

void AimWptIndex_t::Clear(void)
{
	for (int i = 0; i < size; i++)
	{
		aim_index[i] = NO_VAL;
	}
}

int AimWptIndex_t::Count(void)
{
	int count = 0;

	for (int i = 0; i < size; i++)
	{
		if (aim_index[i] != NO_VAL)
			count++;
	}

	return count;
}

int AimWptIndex_t::Get(int array_index)
{
	if ((array_index >= 0) && (array_index < size))
		return aim_index[array_index];

	return NO_VAL;
}

int AimWptIndex_t::Print(int array_index)
{
	if ((array_index >= 0) && (array_index < size))
	{
		return aim_index[array_index] + 1;
	}

	return NO_VAL;
}

