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
// bot_combat.cpp
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
#include "console_output.h"

extern bot_weapon_t weapon_defs[MAX_WEAPONS];

float rg_modif = 2.5f;		// multiplier that modifies all effective ranges for game purpose

// used when current enemy is quite far and bot isn't a sniper
float g_time_to_check_for_closer_enemy[BOT_SKILL_LEVELS] = { 1.5f, 2.0f, 4.0f, 7.0f, 12.0f };

// better bot = longer delay between two sprints towards enemy (i.e. better bots hold their positions more often)
float g_combat_advance_delay[BOT_SKILL_LEVELS] = { 7.0f, 5.5f, 4.0f, 2.5f, 1.0f };

// times used to generate the next check of combat stance
float g_time_of_next_combat_stance_check_min[BOT_SKILL_LEVELS] = { 1.0f, 2.0f, 3.0f, 5.0f, 7.0f };
float g_time_of_next_combat_stance_check_max[BOT_SKILL_LEVELS] = { 2.0f, 3.5f, 5.0f, 7.5f, 10.0f };

// time to put grenade launcher on/off shoulder, values are based on in game tests,
// best time is what the engine allows us (delay between the switch command and firing the nade) and all the other times are increased a little to represent less skilled bots
float g_time_to_ready_gl[BOT_SKILL_LEVELS] = { 2.3f, 2.5f, 2.8f, 3.2f, 3.7f };

// times used to generate the duration of fully automatic fire for submachine guns
float g_fullautofire_min_delay[BOT_SKILL_LEVELS] = { 0.3f, 0.6f, 0.9f, 1.2f, 1.5f };
float g_fullautofire_max_delay[BOT_SKILL_LEVELS] = { 0.7f, 1.3f, 1.9f, 2.5f, 3.1f };

// times used to generate the duration of fully automatic fire for machine gun
float g_fullautofire_min_delay_mg[BOT_SKILL_LEVELS] = { 0.3f, 0.8f, 1.3f, 2.5f, 4.6f };
float g_fullautofire_max_delay_mg[BOT_SKILL_LEVELS] = { 0.7f, 1.3f, 1.9f, 3.2f, 5.4f };

#ifdef _DEBUG

bool in_bot_dev_level1 = FALSE;		// if TRUE print basic info at_console

#endif

// BotFireWeapon(), BotUseKnife() and BotThrowGrenade() return constants
#define RETURN_NOTFIRED		0
#define RETURN_FIRED		1
#define RETURN_RELOADING	2
#define RETURN_NOAMMO		3
#define RETURN_TOOCLOSE		4
#define RETURN_TOOFAR		5
#define RETURN_SECONDARY	6
#define RETURN_TAKING		7
#define RETURN_PRIMING		8

// we have to init them by -10, because of some checks where -1 would cause bug eg. pBot->main_weapon
const int default_ID = -10;
int dod_weapon_amerknife =		default_ID;
int dod_weapon_gerknife =		default_ID;
int dod_weapon_colt =			default_ID;
int dod_weapon_luger =			default_ID;
int dod_weapon_garand =			default_ID;
int dod_weapon_scopedkar =		default_ID;
int dod_weapon_thompson =		default_ID;
int dod_weapon_mp44 =			default_ID;
int dod_weapon_spring =			default_ID;
int dod_weapon_kar =			default_ID;
int dod_weapon_bar =			default_ID;
int dod_weapon_mp40 =			default_ID;
int dod_weapon_handgrenade =	default_ID;
int dod_weapon_stickgrenade =	default_ID;

int dod_weapon_mg42 =			default_ID;
int dod_weapon_30cal =			default_ID;
int dod_weapon_spade =			default_ID;
int dod_weapon_m1carbine =		default_ID;
int dod_weapon_mg34 =			default_ID;
int dod_weapon_greasegun =		default_ID;
int dod_weapon_fg42 =			default_ID;
int dod_weapon_k43 =			default_ID;
int dod_weapon_enfield =		default_ID;
int dod_weapon_sten =			default_ID;
int dod_weapon_bren =			default_ID;
int dod_weapon_webley =			default_ID;
int dod_weapon_bazooka =		default_ID;
int dod_weapon_pschreck =		default_ID;
int dod_weapon_piat =			default_ID;

// list of weapon names, DOD doesn't seem to give us the weapon name within the weapon list message
// ordered by the weapon IDs
char* weapon_name[] = { "n/a", "weapon_amerknife", "weapon_gerknife", "weapon_colt", "weapon_luger", "weapon_garand", "weapon_scopedkar", "weapon_thompson", "weapon_mp44",
		"weapon_spring", "weapon_kar", "weapon_bar", "weapon_mp40", "weapon_handgrenade", "weapon_stickgrenade",
		"n/a", "n/a",	// the ID #15 and #16 are free (unused)
		"weapon_mg42", "weapon_30cal", "weapon_spade", "weapon_m1carbine", "weapon_mg34", "weapon_greasegun", "weapon_fg42", "weapon_k43", "weapon_enfield",
		"weapon_sten", "weapon_bren", "weapon_webley", "weapon_bazooka", "weapon_pschreck", "weapon_piat" };


// bot_combat functions prototypes
bool InitBaseWeapons(void);
void InitWeaponArraySlot(int index);
float GetWeaponPrimaryBaseDelay(int weaponID);
void SetTargetOffsetsDefaults(int index);
inline void SelectMainWeapon(bot_t* pBot);
inline void SelectBackupWeapon(bot_t* pBot);
inline void SelectMeleeWeapon(bot_t* pBot);
inline void SelectGrenade(bot_t* pBot);
void BotReactions(bot_t *pBot);
float BotGetDistanceToEnemy(bot_t* pBot);
void DontSeeEnemyActions(bot_t *pBot);
bool CanAimForHeadshot(int headshot_chance, int aim_skill, bool is_sniper);
void BotFireMountedGun(bot_t* pBot, float enemy_distance);
int BotFireWeapon(bot_t* pBot, float enemy_distance);
int BotUseKnife(bot_t* pBot, float enemy_distance);
int BotThrowGrenade(bot_t* pBot, float enemy_distance);
void PostThrowGrenade(bot_t* pBot, int result, const char* loc);
bool CanUseBackupInsteadofReload(bot_t *pBot, float enemy_distance = 0.0f);
void IsChanceToAdvance(bot_t *pBot);
inline void CheckStance(bot_t *pBot, float enemy_distance);
bool IsEnemyCloseEnough(bot_t *pBot, float enemy_distance);
bool IsOutOfMinimumSafeDistanceForWeapon(bot_t* pBot, int weapon_index, float enemy_distance);
inline void BotDecideFullAutoFire(bot_t* pBot);
inline void BotDecideFullAutoFireOnMachinegun(bot_t* pBot, float enemy_distance);


/*
* inits a few weapons that have same ID in all mod versions
* NOTE: There are all DOD weapons as MB supports only the latest version
*/
bool InitBaseWeapons(void)
{
	dod_weapon_amerknife = WEAPON_AMERKNIFE;
	dod_weapon_gerknife = WEAPON_GERKNIFE;
	dod_weapon_colt = WEAPON_COLT;
	dod_weapon_luger = WEAPON_LUGER;
	dod_weapon_garand = WEAPON_GARAND;
	dod_weapon_scopedkar = WEAPON_SCOPEDKAR;
	dod_weapon_thompson = WEAPON_THOMPSON;
	dod_weapon_mp44 = WEAPON_MP44;
	dod_weapon_spring = WEAPON_SPRING;
	dod_weapon_kar = WEAPON_KAR;
	dod_weapon_bar = WEAPON_BAR;
	dod_weapon_mp40 = WEAPON_MP40;
	dod_weapon_handgrenade = WEAPON_HANDGRENADE;
	dod_weapon_stickgrenade = WEAPON_STICKGRENADE;

	dod_weapon_mg42 = WEAPON_MG42;
	dod_weapon_30cal = WEAPON_30CAL;
	dod_weapon_spade = WEAPON_SPADE;
	dod_weapon_m1carbine = WEAPON_M1CARBINE;
	dod_weapon_mg34 = WEAPON_MG34;
	dod_weapon_greasegun = WEAPON_GREASEGUN;
	dod_weapon_fg42 = WEAPON_FG42;
	dod_weapon_k43 = WEAPON_K43;
	dod_weapon_enfield = WEAPON_ENFIELD;
	dod_weapon_sten = WEAPON_STEN;
	dod_weapon_bren = WEAPON_BREN;
	dod_weapon_webley = WEAPON_WEBLEY;
	dod_weapon_bazooka = WEAPON_BAZOOKA;
	dod_weapon_pschreck = WEAPON_PSCHRECK;
	dod_weapon_piat = WEAPON_PIAT;

	// test if all weapons were successfully initialized
	if ((dod_weapon_amerknife == default_ID) || (dod_weapon_gerknife == default_ID) || (dod_weapon_colt == default_ID) || (dod_weapon_luger == default_ID) ||
		(dod_weapon_garand == default_ID) || (dod_weapon_scopedkar == default_ID) || (dod_weapon_thompson == default_ID) || (dod_weapon_mp44 == default_ID) ||
		(dod_weapon_spring == default_ID) || (dod_weapon_kar == default_ID) || (dod_weapon_bar == default_ID) || (dod_weapon_mp40 == default_ID) ||
		(dod_weapon_handgrenade == default_ID) || (dod_weapon_stickgrenade == default_ID) ||

		(dod_weapon_mg42 == default_ID) || (dod_weapon_30cal == default_ID) || (dod_weapon_spade == default_ID) || (dod_weapon_m1carbine == default_ID) ||
		(dod_weapon_mg34 == default_ID) || (dod_weapon_greasegun == default_ID) || (dod_weapon_fg42 == default_ID) || (dod_weapon_k43 == default_ID) ||
		(dod_weapon_enfield == default_ID) || (dod_weapon_sten == default_ID) || (dod_weapon_bren == default_ID) || (dod_weapon_webley == default_ID) ||
		(dod_weapon_bazooka == default_ID) || (dod_weapon_pschreck == default_ID) || (dod_weapon_piat == default_ID))
	{
		return false;
	}

	return true;
}


/*
* inits the right weapon set based on mod version
*/
bool InitWeaponsForThisMod(void)
{
	bool a_problem = false;

	// these weapon have their IDs same in all version se we can call them here
	if (InitBaseWeapons() == false)
		a_problem = true;

	if (g_mod_version == DOD_13)
	{
		if (a_problem == false)//(InitDod13Weapons())
			ALERT(at_console, "MarineBot DoD 1.3 weapon detection done\n");
		else
		{
			a_problem = true;
			ALERT(at_console, "MarineBot cannot detect DoD 1.3 weapons!\n");
		}
	}

	// there was some problem during initialization so return error
	if (a_problem)
		return false;

	return true;
}


/*
* inits one array slot to default values to revert to when things fail
*/
void InitWeaponArraySlot(int index)
{
	bot_weapon_select[index].iId = index;
	bot_weapon_select[index].min_safe_distance = 0.0f;
	bot_weapon_select[index].max_effective_distance = 9999.0f;

	bot_fire_delay[index].iId = index;
	bot_fire_delay[index].primary_base_delay = 0.5f;

	for (int i = 0; i < BOT_SKILL_LEVELS; ++i)
	{
		bot_fire_delay[index].primary_min_delay[i] = 0.0f;
		bot_fire_delay[index].primary_max_delay[i] = 1.0f;
	}
}


/*
* inits both weapon stucts (select as well as delay)
* weapons are stored based on their ID
*/
bool BotWeaponArraysInit(const char* weapon_definitions_filename)
{
	int index;
	static int entry_counter;
	bool read_minsd, read_maxed, read_modif, read_based, read_mind, read_maxd;
	char weapon_id[5]{};
	char weapon_id_check[5]{};
	char the_entry[32];
	bool w_max_dist_modif;
	
	// default values to use when all safety tools fail
	const float default_min_dist = 0.0f;
	const float default_max_dist = 9999.0f;
	const float default_base_delay = 0.5f;
	const float default_min_delay = 0.0f;
	const float default_max_delay = 1.0f;

	// if there is no file with weapon definitions then init all weapons to defaults which would allow some basic weapon use
	if (configFile.OpenConfigFile(weapon_definitions_filename) == false)
	{
		for (index = 0; index < MAX_WEAPONS; ++index)
		{
			InitWeaponArraySlot(index);
		}

		ALERT(at_console, "MarineBot weapons initialization WASN'T done\n");
		return false;
	}

	// first we should reset the configuration history so that we can start afresh without any leftovers from previous configuration file
	configFile.ResetConfigHistory();

	for (index = 0; index < MAX_WEAPONS; ++index)
	{
		// first init this array slot using the defaults
		InitWeaponArraySlot(index);

		// weapon data we need to read
		sprintf(weapon_id, "ID%d", index);

		// next weapon ID serves as a validity check to prevent reading wrong data
		sprintf(weapon_id_check, "ID%d", index + 1);
		configFile.SetScope(weapon_id_check);

		// did we find a valid data for this array slot?
		if (configFile.FindKeyScope(weapon_id))
		{
			// reset the counter
			entry_counter = 0;
			// and reset also the safety checks
			read_minsd = read_maxed = read_modif = read_based = read_mind = read_maxd = false;

			// keep reading the data till we get out of the scope
			while (configFile.ReadEntryScope(the_entry))
			{
				// see what the data entry is...
				if ((strcmp(the_entry, "min_safe_distance") == 0) && !read_minsd)
				{
					// read its value
					bot_weapon_select[index].min_safe_distance = fabsf(configFile.ReadFloatValue(default_min_dist));

					// and increase the counter of successfully found entries
					entry_counter++;
					// also set the safety check to make sure it's read only once
					// in case someone did mess up the external file and added this variable twice
					read_minsd = true;
				}
				else if ((strcmp(the_entry, "max_effective_distance") == 0) && !read_maxed)
				{
					bot_weapon_select[index].max_effective_distance = fabsf(configFile.ReadFloatValue(default_max_dist));
					entry_counter++;
					read_maxed = true;
				}
				else if ((strcmp(the_entry, "modif") == 0) && !read_modif)
				{
					w_max_dist_modif = configFile.ReadBooleanValue(false);
					// modify the max effective distance only if it is NOT set to max default value (ie. 9999.0)
					// in other words don't modify sniper rifle range even if user set it so
					if (w_max_dist_modif &&
						(bot_weapon_select[index].max_effective_distance != default_max_dist))
					{
						bot_weapon_select[index].max_effective_distance *= rg_modif;
					}

					entry_counter++;
					read_modif = true;
				}
				else if ((strcmp(the_entry, "primary_base_delay") == 0) && !read_based)
				{
					bot_fire_delay[index].primary_base_delay = fabsf(configFile.ReadFloatValue(default_base_delay));
					entry_counter++;
					read_based = true;
				}
				else if ((strcmp(the_entry, "primary_min_delay") == 0) && !read_mind)
				{
					configFile.ReadFloatArray(bot_fire_delay[index].primary_min_delay, BOT_SKILL_LEVELS, default_min_delay);
					entry_counter++;
					read_mind = true;
				}
				else if ((strcmp(the_entry, "primary_max_delay") == 0) && !read_maxd)
				{
					configFile.ReadFloatArray(bot_fire_delay[index].primary_max_delay, BOT_SKILL_LEVELS, default_max_delay);
					entry_counter++;
					read_maxd = true;
				}
			}

			// set read error if we weren't able to read all variables
			if (entry_counter < 6)
			{
				configFile.SetReadError();
				configFile.CreateErrorMessage("missing entry for", configFile.GetEntryName());
			}

			// return to the beginning of the file for the next weapon data seek
			configFile.Rewind();
		}
		else
		{
			// there is no entry for this weapon slot so rewind and try next weapon slot
			configFile.Rewind();
		}


#ifdef DEBUG
		//@@@@@@@@@@@@																								DELETE IT
		/*/
		char msg[512];
		sprintf(msg, "WID=%s | minDist=%.2f | maxDist=%.2f | modif=%d | baseDelay=%.2f | minDelay[%.2f %.2f %.2f %.2f %.2f] | maxDelay[%.2f %.2f %.2f %.2f %.2f]\n",
			weapon_id, bot_weapon_select[index].min_safe_distance, bot_weapon_select[index].max_effective_distance, w_max_dist_modif, bot_fire_delay[index].primary_base_delay,
			bot_fire_delay[index].primary_min_delay[0], bot_fire_delay[index].primary_min_delay[1], bot_fire_delay[index].primary_min_delay[2],
			bot_fire_delay[index].primary_min_delay[3], bot_fire_delay[index].primary_min_delay[4],
			bot_fire_delay[index].primary_max_delay[0], bot_fire_delay[index].primary_max_delay[1], bot_fire_delay[index].primary_max_delay[2],
			bot_fire_delay[index].primary_max_delay[3], bot_fire_delay[index].primary_max_delay[4]);

		conOutput.Print(NULL, msg, MType::msg_null);
		/**/
#endif // DEBUG


	}

	// close the weapon definitions file, because we don't need it anymore
	configFile.CloseConfigFile();

	// was there any missing variable?
	if (configFile.IsReadError())
	{
		// is there a specific error message?
		if (configFile.IsErrorMessage())
		{
			// then let the user know what is missing in the config file
			conOutput.Print(NULL, configFile.GetErrorMessage(), MType::msg_error);
		}

		// we also return false to print the general error message into console or top of the HUD once the client joins
		return false;
	}

	ALERT(at_console, "MarineBot weapons initialization done\n");
	return true;
}


float GetWeaponPrimaryBaseDelay(int weaponID)
{
	if (weaponID != NO_VAL)
	{
		bot_fire_delay_t* pDelay = &bot_fire_delay[0];
		return pDelay[weaponID].primary_base_delay;
	}

	// universal value, ie. one that is used in the array init too
	return 0.5f;
}


/*
* the default values for target offsets array
* used in cases when the external file was missing or had invalid data
*/
void SetTargetOffsetsDefaults(int index)
{
	if (index == 0)
	{
		bot_target_offset[index].x_axis = 0.0f;
		bot_target_offset[index].y_axis = 0.0f;
		bot_target_offset[index].z_axis = 0.0f;
		bot_target_offset[index].x_axis_sniper = 0.0f;
		bot_target_offset[index].y_axis_sniper = 0.0f;
		bot_target_offset[index].z_axis_sniper = 0.0f;
	}
	else if (index == 1)
	{
		bot_target_offset[index].x_axis = 2.5f;
		bot_target_offset[index].y_axis = 2.5f;
		bot_target_offset[index].z_axis = 5.0f;
		bot_target_offset[index].x_axis_sniper = 1.0f;
		bot_target_offset[index].y_axis_sniper = 1.0f;
		bot_target_offset[index].z_axis_sniper = 2.0f;
	}
	else if (index == 2)
	{
		bot_target_offset[index].x_axis = 7.0f;
		bot_target_offset[index].y_axis = 7.0f;
		bot_target_offset[index].z_axis = 13.0f;
		bot_target_offset[index].x_axis_sniper = 5.0f;
		bot_target_offset[index].y_axis_sniper = 5.0f;
		bot_target_offset[index].z_axis_sniper = 9.0f;
	}
	else if (index == 3)
	{
		bot_target_offset[index].x_axis = 15.0f;
		bot_target_offset[index].y_axis = 15.0f;
		bot_target_offset[index].z_axis = 20.0f;
		bot_target_offset[index].x_axis_sniper = 10.0f;
		bot_target_offset[index].y_axis_sniper = 10.0f;
		bot_target_offset[index].z_axis_sniper = 15.0f;
	}
	// i.e. aimskill == 4, using an universal 'else' statement will ensure that the array will be initialized
	// even if someone would have added more bot skills and forgot to update this default function
	else 
	{
		bot_target_offset[index].x_axis = 24.0f;
		bot_target_offset[index].y_axis = 24.0f;
		bot_target_offset[index].z_axis = 30.0f;
		bot_target_offset[index].x_axis_sniper = 17.0f;
		bot_target_offset[index].y_axis_sniper = 17.0f;
		bot_target_offset[index].z_axis_sniper = 23.0f;
	}

#ifdef DEBUG
	//ALERT(at_console, "Set Target Offsets Defaults function called\n");
#endif // DEBUG

}


/*
* intis the array of target offsets for use in BotBodyTarget() function
*/
bool BotTargetOffsetsArrayInit(const char* target_offsets_filename)
{
	int index;
	static int entry_counter;
	bool read_x, read_y, read_z, read_xs, read_ys, read_zs;
	char aimskill_id[10]{};			// allows up to 99 skill levels
	char aimskill_id_check[10]{};
	char the_entry[32];

	// default value to use when all safety tools fail
	const float default_value = 2.0f;

	// if there is no file with target offsets then use the defaults
	// to ensure standard functionality
	if (configFile.OpenConfigFile(target_offsets_filename) == false)
	{
		for (index = 0; index < BOT_SKILL_LEVELS; ++index)
		{
			SetTargetOffsetsDefaults(index);
		}

		ALERT(at_console, "MarineBot TargetBodyOffsets set to hardcoded default values\n");
		return false;
	}

	configFile.ResetConfigHistory();

	for (index = 0; index < BOT_SKILL_LEVELS; ++index)
	{
		// first init this array slot using the defaults
		SetTargetOffsetsDefaults(index);

		sprintf(aimskill_id, "AIMSKILL%d", index + 1);		// deal with the array index vs. real world value

		sprintf(aimskill_id_check, "AIMSKILL%d", index + 2);
		configFile.SetScope(aimskill_id_check);

		// did we find a valid data for this array slot?
		if (configFile.FindKeyScope(aimskill_id))
		{
			// reset the counter
			entry_counter = 0;
			// also reset the safety checks
			read_x = read_y = read_z = read_xs = read_ys = read_zs = false;

			// keep reading the data till we get out of the scope
			while (configFile.ReadEntryScope(the_entry))
			{
				// see what the data entry is...
				if ((strcmp(the_entry, "x_axis") == 0) && !read_x)
				{
					// read its value and make sure it is always positive
					bot_target_offset[index].x_axis = fabsf(configFile.ReadFloatValue(default_value));
					entry_counter++;
					read_x = true;
				}
				else if ((strcmp(the_entry, "y_axis") == 0) && !read_y)
				{
					bot_target_offset[index].y_axis = fabsf(configFile.ReadFloatValue(default_value));
					entry_counter++;
					read_y = true;
				}
				else if ((strcmp(the_entry, "z_axis") == 0) && !read_z)
				{
					bot_target_offset[index].z_axis = fabsf(configFile.ReadFloatValue(default_value));
					entry_counter++;
					read_z = true;
				}
				else if ((strcmp(the_entry, "x_axis_sniper") == 0) && !read_xs)
				{
					bot_target_offset[index].x_axis_sniper = fabsf(configFile.ReadFloatValue(default_value));
					entry_counter++;
					read_xs = true;
				}
				else if ((strcmp(the_entry, "y_axis_sniper") == 0) && !read_ys)
				{
					bot_target_offset[index].y_axis_sniper = fabsf(configFile.ReadFloatValue(default_value));
					entry_counter++;
					read_ys = true;
				}
				else if ((strcmp(the_entry, "z_axis_sniper") == 0) && !read_zs)
				{
					bot_target_offset[index].z_axis_sniper = fabsf(configFile.ReadFloatValue(default_value));
					entry_counter++;
					read_zs = true;
				}
			}

			// set read error if we weren't able to read all variables
			if (entry_counter < 6)
			{
				configFile.SetReadError();
				configFile.CreateErrorMessage("missing entry for", configFile.GetEntryName());
			}

			// return to the beginning of the file for the next aimskill data seek
			configFile.Rewind();
		}
		else
		{
			// there is no entry for this aimskill slot so rewind and try next aimskill slot
			configFile.Rewind();
		}

#ifdef DEBUG
		//@@@@@@@@@@@@																								DELETE IT
		/*/
		char msg[512];
		sprintf(msg, "%s | Xaxis=%.2f | Yaxis=%.2f | Zaxis=%.2f | Xsniper=%.2f | Ysniper=%.2f | Zsniper=%.2f\n",
			aimskill_id, bot_target_offset[index].x_axis, bot_target_offset[index].y_axis, bot_target_offset[index].z_axis,
			bot_target_offset[index].x_axis_sniper, bot_target_offset[index].y_axis_sniper, bot_target_offset[index].z_axis_sniper);

		conOutput.Print(NULL, msg, MType::msg_null);
		/**/
#endif // DEBUG

	}

	configFile.CloseConfigFile();

	// was there any missing variable?
	if (configFile.IsReadError())
	{
		if (configFile.IsErrorMessage())
		{
			conOutput.Print(NULL, configFile.GetErrorMessage(), MType::msg_error);
		}

		// we also return false to print the general error message into console or top of the HUD once the client joins
		return false;
	}

	ALERT(at_console, "MarineBot TargetBodyOffsets set to values read from the external file\n");
	return true;
}


/*
* checks the amount of magazines for main weapon and sets out of ammo if there are no magazines left and the weapon clip is also empty
*/
bool bot_t::CheckMainWeaponOutOfAmmo(const char* loc)
{
	if ((current_weapon.iClip == 0) && (current_weapon.iAmmo1 == 0) && (IsNoAmmoForMainWeapon() == false) && IsWeaponReady() && (current_weapon.iId == main_weapon))
	{
		SetWeaponStatus(WS_NOAMMOFORMAIN);


#ifdef DEBUG
		if ((loc != NULL) && botdebugger.IsDebugWeapons())
		{
			char dbgmsg[128]{};
			sprintf(dbgmsg, "MainWeaponOutOfAmmo() called @ %s)\n", loc);
			conOutput.Notify(dbgmsg, this);
		}
#else
		if (botdebugger.IsDebugWeapons())
			conOutput.Notify("MAIN weapon completely out of ammo (no magazines)\n", this);
#endif // DEBUG

	}

	return IsNoAmmoForMainWeapon();
}


/*
* checks the amount of magazines for backup weapon and sets out of ammo if there are no magazines left and the weapon clip is also empty
*/
bool bot_t::CheckBackupWeaponOutOfAmmo(const char* loc)
{
	if ((current_weapon.iClip == 0) && (current_weapon.iAmmo1 == 0) && (IsNoAmmoForBackupWeapon() == false) && IsWeaponReady() && (current_weapon.iId == backup_weapon))
	{
		SetWeaponStatus(WS_NOAMMOFORBACKUP);


#ifdef DEBUG
		if ((loc != NULL) && botdebugger.IsDebugWeapons())
		{
			char dbgmsg[128]{};
			sprintf(dbgmsg, "BackupWeaponOutOfAmmo() called @ %s)\n", loc);
			conOutput.Notify(dbgmsg, this);
		}
#else
		if (botdebugger.IsDebugWeapons())
			conOutput.Notify("BACKUP weapon completely out of ammo (no magazines)\n", this);
#endif // DEBUG

	}

	return IsNoAmmoForBackupWeapon();
}


/*
* sets the no ammo bit based on used weapon (ie for main weapon or for backup weapon) without checking (ie we already checked that prior calling this function)
*/
void bot_t::SetWeaponIsOutOfAmmo(const char* loc)
{

#ifdef DEBUG
	if ((loc != NULL) && botdebugger.IsDebugWeapons())
	{
		char dbgmsg[128]{};
		sprintf(dbgmsg, "WeaponIsOutOfAmmo() called @ %s)\n", loc);
		conOutput.Notify(dbgmsg, this);
	}
#endif // DEBUG


	// is the used weapon a main weapon?
	if (IsUsedWeaponMain())
	{
		// then set no ammo bit for the main weapon
		SetWeaponStatus(WS_NOAMMOFORMAIN);

#ifndef DEBUG
		if (botdebugger.IsDebugWeapons())
			conOutput.Notify("MAIN weapon completely out of ammo (no magazines)\n", this);
#endif // !DEBUG

	}
	// otherwise set it for backup weapon
	else
	{
		SetWeaponStatus(WS_NOAMMOFORBACKUP);

#ifndef DEBUG
		if (botdebugger.IsDebugWeapons())
			conOutput.Notify("BACKUP weapon completely out of ammo (no magazines)\n", this);
#endif // !DEBUG

	}
}


/*
* checks whether magazine or grenade chamber in case of weapon with attached grenade launcher is empty
*/
bool bot_t::IsCurrentWeaponEmpty(void)
{
	// DoD sets zero iClip even for melee weapons so checking for empty weapon needs specific exception else we would get invalid results elsewhere, eg. a try to reload melee weapon
	if ((current_weapon.iId != dod_weapon_amerknife) && (current_weapon.iId != dod_weapon_gerknife) && (current_weapon.iId != dod_weapon_spade))
	{
		// is the clip empty?
		if (current_weapon.iClip == 0)
			return true;
	}

	return false;
}


/*
* checks whether the bot should reload his current weapon (based on iClip value and the type of the weapon)
* knife as well as claymore mine won't pass current set of if statements (tested in FA 3.0) so this is safe
* !!! grenades are untested !!! - those are used only in combat now so this seems to be okay as well
*/
bool bot_t::ShouldReload(const char* loc)
{
	bot_current_weapon_t w = current_weapon;

	// don't check things if the weapon isn't ready
	if (weapon_action != W_READY)
		return false;



#ifdef DEBUG

	//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@	 													// NEW CODE 094 (remove it)
	if ((loc != NULL) && (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons()))
	{
		char dm[128]{};
		sprintf(dm, "ShouldReload() called @ (%s) --- clip (=%d)\n", loc, w.iClip);
		//conOutput.Notify(dm, this);
	}
	//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

#endif // DEBUG


	// always reload if the weapon is empty and have ammo for it
	if ((w.iClip == 0) && (w.iAmmo1 > 0))
	{
		// DoD doesn't allow reloading machine guns (except for bren) that are not deployed on bipod
		if (IsLimitedReloadMachinegun(w.iId) && (IsTask(TASK_BIPOD) == false))
			return false;

		return true;
	}

	// if NOT tasked to check weapon clip for remaining ammo then there is no need to reload weapon either (i.e. we don't want to check ammo every frame)
	if (IsSubTask(ST_W_CLIP) == false)
		return false;



#ifdef DEBUG
	if ((loc != NULL) && (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons()))
	{
		char dm[128]{};
		sprintf(dm, "ShouldReload() called @ (%s) -> going to really check clip (=%d)\n", loc, w.iClip);
		conOutput.Notify(dm);
	}
#endif // DEBUG




	// has bot more than 5 rounds in reserves? then see if there's a reason to reload the gun
	if (w.iAmmo1 > 5)
	{
		// is there only 1 or 2 rounds left in the gun?
		if (((w.iId == dod_weapon_kar) || (w.iId == dod_weapon_scopedkar) || (w.iId == dod_weapon_spring) || IsHandgun(w.iId)) && (w.iClip < 3))
			return true; // then reload
		
		// less than half a clip?
		else if (((w.iId == dod_weapon_enfield) || (w.iId == dod_weapon_k43)) && (w.iClip < 5))
			return true;
		
		// now we're checking the reserves once more, because all the other weapons can load a lot of ammo so it'd be pointless to reload them if there wasn't enough remaining ammo
		else if ((w.iId == dod_weapon_m1carbine) && (w.iClip < 6) && (w.iAmmo1 > 10))
			return true;
		else if ((IsSMG(w.iId) || (w.iId == dod_weapon_bren)) && (w.iClip < 10) && (w.iAmmo1 > 19))
			return true;
		else if (IsLMG(w.iId, pEdict->v.playerclass) && (w.iClip < 8) && (w.iAmmo1 > 14))
			return true;
		else if (IsLimitedReloadMachinegun(w.iId) && IsTask(TASK_BIPOD) && (w.iClip < 25) && (w.iAmmo1 > 50))
			return true;
	}

	// ammo left in current weapon isn't at critical level so we don't have to check it again
	RemoveSubTask(ST_W_CLIP);

	// and we also won't reload
	return false;
}


/*
* checks whether bot can start reloading his current weapon and sets the weapon action if so
*/
void bot_t::ReloadWeapon(const char* loc)
{
	// weapon is ready and not pressed the reload button yet
	if (IsWeaponReady() && (IsWeaponStatus(WS_PRESSRELOAD) == false) && IsNotReloadingWeapon())
	{
		
#ifdef DEBUG

		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@														// NEW CODE 094 (remove it)
		if ((loc != NULL) && (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons()))
		{
			char dm[128]{};
			sprintf(dm, "Reload Weapon() called @ %s\n", loc);
			conOutput.Notify(dm, this);
		}
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

#endif // DEBUG



		// is this a machine gun that cannot be reloaded normally AND bipod isn't used?
		if (IsLimitedReloadMachinegun(current_weapon.iId) && (IsTask(TASK_BIPOD) == false))
		{
			// did bot use fully automatic fire in out of combat mode AND is too soon after shooting?
			if (IsWeaponStatus(WS_RELOADSECONDARY) && ((f_shoot_time + 0.1f) > gpGlobals->time))
			{

#ifdef DEBUG
				if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
				{
					char dp[128]{};
					sprintf(dp, "Reload Weapon() -> breaking it due to being too soon after shooting the MG to deploy BIPOD!!!\n");
					conOutput.Notify(dp, this);
				}
#endif // DEBUG

				// then wait and give engine time to finish the action of shooting the weapon (some animations or whatever)
				// either way around if we didn't wait awhile then deploying the bipod would fail
				return;
			}

			// let's see what possibilities the bot has
			if (CanDeployBipod(pEdict))
				BotUseBipod(this, true, "Reload Weapon() -> deploy it to reload the machine gun");
			else if ((IsNoAmmoForBackupWeapon() == false))
				UseWeapon(uWeapon::backup);
			else
				UseWeapon(uWeapon::knife);

			// and break reloading right now
			return;
		}

		// change weapon action to actually start reloading (i.e. weapon is NOT ready to fire since now)
		weapon_action = W_INRELOAD;

		// trying to go/resume prone will invalidate reloading in FA so prevent the bot to do so
		SetBehaviour(BOT_DONTGOPRONE);



		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@			NEW CODE 094 (remove it)
#ifdef DEBUG
		if (botdebugger.IsDebugStance())
		{
			char dp[128]{};
			sprintf(dp, "SET DONTGOPRONE behaviour @ Reload Weapon()\n");
			conOutput.Notify(dp, this);
		}
#endif // DEBUG





		// weapon is going to be reloaded so there's no need to check how many rounds are left in it (it'll be full)
		RemoveSubTask(ST_W_CLIP);
	}
}


/*
* checks amount of magazines the bot should take from ammobox
* also deals with grenades (makes the other type available on multi grenade bot classes and fixes cases when any grenades are picked up from ground as free weapons on map)
*/
void bot_t::CheckAmmoReserves(const char* loc)
{
	int main_weap_secondary_ammo = 0;

	// don't check ammo when the weapon is NOT ready OR the bot fired the weapon recently (or changed weapons where it's set as well) OR is paused right now where the bot could be merging magazines
	if ((weapon_action != W_READY) || (f_shoot_time + 1.0f > gpGlobals->time) || (IsNotPaused() == false))
		return;


#ifdef DEBUG
	//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	if ((loc != NULL) && (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons()))
	{
		//char dbgmsg[128]{};
		//sprintf(dbgmsg, "Check AmmoReserves() called @ %s\n", loc);
		//conOutput.Notify(dbgmsg, this);
	}
#endif // DEBUG



	// update the time ammo reserves been checked
	SetCheckAmmoReservesTime();

	// and reset the task, because bot just checked ammunition
	RemoveTask(TASK_CHECKAMMO);

	// bot should also check current weapon magazine for remaining ammo
	SetSubTask(ST_W_CLIP);

	//if (botdebugger.IsDebugActions())
		//conOutput.Notify("Check AmmoReserves() -> ammunition was checked ...\n", this);

	// let the bot decide whether he will actively look for explosives charge when he has none or not (bots going for goals will always seek explosives,
	// the other bots will randomly decide with tiny chance on to do so - this gets called repeatedly so it has to be such tiny chance otherwise every bot would seek explosives)
	if ((IsEquippedWithExplosiveCharge() == false) && (IsNeed(NEED_GOAL) || ((IsNeed(NEED_GOAL) == false) && (RANDOM_LONG(1, 100) < 2))))
		SetNeed(NEED_EXLOSIVESCHARGE);


#ifdef DEBUG
	// just for debugging so that bot doesn't reset manually set flags like noammo etc.
	if (IsWeaponStatus(WS_TEST_DONTCHECKAMMO))
		return;
#endif // DEBUG

	// is melee only game mode enabled? then quit right here so that the bot cannot reset the variables forcing him to use only melee weapons
	if (internals.IsMeleeOnlyMode())
		return;

	// is there any reserve ammo for main weapon AND is main weapon no ammo flag set? (ie we think that main weapon is completely empty, but we still have some ammo for it)
	if ((main_weapon != NO_VAL) && (curr_rgAmmo[weapon_defs[main_weapon].iAmmo1] > 0) && IsNoAmmoForMainWeapon())
	{
		RemoveWeaponStatus(WS_NOAMMOFORMAIN);		// main weapon isn't completely empty since now

		if (botdebugger.IsDebugActions())
			conOutput.Notify("Checking ammo -> main weapon is usable again\n", this);
	}

	// is there any reserve ammo for backup weapon AND is backup weapon no ammo flag set? (ie we think that backup weapon is completely empty, but we have some ammo for it)
	if ((backup_weapon != NO_VAL) && (curr_rgAmmo[weapon_defs[backup_weapon].iAmmo1] > 0) && IsNoAmmoForBackupWeapon())
	{
		RemoveWeaponStatus(WS_NOAMMOFORBACKUP);		// backup weapon isn't completely empty since now

		if (botdebugger.IsDebugActions())
			conOutput.Notify("Checking ammo -> backup weapon is usable again\n", this);
	}

	// has bot some unused grenades yet AND the grenade flag is already set to used? (ie we thing that we don't have any grenade, but there is still at least one available)
	if (IsGrenadesDepleted() && (grenade_slot != NO_VAL) && (bot_weapons & (1 << grenade_slot)))
	{
		SetGrenadesAvailable();	// grenades aren't completely used yet

		if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons(1))
			conOutput.Notify("Checking ammo -> grenades are usable again\n", this);
	}

	// fix the case when bot picked up grenades (as an item on a map) and the grenades were not picked up again after respawn now this bot still has a grenade slot marked as USED (ie. bot thinks he has a grenade)
	if (IsGrenadesAvailable() && (grenade_slot != NO_VAL) && ((bot_weapons & (1 << grenade_slot)) == false))
	{
		SetGrenadesDepleted(); // so simply tell him he already used them all

#ifdef _DEBUG
		if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons(1))
			conOutput.Notify("Check AmmoReserves() -> ONGROUND ITEM explosives -> don't have them anymore -> setting the grenades DEPLETED!\n", this);
#endif
	}

	// is bot low on ammo? then set the NEED AMMO need
	if ((GetTakeAmmoForMainWeapon() > 1) || (GetTakeAmmoForBackupWeapon() > 1))
	{
		SetNeed(NEED_AMMO);

		if (botdebugger.IsDebugActions())
		{
			//char msg[128]{};
			//sprintf(msg, "Check AmmoReserves() -> need ammo flag has been set (missing mags for main+backup=%d)\n", GetTakeAmmoForMainWeapon() + GetTakeAmmoForBackupWeapon());
			//conOutput.Notify(msg, this);
		}
	}
	// otherwise bot must have enough ammo so clear it
	else
	{
		RemoveNeed(NEED_AMMO);

		if (botdebugger.IsDebugActions())
		{
			//char msg[128]{};
			//sprintf(msg, "Check AmmoReserves() -> need ammo flag has been removed (missing mags for main+backup=%d)\n", GetTakeAmmoForMainWeapon() + GetTakeAmmoForBackupWeapon());
			//conOutput.Notify(msg, this);
		}
	}
}


/*
* sets the next best weapon to use based on carried weapons and ammo reserves and enemy distance where it is needed
*/
bool bot_t::DecideNextWeapon(const char* loc, foeDist need_dist_check, float foe_distance)
{
	uWeapon currUsedWeapon;

	// backup current used weapon setting
	currUsedWeapon = used_weapon;

	// clears any leftover bits from grenade use
	ResetGrenadeActions();

	// we must first update main weapon and backup weapon empty status
	CheckMainWeaponOutOfAmmo("Decide NextWeapon()");
	CheckBackupWeaponOutOfAmmo("Decide NextWeapon()");

	// does the bot need to know how far away is the enemy scum to do his decision?
	if (need_dist_check == foeDist::checkit)
	{
		// currently using main weapon, but obviously it's not a possible option right now so try to switch to backup weapon if there's ammo for it AND it is safe to use
		if (IsUsedWeaponMain() && (IsNoAmmoForBackupWeapon() == false) && IsOutOfMinimumSafeDistanceForWeapon(this, backup_weapon, foe_distance))
		{
			UseWeapon(uWeapon::backup);
		}
		// currently using backup weapon and still has enough ammo, but...
		else if (IsUsedWeaponBackup() && (IsNoAmmoForBackupWeapon() == false))
		{
			// see if bot can try to switch to main weapon if there's ammo for it AND it is safe to use
			if ((IsNoAmmoForMainWeapon() == false) && IsOutOfMinimumSafeDistanceForWeapon(this, main_weapon, foe_distance))
				UseWeapon(uWeapon::main);
			// otherwise see if it's safe to keep using backup weapon
			else if (IsOutOfMinimumSafeDistanceForWeapon(this, backup_weapon, foe_distance))
				UseWeapon(uWeapon::backup);
			// both failed? then switch to knife
			else
				UseWeapon(uWeapon::knife);
		}
		else
			UseWeapon(uWeapon::knife);
	}
	// otherwise make a simple decision based on availability
	else
	{
		// try to use main weapon if the bot has any and has enough ammo for it
		if ((IsNoAmmoForMainWeapon() == false) && (main_weapon != NO_VAL))
			UseWeapon(uWeapon::main);

		// otherwise try to use backup weapon if exists and has enough ammo
		else if ((IsNoAmmoForBackupWeapon() == false) && (backup_weapon != NO_VAL))
			UseWeapon(uWeapon::backup);

		// if all failed then there's only knife left for the use
		else
			UseWeapon(uWeapon::knife);
	}

	if (botdebugger.IsDebugWeapons())
	{
		char dwm[128]{};
		if (IsUsedWeaponMain() || IsUsedWeaponBackup())
		{
#ifdef DEBUG
			sprintf(dwm, "(@%s) decided that <%s> weapon will be used now\n", loc, util.ConvertUsedWeaponToString(this));
#else
			sprintf(dwm, "Decided that <%s> weapon will be used now\n", util.ConvertUsedWeaponToString(this));
#endif // DEBUG
		}
		else
		{
#ifdef DEBUG
			sprintf(dwm, "(@%s) decided that <%s> will be used now\n", loc, util.ConvertUsedWeaponToString(this));
#else
			sprintf(dwm, "Decided that <%s> will be used now\n", util.ConvertUsedWeaponToString(this));
#endif // DEBUG
		}

		conOutput.Notify(dwm, this);
	}

	// return TRUE if bot decided to use different weapon
	if (used_weapon != currUsedWeapon)
	{
		// makes bot start the switch weapons action
		weapon_action = W_TAKEOTHER;

		// we don't know whether the other weapon supports fully automatic fire mode, bot will decide it later on when he finished switching to that weapon and got back to combat actions
		if (IsTimeToFullAutoFire())
			SetFullAutoFireTime(0.0f);

		// same as above, bot will decide about this later on, now it must be cleared
		if (IsNotSnipeTime() == false)
			SetSnipeTime(0.0f);

		return true;
	}

	return false;
}


/*
* handles all necessary steps to change between any two weapons
*/
void bot_t::ChangeWeapon(void)
{
	// has the weapon change been invalidated?
	if (IsWeaponStatus(WS_INVALID))
	{
		// then select knife, because everyone has it by default...
		UseWeapon(uWeapon::knife);

		// clear the "resetting" bit...
		RemoveWeaponStatus(WS_INVALID);

		// and set weapon action back to ready to start anew in next frame
		weapon_action = W_READY;

		if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
			conOutput.Notify("ChangeWeapon() -> weapon change has been invalidated -> resetting the action!\n", this);

		return;
	}

	// do we need to change weapon? Then wait until going to prone or resume from prone is finished
	if ((weapon_action == W_TAKEOTHER) && IsNotGoingProne())
	{
		// select the correct weapon
		if (IsUsedWeaponMain() && (main_weapon != NO_VAL))
		{
			SelectMainWeapon(this);

			// make the bot check his new weapon
			SetWeaponStatus(WS_CHECKWEAPON);

			// also make him check the option to mount a silencer on it
			RemoveWeaponStatus(WS_SILENCERCHECKED);
		}
		else if (IsUsedWeaponBackup() && (backup_weapon != NO_VAL))
		{
			SelectBackupWeapon(this);

			SetWeaponStatus(WS_CHECKWEAPON);
			RemoveWeaponStatus(WS_SILENCERCHECKED);
		}
		else if (IsUsedWeaponKnife())
		{
			SelectMeleeWeapon(this);
		}
		else if (IsUsedWeaponGrenade() && (grenade_slot != NO_VAL))
		{
			// are all grenades used already?
			if (IsGrenadesDepleted())
			{
				SetWeaponStatus(WS_INVALID);

				return;
			}

			SelectGrenade(this);
		}

		// set appropriate weapon action flag
		weapon_action = W_INCHANGE;

		// reset leftover actions from previous grenade use
		ResetGrenadeActions();

		return;
	}

	// are we still changing the weapon
	if (weapon_action == W_INCHANGE)
	{
		// stop using bipod
		if (IsTask(TASK_BIPOD))
		{
			// it didn't react on the first weapon change, because with bipod down the weapon cannot be changed so return back to initial flag
			weapon_action = W_READY;
			// also we must "reset" the shoot time otherwise the check inside the bipod handling function fails and bot will stay stuck in an infinite loop
			f_shoot_time = gpGlobals->time - 0.2f;

			BotUseBipod(this, true, "ChangeWeapon()");

			if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
				conOutput.Notify("ChangeWeapon() -> removing BIPOD that is preventing a weapon change\n", this);

			return;
		}
		else if (IsRPG(current_weapon.iId) && IsWeaponSecondaryModeActive())
		{
			weapon_action = W_READY;

			BotSwitchGrenadeLauncherFireMode(this, "ChangeWeapon()");

			if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
				conOutput.Notify("ChangeWeapon() -> taking launcher off shoulder which is preventing a weapon change\n", this);

			return;
		}

		// wait for the engine to finish the animations
		if (f_shoot_time < gpGlobals->time)
		{
			// has the bot changed to desired weapon yet
			if (IsUsedWeaponMain() && (current_weapon.iId == main_weapon))
			{
				// set the right weapon flag
				weapon_action = W_INHANDS;

				// update shoot time to prevent the bot to fire from the weapon
				f_shoot_time = gpGlobals->time;

				// bot should also check if this weapons isn't almost empty after the switch
				SetSubTask(ST_W_CLIP);

				if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
					conOutput.Notify("ChangeWeapon() -> changed to MAIN weapon\n", this);

				return;
			}

			else if (IsUsedWeaponBackup() && (current_weapon.iId == backup_weapon))
			{
				weapon_action = W_INHANDS;
				f_shoot_time = gpGlobals->time;
				SetSubTask(ST_W_CLIP);

				if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
					conOutput.Notify("ChangeWeapon() -> changed to BACKUP weapon\n", this);

				return;
			}

			else if (IsUsedWeaponGrenade() && (current_weapon.iId == grenade_slot))
			{
				weapon_action = W_INHANDS;
				f_shoot_time = gpGlobals->time;

				if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
					conOutput.Notify("ChangeWeapon() -> changed to GRENADE\n", this);

				return;
			}

			else if (IsUsedWeaponClaymoreMine() && (current_weapon.iId == claymore_slot))
			{
				weapon_action = W_INHANDS;
				f_shoot_time = gpGlobals->time + 0.5f;

				if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
					conOutput.Notify("ChangeWeapon() -> changed to CLAYMORE MINE\n", this);

				return;
			}

			else if (IsUsedWeaponKnife() && (current_weapon.iId == melee_weapon))
			{
				weapon_action = W_INHANDS;
				f_shoot_time = gpGlobals->time;

				if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
					conOutput.Notify("ChangeWeapon() -> changed to MELEE\n", this);

				return;
			}
			else
				// keep increasing the shoot time to prevent the bot in trying to use the weapon
				f_shoot_time = gpGlobals->time + 0.1f;
		}

#ifdef DEBUG
		if (botdebugger.IsDebugActions())
		{
			ALERT(at_console, "Trying to switch weapons (currT is %.2f)\n", gpGlobals->time);
		}
#endif // DEBUG

	}

	// weapon change is finished and we can set the weapon action to ready again
	if (weapon_action == W_INHANDS)
	{
		if (f_shoot_time + 0.2f < gpGlobals->time)
		{
			weapon_action = W_READY;

			if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
				conOutput.Notify("ChangeWeapon() -> WEAPON is READY now!\n", this);
		}
	}
}


/*
* selects main weapon in the weapon array and calls client command to switch to this weapon
*/
inline void SelectMainWeapon(bot_t* pBot)
{
	char this_one[64]{};
	float switch_time = 1.7f;	// default value that works with most weapons in FA 3.0

	strcpy(this_one, "n/a");

	// we need to access weapon data in order to get weapon name for the client command
	strcpy(this_one, weapon_name[pBot->main_weapon]);

	if (strcmp(this_one, "n/a") == 0)
	{
		pBot->UseWeapon(uWeapon::knife);
		switch_time = 1.0f;

#ifdef _DEBUG
		util.DebugDev("SelectMainWeapon()|no weapon found -- switching to melee\n", -100, -100);
#endif
	}

	// calling the weapon name will select it
	FakeClientCommand(pBot->pEdict, this_one, NULL, NULL);

	// we must update shoot time to allow the engine to process weapon selection and prevent the bot in trying to use the weapon, because it isn't ready yet
	pBot->f_shoot_time = gpGlobals->time + switch_time;

	if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
	{
		char swmsg[128]{};
		sprintf(swmsg, "SelectMainWeapon() - Changing to weapon <%s> (no weapon action till %.2f)\n", util.StripWeaponName(this_one), pBot->f_shoot_time);
		conOutput.Notify(swmsg, pBot);
	}
}


/*
* selects backup weapon in the weapon array and calls client command to switch to this weapon
*/
inline void SelectBackupWeapon(bot_t* pBot)
{
	char this_one[64]{};
	float switch_time = 1.7f;

	strcpy(this_one, "n/a");

	strcpy(this_one, weapon_name[pBot->backup_weapon]);

	// handguns have a little faster selection
	if (IsHandgun(pBot->backup_weapon))
		switch_time = 1.4f;

	if (strcmp(this_one, "n/a") == 0)
	{
		pBot->UseWeapon(uWeapon::knife);
		switch_time = 1.0f;

#ifdef _DEBUG
		util.DebugDev("SelectBackupWeapon()|no weapon found -- switching to melee\n", -100, -100);
#endif
	}

	FakeClientCommand(pBot->pEdict, this_one, NULL, NULL);
	pBot->f_shoot_time = gpGlobals->time + switch_time;

	if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
	{
		char swmsg[128]{};
		sprintf(swmsg, "SelectBackupWeapon() - Changing to weapon <%s> (no weapon action till %.2f)\n", util.StripWeaponName(this_one), pBot->f_shoot_time);
		conOutput.Notify(swmsg, pBot);
	}
}


/*
* selects melee weapon in the weapon array and calls client command to switch to this weapon
*/
inline void SelectMeleeWeapon(bot_t* pBot)
{
	char this_one[64]{};
	float switch_time = 0.2f;

	strcpy(this_one, "n/a");

	strcpy(this_one, weapon_name[pBot->melee_weapon]);

	if (strcmp(this_one, "n/a") == 0)
	{
#ifdef _DEBUG
		util.DebugDev("SelectMelee()|no melee found!!!\n", -100, -100);
#endif
	}

	FakeClientCommand(pBot->pEdict, this_one, NULL, NULL);
	pBot->f_shoot_time = gpGlobals->time + switch_time;

	if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
	{
		char swmsg[128]{};
		sprintf(swmsg, "SelectMeleeWeapon() - Changing to weapon <%s> (no weapon action till %.2f)\n", util.StripWeaponName(this_one), pBot->f_shoot_time);
		conOutput.Notify(swmsg, pBot);
	}
}


/*
* selects grenade and sets some time to switch to it
*/
inline void SelectGrenade(bot_t* pBot)
{
	char this_one[64]{};
	float switch_time = 1.2f;

	strcpy(this_one, "n/a");

	if (pBot->grenade_slot != NO_VAL)
	{
		strcpy(this_one, weapon_name[pBot->grenade_slot]);

		// use the value from the external file so that users can tweak it in case the grenade doesn't work well
		switch_time = GetWeaponPrimaryBaseDelay(pBot->grenade_slot);
	}

	if (strcmp(this_one, "n/a") == 0)
	{
		pBot->UseWeapon(uWeapon::knife);

#ifdef _DEBUG
		util.DebugDev("<<BUG>>SelectGrenade()|no grenade found -- switched to melee\n", -100, -100);
#endif
	}

	FakeClientCommand(pBot->pEdict, this_one, NULL, NULL);
	pBot->f_shoot_time = gpGlobals->time + switch_time;
	// also set the grenade use time in order to prevent trying to switch to different weapon
	pBot->SetGrenadeUseTime(switch_time + 0.5f);

	if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
	{
		char swmsg[128]{};
		sprintf(swmsg, "SelectGrenade() - Changing to weapon <%s> (no weapon action till %.2f)\n", util.StripWeaponName(this_one), pBot->f_shoot_time);
		conOutput.Notify(swmsg, pBot);
	}
}


/*
* checks if main weapon is available and the bot can change weapons right now (ie bot isn't placing claymore mine or isn't already in process of weapon change)
* if all is fine then the bot sets weapon action to start the weapon change
*/
void bot_t::UseMainWeapon(const char* loc)
{
	if ((IsNoAmmoForMainWeapon() == false) && (current_weapon.iId != main_weapon) && IsAllowedToHandleWeapon() && (IsPlantingClaymoreMine() == false) && (HasPlantedClaymoreMine() == false))
	{


#ifdef DEBUG
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@														// NEW CODE 094 (remove it)
		if ((loc != NULL) && (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons()))
		{
			char dm[256]{};
			sprintf(dm, "Use MainWeapon() called @ %s\n", loc);
			conOutput.Notify(dm, true, this);
		}
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

#endif // DEBUG



		UseWeapon(uWeapon::main);

		// set this flag to know that we need to change weapon
		weapon_action = W_TAKEOTHER;

		if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
			conOutput.Notify("is going to switch back to MAIN weapon\n", this);
	}
}


/*
* checks if backup weapon is available and the bot can change weapons right now
*/
void bot_t::UseBackupWeapon(const char* loc)
{
	if ((IsNoAmmoForBackupWeapon() == false) && ((IsNoAmmoForMainWeapon() && (IsUsedWeaponBackup() == false)) || (IsUsedWeaponBackup() && (current_weapon.iId != backup_weapon))) &&
		IsAllowedToHandleWeapon() && (IsPlantingClaymoreMine() == false) && (HasPlantedClaymoreMine() == false))
	{


#ifdef DEBUG
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
		if ((loc != NULL) && (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons()))
		{
			char dm[256]{};
			sprintf(dm, "Use BackupWeapon() called @ %s\n", loc);
			conOutput.Notify(dm, this);
		}
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

#endif // DEBUG



		UseWeapon(uWeapon::backup);
		weapon_action = W_TAKEOTHER;

		if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
			conOutput.Notify("is going to switch to BACKUP weapon\n", this);
	}
}


/*
* checks whether bot can change to knife right now
*/
void bot_t::UseKnife(const char* loc)
{
	if (((IsNoAmmoForMainWeapon() && IsNoAmmoForBackupWeapon() && (IsUsedWeaponKnife() == false)) || (IsUsedWeaponKnife() && (current_weapon.iId != melee_weapon))) &&
		IsAllowedToHandleWeapon() && (IsPlantingClaymoreMine() == false) && (HasPlantedClaymoreMine() == false))
	{

#ifdef DEBUG
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
		if ((loc != NULL) && (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons()))
		{
			char dm[256]{};
			sprintf(dm, "Use Knife() called @ %s\n", loc);
			conOutput.Notify(dm, this);
		}
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

#endif // DEBUG


		UseWeapon(uWeapon::knife);
		weapon_action = W_TAKEOTHER;

		if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
			conOutput.Notify("is going to switch to the KNIFE\n", this);
	}
}


/*
* generates a chance to use the grenade and checks whether the chance meets limits set by the percentage to use a grenade that is defined externally in configuration file
* 
* TODO: There should be added a boolean switch that will allow checking whether the bot has grenades available etc so that this function can be used even out of combat,
* because currenty this function must be called only after the Can Use Grenade function
* 
*/
bool bot_t::UseGrenade(const char* loc)
{
	// first generate the usage chance
	int chance_to_use_it = RANDOM_LONG(1, 100);

	// is main weapon out of ammo? then make more than double the chance to throw a grenade in case of its default value of 25 (ie. 25 + 35 = 65% chance to use it)
	if ((IsNoAmmoForMainWeapon()) && (externals.GetGrenadeUsePercentage() != -1) && (chance_to_use_it <= externals.GetGrenadeUsePercentage() + 35))
	{
		UseWeapon(uWeapon::grenade);
		weapon_action = W_TAKEOTHER;

		// also set some grenade use time to prevent canceling it by calling the decide next weapon function
		SetGrenadeUseTime(3.0f);

		if (botdebugger.IsDebugWeapons())
		{
#ifdef DEBUG
			char dm[128]{};
			sprintf(dm, "Use Grenade() called @ %s -> is going to switch to a GRENADE (main weapon is out of ammo)\n", loc);
			conOutput.Notify(dm, this);
#else
			conOutput.Notify("is going to switch to a GRENADE (main weapon is out of ammo)\n", this);
#endif // DEBUG
		}

		return true;
	}
	// if there's still enough ammo for weapons then use the grenades in the exact percent of time that the user defined in the configuration file
	else if (chance_to_use_it <= externals.GetGrenadeUsePercentage())
	{
		UseWeapon(uWeapon::grenade);
		weapon_action = W_TAKEOTHER;
		SetGrenadeUseTime(3.0f);

		if (botdebugger.IsDebugWeapons())
		{
#ifdef DEBUG
			char dm[128]{};
			sprintf(dm, "Use Grenade() called @ %s -> is going to switch to a GRENADE\n", loc);
			conOutput.Notify(dm, this);
#else
			conOutput.Notify("is going to switch to a GRENADE\n", this);
#endif // DEBUG
		}

		return true;
	}

	// update grenade time to know that we have tried to use a grenade
	SetGrenadeUseTime(0.0f);


#ifdef DEBUG
	// testing - is it time for grenades
	if (botdebugger.IsDebugWeapons() || in_bot_dev_level1)
	{
		char dm[128]{};
		sprintf(dm, "Use Grenade() called @ %s -> useWeapon remained UNCHANGED -> Try to use it next time\n", loc);
		conOutput.Notify(dm, this);
	}
#endif // DEBUG


	return false;
}


/*
* returns the max effective range of given weapon
*/
float bot_t::GetWeaponEffectiveRange(int weapon_index)
{
	if (weapon_index == NO_VAL)
	{
		// always try main weapon first
		// the bot may have backup weapon in hands at the moment, but can still switch back to main (unless it's empty)
		if (IsNoAmmoForMainWeapon() == false)
			weapon_index = main_weapon;
		else
			// if the bot has no main weapon or is out of ammo for it then take the current one
			weapon_index = current_weapon.iId;
	}

	// if there's no weapon at all return zero ... just for sure
	if (weapon_index == NO_VAL)
		return 0.0f;

	float range;

	range = bot_weapon_select[weapon_index].max_effective_distance;

	// see if we do limit the max distance the bot can see (ie. view distance)
	// if so and the weapon effective range is bigger then the limit we will use the view distance limit instead
	if (internals.IsEnemyDistanceLimit() && (range > internals.GetEnemyDistanceLimit()))
		range = internals.GetEnemyDistanceLimit();

	return range;
}


/*
* returns the min safe range of given weapon
*/
float bot_t::GetWeaponSafeRange(int weapon_index)
{
	if (weapon_index == NO_VAL)
	{
		if (IsNoAmmoForMainWeapon() == false)
			weapon_index = main_weapon;
		else
			weapon_index = current_weapon.iId;
	}

	// if there's no weapon at all return something "safe" that would invalidate the checks later on ... just for sure
	if (weapon_index == NO_VAL)
		return 9999.0f;

	float range;

	range = bot_weapon_select[weapon_index].min_safe_distance;

	// there is very slim chance that min safe distance would exceed user defined enemy distance limit, but we'll check it anyway
	if (internals.IsEnemyDistanceLimit() && (range > internals.GetEnemyDistanceLimit()))
		range = internals.GetEnemyDistanceLimit();

	return range;
}


/*
* returns TRUE if the bot meets all requirements to throw a grenade
*/
bool bot_t::CanUseGrenade(float enemy_distance)
{
	// do we have a grenade AND NOT used them all yet AND NOT doing any weapon action AND NOT have bipod deployed AND NOT under water AND
	// is some time after weapon reload AND is some time since been paused (eg. merged clips) AND
	// is it time to use a grenade (based on bot skill where best bots will try to use it every 5 or so seconds and worst bots every 25 or so seconds) AND 
	// the enemy is in safe distance for grenade use
	if ((grenade_slot != NO_VAL) && IsGrenadesAvailable() && IsWeaponReady() && (IsTask(TASK_BIPOD) == false) && (pEdict->v.waterlevel != 3) &&
		(GetWeaponReloadTime() + 1.0f < gpGlobals->time) && IsNotPaused(1.0f) && (grenade_use_time + (5.0f * (float)(GetBotSkill() + 1)) + RANDOM_FLOAT(-0.5f, 1.0f) < gpGlobals->time) &&
		(enemy_distance >= GetWeaponSafeRange(grenade_slot)) && (enemy_distance <= GetWeaponEffectiveRange(grenade_slot)))
	{

#ifdef DEBUG
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
		if (botdebugger.IsDebugWeapons())
		{
			char dm[128]{};
			sprintf(dm, "CanUse GRENADE() called with currT=%.2f | grenT=%.2f | botskill=%d | currT&grenT_delta=%.2f\n", gpGlobals->time, grenade_use_time, GetBotSkill() + 1, gpGlobals->time - grenade_use_time);
			conOutput.Notify(dm, this);
		}
#endif // DEBUG

		return true;
	}

	return false;
}


/*
* checks if the weapon is one of assault rifles
*/
bool IsAssaultRifle(int weapon, int specific_class)
{
	// enfield is without the scope if v.playerclass is 21
	return (((weapon == dod_weapon_enfield) && (specific_class == 21)) || (weapon == dod_weapon_garand) || (weapon == dod_weapon_kar) ||
		(weapon == dod_weapon_k43) || (weapon == dod_weapon_m1carbine));
}


/*
* checks if the weapon is one of sniper rifles
*/
bool IsSniperRifle(int weapon, int specific_class)
{
	// enfield is has the scope if v.playerclass is 23, fg42 has scope when playerclass is 16
	return (((weapon == dod_weapon_enfield) && (specific_class == 23)) || ((weapon == dod_weapon_fg42) && (specific_class == 16)) ||
		(weapon == dod_weapon_scopedkar) || (weapon == dod_weapon_spring));
}


/*
* checks if the weapon is one of machineguns
*/
bool IsMachinegun(int weapon)
{
	return ((weapon == dod_weapon_30cal) || (weapon == dod_weapon_bren) || (weapon == dod_weapon_mg34) || (weapon == dod_weapon_mg42));
}


/*
* checks if the weapon is one of light machineguns
*/
bool IsLMG(int weapon, int specific_class)
{
	// fg42 works as support gun when v.playerclass is 15
	return ((weapon == dod_weapon_bar) || ((weapon == dod_weapon_fg42) && (specific_class == 15)));
}


/*
* checks if the weapon is one of submachineguns
*/
bool IsSMG(int weapon)
{
	return ((weapon == dod_weapon_greasegun) || (weapon == dod_weapon_mp40) || (weapon == dod_weapon_mp44) || (weapon == dod_weapon_sten) || (weapon == dod_weapon_thompson));
}


/*
* checks if the weapon is one of grenade/rocket launchers
*/
bool IsRPG(int weapon)
{
	return ((weapon == dod_weapon_bazooka) || (weapon == dod_weapon_pschreck) || (weapon == dod_weapon_piat));
}


/*
* checks if the weapon is one of handguns/pistols
*/
bool IsHandgun(int weapon)
{
	return ((weapon == dod_weapon_colt) || (weapon == dod_weapon_luger) || (weapon == dod_weapon_webley));
}

bool IsMelee(int weapon) {
	return ((weapon == dod_weapon_amerknife) || (weapon == dod_weapon_gerknife) || (weapon == dod_weapon_spade));
}

/*
* checks if the weapon is a grenade
*/
bool IsGrenade(int weapon)
{
	return ((weapon == dod_weapon_handgrenade) || (weapon == dod_weapon_stickgrenade));
}

bool IsPrimary(int weapon) {
	return !IsHandgun(weapon) && !IsMelee(weapon) && !IsGrenade(weapon);
}


/*
* checks if the weapon has bipod
*/
bool IsBipodWeapon(int weapon, int specific_class)
{
	return ((weapon == dod_weapon_bar) || (weapon == dod_weapon_bren) || (weapon == dod_weapon_30cal) || (weapon == dod_weapon_mg34) || (weapon == dod_weapon_mg42) ||
		((weapon == dod_weapon_fg42) && (specific_class == 15)));
}


/*
* checks if the machine gun cannot be reloaded like other weapons, but needs to be deployed
*/
bool IsLimitedReloadMachinegun(int weapon)
{
	return ((weapon == dod_weapon_30cal) || (weapon == dod_weapon_mg34) || (weapon == dod_weapon_mg42));
}


/*
* checks if the weapon has scope and can zoom in
*/
bool IsWeaponWithOptics(int weapon, bool has_necessary_condition)
{
	// these always have the optics available
	if ((weapon == dod_weapon_scopedkar) || (weapon == dod_weapon_spring))
		return true;

	// has optics only in certain class
	if (((weapon == dod_weapon_enfield) || (weapon == dod_weapon_fg42)) && has_necessary_condition)
		return true;

	return false;
}


/*
* sets correct reload time to successfully finish reloading based on current weapon
*/
void DefineReloadTimeForCurrentWeapon(bot_t* pBot)
{
	// we don't really need to define specific times, because DoD sends a message when weapon reload is finished so we just set a long enough time here and then reset it while processing that message
	pBot->SetWeaponReloadTime(10.0f);
}


/*
* checks if is team play on
*/
void BotCheckTeamplay(void)
{
	// DoD doesn't seem to set the teamplay cvar at all
	auto teamlist = CVAR_GET_STRING("mp_teamlist");
	internals.SetTeamPlay(teamlist && teamlist[0]);
}


/*
* sets correct reaction delay based on bot skill level
* eg. if reaction time is set to 1 second then the best bot will use 0.5s as his reaction time
*/
void BotReactions(bot_t *pBot)
{
	// we shouldn't set bot reactions too often it could cause loop
	if ((pBot->GetBotReactionTime() + 2.0f) > gpGlobals->time)
		return;

	if (pBot->pBotEnemy)
	{
		Vector vEnemyHead = pBot->pBotEnemy->v.origin + pBot->pBotEnemy->v.view_ofs;
		Vector vEnemyBody = pBot->pBotEnemy->v.origin;
		
		// is the enemy right in front of the bot (ie. bot doesn't have to turn to side to face it)?
		if (util.IsInNarrowViewCone(&vEnemyHead, pBot->pEdict, 0.85f) || util.IsInNarrowViewCone(&vEnemyBody, pBot->pEdict, 0.85f))
		{
			// bot will fire/attack in next frame (ie. will skip this frame just to simulate some "aiming")
			pBot->SetBotReactionTime(0.0f);
			return;
		}
	}
	
	float react_time = externals.GetReactionTime();
	int skill = pBot->GetBotSkill() + 1;	// array based

	// we are using skill level 3 as a default level so its reaction time isn't changed, but other skill levels should be modified
	switch (skill)
	{
		case 1:
			react_time -= externals.GetReactionTime() / 2.0f;	// use only 50% of it
			break;
		case 2:
			react_time -= externals.GetReactionTime() / 3.0f;	// use only 66% of it
			break;
		case 4:
			react_time += externals.GetReactionTime() / 2.0f;	// use 150% of it
			break;
		case 5:
			react_time += externals.GetReactionTime();			// use 200% or it
			break;
		default:
			break;
	}

	// if we get under zero reset it back to zero value, shouldn't happen but just in case
	if (react_time < 0.0f)
		react_time = 0.0f;

	// store the reaction time
	pBot->SetBotReactionTime(react_time);
}


/*
* returns distance to current enemy
* returns 9999.0 if the enemy is NULL (ie. has no enemy at the moment)
*/
float BotGetDistanceToEnemy(bot_t* pBot)
{
	if (pBot->pBotEnemy != NULL)
	{
		return (pBot->GetLastKnownEnemyPosition() - pBot->pEdict->v.origin).Length();
	}

	return 9999.0f;
}


/*
* bot does these actions when do not have enemy at all or when do not currently see him (ie lost clear view)
*/
void DontSeeEnemyActions(bot_t *pBot)
{
	// is bot waiting whether enemy becomes visible again?
	if (pBot->IsNotWaitingForEnemy() == false)
	{
		// see if the bot is able to throw a grenade at the last known position of the enemy he just lost track of
		if (pBot->CanUseGrenade(BotGetDistanceToEnemy(pBot)))
		{
			pBot->UseGrenade("DontSeeEnemy Actions()");
		}

		// is not going to/resume from prone AND NOT paused (eg merging mags)?
		if (pBot->IsNotGoingProne() && pBot->IsNotPaused())
		{
			// if the bot doesn't use main weapon and can use it then try to switch back to it... but first handle these specific cases		??? machine guns - should be allowed when lying prone, but check it
			if (IsWeaponWithOptics(pBot->main_weapon, pBot->IsBehaviour(SNIPER)) || IsRPG(pBot->main_weapon) || IsMachinegun(pBot->main_weapon))
			{
				if (IsOutOfMinimumSafeDistanceForWeapon(pBot, pBot->main_weapon, BotGetDistanceToEnemy(pBot)))
					pBot->UseMainWeapon("DontSeeEnemy Actions() -> enemy is far enough");
			}
			// all other weapons are always good to use
			else
				pBot->UseMainWeapon("DontSeeEnemy Actions() -> time to switch back to MAIN WEAPON");

			// is current weapon ready?
			if (pBot->IsAllowedToHandleWeapon())
			{
				if (pBot->ShouldReload("DontSeeEnemy Actions() -> waiting for enemy"))
					pBot->ReloadWeapon("DontSeeEnemy Actions() -> waiting for enemy");
			}
		}
	}

	// hasn't the bot seen enemy in last few seconds AND is chance to speak (time for area clear)?
	if (pBot->NotSeenEnemyfor(8.0f) && pBot->NotSpokeFor(RANDOM_FLOAT(25.5f, 40.0f)) && (RANDOM_LONG(1, 100) <= 1))
	{
		pBot->BotSpeak(voiceCmd::area_clear);
	}
}


/*
* will make sure the bot "forgets" about his current enemy (ie. clear all important variables)
*/
void bot_t::BotForgetEnemy(void)
{
#ifdef _DEBUG

	// can't use hudnotify() here, because it can crash hl engine, if the bot gets hit then the code in botclient.cpp -> fa dmg message can call this function
	// (clientprint in hudnotify() starts new engine msg before the fa_dmg one was finished and hl crashes)
	if (devTool.IsBotDebugging(pEdict))
	{
		char femsg[128]{};
		sprintf(femsg, "%s called ForgetEnemy()\n", name);
		ALERT(at_console, femsg);


		// so if there is a need to log things in file we have to do it manually right here
		util.DebugInFile(femsg);

		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

		// @@@@@@@@@@@@		^^^^ (uncomment logging in file if it is needed for tests) ^^^^					NEW CODE 094

		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

	}

#endif

	// don't have an enemy anymore so null out the pointer
	pBotEnemy = NULL;
	// reset wait for enemy time
	ResetWaitForEnemyTime();
	// clear his last known position
	SetLastKnownEnemyPosition(g_vecZero);
	// reset previous distance to enemy
	SetPrevDistanceToEnemy(0.0f);
	// clear medical treatment flag
	RemoveTask(TASK_HEALHIM);
	// clear heavy tracelining
	RemoveTask(TASK_DEATHFALL);
	// bot most probably fired couple rounds at this enemy so let him check his weapon clip
	SetSubTask(ST_W_CLIP);
	// allow the bot to try deploying bipod later on when he finds new enemy
	RemoveWeaponStatus(WS_CANTBIPOD);
	
	//SetDontCheckStuck("ForgetEnemy()");
}


/*
* search the world for best enemy
* checks if enemy is still alive and visible and sets right behaviour based on it
*/
edict_t* bot_t::BotFindEnemy()
{
	Vector vecEnd;
	edict_t *pNewEnemy;
	edict_t *pPrevEnemy = NULL;		// previous enemy
	bool IsVisible;
	float enemy_distance, nearest_distance;
	int decide_visibility;

	// does the bot already have an enemy?
	if (pBotEnemy != NULL)
	{
		vecEnd = pBotEnemy->v.origin + pBotEnemy->v.view_ofs;

		// the bot is going to medevac downed teammate so don't look for new enemy
		if (IsTask(TASK_HEALHIM) && IsTask(TASK_MEDEVAC))
		{
			return (pBotEnemy);
		}

		// the enemy is already dead (the enemy probably died during previous frame, assume bot or someone else killed it)
		else if (util.IsAlive(pBotEnemy) == false)
		{
			BotForgetEnemy();

			// is still snipe time?
			if (IsNotSnipeTime() == false)
			{
				// break snipe time only if bot is NOT using bipod AND is chance
				if ((IsMachinegun(current_weapon.iId)) && (IsTask(TASK_BIPOD) == false) && (RANDOM_LONG(1, 100) < 10))
				{
					SetSnipeTime(0.0f);	// so stop sniping
#ifdef _DEBUG
					// testing - snipe time
					if (in_bot_dev_level1)
					{
						char msg[80]{};
						sprintf(msg, "BREAKING snipe time\n");
						conOutput.Notify(msg);
					}
#endif
				}
			}
		}
		// the enemy must still be alive so ...
		else
		{
			// this "enemy" is a wounded teammate
			if (IsTask(TASK_HEALHIM))
			{
				// just return its pointer
				return (pBotEnemy);
			}

			// the bot is carrying a goal item so make him less aggresive, but
			// we have to ignore this limit when bot carries an explosive charge, because that isn't true goal item, the explosive charge is a mean to reach map goal
			if (IsTask(TASK_GOALITEM) && (IsSubTask(ST_GOALITEM_BOMB) == false))
			{
				// don't attack "healthy" enemy	-- PROBABLY NOT SO GOOD IDEA TO LIMIT BOT THIS MUCH
				//if (pBotEnemy->v.health > 25)
				//{
				//	BotForgetEnemy();
				//	return (pBotEnemy);
				//}

				enemy_distance = (pBotEnemy->v.origin - pEdict->v.origin).Length();

				// don't attack distant enemy
				if (enemy_distance > ENEMY_DIST_GOALITEM)
				{
					BotForgetEnemy();
					return (pBotEnemy);
				}
			}

			if (IsTask(TASK_IGNORE_ENEMY))
			{
				enemy_distance = (pBotEnemy->v.origin - pEdict->v.origin).Length();
				if (enemy_distance > RANGE_MELEE)
				{
					BotForgetEnemy();
					return (pBotEnemy);
				}
			}

			// the enemy probably won't attack this bot so forget about him
			if (IsTask(TASK_AVOID_ENEMY))
			{
				enemy_distance = (pBotEnemy->v.origin - pEdict->v.origin).Length();
				if (enemy_distance > GetWeaponEffectiveRange() && (enemy_distance > RANGE_MELEE))
				{
					BotForgetEnemy();
					return (pBotEnemy);
				}
			}

			// check for position and visibility first
			bool InFOV = util.IsInViewCone( &vecEnd, pEdict );
			IsVisible = false;
			decide_visibility = util.IsPlayerVisible( vecEnd, pEdict );
			
			// decide visibility based on current weapon (ie. we don't want bots trying to shoot an enemy through fence with a weapon that cannot do it ... eg. small arms)
			switch (decide_visibility)
			{
				case VIS_NO:
					IsVisible = false;
					break;
				case VIS_YES:
					IsVisible = true;
					break;
				case VIS_FENCE:
					if (IsMachinegun(current_weapon.iId) || IsLMG(current_weapon.iId, pEdict->v.playerclass) || IsSniperRifle(current_weapon.iId, pEdict->v.playerclass) ||
						IsAssaultRifle(current_weapon.iId, pEdict->v.playerclass))
						IsVisible = true;
					else
						IsVisible = false;
					break;
				case VIS_WATER:
					BotForgetEnemy();	// NOTE: Just for now. Once the code is really implemented this should be changed to match cases above, ie. either set the enemy is visible or not
					return (pBotEnemy);	// and then also use standard break instead of "hard break aka return" to continue normally
			}

			// enemy is alive and fully visible OR was visible in last few seconds
			if ((InFOV && IsVisible) || HasSeenEnemyInLast(0.2f))
			{
				// is bot waiting for current enemy to become visible?
				if (IsNotWaitingForEnemy() == false)
				{
					// then reset the wait for enemy time, because enemy is visible now
					SetWaitForEnemyTime(-0.1f);
					
					// apply reaction time
					BotReactions(this);
				}

				// should bot search for different enemy?
				if (IsTask(TASK_FIND_ENEMY))
				{
					pPrevEnemy = pBotEnemy;	// safe pointer to your current enemy
				}
				// otherwise return the current one
				else
				{
					// face the enemy
					Vector v_enemy = pBotEnemy->v.origin - pEdict->v.origin;
					Vector bot_angles = UTIL_VecToAngles( v_enemy );
					
					pEdict->v.ideal_yaw = bot_angles.y;
					BotFixIdealYaw(pEdict);

					SetSubTask(ST_FACEENEMY);

					// keep track of when we last saw an enemy
					if (IsVisible)
					{
						SetSeeEnemyTime();
						// store this enemy known (ie. visible) position
						SetLastKnownEnemyPosition(pBotEnemy->v.origin);
					}
					
					return (pBotEnemy);
				}
			}
			else
			{
				// the enemy is completely lost
				if (InFOV == false)
				{
					BotForgetEnemy();
					return (pBotEnemy);
				}
				
				// the enemy must still be somewhere in front of the bot, it's just not visible right now ...

				// is NOT already waiting for the enemy to become visible?
				if (IsNotWaitingForEnemy())
				{
					if (IsGrenadeUseTime())
					{
						// when is the bot adjusting the aiming vector to throw the grenade correctly he can easily lose direct visibility of the enemy
						// so we will simply set the grenade use time as the wait for enemy time to make him finish this action successfully
						SetWaitForEnemyTime(GetGrenadeUseTime() - gpGlobals->time);
						return (pBotEnemy);
					}
					else
					{
						// first generate the percentage chance
						int do_watch = RANDOM_LONG(1, 100);

						// attacking minded bot has a 30% chance to keep watching that direction and wait whether this enemy becomes visible again
						if (IsBehaviour(ATTACKER) && (do_watch <= 30))
						{
							// so set the wait for enemy time, but attackers won't be waiting that long
							SetWaitForEnemyTime(RANDOM_FLOAT(3.0f, 6.0f));
							return (pBotEnemy);
						}
						// defending minded bot has a 65% chance to keep watching that direction and wait whether this enemy becomes visible again
						else if (IsBehaviour(DEFENDER) && (do_watch <= 65))
						{
							// defenders will wait for quite long
							SetWaitForEnemyTime(RANDOM_FLOAT(8.0f, 13.0f));
							return (pBotEnemy);
						}
						// common bot has a 45% chance to keep watching that direction and wait whether this enemy becomes visible again
						else if (IsBehaviour(STANDARD) && (do_watch <= 45))
						{
							SetWaitForEnemyTime(RANDOM_FLOAT(5.0f, 7.5f));
							return (pBotEnemy);
						}
					}
					
					BotForgetEnemy();
					return (pBotEnemy);
				}
				// otherwise bot is already waiting for enemy...
				else
				{
					// is the bot in the process of throwing a grenade at this enemy? then don't try to forget about him, but continue in the action with the grenade
					if (IsGrenadeUseTime())
					{
					}
					else
					{
						// is bot doing so for some time already? then see if we can stop it and forget about this enemy
						if (IsBehaviour(ATTACKER) && (GetWaitForEnemyTime() - 2.0f < gpGlobals->time) && (RANDOM_LONG(1, 100) < 10))
						{
							// is the right moment AND the enemy wasn't really close when bot saw him last time?
							if ((RANDOM_LONG(1, 100) < 35) && ((GetLastKnownEnemyPosition() - pEdict->v.origin).Length() > RANGE_MELEE))
							{
#ifdef _DEBUG
								ALERT(at_console, "<%s>***enemy still not visible - breaking the watch\n", name);
#endif
								BotForgetEnemy();
								return (pBotEnemy);
							}
						}
						else if (IsBehaviour(DEFENDER) && (GetWaitForEnemyTime() - 6.0f < gpGlobals->time) && (RANDOM_LONG(1, 100) < 2))
						{
							if ((RANDOM_LONG(1, 100) < 5) && ((GetLastKnownEnemyPosition() - pEdict->v.origin).Length() > RANGE_MELEE))
							{
#ifdef _DEBUG
								ALERT(at_console, "<%s>***enemy still not visible - breaking the watch\n", name);
#endif
								BotForgetEnemy();
								return (pBotEnemy);
							}
						}
						else if (IsBehaviour(STANDARD) && (GetWaitForEnemyTime() - 3.0f < gpGlobals->time) && (RANDOM_LONG(1, 100) < 5))
						{
							if ((RANDOM_LONG(1, 100) < 20) && ((GetLastKnownEnemyPosition() - pEdict->v.origin).Length() > RANGE_MELEE))
							{
#ifdef _DEBUG
								ALERT(at_console, "<%s>***enemy still not visible - breaking the watch\n", name);
#endif
								BotForgetEnemy();
								return (pBotEnemy);
							}
						}
					}

					// bot decided to continue waiting for his enemy...


//@@@@@@@@@@@@@
#ifdef _DEBUG
					if (GetLastKnownEnemyPosition() == g_vecZero)
					{
						ALERT(at_console, "***<%s> last known position is (0,0,0)\n", name);
						char smsg[256]{};
						sprintf(smsg, "<%s> last known position of <%s> is (0,0,0)\n", name, STRING(pBotEnemy->v.netname));
						util.DebugDev(smsg);
					}
#endif
					
					// is bot tasked to look for a different enemy?
					if (IsTask(TASK_FIND_ENEMY))
					{
						pPrevEnemy = pBotEnemy;	// safe pointer to your current enemy
					}
					// otherwise keep facing current one
					else
					{
						// face the last known position of this enemy
						Vector vec_tolastknownenemypos = GetLastKnownEnemyPosition() - pEdict->v.origin;
						Vector bot_angles = UTIL_VecToAngles(vec_tolastknownenemypos);
						
						pEdict->v.ideal_yaw = bot_angles.y;
						BotFixIdealYaw(pEdict);
						
						// do the weapon management actions only when not trying to throw a grenade
						if (IsGrenadeUseTime() == false)
							DontSeeEnemyActions(this);

						return (pBotEnemy);
					}
				}				
			}	// END enemy isn't fully visible
		}	// END enemy is alive
	}	// END pBot->pBotEnemy != NULL

	pNewEnemy = NULL;

	if (pNewEnemy == NULL)
	{
		float player_distance;

		// if bot already has an enemy try to find someone else who's closer
		if (pPrevEnemy != NULL)
		{
			if (IsNotWaitingForEnemy())
				nearest_distance = (pPrevEnemy->v.origin - pEdict->v.origin).Length();
			// waiting for current enemy case
			else
				nearest_distance = (GetLastKnownEnemyPosition() - pEdict->v.origin).Length();
		}
		// do we limit max enemy distance?
		else if (internals.IsEnemyDistanceLimit())
		{
			// then use the limit for all bots
			nearest_distance = internals.GetEnemyDistanceLimit();
		}
		// no previous enemy and no distance limit?
		else
		{
			// is it a sniper bot then there's no range limit to search for an enemy
			if (IsSniperRifle(current_weapon.iId, pEdict->v.playerclass))
				nearest_distance = 9999.0f;
			// otherwise search for enemies within default distance
			else
				nearest_distance = internals.GetEnemyDistanceLimit();
		}

		// and finally check for special cases and tweak the search distance based on them
		if (IsTask(TASK_GOALITEM) && (IsSubTask(ST_GOALITEM_BOMB) == false))
		{
			nearest_distance = ENEMY_DIST_GOALITEM;
		}
		else if (IsTask(TASK_IGNORE_ENEMY))
		{
			nearest_distance = RANGE_MELEE;
		}
		else if (IsTask(TASK_AVOID_ENEMY))
		{
			nearest_distance = GetWeaponEffectiveRange();
		}

		// search the world for players
		for (int clients = 1; clients <= gpGlobals->maxClients; clients++)
		{
			edict_t *pPlayer = INDEXENT(clients);

			// skip invalid players AND skip self (i.e. this bot)
			if ((pPlayer) && (!pPlayer->free) && (pPlayer != pEdict))
			{
				// skip this player if not alive (i.e. dead or dying)
				if (util.IsAlive(pPlayer) == false)
					continue;

				// ignore observerving player
				if (botdebugger.IsObserverMode() && !(pPlayer->v.flags & FL_FAKECLIENT))
					continue;

				// don't target your teammates or players from unknown team
				if (util.AreTeammates(pPlayer, pEdict))
					continue;

				// get the distance
				player_distance = (pPlayer->v.origin - pEdict->v.origin).Length();

				// skip players that are farther than nearest distance limit
				if (player_distance > nearest_distance)
					continue;

				vecEnd = pPlayer->v.origin + pPlayer->v.view_ofs;

				IsVisible = false;
				decide_visibility = util.IsPlayerVisible( vecEnd, pEdict );
				
				switch (decide_visibility)
				{
					case VIS_NO:
						IsVisible = false;
						break;
					case VIS_YES:
						IsVisible = true;
						break;
					case VIS_FENCE:
						if (IsMachinegun(current_weapon.iId) || IsLMG(current_weapon.iId, pEdict->v.playerclass) || IsSniperRifle(current_weapon.iId, pEdict->v.playerclass) ||
							IsAssaultRifle(current_weapon.iId, pEdict->v.playerclass))
							IsVisible = true;
						else
							IsVisible = false;
						break;
					case VIS_WATER:
						IsVisible = false;
						break;
				}

				// see if bot can see the player
				if (util.IsInViewCone( &vecEnd, pEdict ) && IsVisible)
				{
					// this player must be closer so update the distance limit
					nearest_distance = player_distance;
					pNewEnemy = pPlayer;

					pTeamLeader = NULL;  // don't follow team leader when enemy found
				}
			}
		}
	}

	if(pNewEnemy == NULL) {
		//any monsters nearby?
		float nearest_distance = 1000.f;
		edict_t* pMonster = NULL;
		while(pMonster = util.FindEntityInSphere(pMonster, pEdict->v.origin, nearest_distance)) {
			if(!(pMonster->v.flags & FL_MONSTER) || pMonster->v.takedamage == DAMAGE_NO || !util.IsAlive(pMonster))
				continue;

			if(util.AreTeammates(pEdict, pMonster) || (util.GetTeam(pMonster) == teamNULL && pMonster != pEdict->v.dmg_inflictor))
				continue;

			auto monster_distance = (pMonster->v.origin - pEdict->v.origin).Length();
			if(monster_distance > nearest_distance)
				continue;

			vecEnd = 0.5f * (pMonster->v.absmin + pMonster->v.absmax);
			if(!util.IsPlayerVisible(vecEnd, pEdict))
				continue;

			if(!util.IsInViewCone(&vecEnd, pEdict))
				continue;

			nearest_distance = monster_distance;
			pNewEnemy = pMonster;
		}
	}

	if (pNewEnemy)
	{
		// face the enemy
		Vector v_enemy = pNewEnemy->v.origin - pEdict->v.origin;
		Vector bot_angles = UTIL_VecToAngles( v_enemy );

		pEdict->v.ideal_yaw = bot_angles.y;
		BotFixIdealYaw(pEdict);

		SetSubTask(ST_FACEENEMY);

		// had the bot an enemy even before AND was it exactly this one so break it right now
		if ((pPrevEnemy != NULL) && (pNewEnemy == pPrevEnemy))
			return (pPrevEnemy);

		// had the bot an enemy before then we have to clear all values
		if (pPrevEnemy != NULL)
			BotForgetEnemy();

		// has to be here because the reaction code uses it and if we didn't update it with the new enemy the rections would be set incorrectly
		pBotEnemy = pNewEnemy;

#ifdef DEBUG
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
		if (botdebugger.IsDebugActions())
			conOutput.Notify("***FindEnemy() -> Got NEW ENEMY!!!\n", this);
#endif // DEBUG

		// apply reaction time
		BotReactions(this);

		// keep track of when we last saw an enemy
		SetSeeEnemyTime();

		// store enemy last known (visible) position, the enemy must be visible at this moment otherwise the bot wasn't able to target him
		SetLastKnownEnemyPosition(pBotEnemy->v.origin);

		// initialize the override of advance toward enemy
		SetOverrideAdvanceTime(RANDOM_FLOAT(0.0f, 1.5f));

		// was there no enemy before (i.e. this is completely new enemy)?
		if (pPrevEnemy == NULL)
		{
			// is this bot a leader?
			if (util.CanBeFireTeamLeader(this))
			{
				// did he decide to ignore forming the fireteam?
				if (IsNeed(NEED_FIRETEAM_NOT))
				{
					// then see if this decision can be reconsidered
					if (RANDOM_LONG(1, 100) < 66)
						RemoveNeed(NEED_FIRETEAM_NOT);
				}
				// otherwise see if he didn't try to form a fireteam in a while and is chance to do so now (well after the battle)
				else if ((IsNeed(NEED_FIRETEAM) == false) && NotSpokeFor(RANDOM_FLOAT(10.0f, 20.0f)) && (RANDOM_LONG(1, 100) < 66))
					SetNeed(NEED_FIRETEAM);
			}

			// is chance to speak now? then try to warn teammates (actually it'll be say team text message, because DoD equivalent for get down command is somewhat bugged - hand signaling to wrong direction)
			if (NotSpokeFor(RANDOM_FLOAT(25.5f, 59.0f)) && (RANDOM_LONG(1, 100) <= 50))
				//BotSpeak(voiceCmd::get_down);	// in DoD is "enemy ahead" voice message for this, but it's bugged so it's been replaced with text message for team
				UseTextMessage(botSay::enemy_spotted, NULL, 15.0f);
		}

		// is bot on PATROL path then set return flag
		if (IsPatrolPathWaypoint() && (IsTask(TASK_BACKTOPATROL) == false))
		{
			SetTask(TASK_BACKTOPATROL);

#ifdef _DEBUG
			//@@@@@
			if (botdebugger.IsDebugPaths())
				conOutput.Notify("FindEnemy() -> PATROL path return flag SET on\n", this);
#endif
		}

		pPrevEnemy = NULL;	// null out pointer just for sure
	}

	DontSeeEnemyActions(this);

	// is it some time since we saw an enemy so reset it to prevent doing all those things (speaking etc.) over and over
	if (NotSeenEnemyfor(15.0f))
		ResetSeeEnemyTime();

	return (pNewEnemy);
}


/*
* returns whether the bot can aim for a headshot
*/
bool CanAimForHeadshot(int headshot_chance, int aim_skill, bool is_sniper)
{
	// by default we're using the hard-coded headshot chance based on
	// the aiming skill level and held weapon for each particular bot
	if (externals.GetCustomHeadshotPercentage() == -1)
	{
		// the percentual chance for a headshot based on the aiming skill (from best to worst)...
		// when the bot has a sniper rifle
		int sniper_percentages[BOT_SKILL_LEVELS] = { 100, 85, 50, 25, 5 };
		// and when the bot is using any other weapon
		int others_percentages[BOT_SKILL_LEVELS] = { 95, 70, 30, 10 , 0 };

		// is the bot using a sniper rifle?
		if (is_sniper)
		{
			// then use the percentual chances for sniper rifles
			if (headshot_chance <= sniper_percentages[aim_skill])
			{
#ifdef DEBUG
				//conOutput.Notify("Aiming for a HEADSHOT(sniper)\n");
#endif

				return true;
			}
		}
		else
		{
			if (headshot_chance <= others_percentages[aim_skill])
			{
#ifdef DEBUG
				//conOutput.Notify("Aiming for a HEADSHOT\n");
#endif

				return true;
			}
		}

	}
	// otherwise we'll use the custom headshot percentage read from external .cfg
	// for all bots no matter what are the aiming skill levels and held weapons
	else
	{
		if (headshot_chance <= externals.GetCustomHeadshotPercentage())
		{
#ifdef DEBUG
			//conOutput.Notify("Aiming for a HEADSHOT(user setting)\n");
#endif

			return true;
		}
	}

#ifdef DEBUG
	//conOutput.Notify("Aiming at BODY\n");
#endif

	return false;
}


bool g_test_aim_code = false;		// global switch to allow the new aim patch even in release compilation


/*
* find best point to target based on visiblity and aim skill level
*/
Vector BotBodyTarget(bot_t *pBot)
{
	Vector target, target_origin, target_head;
	float foe_distance;
	float dist_scale = 1.0f;			// distance based modifier for the offsets
	bool is_using_optics = false;		// is the bot targeting enemy through weapon optics/scope?
	float x_ofs, y_ofs, z_ofs;			// plain aiming offsets read from the offsets array
	float d_x = 0.0f, d_y = 0.0f, d_z = 0.0f;		// the final offsets that were modified by target distance
	int hs_percentage;					// precentual chance to aim for a headshot

	edict_t *pEdict = pBot->pEdict;
	edict_t *pBotEnemy = pBot->pBotEnemy;

	foe_distance = (pBotEnemy->v.origin - pEdict->v.origin).Length();

	target_origin = pBotEnemy->v.origin;
	target_head = pBotEnemy->v.view_ofs;

	// calculate the aim vector in a special way when the bot is about to throw the grenade (ie. aim much higher than the enemy head is)
	if (IsGrenade(pBot->current_weapon.iId))
	{
		// use just the origin ie. don't bother with enemy head, because there's nothing like a headshot with grenade
		target = target_origin;

		// when the enemy is at the minimum safe distance aim about 2 times the full player heights above him and every 100 units farther away mean aiming another full player height higher
		dist_scale = 7.0f - ((1000.0f - foe_distance) / 100.0f);
		d_z = 72.0f * dist_scale;

		// is bot lying prone? then aim even heigher
		if (pBot->IsProne())
			d_z += 72.0f;

#ifdef _DEBUG
		if (botdebugger.IsDebugWeapons(2))
		{
			char msg[TEXT_MSG_SIZE]{};
			sprintf(msg, "(GRENADES) BodyTarget() -> dist_scale:%.1f | height aim offset is: %.1f\n", dist_scale, d_z);
			conOutput.Notify(msg, pBot);
		}
#endif
	}
	else
	{
		// generate the percentual chance to aim for a headshot
		hs_percentage = RANDOM_LONG(1, 100);

		// distance modifier is based on the optics/scope zoom level current weapon allows where guns with no optics have worse value
		if (pEdict->v.fov == ZOOM_1X)
		{
			dist_scale = foe_distance / 1500.0f;
			is_using_optics = true;
		}
		else
		{
			dist_scale = foe_distance / 500.0f;
			is_using_optics = false;
		}

		// now decide whether to aim for the headshot or not, if the bot uses weapon optics/scope the chance is slightly higher
		if (CanAimForHeadshot(hs_percentage, pBot->GetAimSkill(), is_using_optics))
			target = target_origin + target_head;
		else
			target = target_origin;

		// get the basic offsets from the offset array based on the aiming skill and the use of optics/scope
		if (is_using_optics)
		{
			x_ofs = bot_target_offset[pBot->GetAimSkill()].x_axis_sniper;
			y_ofs = bot_target_offset[pBot->GetAimSkill()].y_axis_sniper;
			z_ofs = bot_target_offset[pBot->GetAimSkill()].z_axis_sniper;
		}
		else
		{
			x_ofs = bot_target_offset[pBot->GetAimSkill()].x_axis;
			y_ofs = bot_target_offset[pBot->GetAimSkill()].y_axis;
			z_ofs = bot_target_offset[pBot->GetAimSkill()].z_axis;
		}

		// now we can generate the final offsets using the basic/plain offsets and the distance modifier
		if (x_ofs == 0.0f)
			// if the basic/plain offset is zero we will mess only with the distance modifier
			d_x = RANDOM_FLOAT(-0.1f, 0.1f) * dist_scale;
		else
			d_x = RANDOM_FLOAT(-x_ofs, x_ofs) * dist_scale;

		if (y_ofs == 0.0f)
			d_y = RANDOM_FLOAT(-0.1f, 0.1f) * dist_scale;
		else
			d_y = RANDOM_FLOAT(-y_ofs, y_ofs) * dist_scale;

		if (z_ofs == 0.0f)
			d_z = RANDOM_FLOAT(-0.1f, 0.1f) * dist_scale;
		else
			d_z = RANDOM_FLOAT(-z_ofs, z_ofs) * dist_scale;
	}

	// add offset to initial aim vector
	target = target + Vector(d_x, d_y, d_z);

	return target;
}


/*
* fires the mounted gun
*/
void BotFireMountedGun(bot_t* pBot, float enemy_distance)
{
	// is enemy close so use full-auto fire
	if (enemy_distance < 1000.0f)
	{
		pBot->f_shoot_time = gpGlobals->time;
	}
	// otherwise randomly switch between full-auto and simi fire
	else
	{
		if (RANDOM_LONG(1, 100) < 15)
			pBot->f_shoot_time = gpGlobals->time + RANDOM_FLOAT(0.1f, 0.85f);
		else
			pBot->f_shoot_time = gpGlobals->time;
	}

	// check if bot can press the trigger
	if (botdebugger.IsDontShoot() == false)
		pBot->pEdict->v.button |= IN_ATTACK;  // press primary attack button

	return;
}


/*
* fires main or backup weapon (assuming enough ammo exists for that weapon and checking effective range)
* also handles the alternate fire modes such as firing the attached grenade launcher or switching to the use of scope
*/
int BotFireWeapon(bot_t* pBot, float enemy_distance)
{
	bot_weapon_select_t *pSelect = NULL;
	bot_fire_delay_t *pDelay = NULL;
	edict_t *pEdict = NULL;
	int weaponID = NO_VAL;
	bool press_trigger;
	float min_safe_distance, base_delay, min_delay, max_delay;

	// the weapon is NOT ready yet? ... then break it right away
	if (pBot->weapon_action != W_READY)
	{
		return RETURN_NOTFIRED;
	}

	if (internals.IsMeleeOnlyMode())
	{
		pBot->UseKnife();

		return RETURN_NOTFIRED;
	}

	// see which weapon is used
	if (pBot->IsUsedWeaponMain())
		weaponID = pBot->main_weapon;
	else if (pBot->IsUsedWeaponBackup())
		weaponID = pBot->backup_weapon;
	else
	{
#ifdef _DEBUG
		char error_msg[128]{};
		sprintf(error_msg, "<<BUG>>FireWeapon() --->>> not using main or backup weapon (weaponID %d)!!!!\n", pBot->current_weapon.iId);
		util.DebugInFile(error_msg);
#endif
		return RETURN_NOTFIRED;
	}

	// is this slot empty?
	if (weaponID == NO_VAL)
	{
		// switch to melee weapon
		pBot->UseWeapon(uWeapon::knife);

		return RETURN_NOTFIRED;
	}
	
	// the weapon isn't held in hands yet?
	if (pBot->current_weapon.iId != weaponID)
	{
		// then set this flag to allow weapon change
		pBot->weapon_action = W_TAKEOTHER;

		if (botdebugger.IsDebugWeapons())
			conOutput.Notify("FireWeapon() -> going to change current weapon\n", pBot);

		return RETURN_TAKING;
	}

	// first initialize them all 
	pSelect = &bot_weapon_select[0];
	pDelay = &bot_fire_delay[0];
	pEdict = pBot->pEdict;
	press_trigger = FALSE;

	// standard fire mode statement, most weapons have NO secondary fire mode (or special weapon action if you wish)
	if (pBot->IsWeaponSecondaryModeActive() == false)
	{
		// check if clip is empty
		if (pBot->current_weapon.iClip == 0)
		{
			// the bot has no magazines
			if (pBot->current_weapon.iAmmo1 == 0)
			{
#ifdef _DEBUG
				// testing - empty weapon
				if (in_bot_dev_level1)
				{
					ALERT(at_console, "No ammo for weapon ID=%d (inClip=%d Magazines=%d)\n", pBot->current_weapon.iId, pBot->current_weapon.iClip, pBot->current_weapon.iAmmo1);
				}
#endif
				// no ammo flag must be set
				pBot->SetWeaponIsOutOfAmmo("FireWeapon() -> no mags");

				return RETURN_NOAMMO;
			}
			// otherwise the bot has at least one magazine
			else
			{
				// see if we can fast switch to backup weapon instead of realoding
				if (CanUseBackupInsteadofReload(pBot, enemy_distance))
				{
					return RETURN_NOTFIRED;
				}

				pBot->ReloadWeapon("FireWeapon() -> weapon clip is empty");

				return RETURN_RELOADING;
			}
		}	// END if clip is empty

		min_safe_distance = pSelect[weaponID].min_safe_distance;

		// carries the bot this weapon AND is it active (ie. in hands)?
		if ((pBot->bot_weapons & (1 << pSelect[weaponID].iId)) && (pBot->current_weapon.isActive == 1))
		{
			// is the bot in effective range of this weapon?
			if ((enemy_distance <= pSelect[weaponID].max_effective_distance) && (enemy_distance >= min_safe_distance))
			{
#ifdef _DEBUG	// testing - firing weapon and ammo info
				if (in_bot_dev_level1)
				{
					ALERT(at_console, "FIRE weapon ID=%d Array ID=%d (iClip=%d Mags=%d Dist=%.2f)\n",
						pBot->current_weapon.iId, pSelect[weaponID].iId, pBot->current_weapon.iClip, pBot->current_weapon.iAmmo1, enemy_distance);
				}
#endif
				press_trigger = TRUE;	// bot is ready to fire

				// is bot using sniper rifle OR machinegun/light machinegun?
				if (IsWeaponWithOptics(pBot->current_weapon.iId, pBot->IsBehaviour(SNIPER)) || IsBipodWeapon(pBot->current_weapon.iId, pEdict->v.playerclass))
				{
					// is it time to set new snipe time? (but don't set it when bot is moving closer to enemy)
					if (pBot->IsNotSnipeTime() && pBot->IsNotAdvancingTowardEnemy())
					{
						bool start_sniping = FALSE;

						// is the bipod deployed OR is bot using a machine gun? ... then do snipe all the time
						if (pBot->IsTask(TASK_BIPOD) || IsMachinegun(pBot->current_weapon.iId))
							start_sniping = TRUE;
						// otherwise bot needs a chance of 85% to start sniping
						else if (RANDOM_LONG(1, 100) <= 85)
							start_sniping = TRUE;

						if (start_sniping)
						{
							// set sniping time based on behaviour type where bot defenders spend more time sniping
							if (pBot->IsBehaviour(DEFENDER))
								pBot->SetSnipeTime( RANDOM_FLOAT(7.0f, 15.0f) );
							else
								pBot->SetSnipeTime( RANDOM_FLOAT(3.5f, 10.5f) );
#ifdef _DEBUG
							// testing - snipe time
							if (in_bot_dev_level1)
							{
								/*/
								char msg[80];
								sprintf(msg, "Snipe time set %.2f\n", pBot->GetSnipeTime());
								conOutput.Notify(msg, pBot);
								/**/
							}
#endif
						}
					}

					// is bot using one of these sniper rifles AND scope is NOT active yet AND is sniper time?
					if (IsWeaponWithOptics(pBot->current_weapon.iId, pBot->IsBehaviour(SNIPER)) && (pEdict->v.fov != ZOOM_1X) && (pBot->IsNotSnipeTime() == false))
					{
						pEdict->v.button |= IN_ATTACK2;				// switch to scope
						pBot->f_shoot_time = gpGlobals->time + 0.4f;	// time to take effect

						if (botdebugger.IsDebugWeapons())
						{
							char msg[128]{};
							sprintf(msg, "FireWeapon() -> Trying scope on weapon <%s>\n", util.PrintWeaponName(pBot->current_weapon.iId));
							conOutput.Notify(msg, pBot);
						}

						return RETURN_SECONDARY;
					}
					// is bot using one of weapons with bipod AND NOT used bipod yet AND is he inside special standing bipod area?
					else if (IsBipodWeapon(pBot->current_weapon.iId, pEdict->v.playerclass) && (pBot->IsTask(TASK_BIPOD) == false) && IsInStandingBipodSpot(pEdict))
					{
						BotUseBipod(pBot, true, "FireWeapon() -> Trying to deploy bipod at standing bipod spot");

						if (botdebugger.IsDebugWeapons())
						{
							char msg[128]{};
							sprintf(msg, "FireWeapon() -> Trying bipod on weapon <%s> at special bipod spot\n", util.PrintWeaponName(pBot->current_weapon.iId));
							conOutput.Notify(msg, pBot);
						}

						return RETURN_SECONDARY;
					}
					// is bot using one of machineguns AND NOT used bipod yet AND is sniper time?
					else if (IsMachinegun(pBot->current_weapon.iId) && (pBot->IsTask(TASK_BIPOD) == false) && (pBot->IsNotSnipeTime() == false) && CanDeployBipod(pEdict))
					{
						BotUseBipod(pBot, true, "FireWeapon() -> Trying bipod during sniper time");

						if (botdebugger.IsDebugWeapons())
						{
							char msg[128]{};
							sprintf(msg, "FireWeapon() -> Trying bipod on weapon <%s>\n", util.PrintWeaponName(pBot->current_weapon.iId));
							conOutput.Notify(msg, pBot);
						}

						return RETURN_SECONDARY;
					}

#ifdef _DEBUG
					if (botdebugger.IsDebugWeapons())
					{
						char msg[128]{};
						sprintf(msg, "FireWeapon() -> Must stop or slow down to fire weapon <%s>\n", util.PrintWeaponName(pBot->current_weapon.iId));
						conOutput.Notify(msg, pBot);
					}
#endif

					// is bot moving too fast to fire? ... then don't allow shooting
					if (pEdict->v.velocity.Length() > 0.0f)
					{
						return RETURN_NOTFIRED;
					}
				}

				// is bot using one of rocket launchers?
				else if (IsRPG(pBot->current_weapon.iId))
				{
					BotSwitchGrenadeLauncherFireMode(pBot, "FireWeapon() -> Preparing to fire");			// put it on shoulder

					// set short snipe time just to finish preparing the weapon and then fire one grenade
					if (pBot->IsNotAdvancingTowardEnemy() && pBot->IsNotSnipeTime())
						pBot->SetSnipeTime(pBot->f_shoot_time + 2.5f);

					if (botdebugger.IsDebugWeapons())
					{
						char msg[128]{};
						sprintf(msg, "FireWeapon() -> Preparing to fire <%s>\n", util.PrintWeaponName(pBot->current_weapon.iId));
						conOutput.Notify(msg, pBot);
					}

					return RETURN_SECONDARY;
				}

				// is bot using any other weapon AND is he moving?
				else if (pEdict->v.velocity.Length() > 0.0f)
				{
					// the 2 best skill levels always stop before firing, the other skill levels do stop in 80%, 55% and 30% of the time
					if ((pBot->GetBotSkill() < 2) || ((pBot->GetBotSkill() == 2) && (RANDOM_LONG(1, 100) > 20)) || ((pBot->GetBotSkill() == 3) && (RANDOM_LONG(1, 100) > 45)) ||
						((pBot->GetBotSkill() > 3) && (RANDOM_LONG(1, 100) > 70)))
					{
						// stop the bot for a while to lower the velocity and improve accuracy
						pBot->SetDontMoveTime(0.8f);
						return RETURN_NOTFIRED;
					}
				}
			}// END is in effective range

			// is bot too close to fire this weapon?
			else if (enemy_distance < pSelect[weaponID].min_safe_distance)
			{
				if (botdebugger.IsDebugWeapons(2))
				{
					char msg[128]{};
					sprintf(msg, "FireWeapon() -> TOO CLOSE to fire <%s> (FoeDistance=%.2f)\n", util.PrintWeaponName(pBot->current_weapon.iId), enemy_distance);
					conOutput.Notify(msg, pBot);
				}

				return RETURN_TOOCLOSE;
			}

			// is bot too far to fire this weapon?
			else if (enemy_distance > pSelect[weaponID].max_effective_distance)
			{
				if (botdebugger.IsDebugWeapons(2))
				{
					char msg[128]{};
					sprintf(msg, "FireWeapon() -> TOO FAR to fire <%s> (FoeDistance=%.2f)\n", util.PrintWeaponName(pBot->current_weapon.iId), enemy_distance);
					conOutput.Notify(msg, pBot);
				}

				return RETURN_TOOFAR;
			}
		}// END is bot carrying this weapon

#ifdef _DEBUG
		// testing - check if weapons in array pSelect and in array pDelay are in same order
		if (pSelect[weaponID].iId != pDelay[weaponID].iId)
		{
			char error_msg[256]{};
			sprintf(error_msg, "<<BUG>>FireWeapon() -> Weapon order in pSelect(weapon ID=%d) is NOT same as in pDelay(weapon ID=%d)\n", pSelect[weaponID].iId, pDelay[weaponID].iId);
			util.DebugInFile(error_msg);
			conOutput.Notify(error_msg, pBot);

			return RETURN_NOTFIRED;
		}
#endif
	}// END standard weapon mode

	// otherwise if weapon Secondary Mode is active then the bot already has the enemy in sight so now we only need to fire the weapon if the bot is outside min_safe_distance
	else if (pBot->IsWeaponSecondaryModeActive())
	{
#ifdef _DEBUG
		// testing zoom level on weapons with optics
		if (botdebugger.IsDebugWeapons() && (pEdict->v.fov != ZOOM_1X) && IsWeaponWithOptics(pBot->current_weapon.iId, pBot->IsBehaviour(SNIPER)))
		{
			char error_msg[128]{};
			sprintf(error_msg, "<<<BUG>>>FireWeapon() -> weapon with OPTICS NOT ZOOMED <%s> (FoeDistance=%.2f)\n", util.PrintWeaponName(pBot->current_weapon.iId), enemy_distance);
			util.DebugInFile(error_msg);
			conOutput.Notify(error_msg, pBot);
		}
#endif


#ifdef _DEBUG
		// testing - right weapon
		if (in_bot_dev_level1)
			ALERT(at_console, "Secondary fire ID=%d (iClip=%d | Distance=%.2f)\n", pBot->current_weapon.iId, pBot->current_weapon.iClip, enemy_distance);
#endif

		// check if clip is empty AND using weapon with optics
		if (pBot->current_weapon.iClip == 0)
		{
			// the bot has no magazines
			if (pBot->current_weapon.iAmmo1 == 0)
			{
				pBot->SetWeaponIsOutOfAmmo("FireWeapon()|WeapSecondaryModeActive -> weapon has no mags");

				return RETURN_NOAMMO;
			}
			// otherwise the bot has at least one magazine
			else
			{
				// first try to fast switch to backup weapon
				if (IsWeaponWithOptics(pBot->current_weapon.iId, pBot->IsBehaviour(SNIPER)) && CanUseBackupInsteadofReload(pBot, enemy_distance))
				{
					return RETURN_NOTFIRED;
				}

				pBot->ReloadWeapon("FireWeapon()|WeapSecondaryModeActive -> weapon is empty");

				return RETURN_RELOADING;
			}
		}

		// is it time to set a new sniper time?
		if (pBot->IsNotSnipeTime() && pBot->IsNotAdvancingTowardEnemy())
		{
			if (IsWeaponWithOptics(pBot->current_weapon.iId, pBot->IsBehaviour(SNIPER)))
			{
				if (pBot->IsBehaviour(DEFENDER))
					pBot->SetSnipeTime(RANDOM_FLOAT(5.0f, 13.0f));
				else
					pBot->SetSnipeTime(RANDOM_FLOAT(2.5f, 10.5f));
			}
			// RPG case
			else
				pBot->SetSnipeTime(2.5f);

#ifdef _DEBUG
				// testing - snipe time
				if (in_bot_dev_level1)
					ALERT(at_console, "Snipe time=%.2f\n", pBot->GetSnipeTime());
#endif
		}

		if (pEdict->v.velocity.Length() > 0.0f)
		{
			return RETURN_NOTFIRED;
		}

		// get the minimum safe distance value for this weapon
		min_safe_distance = pSelect[weaponID].min_safe_distance;

		// still in safe distance?
		if (enemy_distance >= min_safe_distance)
		{
			press_trigger = TRUE;

#ifdef _DEBUG
			if (botdebugger.IsDebugWeapons(2))
			{
				conOutput.Notify("FireWeapon() -> In safe distance so going to fire the weapon\n", pBot);
				// testing secondary active
				if (in_bot_dev_level1)
					ALERT(at_console, "SECONADRY_active -- firing weapon (ID=%d)\n", pBot->current_weapon.iId);
			}
#endif
		}
		// otherwise don't fire - risking your life, because enemy is too close
		else
		{
			if (IsRPG(pBot->current_weapon.iId))
			{
				BotSwitchGrenadeLauncherFireMode(pBot, "FireWeapon() -> TOO CLOSE to fire");	// put it off shoulder

				if (botdebugger.IsDebugWeapons(2))
				{
					char msg[128]{};
					sprintf(msg, "FireWeapon() -> TOO CLOSE to fire <%s> (FoeDistance=%.2f)\n", util.PrintWeaponName(pBot->current_weapon.iId), enemy_distance);
					conOutput.Notify(msg, pBot);
				}

				return RETURN_FIRED; // like it was fired
			}

			return RETURN_TOOCLOSE;
		}
	}// END is Weapon Secondary Mode Active

	// everything is done so fire the weapon and set correct fire delay (based on bot skill level)
	if (press_trigger)
	{
		// check if bot can press trigger
		if ((botdebugger.IsDontShoot() == false) && (botdebugger.IsDontShootFirearm() == false))
		{
			pEdict->v.button |= IN_ATTACK;  // press primary attack button
		}

		// is it time to use full auto fire?
		if (pBot->IsTimeToFullAutoFire())
		{
			// then keep the fire button pressed for full auto fire
			pBot->f_shoot_time = gpGlobals->time;
		}
		// otherwise press the fire button every once in awhile for single shots
		else
		{
			// first get correct shoot delays for this weapon (the array is filled with data read from external files in weapons folder)
			base_delay = pDelay[weaponID].primary_base_delay;
			min_delay = pDelay[weaponID].primary_min_delay[pBot->GetBotSkill()];
			max_delay = pDelay[weaponID].primary_max_delay[pBot->GetBotSkill()];

			// and set the time of the next shot
			pBot->f_shoot_time = gpGlobals->time + base_delay + RANDOM_FLOAT(min_delay, max_delay);
		}

		// in order to reload the weapon after battle if bot ended it with almost empty clip
		pBot->SetSubTask(ST_W_CLIP);

		if (botdebugger.IsDebugWeapons(1))
		{
			char msg[128]{};
			sprintf(msg, "FireWeapon() -> FIRING IT NOW (next weapon action will be allowed at %.2f)\n", pBot->f_shoot_time);
			conOutput.Notify(msg, pBot);
		}

		return RETURN_FIRED;
	}

	return RETURN_NOTFIRED; // something went wrong
}

/*
* handles knife or spade attack actions (assuming the bot is within effective range)
*/
int BotUseKnife(bot_t* pBot, float enemy_distance)
{
	bot_weapon_select_t *pSelect = NULL;
	bot_fire_delay_t *pDelay = NULL;
	edict_t* pEdict = NULL;

	if (pBot->weapon_action != W_READY)
	{
		return RETURN_NOTFIRED;
	}

	if (pBot->current_weapon.iId != pBot->melee_weapon)
	{
		pBot->weapon_action = W_TAKEOTHER;

		if (botdebugger.IsDebugWeapons())
			conOutput.Notify("UseKnife() -> going to change current weapon\n", pBot);

		return RETURN_TAKING;
	}

	pSelect = &bot_weapon_select[0];
	pDelay = &bot_fire_delay[0];
	pEdict = pBot->pEdict;

#ifdef _DEBUG
	// testing - is it knife?
	if (in_bot_dev_level1)
		ALERT(at_console, "Weapon id=%d Array id=%d\n", pBot->current_weapon.iId, pSelect[pBot->current_weapon.iId].iId);
#endif

	if (enemy_distance <= pSelect[pBot->current_weapon.iId].max_effective_distance)
	{
		int skill = pBot->GetBotSkill();
		float base_delay, min_delay, max_delay;

		if (botdebugger.IsDontShoot() == false)
			pEdict->v.button |= IN_ATTACK;  // press primary attack button

		base_delay = pDelay[pBot->current_weapon.iId].primary_base_delay;
		min_delay = pDelay[pBot->current_weapon.iId].primary_min_delay[skill];
		max_delay = pDelay[pBot->current_weapon.iId].primary_max_delay[skill];

#ifdef _DEBUG
		// testing - knife attack and distance
		if (in_bot_dev_level1)
			ALERT(at_console, "Primary attack with knife (distance=%.2f)\n", enemy_distance);
#endif

		pBot->f_shoot_time = gpGlobals->time + base_delay + RANDOM_FLOAT(min_delay, max_delay);

		if (botdebugger.IsDebugWeapons(1))
		{
			char msg[128]{};
			sprintf(msg, "UseKnife() -> USING IT NOW (next weapon action will be allowed at %.2f)\n", pBot->f_shoot_time);
			conOutput.Notify(msg, pBot);
		}

		return RETURN_FIRED;
	}

	else if (enemy_distance > pSelect[pBot->current_weapon.iId].max_effective_distance)
	{
		if (botdebugger.IsDebugWeapons(2))
		{
			char msg[128]{};
			sprintf(msg, "UseKnife() -> TOO FAR to use it (FoeDistance=%.2f)\n", enemy_distance);
			conOutput.Notify(msg, pBot);
		}

		return RETURN_TOOFAR;
	}

	return RETURN_NOTFIRED;
}

/*
* handles throw grenade actions (assuming the bot is within safe and effective ranges)
* also deals with safety pin removal in newer Firearms versions
*/
int BotThrowGrenade(bot_t* pBot, float enemy_distance)
{
	bot_weapon_select_t *pSelect = NULL;
	bot_fire_delay_t *pDelay = NULL;
	edict_t *pEdict = NULL;
	char msg[TEXT_MSG_SIZE]{};

	if (pBot->IsWeaponReady() == false)
	{
		return RETURN_TAKING;
	}

	// is grenade slot set to no value?
	if (pBot->grenade_slot == NO_VAL)
	{
		// grenades weren't spawned or picked up on the map so we must set grenades depleted flag to prevent running this function
		pBot->SetGrenadesDepleted();

#ifdef _DEBUG
		sprintf(msg, "BotCombat|Bot Throw Grenade()|Invalid run of this function -> gr_slot==-1 | bot class %d\n", pBot->GetBotClass());
		util.DebugDev(msg, -100, -100);
#endif

		return RETURN_NOTFIRED;
	}

	// are grenades really depleted or did the bot use one just a few seconds ago?
	if (pBot->IsGrenadesDepleted())
	{
#ifdef _DEBUG
		if (botdebugger.IsDebugWeapons())
			conOutput.Notify("ThrowGrenade() -> depleted or used one recently\n", pBot);
#endif
		return RETURN_NOTFIRED;
	}

	if (pBot->current_weapon.iId != pBot->grenade_slot)
	{
		if (pBot->bot_weapons & (1<<pBot->grenade_slot))
		{
			if (botdebugger.IsDebugWeapons())
				conOutput.Notify("ThrowGrenade() -> going to change current weapon\n", pBot);

			pBot->weapon_action = W_TAKEOTHER;
			return RETURN_TAKING;
		}
		// this case should not happen if it does then we have to increase the base time in pDelay for this particular grenade ... that will give the engine more time to finish the entity removal task
		else
		{

#ifdef _DEBUG
			if (botdebugger.IsDebugWeapons())
			{
				sprintf(msg, "<<GRENADE>>ThrowGrenade() -> ALREADY USED THEM ALL (currW is %d)\n", pBot->current_weapon.iId);
				conOutput.Notify(msg, pBot);
			}
#endif
			char error_msg[256]{};

			if (pBot->grenade_slot != NO_VAL)
				sprintf(error_msg, "<<BUG>>ThrowGrenade() called after using the last grenade. Try inceasing the primary_base_delay value in marine_bot\\weapons\\modversion file for weapon %s\n", weapon_name[pBot->grenade_slot]);
			else
				sprintf(error_msg, "<<BUG>>ThrowGrenade() called after using the last grenade. Try inceasing the primary_base_delay value in marine_bot\\weapons\\modversion file for all grenades\n");
			util.DebugInFile(error_msg);

			pBot->SetGrenadesDepleted();
			return RETURN_NOTFIRED;
		}
	}

	// let's initialize them all
	pSelect = &bot_weapon_select[0];
	pDelay = &bot_fire_delay[0];
	pEdict = pBot->pEdict;

	if ((pBot->bot_weapons & (1<<pSelect[pBot->current_weapon.iId].iId)) && (pBot->current_weapon.isActive == 1))
	{
		if ((enemy_distance <= pSelect[pBot->current_weapon.iId].max_effective_distance) && (enemy_distance >= pSelect[pBot->current_weapon.iId].min_safe_distance))
		{
#ifdef _DEBUG
			// testing - amount of grenades
			if (in_bot_dev_level1)
				ALERT(at_console, "<<GRENADE>>This grenade can be thrown now -> ID=%d Array ID=%d (Reserves=%d Dist=%.2f currT=%.3f grenIsAvailable=%d)\n",
					pBot->current_weapon.iId, pSelect[pBot->current_weapon.iId].iId, pBot->current_weapon.iAmmo1, enemy_distance, gpGlobals->time, pBot->IsGrenadesAvailable());
#endif

			// in Firearms 2.8 and newer the grenades are implemented with "remove the safety pin first" so we can't throw them right away so...
			if (pBot->IsWeaponStatus(WS_GRENADEPINPULLED) == false)
			{
#ifdef _DEBUG
				if (botdebugger.IsDebugWeapons(2))
				{
					sprintf(msg, "<<GRENADE>>Preparing the grenade ID=%d (currT=%.3f)\n", pSelect[pBot->current_weapon.iId].iId, gpGlobals->time);
					conOutput.Notify(msg, pBot);
				}
#endif
				// in older Firearms versions all we need to do is to set this bit
				pBot->SetWeaponStatus(WS_GRENADEPINPULLED);
			}

			// the pin has already been removed by now so throw the grenade
			if (pBot->IsWeaponStatus(WS_GRENADEPINPULLED))
			{
				// this will throw the grenade
				if (botdebugger.IsDontShoot() == false)
					pEdict->v.button |= IN_ATTACK;

				// set shoot time to prevent firing weapon immediatelly after the bot threw the grenade
				float base = pDelay[pBot->current_weapon.iId].primary_base_delay;
				float min = pDelay[pBot->current_weapon.iId].primary_min_delay[pBot->GetBotSkill()];
				float max = pDelay[pBot->current_weapon.iId].primary_max_delay[pBot->GetBotSkill()];

				pBot->f_shoot_time = gpGlobals->time + base + RANDOM_FLOAT(min, max);

				// most of the time we'll make the bot throw only one grenade at a time by setting the grenades are depleted flag which will make him change to different weapon and
				// then a subsequent check within the check for ammo will unset the flag if bot is using a class equipped with multiple grenades
				if (RANDOM_LONG(1, 100) <= 95)
					pBot->SetGrenadesDepleted();

				// and set some grenade use time else the bot would NOT throw the grenade i.e. it's just to prevent him switching to another weapon during this game frame
				pBot->SetGrenadeUseTime(0.2f);

				if (botdebugger.IsDebugWeapons(1))
				{
					sprintf(msg, "ThrowGrenade() -> THROWING IT NOW (next weapon action will be allowed at %.2f)\n", pBot->f_shoot_time);
					conOutput.Notify(msg, pBot);
				}

#ifdef _DEBUG
				// testing - should really throw it now
				if (in_bot_dev_level1)
					ALERT(at_console, "<<GRENADE>>********THROWING NOW >> ID=%d Array ID=%d (Reserves=%d Dist=%.2f gren_T=%.2f) | shootT=%.2f currT=%.3f (deltaT=%.2f)\n",
						pBot->current_weapon.iId, pSelect[pBot->current_weapon.iId].iId,
						pBot->current_weapon.iAmmo1, enemy_distance, pBot->GetGrenadeUseTime(), pBot->f_shoot_time, gpGlobals->time, pBot->f_shoot_time - gpGlobals->time);
#endif

				return RETURN_FIRED;
			}
		}

		else if (enemy_distance < pSelect[pBot->current_weapon.iId].min_safe_distance)
		{
			if (botdebugger.IsDebugWeapons(1))
			{
				sprintf(msg, "ThrowGrenade() -> TOO CLOSE to throw it (FoeDistance=%.2f | SafetyPinPulled? %d)\n", enemy_distance, pBot->IsWeaponStatus(WS_GRENADEPINPULLED));
				conOutput.Notify(msg, pBot);
			}

			return RETURN_TOOCLOSE;
		}
		
		else if (enemy_distance > pSelect[pBot->current_weapon.iId].max_effective_distance)
		{
			if (botdebugger.IsDebugWeapons(1))
			{
				sprintf(msg, "ThrowGrenade() -> TOO FAR to throw it (FoeDistance=%.2f | SafetyPinPulled? %d)\n", enemy_distance, pBot->IsWeaponStatus(WS_GRENADEPINPULLED));
				conOutput.Notify(msg, pBot);
			}

			return RETURN_TOOFAR;
		}
	}// END is bot carrying this grenade

#ifdef _DEBUG

	if (pSelect[pBot->current_weapon.iId].iId != pDelay[pBot->current_weapon.iId].iId)
	{
		char error_msg[256]{};
		sprintf(error_msg, "<<BUG>>ThrowGrenade() -> Weapon order in pSelect(weapon ID=%d) is NOT same as in pDelay(weapon ID=%d)\n", pSelect[pBot->current_weapon.iId].iId, pDelay[pBot->current_weapon.iId].iId);
		util.DebugInFile(error_msg);
		conOutput.Notify(error_msg, pBot);
	}

	//@@@@@@@@@@@@@@@
	if (botdebugger.IsDebugWeapons())
		conOutput.Notify("ThrowGrenade() -> returning NOTFIRED (end of the function <- WHY THIS HAPPENED?)\n", pBot);

#endif

	return RETURN_NOTFIRED;
}

/*
* this function is meant to be called after the Bot Throw Grenade one, because it checks the result of that function and calls needed actions in order to allow bot handle the grenade action successfully
*/
void PostThrowGrenade(bot_t* pBot, int result, const char* loc)
{
	// are we removing the safety pin?
	if (result == RETURN_PRIMING)
	{

#ifdef _DEBUG
		// testing
		if (botdebugger.IsDebugWeapons() || in_bot_dev_level1)
		{
			char dm[TEXT_MSG_SIZE]{};
			sprintf(dm, "Post ThrowGrenade() @ %s -> ThrowGrenade() returned PRIMING (safety pin removal)\n", loc);
			conOutput.Notify(dm, pBot);
		}
#endif

		// use 75% of the value listed in the external file as the priming time (ie. to remove the safety pin)
		float priming_time = GetWeaponPrimaryBaseDelay(pBot->grenade_slot) * 0.75f;

		// then give the engine some time to finish all things before calling the function again
		pBot->f_shoot_time = gpGlobals->time + priming_time;

		// to prevent switching back to firearms or knife
		pBot->SetGrenadeUseTime(priming_time + 1.0f);
	}
	else if ((result == RETURN_TOOCLOSE) || (result == RETURN_TOOFAR) || ((result == RETURN_NOTFIRED) && pBot->IsGrenadesDepleted()))
	{
		// reset grenade use time to allow switching to usable weapon
		pBot->SetGrenadeUseTime(0.0f);
	}

	// did the bot ran out of grenade use time?
	if (pBot->IsGrenadeUseTime() == false)
	{
		// then get back to the best weapon the bot can use
#ifdef DEBUG
		char extloc[128];
		sprintf(extloc, "Post ThrowGrenade() @ %s -> Grenade time is over", loc);
		pBot->DecideNextWeapon(extloc);
#else
		pBot->DecideNextWeapon();
#endif // DEBUG
	}
}

/*
* checks if the bot can switch to backup weapon instead of trying to reload in the middle of battle
*/
bool CanUseBackupInsteadofReload(bot_t *pBot, float enemy_distance)
{
	// see if the bot doesn't use bipod at the moment and if he has usable backup weapon and the enemy must be in range of backup weapon
	if (pBot->IsUsedWeaponMain() && (pBot->IsTask(TASK_BIPOD) == false) && (pBot->IsNoAmmoForBackupWeapon() == false) &&
		(enemy_distance <= pBot->GetWeaponEffectiveRange(pBot->backup_weapon)))
	{
		// switch to backup weapon
		pBot->UseWeapon(uWeapon::backup);

		if (botdebugger.IsDebugWeapons())
		{
			char fswmsg[128]{};
#ifdef DEBUG
			sprintf(fswmsg, "Going to switch to sidearm instead of reloading <%s> (inClip=%d) (backupW=%s)\n",
				util.StripWeaponName(weapon_name[pBot->current_weapon.iId]), pBot->current_weapon.iClip, util.StripWeaponName(weapon_name[pBot->backup_weapon]));
#else
			sprintf(fswmsg, "Going to switch to sidearm instead of reloading <%s> (backup=<%s>)\n",
				util.StripWeaponName(weapon_name[pBot->current_weapon.iId]), util.StripWeaponName(weapon_name[pBot->backup_weapon]));
#endif // DEBUG

			conOutput.Notify(fswmsg, pBot);
		}

		return true;
	}

	return false;
}

/*
* sets correct advance time based on behaviour, botskill and used weapon
*/
void IsChanceToAdvance(bot_t *pBot)
{
	float since_last_advance, min_delay, max_delay;
	int chance;

	// generate basic chance
	if (internals.IsMeleeOnlyMode())
		chance = RANDOM_LONG(1, 50);	// by using smaller range for the melee only game mode the chance to advance towards the enemy is much higher in such case
	else
		chance = RANDOM_LONG(1, 100);

	int skill_modifier = 0;

	// get bot skill level
	int skill = pBot->GetBotSkill();

	if (pBot->current_weapon.iId == pBot->melee_weapon)
	{
		// time delay since last advance is based on botskill where better bots don't move forward that much, but
		// if the bot is using knife then the logic is inverted, which means better bots do advance often
		since_last_advance = g_combat_advance_delay[BOT_SKILL_LEVELS - 1 - skill];

		// is the melee only game mode activated? then reduce the delay to half in order to make the bots advance forward more often
		if (internals.IsMeleeOnlyMode())
			since_last_advance /= 2.0f;

		// the chance to advance follows the same logic as the time delay so if the bot is using knife
		// then the best bots have highest chance to move towards the enemy
		skill_modifier = (BOT_SKILL_LEVELS - skill) * 5;
	}
	else
	{
		since_last_advance = g_combat_advance_delay[skill];
		skill_modifier = (skill + 1) * 5;
	}

	// is bot attacker AND did NOT advanced in last few seconds
	// (best bot advances towards enemy in 50% of the time and worst in 70% of the time, unless it's the knife case where it is vice versa)
	if (pBot->IsBehaviour(ATTACKER) && (chance < (45 + skill_modifier)) && pBot->NotAdvancedTowardEnemyFor(since_last_advance))
	{
		// NOTE: perhaps these delays could be arrays based on botskill value
		min_delay = 4.0f;
		max_delay = 7.0f;

		// is the melee only game mode activated? then make the bot advance time longer (ie. to make him get close to enemy sooner)
		if (internals.IsMeleeOnlyMode())
		{
			min_delay *= 2.5f;
			max_delay *= 2.0f;
		}

		pBot->SetAdvanceTowardEnemyTime( RANDOM_FLOAT(min_delay, max_delay) );

		// is bot using a knife/melee weapon AND is the right chance?
		if ((pBot->current_weapon.iId == pBot->melee_weapon) && (RANDOM_LONG(1, 100) < 95))
		{
			// then make him run towards the enemy in standing
			pBot->SetStance(GOTO_STANDING, true, "ChanceToAdvance()|Knife -> forced GOTO standing");
		}
		// is bot NOT lying prone AND NOT going to do so?
		else if ((pBot->IsBehaviour(BOT_PRONED) == false) && (pBot->IsBehaviour(GOTO_PRONE) == false))
		{
			// then try to advance in crouch
			if (RANDOM_LONG(1, 100) < 10)
				pBot->SetStance(GOTO_CROUCH, true, "ChanceToAdvance() -> forced GOTO crouch");
			else
				pBot->SetStance(GOTO_STANDING, true, "ChanceToAdvance() -> forced GOTO standing");
		}
	}
	// or is bot defender (best bot advances towards enemy in 10% of the time and worst in 30% of the time)
	else if (pBot->IsBehaviour(DEFENDER) && (chance < (5 + skill_modifier)) && pBot->NotAdvancedTowardEnemyFor(2.0f * since_last_advance))
	{
		min_delay = 2.0f;
		max_delay = 5.0f;

		if (internals.IsMeleeOnlyMode())
		{
			min_delay *= 2.5f;
			max_delay *= 2.0f;
		}
		
		pBot->SetAdvanceTowardEnemyTime( RANDOM_FLOAT(min_delay, max_delay) );

		if ((pBot->current_weapon.iId == pBot->melee_weapon) && (RANDOM_LONG(1, 100) < 55))
		{
			pBot->SetStance(GOTO_STANDING, true, "ChanceToAdvance()|Knife -> forced GOTO standing");
		}
		else if ((pBot->IsBehaviour(BOT_PRONED) == false) && (pBot->IsBehaviour(GOTO_PRONE) == false))
		{
			if (RANDOM_LONG(1, 100) < 25)
				pBot->SetStance(GOTO_CROUCH, true, "ChanceToAdvance() -> forced GOTO crouch");
			else
				pBot->SetStance(GOTO_STANDING, true, "ChanceToAdvance() -> forced GOTO standing");
		}
	}
	// standard behaviour type (best bot advances towards enemy in 25% of the time and worst in 45% of the time)
	else if (pBot->IsBehaviour(STANDARD) && (chance < (20 + skill_modifier)) && pBot->NotAdvancedTowardEnemyFor(1.5f * since_last_advance))
	{
		min_delay = 3.0f;
		max_delay = 6.0f;

		if (internals.IsMeleeOnlyMode())
		{
			min_delay *= 2.5f;
			max_delay *= 2.0f;
		}

		pBot->SetAdvanceTowardEnemyTime( RANDOM_FLOAT(min_delay, max_delay) );

		if ((pBot->current_weapon.iId == pBot->melee_weapon) && (RANDOM_LONG(1, 100) < 75))
		{
			pBot->SetStance(GOTO_STANDING, true, "ChanceToAdvance()|Knife -> forced GOTO standing");
		}
		else if ((pBot->IsBehaviour(BOT_PRONED) == false) && (pBot->IsBehaviour(GOTO_PRONE) == false))
		{
			if (RANDOM_LONG(1, 100) < 20)
				pBot->SetStance(GOTO_CROUCH, true, "ChanceToAdvance() -> forced GOTO crouch");
			else
				pBot->SetStance(GOTO_STANDING, true, "ChanceToAdvance() -> forced GOTO standing");
		}
	}

	// leave the advance time untouched ie bot will stay in last Stance
	return;
}

/*
* do traceline in all three positions (lying prone, in crouch & standing) to set the best one
* so that the bot still see & can shoot at enemy while having the best possible cover
*/
inline void CheckStance(bot_t *pBot, float enemy_distance)
{
	edict_t *pEdict = pBot->pEdict;
	edict_t *pEnemy = pBot->pBotEnemy;
	Vector v_botshead, v_enemy;
	TraceResult tr;

	// look through bots eyes
	v_botshead = pEdict->v.origin + pEdict->v.view_ofs;
	// at enemy head
	v_enemy = pEnemy->v.origin + pEnemy->v.view_ofs;

	
	// is bot in special standing bipod spot?
	if (IsInStandingBipodSpot(pEdict))
	{
		// then keep standing stance so that the engine allows deploying bipod
		pBot->SetStance(GOTO_STANDING, "Check Stance()|Special BIPOD SPOT -> GOTO standing");
		return;
	}

	// is enemy close enough?
	if (enemy_distance <= pBot->GetWeaponEffectiveRange(pBot->backup_weapon))
	{
		// not already lying prone AND NOT a machinegunner?
		if ((pBot->IsBehaviour(BOT_PRONED) == false) && (IsMachinegun(pBot->main_weapon) == false))
		{
			// then don't go prone
			pBot->SetBehaviour(BOT_DONTGOPRONE);



			//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@			NEW CODE 094 (remove it)
#ifdef DEBUG
			if (botdebugger.IsDebugStance())
			{
				char dp[128]{};
				sprintf(dp, "Check Stance()|enemydist(%.1f) < EffectiveRngOfBackupWeap(%.1f) && NOT proned -> SET DONTGOPRONE behaviour\n",
					enemy_distance, pBot->GetWeaponEffectiveRange(pBot->backup_weapon));
				conOutput.Notify(dp, pBot);
			}
#endif // DEBUG


		}

		// is bot using knife then get up from prone if within melee distance to enemy
		if ((pBot->IsUsedWeaponKnife() || (pBot->current_weapon.iId == pBot->melee_weapon)) && (enemy_distance < RANGE_MELEE))
		{
			pBot->SetStance(GOTO_STANDING, true, "Check Stance()|UseKNIFE -> forced GOTO standing when foe < RangeMelee (ie 300)");

			return;
		}
			
	}

	// don't change Stance if enemy is still in the same distance, do it only in 20% of the time (machine gunners do skip this statement)
	if ((pBot->GetPrevDistanceToEnemy() == enemy_distance) && (IsMachinegun(pBot->current_weapon.iId) == false) && (RANDOM_LONG(1, 100) < 80))
		return;

	/*
	UTIL_TraceLine(v_botshead, v_enemy, dont_ignore_monsters, ignore_glass,	pEdict->v.pContainingEntity, &tr);

		//@@@@@vecEndPos
		ALERT(at_console, "duck try (%.2f) (hitgr %d) (flPlaneDist %.2f)\n", tr.flFraction, tr.iHitgroup, tr.flPlaneDist);
		ALERT(at_console, "duck try hitent (class %s) (net %s) (glob %s)\n", STRING(tr.pHit->v.classname), STRING(tr.pHit->v.netname), STRING(tr.pHit->v.globalname));
		ALERT(at_console, "duck try hitent(edict %x)\n", tr.pHit);
		ALERT(at_console, "(trEnd: x%.1f y%.1f z%.1f) (v_enemy: x%.1f y%.1f z%.1f) (pEnemy: x%.1f y%.1f z%.1f)\n",
			tr.vecEndPos.x, tr.vecEndPos.y, tr.vecEndPos.z, v_enemy.x, v_enemy.y, v_enemy.z, pEnemy->v.origin.x, pEnemy->v.origin.y, pEnemy->v.origin.z);
	*/


	// is bot lying prone?
	if (pBot->IsBehaviour(BOT_PRONED))
	{
		// did it hit something ie. enemy isn't visible now?
		if (util.IsPlayerVisible(v_enemy, pEdict) == VIS_NO)
		{
			// to try the crouched position we have to fake bot's head position
			v_botshead = pEdict->v.origin + pEdict->v.view_ofs + Vector(0.0f, 0.0f, 8.0f);

			// would enemy be visible if bot went crouch?
			if (util.IsPlayerVisible(v_enemy, v_botshead, pEdict) == VIS_YES)
			{
				pBot->SetStance(GOTO_CROUCH, "Check Stance()|PRONE -> foe visible in crouch");
				return;
			}
			else
			{
				// try visibility in standing
				v_botshead  = pEdict->v.origin + pEdict->v.view_ofs + Vector(0.0f, 0.0f, 42.0f);

				// and see whether the enemy can be seen that way
				if (util.IsPlayerVisible(v_enemy, v_botshead, pEdict) == VIS_YES)
				{
					pBot->SetStance(GOTO_STANDING, "Check Stance()|PRONE -> foe visible in standing");
					return;
				}


				// NOTE: here should be some code that tests strafe to side
			}
		}

		return;
	}

	// is bot in crouch?
	else if (pBot->IsBehaviour(BOT_CROUCHED))
	{
		// machine gunners should try going prone whenever it is possible, other bots can decide at will
		if ((pBot->IsSubTask(ST_CANTPRONE) == false) && (pBot->IsBehaviour(BOT_DONTGOPRONE) == false) && (IsMachinegun(pBot->current_weapon.iId) || (RANDOM_LONG(1, 100) < 15)))
		{
			v_botshead = pEdict->v.origin + pEdict->v.view_ofs - Vector(0.0f, 0.0f, 8.0f);

			if (util.IsPlayerVisible(v_enemy, v_botshead, pEdict) == VIS_YES)
			{
				pBot->SetStance(GOTO_PRONE, "Check Stance()|CROUCHED -> foe visible in prone");
				return;
			}
		}
		// or try standing position from time to time
		else if (RANDOM_LONG(1, 100) < 20)
		{
			v_botshead = pEdict->v.origin + pEdict->v.view_ofs + Vector(0.0f, 0.0f, 34.0f);

			// change to standing position
			if (util.IsPlayerVisible(v_enemy, v_botshead, pEdict) == VIS_YES)
			{
				pBot->SetStance(GOTO_STANDING, "Check Stance()|CROUCHED -> foe visible in standing");
				return;
			}
		}

		// is enemy actually visible?
		if (util.IsPlayerVisible(v_enemy, pEdict) == VIS_NO)
		{
			v_botshead = pEdict->v.origin + pEdict->v.view_ofs + Vector(0.0f, 0.0f, 34.0f);

			// stand up if the enemy can be seen that way
			if (util.IsPlayerVisible(v_enemy, v_botshead, pEdict) == VIS_YES)
			{
				pBot->SetStance(GOTO_STANDING, "Check Stance()|CROUCHED -> foe visible in standing");
				return;
			}


			// NOTE: here should be some code that tests strafe to side
		}

		return;
	}

	else if (pBot->IsBehaviour(BOT_STANDING))
	{
		v_botshead = pEdict->v.origin + pEdict->v.view_ofs - Vector(0.0f, 0.0f, 42.0f);

		if ((pBot->IsSubTask(ST_CANTPRONE) == false) && (pBot->IsBehaviour(BOT_DONTGOPRONE) == false) && (util.IsPlayerVisible(v_enemy, v_botshead, pEdict) == VIS_YES))
		{
			pBot->SetStance(GOTO_PRONE, "Check Stance()|STANDING -> foe visible in prone");
			return;
		}

		v_botshead = pEdict->v.origin + pEdict->v.view_ofs - Vector(0.0f, 0.0f, 34.0f);

		if (util.IsPlayerVisible(v_enemy, v_botshead, pEdict) == VIS_YES)
		{
			pBot->SetStance(GOTO_CROUCH, "Check Stance()|STANDING -> foe visible in crouch");
			return;
		}

		return;
	}
	else
	{
#ifdef DEBUG
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@														// NEW CODE 094 (remove it)
		if (botdebugger.IsDebugStance())
		{
			conOutput.Notify("Check Stance() -> must be changing stance right now (go/resume prone) so skipping it\n", pBot);
		}
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#endif // DEBUG
	}
}

/*
* returns TRUE if enemy is close enough based on current weapon
* returns FALSE if enemy is still quite far (ie bot can still move towards it)
*/
bool IsEnemyCloseEnough(bot_t *pBot, float enemy_distance)
{
	// sniper rifles
	if (IsWeaponWithOptics(pBot->current_weapon.iId, pBot->IsBehaviour(SNIPER)))
	{
		if (enemy_distance <= pBot->GetWeaponEffectiveRange())
			return true;
	}
	// machineguns OR a gunner class where bot has ammo for his machine gun - gunner bots move around equipped with the pistol most of the time so we must check for it this way
	else if (IsMachinegun(pBot->current_weapon.iId) || (IsMachinegun(pBot->main_weapon) && (pBot->IsNoAmmoForMainWeapon() == false)))
	{
		if (enemy_distance <= pBot->GetWeaponEffectiveRange())
			return true;
	}
	// no firearm
	else if (pBot->current_weapon.iId == pBot->melee_weapon)
	{
		if (enemy_distance < 35.0f)
			return true;
	}
	// grenades
	else if (IsGrenade(pBot->current_weapon.iId))
	{
		if (enemy_distance <= pBot->GetWeaponEffectiveRange())
			return true;
	}
	/*/																						UNSURE ABOUT IT
	// rocket launchers
	else if (IsRPG(pBot->current_weapon.iId) || (IsRPG(pBot->main_weapon) && (pBot->IsNoAmmoForMainWeapon() == false)))
	{
		if (enemy_distance <= pBot->GetWeaponEffectiveRange())
			return true;
	}
	/**/

	// all other weapons
	else
	{
		// bot is either in melee range OR within his weapon range with exception of small skill based chance when he decided to get even closer to his enemy ...
		// best bots will move even closer in 5% of the time, ie. 5 + (0 * 5), while worst bots will do it in 25% of the time, ie. 5 + (4 * 5) 
		if ((enemy_distance < RANGE_MELEE) || ((RANDOM_LONG(1, 100) > (5 + (pBot->GetBotSkill() * 5))) && (enemy_distance <= pBot->GetWeaponEffectiveRange())))
		{
			return true;
		}
	}

	return false;
}


/*
* returns TRUE if the weapon has no min safe range or if the enemy is beyond this safe range 
*/
bool IsOutOfMinimumSafeDistanceForWeapon(bot_t* pBot, int weapon_index, float enemy_distance)
{
	// this weapon has no minimum safe distance so everything is okay
	if (pBot->GetWeaponSafeRange(weapon_index) == 0.0f)
		return true;

	// the enemy is farther than the minimum safe range of this weapon so everything is okay
	if (enemy_distance >= pBot->GetWeaponSafeRange(weapon_index))
		return true;

	return false;
}


/*
* determines whether and for how long will the bot use full auto fire when using any of assault rifles or SMGs
* switching these weapons to full auto fire is decided elsewhere
*/
inline void BotDecideFullAutoFire(bot_t* pBot)
{
	bool can_use = FALSE;

	// full auto fire usage is based on bot skill and it is a combination of the time of the last use and randomly generated chance
	switch (pBot->GetBotSkill())
	{
	case 0:
		// is it quite some time since the last use of fully automatic fire? then do it in 50/50 chance
		if (pBot->NotUsedFullAutoFireFor(0.75f) && (RANDOM_LONG(1, 100) > 50))
			can_use = TRUE;
		break;

	case 1:
		if (pBot->NotUsedFullAutoFireFor(0.65f) && (RANDOM_LONG(1, 100) > 45))
			can_use = TRUE;
		break;

	case 2:
		if (pBot->NotUsedFullAutoFireFor(0.5f) && (RANDOM_LONG(1, 100) > 40))
			can_use = TRUE;
		break;

	case 3:
		if (pBot->NotUsedFullAutoFireFor(0.4f) && (RANDOM_LONG(1, 100) > 35))
			can_use = TRUE;
		break;

	case 4:
		// is it not that long since last full auto fire? then use it in 70% of the time
		if (pBot->NotUsedFullAutoFireFor(0.25f) && (RANDOM_LONG(1, 100) > 30))
			can_use = TRUE;
		break;
	}

	// can bot use full auto fire?
	if (can_use)
		// then set its use time based on bot skill level
		pBot->SetFullAutoFireTime( RANDOM_FLOAT(g_fullautofire_min_delay[pBot->GetBotSkill()], g_fullautofire_max_delay[pBot->GetBotSkill()]) );
}


/*
* determines whether and for how long will the bot use full auto fire when using a machine gun
*/
inline void BotDecideFullAutoFireOnMachinegun(bot_t* pBot, float enemy_distance)
{
	bool can_use = FALSE;

	// full auto fire usage is based on bot skill and it is a combination of enemy distance, time of the last use and randomly generated chance
	switch (pBot->GetBotSkill())
	{
		case 0:
			// enemy is close so use full auto quite often
			if ((enemy_distance < 450.0f) && (RANDOM_LONG(1, 100) > 25))
				can_use = TRUE;
			// enemy is farther, didn't use it in last couple seconds so use full auto fire in 50% of the time
			else if ((enemy_distance < 900.0f) && pBot->NotUsedFullAutoFireFor(0.75f) && (RANDOM_LONG(1, 100) > 50))
				can_use = TRUE;
			// enemy is far, it's quite some time since last usage and pretty slim chance
			else if ((enemy_distance > 900.0f) && pBot->NotUsedFullAutoFireFor(1.5f) && (RANDOM_LONG(1, 100) > 95))
				can_use = TRUE;
			break;
		
		case 1:
			if ((enemy_distance < 450.0f) && (RANDOM_LONG(1, 100) > 20))
				can_use = TRUE;
			else if ((enemy_distance < 900.0f) && pBot->NotUsedFullAutoFireFor(0.65f) && (RANDOM_LONG(1, 100) > 45))
				can_use = TRUE;
			else if ((enemy_distance > 900.0f) && pBot->NotUsedFullAutoFireFor(1.25f) && (RANDOM_LONG(1, 100) > 90))
				can_use = TRUE;
			break;

		case 2:
			if ((enemy_distance < 450.0f) && (RANDOM_LONG(1, 100) > 15))
				can_use = TRUE;
			else if ((enemy_distance < 900.0f) && pBot->NotUsedFullAutoFireFor(0.5f) && (RANDOM_LONG(1, 100) > 40))
				can_use = TRUE;
			else if ((enemy_distance > 900.0f) && pBot->NotUsedFullAutoFireFor(1.0f) && (RANDOM_LONG(1, 100) > 85))
				can_use = TRUE;
			break;

		case 3:
			if ((enemy_distance < 450.0f) && (RANDOM_LONG(1, 100) > 10))
				can_use = TRUE;
			else if ((enemy_distance < 900.0f) && pBot->NotUsedFullAutoFireFor(0.4f) && (RANDOM_LONG(1, 100) > 35))
				can_use = TRUE;
			else if ((enemy_distance > 900.0f) && pBot->NotUsedFullAutoFireFor(0.75f) && (RANDOM_LONG(1, 100) > 75))
				can_use = TRUE;
			break;

		case 4:
			// enemy is close so use full auto fire very often
			if ((enemy_distance < 450.0f) && (RANDOM_LONG(1, 100) > 5))
				can_use = TRUE;
			else if ((enemy_distance < 900.0f) && pBot->NotUsedFullAutoFireFor(0.25f) && (RANDOM_LONG(1, 100) > 30))
				can_use = TRUE;
			// even if enemy is far and it's not so long since last use, use full auto quite often
			else if ((enemy_distance > 900.0f) && pBot->NotUsedFullAutoFireFor(0.5f) && (RANDOM_LONG(1, 100) > 65))
				can_use = TRUE;
			break;
	}

	if (can_use)
		pBot->SetFullAutoFireTime( RANDOM_FLOAT(g_fullautofire_min_delay_mg[pBot->GetBotSkill()], g_fullautofire_max_delay_mg[pBot->GetBotSkill()]) );
}


/*
* directs all in combat actions and behaviour
*/
void BotShootAtEnemy( bot_t *pBot )
{
	edict_t* pEdict = pBot->pEdict;
	Vector v_enemy;
	float foe_distance;
	bool out_of_bipod_limit = FALSE;

	// is bot waiting for his current enemy to become visible? (ie. bot lost clear view to this enemy and can only watch to that direction now)
	if (pBot->IsNotWaitingForEnemy() == false)
	{
		v_enemy = pBot->GetLastKnownEnemyPosition() - pEdict->v.origin;

		v_enemy.z = 0;  // ignore z component (up & down)

		pBot->SetPrevDistanceToEnemy(v_enemy.Length());

		// don't move if can't see enemy
		pBot->SetMoveSpeed(MoveSpeed::stop);
		pBot->SetDontCheckStuck();

		if (pBot->IsUsedWeaponGrenade() && (pBot->f_shoot_time <= gpGlobals->time) && pBot->IsNotDeployingBipod())
		{
			int result = BotThrowGrenade(pBot, v_enemy.Length2D());

			PostThrowGrenade(pBot, result, "ShootAtEnemy()|WaitForEnemyTime");
		}
		// is chance to stand up to scan the horizon? (don't do it if the weapon isn't ready for action or this bot is a machine gunner)
		else if (pBot->IsWeaponReady() && (IsMachinegun(pBot->current_weapon.iId) == false) && (RANDOM_LONG(1, 100) < 2))
		{
			// standing up is based on behaviour and chance
			if ((pBot->IsBehaviour(ATTACKER) && (RANDOM_LONG(1, 100) < 35)) || (pBot->IsBehaviour(DEFENDER) && (RANDOM_LONG(1, 100) < 5)) || (pBot->IsBehaviour(STANDARD) && (RANDOM_LONG(1, 100) < 20)))
			{
				if (pBot->IsBehaviour(BOT_PRONED))
				{
					pBot->GoProne("ShootAtEnemy()|WaitForEnemyTime -> StandUp to Scan Horizion");
					pBot->SetStance(GOTO_STANDING, "ShootAtEnemy()|WaitForEnemyTime -> GOTO standing to Scan Horizon");
				}
				else if (pBot->IsBehaviour(BOT_CROUCHED))
					pBot->SetStance(GOTO_STANDING, "ShootAtEnemy()|WaitForEnemyTime -> GOTO standing to Scan Horizon");
			}
		}

		// we can't shoot at this enemy, because we can't see him
		return;
	}

	// see if the bot is under water AND has NOT switched to knife yet
	if ((pEdict->v.waterlevel == 3) && (pBot->IsUsedWeaponKnife() == false))
	{
		pBot->UseWeapon(uWeapon::knife);
	}

	// are we already aiming at the enemy?
	UTIL_MakeVectors(pEdict->v.v_angle);
	TraceResult tr = {};
	UTIL_TraceLine(util.GetGunPosition(pEdict), util.GetGunPosition(pEdict) + gpGlobals->v_forward * 10000.f, dont_ignore_monsters, pEdict, &tr);
	if(tr.pHit != pBot->pBotEnemy) {
		// aim for the head and/or body
		v_enemy = BotBodyTarget(pBot) - util.GetGunPosition(pEdict);

		auto enemy_angles = UTIL_VecToAngles(v_enemy);
		pEdict->v.v_angle.x = enemy_angles.x;
		pEdict->v.ideal_yaw = enemy_angles.y;
	}

	if (pEdict->v.v_angle.y > 180)
		pEdict->v.v_angle.y -=360;

	// is bot using bipod?
	if (pBot->IsTask(TASK_BIPOD))
	{
		// bot can turn normally when using bipod so we must prevent that manually
		// we need to know in which direction (exact yaw angle) bot looked when deployed bipod to count correct limits
		// and when bot exceeds the yaw limit we just put this stored yaw value into pEdict->v.v_angle.y (yaw variable) to make bot look to initial angle again

		// max turn angle the engine allows with bipod
		float bipod_limit = 45.0f;

		// is bot trying to turn more than bipod allows?
		if (fabsf(pBot->GetBipodYawAngle() - pEdict->v.v_angle.y) > bipod_limit)
		{
			// then reset his yaw back to the angle he was looking when deployed the bipod
			pEdict->v.v_angle.y = pBot->GetBipodYawAngle();

			if (pEdict->v.v_angle.y > 180)
				pEdict->v.v_angle.y -= 360;

			// prevents shooting the weapon
			out_of_bipod_limit = TRUE;

			// also try to fold it in order to face the enemy
			BotUseBipod(pBot, false, "ShootAtEnemy()|TaskBipod prevents facing enemy -> FOLD IT");
		}
	}

	// Paulo-La-Frite - START bot aiming bug fix
	if (pEdict->v.v_angle.x > 180)
		pEdict->v.v_angle.x -=360;

	// set the body angles to point the gun correctly
	pEdict->v.angles.x = pEdict->v.v_angle.x / 3;
	pEdict->v.angles.y = pEdict->v.v_angle.y;
	pEdict->v.angles.z = 0;

	// adjust the view angle pitch to aim correctly (MUST be after body v.angles stuff)
	pEdict->v.v_angle.x = -pEdict->v.v_angle.x;
	// Paulo-La-Frite - END

	BotFixIdealYaw(pEdict);

	v_enemy.z = 0;  // ignore z component (up & down)

	foe_distance = v_enemy.Length();  // how far away is the enemy scum?

	// is current enemy quite far AND is it time to check for a closer one AND NOT using sniper rifle?
	if ((foe_distance > 500.0f) && pBot->IsTimeToCheckForCloserEnemy() && (IsWeaponWithOptics(pBot->current_weapon.iId, pBot->IsBehaviour(SNIPER)) == false))
	{
		// then go find a new enemy
		pBot->SetTask(TASK_FIND_ENEMY);
		pBot->SetCheckForCloserEnemyTime( g_time_to_check_for_closer_enemy[pBot->GetBotSkill()] );
	}
	else
		pBot->RemoveTask(TASK_FIND_ENEMY);

	if (pBot->IsInCrampedSpace())
	{
		// will cause that the bot forgets about distant enemy and will continue in navigation
		pBot->SetTask(TASK_AVOID_ENEMY);
	}

	// is the bot paused OR doing medical treatment?
	else if ((pBot->IsNotPaused() == false) || pBot->IsMedicalTreatmentNow())
	{
		// then just don't move and wait
		pBot->SetMoveSpeed(MoveSpeed::stop);
		pBot->SetDontCheckStuck();

		// NOT in cover position? then try to go prone or at least crouch
		if ((pBot->IsBehaviour(BOT_PRONED) == false) && (pBot->IsBehaviour(BOT_CROUCHED) == false) && pBot->IsNotChangingStance())
		{
			// go prone only if paused
			if ((RANDOM_LONG(1, 100) > 50) && (pBot->IsNotPaused() == false))
				pBot->SetStance(GOTO_PRONE, "ShootAtEnemy()|Paused -> GOTO prone");
			else
				pBot->SetStance(GOTO_CROUCH, "ShootAtEnemy()|PausedORHealingTeammate -> GOTO crouch");
		}
	}

	// or is still snipe time?
	else if (pBot->IsNotSnipeTime() == false)
	{
		// if the enemy is far then try to deploy bipod if NOT already using it AND if is able to use it
		if ((foe_distance > 1000.0f) && (pBot->IsTask(TASK_BIPOD) == false) && (pBot->IsWeaponStatus(WS_CANTBIPOD) == false) && CanDeployBipod(pEdict) &&
			IsBipodWeapon(pBot->current_weapon.iId, pEdict->v.playerclass))
			BotUseBipod(pBot, false, "ShootAtEnemy()|SnipingTime -> DeployIt");

		pBot->SetMoveSpeed(MoveSpeed::stop);
		pBot->SetDontCheckStuck();
	}

	// or is bot forced to stay at one place (ie don't move)?
	else if (pBot->IsTask(TASK_DONTMOVEINCOMBAT))
	{
		pBot->SetMoveSpeed(MoveSpeed::stop);
		pBot->SetDontCheckStuck();

		// check if enemy is too far AND bot wasn't in hiding in last 5 seconds
		if ((foe_distance > pBot->GetWeaponEffectiveRange(pBot->current_weapon.iId)) && pBot->BotNotBeenHidingFor(5.0f))
		{
			// NOT in prone AND NOT crouched?
			if ((pBot->IsBehaviour(BOT_PRONED) == false) && (pBot->IsBehaviour(BOT_CROUCHED) == false))
			{
				// change to crouched position
				pBot->SetStance(GOTO_CROUCH, "ShootAtEnemy()|GonnaHide -> GOTO crouch");
			}

			// set time to stay hidden
			pBot->SetBotHideTime( RANDOM_FLOAT(3.0f, 10.0f) );




			// NOTE: Handle SMG's, Handguns etc, bots stop and die when out of range
			// TODO: Check whether we can use the task FIND ENEMY in this case


			//@@@@@@@@@@@@@@@
			//ALERT(at_console, "DontMoveInCombat -> Out of WeapRange (BotHideT=%.2f | Weap=<%s>)\n", pBot->GetBotHideTime(), util.StripWeaponName(pBot->current_weapon.iId));


		}
		// is hide time over but is still some time to next hiding then scan horizon for enemies
		else if (pBot->IsBotNotHiding() && (pBot->GetBotHideTime() + 5.0f > gpGlobals->time))
		{
			// change to standing position
			if ((pBot->IsBehaviour(BOT_STANDING) == false) && (RANDOM_LONG(1, 100) <= 50))
				pBot->SetStance(GOTO_STANDING, "ShootAtEnemy()|inHiding -> GOTO standing to scan horizon");
		}
	}

	// or is still wait time AND NOT avoiding enemy?
	else if (pBot->IsWaitTime() && (pBot->IsTask(TASK_AVOID_ENEMY) == false))
	{
		// the bot is a sniper so hold position
		if (IsWeaponWithOptics(pBot->current_weapon.iId, pBot->IsBehaviour(SNIPER)))
			pBot->SetMoveSpeed(MoveSpeed::stop);
		// is the bot machinegunner?
		else if (IsMachinegun(pBot->current_weapon.iId))
		{
			// the enemy is too far so break the waiting
			if (foe_distance > 1400.0f * rg_modif)
				pBot->SetWaitTime(-0.2f);
			// otherwise hold position
			else
				pBot->SetMoveSpeed(MoveSpeed::stop);
		}
		// otherwise break the waiting
		else
			pBot->SetWaitTime(-0.2f);

		pBot->SetDontCheckStuck();
	}

	// is it time for decision to move toward the enemy?
	// (ie. weapon must be ready, because the chance to advance is based on it AND NOT using bipod AND enemy is still quite far)
	else if (pBot->IsNotAdvancingTowardEnemy() && pBot->IsWeaponReady() && (pBot->IsTask(TASK_BIPOD) == false) && (IsEnemyCloseEnough(pBot, foe_distance) == false))
	{
		// NOT ignoring AND NOT avoiding enemy (i.e. the bot must already be close enough in both cases so no point to move even closer)
		if ((pBot->IsTask(TASK_IGNORE_ENEMY) == false) && (pBot->IsTask(TASK_AVOID_ENEMY) == false))
			IsChanceToAdvance(pBot);

		// this is a special case (knife attack) when we need to allow the movement even if the tests from above return "dont move", but the bot has to trace the forward
		if (pBot->IsTask(TASK_DEATHFALL))
			IsChanceToAdvance(pBot);
	}

	// is it time to trace enemy to set best stance? (ie. weapon must be ready AND bot is NOT hiding AND NOT using a bipod AND NOT reloading AND NOT using grenade)
	else if (pBot->IsTimeToCheckStance() && pBot->IsWeaponReady() && pBot->BotNotBeenHidingFor(5.0f) && (pBot->IsTask(TASK_BIPOD) == false) && pBot->IsNotReloadingWeapon() && (pBot->IsGrenadeUseTime() == false))
	{
		CheckStance(pBot, foe_distance);

		float min_delay = g_time_of_next_combat_stance_check_min[pBot->GetBotSkill()];
		float max_delay = g_time_of_next_combat_stance_check_max[pBot->GetBotSkill()];

		// makes machine gunners check their combat stance more often because they need to go prone and deploy bipod to be effective
		if (IsMachinegun(pBot->current_weapon.iId))
		{
			min_delay = min_delay / 2.0f;
			max_delay = max_delay / 2.0f;
		}

		pBot->SetCheckStanceTime(RANDOM_FLOAT(min_delay, max_delay));
	}

	// is it time to move towards the enemy (moves are based on carried weapon)?
	if (pBot->IsNotAdvancingTowardEnemy() == false)
	{
		// don't move if the bot is already close enough to his enemy
		if (IsEnemyCloseEnough(pBot, foe_distance))
		{
			pBot->SetMoveSpeed(MoveSpeed::stop);
			pBot->SetDontCheckStuck();

			// reset advance time
			pBot->SetAdvanceTowardEnemyTime(0.0f);
		}
		// otherwise move at full speed
		else
			pBot->SetMoveSpeed(MoveSpeed::max);
	}
	// otherwise don't move
	else
	{
		pBot->SetMoveSpeed(MoveSpeed::stop);
		pBot->SetDontCheckStuck();
	}

	// is it time to shoot yet?
	if ((out_of_bipod_limit == false) && (pBot->f_shoot_time <= gpGlobals->time) && pBot->IsNotReloadingWeapon() && pBot->IsBotReactionTimeOver() && pBot->IsNotDeployingBipod())
	{
		int result = NO_VAL;

		// see if we can use a grenade...
		if (pBot->CanUseGrenade(foe_distance))
		{
			if (pBot->UseGrenade("ShootatEnemy()"))
			{
#ifdef _DEBUG
				//@@@@@@@@@@@@@@@
				if (IsWeaponWithOptics(pBot->current_weapon.iId, pBot->IsBehaviour(SNIPER)) && botdebugger.IsDebugWeapons())
				{
					conOutput.Notify("ShootatEnemy() -> Grenade use auto Scope off\n", pBot);
				}
#endif
			}
		}
		// is bot using or about to use the knife?
		else if (pBot->IsUsedWeaponKnife())
		{
			result = BotUseKnife(pBot, v_enemy.Length2D());

			if (result == RETURN_TOOFAR)
			{
				// try to take back main weapon if there's enough ammo for it AND is NOT under water AND bot is beyond minimum safe distance for this weapon (eg. RPG or sniper rifles)
				if ((pBot->IsNoAmmoForMainWeapon() == false) && (pEdict->v.waterlevel != 3) && IsOutOfMinimumSafeDistanceForWeapon(pBot, pBot->main_weapon, foe_distance))
				{
					pBot->UseWeapon(uWeapon::main);
				}
				// main is unavailable so try backup weapon
				else if ((pBot->IsNoAmmoForBackupWeapon() == false) && (pEdict->v.waterlevel != 3) && IsOutOfMinimumSafeDistanceForWeapon(pBot, pBot->backup_weapon, foe_distance))
				{
					pBot->UseWeapon(uWeapon::backup);
				}
				else
				{
					// see if the bot can override combat advance and start moving towards enemy?
					if (pBot->CanOverrideAdvanceTime())
					{
						// best skilled bot does it in 75% of time (worst skilled bot only in 55%)
						if (RANDOM_LONG(1, 100) >= 25 + (5 * pBot->GetBotSkill()))
						{
							pBot->SetAdvanceTowardEnemyTime(0.0f);

							// if the bot is on "limited movement" path then we have to scan the forward direction for danger of deathfall
							if ((pBot->IsTask(TASK_AVOID_ENEMY) || pBot->IsTask(TASK_IGNORE_ENEMY)) && (foe_distance < RANGE_MELEE))
								pBot->SetTask(TASK_DEATHFALL);

#ifdef _DEBUG
							//@@@@@@@@@@@@@@@@
							if (botdebugger.IsDebugStance())
							{
								conOutput.Notify("(KNIFE)Time to advance has been reset to zero->about to start moving towards enemy\n", pBot);
							}
#endif
						}

						// set the time for next try to override the advance toward the enemy time
						pBot->SetOverrideAdvanceTime(RANDOM_FLOAT(1.0f, 3.0f));
					}
				}
			}
		}
		// is bot using or about to use a grenade?
		else if (pBot->IsUsedWeaponGrenade())
		{
			result = BotThrowGrenade(pBot, v_enemy.Length2D());
			
			PostThrowGrenade(pBot, result, "ShootatEnemy()");
		}
		// is bot using main weapon OR backup weapon?
		else if (pBot->IsUsedWeaponMain() || pBot->IsUsedWeaponBackup())
		{
			// is it time to try to use a full auto fire (ie. not already doing so)?
			if (pBot->IsTimeToFullAutoFire() == false)
			{
				// is bot using either support rifle OR submachine gun AND is there enough ammo left in magazine?
				if ((IsLMG(pBot->current_weapon.iId, pBot->pEdict->v.playerclass) || IsSMG(pBot->current_weapon.iId)) && (pBot->current_weapon.iClip > 7))
					BotDecideFullAutoFire(pBot);

				// is bot using machinegun AND is there enough ammo left in magazine?
				else if (IsMachinegun(pBot->current_weapon.iId) && (pBot->current_weapon.iClip > 15))
					BotDecideFullAutoFireOnMachinegun(pBot, foe_distance);
			}

			result = BotFireWeapon(pBot, v_enemy.Length2D());

			if (result == RETURN_RELOADING)
			{
				pBot->SetFullAutoFireTime(0.0f);	// clear full auto fire time
			}
			else if (result == RETURN_NOAMMO)
			{
				pBot->DecideNextWeapon("ShootAtEnemy() -> FireWeapon() returned NO AMMO");
			}
			else if (result == RETURN_TOOCLOSE)
			{
				pBot->DecideNextWeapon("ShootAtEnemy() -> FireWeapon() returned TOO CLOSE", foeDist::checkit, foe_distance);
			}
			else if (result == RETURN_TOOFAR)
			{
				// see if the bot can override combat advance and start moving towards enemy?
				if (pBot->IsUsedWeaponMain() && pBot->CanOverrideAdvanceTime())
				{
					// best bot will do it in 35% of time (worst bot will do it only in 15% of time)
					if (RANDOM_LONG(1, 100) <= 35 - (5 * pBot->GetBotSkill()))
					{
						pBot->SetAdvanceTowardEnemyTime(0.0f);

						if (pBot->IsTask(TASK_AVOID_ENEMY) || pBot->IsTask(TASK_IGNORE_ENEMY))
							pBot->SetTask(TASK_DEATHFALL);

#ifdef _DEBUG
						//@@@@@@@@@@@@@@@@
						if (botdebugger.IsDebugStance())
						{
							conOutput.Notify("(MAIN) Time to advance has been reset to zero -> about to start moving towards enemy\n", pBot);
						}
#endif

					}

					// set the time for next try to override the advance toward enemy time
					pBot->SetOverrideAdvanceTime(RANDOM_FLOAT(3.5f, 5.0f));
				}

				else if (pBot->IsUsedWeaponBackup())
				{
					// backup weapons tend to have short effective range so we will check whether we can switch to main weapon first
					if (pBot->DecideNextWeapon("ShootAtEnemy() -> FireWeapon() for backup weapon returned TOO FAR", foeDist::checkit, foe_distance))
					{
					}
					else if (pBot->CanOverrideAdvanceTime())
					{
						// best bots do it in 55% of time while worst bots only in 35%
						if (RANDOM_LONG(1, 100) <= 55 - (5 * pBot->GetBotSkill()))
						{
							pBot->SetAdvanceTowardEnemyTime(0.0f);

							if (pBot->IsTask(TASK_AVOID_ENEMY) || pBot->IsTask(TASK_IGNORE_ENEMY))
								pBot->SetTask(TASK_DEATHFALL);

#ifdef _DEBUG
							//@@@@@@@@@@@@@@@@
							if (botdebugger.IsDebugStance())
							{
								conOutput.Notify("(BACKUP) Time to advance has been reset to zero -> about to start moving towards enemy\n", pBot);
							}
#endif

						}

						pBot->SetOverrideAdvanceTime(RANDOM_FLOAT(2.5f, 4.0f));
					}
				}
			}
		}
	}

	// backup enemy distance
	pBot->SetPrevDistanceToEnemy(foe_distance);
}


/*
* used when bot needs to fire his weapon while not in combat, for example at a breakable object
*/
void BotFireWeaponOutOfCombat(bot_t* pBot)
{
	bot_fire_delay_t* pDelay = &bot_fire_delay[0];

	// is it a fully automatic weapon? then hold down "fire" button
	if (IsSMG(pBot->current_weapon.iId) || IsMachinegun(pBot->current_weapon.iId) || IsLMG(pBot->current_weapon.iId, pBot->pEdict->v.playerclass))
	{
		pBot->pEdict->v.button |= IN_ATTACK;
		// store the time even here so the bot knows when did he shoot last
		pBot->f_shoot_time = gpGlobals->time;

		// it's needed for correct bipod deployment on machine gun, if bot tried to deploy it too soon after shooting this action would not be registered and would fail
		pBot->SetWeaponStatus(WS_RELOADSECONDARY);
	}
	else
	{
		// press "fire" button
		pBot->pEdict->v.button |= IN_ATTACK;

		// then get correct shoot delays for this weapon (the array is filled with data read from external files in weapons folder)
		float base_delay = pDelay[pBot->current_weapon.iId].primary_base_delay;
		float min_delay = pDelay[pBot->current_weapon.iId].primary_min_delay[pBot->GetBotSkill()];
		float max_delay = pDelay[pBot->current_weapon.iId].primary_max_delay[pBot->GetBotSkill()];

		// and finally set the time of the next shot
		pBot->f_shoot_time = gpGlobals->time + base_delay + RANDOM_FLOAT(min_delay, max_delay);
	}

	if (botdebugger.IsDebugWeapons(1))
	//if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
	{
		char msg[128]{};
		sprintf(msg, "FireWeaponOutOfCombat() -> FIRING IT NOW (next weapon action will be allowed at %.2f)\n", pBot->f_shoot_time);
		conOutput.Notify(msg, pBot);
	}
}


/*
* checks whether bot currently uses any of dangerous explosives and makes him switch to a safe weapon in order to "attack" the breakable object in front
*/
bool BotSelectWeaponToDestroyThisBreakable(bot_t* pBot, bool check_distance)
{
	if (IsRPG(pBot->current_weapon.iId))
	{
		// do we need to check the distance to the breakable object?
		if (check_distance)
		{
			TraceResult tr;

			UTIL_MakeVectors(pBot->pEdict->v.v_angle);
			Vector v_src = pBot->pEdict->v.origin + pBot->pEdict->v.view_ofs;
			// we have to check up to the max distance at which the object can be for any possible case otherwise the hit point may not be farther than weapon safe range and bot would switch to knife then
			Vector v_dest = v_src + gpGlobals->v_forward * (EXTENDED_SEARCH_RADIUS * 2.0f);

			UTIL_TraceLine(v_src, v_dest, dont_ignore_monsters, pBot->pEdict, &tr);

			float distance_to_object = (tr.vecEndPos - v_src).Length();

			// see if the object is farther than half of the safe distance for this weapon and if so then make the bot keep using this weapon
			if (pBot->IsInSafeDistanceToShoot(distance_to_object))
				return false;
			// otherwise try to use different weapon
			else
			{
				// is this bot anti-armor specialist on his class specific path trying to destroy something that can be destroyed only by explosives?
				if (pBot->IsSubTask(ST_MEDEVAC_ST))
				{
					if (botdebugger.IsDebugActions() || botdebugger.IsDebugWaypoints())
						conOutput.Notify("***Too close to shoot -> leaving\n", pBot);

					// then the bot has to break the whole action, because he's too close to the object to use the rocket/grenade launcher
					pBot->SetWaitTime(0.0f);
					return true;
				}
				else
					pBot->DecideNextWeapon("SelectWeaponToDestroyThisBreakable()", foeDist::checkit, distance_to_object);
			}
		}

		if (pBot->IsAllowedToHandleWeapon())
		{
			// here we can't call UseKnife, because the checks in it won't pass till bot is completely out of ammo, so we have to switch manually 
			pBot->UseWeapon(uWeapon::knife);
			pBot->weapon_action = W_TAKEOTHER;

			if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
				conOutput.Notify("is going to switch to MELEE\n", pBot);
		}

		return true;
	}
	else if (pBot->IsUsedWeaponGrenade())
	{
		if (pBot->IsNoAmmoForMainWeapon() == false)
			pBot->UseMainWeapon();
		else
			pBot->UseBackupWeapon();

		return true;
	}

	return false;
}


/*
* returns TRUE if bot is inside specific standing bipod area like sandbags or window and such like
*/
bool IsInStandingBipodSpot(edict_t* pEdict)
{
	return (pEdict->v.vuser1.x == VUSR1_BIPOD_SPOT);
}


/*
* returns TRUE if bot is either lying prone or at specific spot where he can deploy bipod
*/
bool CanDeployBipod(edict_t* pEdict)
{
	return ((pEdict->v.iuser3 == USR3_PRONE) || (pEdict->v.vuser1.x == VUSR1_BIPOD_SPOT));
}


/*
* handles using weapon bipod, forced_call == false means the bot will call bipod command at random chance
*/
void BotUseBipod(bot_t *pBot, bool forced_call, const char* loc)
{
	// first check if the bot can start the action ...
	if (pBot->IsAllowedToHandleWeapon(0.1f) && pBot->IsNotGoingProne() && pBot->IsNotReloadingWeapon() && (pBot->IsBandagingNow() == false))
	{
		// see if NOT already handling the bipod AND is either forced to use it or is chance to use it
		if (pBot->IsNotDeployingBipod() && (forced_call || (RANDOM_LONG(1, 100) < 66)))
		{
			// now the bot is free to use the bipod so press secondary attack button ...
			pBot->pEdict->v.button |= IN_ATTACK2;

			// and set correct time to finish this action
			pBot->SetBipodDeployTime( GetBipodHandlingTime(pBot->current_weapon.iId, pBot->pEdict->v.playerclass, false) );

			// store current yaw view angle to allow counting max turn limits for bipod
			if (pBot->IsTask(TASK_BIPOD) == false)
				pBot->SetBipodYawAngle(pBot->pEdict->v.v_angle.y);

			// we must also clear this bit to allow the test for failure
			pBot->RemoveWeaponStatus(WS_CANTBIPOD);

			pBot->SetWeaponStatus(WS_BIPODMANIPULATION);




#ifdef DEBUG
			//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@														// NEW CODE 094 (remove it)
			if ((loc != NULL) && (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons()))
			{
				char dm[128]{};
				sprintf(dm, "called BIPOD command @ %s\n", loc);
				conOutput.Notify(dm, pBot);
			}
			//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#endif // DEBUG



			return;
		}

		// this is point where we handle bipod failure
		// seeing we didn't pass through previous if statement we know that the bot:
		// 1) decided not to deploy or fold bipod
		//    (deploying - bot decided not to use bipod on current enemy so we must lock it)
		//	  (folding - this next statement won't pass either and bot will try again in next game frame)
		// 2) that the engine didn't allow deploying bipod there and this function is now being called again so...
		if (pBot->IsTask(TASK_BIPOD) == false)
		{
			// set this bit to prevent trying to deploy bipod over and over again
			pBot->SetWeaponStatus(WS_CANTBIPOD);

			// and reset bipod time that is preventing the bot to open fire at his enemy
			pBot->SetBipodDeployTime(0.0f);



#ifdef DEBUG
			//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@														// NEW CODE 094 (remove it)
			if ((loc != NULL) && (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons()))
			{
				char dm[128]{};
				sprintf(dm, "tried BIPOD command @ %s, but FAILED !!!\n", loc);
				conOutput.Notify(dm, pBot);
			}
			//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#endif // DEBUG
		}

	}

	return;
}


/*
* returns correct time value based on given weapon to either deploying its bipod or folding it
*/
float GetBipodHandlingTime(int weapon, int specific_class, bool folding)
{
	// there seems to be no difference between deploying or folding the bipod so we can return just one value per weapon type...
	if ((weapon == dod_weapon_mg34) || (weapon == dod_weapon_mg42))
		return 0.9f;

	if (weapon == dod_weapon_30cal)
		return 1.2f;

	if ((weapon == dod_weapon_fg42) && (specific_class == 15))
		return 1.5f;

	if (weapon == dod_weapon_bren)
		return 1.7f;

	if ((weapon == dod_weapon_bar))
		return 1.8f;


	char dm[256]{};
	sprintf(dm, "BUG - Get BipodHandlingTime() was called for weaponID=%d (ModVersion=%d)\n", weapon, g_mod_version);
	util.DebugInFile(dm);

	return 0.0f;
}


/*
* switches grenade launcher between ready to fire and hold
*/
void BotSwitchGrenadeLauncherFireMode(bot_t* pBot, const char* loc)
{

	pBot->pEdict->v.button |= IN_ATTACK2;	// switch weapon fire modes (ie. primary fire or the attached GL)
	pBot->f_shoot_time = gpGlobals->time + g_time_to_ready_gl[pBot->GetBotSkill()]; // time to take effect

#ifdef DEBUG
	if ((loc != NULL) && botdebugger.IsDebugWeapons())
	{
		char dm[128]{};
		sprintf(dm, "called SwitchWeaponToSecondaryFireMode() @ %s\n", loc);
		conOutput.Notify(dm, pBot);
	}
#endif // DEBUG
}


/*
* returns TRUE if grenade launcher is on shoulder in ready to fire state
*/
bool IsGrenadeLauncherReadyToFire(edict_t *pEdict, int weapon)
{
	if ((weapon == dod_weapon_bazooka) || (weapon == dod_weapon_pschreck))
		return (pEdict->v.weaponanim == 2);
	else if (weapon == dod_weapon_piat)
		return ((pEdict->v.weaponanim == 4) || (pEdict->v.weaponanim == 5));	// 4 is with ammo in chamber and 5 is when it is empty

	// just in case someone passed invalid weapon ID
	return false;
}


/*
* returns TRUE if grenade launcher is off shoulder (ie. cannot be fired)
* actually we don't check weapon ID here, just whether weapon animation is equal to 0 or 1 in case of piat with empty chamber
*/
bool IsGrenadeLauncherOffShoulder(edict_t* pEdict)
{
	return ((pEdict->v.weaponanim == 0) || (pEdict->v.weaponanim == 1));
}