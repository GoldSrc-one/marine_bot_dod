//////////////////////////////////////////////////////////////////////////////////////////////
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
// waypoint.cpp
// 
////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef __linux__
#include <io.h>
#endif
#include <fcntl.h>
#ifndef __linux__
#include <sys\stat.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "defines.h"

#pragma warning( disable: 4005 91 )

#include "extdll.h"
#include "enginecallback.h"
#include "util.h"
#include "cbase.h"

#pragma warning( default: 4005 91 )

#include "bot.h"
#include "bot_func.h"
#include "bot_manager.h"
#include "client_commands.h"
#include "console_output.h"
#include "waypoint.h"

#define RANGE_20_WPT (W_FL_AMMOBOX | W_FL_DOOR | W_FL_DOORUSE | W_FL_JUMP | W_FL_DUCKJUMP | W_FL_LADDER | W_FL_USE)

extern int m_spriteTexture;
extern int m_spriteTexturePath1;
extern int m_spriteTexturePath2;
extern int m_spriteTexturePath3;


// the array of trigger events (being saved into waypoint file)
TRIGGER_EVENT trigger_events[MAX_TRIGGERS];

// the array of current game state of the events (not being saved into waypoint file)
trigger_event_gamestate_t trigger_gamestate[MAX_TRIGGERS];

// the triggers names as printable strings, must keep same order and amount of members as the enum class Waypoint Triggers IDs
const char* waypoint_triggers_names[MAX_TRIGGERS + 1] =
{
	"no_event",			// a.k.a. trigger_none, but we need to use it in waypoint info so if we print a 'no event' there then it makes more sense
	"trigger1",
	"trigger2",
	"trigger3",
	"trigger4",
	"trigger5",
	"trigger6",
	"trigger7",
	"trigger8"
};

// waypoints with information bits (flags)
WAYPOINT waypoints[MAX_WAYPOINTS];

// number of waypoints currently in use (cannot exceed MAX_WAYPOINTS)
int num_waypoints;

// waypoint neighbors
short num_neighbors[MAX_WAYPOINTS];
short* neighbors[MAX_WAYPOINTS];
short num_inv_neighbors[MAX_WAYPOINTS];
short* inv_neighbors[MAX_WAYPOINTS];
float waypoint_penalty[MAX_WAYPOINTS];

// declare the array of paths
W_PATH *w_paths[MAX_W_PATHS];

// number of w_paths currently in use (cannot exceed MAX_W_PATHS)
int num_w_paths;

path_value_manager_t pathvaluer;
waypointsbrowser_t wptser;
waypoint_console_output_manager_t wptoutputer;
waypoint_editing_functions_t wpteditor;
path_editing_functions_t patheditor;
waypoints_and_paths_repair_functions_t wptfixer;
waypoints_and_paths_managing_functions_t wptmanager;


char wpt_author[32];
char wpt_modified[32];

// variables used only in this file
static FILE* fp;
const char* wpt_warning = "Bots will not play this map well!\n";		// universal warning printed to console when waypoint or path file failed to load properly
const char* string_prior = "priority(1-highest)=";				// used in waypoint info and trigger waypoint info
const char* string_no_prior = " !NO PRIORITY!";						// warning about zero priority added at the end of the line in waypoint info and trigger waypoint info
const char* string_time = "\"wait/guard\" time=";					// used in waypoint info
const char* string_range = "Wpt range=";							// used in waypoint info
Vector last_waypoint;								// for autowaypointing
bool g_waypoint_paths = false;						// have any paths been allocated?
float wp_display_time;								// time that all nearby and visible waypoints were displayed (while editing)
float f_path_time;									// time that all nearby and visible paths were displayed (while editing)
float f_compass_time;								// time that the compass was displayed

// functions prototypes used in this file

void LinkedListError(const char* location = NULL, int path_index = NO_VAL);

void FreeAllTriggers(void);
bool IsMessageValid(const char* msg);
int TriggerNameToInt(const char* trigger_name);
int TriggerNameToIndex(const char* trigger_name);
TriggerId TriggerNameToId(const char* trigger_name);
TriggerId TriggerIndexToId(int i);
int TriggerIdToInt(TriggerId triggerId);
TriggerId IntToTriggerId(int i);//						This one is only needed for old waypoints (version 6 and 7) conversion. Can be erased once the conversions are no longer needed!

bool StartNewPath(int wpt_index, int array_slot = 0);
int ContinueCurrPath(int wpt_index, bool check_presence = true);
bool ExcludeFromPath(int wpt_index, int path_index);
bool ExcludeFromPath(W_PATH* p = NULL, int path_index = NO_VAL);
bool DeleteWholePath(int path_index);

void WaypointSetValue(bot_t* pBot, int wpt_index, WAYPOINT_VALUE* wpt_value);

// these few are used in WaypointThink

bool WaypointReachable(Vector v_srv, Vector v_dest, edict_t* pEntity);
void SetWaypointSize(int wpt_index, Vector& start, Vector& end);
Vector SetWaypointColor(int wpt_index);
int SetPathTexture(int path_index);
Vector SetPathColor(int path_index);
void WaypointBeam(edict_t* pEntity, Vector start, Vector end, int width, int noise, Vector color, int brightness, int speed, float duration);
void DrawTheBeam(edict_t* pEntity, Vector start, Vector end, int sprite, int life, int width, int noise, int red, int green, int blue, int brightness, int speed);

#ifdef _DEBUG
void DevDrawBeam(edict_t* pEntity, Vector start, Vector end, int red, int green, int blue, int life = 0, int speed = 10);
#endif


/*
* initializes the whole paths values array and resets the counters of available and valued paths
*/
void path_value_manager_t::InitPathValueArray(PATH_VALUE* path_value_array)
{
	for (int i = 0; i < array_size; i++)
	{
		ResetArraySlot(&path_value_array[i]);
	}
	
	available_paths_count = 0;
	valued_paths_count = 0;
}

/*
* assigns a value/usefulness to given path based on either bot behaviour or current needs or tasks
*/
void path_value_manager_t::SetValue(bot_t* pBot, int path_index, PATH_VALUE* path_value_array)
{
	/*/
#ifdef _DEBUG
	char msg[128];
	sprintf(msg, "SetPthVal called for path #%d\n", path_index + 1);
	conOutput.Notify(msg);
#endif
	/**/
	if (wptmanager.IsPath(path_index, PathT::carry_goal_item) && pBot->IsTask(TASK_GOALITEM))
	{
		path_value_array->path_value += 50;			// increase the value/usefulness of this path
	}

	// goals in DoD aren't just standard "capture the flag" but also "plant an explosives charge here" ...
	if ((wptmanager.IsPath(path_index, PathT::goal_team_one_tag) && pBot->IsBotTeam(teamONE.GetTeamId())) || (wptmanager.IsPath(path_index, PathT::goal_team_two_tag) && pBot->IsBotTeam(teamTWO.GetTeamId())))
	{
		// therefore we must check the goal itself and also check whether the bot has needed explosives charge
		if (wptmanager.IsPushpointGoalOnPathReachableForThisBot(pBot, path_index))
		{
			// has bot a need to reach map goals? then we will set much higher value to this path
			if (pBot->IsNeed(NEED_GOAL))
			{
				// and if the map objectives require explosives charges to capture then we'll set the value of this path on par with the "carry item" one so that the bot can actually visit this map goal
				// objective even when the waypoints creator decided to use the optional hint in form of "carry item" path type on other path on this junction,
				// this way the bot will decide 50-50 whether to go to this map objective or will continue via the other path (the "carry item" one) towards the other/distant map objective
				if (internals.IsMapGoalBasedOnExplosives())
					path_value_array->path_value += 50;
				else
					path_value_array->path_value += 25;
			}
			// otherwise we will raise the path value slightly only in roughly 1/3 cases
			else
			{
				if (RANDOM_LONG(1, 100) > 66)
				{
					if (internals.IsMapGoalBasedOnExplosives())
						path_value_array->path_value += 50;
					else
						path_value_array->path_value++;
				}
			}
		}
		// if the bot doesn't have "the tools" then ignore such path
		else
			path_value_array->path_value -= 500;
	}

	if (wptmanager.IsPath(path_index, PathT::goal_explosives_tag) && pBot->IsEquippedWithExplosiveCharge())
	{
		// find the waypoint on this path and see if it is set as a goal for the team this bot is in
		if (wptmanager.IsPathWaypointTypeTeamPriority(path_index, WptT::claymore, 1, pBot->GetBotTeam()))
		{
			if (pBot->IsNeed(NEED_GOAL))
				path_value_array->path_value += 25;
			else
			{
				if (RANDOM_LONG(1, 100) > 66)
					path_value_array->path_value++;
			}
		}
	}

	// we'll raise the value of this path only if the waypoint isn't disabled for this team
	if (wptmanager.IsPath(path_index, PathT::ammo_tag) && pBot->IsNeed(NEED_EXLOSIVESCHARGE) && (wptmanager.IsPathWaypointTypeTeamPriority(path_index, WptT::ammobox, NON_ZERO_WPT_PRIORITY, pBot->GetBotTeam())))
	{
		// and we'll raise the value of the path even higher if the map objectives require explosives charges to capture
		// ie. more than the carry item and goal paths so that the bots go pickup the explosives charges first ... before they move towards the goals
		if (internals.IsMapGoalBasedOnExplosives())
			path_value_array->path_value += 60;
		else
			path_value_array->path_value += 10;
	}

	if (wptmanager.IsPath(path_index, PathT::bandages_tag) && pBot->IsNeed(NEED_BANDAGES) && (wptmanager.IsPathWaypointTypeTeamPriority(path_index, WptT::bandage, NON_ZERO_WPT_PRIORITY, pBot->GetBotTeam())))
	{
		// is this bot a medic?
		//if (pBot->IsFASkill(FAID))
		//	path_value_array->path_value += 15;
		//else
			path_value_array->path_value += 5;

		// is bot in need of bandages while bleeding then raise the value for this path even more
		if (pBot->IsTask(TASK_BLEEDING))
			path_value_array->path_value += 10;
	}

	if (((wptmanager.IsPath(path_index, PathT::sniper_class) && pBot->IsBehaviour(SNIPER))) || (wptmanager.IsPath(path_index, PathT::mgunner_class) && pBot->IsBehaviour(MGUNNER)) ||
		(wptmanager.IsPath(path_index, PathT::antiarmor_class) && pBot->IsBehaviour(AASPEC)))
	{
		path_value_array->path_value += 3;
	}

	if (wptmanager.IsPath(path_index, PathT::patrol_cycle) && pBot->IsBehaviour(DEFENDER))
	{
		path_value_array->path_value += 2;
	}

	if (wptmanager.IsPath(path_index, PathT::roadblocked_tag))
	{
		// is it a non one-way path where is a goal placed claymore waypoint AND this bot carries needed explosives?
		if (wptmanager.IsRoadblockedPathException(pBot, path_index))
			;// then do NOT "trash" it, because this bot can get through the block by blowing it up
		else
			path_value_array->path_value -= 500;
	}

	// mark this slot used only if the path has NOT a negative value
	if (path_value_array->path_value >= 0)
		path_value_array->path_index = path_index;
	else
	{
		ResetArraySlot(path_value_array);
	}
}


/*
* counts valued paths in the array and returns TRUE if there's at least one
*/
bool path_value_manager_t::IsAnyValuedPath(PATH_VALUE* path_value_array)
{
	for (int i = 0; i < array_size; i++)
	{
		if (path_value_array[i].path_value > 0)
			valued_paths_count++;
	}

	if (valued_paths_count > 0)
		return true;

	return false;
}


/*
* returns the index of most valued path, but it can also ignore path value if it is NOT one of the top rated like a "carry goal item" for example
*/
int path_value_manager_t::GetMostValuedPath(PATH_VALUE* path_value_array)
{
	int path_index = NO_VAL;
	int best_value = NO_VAL;

	for (int i = 0; i < array_size; i++)
	{
		if (path_value_array[i].path_value > best_value)
		{
			// this will make sniper/mgunner/defender bots ignore their paths occasionally, so that we have a bit of randomness in decision making
			if ((path_value_array[i].path_value < 10) && (RANDOM_LONG(1, 100) <= 10))
				;
			else
			{
				path_index = path_value_array[i].path_index;
				best_value = path_value_array[i].path_value;
			}
		}
		// if there are more same valued paths then decide randomly
		else if ((path_value_array[i].path_value == best_value) && (RANDOM_LONG(1, 100) < 50))
		{
			path_index = path_value_array[i].path_index;
			best_value = path_value_array[i].path_value;
		}
	}

	// if things fail then get a random path
	if (path_index == NO_VAL)
		return GetRandomPath(path_value_array);

	return path_index;
}


/*
* returns the index of random path from the array
*/
int path_value_manager_t::GetRandomPath(PATH_VALUE* path_value_array)
{
	for (int i = 0; i < array_size; i++)
	{
		if (path_value_array[i].path_index != NO_VAL)
			available_paths_count++;
	}

	// we're working with array index therefore we must always decrease the available paths count by 1 else we would point to nonexistent data
	return path_value_array[RANDOM_LONG(0, available_paths_count - 1)].path_index;
}


/*
* assigns default values for the path value array
*/
void path_value_manager_t::ResetArraySlot(PATH_VALUE* path_value_array)
{
	path_value_array->path_index = NO_VAL;
	path_value_array->path_value = 0;
}


// sets all waypoints browser variables to defaults
waypointsbrowser_t::waypointsbrowser_t()
{
	ResetShowWaypoints();
	ResetShowPaths();
	ResetCheckAims();
	ResetCheckCross();
	ResetCheckRanges();
	ResetCheckShoot();
	ResetAutoWaypointing();
	ResetAutoAddToPath();
	ResetWaypointsDrawDistance();
	ResetWaypointsDisplayTime();
	ResetPathsDisplayTime();
	ResetAutoWaypointingDistance();
	ResetCompassIndex();
	ResetPathToHighlight();
};


/*
* resets all waypoints browser variables to default values on map change to prevent overloading game engine
*/
void waypointsbrowser_t::ResetOnMapChange(void)
{
	ResetShowWaypoints();
	ResetShowPaths();
	ResetCheckAims();
	ResetCheckCross();
	ResetCheckRanges();
	ResetCheckShoot();
	ResetAutoWaypointing();
	ResetAutoAddToPath();
	ResetWaypointsDrawDistance();
	ResetWaypointsDisplayTime();
	ResetPathsDisplayTime();
	ResetAutoWaypointingDistance();
	ResetCompassIndex();
	ResetPathToHighlight();
};


/*
* draws a simple beam connecting player origin and waypoint origin specified in argument
* handles also turning the feature off by the off argument
*/
bool waypointsbrowser_t::ShowCompass(edict_t* pEntity, const char* arg2)
{
	if (conInput.IsValidWaypointIndex(arg2, false, "off"))
	{
		if (FStrEq(arg2, "off"))
		{
			ResetCompassIndex();

			ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoint compass DISABLED!\n");
			return true;
		}

		if (waypoints[conInput.GetValidIndex()].flags & W_FL_DELETED)
		{
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "cannot guide you to deleted waypoint!\n");
			return false;
		}

		// now everything seems to be valid so we can turn it on
		SetCompassIndex(conInput.GetValidIndex());
		f_compass_time = 0.0f;

		// turn these two on as well so the user can see things right away
		SetShowWaypoints(true);
		SetShowPaths(true);

		// but turn these off to prevent overloading game engine which then makes the waypoints and all these beams to flicker
		ResetCheckAims();
		ResetCheckCross();
		ResetCheckRanges();
		ResetCheckShoot();

		return true;
	}
	else
	{
		// don't play the default sound confirmation here, it's done elsewhere
		conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity, true);
	}

	return false;
}


/*
* prints info about waypoint into the console
* if info_level == 0 it prints only important info for that waypoint
* if info_level == 1 it prints all except the comment (what the bot does there)
* if info_level == 2 it prints all and also the comment
*/
void waypointsbrowser_t::PrintWaypointInfo(edict_t* pEntity, const char* arg2, const char* arg3)
{
	char msg[256], wpt_flags_as_str[256], additional_info_1[80], additional_info_2[80], comment[512], steam_msg[512];
	int index, flags, teamOne_prior, teamTwo_prior, info_level;
	float teamOne_time, teamTwo_time, range;

	// we have to initialize these two first
	index = NO_VAL;
	info_level = 0;

	// now check the arguments for data
	if ((arg2 != NULL) && (*arg2 != 0))
	{
		if (FStrEq(arg2, "help") || FStrEq(arg2, "?"))
		{
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "wpt info <arg1> <arg2> where arg could be either waypoint index or 'more' or 'full'\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "'wpt info 5 more' will print additional info about waypoint no. 5\n'wpt info more 5' will do exactly the same\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "'wpt info more' will print additional information about any waypoint, but you must stand realy close to it\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "'wpt info full 5' will print all information about waypoint no. 5 including short description of what the bot will do there\n");
		}
		else if (FStrEq(arg2, "full"))
			info_level = 2;
		else if (FStrEq(arg2, "more"))
			info_level = 1;
		else if (conInput.IsValidWaypointIndex(arg2))
			index = conInput.GetValidIndex();
	}

	if ((arg3 != NULL) && (*arg3 != 0))
	{
		if (FStrEq(arg3, "full"))
			info_level = 2;
		else if (FStrEq(arg3, "more"))
			info_level = 1;
		else if (conInput.IsValidWaypointIndex(arg3))
			index = conInput.GetValidIndex();
	}

	// if neither argument passed waypoint index then try to find a waypoint nearby
	if (index == NO_VAL)
		index = wptmanager.FindNearestWaypointToPlayer(pEntity);

	// we still got no waypoint so we have nothing to print
	if (index == NO_VAL)
		return;

	// get the data out of the waypoint
	flags = waypoints[index].flags;
	teamOne_prior = waypoints[index].red_priority;
	teamOne_time = waypoints[index].red_time;
	teamTwo_prior = waypoints[index].blue_priority;
	teamTwo_time = waypoints[index].blue_time;
	range = waypoints[index].range;

	if (info_level == 2)
	{
		// init the comment
		strcpy(comment, "Bot will ");

		if (flags & W_FL_AIMING)
			strcat(comment, "aim at this point");
		else if (flags & W_FL_CROSS)
			strcat(comment, "choose next waypoint to continue");
		else
		{
			if (range < WPT_RANGE_SMALL)
			{
				strcat(comment, "slow down");
			}

			if (flags & W_FL_AMMOBOX)
			{
				if (strlen(comment) > 10)
					strcat(comment, " & ");
				strcat(comment, "take the ammo from ammobox");
			}
			if (flags & W_FL_BANDAGE)
			{
				if (strlen(comment) > 10)
					strcat(comment, " & ");
				strcat(comment, "take the bandages");
			}
			if (flags & W_FL_CHUTE)
			{
				if (strlen(comment) > 10)
					strcat(comment, " & ");
				strcat(comment, "continue forward only with parachute");
			}
			if (flags & W_FL_CROUCH)
			{
				if (strlen(comment) > 10)
					strcat(comment, " & ");
				strcat(comment, "crouch");
			}
			if (flags & W_FL_DOOR)
			{
				if (strlen(comment) > 10)
					strcat(comment, " & ");
				strcat(comment, "pass the door");
			}
			if (flags & W_FL_DOORUSE)
			{
				if (strlen(comment) > 10)
					strcat(comment, " & ");
				strcat(comment, "use the door to pass");
			}
			if (flags & W_FL_GOBACK)
			{
				if (strlen(comment) > 10)
					strcat(comment, " & ");
				strcat(comment, "turn back and return");
			}
			if (flags & W_FL_JUMP)
			{
				if (strlen(comment) > 10)
					strcat(comment, " & ");
				strcat(comment, "jump up or over");
			}
			if (flags & W_FL_DUCKJUMP)
			{
				if (strlen(comment) > 10)
					strcat(comment, " & ");
				strcat(comment, "use duckjump here");
			}
			if (flags & W_FL_LADDER)
			{
				if (strlen(comment) > 10)
					strcat(comment, " & ");
				strcat(comment, "climb the ladder");
			}
			if (flags & W_FL_MINE)
			{
				if (strlen(comment) > 10)
					strcat(comment, " & ");
				strcat(comment, "plant an explosive here");
			}
			if (flags & W_FL_PRONE)
			{
				if (strlen(comment) > 10)
					strcat(comment, " & ");
				strcat(comment, "be lying down");
			}
			if (flags & W_FL_PUSHPOINT)
			{
				if (strlen(comment) > 10)
					strcat(comment, " & ");
				strcat(comment, "capture the flag here");
			}
			if (flags & W_FL_ROADBLOCK)
			{
				if (strlen(comment) > 10)
					strcat(comment, " & ");
				strcat(comment, "check passability");
			}
			if (flags & W_FL_SPRINT)
			{
				if (strlen(comment) > 10)
					strcat(comment, " & ");
				strcat(comment, "sprint from here");
			}
			if (flags & W_FL_TRIGGER)
			{
				if (strlen(comment) > 10)
					strcat(comment, " & ");
				strcat(comment, "use priorities based on events");
			}
			if (flags & W_FL_USE)
			{
				if (strlen(comment) > 10)
					strcat(comment, " & ");
				strcat(comment, "use button, mounted gun etc.");
			}
			// these should be last
			if (flags & W_FL_FIRE)
			{
				if (strlen(comment) > 10)
					strcat(comment, " & ");
				strcat(comment, "fire the weapon");
			}
			if (flags & W_FL_SNIPER)
			{
				if (strlen(comment) > 10)
					strcat(comment, " & ");
				strcat(comment, "not move in combat");
			}
			if (flags & W_FL_DELETED)
			{
				if (strlen(comment) > 10)
					strcat(comment, " & ");
				strcat(comment, "ignore deleted waypoint");
			}
			if (flags & W_FL_STD)
			{
				// print this only if there's no other flag on this waypoint
				if (strlen(comment) < 11)
					strcat(comment, "just pass around");
			}
		}

		// finalize the comment string
		strcat(comment, "\n");
	}

	// first we get all tags on this waypoint as strings separated by spaces
	wptmanager.GetWaypointName(index, wpt_flags_as_str);

	// now start building the message
	sprintf(msg, "Waypoint %d of %d total (flags/tags= %s)\n", index + 1, num_waypoints, wpt_flags_as_str);

	// prepare the fancy HUD text that is used on Steam client, old WON client doesn't use it, because there is the notify area that can display 4 lines of the message (Steam shows just 1 line)
	if (is_steam)
	{
		strcpy(steam_msg, msg);
		
		// see if the message exceeds the HUD text line limits and if so then try to drop not so important info off it first
		if (IsHUDTextLineOverLengthLimit(steam_msg))
			conOutput.RemoveStringFromBuffer(steam_msg, "flags/");

		// try to maintain user defined max length of the HUD text line, also in Firearms a line of the fancy HUD text seems to be limited to 80 characters, anything more than that makes the game crash
		// (not in DoD, but in low screen resolution the text goes beyond the edge of the screen and isn't readable) so in cases when there are too many waypoint tags we have to take care of it
		// by scanning the tags backwards to find suitable space to shorten the list and marking this event with 3 dots
		conOutput.ShortenLineOfText(steam_msg, " ...)\n", externals.GetHUDTextLineLength(), 40);
	}

	//ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

	// we can't use standard printing here, because the line can exceed the limits of engine Text Message due to the fact that the waypoint can have several tags,
	// therefore we will use our own printing to client's console where we can print any long line of text in parts to fit into engine Text Message
	conOutput.PrintToClient(pEntity, HUD_PRINTNOTIFY, msg);

	// print this only if info_level == 0
	if (wptmanager.IsWaypoint(index, WptT::cross) && (info_level == 0))
	{
		sprintf(additional_info_1, "  %s %.1f\n", string_range, range);

		ClientPrint(pEntity, HUD_PRINTNOTIFY, additional_info_1);

		if (is_steam)
			strcat(steam_msg, additional_info_1);
	}
	// print this only if info_level == 0
	else if (wptmanager.IsWaypoint(index, WptT::aim) && (info_level == 0))
	{
		sprintf(additional_info_1, "  %s %s %d\n", teamONE.GetTeamName2wordsFUC(), string_prior, teamOne_prior);

		if (teamOne_prior == 0)
			conOutput.AppendBeforeNewline(additional_info_1, string_no_prior);

		sprintf(additional_info_2, "  %s %s %d\n", teamTWO.GetTeamName2wordsFUC(), string_prior, teamTwo_prior);

		if (teamTwo_prior == 0)
			conOutput.AppendBeforeNewline(additional_info_2, string_no_prior);

		ClientPrint(pEntity, HUD_PRINTNOTIFY, additional_info_1);
		ClientPrint(pEntity, HUD_PRINTNOTIFY, additional_info_2);

		if (is_steam)
		{
			if (IsHUDTextLineOverLengthLimit(additional_info_1) || IsHUDTextLineOverLengthLimit(additional_info_2))
			{
				conOutput.RemoveStringFromBuffer(additional_info_1, "(1-highest)");
				conOutput.RemoveStringFromBuffer(additional_info_2, "(1-highest)");
			}

			strcat(steam_msg, additional_info_1);
			strcat(steam_msg, additional_info_2);
		}
	}
	else
	{
		sprintf(additional_info_1, "  %s %s %d %s %.1f\n", teamONE.GetTeamName2wordsFUC(), string_prior, teamOne_prior, string_time, teamOne_time);

		if (teamOne_prior == 0)
			conOutput.AppendBeforeNewline(additional_info_1, string_no_prior);

		sprintf(additional_info_2, "  %s %s %d %s %.1f\n", teamTWO.GetTeamName2wordsFUC(), string_prior, teamTwo_prior, string_time, teamTwo_time);

		if (teamTwo_prior == 0)
			conOutput.AppendBeforeNewline(additional_info_2, string_no_prior);

		ClientPrint(pEntity, HUD_PRINTNOTIFY, additional_info_1);
		ClientPrint(pEntity, HUD_PRINTNOTIFY, additional_info_2);

		if (is_steam)
		{
			if (IsHUDTextLineOverLengthLimit(additional_info_1) || IsHUDTextLineOverLengthLimit(additional_info_2))
			{
				conOutput.RemoveStringFromBuffer(additional_info_1, "(1-highest)");
				conOutput.RemoveStringFromBuffer(additional_info_2, "(1-highest)");

				if (IsHUDTextLineOverLengthLimit(additional_info_1) || IsHUDTextLineOverLengthLimit(additional_info_2))
				{
					conOutput.RemoveStringFromBuffer(additional_info_1, "/guard");
					conOutput.RemoveStringFromBuffer(additional_info_2, "/guard");
				}
			}

			strcat(steam_msg, additional_info_1);
			strcat(steam_msg, additional_info_2);
		}

		// print the range for waypoints that don't use it only with higher info levels
		if ((wptmanager.IsNoRangeWaypoint(index) == false) || (wptmanager.IsNoRangeWaypoint(index) && (info_level > 0)))
		{
			sprintf(additional_info_1, "  %s %.1f\n", string_range, range);

			ClientPrint(pEntity, HUD_PRINTNOTIFY, additional_info_1);

			if (is_steam)
				strcat(steam_msg, additional_info_1);
		}
	}

	// print this only if info_level == 2 (complete info)
	if (info_level == 2)
		conOutput.PrintToClient(pEntity, HUD_PRINTNOTIFY, comment);

	if (is_steam)
	{
		// "clean" the console so its text doesn't collide with the HUD message
		ClientPrint(pEntity, HUD_PRINTNOTIFY, "\n");
	
		// display it on screen using the HUD message if we are on steam
		DisplayMsg(pEntity, steam_msg);
	}
}


/*
* prints info about trigger waypoint into the console
*/
void waypointsbrowser_t::PrintTriggerWaypointInfo(edict_t* pEntity, const char* arg2)
{
	char msg[256], wpt_flags_as_str[256], additional_info_1[80], additional_info_2[80], steam_msg[512];
	const char* string_curr_prior = "currently returned by this waypoint";
	const char* string_trailing = "trigger priority=";
	int index, teamOne_prior, teamTwo_prior, teamOne_trigger_prior, teamTwo_trigger_prior;
	TriggerId trigger_on_id, trigger_off_id;

	// find the nearest trigger waypoint
	index = wptmanager.FindNearestWaypointOfTypeToPlayer(pEntity, 50.0f, WptT::trigger);

	if (index == NO_VAL)
		return;

	teamOne_prior = waypoints[index].red_priority;
	teamTwo_prior = waypoints[index].blue_priority;
	teamOne_trigger_prior = waypoints[index].trigger_red_priority;
	teamTwo_trigger_prior = waypoints[index].trigger_blue_priority;
	trigger_on_id = waypoints[index].trigger_event_on;
	trigger_off_id = waypoints[index].trigger_event_off;

	wptmanager.GetWaypointName(index, wpt_flags_as_str);

	sprintf(msg, "Waypoint %d of %d total (flags/tags= %s)\n", index + 1, num_waypoints, wpt_flags_as_str);

	if (is_steam)
	{
		strcpy(steam_msg, msg);

		if (IsHUDTextLineOverLengthLimit(steam_msg))
			conOutput.RemoveStringFromBuffer(steam_msg, "flags/");
		
		conOutput.ShortenLineOfText(steam_msg, " ...)\n", externals.GetHUDTextLineLength(), 40);
	}

	//ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
	conOutput.PrintToClient(pEntity, HUD_PRINTNOTIFY, msg);

	if (FStrEq(arg2, "current"))
	{
		teamOne_prior = wptmanager.GetTriggerWaypointPriority(index, teamONE.GetTeamId());
		teamTwo_prior = wptmanager.GetTriggerWaypointPriority(index, teamTWO.GetTeamId());

		sprintf(additional_info_1, "  %s priority %s= %d\n", teamONE.GetTeamName2wordsFUC(), string_curr_prior, teamOne_prior);

		if (teamOne_prior == 0)
			conOutput.AppendBeforeNewline(additional_info_1, string_no_prior);

		sprintf(additional_info_2, "  %s priority %s= %d\n", teamTWO.GetTeamName2wordsFUC(), string_curr_prior, teamTwo_prior);

		if (teamTwo_prior == 0)
			conOutput.AppendBeforeNewline(additional_info_2, string_no_prior);
	}
	else
	{
		sprintf(additional_info_1, "  %s %s %d %s %d\n", teamONE.GetTeamName2wordsFUC(), string_prior, teamOne_prior, string_trailing, teamOne_trigger_prior);

		if ((teamOne_prior == 0) || (teamOne_trigger_prior == 0))
			conOutput.AppendBeforeNewline(additional_info_1, string_no_prior);

		sprintf(additional_info_2, "  %s %s %d %s %d\n", teamTWO.GetTeamName2wordsFUC(), string_prior, teamTwo_prior, string_trailing, teamTwo_trigger_prior);

		if ((teamTwo_prior == 0) || (teamTwo_trigger_prior == 0))
			conOutput.AppendBeforeNewline(additional_info_2, string_no_prior);
	}

	sprintf(msg, "  Trigger on event=\"%s\" trigger off event=\"%s\"\n", waypoint_triggers_names[TriggerIdToInt(trigger_on_id)], waypoint_triggers_names[TriggerIdToInt(trigger_off_id)]);

	ClientPrint(pEntity, HUD_PRINTNOTIFY, additional_info_1);
	ClientPrint(pEntity, HUD_PRINTNOTIFY, additional_info_2);
	ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

	if (is_steam)
	{
		if (IsHUDTextLineOverLengthLimit(additional_info_1) || IsHUDTextLineOverLengthLimit(additional_info_2))
		{
			conOutput.RemoveStringFromBuffer(additional_info_1, "(1-highest)");
			conOutput.RemoveStringFromBuffer(additional_info_2, "(1-highest)");

			conOutput.RemoveStringFromBuffer(additional_info_1, " by this waypoint");
			conOutput.RemoveStringFromBuffer(additional_info_2, " by this waypoint");
		}

		strcat(steam_msg, additional_info_1);
		strcat(steam_msg, additional_info_2);
		strcat(steam_msg, msg);

		ClientPrint(pEntity, HUD_PRINTNOTIFY, "\n");
		DisplayMsg(pEntity, steam_msg);
	}
}


/*
* lists through all waypoints printing their index and their flags/tags
*/
void waypointsbrowser_t::PrintAllWaypoints(edict_t* pEntity)
{
	char msg[256], wpt_flags_as_str[256];

	static int printed = 0;
	int right_now = 0;

	if (printed == 0)
		ClientPrint(pEntity, HUD_PRINTCONSOLE, "Printing all used waypoints ...\n");

	for (int index = printed; index < num_waypoints; index++)
	{
		wptmanager.GetWaypointName(index, wpt_flags_as_str);

		sprintf(msg, "Waypoint no. %d and its flags/tags= %s\n", index + 1, wpt_flags_as_str);
		//ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
		conOutput.PrintToClient(pEntity, HUD_PRINTCONSOLE, msg);

		printed++;
		right_now++;

		// did we print 22 lines of text in the console yet? (22 lines of text is roughly what a user can see on classic non-scrollable WON console in resolution 1024 x 768)
		// if so then stop it and wait for next call
		if (right_now == 22)
			return;
	}

	// we must have printed all waypoints so reset the static "memory"
	printed = 0;

	return;
}


/*
* prints info about path
* specified in path_index
* or the first path on the nearest waypoint
* or currently edited path
*/
bool waypointsbrowser_t::PrintPathInfo(edict_t* pEntity, int path_index)
{
	char msg[TEXT_MSG_SIZE];
	char steam_msg[512];	// 512 due to TE_TEXTMESSAGE limitiation
	W_PATH* p;
	bool have_path = false;

	// if no path is specified
	if (path_index == NO_VAL)
	{
		// look for a nearby waypoint first...
		int nearby_wpt = wptmanager.FindNearestWaypointToPlayer(pEntity);

		// and try to search for a path on that waypoint, but only if there actually is any waypoint nearby
		if (nearby_wpt != NO_VAL)
		{
			// now try to get the path from that waypoint ... if there are more paths on that waypoint get the first one
			for (path_index = 0; path_index < num_w_paths; path_index++)
			{
				// skip free slots
				if (w_paths[path_index] == NULL)
					continue;

				// are we highlighting red team accessible path AND path is NOT for red team
				if ((GetPathToHighlight() == HIGHLIGHT_TEAMONE) && !(w_paths[path_index]->flags & (P_FL_TEAM_NO | P_FL_TEAM_RED)))
					continue;	// so skip it
				// are we highlighting blue team accessible path AND path is NOT for blue team
				else if ((GetPathToHighlight() == HIGHLIGHT_TEAMTWO) && !(w_paths[path_index]->flags & (P_FL_TEAM_NO | P_FL_TEAM_BLUE)))
					continue;
				// are we highlighting one-way path AND path is NOT one-way
				else if ((GetPathToHighlight() == HIGHLIGHT_ONEWAY) && !(w_paths[path_index]->flags & P_FL_WAY_ONE))
					continue;
				// are we highlighting sniper path AND path is NOT a sniper only
				else if ((GetPathToHighlight() == HIGHLIGHT_SNIPER) && !(w_paths[path_index]->flags & P_FL_CLASS_SNIPER))
					continue;
				// are we highlighting machine gunner path AND path is NOT machine gunner only
				else if ((GetPathToHighlight() == HIGHLIGHT_MGUNNER) && !(w_paths[path_index]->flags & P_FL_CLASS_MGUNNER))
					continue;
				// are we highlighting anti-armor specialist path AND path is NOT anti-armor specialist only
				else if ((GetPathToHighlight() == HIGHLIGHT_ANTIARMOR) && !(w_paths[path_index]->flags & P_FL_CLASS_ANTIARMOR))
					continue;
				// are we highlighting specific path AND this index doesn't match it
				else if ((GetPathToHighlight() != HIGHLIGHT_DISABLED) && (GetPathToHighlight() != path_index) && (GetPathToHighlight() != HIGHLIGHT_TEAMONE) && (GetPathToHighlight() != HIGHLIGHT_TEAMTWO) &&
					(GetPathToHighlight() != HIGHLIGHT_ONEWAY) && (GetPathToHighlight() != HIGHLIGHT_SNIPER) && (GetPathToHighlight() != HIGHLIGHT_MGUNNER) && (GetPathToHighlight() != HIGHLIGHT_ANTIARMOR))
					continue;

				p = w_paths[path_index];

				// search whole path for this waypoint
				while (p)
				{
					if (p->wpt_index == nearby_wpt)
					{
						have_path = true;
						break;
					}

					p = p->next;	// check next node
				}

				// we already found correct path so there's no need to search through the rest of paths
				if (have_path)
					break;
			}
		}
	}

	// if not close to any waypoint, but there is a path that is currently edited
	if ((path_index == NO_VAL) && conInput.IsMissingArgument() && internals.IsPathToContinue())
		path_index = internals.GetPathToContinue();			// then use it

	// do we have a path index, but the path doesn't exist OR we still don't have any valid path index then there's nothing to be printed
	if (((path_index != NO_VAL) && (w_paths[path_index] == NULL)) || (path_index == NO_VAL))
		return false;

	// print path index
	sprintf(msg, "Path %d out of %d | Path length is %d | Path starts on wpt %d\n", path_index + 1, num_w_paths, wptmanager.GetPathLength(path_index), wptmanager.GetPathStart(path_index) + 1);

	// prepare additional steam message
	if (is_steam)
	{
		strcpy(steam_msg, msg);

		if (IsHUDTextLineOverLengthLimit(steam_msg))
		{
			// we want to keep the first word Path, but cut the other two in order to shorten the line
			conOutput.RemoveStringFromBuffer(steam_msg, "Path ", 6);
			conOutput.RemoveStringFromBuffer(steam_msg, "Path ", 6);
		}

		strcat(steam_msg, "  Path flags: ");
	}

	ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

	if (w_paths[path_index]->flags & P_FL_TEAM_NO)
	{
		sprintf(msg, "This path is used by both teams\n");
		if (is_steam)
			strcat(steam_msg, "BothTeams ");
	}
	else if (w_paths[path_index]->flags & P_FL_TEAM_RED)
	{
		sprintf(msg, "This path is used by %s only!\n", teamONE.GetTeamName2wordsLC());
		if (is_steam)
			strcat(steam_msg, teamONE.GetTeamNameAltF1word());
	}
	else if (w_paths[path_index]->flags & P_FL_TEAM_BLUE)
	{
		sprintf(msg, "This path is used by %s only!\n", teamTWO.GetTeamName2wordsLC());
		if (is_steam)
			strcat(steam_msg, teamTWO.GetTeamNameAltF1word());
	}

	ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

	if (w_paths[path_index]->flags & P_FL_WAY_ONE)
	{
		sprintf(msg, "This path is one way only!\n");
		if (is_steam)
			strcat(steam_msg, "OneWay ");
	}
	else if (w_paths[path_index]->flags & P_FL_WAY_TWO)
	{
		sprintf(msg, "This path is two-way\n");
		if (is_steam)
			strcat(steam_msg, "TwoWay ");
	}
	else if (w_paths[path_index]->flags & P_FL_WAY_PATROL)
	{
		sprintf(msg, "This path is patrol type (cycle start-end-start)!\n");
		if (is_steam)
			strcat(steam_msg, "PatrolPath ");
	}

	ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

	wptmanager.GetPathClass(path_index, msg);

	if (is_steam)
	{
		if (strstr(msg, "snipers") != NULL)
			strcat(steam_msg, "Snipers ");
		if (strstr(msg, "mgunners") != NULL)
			strcat(steam_msg, "MGunners ");
		if (strstr(msg, "anti-armor") != NULL)
			strcat(steam_msg, "AntiArmorSpecs ");
		if (strstr(msg, "classes") != NULL)
			strcat(steam_msg, "AllClasses ");
	}

	ClientPrint(pEntity, HUD_PRINTNOTIFY, "This path is used by ");
	ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

	if (w_paths[path_index]->flags & P_FL_MISC_IGNORE)
	{
		ClientPrint(pEntity, HUD_PRINTNOTIFY, "The bot will ignore enemies while on this path\n");
		if (is_steam)
			strcat(steam_msg, "IgnoreEnemy ");
	}
	else if (w_paths[path_index]->flags & P_FL_MISC_AVOID)
	{
		ClientPrint(pEntity, HUD_PRINTNOTIFY, "The bot will avoid distant enemies while on this path\n");
		if (is_steam)
			strcat(steam_msg, "AvoidEnemy ");
	}

	if (w_paths[path_index]->flags & P_FL_MISC_GITEM)
	{
		ClientPrint(pEntity, HUD_PRINTNOTIFY, "The bot will look for this path while carrying goal item\n");
		if (is_steam)
			strcat(steam_msg, "CarryItem ");
	}

	if (is_steam)
	{
		// empty the notify area so it doesn't collide with the HUD message
		ClientPrint(pEntity, HUD_PRINTNOTIFY, "\n");
		// the path tags are all added one after another on one line so better to check for the length of this line in order to prevent sudden game crash (unreadable message in DoD) due to exceeding
		// 80 characters on one line of HUD text, but the steam message contains 2 lines so we will have to skip the 1st line and check the 2nd one, because the tags are there so the risk of the message
		// going beyond the edge of the screen is on that line, in certain cases the 1st line will have roughly 60 characters at minimum; so we sum those 60 characters and the limiting 80 characters
		// of the 2nd line to start checking at position 140 in the string and we will check 30 characters back which should give us enough space to find a suitable spot for the cut off and addition of the 3 dots
		conOutput.ShortenLineOfText(steam_msg, " ... ", 60 + externals.GetHUDTextLineLength(), 30);
		// terminate the message
		steam_msg[strlen(steam_msg) - 1] = '\0';
		// and display it on screen
		DisplayMsg(pEntity, steam_msg);
	}

	return true;
}


/*
* prints all waypoint indexes and their types that are on given path
*/
bool waypointsbrowser_t::PrintWholePath(edict_t* pEntity, int path_index)
{
	if ((path_index == NO_VAL) || (w_paths[path_index] == NULL))
		return false;

	W_PATH* p = w_paths[path_index];
	char msg[256], wpt_flags_as_str[242];

	static int printed = 0;
	static int slot = 0;
	// there's no maximum at how long a path can be so let's expect here that no one would add more than 352 waypoints in one path
	int max_on_screen[] = { 22, 44, 66, 88, 110, 132, 154, 176, 198, 220, 242, 264, 286, 308, 330, 352 };

	// we need to remember which path we are printing in order to reset things if the client changes mind and starts printing another path before we finished printing 'current one'
	static int last_path = NO_VAL;

	// reset things if we are going to print a new path
	if (last_path != path_index)
	{
		printed = 0;
		slot = 0;
	}

	int safety_stop = 0;

	// the path is too long for one printing and we have already printed part of its waypoints
	// now we are calling it again so we must set the path pointer on the last printed waypoint before we start printing next part
	if (printed != 0)
	{
		int fast_forward = 0;

		while (p)
		{
			p = p->next;
			fast_forward++;

			if (fast_forward == printed)
				break;

			safety_stop++;
			if (safety_stop > LINKEDLIST_LOOPS_THRESHOLD)
				LinkedListError("Print Whole Path (fast forward)", path_index);
		}
	}

	// print the intro message only when we start from the beginning
	if (printed == 0)
		ClientPrint(pEntity, HUD_PRINTCONSOLE, "Printing whole path waypoint by waypoint (in order from start to end) ...\n");

	safety_stop = 0;

	while (p)
	{
		wptmanager.GetWaypointName(p->wpt_index, wpt_flags_as_str);

		sprintf(msg, "[#%d.] waypoint no. %d - %s\n", printed + 1, p->wpt_index + 1, wpt_flags_as_str);
		//ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
		conOutput.PrintToClient(pEntity, HUD_PRINTCONSOLE, msg);

		p = p->next;
		printed++;

		// did we print 22 lines on the screen already?
		if (printed == max_on_screen[slot])
		{
			// then select next slot, remember this path and "wait for next call"
			slot++;
			last_path = path_index;
			return true;
		}

		safety_stop++;
		if (safety_stop > LINKEDLIST_LOOPS_THRESHOLD)
			LinkedListError("Print Whole Path", path_index);
	}

	printed = 0;
	slot = 0;

	return true;
}


/*
* prints info about all paths on given waypoint index
* all paths on nearest waypoint if we pass -10 as waypoint index
* all used paths (ie. all stored in the path waypoint file for this map) if we pass -1 as waypoint index
*/
bool waypointsbrowser_t::PrintAllPaths(edict_t* pEntity, int wpt_index)
{
	W_PATH* p;
	char* team_fl, * way_fl;
	char class_fl[64]{};
	char misc[TEXT_MSG_SIZE]{};
	char msg[256];
	// these 3 allow us to split the output and print 22 paths each time this method gets called (it's used to deal with non steam console limits as well as limited steam console history)
	static int printed_paths = 0;
	static int slot = 0;
	int max_on_screen[] = { 22, 44, 66, 88, 110, 132, 154, 176, 198, 220, 242, 264, 286, 308, 330, 352, 374, 396, 418, 440, 462, 484, 506, 528 };

	if (wpt_index == -10)
	{
		// find the nearest waypoint
		wpt_index = wptmanager.FindNearestWaypointToPlayer(pEntity);

		// if there is no waypoint nearby we have to stop it here in this case
		if (wpt_index == NO_VAL)
			return false;
	}

	// aim and cross waypoints cannot be in a path
	if (wptmanager.IsWaypoint(wpt_index, WptT::aim, WptT::cross))
		return false;

	// if we aren't printing all used paths or we already printed them all
	if ((wpt_index != NO_VAL) || ((printed_paths + 1) >= num_w_paths))
	{
		// we have to reset these two
		printed_paths = 0;
		slot = 0;
	}

	for (int path_index = printed_paths; path_index < num_w_paths; path_index++)
	{
		// are we printing only paths on given waypoint and this path isn't on this waypoint then skip it
		if ((wpt_index != NO_VAL) && (wptmanager.IsWaypointOnPath(wpt_index, path_index) == false))
			continue;

		if (w_paths[path_index] == NULL)
			sprintf(msg, "path #%d | doesn't exist (it's deleted)!\n", path_index + 1);
		else
		{
			p = w_paths[path_index];

			if (p->flags & P_FL_TEAM_NO)
				team_fl = "both team";
			else if (p->flags & P_FL_TEAM_RED)
				team_fl = teamONE.GetTeamName2wordsLC();
			else if (p->flags & P_FL_TEAM_BLUE)
				team_fl = teamTWO.GetTeamName2wordsLC();
			else
				team_fl = "ERROR";

			if (p->flags & P_FL_CLASS_ALL)
			{
				strcpy(class_fl, "all class");
			}
			else if (p->flags & (P_FL_CLASS_SNIPER | P_FL_CLASS_MGUNNER | P_FL_CLASS_ANTIARMOR))
			{
				if (p->flags & P_FL_CLASS_SNIPER)
				{
					if (p->flags & (P_FL_CLASS_MGUNNER | P_FL_CLASS_ANTIARMOR))
						strcpy(class_fl, "sniper & ");
					else
						strcpy(class_fl, "sniper only");
				}
				if (p->flags & P_FL_CLASS_MGUNNER)
				{
					if (p->flags & (P_FL_CLASS_SNIPER | P_FL_CLASS_ANTIARMOR))
						strcat(class_fl, "mgunner & ");// append this tag to a previous one, or to previously empty string in the case of mgunner and anti-armor path
					else
						strcpy(class_fl, "mgunner only");
				}
				if (p->flags & P_FL_CLASS_ANTIARMOR)
				{
					if (p->flags & (P_FL_CLASS_SNIPER | P_FL_CLASS_MGUNNER))
						strcat(class_fl, "anti-armor & ");
					else
						strcpy(class_fl, "anti-armor only");
				}

				int length = strlen(class_fl);

				// is there a space character at the end of the string?
				if (class_fl[length - 1] == ' ')
				{
					// then terminate the string properly ie. remove the " & " part from its end
					class_fl[length - 3] = 0;
				}
			}
			else
				strcpy(class_fl, "ERROR");

			if (p->flags & P_FL_WAY_ONE)
				way_fl = "one-way";
			else if (p->flags & P_FL_WAY_TWO)
				way_fl = "two-way";
			else if (p->flags & P_FL_WAY_PATROL)
				way_fl = "patrol";
			else
				way_fl = "ERROR";

			misc[0] = 0;

			if (p->flags & P_FL_MISC_AMMO)
			{
				strcpy(misc, " ammo ");
			}
			if (p->flags & P_FL_MISC_BANDAGES)
			{
				if (misc[0] == 0)
					strcpy(misc, " bandages ");
				else
					strcat(misc, "& bandages ");
			}
			if (p->flags & P_FL_MISC_ROADBLOCKED)
			{
				if (misc[0] == 0)
					strcpy(misc, " RoadBlocked ");
				else
					strcat(misc, "& RoadBlocked ");
			}
			if (p->flags & P_FL_MISC_GOAL_RED)
			{
				if (misc[0] == 0)
					strcpy(misc, teamONE.GetTeamNameForGoalAltF1word());
				else
				{
					strcat(misc, "&");
					strcat(misc, teamONE.GetTeamNameForGoalAltF1word());
				}
			}
			if (p->flags & P_FL_MISC_GOAL_BLUE)
			{
				if (misc[0] == 0)
					strcpy(misc, teamTWO.GetTeamNameForGoalAltF1word());
				else
				{
					strcat(misc, "&");
					strcat(misc, teamTWO.GetTeamNameForGoalAltF1word());
				}
			}
			if (p->flags & P_FL_MISC_AVOID)
			{
				if (misc[0] == 0)
					strcpy(misc, " AvoidEnemy ");
				else
					strcat(misc, "& AvoidEnemy ");
			}
			if (p->flags & P_FL_MISC_IGNORE)
			{
				if (misc[0] == 0)
					strcpy(misc, " IgnoreEnemy ");
				else
					strcat(misc, "& IgnoreEnemy ");
			}
			if (p->flags & P_FL_MISC_GITEM)
			{
				if (misc[0] == 0)
					strcpy(misc, " CarryItem ");
				else
					strcat(misc, "& CarryItem ");
			}
			if (p->flags & P_FL_MISC_GEXPLOSIVES)
			{
				if (misc[0] == 0)
					strcpy(misc, " ExplosivesGoal ");
				else
					strcat(misc, "& ExplosivesGoal ");
			}

			if (misc[0] == 0)
				strcpy(misc, " NOTHING ");

			// we must cut the trailing space in the string also make sure it's null terminated seeing we are working with strcat and things may have gone wrong 
			misc[strlen(misc) - 1] = 0;

			sprintf(msg, "path #%d | length %d | starts on wpt #%d | %s, %s, %s (misc:%s)\n", path_index + 1, wptmanager.GetPathLength(path_index), p->wpt_index + 1, team_fl, way_fl, class_fl, misc);
		}

		//ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
		conOutput.PrintToClient(pEntity, HUD_PRINTCONSOLE, msg);

		// split the output only when printing all used paths
		if (wpt_index == NO_VAL)
			printed_paths++;

		// did we already use all lines/rows in this slot (ie. we printed 22 paths)
		if (printed_paths == max_on_screen[slot])
		{
			// then select next slot and "wait for next call"
			slot++;
			return true;
		}
	}

	return true;
}


/*
* returns the waypoint system version this file was created in
*/
int waypointsbrowser_t::GetWaypointsSystemVersion(void)
{
	char mapname[64];
	char filename[256];
	WAYPOINT_HDR header;

	strcpy(mapname, STRING(gpGlobals->mapname));
	strcat(mapname, ".wpt");

	if (internals.IsCustomWaypoints())
		util.MarineBotFileName(filename, "customwpts", mapname);
	else
		util.MarineBotFileName(filename, "defaultwpts", mapname);

	FILE* bfp = fopen(filename, "rb");

	// if file exists, read only the header info from it
	if (bfp != NULL)
	{
		fread(&header, sizeof(header), 1, bfp);

		header.filetype[7] = 0;
		if (strcmp(header.filetype, "FAM_bot") == 0)
		{
			int version;

			version = header.waypoint_file_version;

			fclose(bfp);

			return version;
		}
	}

	return -1;
}


/*
* read waypoints author signature as well as waypoint modifier signature from waypoint file header
*/
void waypointsbrowser_t::PrintWaypointsAuthors(char* author, char* modified_by)
{
	char mapname[64];
	char filename[256];
	WAYPOINT_HDR header;

	strcpy(mapname, STRING(gpGlobals->mapname));
	strcat(mapname, ".wpt");

	if (internals.IsCustomWaypoints())
		util.MarineBotFileName(filename, "customwpts", mapname);
	else
		util.MarineBotFileName(filename, "defaultwpts", mapname);

	FILE* bfp = fopen(filename, "rb");

	if (bfp != NULL)
	{
		fread(&header, sizeof(header), 1, bfp);

		// get the author signature if there's any written in the file header
		if ((strcmp(header.author, "") == 0) || (strcmp(header.author, "unknown") == 0))
			strcpy(author, "noauthor");		// there's NO author tag in this waypoint file
		else
			strcpy(author, header.author);
		// get the signature of guy who modified them if exists
		if ((strcmp(header.modified_by, "") == 0) || (strcmp(header.modified_by, "unknown") == 0))
			strcpy(modified_by, "nosig");	// waypoint file is NOT subscribed
		else
			strcpy(modified_by, header.modified_by);

		fclose(bfp);
	}
	// waypoint file doesn't exist
	else
	{
		strcpy(author, "nofile");
		strcpy(modified_by, "nofile");
	}
}


// sets all waypoint console output manager variables to defaults
waypoint_console_output_manager_t::waypoint_console_output_manager_t()
{
	ResetErrorCount();
	ResetWarningCount();
	ResetPrintedLines();
	ResetPrintErrorsOnly();
	ResetOverrideCounterReset();
};


void waypoint_console_output_manager_t::ResetAmountOfFoundIssues(void)
{
	ResetErrorCount();
	ResetWarningCount();
};


/*
* counters reset should be done at the start of each routine where it is needed, but master routines that call multiple subroutines need to count the total amount of errors therefore
* there is the override switch to disable the counter reset, make sure to unset the override at the end of any master routine where you used it
*/
void waypoint_console_output_manager_t::ResetCounters(void)
{
	// by default we will reset the counters unless it is overridden by this switch
	if (IsOverrideCounterReset() == false)
	{
		ResetErrorCount();
		ResetWarningCount();
		ResetPrintedLines();
	}
}


void waypoint_console_output_manager_t::AddError(int LinesOfText)
{
	IncErrorCount();
	IncPrintedLines(LinesOfText);
}


void waypoint_console_output_manager_t::AddWarning(int LinesOfText)
{
	IncWarningCount();
	IncPrintedLines(LinesOfText);
}


void waypoint_console_output_manager_t::ProcessIt(const char* message, bool log_in_file)
{
	static bool printing_disabled = false;

	// can we still print it on the screen?
	if (CanStillPrintIt())
	{
		// is printing disabled AND we limit printing to bugs only and this message is a bug OR we don't limit printing anymore? then enable printing again
		if (printing_disabled && (((message[0] == 'B') && IsPrintErrorsOnly()) || (IsPrintErrorsOnly() == false)))
			printing_disabled = false;
		// is printing limited to bugs only AND the message is a warning? then disable the printing (ie. don't print this message)
		else if (IsPrintErrorsOnly() && (message[0] == 'W'))
			printing_disabled = true;

		if (printing_disabled == false)
			conOutput.PrintToClient(conInput.GetCommandInvoker(), HUD_PRINTCONSOLE, message);
	}

	// we'll write the message in to the debugging file if needed
	if (log_in_file)
		util.DebugInFile(message);
}


/*
* initializes all waypoint and path structures including their display times
*/
void waypoint_editing_functions_t::InitAll(void)
{
	// free neighbors
	for(int i = 0; i < MAX_WAYPOINTS; i++) {
		if(neighbors[i])
			free(neighbors[i]);
		if(inv_neighbors[i])
			free(inv_neighbors[i]);
		neighbors[i] = NULL;
		inv_neighbors[i] = NULL;
		num_neighbors[i] = 0;
		num_inv_neighbors[i] = 0;
		waypoint_penalty[i] = 0;
	}

	// initialize the trigger event messages
	FreeAllTriggers();

	// have any waypoint path nodes been allocated yet?
	if (g_waypoint_paths)
	{
		patheditor.FreeAllPaths();
	}

	// destroy all waypoints with all their flags, priorities etc.
	for (int i = 0; i < MAX_WAYPOINTS; i++)
	{
		InitThisWaypoint(i);
	}

	// reset waypoints and paths display times
	wp_display_time = 0.0f;
	f_path_time = 0.0f;

	num_waypoints = 0;
	num_w_paths = 0;

	last_waypoint = g_vecZero;
}


/*
* adds given waypoint to current player position
* returns 0 if the waypoint was successfully added
* returns -1 if the waypoint_name isn't known waypoint
* returns -2 if there already is a waypoint in the vicinity
* returns -3 if we reached max amount of waypoints
*/
int waypoint_editing_functions_t::Add(edict_t* pEntity, const char* waypoint_name)
{
	WptT type_to_be_added;

	if (num_waypoints >= MAX_WAYPOINTS)
		return -3;

	// don't allow placing waypoints too close
	if (wptmanager.FindNearestWaypointToPlayer(pEntity, 20.0f) != NO_VAL)
	{
		// due to the old menu system the sound confirmation needs to be here
		EMIT_SOUND_DYN2(pEntity, CHAN_WEAPON, "buttons/button10.wav", 1.0, ATTN_NORM, 0, 100);

		return -2;
	}

	// no waypoint specified? then add the default waypoint
	if (conInput.IsMissingArgument())
		waypoint_name = "normal";

	// convert the name to the waypoint type
	type_to_be_added = wptmanager.GetWaypointTypeFromName(waypoint_name);

	// finally try to add the waypoint to the map
	if (AddType(pEntity->v.origin, type_to_be_added) == WptT::deleted)
	{
		// the function returned "deleted" waypoint which means nothing was added so we will return no value as a sign of non existent waypoint
		return NO_VAL;
	}

	// confirm successful waypoint addition
	EMIT_SOUND_DYN2(pEntity, CHAN_WEAPON, "weapons/xbow_hit1.wav", 1.0, ATTN_NORM, 0, 100);

	// and start displaying the waypoints
	wptser.SetShowWaypoints(true);

	return 0;
}


/*
* adds new waypoint to given location and sets all waypoint values (priority, time etc.)
* returns the Waypoint types value
*/
WptT waypoint_editing_functions_t::AddType(const Vector position, WptT wpt_type)
{
	int index;

	if (num_waypoints >= MAX_WAYPOINTS)
		return WptT::deleted;

	index = 0;

	// find the next available slot for the new waypoint...
	while (index < num_waypoints)
	{
		if (waypoints[index].flags & W_FL_DELETED)
			break;

		index++;
	}

	// reset this waypoint slot before using it
	InitThisWaypoint(index);

	waypoints[index].flags = 0;		// no flag yet, must be to clear the deleted flag

	// set standard flag used as a default
	waypoints[index].flags |= W_FL_STD;

	// if the waypoint creator set own value then we'll use it otherwise this variable holds hard coded default value so we can safely use it this way
	waypoints[index].range = internals.GetCustomDefaultWaypointRange();

	// now add all other flags based on the waypoint type
	if (wpt_type == WptT::normal)
	{
		// we don't need to add anything else, the flag is already set so we only check if autowaypointing is turned on and ...
		if (wptser.IsAutoWaypointing())
		{
			edict_t* pent = NULL;

			// search the surrounding for ammobox entity
			while ((pent = util.FindEntityInSphere(pent, position, STANDARD_SEARCH_RADIUS)) != NULL)
			{
				// if we found it then change the waypoint type to ammobox waypoint
				if (strcmp(STRING(pent->v.classname), "ammobox") == 0)
				{
					waypoints[index].flags |= W_FL_AMMOBOX;	// add another flag that makes this type
					waypoints[index].range = WPT_RANGE_SMALL;	// and set the range to this type specific value

					break;	// we are checking just for this single entity so no point continue looking for anything else now when we found it
				}
			}
		}
	}
	else if (wpt_type == WptT::ammobox)
	{
		waypoints[index].flags |= W_FL_AMMOBOX;
	}
	else if (wpt_type == WptT::parachute)
	{
		waypoints[index].flags |= W_FL_CHUTE;
	}
	else if (wpt_type == WptT::crouch)
	{
		waypoints[index].flags |= W_FL_CROUCH;
	}
	else if (wpt_type == WptT::door)
	{
		waypoints[index].flags |= W_FL_DOOR;
		waypoints[index].range = WPT_RANGE_SMALL;
	}
	else if (wpt_type == WptT::dooruse)
	{
		waypoints[index].flags |= W_FL_DOORUSE;
		waypoints[index].range = WPT_RANGE_SMALL;
	}
	else if (wpt_type == WptT::shoot)
	{
		waypoints[index].flags |= W_FL_FIRE;
	}
	else if (wpt_type == WptT::goback)
	{
		waypoints[index].flags |= W_FL_GOBACK;
	}
	else if (wpt_type == WptT::jump)
	{
		waypoints[index].flags |= W_FL_JUMP;
		waypoints[index].range = WPT_RANGE_SMALL;
	}
	else if (wpt_type == WptT::duckjump)
	{
		waypoints[index].flags |= W_FL_DUCKJUMP;
		waypoints[index].range = WPT_RANGE_SMALL;
	}
	else if (wpt_type == WptT::ladder)
	{
		waypoints[index].flags |= W_FL_LADDER;
		waypoints[index].range = WPT_RANGE_SMALL;
	}
	else if (wpt_type == WptT::claymore)
	{
		waypoints[index].flags |= W_FL_MINE;
		waypoints[index].range = WPT_RANGE_SMALL;
	}
	else if (wpt_type == WptT::prone)
	{
		waypoints[index].flags |= W_FL_PRONE;
	}
	else if (wpt_type == WptT::pushpoint)
	{
		waypoints[index].flags |= W_FL_PUSHPOINT;
		waypoints[index].range = WPT_RANGE_SMALL;
	}
	else if (wpt_type == WptT::roadblock)
	{
		waypoints[index].flags |= W_FL_ROADBLOCK;
	}
	else if (wpt_type == WptT::sprint)
	{
		waypoints[index].flags |= W_FL_SPRINT;
	}
	else if (wpt_type == WptT::sniper)
	{
		waypoints[index].flags |= W_FL_SNIPER;
	}
	else if (wpt_type == WptT::trigger)
	{
		waypoints[index].flags |= W_FL_TRIGGER;
	}
	else if (wpt_type == WptT::use)
	{
		waypoints[index].flags |= W_FL_USE;
		waypoints[index].range = WPT_RANGE_SMALL;
	}
	else if (wpt_type == WptT::aim)
	{
		waypoints[index].flags = 0;				// we need to clear the STD flag
		waypoints[index].flags |= W_FL_AIMING;
		waypoints[index].range = 0.0f;
	}
	else if (wpt_type == WptT::cross)
	{
		waypoints[index].flags = 0;
		waypoints[index].flags |= W_FL_CROSS;
		waypoints[index].range = WPT_CROSS_RANGE;

		// stop autowaypointing when a cross waypoint is added manually
		if (wptser.IsAutoWaypointing())
		{
			wpteditor.StartAutoWaypointg(false);

			// also change its range to match the autowaypointing distance in order to reach (be able to connect to) normal waypoint/s added by the autowaypointing
			waypoints[index].range = wptser.GetAutoWaypointingDistance() + 10.0f;
		}
	}
	else
	{
		// reset the flag back to default ie to deleted waypoint
		waypoints[index].flags = W_FL_DELETED;

		return WptT::deleted;
	}

	// store the origin (location) of this waypoint (use entity origin)
	waypoints[index].origin = position;

	// store the last used waypoint for the auto waypoint code...
	if ((wpt_type != WptT::aim) && (wpt_type != WptT::cross))
		last_waypoint = position;

	// reset the display time for waypoints to show this waypoint right away if the waypoints are turned on...
	wp_display_time = 0.0f;

	// increment total number of waypoints if adding at end of array...
	if (index == num_waypoints)
		num_waypoints++;

	// finally search the surrounding for bandages entity
	DetectBandagesAroundWaypoint(index);

	return wpt_type;
}


/*
* deletes the nearest waypoint to actual player position
* removes it also from any path the waypoint is in
* clears/frees this waypoint slot in global waypoint array
*/
void waypoint_editing_functions_t::Delete(edict_t* pEntity)
{
	int index;
	int count = 0;

	if (num_waypoints < 1)
		return;

	index = wptmanager.FindNearestWaypointToPlayer(pEntity);

	if (index == NO_VAL)
		return;

	// check if there are any paths
	if (num_w_paths >= 1)
	{
		// go through all paths
		for (int path_index = 0; path_index < num_w_paths; path_index++)
		{
			// remove this waypoint from any path this waypoint was added to
			ExcludeFromPath(index, path_index);
		}
	}

	// if we are deleting the last added waypoint then we also need to reset the position for autowaypointing otherwise we won't be able to start autowaypointing nearby
	if (waypoints[index].origin == last_waypoint)
		last_waypoint = g_vecZero;

	// call the init method which will reset the waypoint
	InitThisWaypoint(index);

	// refresh the waypoints if they are displayed to hide the waypoint beam and stuff right away
	wp_display_time = 0.0;

	// deactivate compass if this is the waypoint the user was looking for
	if (wptser.GetCompassIndex() == index)
		wptser.ResetCompassIndex();

	EMIT_SOUND_DYN2(pEntity, CHAN_WEAPON, "weapons/mine_activate.wav", 1.0, ATTN_NORM, 0, 100);
}


/*
* changes the type (or flag or tag) on given waypoint to the one specified in new_type
* if no waypoint index is passed it tries to search one in the vicinity
* returns -1 if there's no waypoint
* returns -2 if the index points to a deleted waypoint
* returns -3 if waypoint flag is missing or doesn't match known waypoints flags
* returns -4 if the new flag cannot be combined with flag/s already present on the waypoint
* returns -5 if the flag has been successfully added
* returns -6 if the flag was correctly removed
*/
int waypoint_editing_functions_t::ChangeType(edict_t* pEntity, const char* new_type, int wpt_index)
{
	int output = NO_VAL;
	int invalid_wpt_index = -2;
	int unknown_arg = -3;
	int forbidden_combination = -4;
	int added = -5;
	int removed = -6;

	// check given type whether it is a known waypoint
	if (wptmanager.GetWaypointTypeFromName(new_type) == WptT::scrapped_flagtype)
		return unknown_arg;

	// missing waypoint index so try to find one in the vicinity
	if (conInput.IsMissingArgument())
	{
		wpt_index = wptmanager.FindNearestWaypointToPlayer(pEntity);

		if (wpt_index == NO_VAL)
			return NO_VAL;
	}

	// see if this waypoint even exists or is deleted
	if ((wpt_index == NO_VAL) || wptmanager.IsWaypoint(wpt_index, WptT::deleted))
		return invalid_wpt_index;

	// handle special waypoints first
	if (FStrEq(new_type, "aim"))
	{
		// if the aiming flag was on this waypoint then remove it
		if (waypoints[wpt_index].flags & W_FL_AIMING)
		{
			waypoints[wpt_index].flags = 0;			// clear all flags
			waypoints[wpt_index].flags |= W_FL_STD;	// and set default flag (ie "reset" the waypoint)
			waypoints[wpt_index].range = WPT_RANGE;	// set default waypoint range
			output = removed;
		}
		// otherwise set it
		else
		{
			waypoints[wpt_index].flags = 0;		// reset all flags because this waypoint is special
			waypoints[wpt_index].flags |= W_FL_AIMING;	// set only the aiming flag
			waypoints[wpt_index].red_time = 0.0f;			// reset also both wait times
			waypoints[wpt_index].blue_time = 0.0f;
			waypoints[wpt_index].range = 0.0f;		// reset also range
			output = added;
		}

		// we need to immediately leave this method when special waypoint is done
		// due to it's speciality (no STD flag)
		return output;
	}
	else if (FStrEq(new_type, "cross"))
	{
		if (waypoints[wpt_index].flags & W_FL_CROSS)
		{
			waypoints[wpt_index].flags = 0;
			waypoints[wpt_index].flags |= W_FL_STD;
			waypoints[wpt_index].range = WPT_RANGE;
			output = removed;
		}
		else
		{
			waypoints[wpt_index].flags = 0;
			waypoints[wpt_index].flags |= W_FL_CROSS;
			waypoints[wpt_index].red_time = 0.0f;
			waypoints[wpt_index].blue_time = 0.0f;
			waypoints[wpt_index].range = WPT_CROSS_RANGE;	// set the range to cross default
			output = added;

			// see if this waypoint is part of any path
			int in_path = wptmanager.FindPath(wpt_index);

			// convert this waypoint index to a string value
			char wpt_index_as_char[6];
			sprintf(wpt_index_as_char, "%d", wpt_index + 1);

			while (in_path != NO_VAL)
			{
				char path_index_as_char[6];
				sprintf(path_index_as_char, "%d", in_path + 1);

				// first try to split the path on this waypoint
				patheditor.Split(pEntity, path_index_as_char, wpt_index_as_char);

				// no matter how the splitting went
				// (even if it was successfully splitted this waypoint is still inside this path as its last waypoint)
				// exclude this waypoint from this path
				ExcludeFromPath(wpt_index, in_path);

				// and check if there is yet another path on this waypoint
				in_path = wptmanager.FindPath(wpt_index);
			}

			// if we are autowaypointing ...
			if (wptser.IsAutoWaypointing())
			{
				// change its range to match the autowaypointing distance
				// in order to reach (be able to connect to) normal waypoint/s added by the autowaypointing
				waypoints[wpt_index].range = wptser.GetAutoWaypointingDistance() + 10.0f;
			}
		}

		return output;
	}

	// if we are changing directly from one of these waypoints do "reset" the waypoint
	if (waypoints[wpt_index].flags & (W_FL_AIMING | W_FL_CROSS))
	{
		waypoints[wpt_index].flags = 0;
		waypoints[wpt_index].flags |= W_FL_STD;
		waypoints[wpt_index].range = WPT_RANGE;
	}

	// continue with standard waypoint types
	if (FStrEq(new_type, "normal"))
	{
		// allow this to make the waypointing easier
		// user is able to "reset" the waypoint this way
		if (waypoints[wpt_index].flags & W_FL_CROUCH)
			waypoints[wpt_index].flags &= ~W_FL_CROUCH;
		else if (waypoints[wpt_index].flags & W_FL_PRONE)
			waypoints[wpt_index].flags &= ~W_FL_PRONE;

		waypoints[wpt_index].flags |= W_FL_STD;

		output = added;
	}

	else if (FStrEq(new_type, "ammobox"))
	{
		if (waypoints[wpt_index].flags & W_FL_AMMOBOX)
		{
			waypoints[wpt_index].flags &= ~W_FL_AMMOBOX;
			output = removed;
		}
		else
		{
			// ammobox itself can work as a turnback marker if used at the end of a path
			if (waypoints[wpt_index].flags & W_FL_GOBACK)
				waypoints[wpt_index].flags &= ~W_FL_GOBACK;

			// waypoint cannot be ammobox and a use at the same time
			if (waypoints[wpt_index].flags & W_FL_USE)
				waypoints[wpt_index].flags &= ~W_FL_USE;

			waypoints[wpt_index].flags |= W_FL_AMMOBOX;
			waypoints[wpt_index].red_time = 0.0f;
			waypoints[wpt_index].blue_time = 0.0f;
			output = added;
		}
	}

	else if (FStrEq(new_type, "parachute"))
	{
		if (waypoints[wpt_index].flags & W_FL_CHUTE)
		{
			waypoints[wpt_index].flags &= ~W_FL_CHUTE;
			output = removed;
		}
		else
		{
			// you can't go prone during parachuting
			if (waypoints[wpt_index].flags & W_FL_PRONE)
				waypoints[wpt_index].flags &= ~W_FL_PRONE;

			if (waypoints[wpt_index].flags & W_FL_DOOR)
				waypoints[wpt_index].flags &= ~W_FL_DOOR;

			if (waypoints[wpt_index].flags & W_FL_DOORUSE)
				waypoints[wpt_index].flags &= ~W_FL_DOORUSE;

			if (waypoints[wpt_index].flags & W_FL_USE)
				waypoints[wpt_index].flags &= ~W_FL_USE;

			waypoints[wpt_index].red_time = 0.0f;
			waypoints[wpt_index].blue_time = 0.0f;
			waypoints[wpt_index].flags |= W_FL_CHUTE;
			output = added;
		}
	}

	else if (FStrEq(new_type, "crouch"))
	{
		if (waypoints[wpt_index].flags & W_FL_CROUCH)
		{
			waypoints[wpt_index].flags &= ~W_FL_CROUCH;
			output = removed;
		}
		else
		{
			if (waypoints[wpt_index].flags & W_FL_PRONE)
				waypoints[wpt_index].flags &= ~W_FL_PRONE;

			waypoints[wpt_index].flags |= W_FL_CROUCH;
			output = added;
		}
	}

	else if (FStrEq(new_type, "door"))
	{
		if (waypoints[wpt_index].flags & W_FL_DOOR)
		{
			waypoints[wpt_index].flags &= ~W_FL_DOOR;
			output = removed;
		}
		else
		{
			if (waypoints[wpt_index].flags & W_FL_DOORUSE)
				waypoints[wpt_index].flags &= ~W_FL_DOORUSE;

			if (waypoints[wpt_index].flags & W_FL_CHUTE)
				waypoints[wpt_index].flags &= ~W_FL_CHUTE;

			waypoints[wpt_index].flags |= W_FL_DOOR;
			waypoints[wpt_index].range = WPT_RANGE_SMALL;
			output = added;
		}
	}

	else if (FStrEq(new_type, "usedoor"))
	{
		if (waypoints[wpt_index].flags & W_FL_DOORUSE)
		{
			waypoints[wpt_index].flags &= ~W_FL_DOORUSE;
			output = removed;
		}
		else
		{
			if (waypoints[wpt_index].flags & W_FL_DOOR)
				waypoints[wpt_index].flags &= ~W_FL_DOOR;

			if (waypoints[wpt_index].flags & W_FL_CHUTE)
				waypoints[wpt_index].flags &= ~W_FL_CHUTE;

			if (waypoints[wpt_index].flags & W_FL_USE)
				waypoints[wpt_index].flags &= ~W_FL_USE;

			waypoints[wpt_index].flags |= W_FL_DOORUSE;
			waypoints[wpt_index].range = WPT_RANGE_SMALL;
			output = added;
		}
	}

	else if (FStrEq(new_type, "shoot"))
	{
		if (waypoints[wpt_index].flags & W_FL_FIRE)
		{
			waypoints[wpt_index].flags &= ~W_FL_FIRE;
			output = removed;
		}
		else
		{
			if (waypoints[wpt_index].flags & W_FL_MINE)
				waypoints[wpt_index].flags &= ~W_FL_MINE;

			waypoints[wpt_index].flags |= W_FL_FIRE;
			output = added;
		}
	}

	else if (FStrEq(new_type, "goback"))
	{
		// both the ammobox as well as the use can serve as turnback if used at the end of a path
		if (waypoints[wpt_index].flags & (W_FL_AMMOBOX | W_FL_USE))
		{
			// so we need to immediately break it here to prevent any changes be done 
			return forbidden_combination;
		}

		if (waypoints[wpt_index].flags & W_FL_GOBACK)
		{
			waypoints[wpt_index].flags &= ~W_FL_GOBACK;
			output = removed;
		}
		else
		{
			waypoints[wpt_index].flags |= W_FL_GOBACK;
			output = added;
		}
	}

	else if (FStrEq(new_type, "jump"))
	{
		if (waypoints[wpt_index].flags & W_FL_JUMP)
		{
			waypoints[wpt_index].flags &= ~W_FL_JUMP;
			output = removed;
		}
		else
		{
			if (waypoints[wpt_index].flags & W_FL_PRONE)
				waypoints[wpt_index].flags &= ~W_FL_PRONE;

			if (waypoints[wpt_index].flags & W_FL_DUCKJUMP)
				waypoints[wpt_index].flags &= ~W_FL_DUCKJUMP;

			waypoints[wpt_index].flags |= W_FL_JUMP;
			waypoints[wpt_index].red_time = 0.0f;
			waypoints[wpt_index].blue_time = 0.0f;
			if (waypoints[wpt_index].range > WPT_RANGE_SMALL)
				waypoints[wpt_index].range = WPT_RANGE_SMALL;
			output = added;
		}
	}

	else if (FStrEq(new_type, "duckjump"))
	{
		if (waypoints[wpt_index].flags & W_FL_DUCKJUMP)
		{
			waypoints[wpt_index].flags &= ~W_FL_DUCKJUMP;
			output = removed;
		}
		else
		{
			if (waypoints[wpt_index].flags & W_FL_PRONE)
				waypoints[wpt_index].flags &= ~W_FL_PRONE;

			if (waypoints[wpt_index].flags & W_FL_JUMP)
				waypoints[wpt_index].flags &= ~W_FL_JUMP;

			waypoints[wpt_index].flags |= W_FL_DUCKJUMP;
			waypoints[wpt_index].red_time = 0.0f;
			waypoints[wpt_index].blue_time = 0.0f;
			if (waypoints[wpt_index].range > WPT_RANGE_SMALL)
				waypoints[wpt_index].range = WPT_RANGE_SMALL;
			output = added;
		}
	}

	else if (FStrEq(new_type, "ladder"))
	{
		if (waypoints[wpt_index].flags & W_FL_LADDER)
		{
			waypoints[wpt_index].flags &= ~W_FL_LADDER;
			output = removed;
		}
		else
		{
			if (waypoints[wpt_index].flags & W_FL_PRONE)
				waypoints[wpt_index].flags &= ~W_FL_PRONE;

			if (waypoints[wpt_index].flags & W_FL_SPRINT)
				waypoints[wpt_index].flags &= ~W_FL_SPRINT;

			waypoints[wpt_index].flags |= W_FL_LADDER;
			waypoints[wpt_index].red_time = 0.0f;
			waypoints[wpt_index].blue_time = 0.0f;
			waypoints[wpt_index].range = WPT_RANGE_SMALL;
			output = added;
		}
	}

	else if (FStrEq(new_type, "claymore"))
	{
		if (waypoints[wpt_index].flags & W_FL_MINE)
		{
			waypoints[wpt_index].flags &= ~W_FL_MINE;
			output = removed;
		}
		else
		{
			// you can't place claymores crouched
			if (waypoints[wpt_index].flags & W_FL_CROUCH)
				waypoints[wpt_index].flags &= ~W_FL_CROUCH;

			if (waypoints[wpt_index].flags & W_FL_FIRE)
				waypoints[wpt_index].flags &= ~W_FL_FIRE;

			waypoints[wpt_index].flags |= W_FL_MINE;
			if (waypoints[wpt_index].range > WPT_RANGE_SMALL)
				waypoints[wpt_index].range = WPT_RANGE_SMALL;
			output = added;
		}
	}

	else if (FStrEq(new_type, "prone"))
	{
		if (waypoints[wpt_index].flags & W_FL_PRONE)
		{
			waypoints[wpt_index].flags &= ~W_FL_PRONE;
			output = removed;
		}
		else
		{
			if (waypoints[wpt_index].flags & W_FL_CROUCH)
				waypoints[wpt_index].flags &= ~W_FL_CROUCH;

			waypoints[wpt_index].flags |= W_FL_PRONE;
			output = added;
		}
	}

	else if ((FStrEq(new_type, "flag")) || (FStrEq(new_type, "pushpoint")))
	{
		if (waypoints[wpt_index].flags & W_FL_PUSHPOINT)
		{
			waypoints[wpt_index].flags &= ~W_FL_PUSHPOINT;
			output = removed;
		}
		else
		{
			waypoints[wpt_index].flags |= W_FL_PUSHPOINT;
			output = added;
		}
	}

	else if (FStrEq(new_type, "roadblock"))
	{
		if (waypoints[wpt_index].flags & W_FL_ROADBLOCK)
		{
			waypoints[wpt_index].flags &= ~W_FL_ROADBLOCK;
			output = removed;
		}
		else
		{
			waypoints[wpt_index].flags |= W_FL_ROADBLOCK;
			output = added;
		}
	}

	else if (FStrEq(new_type, "sprint"))
	{
		if (waypoints[wpt_index].flags & W_FL_SPRINT)
		{
			waypoints[wpt_index].flags &= ~W_FL_SPRINT;
			output = removed;
		}
		else
		{
			if (waypoints[wpt_index].flags & W_FL_CROUCH)
				waypoints[wpt_index].flags &= ~W_FL_CROUCH;
			else if (waypoints[wpt_index].flags & W_FL_PRONE)
				waypoints[wpt_index].flags &= ~W_FL_PRONE;

			if (waypoints[wpt_index].flags & W_FL_LADDER)
				waypoints[wpt_index].flags &= ~W_FL_LADDER;

			waypoints[wpt_index].flags |= W_FL_SPRINT;
			output = added;
		}
	}

	else if (FStrEq(new_type, "sniper"))
	{
		if (waypoints[wpt_index].flags & W_FL_SNIPER)
		{
			waypoints[wpt_index].flags &= ~W_FL_SNIPER;
			output = removed;
		}
		else
		{
			waypoints[wpt_index].flags |= W_FL_SNIPER;
			output = added;
		}
	}

	else if (FStrEq(new_type, "trigger"))
	{
		if (waypoints[wpt_index].flags & W_FL_TRIGGER)
		{
			waypoints[wpt_index].flags &= ~W_FL_TRIGGER;
			output = removed;
		}
		else
		{
			waypoints[wpt_index].flags |= W_FL_TRIGGER;
			output = added;
		}
	}

	else if (FStrEq(new_type, "use"))
	{
		if (waypoints[wpt_index].flags & W_FL_USE)
		{
			waypoints[wpt_index].flags &= ~W_FL_USE;
			output = removed;
		}
		else
		{
			if (waypoints[wpt_index].flags & W_FL_AMMOBOX)
				waypoints[wpt_index].flags &= ~W_FL_AMMOBOX;
			// use itself can work as a turnback marker if used at the end of a path
			if (waypoints[wpt_index].flags & W_FL_GOBACK)
				waypoints[wpt_index].flags &= ~W_FL_GOBACK;

			waypoints[wpt_index].flags |= W_FL_USE;
			waypoints[wpt_index].red_time = 0.0f;
			waypoints[wpt_index].blue_time = 0.0f;
			if (waypoints[wpt_index].range > WPT_RANGE_SMALL)
				waypoints[wpt_index].range = WPT_RANGE_SMALL;
			output = added;
		}
	}

	else
		output = unknown_arg;

	DetectBandagesAroundWaypoint(wpt_index);

	return output;
}


/*
* changes priority on the nearest waypoint from current to the one specified set_this_priority
* if there is specified a team we want to change the priority for it changes only its value
* if team is NOT specified it sets the new priority value for both teams
* or if there is no new priority specified it checks current priorities and sets the higher one for both teams
* returns -4 if there is no waypoint close
* returns -3 if the new priority is invalid
* returns -2 if the team value is invalid
* returns -1 if something else went wrong
* returns the new priority if everything is OK
*/
int waypoint_editing_functions_t::ChangePriority(edict_t* pEntity, const char* set_this_priority, const char* for_team)
{
	int index, new_priority, wpt_priority = -1, team = -1;

	// find the nearest waypoint
	index = wptmanager.FindNearestWaypointToPlayer(pEntity);

	if (index == NO_VAL)
		return -4;

	// is there a new priority value?
	if ((set_this_priority != NULL) && (*set_this_priority != 0))
	{
		if (conInput.IsValidIntegerValue(set_this_priority, MIN_WPT_PRIOR, MAX_WPT_PRIOR))
			new_priority = conInput.GetIntegerValue();
		else
			return -3;
	}
	// otherwise check wpt_priorities and set the higher priority (lower number) for both teams
	else
	{
		// red is higher so copy it for blue team
		if (waypoints[index].red_priority < waypoints[index].blue_priority)
		{
			if (waypoints[index].red_priority == 0)
				waypoints[index].red_priority = waypoints[index].blue_priority;
			else
				waypoints[index].blue_priority = waypoints[index].red_priority;

			return waypoints[index].red_priority + 10;
		}
		// blue is higher so copy it for red team
		else if (waypoints[index].blue_priority < waypoints[index].red_priority)
		{
			if (waypoints[index].blue_priority == 0)
				waypoints[index].blue_priority = waypoints[index].red_priority;
			else
				waypoints[index].red_priority = waypoints[index].blue_priority;

			return waypoints[index].blue_priority + 10;
		}
		else
			return -1;
	}

	// is the team specified?
	if ((for_team != NULL) && (*for_team != 0))
	{
		if (conInput.IsValidTeam(for_team, true, false))
			team = conInput.GetIntegerValue();
		else
			return -2;
	}
	// otherwise both priorities are counted
	else
	{
		// does any priority differs from the new one
		if ((waypoints[index].red_priority != new_priority) || (waypoints[index].blue_priority != new_priority))
		{
			waypoints[index].red_priority = new_priority;
			waypoints[index].blue_priority = new_priority;

			if (new_priority != 0)
				return new_priority + 10;
			else
				return new_priority;
		}
	}

	// read red priority for this waypoint
	if (team == teamONE.GetTeamId())
		wpt_priority = waypoints[index].red_priority;
	// read blue priority for this waypoint
	if (team == teamTWO.GetTeamId())
		wpt_priority = waypoints[index].blue_priority;

	// is new priority NOT same to waypoint priority so set it
	if ((wpt_priority != -1) && (new_priority != wpt_priority))
	{
		if (team == teamONE.GetTeamId())
			waypoints[index].red_priority = new_priority;
		else if (team == teamTWO.GetTeamId())
			waypoints[index].blue_priority = new_priority;

		return new_priority;
	}

	return -1;
}


/*
* changes the time on the nearest waypoint
* works completely same to Priority change function (also return values are the same)
*/
float waypoint_editing_functions_t::ChangeTime(edict_t* pEntity, const char* set_this_time, const char* for_team)
{
	int index, team = -1;
	float new_time, wpt_time = -1.0f;

	// find the nearest waypoint
	index = wptmanager.FindNearestWaypointToPlayer(pEntity);

	if (index == NO_VAL)
		return -4.0f;

	// is there a new time value?
	if ((set_this_time != NULL) && (*set_this_time != 0))
	{
		// and is it a valid time value?
		// 600 seconds means max of 10 minutes waiting in case of a bot with standard behaviour, bot defenders will wait longer of course
		if (conInput.IsValidFloatValue(set_this_time, 0.0f, 600.0f))
			new_time = conInput.GetFloatValue();
		else
			return -3.0f;
	}
	// otherwise check both waypoint times and set the higher time for both teams
	else
	{
		// is red_time higher than blue_time
		if (waypoints[index].red_time > waypoints[index].blue_time)
		{
			waypoints[index].blue_time = waypoints[index].red_time;

			return waypoints[index].red_time + 1000.0f;
		}
		// is blue_time higher than red_time
		else if (waypoints[index].blue_time > waypoints[index].red_time)
		{
			waypoints[index].red_time = waypoints[index].blue_time;

			return waypoints[index].blue_time + 1000.0f;
		}
		// otherwise both are same
		else
			return -1.0f;
	}

	// if waypoint range is larger than half of max reachable range set it to the double of the default range
	if (waypoints[index].range > MAX_WPT_DIST / 2.0f)
		waypoints[index].range = WPT_RANGE * 2.0f;

	// is the team specified?
	if ((for_team != NULL) && (*for_team != 0))
	{
		if (conInput.IsValidTeam(for_team, true, false))
			team = conInput.GetIntegerValue();
		else
			return -2.0f;
	}
	// otherwise both times are counted
	else
	{
		// does any time differs from the new one
		if ((waypoints[index].red_time != new_time) || (waypoints[index].blue_time != new_time))
		{
			waypoints[index].red_time = new_time;
			waypoints[index].blue_time = new_time;

			if (new_time != 0.0f)
				return new_time + 1000.0f;
			else
				return new_time;
		}
	}

	// read red time for this waypoint
	if (team == teamONE.GetTeamId())
		wpt_time = waypoints[index].red_time;
	// read blue time for this waypoint
	if (team == teamTWO.GetTeamId())
		wpt_time = waypoints[index].blue_time;

	// is new time NOT same to waypoint time so set it
	if ((wpt_time != -1.0f) && (new_time != wpt_time))
	{
		if (team == teamONE.GetTeamId())
			waypoints[index].red_time = new_time;
		else if (team == teamTWO.GetTeamId())
			waypoints[index].blue_time = new_time;

		return new_time;
	}

	return -1.0f;
}


/*
* changes the range on the nearest waypoint to the value specified in arg2
* returns -3.0 if the new range is same to current value
* returns -2.0 if there is no waypoint close
* returns -1.0 if the argument is either missing or invalid
* returns the new value if everything is OK
*/
float waypoint_editing_functions_t::ChangeRange(edict_t* pEntity, const char* set_this_range)
{
	int index;
	float range, output = -1.0f;

	// find the nearest waypoint...
	index = wptmanager.FindNearestWaypointToPlayer(pEntity);

	if (index == NO_VAL)
		return -2.0f;

	// is new range specified and valid?
	if (conInput.IsValidFloatValue(set_this_range, 0.0f, MAX_WPT_DIST))
	{
		range = conInput.GetFloatValue();

		// if the new range is NOT same to current waypoint range
		if (range != waypoints[index].range)
		{
			// then set it
			waypoints[index].range = range;
			output = range;
		}
		else if (range == waypoints[index].range)
			output = -3.0f;
	}

	return output;
}


/*
* changes the range on the nearest waypoint by constant value specified in arg2
* returns -4.0 if the waypoint range would have exceeded max allowed waypoint range
* returns -3.0 if the waypoint range would have gone below zero
* returns -2.0 if there is no waypoint close
* returns -1.0 if the argument is invalid
* returns the new value if everything is OK
*/
float waypoint_editing_functions_t::ChangeRangeByConstantValue(edict_t* pEntity, const char* the_value, bool decreasing)
{
	int index;
	float range_change, current_range;

	// find the nearest waypoint...
	index = wptmanager.FindNearestWaypointToPlayer(pEntity);

	if (index == NO_VAL)
		return -2.0f;

	// is the constant value specified and valid?
	if (conInput.IsValidFloatValue(the_value, 0.0f, WPT_RANGE))
	{
		range_change = conInput.GetFloatValue();

		// get current range on this waypoint
		current_range = waypoints[index].range;

		// are we going to use dynamic value?
		if (range_change == 0.0f)
		{
			// then use 5 units for waypoint range below default range and 10 units for waypoint range larger than that
			if (current_range < WPT_RANGE)
				range_change = 5.0f;
			else
				range_change = 10.0f;
		}

		// see whether we are increasing or decreasing the range
		if (decreasing)
			current_range -= range_change;
		else
			current_range += range_change;

		// return error if the waypoint range would have gone below zero
		if (current_range < 0.0f)
			return -3.0f;

		// return error if the waypoint range would have exceeded max range value
		if (current_range > MAX_WPT_DIST)
			return -4.0f;

		// finally set the new range
		waypoints[index].range = current_range;

		// and return its new value as a confirmation
		return current_range;
	}

	// invalid argument
	return -1.0f;
}


/*
* changes the origin of given waypoint
* either current player position is used as the new waypoint origin or if additional arguments are used the waypoint will move in given direction by given value
*/
bool waypoint_editing_functions_t::ChangePosition(edict_t* pEntity, int wpt_index, const char* arg2, const char* arg3)
{
	// no waypoint specified? then try to find one nearby
	if (conInput.IsMissingArgument())
		wpt_index = wptmanager.FindNearestWaypointToPlayer(pEntity);

	// is it valid waypoint?
	if ((wpt_index != NO_VAL) && (wpt_index < num_waypoints) && (wptmanager.IsWaypoint(wpt_index, WptT::deleted) == false))
	{
		// are we going to move with the waypoint in one specific direction?
		if ((arg2 != NULL) && (*arg2 != 0))
		{
			float z_coord_backup = waypoints[wpt_index].origin.z;	// let's remember current z-coord in order to keep the left, right, forward and back shifts planar only
			UTIL_MakeVectors(pEntity->v.v_angle);

			if (conInput.IsValidFloatValue(arg3, 1.0f, MAX_WPT_DIST) == false)
				return false;
			else if (FStrEq(arg2, "up"))
				waypoints[wpt_index].origin.z += conInput.GetFloatValue();
			else if (FStrEq(arg2, "down"))
				waypoints[wpt_index].origin.z -= conInput.GetFloatValue();
			else if (FStrEq(arg2, "left"))
			{
				waypoints[wpt_index].origin = waypoints[wpt_index].origin - gpGlobals->v_right * conInput.GetFloatValue();
				// now we have to return the waypoint back on its original height level, because the direction vectors based on player view angles work in 3D space
				waypoints[wpt_index].origin.z = z_coord_backup;
			}
			else if (FStrEq(arg2, "right"))
			{
				waypoints[wpt_index].origin = waypoints[wpt_index].origin + gpGlobals->v_right * conInput.GetFloatValue();
				waypoints[wpt_index].origin.z = z_coord_backup;
			}
			else if (FStrEq(arg2, "forward"))
			{
				waypoints[wpt_index].origin = waypoints[wpt_index].origin + gpGlobals->v_forward * conInput.GetFloatValue();
				waypoints[wpt_index].origin.z = z_coord_backup;
			}
			else if (FStrEq(arg2, "back"))
			{
				waypoints[wpt_index].origin = waypoints[wpt_index].origin - gpGlobals->v_forward * conInput.GetFloatValue();
				waypoints[wpt_index].origin.z = z_coord_backup;
			}
			else
				return false;
		}
		// otherwise move it to current player position
		else
		{
			waypoints[wpt_index].origin = pEntity->v.origin;
		}

		// update waypoint flag/tag if we moved it next to bandages
		DetectBandagesAroundWaypoint(wpt_index);

		// reset display time so that the waypoint is redrawn immediately
		wp_display_time = 0.0f;

		return true;
	}

	return false;
}


/*
* sets all or selected waypoint data (priority, time etc.) to default values (for both teams), only the waypoint origin is left intact
* returns FALSE if there is no waypoint close
* returns TRUE if everything is OK
*/
int waypoint_editing_functions_t::ResetData(edict_t* pEntity, const char* arg1, const char* arg2, const char* arg3, const char* arg4)
{
	int index;
	bool success = false;

	// find the nearest waypoint
	index = wptmanager.FindNearestWaypointToPlayer(pEntity);

	if (index == NO_VAL)
		return NO_VAL;

	if (FStrEq(arg1, "all"))
	{
		// we must store the waypoint position first
		Vector temp_origin = waypoints[index].origin;
		// than we can reset the waypoint back to defualt values
		InitThisWaypoint(index);
		// return the waypoint back to its position
		waypoints[index].origin = temp_origin;
		// and finally change it to standard waypoint
		waypoints[index].flags = 0;
		waypoints[index].flags |= W_FL_STD;

		return 1;
	}

	// check for all naming variations of waypoint type
	if ((FStrEq(arg1, "type")) || (FStrEq(arg2, "type")) || (FStrEq(arg3, "type")) || (FStrEq(arg4, "type")) ||
		(FStrEq(arg1, "flag")) || (FStrEq(arg2, "flag")) || (FStrEq(arg3, "flag")) || (FStrEq(arg4, "flag")) ||
		(FStrEq(arg1, "tag")) || (FStrEq(arg2, "tag")) || (FStrEq(arg3, "tag")) || (FStrEq(arg4, "tag")))
	{
		waypoints[index].flags = 0;
		waypoints[index].flags |= W_FL_STD;

		success = true;
	}
	if ((FStrEq(arg1, "priority")) || (FStrEq(arg2, "priority")) || (FStrEq(arg3, "priority")) || (FStrEq(arg4, "priority")))
	{
		waypoints[index].red_priority = MAX_WPT_PRIOR;
		waypoints[index].blue_priority = MAX_WPT_PRIOR;

		success = true;
	}
	if ((FStrEq(arg1, "time")) || (FStrEq(arg2, "time")) || (FStrEq(arg3, "time")) || (FStrEq(arg4, "time")))
	{
		waypoints[index].red_time = 0.0f;
		waypoints[index].blue_time = 0.0f;

		success = true;
	}
	if ((FStrEq(arg1, "range")) || (FStrEq(arg2, "range")) || (FStrEq(arg3, "range")) || (FStrEq(arg4, "range")))
	{
		// set correct default range based on waypoint flag
		if (waypoints[index].flags & W_FL_AIMING)
			waypoints[index].range = 0.0f;
		else if (waypoints[index].flags & W_FL_CROSS)
			waypoints[index].range = WPT_CROSS_RANGE;
		else if (waypoints[index].flags & RANGE_20_WPT)
			waypoints[index].range = WPT_RANGE_SMALL;
		else
			waypoints[index].range = WPT_RANGE;

		success = true;
	}
	if ((FStrEq(arg1, "triggerpriority")) || (FStrEq(arg2, "triggerpriority")) || (FStrEq(arg3, "triggerpriority")) || (FStrEq(arg4, "triggerpriority")))
	{
		waypoints[index].trigger_red_priority = MAX_WPT_PRIOR;
		waypoints[index].trigger_blue_priority = MAX_WPT_PRIOR;

		success = true;
	}
	if ((FStrEq(arg1, "triggerevent")) || (FStrEq(arg2, "triggerevent")) || (FStrEq(arg3, "triggerevent")) || (FStrEq(arg4, "triggerevent")))
	{
		waypoints[index].trigger_event_on = TriggerId::trigger_none;
		waypoints[index].trigger_event_off = TriggerId::trigger_none;

		success = true;
	}

	if (success)
		return 1;

	return 0;
}


/*
* connects the game message to given trigger slot
*/
int waypoint_editing_functions_t::AddTriggerEvent(const char* trigger_name, const char* trigger_message)
{
	// incorrect input
	if ((strlen(trigger_name) < 1) || (strlen(trigger_message) < 1))
		return -1;

	int index = NO_VAL;

	index = TriggerNameToIndex(trigger_name);

	// invalid trigger name
	if (index == NO_VAL)
		return -2;

	// already used
	if (trigger_gamestate[index].GetUsed())
		return -3;

	// the max length of the trigger event message is 256
	// so inform the user if he is trying to use longer one
	if (strlen(trigger_message) > 255)
		return -4;

	// store the message
	strcpy(trigger_events[index].message, trigger_message);
	trigger_gamestate[index].SetUsed(true);

	return 1;
}


/*
* frees the trigger slot
*/
int waypoint_editing_functions_t::DeleteTriggerEvent(const char* trigger_name)
{
	// incorrect input
	if (strlen(trigger_name) < 1)
		return -1;

	int index = NO_VAL;

	index = TriggerNameToIndex(trigger_name);

	// invalid trigger name
	if (index == NO_VAL)
		return -2;

	// already free
	if (!trigger_gamestate[index].GetUsed())
		return -3;

	// delete the message
	trigger_events[index].message[0] = '\0';
	trigger_gamestate[index].SetUsed(false);

	return 1;
}


/*
* changes the trigger priority on the nearest trigger waypoint from current one
* to the one specified in priority argument
* if there is specified a team we want to change the priority for it changes only its value
* if team is NOT specified it sets the new value for both teams
* or if there is no new priority specified it checks current priorities and sets the higher one for both teams
* returns -4 if there is no waypoint close
* returns -3 if the new priority is invalid
* returns -2 if the team value is invalid
* returns -1 if something else went wrong
* returns the new priority if everything is OK
*/
int waypoint_editing_functions_t::ChangeTriggerPriority(edict_t* pEntity, const char* priority, const char* for_team)
{
	int index, new_priority, wpt_priority = -1, team = -1;

	// find the nearest trigger waypoint
	index = wptmanager.FindNearestWaypointOfTypeToPlayer(pEntity, 50.0f, WptT::trigger);

	if (index == NO_VAL)
		return -4;

	// is new priority value
	if ((priority != NULL) && (*priority != 0))
	{
		// is priority valid?
		if (conInput.IsValidIntegerValue(priority, MIN_WPT_PRIOR, MAX_WPT_PRIOR))
			new_priority = conInput.GetIntegerValue();
		else
			return -3;
	}
	// otherwise check priorities and set the higher priority (lower number) for both teams
	else
	{
		// red is higher so copy it for blue team
		if (waypoints[index].trigger_red_priority < waypoints[index].trigger_blue_priority)
		{
			if (waypoints[index].trigger_red_priority == 0)
				waypoints[index].trigger_red_priority = waypoints[index].trigger_blue_priority;
			else
				waypoints[index].trigger_blue_priority = waypoints[index].trigger_red_priority;

			return waypoints[index].trigger_red_priority + 10;
		}
		// blue is higher so copy it for red team
		else if (waypoints[index].trigger_blue_priority < waypoints[index].trigger_red_priority)
		{
			if (waypoints[index].trigger_blue_priority == 0)
				waypoints[index].trigger_blue_priority = waypoints[index].trigger_red_priority;
			else
				waypoints[index].trigger_red_priority = waypoints[index].trigger_blue_priority;

			return waypoints[index].trigger_blue_priority + 10;
		}
		else
			return -1;
	}

	// is team specified
	if ((for_team != NULL) && (*for_team != 0))
	{
		if (conInput.IsValidTeam(for_team, true, false))
			team = conInput.GetIntegerValue();
		else
			return -2;
	}
	// otherwise both priorities are counted
	else
	{
		// does any priority differs from the new one
		if ((waypoints[index].trigger_red_priority != new_priority) ||
			(waypoints[index].trigger_blue_priority != new_priority))
		{
			waypoints[index].trigger_red_priority = new_priority;
			waypoints[index].trigger_blue_priority = new_priority;

			if (new_priority != 0)
				return new_priority + 10;
			else
				return new_priority;
		}
	}

	// read red priority for this waypoint
	if (team == teamONE.GetTeamId())
		wpt_priority = waypoints[index].trigger_red_priority;
	// read blue priority for this waypoint
	if (team == teamTWO.GetTeamId())
		wpt_priority = waypoints[index].trigger_blue_priority;

	// is new priority NOT same to waypoint priority so set it
	if ((wpt_priority != -1) && (new_priority != wpt_priority))
	{
		if (team == teamONE.GetTeamId())
			waypoints[index].trigger_red_priority = new_priority;
		else if (team == teamTWO.GetTeamId())
			waypoints[index].trigger_blue_priority = new_priority;

		return new_priority;
	}

	return -1;
}


/*
* connects given trigger message to nearest trigger waypoint
* the state argument can be "on" or "off" and determines if this message will trigger the waypoint on or off
*/
int waypoint_editing_functions_t::ConnectTriggerEvent(edict_t* pEntity, const char* trigger_name, const char* state)
{
	// incorrect input
	if (strlen(trigger_name) < 1)
		return -1;

	int index = NO_VAL;
	bool trigger_on;

	index = TriggerNameToIndex(trigger_name);

	// invalid trigger name
	if (index == NO_VAL)
		return -2;

	// find if this message will trigger it on or off
	if ((strlen(state) < 1) || (FStrEq(state, "on")))
		trigger_on = true;
	else if (FStrEq(state, "off"))
		trigger_on = false;
	else
		return -3;

	index = wptmanager.FindNearestWaypointOfTypeToPlayer(pEntity, 50.0f, WptT::trigger);

	// no trigger waypoint around
	if (index == NO_VAL)
		return -4;

	// connect the trigger message to this waypoint
	if (trigger_on)
		waypoints[index].trigger_event_on = TriggerNameToId(trigger_name);
	else
		waypoints[index].trigger_event_off = TriggerNameToId(trigger_name);

	return 1;
}


/*
* removes given trigger message from nearest trigger waypoint based on the state argument
*/
int waypoint_editing_functions_t::RemoveTriggerEvent(edict_t* pEntity, const char* state)
{
	int index = NO_VAL;
	bool trigger_on;

	// find if it is a trigger on or off that's going to be removed
	if (strlen(state) < 1)
		return -1;
	else if (FStrEq(state, "on"))
		trigger_on = true;
	else if (FStrEq(state, "off"))
		trigger_on = false;
	else
		return -2;

	index = wptmanager.FindNearestWaypointOfTypeToPlayer(pEntity, 50.0f, WptT::trigger);

	// no trigger waypoint around
	if (index == NO_VAL)
		return -3;

	// reset appropriate trigger event for this waypoint
	if (trigger_on)
		waypoints[index].trigger_event_on = TriggerId::trigger_none;
	else
		waypoints[index].trigger_event_off = TriggerId::trigger_none;

	return 1;
}


/*
* handles all events with turning the auto waypoint on and off
*/
void waypoint_editing_functions_t::StartAutoWaypointg(bool switch_on)
{
	extern edict_t* listenserver_edict;

	if (switch_on)
	{
		wptser.SetAutoWaypointing(true);	// turn autowaypointing on
		wptser.SetShowWaypoints(true);		// turn waypoints on too just in case

		// sign the waypoints as autowaypointed ones
		if (Subscribe("built-in auto waypointing", true, true))
		{
			// we should also clear the 'modified by' signature
			Subscribe("clear", false, false);
			SaveWaypoints(NULL);
			patheditor.SavePaths(NULL);
		}

		wptser.SetAutoAddToPath(true);		// activate also auto adding to path
		wptser.SetShowPaths(true);			// start displaying the paths as well
	}
	else
	{
		wptser.ResetAutoWaypointing();		// stop autowaypointing
		wptser.ResetAutoAddToPath();		// deactivate automatic additions to path

		// correctly end current path
		patheditor.Finish(listenserver_edict);

		// run waypoint self cleaning routines
		wptfixer.RepairWaypointRangeAndPosition(listenserver_edict);
		wptfixer.RepairCrossWaypointRange();
		wptfixer.RepairInvalidPathMerge();
	}

	return;
}


/*
* deletes all waypoints & paths
* then removes both signatures
* and finally erases both files from the HDD
*/
void waypoint_editing_functions_t::WipeAll(void)
{
	char mapname[64];
	char filename[256];

	InitAll();

	// reset author's signature
	strcpy(wpt_author, "unknown");
	// reset modified_by signature
	strcpy(wpt_modified, "unknown");

	strcpy(mapname, STRING(gpGlobals->mapname));
	strcat(mapname, ".wpt");

	// build the filename/filepath for this file
	if (internals.IsCustomWaypoints())
		util.MarineBotFileName(filename, "customwpts", mapname);
	else
		util.MarineBotFileName(filename, "defaultwpts", mapname);

	// and then erase it from HDD
	remove(filename);

	strcpy(mapname, STRING(gpGlobals->mapname));
	strcat(mapname, ".pth");

	if (internals.IsCustomWaypoints())
		util.MarineBotFileName(filename, "customwpts", mapname);
	else
		util.MarineBotFileName(filename, "defaultwpts", mapname);

	remove(filename);

	return;
}


/*
* adds waypoints author signature or signature of the one who modified them based on the switch
* authors signature cannot be changed unless enforced switch is used
*/
bool waypoint_editing_functions_t::Subscribe(const char* signature, bool is_it_the_author, bool enforced)
{
	char mapname[64];
	char filename[256];
	WAYPOINT_HDR header;

	strcpy(mapname, STRING(gpGlobals->mapname));
	strcat(mapname, ".wpt");

	if (internals.IsCustomWaypoints())
		util.MarineBotFileName(filename, "customwpts", mapname);
	else
		util.MarineBotFileName(filename, "defaultwpts", mapname);

	FILE* bfp = fopen(filename, "rb");

	if (bfp != NULL)
	{
		fread(&header, sizeof(header), 1, bfp);

		// is it authors sig
		if (is_it_the_author)
		{
			// is wpt file NOT subcribed OR do we force it so access granted to change wpt_author (no file writing)
			if ((strcmp(header.author, "") == 0) || (strcmp(header.author, "unknown") == 0) || enforced)
			{
				strcpy(wpt_author, signature);
				wpt_author[31] = 0;		// must be ended properly

				fclose(bfp);
				return true;
			}
			// otherwise access denied
			else
			{
				fclose(bfp);
				return false;
			}
		}
		// the signature of the one who modified the waypoints (no file writing)
		else
		{
			if (FStrEq(signature, "clear"))
			{
				wpt_modified[0] = '\0';		// wipe the modified by variable
			}
			else
			{
				strcpy(wpt_modified, signature);
				wpt_modified[31] = 0;		// must be ended properly
			}

			fclose(bfp);
			return true;
		}
	}
	
	return false;
}


/*
* autosaves waypoints and paths after given time to special files
*/
bool waypoint_editing_functions_t::AutoSaveWaypoints(void)
{
	// don't save waypoints if we are currently building a path
	// because path save routine automatically stops it
	// so we would break stuff such as auto waypointing for example
	// that would stop creating the path and would only add waypoints
	if (internals.IsPathToContinue())
		return false;

	char custom_name[32];

	// set name for those files
	strcpy(custom_name, "autosave");

	if (SaveWaypoints(custom_name))
	{
		if (patheditor.SavePaths(custom_name))
			return true;
	}

	return false;
}


/*
* saves waypoint structure into the file
*/
bool waypoint_editing_functions_t::SaveWaypoints(const char* custom_filename)
{
	char filename[256];
	char mapname[64];
	WAYPOINT_HDR header{};
	int index;

	// init the waypoint file header with all data

	strcpy(header.filetype, "FAM_bot");

	header.waypoint_file_version = WAYPOINT_VERSION;
	header.waypoint_file_flags = 0;  // not currently used
	header.number_of_waypoints = num_waypoints;

	memset(header.mapname, 0, sizeof(header.mapname));
	strncpy(header.mapname, STRING(gpGlobals->mapname), 31);
	header.mapname[31] = 0;

	// write author's signature
	memset(header.author, 0, sizeof(header.author));
	if (wpt_author[0] == 0)
		strncpy(header.author, "unknown", 31);
	else
		strncpy(header.author, wpt_author, 31);
	header.author[31] = 0;

	// write the signature of the one who modified them
	memset(header.modified_by, 0, sizeof(header.modified_by));
	if (wpt_modified[0] == 0)
		strncpy(header.modified_by, "unknown", 31);
	else
		strncpy(header.modified_by, wpt_modified, 31);
	header.modified_by[31] = 0;

	// if we used our own name save them under it (eg. autosave)
	if (custom_filename != NULL)
		strcpy(mapname, custom_filename);
	// otherwise use current map name
	else
		strcpy(mapname, STRING(gpGlobals->mapname));
	strcat(mapname, ".wpt");

	if (internals.IsCustomWaypoints())
		util.MarineBotFileName(filename, "customwpts", mapname);
	else
		util.MarineBotFileName(filename, "defaultwpts", mapname);

	FILE* bfp = fopen(filename, "wb");

	if (bfp != NULL)
	{
		// write the waypoint header to the file
		fwrite(&header, sizeof(header), 1, bfp);

		// write the waypoint data to the file
		for (index = 0; index < num_waypoints; index++)
		{
			fwrite(&waypoints[index], sizeof(waypoints[0]), 1, bfp);
		}

		// write the triggers
		for (index = 0; index < MAX_TRIGGERS; index++)
		{
			fwrite(&trigger_events[index], sizeof(trigger_events[0]), 1, bfp);
		}

		fclose(bfp);

		// the waypoint file exists now so we must delete both error messages
		errormsgs.DeleteErrorCode(UEMS_WARN_WPT);
		errormsgs.DeleteFromCopyOfErCodes(UEMS_WARN_WPT);

		return true;
	}

	return false;
}


/*
* loads waypoint structure from file
* returns -10 if waypoint file was not found (doesn't exist)
* returns -1 if old waypoint structure is detected (to allow auto conversion)
* returns 0 if something went wrong (like not MarineBot waypoints etc.)
* returns 1 if everything is OK
*/
int waypoint_editing_functions_t::LoadWaypoints(edict_t* pEntity, const char* custom_filename)
{
	extern bool is_dedicated_server;

	char mapname[64];
	char filename[256];
	WAYPOINT_HDR header;
	char msg[256];

	if (custom_filename == NULL)
	{
		strcpy(mapname, STRING(gpGlobals->mapname));
		strcat(mapname, ".wpt");
	}
	else
	{
		strcpy(mapname, custom_filename);
		strcat(mapname, ".wpt");
	}

	if (internals.IsCustomWaypoints())
		util.MarineBotFileName(filename, "customwpts", mapname);
	else
		util.MarineBotFileName(filename, "defaultwpts", mapname);

	if (is_dedicated_server)
	{
		sprintf(msg, "loading waypoint file: %s\n", filename);

		conOutput.Print(NULL, msg, MType::msg_info);
	}

	FILE* bfp = fopen(filename, "rb");

	// if file exists, read the waypoint structure from it
	if (bfp != NULL)
	{
		fread(&header, sizeof(header), 1, bfp);

		header.filetype[7] = 0;
		if (strcmp(header.filetype, "FAM_bot") == 0)
		{
			if (header.waypoint_file_version != WAYPOINT_VERSION)
			{
				sprintf(msg, "Outdated waypoint file version: %d (current waypoint system version: %d)\n", header.waypoint_file_version, WAYPOINT_VERSION);
				conOutput.Print(pEntity, msg, MType::msg_error);

				conOutput.Print(pEntity, "Waypoints not loading!\n", MType::msg_warning);

				// don't print this on user command "wpt load", because in that case we don't autoconvert them
				if (pEntity == NULL)
				{
					conOutput.Print(pEntity, "Auto conversion started...\n", MType::msg_info);
				}

				fclose(bfp);
				return -1;		// to start auto waypoint conversion
			}

			header.mapname[31] = 0;

			if ((strcmp(header.mapname, STRING(gpGlobals->mapname)) == 0) || (custom_filename != NULL))
			{
				// remove any existing waypoints
				InitAll();

				for (int i = 0; i < header.number_of_waypoints; i++)
				{
					fread(&waypoints[i], sizeof(waypoints[0]), 1, bfp);
					num_waypoints++;

					// fix some bugs that might happen

					// fix some problem waypoint tag/flag combinations ...
					wptfixer.RepairInvalidCombinationOfWaypointFlags(i);
				}

				// read the triggers
				for (int index = 0; index < MAX_TRIGGERS; index++)
				{
					fread(&trigger_events[index], sizeof(trigger_events[0]), 1, bfp);

					// we have to mark this slot as a used
					if (strlen(trigger_events[index].message) > 1)
						trigger_gamestate[index].SetUsed(true);
				}

				// read the neighbors
				for(int i = 0; i < header.number_of_waypoints; i++) {
					if(fread(&num_neighbors[i], sizeof(short), 1, bfp) != 1)
						break;

					if(!num_neighbors[i])
						continue;

					neighbors[i] = (short*)malloc(num_neighbors[i] * sizeof(short));
					fread(neighbors[i], sizeof(short), num_neighbors[i], bfp);
				}
				// compute inverse neighbors
				for(int i = 0; i < header.number_of_waypoints; i++) {
					for(int j = 0; j < header.number_of_waypoints; j++)
						for(int jNeighbor = 0; jNeighbor < num_neighbors[j]; jNeighbor++) 
							if(neighbors[j][jNeighbor] == i)
								num_inv_neighbors[i]++;

					inv_neighbors[i] = (short*)malloc(num_inv_neighbors[i] * sizeof(short));
					int iNeighbor = 0;
					for(int j = 0; j < header.number_of_waypoints; j++)
						for(int jNeighbor = 0; jNeighbor < num_neighbors[j]; jNeighbor++)
							if(neighbors[j][jNeighbor] == i)
								inv_neighbors[i][iNeighbor++] = j;
				}
			}
			else
			{
				sprintf(msg, "Waypoints are not for this map: %s\n", filename);
				conOutput.Print(pEntity, msg, MType::msg_warning);

				// don't print this while map loads
				if (pEntity)
					conOutput.Print(pEntity, wpt_warning, MType::msg_info);

				fclose(bfp);
				return 0;
			}

			// get waypoint's signatures
			header.author[31] = 0;
			if (strcmp(header.author, "") == 0)
				strcpy(wpt_author, "unknown");
			else
				strcpy(wpt_author, header.author);

			header.modified_by[31] = 0;
			if (strcmp(header.modified_by, "") == 0)
				strcpy(wpt_modified, "unknown");
			else
				strcpy(wpt_modified, header.modified_by);
		}
		else
		{
			sprintf(msg, "Not a MarineBot waypoint file: %s\n", filename);
			conOutput.Print(pEntity, msg, MType::msg_error);

			if (pEntity)
				conOutput.Print(pEntity, wpt_warning, MType::msg_info);

			fclose(bfp);
			return 0;
		}

		fclose(bfp);
	}
	else
	{
		sprintf(msg, "No waypoint file: %s\n", filename);
		conOutput.Print(pEntity, msg, MType::msg_error);

		if (pEntity)
			conOutput.Print(pEntity, wpt_warning, MType::msg_info);

		return -10;
	}

	return 1;
}


/*
* loads only some waypoint data (based on waypoint version) to convert older waypoints to the latest (actual) version
*/
bool waypoint_editing_functions_t::LoadUnsupportedWaypoints(edict_t* pEntity)
{
	extern bool is_dedicated_server;

	char mapname[64];
	char filename[256];
	//OLD_WPT_HDR header;
	WAYPOINT_HDR header;		// can be used current header until any change is done
	char msg[256];
	int index;
	OLD_WAYPOINT oldwaypoints_ver[1];	// we need just one slot because we are converting them one by one
	TRIGGER_EVENT old_triggers[1]{};	// we can use current trigger event structure, because there is no difference between version 7 and 8
	bool known = false;

	strcpy(mapname, STRING(gpGlobals->mapname));
	strcat(mapname, ".wpt");

	if (internals.IsCustomWaypoints())
		util.MarineBotFileName(filename, "customwpts", mapname);
	else
		util.MarineBotFileName(filename, "defaultwpts", mapname);

	if (is_dedicated_server)
	{
		sprintf(msg, "loading unsupported waypoints from: %s\n", filename);

		conOutput.Print(NULL, msg, MType::msg_info);
	}

	FILE* bfp = fopen(filename, "rb");

	// if file exists, read the waypoint structure from it
	if (bfp != NULL)
	{
		fread(&header, sizeof(header), 1, bfp);

		header.filetype[7] = 0;
		if (strcmp(header.filetype, "FAM_bot") == 0)
		{
			if (header.waypoint_file_version == WAYPOINT_VERSION)
			{
				conOutput.Print(pEntity, "This waypoint file isn't outdated. No need to load it this way.\n", MType::msg_info);

				fclose(bfp);
				return false;
			}

			if (header.waypoint_file_version != WAYPOINT_VERSION)
			{
				known = false;

				// convert only waypoints that are one version back
				if (header.waypoint_file_version == OLD_WAYPOINT_VERSION)
				{
					// this is known older waypoint system
					known = true;

					sprintf(msg, "found known older MarineBot waypoint file (version %d - MB0.91b up to 0.95b) - conversion in progress...\n", OLD_WAYPOINT_VERSION);
					conOutput.Print(pEntity, msg, MType::msg_info);
				}
				// otherwise ignore all other (older) waypoint versions
				else
				{
					conOutput.Print(pEntity, "unknown waypoints data (probably too old) - conversion failed!\n", MType::msg_error);

					fclose(bfp);
					return false;
				}
			}

			header.mapname[31] = 0;

			if (strcmp(header.mapname, STRING(gpGlobals->mapname)) == 0)
			{
				// remove any existing waypoints
				InitAll();

				conOutput.Print(pEntity, "Loading converted waypoint file...\n", MType::msg_info);

				for (index = 0; index < header.number_of_waypoints; index++)
				{
					// init all new values to default at first, this is being done a few lines above, but we do it again just to be completely sure
					InitThisWaypoint(index);

					// just for sure, we don't want to load too old waypoints, because those might have a lot different structure so then we'd be unable to read correct data from the file
					if (known)
					{
						// read all data for this waypoint
						fread(&oldwaypoints_ver[0], sizeof(oldwaypoints_ver[0]), 1, bfp);
						num_waypoints++;

						waypoints[index].origin = oldwaypoints_ver[0].origin;

						// to prevent unknown waypoint flag which cause weird things like aim & cross connections going to map origin etc.
						if (oldwaypoints_ver[0].flags == 0)
							waypoints[index].flags |= W_FL_DELETED;
						// otherwise use stored flag
						else
							waypoints[index].flags = oldwaypoints_ver[0].flags;

						waypoints[index].red_priority = oldwaypoints_ver[0].red_priority;
						waypoints[index].blue_priority = oldwaypoints_ver[0].blue_priority;
						waypoints[index].red_time = oldwaypoints_ver[0].red_time;
						waypoints[index].blue_time = oldwaypoints_ver[0].blue_time;
						waypoints[index].trigger_red_priority = oldwaypoints_ver[0].trigger_red_priority;
						waypoints[index].trigger_blue_priority = oldwaypoints_ver[0].trigger_blue_priority;
						waypoints[index].trigger_event_on = IntToTriggerId(oldwaypoints_ver[0].trigger_event_on);	// version 7 used to store triggers as int value
						waypoints[index].trigger_event_off = IntToTriggerId(oldwaypoints_ver[0].trigger_event_off);	// so we must convert it to ID now
						waypoints[index].range = oldwaypoints_ver[0].range;
					}

					// fix possible problems with special waypoint types
					if ((waypoints[index].flags & W_FL_AIMING) && (waypoints[index].range != 0.0f))
						waypoints[index].range = 0.0f;

					if ((waypoints[index].flags & RANGE_20_WPT) && (waypoints[index].range > WPT_RANGE_SMALL))
						waypoints[index].range = WPT_RANGE_SMALL;

					// reset range to default if it is too big
					if (waypoints[index].range > MAX_WPT_DIST)
						waypoints[index].range = WPT_RANGE;

					// fix some problem waypoint tag/flag combinations ...
					wptfixer.RepairInvalidCombinationOfWaypointFlags(index);

					// finaly convert old/obsolete things!!! -->> There's nothing to be converted now (i.e. no critical changes between version 7 and 8)

					// well except for missing bandages flags/tags, but we can't do it here, because if the conversion is called automatically then there are no entities yet so
					// we just let the system know that there's an unfinished task and it will be done once the map is fully loaded
					internals.SetIsWaypointConversionUnfinished();
				}

				// read & convert the triggers
				for (int index = 0; index < MAX_TRIGGERS; index++)
				{
					fread(&old_triggers[0], sizeof(old_triggers[0]), 1, bfp);

					// actually there's no conversion needed, because version 7 and 8 match, so we just copy the data
					trigger_events[index].name = old_triggers[0].name;
					strcpy(trigger_events[index].message, old_triggers[0].message);

					// we have to mark this slot as a used
					if (strlen(trigger_events[index].message) > 1)
						trigger_gamestate[index].SetUsed(true);
				}

				// fix missing signatures
				header.author[31] = 0;
				if (strcmp(header.author, "") == 0)
					strcpy(wpt_author, "unknown");
				else
					strcpy(wpt_author, header.author);

				header.modified_by[31] = 0;
				if (strcmp(header.modified_by, "") == 0)
					strcpy(wpt_modified, "unknown");
				else
					strcpy(wpt_modified, header.modified_by);

			}
			else
			{
				sprintf(msg, "MarineBot waypoints are not for this map: %s\n", filename);
				conOutput.Print(pEntity, msg, MType::msg_warning);
				conOutput.Print(pEntity, wpt_warning, MType::msg_info);

				fclose(bfp);
				return false;
			}
		}
		else
		{
			sprintf(msg, "Not a MarineBot waypoint file: %s\n", filename);
			conOutput.Print(pEntity, msg, MType::msg_warning);
			conOutput.Print(pEntity, wpt_warning, MType::msg_info);

			fclose(bfp);
			return false;
		}

		fclose(bfp);
	}
	else
	{
		sprintf(msg, "No waypoint file: %s\n", filename);
		conOutput.Print(pEntity, msg, MType::msg_warning);
		conOutput.Print(pEntity, wpt_warning, MType::msg_info);

		return false;
	}

	return true;
}


/*
* loads and converts old waypoints in version 6 (used in MB0.9) to the latest version
* there are many waypoints available in this even older system and we can still use them without bigger issues
*/
bool waypoint_editing_functions_t::LoadUnsupportedWaypointsVersion6(edict_t* pEntity)
{
	char mapname[64];
	char filename[256];
	WAYPOINT_HDR header;		// can be used current header until any change is done
	char msg[256];
	int index;
	OLD_WAYPOINT oldwaypoints_ver[1];	// we need just one slot because we are converting them one by one
	TRIGGER_EVENT_OLD old_triggers[1]{};
	int OLD_WAYPOINT_VERSION_6 = 6;		// we have to "override" standard conversion by sending even older version number
	bool known = false;

	strcpy(mapname, STRING(gpGlobals->mapname));
	strcat(mapname, ".wpt");

	if (internals.IsCustomWaypoints())
		util.MarineBotFileName(filename, "customwpts", mapname);
	else
		util.MarineBotFileName(filename, "defaultwpts", mapname);

	FILE* bfp = fopen(filename, "rb");

	// if file exists, read the waypoint structure from it
	if (bfp != NULL)
	{
		fread(&header, sizeof(header), 1, bfp);

		header.filetype[7] = 0;
		if (strcmp(header.filetype, "FAM_bot") == 0)
		{
			if (header.waypoint_file_version == WAYPOINT_VERSION)
			{
				conOutput.Print(pEntity, "This waypoint file isn't outdated. No need to load it this way.\n", MType::msg_info);

				fclose(bfp);
				return false;
			}

			if (header.waypoint_file_version != WAYPOINT_VERSION)
			{
				known = false;

				// convert only waypoints that match this version
				if (header.waypoint_file_version == OLD_WAYPOINT_VERSION_6)
				{
					// this is known older waypoint system
					known = true;

					sprintf(msg, "found known older MarineBot waypoint file (version %d - MB0.9b) - conversion in progress...\n", OLD_WAYPOINT_VERSION_6);
					conOutput.Print(pEntity, msg, MType::msg_info);
				}
				// otherwise ignore all other waypoint versions
				else
				{
					sprintf(msg, "unknown waypoints data (don't match version %d) - conversion failed!\n", OLD_WAYPOINT_VERSION_6);
					conOutput.Print(pEntity, msg, MType::msg_error);

					fclose(bfp);
					return false;
				}
			}

			header.mapname[31] = 0;

			if (strcmp(header.mapname, STRING(gpGlobals->mapname)) == 0)
			{
				// remove any existing waypoints
				InitAll();

				conOutput.Print(pEntity, "Loading converted waypoint file...\n", MType::msg_info);

				for (index = 0; index < header.number_of_waypoints; index++)
				{
					InitThisWaypoint(index);

					if (known)
					{
						// read all data for this waypoint
						fread(&oldwaypoints_ver[0], sizeof(oldwaypoints_ver[0]), 1, bfp);
						num_waypoints++;

						waypoints[index].origin = oldwaypoints_ver[0].origin;

						// to prevent unknown waypoint flag which cause weird things like aim & cross connections going to map origin etc.
						if (oldwaypoints_ver[0].flags == 0)
							waypoints[index].flags |= W_FL_DELETED;
						// otherwise use stored flag
						else
							waypoints[index].flags = oldwaypoints_ver[0].flags;

						waypoints[index].red_priority = oldwaypoints_ver[0].red_priority;
						waypoints[index].blue_priority = oldwaypoints_ver[0].blue_priority;
						waypoints[index].red_time = oldwaypoints_ver[0].red_time;
						waypoints[index].blue_time = oldwaypoints_ver[0].blue_time;
						waypoints[index].trigger_red_priority = oldwaypoints_ver[0].trigger_red_priority;
						waypoints[index].trigger_blue_priority = oldwaypoints_ver[0].trigger_blue_priority;
						waypoints[index].trigger_event_on = IntToTriggerId(oldwaypoints_ver[0].trigger_event_on);	// conversion from int value to TriggerId is needed, because
						waypoints[index].trigger_event_off = IntToTriggerId(oldwaypoints_ver[0].trigger_event_off);	// current version works directly with TriggerIds
						waypoints[index].range = oldwaypoints_ver[0].range;
					}

					// fix possible problems with special waypoint types
					if ((waypoints[index].flags & W_FL_AIMING) && (waypoints[index].range != 0.0f))
						waypoints[index].range = 0.0f;

					if ((waypoints[index].flags & RANGE_20_WPT) && (waypoints[index].range > WPT_RANGE_SMALL))
						waypoints[index].range = WPT_RANGE_SMALL;

					// reset range to default if it is too big
					if (waypoints[index].range > MAX_WPT_DIST)
						waypoints[index].range = WPT_RANGE;

					// fix some problem waypoint tag/flag combinations ...
					wptfixer.RepairInvalidCombinationOfWaypointFlags(index);

					// finaly convert old/obsolete things!!! -->> There's nothing to be converted now (i.e. no critical changes between these versions)

					// except for missing bandages tags, but that needs to be done elsewhere
					internals.SetIsWaypointConversionUnfinished();
				}

				// read & convert the triggers
				for (index = 0; index < MAX_TRIGGERS; index++)
				{
					// the version 6 triggers used different structure so we have to convert them to version 8 system

					fread(&old_triggers[0], sizeof(old_triggers[0]), 1, bfp);

					// copy just these two values, ignoring the other two
					trigger_events[index].name = old_triggers[0].name;
					strcpy(trigger_events[index].message, old_triggers[0].message);

					// we have to mark this slot as a used
					if (strlen(trigger_events[index].message) > 1)
						trigger_gamestate[index].SetUsed(true);
				}

				// fix missing signatures
				header.author[31] = 0;
				if (strcmp(header.author, "") == 0)
					strcpy(wpt_author, "unknown");
				else
					strcpy(wpt_author, header.author);

				header.modified_by[31] = 0;
				if (strcmp(header.modified_by, "") == 0)
					strcpy(wpt_modified, "unknown");
				else
					strcpy(wpt_modified, header.modified_by);

			}
			else
			{
				sprintf(msg, "MarineBot waypoints are not for this map: %s\n", filename);
				conOutput.Print(pEntity, msg, MType::msg_warning);
				conOutput.Print(pEntity, wpt_warning, MType::msg_info);

				fclose(bfp);
				return false;
			}
		}
		else
		{
			sprintf(msg, "Not a MarineBot waypoint file: %s\n", filename);
			conOutput.Print(pEntity, msg, MType::msg_warning);
			conOutput.Print(pEntity, wpt_warning, MType::msg_info);

			fclose(bfp);
			return false;
		}

		fclose(bfp);
	}
	else
	{
		sprintf(msg, "No waypoint file: %s\n", filename);
		conOutput.Print(pEntity, msg, MType::msg_warning);
		conOutput.Print(pEntity, wpt_warning, MType::msg_info);

		return false;
	}

	return true;
}


/*
*/
bool waypoint_editing_functions_t::LoadFirearmsWaypoints(edict_t* pEntity, const char* custom_filename)
{
	char mapname[64]{};
	char filename[256]{};
	WAYPOINT_HDR header{};
	char msg[256]{};

	strcpy(mapname, custom_filename);
	strcat(mapname, ".wpt");

	if (internals.IsCustomWaypoints())
		util.MarineBotFileName(filename, "customwpts", mapname);
	else
		util.MarineBotFileName(filename, "defaultwpts", mapname);

	FILE* bfp = fopen(filename, "rb");

	// if file exists, read the waypoint structure from it
	if (bfp != NULL)
	{
		fread(&header, sizeof(header), 1, bfp);

		header.filetype[7] = 0;
		if (strcmp(header.filetype, "FAM_bot") == 0)
		{
			if (header.waypoint_file_version != WAYPOINT_VERSION)
			{
				sprintf(msg, "Outdated waypoint file version: %d (current waypoint system version: %d)\n", header.waypoint_file_version, WAYPOINT_VERSION);
				conOutput.Print(pEntity, msg, MType::msg_error);

				conOutput.Print(pEntity, "Waypoints not loading!\n", MType::msg_warning);

				fclose(bfp);
				return false;
			}

			header.mapname[31] = 0;

			if (strcmp(header.mapname, custom_filename) == 0)
			{
				// remove any existing waypoints
				InitAll();

				for (int i = 0; i < header.number_of_waypoints; i++)
				{
					fread(&waypoints[i], sizeof(waypoints[0]), 1, bfp);
					num_waypoints++;

					// remove nonexistent waypoint flags (well the flags do exist, but there is no ammobox, bandage, claymore mine or parachute entity in DoD)
					if (waypoints[i].flags & W_FL_AMMOBOX)
					{
						waypoints[i].flags &= ~W_FL_AMMOBOX;
					}

					if (waypoints[i].flags & W_FL_BANDAGE)
					{
						waypoints[i].flags &= ~W_FL_BANDAGE;
					}
					
					if (waypoints[i].flags & W_FL_CHUTE)
					{
						waypoints[i].flags &= ~W_FL_CHUTE;
					}
					
					if (waypoints[i].flags & W_FL_MINE)
					{
						waypoints[i].flags &= ~W_FL_MINE;
					}

					// fix some problem waypoint tag/flag combinations ...
					wptfixer.RepairInvalidCombinationOfWaypointFlags(i);
				}

				// read the triggers
				for (int index = 0; index < MAX_TRIGGERS; index++)
				{
					fread(&trigger_events[index], sizeof(trigger_events[0]), 1, bfp);

					// we have to mark this slot as a used
					if (strlen(trigger_events[index].message) > 1)
						trigger_gamestate[index].SetUsed(true);
				}
			}
			else
			{
				sprintf(msg, "Waypoints are not for this map: %s\n", filename);
				conOutput.Print(pEntity, msg, MType::msg_warning);

				fclose(bfp);
				return false;
			}

			// get waypoints signatures
			header.author[31] = 0;
			if (strcmp(header.author, "") == 0)
				strcpy(wpt_author, "unknown");
			else
				strcpy(wpt_author, header.author);

			header.modified_by[31] = 0;
			if (strcmp(header.modified_by, "") == 0)
				strcpy(wpt_modified, "unknown");
			else
				strcpy(wpt_modified, header.modified_by);
		}
		else
		{
			sprintf(msg, "Not a MarineBot waypoint file: %s\n", filename);
			conOutput.Print(pEntity, msg, MType::msg_error);

			fclose(bfp);
			return false;
		}

		fclose(bfp);
	}
	else
	{
		sprintf(msg, "No waypoint file: %s\n", filename);
		conOutput.Print(pEntity, msg, MType::msg_error);

		return false;
	}

	return true;
}


/*
* calls functions needed to update waypoint properties that cannot be run inside the conversion function due to missing data about map entities etc.
*/
void waypoint_editing_functions_t::FinalizeWaypointConversion(void)
{
	// go through all waypoints and ...
	for (int index = 0; index < num_waypoints; index++)
	{
		// waypoint systems preceding the version 8 didn't work with bandage entity so we have to patch that
		DetectBandagesAroundWaypoint(index);
	}
}


/*
* initializes the waypoint (ie. one slot in the array of all waypoints)
*/
void waypoint_editing_functions_t::InitThisWaypoint(int wpt_index)
{
	waypoints[wpt_index].flags = W_FL_DELETED;
	waypoints[wpt_index].red_priority = MAX_WPT_PRIOR;									// lowest priority
	waypoints[wpt_index].red_time = 0.0f;												// no "wait at this waypoint" time
	waypoints[wpt_index].blue_priority = MAX_WPT_PRIOR;
	waypoints[wpt_index].blue_time = 0.0f;
	waypoints[wpt_index].trigger_red_priority = MAX_WPT_PRIOR;
	waypoints[wpt_index].trigger_blue_priority = MAX_WPT_PRIOR;
	waypoints[wpt_index].trigger_event_on = TriggerId::trigger_none;					// no trigger event
	waypoints[wpt_index].trigger_event_off = TriggerId::trigger_none;
	waypoints[wpt_index].range = WPT_RANGE;												// default range
	waypoints[wpt_index].origin = g_vecZero;											// located at map origin
}


/*
* checks the vicinity of given waypoint whether there are bandages to add bandages tag on it (or remove it)
*/
void waypoint_editing_functions_t::DetectBandagesAroundWaypoint(int wpt_index)
{
	if ((wpt_index < 0) || (wpt_index > num_waypoints))
		return;

	edict_t* pent = NULL;

	while ((pent = util.FindEntityInSphere(pent, waypoints[wpt_index].origin, WPT_BANDAGE_RANGE)) != NULL)
	{
		if (strcmp(STRING(pent->v.classname), "item_bandage") == 0)
		{
			waypoints[wpt_index].flags |= W_FL_BANDAGE;

			// set the range to a value specific for this type if it is larger, because you can't take them from further away
			if (waypoints[wpt_index].range > WPT_BANDAGE_RANGE)
				waypoints[wpt_index].range = WPT_BANDAGE_RANGE;

			return;
		}
	}

	// we haven't found bandages nearby so we have to remove this flag
	if (waypoints[wpt_index].flags & W_FL_BANDAGE)
		waypoints[wpt_index].flags &= ~W_FL_BANDAGE;
}


/*
* starts new path on given or the nearest waypoint to player position
* returns TRUE if path was successfully started
* return FALSE if not (no waypoint close or path cannot be created)
*/
bool path_editing_functions_t::Create(edict_t* pEntity, int wpt_index)
{
	if (wpt_index == NO_VAL)
	{
		wpt_index = wptmanager.FindNearestWaypointToPlayer(pEntity);

		// here could be detailed print for some sort of debugging
	}

	if ((wpt_index != NO_VAL) && StartNewPath(wpt_index))
		return true;

	return false;
}


/*
* stops pointing to current path (clears the pointer to it)
* also runs the self repairing functions on this path and calls the auto tagging (eg. ammobox detection) for this path
* returns TRUE only if there was any path finished
*/
bool path_editing_functions_t::Finish(edict_t* pEntity)
{
	// is any path currenly in edit
	if (internals.IsPathToContinue())
	{
		int path_validity;

		path_validity = wptfixer.ValidatePath(internals.GetPathToContinue());

		if (path_validity == 1)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "there was an error in this path that has been automatically fixed\n");
		else if (path_validity == -1)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "there is an error in this path that can not be fixed automatically\n");

		// also update this path status
		wptfixer.UpdatePathStatus(internals.GetPathToContinue());

		// we aren't going to edit this path anymore so we must reset the pointer
		internals.ResetPathToContinue();

		// reset paths display time to sync them all together
		f_path_time = 0.0f;

		return true;
	}

	return false;
}


/*
* continues in path specified in path_index (sets the pointer to it again)
* if no path_index is specified then it'll search for the nearest waypoint to check if there is any path on it
* (takes the first path that is found)
*/
bool path_editing_functions_t::Continue(edict_t* pEntity, int path_index)
{
	// if path isn't specified try finding one on nearby waypoint
	if (path_index == NO_VAL)
	{
		int nearby_waypoint = NO_VAL;

		nearby_waypoint = wptmanager.FindNearestWaypointToPlayer(pEntity);

		if (nearby_waypoint == NO_VAL)
			return FALSE;

		// get the first path on this waypoint 
		path_index = wptmanager.FindPath(nearby_waypoint);
	}

	// is the path valid
	if ((path_index != NO_VAL) && (w_paths[path_index] != NULL))
	{
		// set the pointer to it
		internals.SetPathToContinue(path_index);

		return true;
	}

	return false;
}


/*
* deletes the whole path specified in path_index
* or path that is on the nearest waypoint (the first path that is found)
*/
bool path_editing_functions_t::Delete(edict_t* pEntity, int path_index)
{
	// if path isn't specified try to find one
	if (path_index == NO_VAL)
	{
		int nearby_waypoint = NO_VAL;

		nearby_waypoint = wptmanager.FindNearestWaypointToPlayer(pEntity);

		if (nearby_waypoint == NO_VAL)
			return false;

		// find the first path on this waypoint
		path_index = wptmanager.FindPath(nearby_waypoint);
	}

	if (DeleteWholePath(path_index))
		return true;

	return false;
}


/*
* adds given or the nearest waypoint to currently edited path (appends to the end of the path)
*/
bool path_editing_functions_t::AddWaypoint(edict_t* pEntity, int wpt_index)
{
	if (wpt_index == NO_VAL)
	{
		// if automatic additions to path is active then the waypoint must be really close
		if (wptser.IsAutoAddToPath())
			wpt_index = wptmanager.FindNearestWaypointToPlayer(pEntity, AUTOADD_DISTANCE);
		// otherwise use standard distance
		else
			wpt_index = wptmanager.FindNearestWaypointToPlayer(pEntity);
	}

	if ((wpt_index != NO_VAL) && (ContinueCurrPath(wpt_index) == 1))
		return true;

	return false;
}


/*
* inserts a waypoint specified via new_wpt into path as to_path between its two waypoints in arg4 and arg5
* allows also insertion to the beginning or end of the path through keyword passed in arg4 or arg5
*/
int path_editing_functions_t::InsertWaypoint(const char* new_wpt, const char* to_path, const char* arg4, const char* arg5)
{
	int wpt_index, path_index, path_wpt1, path_wpt2;
	W_PATH* p;
	int special_case;		// holds 1 when we are inserting waypoint right to the start of the path
	// holds 2 when we are inserting waypoint to the end of the path
	// holds 0 when the waypoint is inserted 'into' path (between its two waypoints)

// init all values
	wpt_index = path_index = path_wpt1 = path_wpt2 = NO_VAL;
	special_case = 0;										// we assume the insertion is 'into' the path by default

	// get the index of waypoint we want to insert if exist
	if (conInput.IsValidWaypointIndex(new_wpt))
		wpt_index = conInput.GetValidIndex();

	// is the waypoint for insertion valid (ie. not one of those that cannot be added to a path)
	if ((wpt_index == NO_VAL) || (waypoints[wpt_index].flags == 0) ||
		wptmanager.IsWaypoint(wpt_index, WptT::aim, WptT::cross, WptT::deleted))
		return -4;

	// get the index of path which we want to insert into if exist
	if (conInput.IsValidPathIndex(to_path))
		path_index = conInput.GetValidIndex();

	// does the path exist or
	// has the path less then two waypoints (should not happen, but just for sure) in case of common insertion
	if ((path_index == NO_VAL) || (w_paths[path_index] == NULL) ||
		((wptmanager.GetPathLength(path_index) < 2) && (special_case == 0)))
		return -3;

	// get the index of waypoint we want to insert behind if exist
	if ((arg4 != NULL) && (*arg4 != 0))
	{
		if (FStrEq(arg4, "start") || FStrEq(arg4, "beginning") || FStrEq(arg4, "tostart") || FStrEq(arg4, "to_start") ||
			FStrEq(arg4, "tobeginning") || FStrEq(arg4, "to_beginning"))
			special_case = 1;
		else if (FStrEq(arg4, "end") || FStrEq(arg4, "toend") || FStrEq(arg4, "to_end"))
			special_case = 2;
		else
		{
			if (conInput.IsValidWaypointIndex(arg4))
				path_wpt1 = conInput.GetValidIndex();

			if ((path_wpt1 == NO_VAL) || wptmanager.IsWaypoint(path_wpt1, WptT::aim, WptT::cross, WptT::deleted))
				return -2;
		}
	}

	// get the index of waypoint we want to insert before if exist
	if ((special_case == 0) && (arg5 != NULL) && (*arg5 != 0))
	{
		if (FStrEq(arg5, "start") || FStrEq(arg5, "beginning") || FStrEq(arg5, "tostart") || FStrEq(arg5, "to_start") ||
			FStrEq(arg5, "tobeginning") || FStrEq(arg5, "to_beginning"))
			special_case = 1;
		else if (FStrEq(arg5, "end") || FStrEq(arg5, "toend") || FStrEq(arg5, "to_end"))
			special_case = 2;
		else
		{
			if (conInput.IsValidWaypointIndex(arg5))
				path_wpt2 = conInput.GetValidIndex();

			if ((path_wpt2 == NO_VAL) || wptmanager.IsWaypoint(path_wpt2, WptT::aim, WptT::cross, WptT::deleted))
				return -1;
		}
	}

	char msg[128];

	if (special_case == 1)
		sprintf(msg, "Trying to insert waypoint #%d to the beginning of path #%d ...\n", wpt_index + 1, path_index + 1);
	else if (special_case == 2)
		sprintf(msg, "Trying to insert waypoint #%d to the end of path #%d ...\n", wpt_index + 1, path_index + 1);
	else
		sprintf(msg, "Trying to insert waypoint #%d into path #%d between wpts #%d and #%d ...\n",
			wpt_index + 1, path_index + 1, path_wpt1 + 1, path_wpt2 + 1);

	conOutput.Print(NULL, msg);

	// is one of path waypoints same to the waypoint for insertion
	if ((wpt_index == path_wpt1) || (wpt_index == path_wpt2))
		return -6;

	// are both path waypoints same and NOT special insertion and were they given at all
	if ((path_wpt1 == path_wpt2) && (special_case == 0) && (path_wpt1 != NO_VAL))
		return -5;

	// waypoint already is in this path
	if (wptmanager.IsWaypointOnPath(wpt_index, path_index))
		return -6;

	// if we are inserting 'into' path (ie. between its two waypoints) then we must check their validity
	if ((special_case == 0) && (wptmanager.IsWaypointOnPath(path_wpt1, path_index) == FALSE))
		return -2;

	// if we are inserting 'into' path (ie. between its two waypoints) then we must check their validity
	if ((special_case == 0) && (wptmanager.IsWaypointOnPath(path_wpt2, path_index) == FALSE))
		return -1;

	// if we are inserting the waypoint at the beginning or end of the path
	// then just point at the path in the array of all paths
	if (special_case != 0)
		p = w_paths[path_index];
	// otherwise we must find one of the two path waypoints to get a pointer on it
	else
		p = wptmanager.GetWaypointPointer(path_wpt1, path_index);

	// check if the pointer exists
	if (p)
	{
		W_PATH* next;
		W_PATH* new_node = NULL;	// a node for the waypoint we need to insert

		// first we will handle the case when we are inserting new waypoint at the beginning of the path
		if (special_case == 1)
		{
			new_node = (W_PATH*)malloc(sizeof(W_PATH));	// create new node

			if (new_node == NULL)
			{
				ALERT(at_error, "MarineBot - Error allocating memory for path!\n");
				return 0;		// no memory for path
			}

			p->prev = new_node;		// put the new node in front of the first path waypoint
			new_node->next = p;		// and connect the rest of the path behind this new start point
			new_node->prev = NULL;	// NULL this pointer, cause this node is the head node so there's nothing before it

			new_node->wpt_index = wpt_index;	// next step is to store the waypoint index into the new node

			// this path has a new start point now so let's update the array of all paths
			w_paths[path_index] = new_node;

			// finally we must also update the path data
			w_paths[path_index]->flags = p->flags;

			return 1;
		}
		// then we will handle the second special insertion ... at the end of the path
		else if (special_case == 2)
		{
			W_PATH* prev_node = NULL;		// temp pointer to previous node (ie. the end of our path)
			int safety_stop = 0;

			// first we must go through the path in order to reach its end point ...
			while (p)
			{
				prev_node = p;				// save the previous node in linked list
				p = p->next;				// go to next node in linked list (ie. go through the path)

				safety_stop++;
				if (safety_stop > LINKEDLIST_LOOPS_THRESHOLD)
					LinkedListError("Insert Waypoint", path_index);
			}

			new_node = (W_PATH*)malloc(sizeof(W_PATH));		// create new node

			if (new_node == NULL)
			{
				ALERT(at_error, "MarineBot - Error allocating memory for path!\n");
				return 0;
			}

			new_node->wpt_index = wpt_index;		// store new waypoint
			new_node->next = NULL;					// NULL next node pointer, cause we are at the end of the path

			if (prev_node != NULL)
			{
				// and finally connect the new node to the end of the path
				new_node->prev = prev_node;
				prev_node->next = new_node;

				return 1;
			}

			// seeing the pointer to prev node is NULL we weren't able to go through the path to reach its end point so
			// we must end it here with unknown error
			return 0;
		}

		// this is the default case where we are inserting new waypoint 'into' the path (between its two waypoints)
		// so first go to next waypoint in the path ...
		next = p->next;

		// does the next waypoint exist AND
		// are first and second path waypoints really neighbours (ie. is this waypoint index the one we are looking for)
		if (next && (next->wpt_index == path_wpt2))
		{
			// so insert the new waypoint behind path_wpt1...

			new_node = (W_PATH*)malloc(sizeof(W_PATH));

			if (new_node == NULL)
			{
				ALERT(at_error, "MarineBot - Error allocating memory for path!\n");
				return 0;
			}

			p->next = new_node;		// connect the new node behind path waypoint1
			new_node->prev = p;		// set also the opposite connection

			new_node->wpt_index = wpt_index;

			new_node->next = next;	// connect the other end of the path to this new node
			next->prev = new_node;	// set also the opposite connection

			return 1;
		}

		// next waypoint wasn't the second path waypoint we're looking for so try the previous waypoint
		// doing it this way gives the end-user freedom ... the order of the last two arguments doesn't matter now,
		// because command 'pathwpt insert 7 20 5 6' is the same as 'pathwpt insert 7 20 6 5'
		next = p->prev;

		// does the previous waypoint exist AND is it the one we are looking for
		if (next && (next->wpt_index == path_wpt2))
		{
			// so insert the new waypoint before path_wpt1...

			new_node = (W_PATH*)malloc(sizeof(W_PATH));

			if (new_node == NULL)
			{
				ALERT(at_error, "MarineBot - Error allocating memory for path!\n");
				return 0;
			}

			p->prev = new_node;
			new_node->next = p;

			new_node->wpt_index = wpt_index;

			new_node->prev = next;
			next->next = new_node;

			return 1;
		}
	}

	return 0;
}


/*
* removes specified waypoint from specified or currently edited path
* or if no argument is given then it tries to find nearby waypoint to remove it from the path that is currently in edit or from the path that is on this waypoint
*/
bool path_editing_functions_t::RemoveWaypoint(edict_t* pEntity, int wpt_index, int from_path_index)
{
	// no waypoint given?
	if (wpt_index == NO_VAL)
	{
		// then look for one nearby
		wpt_index = wptmanager.FindNearestWaypointToPlayer(pEntity);

		if (wpt_index == NO_VAL)
			return false;
	}
	// do we have just the waypoint, but path index wasn't specified? then return error, because there has to be either both arguments or none
	else if (conInput.IsMissingArgument())
		return false;

	// no path given?
	if (from_path_index == NO_VAL)
	{
		// then try to use actual path (ie. the one currently in edit)
		from_path_index = internals.GetPathToContinue();

		// if still no path? then try to find any path on the waypoint
		if (from_path_index == NO_VAL)
			from_path_index = wptmanager.FindPath(wpt_index);
	}

	if ((from_path_index != NO_VAL) && ExcludeFromPath(wpt_index, from_path_index))
		return true;

	return false;
}


/*
* splits path into two parts, either by given path index via this_path or the first found path on given waypoint via on_wpt or on nearby waypoint if on_wpt is NULL
* returns the index of the new path if we successfully split original path
* returns -1 if not close to any wpt or there's no path on it
* returns -2 if the path is too short (ie. path has less than 4 wpts -> new paths after the split must have at least 2 wpts each)
* returns -3 if the waypoint isn't in this path
* returns -4 if we can't split the path at this position (ie. the waypoint is its starting point or ending point or is at the penultimate position in the path)
* returns -5 if we are unable to start a new path (most probably reached the max. amount of paths)
* returns -6 if something went wrong when shuffling the waypoints between the paths or if either of the path offsprings didn't pass validation
* returns -7 if the path doesn't exist or was deleted
* returns -8 if the waypoint doesn't exist or was deleted
*/
int path_editing_functions_t::Split(edict_t* pEntity, const char* this_path, const char* on_wpt)
{
	int path_index = NO_VAL;
	int wpt_index = NO_VAL;
	int path_length = 0;
	int min_length = 5;		// min length of the path to allow the split
	W_PATH* p = NULL;

	// get path index from argument
	if (conInput.IsValidPathIndex(this_path))
	{
		// this already is an array index, because
		// the conversion is done right in IsValidPathIndex() function
		path_index = conInput.GetValidIndex();

		// return error if such index doesn't exist or it's a deleted path
		if ((path_index == NO_VAL) || (w_paths[path_index] == NULL))
			return -7;

		path_length = wptmanager.GetPathLength(path_index);

		// return error if the path is too short to create two valid path "offsprings" after we divide it
		if ((path_length > 0) && (path_length < min_length))
			return -2;
	}

	// get waypoint index from argument
	if (conInput.IsValidWaypointIndex(on_wpt))
	{
		wpt_index = conInput.GetValidIndex();

		// return error if such waypoint doesn't exist or was deleted
		if ((wpt_index < 0) || (wpt_index > num_waypoints) || wptmanager.IsWaypoint(wpt_index, WptT::deleted))
			return -8;
	}

	// try to find nearby waypoint if there was no argument
	if (wpt_index == NO_VAL)
	{
		wpt_index = wptmanager.FindNearestWaypointToPlayer(pEntity);

		// there's no waypoint where the split could be done so return error
		if (wpt_index == NO_VAL)
			return -1;
	}

	// if there was no path given as argument then ...
	if (path_index == NO_VAL)
	{
		// find the first path on waypoint
		path_index = wptmanager.FindPath(wpt_index);

		if (path_index == NO_VAL)
			return -1;

		path_length = wptmanager.GetPathLength(path_index);

		if ((path_length > 0) && (path_length < min_length))
			return -2;
	}

	// find the position of the waypoint in the path
	p = wptmanager.GetWaypointPointer(wpt_index, path_index);

	// return error if the waypoint isn't present in our path
	if (p == NULL)
		return -3;

	// if there is no previous node then this waypoint must be at the beginning of the path so we can't split it here
	if (p->prev == NULL)
		return -4;

	// if there aren't at least two nodes preceding this one then we can't split the path
	// in other words this must be at least 3rd waypoint from the beginning of the path to allow the split
	// this is due to the fact that if the splitting is done automatically as a result of waypoint change
	// to a cross waypoint then such waypoint will be excluded from the path ...
	// therefore there must be at least three waypoints left in the first part of path
	if (p->prev->prev == NULL)
		return -4;

	// if there is no next node then the waypoint is at the end of the path so we can't split the path here either
	if (p->next == NULL)
		return -4;

	// if there aren't at least two nodes after this then we can't split the path at this position
	// in other words there must be at least two waypoints left in the path
	// so the part we'll cut off will be a valid path (ie. a path with at least two waypoints)
	if (p->next->next == NULL)
		return -4;

	// set the pointer to next node
	// (ie. next waypoint in the path, because that is the waypoint which will be the starting point of the new path)
	p = p->next;

	// return error if we can't start new path
	if (StartNewPath(p->wpt_index) == false)
		return -5;

	// back up the index of the new path
	int other_path = internals.GetPathToContinue();

	// copy all path data from current path to the new one
	w_paths[other_path]->flags = w_paths[path_index]->flags;

	// and remove the waypoint from current path
	if (ExcludeFromPath(p, path_index) == false)
		return -6;

	// the pointer was freed so we must find the position of the waypoint in the path again
	p = wptmanager.GetWaypointPointer(wpt_index, path_index);

	// so that we can again set the pointer to next node and proceed the rest of the path
	p = p->next;

	int safety_stop = 0;

	// now go through the rest of the path and move it to the other path
	while (p)
	{
		if (ContinueCurrPath(p->wpt_index) < 1)
			return -6;

		if (ExcludeFromPath(p, path_index) == false)
			return -6;

		p = wptmanager.GetWaypointPointer(wpt_index, path_index);
		p = p->next;

		safety_stop++;
		if (safety_stop > LINKEDLIST_LOOPS_THRESHOLD)
			LinkedListError("Split Path", path_index);
	}

	// we're done here so we must reset it
	internals.ResetPathToContinue();

	// check both paths for errors
	if (wptfixer.ValidatePath(path_index) != -1)
	{
		if (wptfixer.ValidatePath(other_path) != -1)
		{
			// and update both path status, because we changed them both
			wptfixer.UpdatePathStatus(path_index);
			wptfixer.UpdatePathStatus(other_path);

			return other_path;
		}
	}

	return -6;
}


/*
* changes the order of waypoints on the path, ie. if it was created like this 1->2->3 then it will become 3->2->1
* it doesn't change the direction tag at all so if the path is one-way it will remain one-way
* it works with either given path index or path that is currently worked on or the first found path on nearby waypoint
*/
bool path_editing_functions_t::Reverse(edict_t* pEntity, int path_index)
{
	int wpt_index = NO_VAL;
	int backup_of_path_to_continue = NO_VAL;

	if (internals.IsPathToContinue())
		backup_of_path_to_continue = internals.GetPathToContinue();

	// no path given?
	if (path_index == NO_VAL)
	{
		// then try to use the one that the user currently works on
		path_index = internals.GetPathToContinue();

		if (path_index == NO_VAL)
		{
			// or try to get the path from nearby waypoint
			wpt_index = wptmanager.FindNearestWaypointToPlayer(pEntity);

			if (wpt_index == NO_VAL)
				return false;

			path_index = wptmanager.FindPath(wpt_index);

			if (path_index == NO_VAL)
				return false;
		}
	}

	int path_length = wptmanager.GetPathLength(path_index);

	// process only valid paths
	if (path_length < 2)
		return false;

	// what we do here is that we always take the last waypoint from our path and add it to a new temporary path
	wpt_index = wptmanager.GetPathEnd(path_index);

	if ((wpt_index != NO_VAL) && StartNewPath(wpt_index))
	{
		// get the index of the temporary path
		int temp_path = internals.GetPathToContinue();

		// we need to copy all path data ie. the class, team and other tags
		w_paths[temp_path]->flags = w_paths[path_index]->flags;

		// the length must be reduced by 1, because we are removing the last waypoint outside the "for" cycle
		path_length--;

		if (ExcludeFromPath(wpt_index, path_index))
		{
			for (int i = 0; i < path_length; i++)
			{
				wpt_index = wptmanager.GetPathEnd(path_index);

				if ((wpt_index != NO_VAL) && ExcludeFromPath(wpt_index, path_index))
					ContinueCurrPath(wpt_index);
			}
		}

		// now that the temporary path is created, in reverse order all we need to do is simply copy it back to previous array slot so that the path index doesn't change
		wpt_index = wptmanager.GetPathStart(temp_path);

		if ((wpt_index != NO_VAL) && StartNewPath(wpt_index, path_index))
		{
			// we have to get the path data back too
			w_paths[path_index]->flags = w_paths[temp_path]->flags;

			if (ExcludeFromPath(wpt_index, temp_path))
			{
				for (int i = 0; i < path_length; i++)
				{
					wpt_index = wptmanager.GetPathStart(temp_path);

					if ((wpt_index != NO_VAL) && ExcludeFromPath(wpt_index, temp_path))
						ContinueCurrPath(wpt_index);
				}
			}
		}

		// start new path function always assigns this variable so we have to reset it
		internals.ResetPathToContinue();

		// did the user work on some path before calling this function? then activate it again, because it's been tampered with this variable in this function
		if (backup_of_path_to_continue != NO_VAL)
			internals.SetPathToContinue(backup_of_path_to_continue);

		if (wptfixer.ValidatePath(path_index) != -1)
			return true;
	}
	
	return false;
}


/*
* changes way/direction flag on path that is specified in path_index
* or the first path on the nearest waypoint
* or the currently edited path
* returns 0 is that direction already is on this path
* returns 1 if it was successfully changed
* returns -1 if new type is NOT specified or is invalid
* returns -2 if the path is invalid
*/
int path_editing_functions_t::ChangeDirection(edict_t* pEntity, const char* new_value, int path_index)
{
	int new_class_val = P_FL_WAY_TWO;

	if ((new_value == NULL) || (*new_value == 0))
		return -1;

	if (FStrEq(new_value, "one"))
		new_class_val = P_FL_WAY_ONE;
	else if (FStrEq(new_value, "two"))
		new_class_val = P_FL_WAY_TWO;
	else if (FStrEq(new_value, "patrol"))
		new_class_val = P_FL_WAY_PATROL;
	else
		return -1;

	if ((path_index != NO_VAL) && (w_paths[path_index] == NULL))
		return -2;

	if ((path_index == NO_VAL) && internals.IsPathToContinue())
		path_index = internals.GetPathToContinue();

	if (path_index == NO_VAL)
	{
		int closest_wpt = wptmanager.FindNearestWaypointToPlayer(pEntity);

		path_index = wptmanager.FindPath(closest_wpt);
	}

	if ((path_index != NO_VAL) && w_paths[path_index])
	{
		if (new_class_val & P_FL_WAY_TWO)
		{
			// already is present so no point doing anything
			if (w_paths[path_index]->flags & P_FL_WAY_TWO)
				return 0;

			RemoveExistingDirectionFlags(path_index);
			w_paths[path_index]->flags |= P_FL_WAY_TWO;
		}
		else if (new_class_val & P_FL_WAY_ONE)
		{
			// is already present?
			if (w_paths[path_index]->flags & P_FL_WAY_ONE)
			{
				// then change the path back to default
				RemoveExistingDirectionFlags(path_index);
				w_paths[path_index]->flags |= P_FL_WAY_TWO;
			}
			// otherwise change it to one-way
			else
			{
				RemoveExistingDirectionFlags(path_index);
				w_paths[path_index]->flags |= P_FL_WAY_ONE;
			}
		}
		else if (new_class_val & P_FL_WAY_PATROL)
		{
			if (w_paths[path_index]->flags & P_FL_WAY_PATROL)
			{
				RemoveExistingDirectionFlags(path_index);
				w_paths[path_index]->flags |= P_FL_WAY_TWO;
			}
			else
			{
				RemoveExistingDirectionFlags(path_index);
				w_paths[path_index]->flags |= P_FL_WAY_PATROL;
			}
		}

		return 1;
	}

	return -2;
}


/*
* changes team flag on path that is specified in path_index
* or the first path on the nearest waypoint
* or the currently edited path
* returns 1 if the team flag was successfully changed
* returns 0 if the path already has that team flag
* returns -1 if new type is NOT specified or is invalid
* returns -2 if path is invalid
*/
int path_editing_functions_t::ChangeTeam(edict_t* pEntity, const char* new_value, int path_index)
{
	// see whether the new team value (ie arg2) is valid
	if (conInput.IsValidTeam(new_value) == false)
		return -1;

	// is path specified AND that path doesn't exist
	if ((path_index != NO_VAL) && (w_paths[path_index] == NULL))
		return -2;

	// if no path is specified, but there is a path that is currently edited then use it
	if ((path_index == NO_VAL) && internals.IsPathToContinue())
		path_index = internals.GetPathToContinue();

	// if no path is specified (and we aren't editing any path either) then..
	if (path_index == NO_VAL)
	{
		// try to find any nearby waypoint first
		int closest_wpt = wptmanager.FindNearestWaypointToPlayer(pEntity);

		// and get the first path from the nearest waypoint
		path_index = wptmanager.FindPath(closest_wpt);
	}

	// change the team flag only if the path exist
	if ((path_index != NO_VAL) && w_paths[path_index])
	{
		if (conInput.IsTeamBoth())
		{
			// already is a both team path so end it right here
			if (w_paths[path_index]->flags & P_FL_TEAM_NO)
				return 0;

			// remove the old team flag first
			RemoveExistingTeamFlags(path_index);

			// set the both team flag
			w_paths[path_index]->flags |= P_FL_TEAM_NO;
		}
		else if (conInput.IsTeamOne())
		{
			// already is a red team path?
			if (w_paths[path_index]->flags & P_FL_TEAM_RED)
			{
				// then return it to default both teams path
				RemoveExistingTeamFlags(path_index);
				w_paths[path_index]->flags |= P_FL_TEAM_NO;
			}
			// otherwise make this path a red team only
			else
			{
				RemoveExistingTeamFlags(path_index);
				w_paths[path_index]->flags |= P_FL_TEAM_RED;
			}
		}
		else if (conInput.IsTeamTwo())
		{
			if (w_paths[path_index]->flags & P_FL_TEAM_BLUE)
			{
				RemoveExistingTeamFlags(path_index);
				w_paths[path_index]->flags |= P_FL_TEAM_NO;
			}
			else
			{
				RemoveExistingTeamFlags(path_index);
				w_paths[path_index]->flags |= P_FL_TEAM_BLUE;
			}
		}

		return 1;
	}

	return -2;
}


/*
* changes class flag on path that is specified in path_index
* or the first path on the nearest waypoint
* or the currently edited path
* returns 1 if everything is OK
* returns -1 if new type is NOT specified or is invalid
* returns -2 if path is invalid
*/
int path_editing_functions_t::ChangeClass(edict_t* pEntity, const char* new_value, int path_index)
{
	int new_class_val;

	if ((new_value == NULL) || (*new_value == 0))
		return -1;

	if (FStrEq(new_value, "all"))
		new_class_val = P_FL_CLASS_ALL;
	else if (FStrEq(new_value, "sniper"))
		new_class_val = P_FL_CLASS_SNIPER;
	else if (FStrEq(new_value, "mgunner"))
		new_class_val = P_FL_CLASS_MGUNNER;
	else if (FStrEq(new_value, "antiarmor") || FStrEq(new_value, "anti-armor"))
		new_class_val = P_FL_CLASS_ANTIARMOR;
	else
		return -1;

	if ((path_index != NO_VAL) && (w_paths[path_index] == NULL))
		return -2;

	if ((path_index == NO_VAL) && internals.IsPathToContinue())
		path_index = internals.GetPathToContinue();

	if (path_index == NO_VAL)
	{
		int closest_wpt = wptmanager.FindNearestWaypointToPlayer(pEntity);

		path_index = wptmanager.FindPath(closest_wpt);
	}

	if ((path_index != NO_VAL) && w_paths[path_index])
	{
		// do we change this one to a path for all
		if (new_class_val & P_FL_CLASS_ALL)
		{
			// already is present so no point doing anything
			if (w_paths[path_index]->flags & P_FL_CLASS_ALL)
				return 0;

			// remove all class restrictions, becuase this path is going to be accessible for all again
			RemoveExistingClassFlags(path_index);

			// now set the all classes flag
			w_paths[path_index]->flags |= P_FL_CLASS_ALL;

			return 1;
		}

		// we're making this path a class restricted so we must remove the all classes flag first
		if (w_paths[path_index]->flags & P_FL_CLASS_ALL)
			w_paths[path_index]->flags &= ~P_FL_CLASS_ALL;

		// now if the class restriction flag already is on the path then remove it
		if (w_paths[path_index]->flags & new_class_val)
			w_paths[path_index]->flags &= ~new_class_val;
		// otherwise set the new class restriction flag
		else
			w_paths[path_index]->flags |= new_class_val;

		// are there NO class restrictions at all? (ie. we removed them one by another)
		if ((wptmanager.IsPath(path_index, PathT::sniper_class) == false) && (wptmanager.IsPath(path_index, PathT::mgunner_class) == false) &&
			(wptmanager.IsPath(path_index, PathT::antiarmor_class) == false))
		{
			// then make it all classes again
			w_paths[path_index]->flags |= P_FL_CLASS_ALL;
		}

		return 1;
	}

	return -2;
}


/*
* changes miscellaneous/additional flag on path that is specified in path_index
* or the first path on the nearest waypoint
* or the currently edited path
* returns 1 if everything is OK
* returns -1 if new type is NOT specified or is invalid
* returns -2 if path is invalid
*/
int path_editing_functions_t::ChangeMisc(edict_t* pEntity, const char* new_value, int path_index)
{
	int new_misc_val;

	if ((new_value == NULL) || (*new_value == 0))
		return -1;

	if ((FStrEq(new_value, "avoid_enemy")) || (FStrEq(new_value, "avoidenemy")))
		new_misc_val = P_FL_MISC_AVOID;
	else if ((FStrEq(new_value, "ignore_enemy")) || (FStrEq(new_value, "ignoreenemy")))
		new_misc_val = P_FL_MISC_IGNORE;
	else if ((FStrEq(new_value, "carry_item")) || (FStrEq(new_value, "carryitem")))
		new_misc_val = P_FL_MISC_GITEM;
	else
		return -1;

	if ((path_index != NO_VAL) && (w_paths[path_index] == NULL))
		return -2;

	if ((path_index == NO_VAL) && internals.IsPathToContinue())
		path_index = internals.GetPathToContinue();

	if (path_index == NO_VAL)
	{
		int closest_wpt = wptmanager.FindNearestWaypointToPlayer(pEntity);

		path_index = wptmanager.FindPath(closest_wpt);
	}

	if ((path_index != NO_VAL) && w_paths[path_index])
	{
		// if the flag already is on this path then remove it
		if (w_paths[path_index]->flags & new_misc_val)
			w_paths[path_index]->flags &= ~new_misc_val;
		// otherwise set it
		else
			w_paths[path_index]->flags |= new_misc_val;

		// fix unwanted/crazy combinations ...

		// remove ignore when we are setting avoid (ie. it's a bit strange to tell the bot to ignore all enemies and avoid just the distant ones at the same time)
		if ((new_misc_val & P_FL_MISC_AVOID) && (w_paths[path_index]->flags & P_FL_MISC_IGNORE))
			w_paths[path_index]->flags &= ~P_FL_MISC_IGNORE;

		// the same issue as above
		if ((new_misc_val & P_FL_MISC_IGNORE) && (w_paths[path_index]->flags & P_FL_MISC_AVOID))
			w_paths[path_index]->flags &= ~P_FL_MISC_AVOID;

		return 1;
	}

	return -2;
}


/*
* resets path back to default values (both team, all classes, one way)
* specified in path_index
* or the first path on the nearest waypoint
* or the currently edited path
*/
bool path_editing_functions_t::ResetToDefaults(edict_t* pEntity, int path_index)
{
	if ((path_index == NO_VAL) && internals.IsPathToContinue())
		path_index = internals.GetPathToContinue();

	if (path_index == NO_VAL)
	{
		int closest_wpt = wptmanager.FindNearestWaypointToPlayer(pEntity);

		path_index = wptmanager.FindPath(closest_wpt);
	}

	// is path specified AND the path doesn't exist OR path_index isn't valid (ie -1)
	if (((path_index != NO_VAL) && (w_paths[path_index] == NULL)) || (path_index == NO_VAL))
		return false;

	// clear all flags
	w_paths[path_index]->flags = 0;

	// and set only the default values
	w_paths[path_index]->flags |= P_FL_TEAM_NO | P_FL_CLASS_ALL | P_FL_WAY_TWO;

	return true;
}


/*
* sets the automatically assigned path type (via path_type) to given path
*/
void path_editing_functions_t::SetAutoTag(PathT path_type, int path_index)
{
	if ((path_index == NO_VAL) || (w_paths[path_index] == NULL) || (path_type <= PathT::scrapped_flagtype))
		return;

	if (path_type == PathT::ammo_tag)
		w_paths[path_index]->flags |= P_FL_MISC_AMMO;
	else if (path_type == PathT::bandages_tag)
		w_paths[path_index]->flags |= P_FL_MISC_BANDAGES;
	else if (path_type == PathT::goal_team_one_tag)
		w_paths[path_index]->flags |= P_FL_MISC_GOAL_RED;
	else if (path_type == PathT::goal_team_two_tag)
		w_paths[path_index]->flags |= P_FL_MISC_GOAL_BLUE;
	else if (path_type == PathT::goal_explosives_tag)
		w_paths[path_index]->flags |= P_FL_MISC_GEXPLOSIVES;
	else if (path_type == PathT::roadblocked_tag)
		w_paths[path_index]->flags |= P_FL_MISC_ROADBLOCKED;

	return;
}


/*
* removes the automatically assigned path type (via path_type) from given path
*/
void path_editing_functions_t::ResetAutoTag(PathT path_type, int path_index)
{
	if ((path_index == NO_VAL) || (w_paths[path_index] == NULL) || (path_type <= PathT::scrapped_flagtype))
		return;

	if (path_type == PathT::ammo_tag)
		w_paths[path_index]->flags &= ~P_FL_MISC_AMMO;
	else if (path_type == PathT::bandages_tag)
		w_paths[path_index]->flags &= ~P_FL_MISC_BANDAGES;
	else if (path_type == PathT::goal_team_one_tag)
		w_paths[path_index]->flags &= ~P_FL_MISC_GOAL_RED;
	else if (path_type == PathT::goal_team_two_tag)
		w_paths[path_index]->flags &= ~P_FL_MISC_GOAL_BLUE;
	else if (path_type == PathT::goal_explosives_tag)
		w_paths[path_index]->flags &= ~P_FL_MISC_GEXPLOSIVES;
	else if (path_type == PathT::roadblocked_tag)
		w_paths[path_index]->flags &= ~P_FL_MISC_ROADBLOCKED;

	return;
}


/*
* saves path stucture into the file
* returns TRUE if everything is OK
* returns FALSE if something went wrong or if there are no paths
*/
bool path_editing_functions_t::SavePaths(const char* custom_filename)
{
	char filename[256];
	char mapname[64];
	PATH_HDR header{};
	int path_index, path_length, path_count;

	// remove all invalid paths first
	wptfixer.DeleteInvalidPaths(false);

	// check if at least one path exists
	path_count = 0;
	for (int paths = 0; paths < num_w_paths; paths++)
	{
		if (w_paths[paths] != NULL)
		{
			// break it we know that path exists
			path_count++;
			break;
		}
	}

	// don't write .pth file if there are no paths
	if (path_count == 0)
	{
		return false;
	}

	// init the path file header with all important data

	strcpy(header.filetype, "FAM_bot");

	header.waypoint_file_version = WAYPOINT_VERSION;
	header.waypoint_flag = num_waypoints;		// critical for compatibility
	header.number_of_paths = num_w_paths;

	memset(header.mapname, 0, sizeof(header.mapname));
	strncpy(header.mapname, STRING(gpGlobals->mapname), 31);
	header.mapname[31] = 0;

	// write authors signature
	memset(header.author, 0, sizeof(header.author));
	if (wpt_author[0] == 0)
		strncpy(header.author, "unknown", 31);
	else
		strncpy(header.author, wpt_author, 31);
	header.author[31] = 0;

	// write the one who modified them
	memset(header.modified_by, 0, sizeof(header.modified_by));
	if (wpt_modified[0] == 0)
		strncpy(header.modified_by, "unknown", 31);
	else
		strncpy(header.modified_by, wpt_modified, 31);
	header.modified_by[31] = 0;

	// if we used our own name save them under it
	if (custom_filename != NULL)
		strcpy(mapname, custom_filename);
	// otherwise use real mapname
	else
		strcpy(mapname, STRING(gpGlobals->mapname));
	strcat(mapname, ".pth");

	if (internals.IsCustomWaypoints())
		util.MarineBotFileName(filename, "customwpts", mapname);
	else
		util.MarineBotFileName(filename, "defaultwpts", mapname);

	FILE* bfp = fopen(filename, "wb");

	// if file wasn't opened
	if (bfp == NULL)
		return false;

	// write the path header to the file
	fwrite(&header, sizeof(header), 1, bfp);

	// save all the paths
	for (path_index = 0; path_index < num_w_paths; path_index++)
	{
		// does this path exist
		if (w_paths[path_index] != NULL)
		{
			// update this path status first
			wptfixer.UpdatePathStatus(path_index);

			// save path_index
			fwrite(&path_index, sizeof(path_index), 1, bfp);

			// get length of this path
			path_length = wptmanager.GetPathLength(path_index);

			// save path length
			fwrite(&path_length, sizeof(path_length), 1, bfp);

			// save paths flags
			fwrite(&w_paths[path_index]->flags, sizeof(w_paths[path_index]->flags), 1, bfp);

			// now save the whole path

			W_PATH* p = w_paths[path_index];	// set the pointer to the head node

			// save path wpt_index one by one until reaches the end
			while (p != NULL)
			{
				fwrite(&p->wpt_index, sizeof(p->wpt_index), 1, bfp);

				p = p->next;  // go to next node in linked list
			}
		}
		// otherwise store only index and length = 0
		else
		{
			// save path_index
			fwrite(&path_index, sizeof(path_index), 1, bfp);

			// set path length to zero (to know that the path was deleted)
			path_length = 0;

			// save path length
			fwrite(&path_length, sizeof(path_length), 1, bfp);
		}
	}

	fclose(bfp);

	// now the path file has been saved so delete the associated error message
	// to stop displaying it through the HUD message system
	errormsgs.DeleteErrorCode(UEMS_WARN_PTH);
	// delete also the error message from the copy of the error messages
	// this must be done because the copy is being refreshed once all the messages have been sent,
	// so this message can still be pending and would have been displayed despite the fact that
	// the path file is already valid
	errormsgs.DeleteFromCopyOfErCodes(UEMS_WARN_PTH);

	return true;
}


/*
* loads path stucture from file
* returns -1 if old path structure is detected (to allow auto conversion)
* returns 0 if something went wrong
* returns 1 if everything is OK
*/
int path_editing_functions_t::LoadPaths(edict_t* pEntity, const char* custom_filename)
{
	extern bool is_dedicated_server;

	char mapname[64];
	char filename[256];
	PATH_HDR header;
	char msg[256];
	bool show_paths_again = false;		// TRUE if paths were turned on
	W_PATH* p = NULL;
	int path_index, path_length, flags, wpt_index;

	if (custom_filename == NULL)
	{
		strcpy(mapname, STRING(gpGlobals->mapname));
		strcat(mapname, ".pth");
	}
	else
	{
		strcpy(mapname, custom_filename);
		strcat(mapname, ".pth");
	}

	if (internals.IsCustomWaypoints())
		util.MarineBotFileName(filename, "customwpts", mapname);
	else
		util.MarineBotFileName(filename, "defaultwpts", mapname);

	if (is_dedicated_server)
	{
		sprintf(msg, "loading path file: %s\n", filename);
		conOutput.Print(NULL, msg, MType::msg_info);
	}

	FILE* bfp = fopen(filename, "rb");

	// if file exists, read the path structure from it
	if (bfp != NULL)
	{
		fread(&header, sizeof(header), 1, bfp);

		header.filetype[7] = 0;
		if (strcmp(header.filetype, "FAM_bot") == 0)
		{
			if (header.waypoint_file_version != WAYPOINT_VERSION)
			{
				sprintf(msg, "Outdated version of MarineBot paths: %d (current waypoint system version: %d)\n", header.waypoint_file_version, WAYPOINT_VERSION);
				conOutput.Print(pEntity, msg, MType::msg_error);
				conOutput.Print(pEntity, "Paths not loading!\n", MType::msg_warning);

				// "wpt load" client command shouldn't call this function at all, but just in case
				if (pEntity == NULL)
				{
					conOutput.Print(pEntity, "Auto conversion started...\n", MType::msg_info);
				}

				fclose(bfp);
				return -1;	// to start automatic conversion
			}

			// if current waypoint count doesn't match with path file record
			if (header.waypoint_flag != num_waypoints)
			{
				conOutput.Print(pEntity, "Waypoint file and path file are out of sync!\n", MType::msg_error);

				fclose(bfp);
				return 0;
			}

			// init total count of paths
			num_w_paths = header.number_of_paths;

			// check if path count is valid
			if (num_w_paths >= MAX_W_PATHS)
			{
				ALERT(at_error, "MarineBot - Error the path count exceeded max limit!\n");

				fclose(bfp);
				return 0;
			}

			header.mapname[31] = 0;

			if ((strcmp(header.mapname, STRING(gpGlobals->mapname)) == 0) || (custom_filename != NULL))
			{
				// remove any existing paths
				FreeAllPaths();

				// if paths are displayed then hide them until the rebuild is done
				if (wptser.IsShowPaths())
				{
					wptser.ResetShowPaths();
					show_paths_again = true;
				}

				// load and rebuild all paths
				for (int all_paths = 0; all_paths < num_w_paths; all_paths++)
				{
					// read the stored path_index
					fread(&path_index, sizeof(path_index), 1, bfp);

					p = (W_PATH*)malloc(sizeof(W_PATH));	// create new head node

					if (p == NULL)
					{
						ALERT(at_error, "MarineBot - Error allocating memory for path!\n");

						fclose(bfp);
						return 0;
					}

					// init pointers to previous and next node
					p->prev = NULL;
					p->next = NULL;

					// store head node of the new path into w_paths array
					w_paths[path_index] = p;

					// init actual path (a must for Continue Curr Path())
					internals.SetPathToContinue(path_index);

					// read the path length
					fread(&path_length, sizeof(path_length), 1, bfp);

					// delete the path and go to next path
					if (path_length == 0)
					{
						DeleteWholePath(path_index);
						continue;
					}

					// read path flags to local temp flags
					fread(&flags, sizeof(w_paths[path_index]->flags), 1, bfp);

					// read the head node wpt_index
					fread(&wpt_index, sizeof(p->wpt_index), 1, bfp);

					p->wpt_index = wpt_index;

					// read rest of paths wpt_index one by one from 1 up to path_length, because the first is already done (in head node)
					for (int index = 1; index < path_length; index++)
					{
						fread(&wpt_index, sizeof(p->wpt_index), 1, bfp);

						// check if waypoint was added correctly
						if (ContinueCurrPath(wpt_index, false) < 1)
						{
							fclose(bfp);
							return 0;
						}
					}

					// finally set all path flags
					w_paths[path_index]->flags = flags;

					// we better remove both goal tags here, the paths will get updated by the regular call of waypoint update function and this safety clean up we prevent bugs in case that the map
					// been updated on the entities or better to say someone just modified the properties of the goal entities, in other words doing the clean up here means one call, but doing it
					// in the waypoint update function would mean repeated calls every couple seconds so this way should cost less system resources
					ResetAutoTag(PathT::goal_team_one_tag, path_index);
					ResetAutoTag(PathT::goal_team_two_tag, path_index);
				}

				g_waypoint_paths = true;	// keep track so path can be freed
			}
			else
			{
				sprintf(msg, "MarineBot paths are not for this map: %s\n", filename);
				conOutput.Print(pEntity, msg, MType::msg_warning);

				// don't print this while map loads
				if (pEntity)
					conOutput.Print(pEntity, wpt_warning, MType::msg_info);

				fclose(bfp);
				return 0;
			}

			header.author[31] = 0;
			if (strcmp(header.author, "") == 0)
				strcpy(wpt_author, "unknown");
			else
				strcpy(wpt_author, header.author);

			header.modified_by[31] = 0;
			if (strcmp(header.modified_by, "") == 0)
				strcpy(wpt_modified, "unknown");
			else
				strcpy(wpt_modified, header.modified_by);
		}
		else
		{
			sprintf(msg, "Not a MarineBot path file: %s\n", filename);
			conOutput.Print(pEntity, msg, MType::msg_warning);

			if (pEntity)
				conOutput.Print(pEntity, wpt_warning, MType::msg_info);

			fclose(bfp);
			return 0;
		}

		fclose(bfp);
	}
	else
	{
		sprintf(msg, "No path file: %s\n", filename);
		conOutput.Print(pEntity, msg, MType::msg_warning);

		if (pEntity)
			conOutput.Print(pEntity, wpt_warning, MType::msg_info);

		return 0;
	}

	// clear current path "pointer"
	internals.ResetPathToContinue();

	// if the paths were temporarily hidden then display them again
	if (show_paths_again)
		wptser.SetShowPaths(true);

	return 1;
}


/*
* loads only some path data (based on waypoint version) to convert older paths to latest (actual) version
*/
bool path_editing_functions_t::LoadUnsupportedPaths(edict_t* pEntity)
{
	extern bool is_dedicated_server;

	char mapname[64];
	char filename[256];
	//OLD_PATH_HDR old_header;
	PATH_HDR header;		// can be used current header until any change is done
	char msg[256];
	bool show_paths_again = false;		// TRUE if paths were turned on
	W_PATH* p = NULL;
	int path_index, path_length, flags, wpt_index;
	bool known;
	OLD_W_PATH* old_paths = NULL;	// we don't really need it here

	strcpy(mapname, STRING(gpGlobals->mapname));
	strcat(mapname, ".pth");

	if (internals.IsCustomWaypoints())
		util.MarineBotFileName(filename, "customwpts", mapname);
	else
		util.MarineBotFileName(filename, "defaultwpts", mapname);

	if (is_dedicated_server)
	{
		sprintf(msg, "loading unsupported paths from: %s\n", filename);
		conOutput.Print(NULL, msg, MType::msg_info);
	}

	FILE* bfp = fopen(filename, "rb");

	// if file exists, read the path structure from it
	if (bfp != NULL)
	{
		fread(&header, sizeof(header), 1, bfp);

		header.filetype[7] = 0;
		if (strcmp(header.filetype, "FAM_bot") == 0)
		{
			if (header.waypoint_file_version == WAYPOINT_VERSION)
			{
				conOutput.Print(pEntity, "This path file isn't outdated. No need to load it this way.\n", MType::msg_info);

				fclose(bfp);
				return false;
			}

			if (header.waypoint_file_version != WAYPOINT_VERSION)
			{
				known = false;

				if (header.waypoint_file_version == OLD_WAYPOINT_VERSION)
				{
					known = true;

					sprintf(msg, "found known older MarineBot path file (version %d - MB0.91b up to 0.95b) - conversion in progress...\n", OLD_WAYPOINT_VERSION);
					conOutput.Print(pEntity, msg, MType::msg_info);
				}
				else
				{
					conOutput.Print(pEntity, "unknown paths data (probably too old) - conversion failed!", MType::msg_error);

					fclose(bfp);
					return false;
				}
			}

			if (header.waypoint_flag != num_waypoints)
			{
				conOutput.Print(pEntity, "Waypoint file and path file are out of sync! - conversion failed!\n", MType::msg_error);

				fclose(bfp);
				return false;
			}

			num_w_paths = header.number_of_paths;

			if (num_w_paths >= MAX_W_PATHS)
			{
				ALERT(at_error, "MarineBot - Error the path count exceeded max limit!\n");

				fclose(bfp);
				return false;
			}

			header.mapname[31] = 0;

			if (strcmp(header.mapname, STRING(gpGlobals->mapname)) == 0)
			{
				FreeAllPaths();

				conOutput.Print(pEntity, "Loading converted path file...\n", MType::msg_info);

				if (wptser.IsShowPaths())
				{
					wptser.ResetShowPaths();
					show_paths_again = true;
				}

				// load and rebuild all paths
				for (int all_paths = 0; all_paths < num_w_paths; all_paths++)
				{
					// create new head node for the new path
					p = (W_PATH*)malloc(sizeof(W_PATH));

					if (p == NULL)
					{
						ALERT(at_error, "MarineBot - Error allocating memory for path!\n");

						fclose(bfp);
						return false;
					}

					// store head node of the new path into w_paths array
					w_paths[all_paths] = p;

					// init all new values to default at first
					w_paths[all_paths]->wpt_index = NO_VAL;
					w_paths[all_paths]->flags = 0;		// init flags
					w_paths[all_paths]->prev = NULL;
					w_paths[all_paths]->next = NULL;

					if (known)
					{
						// read the stored path_index
						fread(&path_index, sizeof(path_index), 1, bfp);

						if (p == NULL)
						{
							ALERT(at_error, "MarineBot - Error allocating memory for path!\n");

							fclose(bfp);
							return false;
						}

						// init pointers to previous and next node
						p->prev = NULL;
						p->next = NULL;

						// store head node of the new path into w_paths array
						w_paths[path_index] = p;

						// init actual path (a must for Continue Curr Path())
						internals.SetPathToContinue(path_index);

						// read the path length
						fread(&path_length, sizeof(path_length), 1, bfp);

						// delete the path and go to next path
						if (path_length == 0)
						{
							DeleteWholePath(path_index);
							continue;
						}

						flags = 0;

						// read path flags to local temp flags
						fread(&flags, sizeof(old_paths->flags), 1, bfp);

						// read the head node wpt_index
						fread(&wpt_index, sizeof(old_paths->wpt_index), 1, bfp);

						p->wpt_index = wpt_index;

						// read rest of paths wpt_index one by one from 2nd (ie. 1) up to path_length, the 1st (ie. 0) is already done (in head node)
						for (int index = 1; index < path_length; index++)
						{
							fread(&wpt_index, sizeof(old_paths->wpt_index), 1, bfp);

							// check if wpt was added correctly
							if (ContinueCurrPath(wpt_index, false) < 1)
							{
								fclose(bfp);
								return false;
							}
						}

						// finally convert old/obsolete things!!! -->> There's nothing to be converted now  (i.e. no critical changes between version 7 and 8)

						// just take the whole flags value and use it
						w_paths[path_index]->flags = flags;

						// and do the safety clean up like we do in standard load function
						ResetAutoTag(PathT::goal_team_one_tag, path_index);
						ResetAutoTag(PathT::goal_team_two_tag, path_index);
					}
				}

				g_waypoint_paths = true;	// keep track so path can be freed
			}
			else
			{
				sprintf(msg, "MarineBot paths are not for this map: %s\n", filename);
				conOutput.Print(pEntity, msg, MType::msg_warning);

				fclose(bfp);
				return false;
			}

			header.author[31] = 0;
			// if no author so set it to unknown
			if (strcmp(header.author, "") == 0)
				strcpy(wpt_author, "unknown");
			else
				strcpy(wpt_author, header.author);

			header.modified_by[31] = 0;
			// the guy who modified the waypoints is not specified? then set it to unknown
			if (strcmp(header.modified_by, "") == 0)
				strcpy(wpt_modified, "unknown");
			else
				strcpy(wpt_modified, header.modified_by);
		}
		else
		{
			sprintf(msg, "Not a MarineBot path file: %s\n", filename);
			conOutput.Print(pEntity, msg, MType::msg_error);

			fclose(bfp);
			return false;
		}

		fclose(bfp);
	}
	else
	{
		sprintf(msg, "No path file: %s\n", filename);
		conOutput.Print(pEntity, msg, MType::msg_warning);

		return false;
	}

	internals.ResetPathToContinue();

	if (show_paths_again)
		wptser.SetShowPaths(true);

	return true;
}


/*
* loads paths from waypoint system in version 6 (used in MB0.9) and converts them to the latest version
* there are many waypoints available in this even older system and we can still use them without bigger issues
*/
bool path_editing_functions_t::LoadUnsupportedPathsVersion6(edict_t* pEntity)
{
	char mapname[64];
	char filename[256];
	PATH_HDR header;		// can be used current header until any change is done
	char msg[256];
	bool show_paths_again = false;		// TRUE if paths were turned on
	W_PATH* p = NULL;
	int path_index, path_length, flags, wpt_index;
	bool known;
	OLD_W_PATH* old_paths = NULL;	// we don't really need it here
	int OLD_WAYPOINT_VERSION_6 = 6;		// we have to "override" standard conversion by sending even older version number

	strcpy(mapname, STRING(gpGlobals->mapname));
	strcat(mapname, ".pth");

	if (internals.IsCustomWaypoints())
		util.MarineBotFileName(filename, "customwpts", mapname);
	else
		util.MarineBotFileName(filename, "defaultwpts", mapname);

	FILE* bfp = fopen(filename, "rb");

	// if file exists, read the path structure from it
	if (bfp != NULL)
	{
		fread(&header, sizeof(header), 1, bfp);

		header.filetype[7] = 0;
		if (strcmp(header.filetype, "FAM_bot") == 0)
		{
			if (header.waypoint_file_version == WAYPOINT_VERSION)
			{
				conOutput.Print(pEntity, "This path file isn't outdated. No need to load it this way.\n", MType::msg_info);

				fclose(bfp);
				return false;
			}

			if (header.waypoint_file_version != WAYPOINT_VERSION)
			{
				known = false;

				if (header.waypoint_file_version == OLD_WAYPOINT_VERSION_6)
				{
					known = true;

					sprintf(msg, "found known older MarineBot path file (version %d - MB0.9b) - conversion in progress...\n", OLD_WAYPOINT_VERSION_6);
					conOutput.Print(pEntity, msg, MType::msg_info);
				}
				else
				{
					sprintf(msg, "unknown paths data (probably too old) - conversion failed!\n");
					conOutput.Print(pEntity, msg, MType::msg_error);

					fclose(bfp);
					return false;
				}
			}

			if (header.waypoint_flag != num_waypoints)
			{
				conOutput.Print(pEntity, "Waypoint file and path file are out of sync! - conversion failed!\n", MType::msg_error);

				fclose(bfp);
				return false;
			}

			num_w_paths = header.number_of_paths;

			if (num_w_paths >= MAX_W_PATHS)
			{
				ALERT(at_error, "MarineBot - Error the path count exceeded max limit!\n");

				fclose(bfp);
				return false;
			}

			header.mapname[31] = 0;

			if (strcmp(header.mapname, STRING(gpGlobals->mapname)) == 0)
			{
				FreeAllPaths();

				conOutput.Print(pEntity, "Loading converted path file...\n", MType::msg_info);

				if (wptser.IsShowPaths())
				{
					wptser.ResetShowPaths();
					show_paths_again = true;
				}

				// load and rebuild all paths
				for (int all_paths = 0; all_paths < num_w_paths; all_paths++)
				{
					// create new head node
					p = (W_PATH*)malloc(sizeof(W_PATH));

					if (p == NULL)
					{
						ALERT(at_error, "MarineBot - Error allocating memory for path!\n");

						return false;
					}

					// store head node of the new path into w_paths array
					w_paths[all_paths] = p;

					// init all new values to default at first
					w_paths[all_paths]->wpt_index = NO_VAL;
					w_paths[all_paths]->flags = 0;		// init flags
					w_paths[all_paths]->prev = NULL;
					w_paths[all_paths]->next = NULL;

					if (known)
					{
						// read the stored path_index
						fread(&path_index, sizeof(path_index), 1, bfp);

						if (p == NULL)
						{
							ALERT(at_error, "MarineBot - Error allocating memory for path!\n");

							fclose(bfp);
							return false;
						}

						// init pointers to previous and next node
						p->prev = NULL;
						p->next = NULL;

						// store head node of the new path into w_paths array
						w_paths[path_index] = p;

						// init actual path (a must for Continue Curr Path())
						internals.SetPathToContinue(path_index);

						// read the path length
						fread(&path_length, sizeof(path_length), 1, bfp);

						// delete the path and go to next path
						if (path_length == 0)
						{
							DeleteWholePath(path_index);
							continue;
						}

						flags = 0;

						// read path flags to local temp flags
						fread(&flags, sizeof(old_paths->flags), 1, bfp);

						// read the head node wpt_index
						fread(&wpt_index, sizeof(old_paths->wpt_index), 1, bfp);

						p->wpt_index = wpt_index;

						// read rest of paths wpt_index one by one from 2nd (ie. 1) up to path_length, the 1st (ie. 0) is already done (in head node)
						for (int index = 1; index < path_length; index++)
						{
							fread(&wpt_index, sizeof(old_paths->wpt_index), 1, bfp);

							// check if wpt was added correctly
							if (ContinueCurrPath(wpt_index, false) < 1)
							{
								fclose(bfp);
								return false;
							}
						}

						// finally convert old/obsolete things!!! -->> There's nothing to be converted now (i.e. no critical changes between these versions)

						// just take the whole flags value and use it
						w_paths[path_index]->flags = flags;

						// and do the safety clean up like we do in standard load function
						ResetAutoTag(PathT::goal_team_one_tag, path_index);
						ResetAutoTag(PathT::goal_team_two_tag, path_index);
					}
				}

				g_waypoint_paths = true;	// keep track so path can be freed
			}
			else
			{
				sprintf(msg, "MarineBot paths are not for this map: %s\n", filename);
				conOutput.Print(pEntity, msg, MType::msg_warning);

				fclose(bfp);
				return false;
			}

			header.author[31] = 0;
			// if no author then set it to unknown
			if (strcmp(header.author, "") == 0)
				strcpy(wpt_author, "unknown");
			else
				strcpy(wpt_author, header.author);

			header.modified_by[31] = 0;
			// the guy who modified the waypoints is not specified? then set it to unknown
			if (strcmp(header.modified_by, "") == 0)
				strcpy(wpt_modified, "unknown");
			else
				strcpy(wpt_modified, header.modified_by);
		}
		else
		{
			sprintf(msg, "Not a MarineBot path file: %s\n", filename);
			conOutput.Print(pEntity, msg, MType::msg_error);

			fclose(bfp);
			return false;
		}

		fclose(bfp);
	}
	else
	{
		sprintf(msg, "No path file: %s\n", filename);
		conOutput.Print(pEntity, msg, MType::msg_warning);

		return false;
	}

	internals.ResetPathToContinue();

	if (show_paths_again)
		wptser.SetShowPaths(true);

	return true;
}


/*
*/
bool path_editing_functions_t::LoadFirearmsPaths(edict_t* pEntity, const char* custom_filename)
{
	char mapname[64]{};
	char filename[256]{};
	PATH_HDR header{};
	char msg[256]{};
	bool show_paths_again = false;		// TRUE if paths were turned on
	W_PATH* p = NULL;
	int path_index, path_length, flags, wpt_index;

	strcpy(mapname, custom_filename);
	strcat(mapname, ".pth");

	if (internals.IsCustomWaypoints())
		util.MarineBotFileName(filename, "customwpts", mapname);
	else
		util.MarineBotFileName(filename, "defaultwpts", mapname);

	FILE* bfp = fopen(filename, "rb");

	// if file exists, read the path structure from it
	if (bfp != NULL)
	{
		fread(&header, sizeof(header), 1, bfp);

		header.filetype[7] = 0;
		if (strcmp(header.filetype, "FAM_bot") == 0)
		{
			if (header.waypoint_file_version != WAYPOINT_VERSION)
			{
				sprintf(msg, "Outdated version of MarineBot paths: %d (current waypoint system version: %d)\n", header.waypoint_file_version, WAYPOINT_VERSION);
				conOutput.Print(pEntity, msg, MType::msg_error);
				conOutput.Print(pEntity, "Paths not loading!\n", MType::msg_warning);

				fclose(bfp);
				return false;
			}

			// if current waypoint count doesn't match with path file record
			if (header.waypoint_flag != num_waypoints)
			{
				conOutput.Print(pEntity, "Waypoint file and path file are out of sync!\n", MType::msg_error);

				fclose(bfp);
				return false;
			}

			// init total count of paths
			num_w_paths = header.number_of_paths;

			// check if path count is valid
			if (num_w_paths >= MAX_W_PATHS)
			{
				ALERT(at_error, "MarineBot - Error the path count exceeded max limit!\n");

				fclose(bfp);
				return false;
			}

			header.mapname[31] = 0;

			if (strcmp(header.mapname, custom_filename) == 0)
			{
				// remove any existing paths
				FreeAllPaths();

				// if paths are displayed then hide them until the rebuild is done
				if (wptser.IsShowPaths())
				{
					wptser.ResetShowPaths();
					show_paths_again = true;
				}

				// load and rebuild all paths
				for (int all_paths = 0; all_paths < num_w_paths; all_paths++)
				{
					// read the stored path_index
					fread(&path_index, sizeof(path_index), 1, bfp);

					p = (W_PATH*)malloc(sizeof(W_PATH));	// create new head node

					if (p == NULL)
					{
						ALERT(at_error, "MarineBot - Error allocating memory for path!\n");

						fclose(bfp);
						return false;
					}

					// init pointers to previous and next node
					p->prev = NULL;
					p->next = NULL;

					// store head node of the new path into w_paths array
					w_paths[path_index] = p;

					// init actual path (a must for Continue Curr Path())
					internals.SetPathToContinue(path_index);

					// read the path length
					fread(&path_length, sizeof(path_length), 1, bfp);

					// delete the path and go to next path
					if (path_length == 0)
					{
						DeleteWholePath(path_index);
						continue;
					}

					// read path flags to local temp flags
					fread(&flags, sizeof(w_paths[path_index]->flags), 1, bfp);

					// read the head node wpt_index
					fread(&wpt_index, sizeof(p->wpt_index), 1, bfp);

					p->wpt_index = wpt_index;

					// read rest of paths wpt_index one by one from 1 up to path_length, because the first is already done (in head node)
					for (int index = 1; index < path_length; index++)
					{
						fread(&wpt_index, sizeof(p->wpt_index), 1, bfp);

						// check if waypoint was added correctly
						if (ContinueCurrPath(wpt_index, false) < 1)
						{
							fclose(bfp);
							return false;
						}
					}

					// finally set all path flags
					w_paths[path_index]->flags = flags;

					// and do the safety clean up like we do in standard load function
					ResetAutoTag(PathT::goal_team_one_tag, path_index);
					ResetAutoTag(PathT::goal_team_two_tag, path_index);
				}

				g_waypoint_paths = true;	// keep track so path can be freed
			}
			else
			{
				sprintf(msg, "MarineBot paths are not for this map: %s\n", filename);
				conOutput.Print(pEntity, msg, MType::msg_warning);

				fclose(bfp);
				return false;
			}

			header.author[31] = 0;
			if (strcmp(header.author, "") == 0)
				strcpy(wpt_author, "unknown");
			else
				strcpy(wpt_author, header.author);

			header.modified_by[31] = 0;
			if (strcmp(header.modified_by, "") == 0)
				strcpy(wpt_modified, "unknown");
			else
				strcpy(wpt_modified, header.modified_by);
		}
		else
		{
			sprintf(msg, "Not a MarineBot path file: %s\n", filename);
			conOutput.Print(pEntity, msg, MType::msg_warning);

			fclose(bfp);
			return false;
		}

		fclose(bfp);
	}
	else
	{
		sprintf(msg, "No path file: %s\n", filename);
		conOutput.Print(pEntity, msg, MType::msg_warning);

		return false;
	}

	// clear current path "pointer"
	internals.ResetPathToContinue();

	// if the paths were temporarily hidden then display them again
	if (show_paths_again)
		wptser.SetShowPaths(true);

	return true;
}


/*
* frees all paths
*/
void path_editing_functions_t::FreeAllPaths(void)
{
	for (int path_index = 0; path_index < MAX_W_PATHS; path_index++)
	{
		int safety_stop = 0;		// to stop it if encounters an error

		if (w_paths[path_index])
		{
			W_PATH* p = w_paths[path_index];	// set the pointer to the head node
			W_PATH* p_next;

			while (p)
			{
				p_next = p->next;	// save the link to next
				free(p);			// free this node
				p = p_next;			// update the head node

				safety_stop++;
				if (safety_stop > LINKEDLIST_LOOPS_THRESHOLD)
					LinkedListError("Free All Paths", path_index);
			}

			w_paths[path_index] = NULL;
		}
	}
}


/*
* removes all existing direction flags on given path
*/
void path_editing_functions_t::RemoveExistingDirectionFlags(int path_index)
{
	if (path_index == NO_VAL)
		return;

	if (w_paths[path_index]->flags & P_FL_WAY_ONE)
		w_paths[path_index]->flags &= ~P_FL_WAY_ONE;
	else if (w_paths[path_index]->flags & P_FL_WAY_TWO)
		w_paths[path_index]->flags &= ~P_FL_WAY_TWO;
	else if (w_paths[path_index]->flags & P_FL_WAY_PATROL)
		w_paths[path_index]->flags &= ~P_FL_WAY_PATROL;

	return;
}


/*
* removes all existing team flags on given path
*/
void path_editing_functions_t::RemoveExistingTeamFlags(int path_index)
{
	if (path_index == NO_VAL)
		return;

	if (w_paths[path_index]->flags & P_FL_TEAM_NO)
		w_paths[path_index]->flags &= ~P_FL_TEAM_NO;
	else if (w_paths[path_index]->flags & P_FL_TEAM_RED)
		w_paths[path_index]->flags &= ~P_FL_TEAM_RED;
	else if (w_paths[path_index]->flags & P_FL_TEAM_BLUE)
		w_paths[path_index]->flags &= ~P_FL_TEAM_BLUE;

	return;
}


/*
* removes all existing class flags on given path
*/
void path_editing_functions_t::RemoveExistingClassFlags(int path_index)
{
	if (path_index == NO_VAL)
		return;

	if (w_paths[path_index]->flags & P_FL_CLASS_ALL)
		w_paths[path_index]->flags &= ~P_FL_CLASS_ALL;

	// no "else ifs" because these classes can be together on a path (ie. sniper + mgunner only path) 
	if (w_paths[path_index]->flags & P_FL_CLASS_SNIPER)
		w_paths[path_index]->flags &= ~P_FL_CLASS_SNIPER;
	if (w_paths[path_index]->flags & P_FL_CLASS_MGUNNER)
		w_paths[path_index]->flags &= ~P_FL_CLASS_MGUNNER;

	if (w_paths[path_index]->flags & P_FL_CLASS_ANTIARMOR)
		w_paths[path_index]->flags &= ~P_FL_CLASS_ANTIARMOR;

	return;
}


/*
* checks all waypoints for possible issues like ...
* invalid combination of waypoint types such as ammobox + goback
* if sniper spot is missing an aim waypoint then we report it as a bug
*/
void waypoints_and_paths_repair_functions_t::CheckWaypointsForProblems(bool log_in_file)
{
	char msg[TEXT_MSG_SIZE];

	// first reset all the counters
	wptoutputer.ResetCounters();

	// now we must prevent resetting them in each of the subroutines called in this function, because we need to know the total amount of issues, but also stay below the limits the ingame console can show
	wptoutputer.SetOverrideCounterReset();

	// go through all waypoints ...
	for (int wpt_index = 0; wpt_index < num_waypoints; wpt_index++)
	{
		// check invalid waypoint type/tag/flag combinations
		RepairInvalidCombinationOfWaypointFlags(wpt_index, false, log_in_file);

		// check issues with zero priority setting
		if ((waypoints[wpt_index].red_priority == 0) && (waypoints[wpt_index].blue_priority == 0) &&
			(wptmanager.IsWaypoint(wpt_index, WptT::ammobox, WptT::claymore, WptT::goback) || wptmanager.IsWaypoint(wpt_index, WptT::roadblock, WptT::shoot, WptT::sprint) ||
				wptmanager.IsWaypoint(wpt_index, WptT::use)))
		{
			// see whether this waypoint is a trigger one... in which case the second pair of priorities can make this waypoint a valid setup so we won't print anything, ie. after capturing a map goal object/point
			// the bot needs to stop moving forward so previously disabled goback waypoint will be active now, or some button became active so the use (+ trigger) waypoint will have to do the same
			if ((wptmanager.IsWaypoint(wpt_index, WptT::trigger) == false) ||
				(wptmanager.IsWaypoint(wpt_index, WptT::trigger) && (waypoints[wpt_index].trigger_red_priority == 0) && (waypoints[wpt_index].trigger_blue_priority == 0)))
			{
				// see if this waypoint is connected to a cross waypoint... in which case we will only warn about such setup, because for example on such goback waypoint the bot won't get stuck,
				// or the second roadblock waypoint in the pair can be ignored because the bot already passed though dangerous spot and he will never choose a zero priority waypoint while deciding at
				// cross waypoint so in this case he won't go towards the dangerous place from the other side - therefore this warning will just point to such unusual/strange setup to double check it
				if (wptmanager.FindConnectedCross(wpt_index))
				{
					sprintf(msg, "WARNING: Waypoint action is disabled for both teams on waypoint no. %d.\n", wpt_index + 1);
					wptoutputer.AddWarning();
				}
				// otherwise this is a bug because the "action tag" has no meaning if it is going to be ignored so it's pointless to have it there at all, and sometimes it can lead to getting the bot stuck
				else
				{
					sprintf(msg, "BUG: Waypoint action is disabled for both teams on waypoint no. %d.\n", wpt_index + 1);
					wptoutputer.AddError();
				}
				
				wptoutputer.ProcessIt(msg, log_in_file);
			}
		}

		// check for the issue of priority set to value 1 on shoot waypoint
		if (wptmanager.IsWaypoint(wpt_index, WptT::shoot) && ((waypoints[wpt_index].red_priority == 1) || (waypoints[wpt_index].blue_priority == 1)))
		{
			sprintf(msg, "WARNING: Priority 1 on waypoint no. %d disables the search for breakable object. Also bot won't switch weapons.\n", wpt_index + 1);
			wptoutputer.AddWarning();
			wptoutputer.ProcessIt(msg, log_in_file);
		}

		// check issues with wait time
		if ((wptmanager.GetWaypointWaitTime(wpt_index, teamONE.GetTeamId()) > 0.0f) || (wptmanager.GetWaypointWaitTime(wpt_index, teamTWO.GetTeamId()) > 0.0f))
		{
			// check for invalid combination of waypoint flag/tag/type and presence of wait time
			if (IsInvalidWaitTime(wpt_index, false, log_in_file) == false)
			{
				// now we know this waypoint can have a wait time assigned so we can ...

				// check for properly made camper/sniper spots but ignore the 'shoot', 'claymore', 'use' and 'parachute' waypoints as they aren't meant to be such spots
				if ((wptmanager.IsWaypoint(wpt_index, WptT::shoot, WptT::claymore, WptT::use) == false) && (wptmanager.IsWaypoint(wpt_index, WptT::parachute) == false))
				{
					int path_index = wptmanager.FindPath(wpt_index);

					FixSniperSpot(path_index, wpt_index, false, log_in_file);
				}

				// check issues with shoot waypoint
				if (wptmanager.IsWaypoint(wpt_index, WptT::shoot) &&
					(((waypoints[wpt_index].red_time > 0.0f) && (waypoints[wpt_index].red_time < 3.0f)) || ((waypoints[wpt_index].blue_time > 0.0f) && (waypoints[wpt_index].blue_time < 3.0f))))
				{
					sprintf(msg, "WARNING: Wait time on waypoint no. %d may be too short for the bot to target the object.\n", wpt_index + 1);
					wptoutputer.AddWarning();
					wptoutputer.ProcessIt(msg, log_in_file);
				}
			}

			// check for using zero priority to disable the wait time which is nonsense unless it's used for specific purpose on a trigger waypoint
			RepairInvalidCombinationOfWaypointPriorityAndTime(wpt_index, false, log_in_file);
		}

		// check issues with range
		if (wptmanager.IsNoRangeWaypoint(wpt_index) && (waypoints[wpt_index].range != WPT_RANGE_SMALL))
		{
			sprintf(msg, "WARNING: Range on waypoint no. %d should be left on value %.1f. Bot doesn't use range setting on this waypoint.\n", wpt_index + 1, WPT_RANGE_SMALL);
			wptoutputer.AddWarning();
			wptoutputer.ProcessIt(msg, log_in_file);
		}

		// check issues with the door waypoint
		if (wptmanager.IsWaypoint(wpt_index, WptT::door, WptT::dooruse))
		{
			edict_t* pent = NULL;

			// see whether there is any door entity in the vicinity of this waypoint
			while ((pent = util.FindEntityInSphere(pent, waypoints[wpt_index].origin, STANDARD_SEARCH_RADIUS)) != NULL)
			{
				// we've found what we were looking for so no point continue searching anymore
				if (util.IsDoorEntity(pent))
					break;
			}

			// no doors around
			if (pent == NULL)
			{
				sprintf(msg, "WARNING: No doors near waypoint no. %d. Move it closer.\n", wpt_index + 1);
				wptoutputer.AddWarning();
				wptoutputer.ProcessIt(msg, log_in_file);
			}
		}

		// check issues with the trigger waypoint
		if (wptmanager.IsWaypoint(wpt_index, WptT::trigger))
		{
			if (waypoints[wpt_index].trigger_event_on == TriggerId::trigger_none)
			{
				sprintf(msg, "BUG: Missing the 'on' event for trigger waypoint no. %d.\n", wpt_index + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);
			}

			if (waypoints[wpt_index].trigger_event_off == TriggerId::trigger_none)
			{
				sprintf(msg, "WARNING: Missing the 'off' event for trigger waypoint no. %d. 'Off' event isn't always necessary.\n", wpt_index + 1);
				wptoutputer.AddWarning();
				wptoutputer.ProcessIt(msg, log_in_file);
			}
		}
	}

	// finally we'll return this override switch back to default state in order to allow successful counters reset next time this function is called as well as when the used routines are called separately
	wptoutputer.ResetOverrideCounterReset();

	return;
}


/*
* checks all paths for various issues such as...
* invalid combinations of waypoints and path types or invalid merge of paths or invalid end of path
* and finally it also warns about solitary waypoints
*/
void waypoints_and_paths_repair_functions_t::CheckPathsForProblems(bool log_in_file)
{
	char msg[256];
	int safety_stop = 0;

	// reset all the counters
	wptoutputer.ResetCounters();

	// also we must prevent resetting them in each of the subroutines called in this function
	wptoutputer.SetOverrideCounterReset();

	// go through all paths ...
	for (int path_index = 0; path_index < num_w_paths; path_index++)
	{
		// look for fatal errors such as a goback waypoint on one-way path
		if (IsInvalidCombinationOfWaypointAndPath(path_index, PathT::one_way, WptT::goback))
		{
			sprintf(msg, "BUG: There's a goback waypoint on one-way path (path no. %d)\n", path_index + 1);
			wptoutputer.AddError();
			wptoutputer.ProcessIt(msg, log_in_file);
		}

		// we should also warn the user about not so well combinations such as a parachute waypoint on a one-way path
		if (IsInvalidCombinationOfWaypointAndPath(path_index, PathT::one_way, WptT::parachute))
		{
			sprintf(msg, "WARNING: There's a parachute waypoint on one-way path (path no. %d)\n", path_index + 1);
			// in order to maintain correct console output we must pass the right amount of text lines,
			// because of the additional message following this warning (ie. this will ensure that we will either print both lines or none)
			wptoutputer.AddWarning(2);
			wptoutputer.ProcessIt(msg, log_in_file);

			sprintf(msg, "     Make sure the bot already has the parachute pack there.\n");
			wptoutputer.ProcessIt(msg, log_in_file);
		}

		if (IsInvalidCombinationOfWaypointAndPath(path_index, PathT::patrol_cycle, WptT::claymore))
		{
			sprintf(msg, "WARNING: There's a claymore waypoint on patrol path (path no. %d)\n", path_index + 1);
			wptoutputer.AddWarning(2);
			wptoutputer.ProcessIt(msg, log_in_file);

			sprintf(msg, "     The bot can die on his own bomb while patrolling.\n");
			wptoutputer.ProcessIt(msg, log_in_file);
		}

		if (IsInvalidCombinationOfWaypointAndPath(path_index, PathT::patrol_cycle, WptT::jump) || IsInvalidCombinationOfWaypointAndPath(path_index, PathT::patrol_cycle, WptT::duckjump))
		{
			sprintf(msg, "WARNING: There's a jump waypoint on patrol path (path no. %d)\n", path_index + 1);
			wptoutputer.AddWarning(2);
			wptoutputer.ProcessIt(msg, log_in_file);

			sprintf(msg, "     Make sure the bot can get over the obstacle from both directions.\n");
			wptoutputer.ProcessIt(msg, log_in_file);
		}

		if (IsInvalidCombinationOfWaypointAndPath(path_index, PathT::patrol_cycle, WptT::ladder))
		{
			sprintf(msg, "WARNING: There's a ladder waypoint on patrol path (path no. %d)\n", path_index + 1);
			wptoutputer.AddWarning(2);
			wptoutputer.ProcessIt(msg, log_in_file);

			sprintf(msg, "     They can get stuck on it when more bots are patrolling.\n");
			wptoutputer.ProcessIt(msg, log_in_file);
		}

		if (IsInvalidCombinationOfWaypointAndPath(path_index, PathT::patrol_cycle, WptT::parachute))
		{
			sprintf(msg, "BUG: There's a parachute waypoint on patrol path (path no. %d)\n", path_index + 1);
			wptoutputer.AddError();
			wptoutputer.ProcessIt(msg, log_in_file);
		}

		if (IsInvalidCombinationOfWaypointAndPath(path_index, PathT::patrol_cycle, WptT::shoot))
		{
			W_PATH* p = wptmanager.GetWaypointTypePointer(WptT::shoot, path_index);

			if (p != NULL)
			{
				// now we must check whether this is a "forced shoot for the fun" waypoint or regular "break obstacle" one
				if ((wptmanager.IsPath(path_index, PathT::team_one) && (waypoints[p->wpt_index].red_priority == 1)) ||
					(wptmanager.IsPath(path_index, PathT::team_two) && (waypoints[p->wpt_index].blue_priority == 1)) ||
					(wptmanager.IsPath(path_index, PathT::both_teams) && ((waypoints[p->wpt_index].red_priority == 1) || (waypoints[p->wpt_index].blue_priority == 1))))
				{
					sprintf(msg, "WARNING: There's a shoot waypoint on patrol path (path no. %d)\n", path_index + 1);
					wptoutputer.AddWarning(2);
					wptoutputer.ProcessIt(msg, log_in_file);

					sprintf(msg, "     The bot will fire his gun every patrolling pass due to priority = 1.\n");
					wptoutputer.ProcessIt(msg, log_in_file);
				}
			}
		}

		if (IsInvalidCombinationOfWaypointAndPath(path_index, PathT::patrol_cycle, WptT::use))
		{
			sprintf(msg, "WARNING: There's a use waypoint on patrol path (path no. %d)\n", path_index + 1);
			wptoutputer.AddWarning(2);
			wptoutputer.ProcessIt(msg, log_in_file);

			sprintf(msg, "     The bot will use this object every patrolling pass.\n");
			wptoutputer.ProcessIt(msg, log_in_file);
		}

		if (IsInvalidCombinationOfWaypointAndPath(path_index, PathT::one_way, WptT::roadblock))
		{
			sprintf(msg, "WARNING: There's a roadblock waypoint on one-way path (path no. %d)\n", path_index + 1);
			wptoutputer.AddWarning(2);
			wptoutputer.ProcessIt(msg, log_in_file);

			sprintf(msg, "     The bot can get stuck on it till the pathway gets free again.\n");
			wptoutputer.ProcessIt(msg, log_in_file);
		}

		// check for paths starting on zero priority waypoints ... bot cannot choose them at all thus they are pointless unless it's done on purpose using the trigger waypoint to open them with some game event
		int start_wpt = wptmanager.GetPathStart(path_index);
		if ((start_wpt != NO_VAL) && (wptmanager.IsWaypoint(start_wpt, WptT::trigger) == false))
		{
			if (((wptmanager.GetWaypointPriority(start_wpt, teamONE.GetTeamId()) == 0) && wptmanager.IsPath(path_index, PathT::team_one)) ||
				((wptmanager.GetWaypointPriority(start_wpt, teamTWO.GetTeamId()) == 0) && wptmanager.IsPath(path_index, PathT::team_two)) ||
				((wptmanager.GetWaypointPriority(start_wpt, teamONE.GetTeamId()) == 0) && (wptmanager.GetWaypointPriority(start_wpt, teamTWO.GetTeamId()) == 0) && wptmanager.IsPath(path_index, PathT::both_teams)))
			{
				sprintf(msg, "BUG: Path no. %d starts on zero priority waypoint.\n", path_index + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);
			}
		}

		// look for invalid path ends ie. paths not ending at a cross waypoint or not ending at ammobox etc.
		CheckInvalidPathEnd(path_index, log_in_file);

		// also report an invalid merge of paths
		RepairInvalidPathMerge(path_index, false, log_in_file);

		// look for multiple addition of the same waypoint into this path
		W_PATH* p = w_paths[path_index];

		while (p)
		{
			// while going through the whole path waypoint by waypoint we will also check for specific waypoint types that need to be in a pair (direct neighbours) on the path
			if (wptmanager.IsWaypoint(p->wpt_index, WptT::roadblock) && (wptmanager.IsWaypointTypeNeighbourOnPath(p, WptT::roadblock) == false))
			{
				sprintf(msg, "BUG: Roadblock waypoint no. %d is not in a pair on path no. %d.\n", p->wpt_index + 1, path_index + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);
			}

			W_PATH* rp = p->next;				// start on the next waypoint from this path ...
			int stop = 0;

			// next we're going to check the distance between the neighbouring waypoints on the path because of given limit on reaching the waypoint in time (we're ignoring parachute waypoints)
			if (rp && (wptmanager.GetDistanceBetweenWaypoints(p->wpt_index, rp->wpt_index) > 1100.0f) && (wptmanager.IsWaypoint(p->wpt_index, WptT::parachute) == false))
			{
				sprintf(msg, "WARNING: Waypoints no. %d and %d on path no. %d are quite far from each other\n", p->wpt_index + 1, rp->wpt_index + 1, path_index + 1);
				wptoutputer.AddWarning(2);
				wptoutputer.ProcessIt(msg, log_in_file);

				sprintf(msg, "     The bot may have to reset the navigation and thus lose the direction while moving between them.\n");
				wptoutputer.ProcessIt(msg, log_in_file);
			}

			while (rp)
			{
				// go through the rest of the path and look for repeated addition of the same waypoint
				if (rp->wpt_index == p->wpt_index)
				{
					sprintf(msg, "BUG: Waypoint no. %d has been added more than once to path no. %d.\n", p->wpt_index + 1, path_index + 1);
					wptoutputer.AddError();
					wptoutputer.ProcessIt(msg, log_in_file);
				}

				rp = rp->next;

				stop++;
				if (stop > LINKEDLIST_LOOPS_THRESHOLD)
					LinkedListError("Check Paths For Problems (nested search)", path_index);
			}

			p = p->next;

			safety_stop++;
			if (safety_stop > LINKEDLIST_LOOPS_THRESHOLD)
				LinkedListError("Check Paths For Problems", path_index);
		}
	}

	// now go through all waypoints and check for solitary waypoints (i.e. waypoints that aren't part of any path)
	for (int wpt_index = 0; wpt_index < num_waypoints; wpt_index++)
	{
		// skip all waypoints that aren't allowed in paths
		if (wptmanager.IsWaypoint(wpt_index, WptT::deleted, WptT::aim, WptT::cross))
			continue;

		// if we can't find any path on this waypoint then it must be solitary
		if (wptmanager.FindPath(wpt_index) == NO_VAL)
		{
			sprintf(msg, "WARNING: Waypoint no. %d isn't part of any path. There should NOT be solitary waypoints except for 'aim' and 'cross'.\n", wpt_index + 1);
			wptoutputer.AddWarning();
			wptoutputer.ProcessIt(msg, log_in_file);
		}
	}

	// and of course return this override switch back to default state
	wptoutputer.ResetOverrideCounterReset();

	return;
}


/*
* checks waypoint for invalid flags/tags combination and fixes them if not used in purely checking mode
* fixes also zero flag/tag by making such waypoint a deleted one
* returns 1 if everything was okay
* returns 0 if there was any flag/tag repair done
*/
int waypoints_and_paths_repair_functions_t::RepairInvalidCombinationOfWaypointFlags(int wpt_index, bool repair_it, bool log_in_file)
{
	// to know we have fixed something
	bool fixed = false;
	char msg[256];

	if (wpt_index == NO_VAL)
		return -1;

	// reset all the counters
	wptoutputer.ResetCounters();

	// the order is NOT alphabetical and it should not be so, we are following the logical priority here (or common sense if you wish)
	// i.e. we are trying to always remove that "stupid" additional flag, also you don't have to worry about missing flag/tag for the stance, because
	// even if we remove the stance flag (e.g. prone) in some statement, there is always the default flag (w_fl_std) present on that waypoint,
	// it's just so that the functions like wpt info don't show it in certain cases

	if ((waypoints[wpt_index].flags & W_FL_AMMOBOX) || (waypoints[wpt_index].flags & W_FL_USE))
	{
		// ammobox and use waypoints are taken as gobacks by default if they are ending a path therefore the goback flag/tag shouldn't be there
		if (waypoints[wpt_index].flags & W_FL_GOBACK)
		{
			if (repair_it)
			{
				waypoints[wpt_index].flags &= ~W_FL_GOBACK;
				fixed = true;
			}
			else
			{
				if (wptmanager.IsWaypoint(wpt_index, WptT::ammobox))
				{
					sprintf(msg, "BUG: Invalid combination of types ammobox and goback on waypoint no. %d.\n", wpt_index + 1);
				}
				else
				{
					sprintf(msg, "BUG: Invalid combination of types goback and use on waypoint no. %d.\n", wpt_index + 1);
				}

				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);
			}
		}
	}

	// once the waypoint is ammobox then it cannot be a use one
	if (waypoints[wpt_index].flags & W_FL_AMMOBOX)
	{
		if (waypoints[wpt_index].flags & W_FL_USE)
		{
			if (repair_it)
			{
				waypoints[wpt_index].flags &= ~W_FL_USE;
				fixed = true;
			}
			else
			{
				sprintf(msg, "BUG: Invalid combination of types ammobox and use on waypoint no. %d.\n", wpt_index + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);
			}
		}
	}

	// parachute waypoint is meant as a check gate ... you can pass it only if you have the parachute ... nothing is opened or used there
	if (waypoints[wpt_index].flags & W_FL_CHUTE)
	{
		if (waypoints[wpt_index].flags & W_FL_DOOR)
		{
			if (repair_it)
			{
				waypoints[wpt_index].flags &= ~W_FL_DOOR;
				fixed = true;
			}
			else
			{
				sprintf(msg, "BUG: Invalid combination of types door and parachute on waypoint no. %d.\n", wpt_index + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);
			}
		}

		if (waypoints[wpt_index].flags & W_FL_DOORUSE)
		{
			if (repair_it)
			{
				waypoints[wpt_index].flags &= ~W_FL_DOORUSE;
				fixed = true;
			}
			else
			{
				sprintf(msg, "BUG: Invalid combination of types parachute and usedoor on waypoint no. %d.\n", wpt_index + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);
			}
		}

		// it serves as a turnback point by itself if the bot doesn't carry the parachute
		if (waypoints[wpt_index].flags & W_FL_GOBACK)
		{
			if (repair_it)
			{
				waypoints[wpt_index].flags &= ~W_FL_GOBACK;
				fixed = true;
			}
			else
			{
				sprintf(msg, "BUG: Invalid combination of types parachute and goback on waypoint no. %d.\n", wpt_index + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);
			}
		}

		// you can't go prone during parachuting
		if (waypoints[wpt_index].flags & W_FL_PRONE)
		{
			if (repair_it)
			{
				waypoints[wpt_index].flags &= ~W_FL_PRONE;
				fixed = true;
			}
			else
			{
				sprintf(msg, "BUG: Invalid combination of types parachute and prone on waypoint no. %d.\n", wpt_index + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);
			}
		}

		if (waypoints[wpt_index].flags & W_FL_USE)
		{
			if (repair_it)
			{
				waypoints[wpt_index].flags &= ~W_FL_USE;
				fixed = true;
			}
			else
			{
				sprintf(msg, "BUG: Invalid combination of types parachute and use on waypoint no. %d.\n", wpt_index + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);
			}
		}
	}

	// 'use door' is a stand-alone waypoint not a combination of door and use waypoints
	if (waypoints[wpt_index].flags & W_FL_DOORUSE)
	{
		if (waypoints[wpt_index].flags & W_FL_DOOR)
		{
			if (repair_it)
			{
				waypoints[wpt_index].flags &= ~W_FL_DOOR;
				fixed = true;
			}
			else
			{
				sprintf(msg, "BUG: Invalid combination of types door and usedoor on waypoint no. %d.\n", wpt_index + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);
			}
		}

		if (waypoints[wpt_index].flags & W_FL_USE)
		{
			if (repair_it)
			{
				waypoints[wpt_index].flags &= ~W_FL_USE;
				fixed = true;
			}
			else
			{
				sprintf(msg, "BUG: Invalid combination of types door and use on waypoint no. %d.\n", wpt_index + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);
			}
		}
	}

	// either duckjump or just jump ... you can't do both
	if (waypoints[wpt_index].flags & W_FL_DUCKJUMP)
	{
		if (waypoints[wpt_index].flags & W_FL_JUMP)
		{
			if (repair_it)
			{
				waypoints[wpt_index].flags &= ~W_FL_JUMP;
				fixed = true;
			}
			else
			{
				sprintf(msg, "BUG: Invalid combination of types duckjump and jump on waypoint no. %d.\n", wpt_index + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);
			}
		}
	}

	// ladder and prone stance don't work
	if ((waypoints[wpt_index].flags & W_FL_LADDER))
	{
		if (waypoints[wpt_index].flags & W_FL_PRONE)
		{
			if (repair_it)
			{
				waypoints[wpt_index].flags &= ~W_FL_PRONE;
				fixed = true;
			}
			else
			{
				sprintf(msg, "BUG: Invalid combination of types ladder and prone on waypoint no. %d.\n", wpt_index + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);
			}
		}
	}

	// you can't sprint when crouched or proned
	if (waypoints[wpt_index].flags & W_FL_SPRINT)
	{
		if (waypoints[wpt_index].flags & W_FL_CROUCH)
		{
			if (repair_it)
			{
				waypoints[wpt_index].flags &= ~W_FL_CROUCH;
				fixed = true;
			}
			else
			{
				sprintf(msg, "BUG: Invalid combination of types crouch and sprint on waypoint no. %d.\n", wpt_index + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);
			}
		}

		if (waypoints[wpt_index].flags & W_FL_PRONE)
		{
			if (repair_it)
			{
				waypoints[wpt_index].flags &= ~W_FL_PRONE;
				fixed = true;
			}
			else
			{
				sprintf(msg, "BUG: Invalid combination of types prone and sprint on waypoint no. %d.\n", wpt_index + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);
			}
		}
	}

	// you can be either proned or crouched not both
	if (waypoints[wpt_index].flags & W_FL_PRONE)
	{
		if (waypoints[wpt_index].flags & W_FL_CROUCH)
		{
			if (repair_it)
			{
				waypoints[wpt_index].flags &= ~W_FL_CROUCH;
				fixed = true;
			}
			else
			{
				sprintf(msg, "BUG: Invalid combination of types crouch and prone on waypoint no. %d.\n", wpt_index + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);
			}
		}

		// also no jump or duckjump when one is proned
		if (waypoints[wpt_index].flags & W_FL_DUCKJUMP)
		{
			if (repair_it)
			{
				waypoints[wpt_index].flags &= ~W_FL_DUCKJUMP;
				fixed = true;
			}
			else
			{
				sprintf(msg, "BUG: Invalid combination of types duckjump and prone on waypoint no. %d.\n", wpt_index + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);
			}
		}

		if (waypoints[wpt_index].flags & W_FL_JUMP)
		{
			if (repair_it)
			{
				waypoints[wpt_index].flags &= ~W_FL_JUMP;
				fixed = true;
			}
			else
			{
				sprintf(msg, "BUG: Invalid combination of types jump and prone on waypoint no. %d.\n", wpt_index + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);
			}
		}
	}

	// roadblock is taken as a turnback point if the path is blocked so there's no point to add a goback tag too
	if (waypoints[wpt_index].flags & W_FL_ROADBLOCK)
	{
		if (waypoints[wpt_index].flags & W_FL_GOBACK)
		{
			if (repair_it)
			{
				waypoints[wpt_index].flags &= ~W_FL_GOBACK;
				fixed = true;
			}
			else
			{
				sprintf(msg, "BUG: Invalid combination of types roadblock and goback on waypoint no. %d.\n", wpt_index + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);
			}
		}
	}

	// if it happened that this waypoint flag is zero (i.e. there is no type set on this waypoint, which causes weird things such as aim & cross connections going to map origin etc.)
	// convert it to deleted flag - do NOT remove this statement - it's a safety catch!
	if (waypoints[wpt_index].flags == 0)
	{
		waypoints[wpt_index].flags |= W_FL_DELETED;

		// to prevent confusion when we use the checking mode since this way we will never print the universal ALERT message
		if (repair_it)
			fixed = true;
	}

	// in the checking mode we never set the 'fixed' variable so in order to return correct value we'll use the error & warning counter
	if (wptoutputer.GetAmountOfFoundIssues() > 0)
		return 0;

	// did we fix anything?
	if (fixed)
	{
		// then make the client know about it (this way it will be visible only when developer mode is on)
		ALERT(at_console, "Waypoint self cleaning fixed invalid flags/tags/types combination on waypoint #%d\n", wpt_index + 1);

		return 0;
	}

	// everything was okay
	return 1;
}


/*
* checks the waypoint for a combination of zero priority and set wait time for the same team which is nonsense, because the bot will then ignore the wait time
* if such combination is found then based on the boolean switch it will be either reported or fixed
*/
int waypoints_and_paths_repair_functions_t::RepairInvalidCombinationOfWaypointPriorityAndTime(int wpt_index, bool repair_it, bool log_in_file)
{
	bool report_it = false;

	if (wpt_index == NO_VAL)
		return NO_VAL;

	// ignore trigger waypoints, because there this can be set on purpose to allow bot act in specific way... eg. start camping at some spot after capturing some objective
	if (wptmanager.IsWaypoint(wpt_index, WptT::trigger))
		return 0;

	wptoutputer.ResetCounters();

	if ((wptmanager.GetWaypointPriority(wpt_index, teamONE.GetTeamId()) == 0) && (wptmanager.GetWaypointWaitTime(wpt_index, teamONE.GetTeamId()) > 0.0f))
	{
		if (repair_it)
		{
			// on shoot waypoint don't reset the priority, but only the wait time
			if (wptmanager.IsWaypoint(wpt_index, WptT::shoot) == false)
				waypoints[wpt_index].red_priority = MAX_WPT_PRIOR;
			
			waypoints[wpt_index].red_time = 0.0f;
			return 1;
		}
		else
			report_it = true;
	}
	else if ((wptmanager.GetWaypointPriority(wpt_index, teamTWO.GetTeamId()) == 0) && (wptmanager.GetWaypointWaitTime(wpt_index, teamTWO.GetTeamId()) > 0.0f))
	{
		if (repair_it)
		{
			if (wptmanager.IsWaypoint(wpt_index, WptT::shoot) == false)
				waypoints[wpt_index].blue_priority = MAX_WPT_PRIOR;
			
			waypoints[wpt_index].blue_time = 0.0f;
			return 1;
		}
		else
			report_it = true;
	}

	if (report_it)
	{
		char msg[TEXT_MSG_SIZE];

		sprintf(msg, "BUG: At least for one team is the wait time disabled by zero priority on waypoint no. %d.\n", wpt_index + 1);
		wptoutputer.AddError();
		wptoutputer.ProcessIt(msg, log_in_file);

		return 1;
	}

	return 0;
}


/*
* goes through all waypoints in order to fix disabled wait time by using zero priority
*/
void waypoints_and_paths_repair_functions_t::RepairInvalidCombinationOfWaypointPriorityAndTime(void)
{
	for (int wpt_index = 0; wpt_index < num_waypoints; wpt_index++)
	{
		if (RepairInvalidCombinationOfWaypointPriorityAndTime(wpt_index, true, false) == 1)
			ALERT(at_console, "Wait time disabled by zero priority on waypoint no. %d was repaired!\n", wpt_index + 1);
	}
}


/*
* checks cross waypoint surrounding whether there's a free path end there
* if so the range on cross waypoint will be increased to reach such path end waypoint
* also checks whether the range isn't too large to limit adding unwanted waypoints to the range to a minimum
* returns -1.0 if the waypoint isn't valid
* otherwise it returns cross waypoint range
*/
float waypoints_and_paths_repair_functions_t::RepairCrossWaypointRange(int wpt_index)
{
	int safety_counter = 0;
	bool keep_trying = true;
	int ignore_this_wpt = NO_VAL;
	float max_range = 0.0f;



	// TODO:	Might be a good idea to change all the console output to the new system "wptoutputer"
	//			including the output in both self controlled fixing functions 



	if ((wpt_index == NO_VAL) || (wptmanager.IsWaypoint(wpt_index, WptT::cross) == false))
		return -1.0;

	// this do-while cycle is a must to correctly handle the case of several paths where both ends are connected to the same cross waypoint
	do
	{
		// first try to find all loose path ends and increase cross waypoint range to reach them
		max_range = SelfControlledCrossWaypointRangeIncrease(wpt_index, ignore_this_wpt);

		// update the range if we had found any loose path end around
		if (max_range > 0.0f)
		{
			waypoints[wpt_index].range = max_range;

			ALERT(at_console, "Increasing the range to %.1f units for cross waypoint no. %d (NumberOfTries=%d)\n", waypoints[wpt_index].range, wpt_index + 1, safety_counter + 1);
		}

		// now we need to check the other case too ... ie. whether we will need to decrease the range, because keeping the range at the lowest possible value
		// should drop the number of unwanted waypoints connected to this cross waypoint to minimum
		max_range = SelfControlledCrossWaypointRangeDecrease(wpt_index, ignore_this_wpt);

		// did we find possible range reduction value for this cross waypoint then we should use it
		if ((max_range > 0.0f) && (max_range < waypoints[wpt_index].range))
		{
			waypoints[wpt_index].range = max_range;

			ALERT(at_console, "Decreasing the range to %.1f units for cross waypoint no. %d (NumberOfTries=%d)\n", waypoints[wpt_index].range, wpt_index + 1, safety_counter + 1);
		}

		// everything seems to be alright so there's no point checking it again
		if ((max_range > 0.0f) && (ignore_this_wpt == NO_VAL))
			keep_trying = false;

		safety_counter++;

		if (safety_counter > 10)
			keep_trying = false;

	} while (keep_trying);

	return waypoints[wpt_index].range;
}


/*
* goes through all cross waypoints in order to connect them with free path ends in their surrounding
*/
void waypoints_and_paths_repair_functions_t::RepairCrossWaypointRange(void)
{
	float original_range = 0.0f;
	float new_range = 0.0f;

	for (int wpt_index = 0; wpt_index < num_waypoints; wpt_index++)
	{
		if (wptmanager.IsWaypoint(wpt_index, WptT::cross))
		{
			// backup current range
			original_range = waypoints[wpt_index].range;

			new_range = RepairCrossWaypointRange(wpt_index);

			if (new_range != original_range)
				ALERT(at_console, "The range on cross waypoint no. %d was repaired!\n", wpt_index + 1);
		}
	}
}


/*
* tries to repair bad waypoint placement and range setting (ie. when the waypoint range does intersect walls etc.)
* the boolean switch allows just range change without reposition
*/
bool waypoints_and_paths_repair_functions_t::RepairWaypointRangeAndPosition(int wpt_index, edict_t* pEdict, bool dont_move)
{
	if ((wpt_index != NO_VAL) && (wpt_index < num_waypoints))
	{
		bool cant_move = false;			// to disable default reposition if we detect that the waypoint has a purpose (eg. when there are bandages right next to it)


		// TODO: We should handle aiming and cross waypoints getting out of range when the master/nearby waypoint got repositioned


		// for now do not work with these waypoints
		if (wptmanager.IsWaypoint(wpt_index, WptT::aim, WptT::cross, WptT::deleted) || wptmanager.IsWaypoint(wpt_index, WptT::ladder))
			return false;

		// get this waypoint range
		float the_range = waypoints[wpt_index].range;

		// we need more space for the whole body ... without this the bot would still hit the obstacle (basically add the body size to the range)
		the_range += 15.0f;

		Vector new_origin;
		edict_t* pent = NULL;
		// the distance we will move the waypoint origin
		float move_d = 10.0f;
		// value used to decrease the range
		float dec_r = 10.0f;

		// first check for some important entities around the waypoint
		while ((pent = util.FindEntityInSphere(pent, waypoints[wpt_index].origin, 30.0f)) != NULL)
		{
			// if there are bandages right next to the waypoint then do not reposition it
			//if ((strcmp(STRING(pent->v.classname), "item_bandage") == 0))										replaced by its own waypoint type now
			//{
			//	cant_move = true;
			//}

			// check for door entity and make the waypoint a door waypoint if it is right in the doorway
			if (util.IsDoorEntity(pent))
			{
				waypoints[wpt_index].flags |= W_FL_DOOR;
				waypoints[wpt_index].range = WPT_RANGE_SMALL;

				cant_move = true;
			}
		}

		// do not reposition jump waypoints else the bots may not be able to move towards enemy/goal at all
		if (wptmanager.IsWaypoint(wpt_index, WptT::duckjump, WptT::jump))
			cant_move = true;

		// don't move with bandage waypoints also roadblocks shouldn't be moved, because then the tracelines may miss target entity
		if (wptmanager.IsWaypoint(wpt_index, WptT::bandage, WptT::roadblock))
			cant_move = true;

		if (cant_move)
			ALERT(at_console, "CANNOT REPOSITION WAYPOINT #%d IT COULD LOSE ITS PURPOSE!!!\n", wpt_index + 1);

		// we've found that this waypoint has a purpose at its current position so we must prevent its reposition
		if (cant_move && !dont_move)
			dont_move = true;

		// init the new origin with the original waypoint position for the first run
		new_origin = waypoints[wpt_index].origin;

		// first check for obstacles using standard waypoint origin
		SelfControlledWaypointReposition(the_range, new_origin, move_d, dec_r, dont_move, pEdict);

		// drop the waypoint origin a few units lower to catch sandbags, ledges and like ...
		new_origin = new_origin - Vector(0, 0, 15);

		// check for obstacles there ...
		SelfControlledWaypointReposition(the_range, new_origin, move_d, dec_r, dont_move, pEdict);

		// and return the origin back where it should be
		new_origin = new_origin + Vector(0, 0, 15);

		// remove the body size addition, but don't go under range == 5
		if (the_range >= 20.0f)
			the_range -= 15.0f;

		// final safety check ie. the range must not be a negative value therefore we use this tiny range that works even in case of narrow passage (eg. doors)
		if (the_range < 0.0f)																						// NEW CODE 095
		{
			the_range = 5.0f;

#ifdef DEBUG

			util.DebugInFile("WptRepairRange&Pos() - Range was negative --> Something went wrong!");

#endif // DEBUG

		}																										// NEW CODE 095 END

		// and set the tweaked range now
		waypoints[wpt_index].range = the_range;

		// if we moved the waypoint then overwrite the original waypoint position with the new one (ie. now we really do move the waypoint itself)
		if (new_origin != waypoints[wpt_index].origin)
		{
			waypoints[wpt_index].origin = new_origin;

			ALERT(at_console, "Waypoint #%d was moved to NEW position (%.1f, %.1f, %.1f)\n", wpt_index + 1,
				waypoints[wpt_index].origin.x, waypoints[wpt_index].origin.y, waypoints[wpt_index].origin.z);
		}

		// check for door entity and make the waypoint a door waypoint if it is right in the doorway
		while ((pent = util.FindEntityInSphere(pent, waypoints[wpt_index].origin, 30.0f)) != NULL)
		{
			if (util.IsDoorEntity(pent))
			{
				waypoints[wpt_index].flags |= W_FL_DOOR;
				if (the_range > WPT_RANGE_SMALL)
					waypoints[wpt_index].range = WPT_RANGE_SMALL;
			}
		}

		while ((pent = util.FindEntityInSphere(pent, waypoints[wpt_index].origin, 50.0f)) != NULL)
		{
			if (strcmp(STRING(pent->v.classname), "ammobox") == 0)
			{
				waypoints[wpt_index].flags |= W_FL_AMMOBOX;
				if (the_range > WPT_RANGE_SMALL)
					waypoints[wpt_index].range = WPT_RANGE_SMALL;
			}
		}

		return true;
	}

	return false;
}


/*
* goes through all waypoints in order to fix their range and/or tweak their position
*/
void waypoints_and_paths_repair_functions_t::RepairWaypointRangeAndPosition(edict_t* pEdict, bool dont_move)
{
	for (int index = 0; index < num_waypoints; index++)
		RepairWaypointRangeAndPosition(index, pEdict, dont_move);

	return;
}


/*
* checks both path ends for possibility of incorrectly made sniper spot
* returns -1 if the path isn't valid
* returns 0 if there's nothing to be fixed
* returns 1 if there was something fixed
*/
int waypoints_and_paths_repair_functions_t::RepairSniperSpot(int path_index)
{
	// first check the validity
	if ((path_index == NO_VAL) || (w_paths[path_index] == NULL))
		return -1;

	int start_waypoint = -1;				// index of the first waypoint on this path that we need to check
	int end_waypoint = -1;					// index of the last waypoint on this path that we need to check
	bool path_start_okay = false;			// when the first path waypoint doesn't need to be checked
	bool path_end_okay = false;				// when the last path waypoint doesn't need to be checked
	bool repair_done = false;				// when there was something fixed (gets set even in the checking mode)

	start_waypoint = wptmanager.GetPathStart(path_index);
	end_waypoint = wptmanager.GetPathEnd(path_index);

	// check validity
	if ((start_waypoint == NO_VAL) || (end_waypoint == NO_VAL))
		return -1;

	// if this path starts at cross waypoint then this isn't a sniper spot
	if (wptmanager.FindConnectedCross(waypoints[start_waypoint].origin, true) != NO_VAL)
	{
		path_start_okay = true;
	}

	// if this path ends at cross waypoint then this isn't a sniper spot
	if (wptmanager.FindConnectedCross(waypoints[end_waypoint].origin, true) != NO_VAL)
	{
		path_end_okay = true;
	}

	// if this path is an ammobox or use or search&destroy something path then this isn't a sniper spot case either
	if (wptmanager.IsWaypoint(start_waypoint, WptT::ammobox, WptT::use) ||
		wptmanager.IsWaypoint(start_waypoint, WptT::claymore, WptT::shoot))
	{
		path_start_okay = true;
	}

	if (wptmanager.IsWaypoint(end_waypoint, WptT::ammobox, WptT::use) ||
		wptmanager.IsWaypoint(end_waypoint, WptT::claymore, WptT::shoot))
	{
		path_end_okay = true;
	}

	// there's no sniper spot at either end of this path so we can break it here
	if (path_start_okay && path_end_okay)
		return 0;

	// check the path start
	if (path_start_okay == false)
	{
		if (FixSniperSpot(path_index, start_waypoint) == 1)
			repair_done = true;
	}

	// check the path end
	if (path_end_okay == false)
	{
		if (FixSniperSpot(path_index, end_waypoint) == 1)
			repair_done = true;
	}

	if (repair_done)
		return 1;

	return 0;
}


/*
* goes through all paths and checks both ends of each path for correctly made sniper spot
*/
void waypoints_and_paths_repair_functions_t::RepairSniperSpot(void)
{
	int result;

	for (int path_index = 0; path_index < num_w_paths; path_index++)
	{
		result = RepairSniperSpot(path_index);

		if (result == 1)
		{
			ALERT(at_console, "sniper spot at one of the ends of path #%d was repaired\n", path_index + 1);
		}
	}

	return;
}


/*
* checks either end of the path for correct ending
* (ie. there's a connection to a cross waypoint or one of the turn back waypoints)
* returns -1 if some error occured, 0 if the path was okay
* returns 1 if there was anything repaired and 10 if there was an unfixable problem
*/
int waypoints_and_paths_repair_functions_t::RepairInvalidPathEnd(int path_index)
{
	// first check the validity
	if ((path_index == NO_VAL) || (w_paths[path_index] == NULL))
		return -1;

	int end_waypoint = NO_VAL;

	// if it is a one way path then we will check just the path end
	if (wptmanager.IsPath(path_index, PathT::one_way))
	{
		end_waypoint = wptmanager.GetPathEnd(path_index);

		// check validity
		if (end_waypoint == NO_VAL)
			return -1;

		// check for cases when a one-way path ends with a goback waypoint
		if (wptmanager.IsWaypoint(end_waypoint, WptT::goback))
		{
			// remove one-way direction bit
			w_paths[path_index]->flags &= ~P_FL_WAY_ONE;

			// and make the path a two-way path from now
			w_paths[path_index]->flags |= P_FL_WAY_TWO;

			return 1;
		}

		// if this path ends at cross waypoint then everything is fine and we can stop it here
		if (wptmanager.FindConnectedCross(waypoints[end_waypoint].origin) != NO_VAL)
		{
			return 0;
		}

		// there is a waypoint nearby so this path doesn't end in a void and we can stop it here 
		if (wptmanager.FindNearestOrdinaryWaypoint(end_waypoint, path_index) != NO_VAL)
		{
			return 0;
		}

		// if neither of these waypoints is on the path then turn the path into a two-way path ...
		if ((wptmanager.IsWaypointTypeOnPath(WptT::parachute, path_index) == false) &&
			(wptmanager.IsWaypointTypeOnPath(WptT::duckjump, path_index) == false) &&
			(wptmanager.IsWaypointTypeOnPath(WptT::jump, path_index) == false))
		{
			// go through all paths
			for (int other_path = 0; other_path < num_w_paths; other_path++)
			{
				if (other_path == path_index)
					continue;

				// check if this waypoint is part of any other path and is somewhere inside that other other (ie. NOT at either of its end)
				if (wptmanager.IsWaypointOnPath(end_waypoint, other_path) && (wptmanager.IsWaypointAtPathEnd(end_waypoint, other_path) == false))
				{
					// then this cannot be repaired
					return 10;
				}
			}

			w_paths[path_index]->flags &= ~P_FL_WAY_ONE;
			w_paths[path_index]->flags |= P_FL_WAY_TWO;

			// and make the end waypoint a goback one unless there already is a turn back marker set
			if (waypoints[end_waypoint].flags & PATH_TURNBACK)
				;
			else
				waypoints[end_waypoint].flags |= W_FL_GOBACK;

			return 1;
		}

		return 10;
	}

	// this path is a two way or patrol path ... so let's check start first	
	end_waypoint = wptmanager.GetPathStart(path_index);

	if (end_waypoint == NO_VAL)
		return -1;

	bool repair_done = false;

	// if this path is ended in a right way we will just continue because we still need to check the other end as well
	if (waypoints[end_waypoint].flags & PATH_TURNBACK)
		;
	else
	{
		// is there no cross at path start?
		if (wptmanager.FindConnectedCross(waypoints[end_waypoint].origin) == NO_VAL)
		{
			// is there no other available waypoint at path start?
			if (wptmanager.FindNearestOrdinaryWaypoint(end_waypoint, path_index) == NO_VAL)
			{
				// go through all paths
				for (int other_path = 0; other_path < num_w_paths; other_path++)
				{
					if (other_path == path_index)
						continue;

					// check if this waypoint is part of any other path and is somewhere inside that other other (ie. NOT at either of its end)
					if (wptmanager.IsWaypointOnPath(end_waypoint, other_path) && (wptmanager.IsWaypointAtPathEnd(end_waypoint, other_path) == false))
					{
						// then this cannot be repaired and there is no point trying to check the end of this path either
						return 10;
					}
				}

				// turn the waypoint into goback and set repair done so we know we have fixed something
				waypoints[end_waypoint].flags |= W_FL_GOBACK;
				repair_done = true;
			}
		}
	}

	// now check the path end
	end_waypoint = wptmanager.GetPathEnd(path_index);

	if (end_waypoint == NO_VAL)
		return -1;

	// we can't stop it here because there's a chance we would return false result if the start was repaired
	if (waypoints[end_waypoint].flags & PATH_TURNBACK)
		;
	else
	{
		if (wptmanager.FindConnectedCross(waypoints[end_waypoint].origin) == NO_VAL)
		{
			if (wptmanager.FindNearestOrdinaryWaypoint(end_waypoint, path_index) == NO_VAL)
			{
				for (int other_path = 0; other_path < num_w_paths; other_path++)
				{
					if (other_path == path_index)
						continue;

					if (wptmanager.IsWaypointOnPath(end_waypoint, other_path) && (wptmanager.IsWaypointAtPathEnd(end_waypoint, other_path) == false))
					{
						return 10;
					}
				}

				waypoints[end_waypoint].flags |= W_FL_GOBACK;
				repair_done = true;
			}
		}
	}

	// did we repair something?
	if (repair_done)
		return 1;

	// everything was okay
	return 0;
}


/*
* goes through all paths and checks either end of the path for correct ending
* (ie. there's a connection to a cross waypoint or one of the turn back waypoints)
*/
void waypoints_and_paths_repair_functions_t::RepairInvalidPathEnd(void)
{
	int result;
	char msg[64];

	for (int path_index = 0; path_index < num_w_paths; path_index++)
	{
		result = RepairInvalidPathEnd(path_index);

		switch (result)
		{
		case 1:
		{
			sprintf(msg, "path #%d was repaired\n", path_index + 1);

			ALERT(at_console, msg);
			util.DebugInFile(msg);		// send this event in error log

			break;
		}
		case 10:
		{
			sprintf(msg, "unable to repair path #%d\n", path_index + 1);

			ALERT(at_console, msg);
			util.DebugInFile(msg);

			break;
		}
		}
	}

	return;
}


/*
* checks both ends of path whether there is a start or end of another path there without connection to cross waypoint
* (ie. the path start/end waypoint isn't connected to a cross waypoint yet there starts or ends another path)
* we are also checking for either of the turn back markers (goback, ammobox or use)
* it can be used in purely checking mode where there is no repairing done
* returns -1 if some error occured
* returns 0 if the path was okay
* returns 1 if there was anything repaired
* returns 2 if there was detected suspicious connection
* NOT USED --- returns 3 if there was anything repaired as well as detected suspicious connection
* returns 4 if there was detected unfixable connection
*/
int waypoints_and_paths_repair_functions_t::RepairInvalidPathMerge(int path_index, bool repair_it, bool log_in_file)
{
	if ((path_index == NO_VAL) || (w_paths[path_index] == NULL))
		return -1;

	int end_waypoint = NO_VAL;
	int start_waypoint = NO_VAL;
	int other_end_waypoint = NO_VAL;
	int other_start_waypoint = NO_VAL;
	bool starts_at_cross = false;
	bool ends_at_cross = false;
	bool invalid_merge_at_start = false;
	bool invalid_merge_at_end = false;
	bool suspicious_start_of_oneway_path_detected = false;
	bool unfixable_merge_detected = false;
	char msg[256];

	start_waypoint = wptmanager.GetPathStart(path_index);
	end_waypoint = wptmanager.GetPathEnd(path_index);

	if ((start_waypoint == NO_VAL) || (end_waypoint == NO_VAL))
		return -1;

	if (wptmanager.FindConnectedCross(waypoints[start_waypoint].origin, true) != NO_VAL)
		starts_at_cross = true;

	if (wptmanager.FindConnectedCross(waypoints[end_waypoint].origin, true) != NO_VAL)
		ends_at_cross = true;

	// if both path ends are connected to a cross waypoint then all is fine and we can stop right away
	if (starts_at_cross && ends_at_cross)
	{
		return 0;
	}

	// if it starts at cross waypoint AND ends with a valid turn-back marker then all is fine
	if (starts_at_cross && (waypoints[end_waypoint].flags & PATH_TURNBACK))
	{
		return 0;
	}

	// if it ends at cross waypoint AND starts with a valid turn-back marker then all is fine again
	if (ends_at_cross && (waypoints[start_waypoint].flags & PATH_TURNBACK))
	{
		return 0;
	}

	// first reset the error counters
	wptoutputer.ResetCounters();

	for (int other_path = 0; other_path < num_w_paths; other_path++)
	{
		if (other_path == path_index)
			continue;

		other_start_waypoint = wptmanager.GetPathStart(other_path);
		other_end_waypoint = wptmanager.GetPathEnd(other_path);

		// path does NOT start at cross waypoint, but starts on the same waypoint as the other path starts OR ends so this must be invalid merge of paths
		if (!starts_at_cross && ((start_waypoint == other_start_waypoint) || (start_waypoint == other_end_waypoint)))
		{
			// fix false positive, because one-way paths can start on one waypoint even outside any cross waypoint
			if ((start_waypoint == other_start_waypoint) &&	wptmanager.IsPath(path_index, PathT::one_way) && wptmanager.IsPath(other_path, PathT::one_way))
				;
			else
				invalid_merge_at_start = true;
		}

		// path does NOT end at cross waypoint but it ends on the same waypoint as the other path starts OR ends so this must be invalid merge of paths
		if (!ends_at_cross && ((end_waypoint == other_start_waypoint) || (end_waypoint == other_end_waypoint)))
			invalid_merge_at_end = true;

		// if there is invalid merge at one or the other path end then report it
		if (invalid_merge_at_start || invalid_merge_at_end)
		{
			// are we going to repair it?
			if (repair_it)
			{
				// now we must decide how will the paths be merged and seeing we always want to keep this path and discard the other path then there are these 4 cases ...

				// first case is typical invalid merge where the waypointer started a new path instead of continuing in the current one so
				// it's a simple append to the end of path_index path the final path will be path_index (start->end) + other path (start->end)
				if (invalid_merge_at_end && (end_waypoint == other_start_waypoint))
				{
					if (MergePaths(path_index, other_path, false) != path_index)
						return -1;
				}
				// second case is that both paths have the ending waypoint same so the final path will be path_index (start->end) + other path (end->start)
				else if (invalid_merge_at_end && (end_waypoint == other_end_waypoint))
				{
					if (MergePaths(path_index, other_path, true) != path_index)
						return -1;
				}
				// 3rd case is that the other path ends on the starting waypoint of this path so the final path will be other path (start->end) + path_index (start->end)
				// basically this is the same as case no. 1, but from the 'other path' point of view
				// but we want to keep this path - it has lower index so it was probably created first and is probably more important
				else if (invalid_merge_at_start && (start_waypoint == other_end_waypoint))
				{
					if (MergePathsInverted(path_index, other_path, false) != path_index)
						return -1;
				}
				// the final case is when both paths have the starting waypoint same so the final path will be other path (end->start) + path_index (start->end)
				// same as above we want to keep the path with lower index so we will use inverted merging again
				else// like -> else if (invalid_merge_at_start && (start_waypoint == other_start_waypoint))
				{
					if (MergePathsInverted(path_index, other_path, true) != path_index)
						return -1;
				}

				if (log_in_file)
				{
					// send this event straight to error log
					sprintf(msg, "Invalid merge on paths #%d and #%d has been REPAIRED!\n", path_index + 1, other_path + 1);
					wptoutputer.AddError();
					util.DebugInFile(msg);
				}
			}
			// or we will just report it
			else
			{
				sprintf(msg, "BUG: Invalid merge on paths no. %d and no. %d has been detected!\n", path_index + 1, other_path + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);
			}

			return 1;
		}

		// we also need to check for cases when this path starts inside another path
		// (not just the end or start waypoint of the other path but any waypoint from the other path == our path start waypoint)
		if (!starts_at_cross && wptmanager.IsWaypointOnPath(start_waypoint, other_path))
		{
			// fix false positive because one-way path can start basically on any waypoint however we will report it as a warning
			if (wptmanager.IsPath(path_index, PathT::one_way))
				suspicious_start_of_oneway_path_detected = true;
			else
			{
				if (repair_it == false)
				{
					sprintf(msg, "BUG: Invalid merge of paths detected! Path no. %d starts inside path no. %d\n", path_index + 1, other_path + 1);
					wptoutputer.AddError();
					wptoutputer.ProcessIt(msg, log_in_file);
				}
				// not enough data to figure out how such path should have looked like so we just report it
				else
				{
					sprintf(msg, "BUG: Invalid merge of paths detected! Path no. %d starts inside path no. %d\n     Unable to automatically fix such merge!\n",
						path_index + 1, other_path + 1);
					// there is additional message on new line so we must set the amount of text lines here
					wptoutputer.AddError(2);
					wptoutputer.ProcessIt(msg, log_in_file);
				}

				// unlike the classic invalid merge for one path end to another path end we can't break the cycle right away,
				// because the other end of this path wouldn't be checked while checking the other path therefore we'll just flag the finding
				unfixable_merge_detected = true;
			}
		}

		// and when this path ends inside another path (any waypoint from the other path == our path end waypoint)
		if (!ends_at_cross && wptmanager.IsWaypointOnPath(end_waypoint, other_path))
		{
			if (repair_it == false)
			{
				sprintf(msg, "BUG: Invalid merge of paths detected! Path no. %d ends inside path no. %d\n", path_index + 1, other_path + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);
			}
			else
			{
				sprintf(msg, "BUG: Invalid merge of paths detected! Path no. %d ends inside path no. %d\n     Unable to automatically fix such merge!\n",
					path_index + 1, other_path + 1);
				wptoutputer.AddError(2);
				wptoutputer.ProcessIt(msg, log_in_file);
			}

			unfixable_merge_detected = true;
		}
	}

	// it has to be outside the cycle as it's a low priority warning (there could be a serious bug at the end of this path that would have been ignored
	// if we broke the cycle at the moment this issue was found)
	if (suspicious_start_of_oneway_path_detected)
	{
		sprintf(msg, "WARNING: Detected suspicious start of one-way path no. %d\n", path_index + 1);
		wptoutputer.AddWarning();
		wptoutputer.ProcessIt(msg, log_in_file);

		// always return the right result
		if (unfixable_merge_detected)
			return 4;

		return 2;
	}

	if (unfixable_merge_detected)
		return 4;

	return 0;
}


/*
* goes through all paths and checks them for invalid merge of paths
* (ie. the end waypoint isn't connected to a cross waypoint yet there starts another path)
*/
void waypoints_and_paths_repair_functions_t::RepairInvalidPathMerge(void)
{
	int result;

	for (int path_index = 0; path_index < num_w_paths; path_index++)
	{
		result = RepairInvalidPathMerge(path_index, true, true);

		switch (result)
		{
		case 1:
			ALERT(at_console, "path #%d was repaired\n", path_index + 1);
			break;
		case 2:
			ALERT(at_console, "check path #%d because there was detected suspicious path connection\n", path_index + 1);
			break;
		case 3:
			ALERT(at_console, "path #%d was repaired, but there's also suspicious path connection on this path\n", path_index + 1);
			break;
		}
	}

	return;
}


/*
* deletes all invalid paths (ie path length = 1) as well as tries to fix those which include invalid waypoints
* returns total number of paths that has been either fixed or deleted
* if print_details is TRUE it also prints index of each removed or fixed path
*/
int waypoints_and_paths_repair_functions_t::DeleteInvalidPaths(bool print_details)
{
	int num_of_removed_paths = 0;
	int result = 0;
	char msg[64];

	wptoutputer.ResetCounters();

	for (int path_index = 0; path_index < num_w_paths; path_index++)
	{
		// first remove all invalid waypoints from this path
		result = ValidatePath(path_index);

		// is path length equal 1 (then the whole path is invalid)
		if (wptmanager.GetPathLength(path_index) == 1)
		{
			// delete that path
			if (DeleteWholePath(path_index))
			{
				num_of_removed_paths++;

				// if removing actual path then clear also "pointer" on it
				if (path_index == internals.GetPathToContinue())
				{
					internals.ResetPathToContinue();
				}

				if (print_details)
				{
					sprintf(msg, "Invalid path (path no. %d) was removed\n", path_index + 1);
					wptoutputer.AddError();
					wptoutputer.ProcessIt(msg);
				}

				// we must reset it in order to prevent printing wrong messages
				// eg. "path no. 5 was removed" followed by "path no. 5 was repaired"
				result = 0;
			}
		}

		// finally set correct return value based on the result
		if (result == 1)
		{
			num_of_removed_paths++;

			if (print_details)
			{
				sprintf(msg, "Invalid path (path no. %d) was repaired\n", path_index + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg);
			}
		}
		else if (result == -1)
		{
			num_of_removed_paths += 600;		// there's max of 512 paths so this value is safe

			if (print_details)
			{
				sprintf(msg, "Unable to repair invalid path (path no. %d)\n", path_index + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg);
			}
		}
	}

	return num_of_removed_paths;
}


/*
* runs path repair routine multiple times if needed to make the path valid
* ie. there aren't any aim or cross waypoints on it and every path waypoint was added just once to the path
* returns -1 if there is such error and it cannot be fixed
* returns 0 if the path is valid (or doesn't exist)
* returns 1 if there was error that has been fixed
*/
int waypoints_and_paths_repair_functions_t::ValidatePath(int path_index)
{
	int path_validity = 2;
	int safety_stop = 0;

	// keep repairing the path until it's valid
	while (path_validity == 2)
	{
		path_validity = PurifyPath(path_index);

		// if things go really wrong then break the loop
		if (safety_stop > LINKEDLIST_LOOPS_THRESHOLD)
			path_validity = -1;

		safety_stop++;
	}

	return path_validity;
}


/*
* goes through all paths and tries to validate them
*/
void waypoints_and_paths_repair_functions_t::ValidatePath(void)
{
	int result;
	char msg[64];

	for (int path_index = 0; path_index < num_w_paths; path_index++)
	{
		result = ValidatePath(path_index);

		if (result == 1)
		{
			sprintf(msg, "path #%d was repaired\n", path_index + 1);

			ALERT(at_console, msg);
			util.DebugInFile(msg);		// send this event to error log
		}
		else if (result == -1)
		{
			sprintf(msg, "unable to repair path #%d\n", path_index + 1);

			ALERT(at_console, msg);
			util.DebugInFile(msg);
		}
	}

	return;
}


/*
* updates path status/flag based on certain waypoints on this path
*/
void waypoints_and_paths_repair_functions_t::UpdatePathStatus(int path_index)
{
	if (path_index == NO_VAL)
		return;

	W_PATH* p;
	int safety_stop = 0;
	bool is_ammo_wpt_present = false;
	bool is_bandage_wpt_present = false;
	bool is_claymore_wpt_present = false;
	bool is_pushpoint_wpt_present = false;
	bool is_roadblock_wpt_present = false;
	bool status_locked = false;

	p = w_paths[path_index];

	while (p)
	{
		safety_stop++;
		if (safety_stop > LINKEDLIST_LOOPS_THRESHOLD)
			LinkedListError("Update Path Status", path_index);

		if (wptmanager.IsWaypoint(p->wpt_index, WptT::ammobox))
		{
			patheditor.SetAutoTag(PathT::ammo_tag, path_index);
			is_ammo_wpt_present = true;
		}

		if (wptmanager.IsWaypoint(p->wpt_index, WptT::bandage))
		{
			patheditor.SetAutoTag(PathT::bandages_tag, path_index);
			is_bandage_wpt_present = true;
		}

		if ((wptmanager.IsWaypoint(p->wpt_index, WptT::claymore)) && wptmanager.IsWaypointPriority(p->wpt_index, 1))
		{
			patheditor.SetAutoTag(PathT::goal_explosives_tag, path_index);
			is_claymore_wpt_present = true;
		}

		if (wptmanager.IsWaypoint(p->wpt_index, WptT::pushpoint))
		{
			UpdatePathStatusPushpoint(p->wpt_index, path_index);
			is_pushpoint_wpt_present = true;
		}

		if (wptmanager.IsWaypoint(p->wpt_index, WptT::roadblock))
		{
			UpdatePathStatusRoadblock(p->wpt_index, path_index, status_locked);
			is_roadblock_wpt_present = true;
		}

		p = p->next;
	}

	// if the path is marked as 'use me if you are low on ammo' but there was no ammobox waypoint on it then...
	if (wptmanager.IsPath(path_index, PathT::ammo_tag) && (is_ammo_wpt_present == false))
	{
		// we must fix it
		patheditor.ResetAutoTag(PathT::ammo_tag, path_index);
	}

	if (wptmanager.IsPath(path_index, PathT::bandages_tag) && (is_bandage_wpt_present == false))
	{
		patheditor.ResetAutoTag(PathT::bandages_tag, path_index);
	}

	if (wptmanager.IsPath(path_index, PathT::goal_explosives_tag) && (is_claymore_wpt_present == false))
	{
		patheditor.ResetAutoTag(PathT::goal_explosives_tag, path_index);
	}

	if ((wptmanager.IsPath(path_index, PathT::goal_team_one_tag) || wptmanager.IsPath(path_index, PathT::goal_team_two_tag)) && (is_pushpoint_wpt_present == false))
	{
		patheditor.ResetAutoTag(PathT::goal_team_one_tag, path_index);
		patheditor.ResetAutoTag(PathT::goal_team_two_tag, path_index);
	}

	if (wptmanager.IsPath(path_index, PathT::roadblocked_tag) && (is_roadblock_wpt_present == false))
	{
		patheditor.ResetAutoTag(PathT::roadblocked_tag, path_index);
	}
}


/*
* swaps the team values for both the waypoints and the paths (eg. a path dedicated to TeamOne will become a path for TeamTwo)
*/
void waypoints_and_paths_repair_functions_t::SwapTeamsInWaypoints(void)
{
	int temp_priority = MAX_WPT_PRIOR;
	float temp_wait_time = 0.0f;

	for (int i = 0; i < num_waypoints; i++)
	{
		if (waypoints[i].flags & W_FL_DELETED)
			continue;
		
		temp_priority = waypoints[i].red_priority;
		waypoints[i].red_priority = waypoints[i].blue_priority;
		waypoints[i].blue_priority = temp_priority;

		temp_priority = waypoints[i].trigger_red_priority;
		waypoints[i].trigger_red_priority = waypoints[i].trigger_blue_priority;
		waypoints[i].trigger_blue_priority = temp_priority;

		temp_wait_time = waypoints[i].red_time;
		waypoints[i].red_time = waypoints[i].blue_time;
		waypoints[i].blue_time = temp_wait_time;
	}

	for (int i = 0; i < num_w_paths; i++)
	{
		if (w_paths[i] == NULL)
			continue;

		if (w_paths[i]->flags & P_FL_TEAM_RED)
		{
			w_paths[i]->flags &= ~P_FL_TEAM_RED;
			w_paths[i]->flags |= P_FL_TEAM_BLUE;
		}
		else if (w_paths[i]->flags & P_FL_TEAM_BLUE)
		{
			w_paths[i]->flags &= ~P_FL_TEAM_BLUE;
			w_paths[i]->flags |= P_FL_TEAM_RED;
		}
	}
}


/*
* checks whether given waypoint can have wait time assigned
* returns false when waypoint flag/tag/type allows assigning a wait time
* returns true when a wait time can NOT be assigned to this waypoint flag/tag/type
* simplified report disables the printing of additional message
*/
bool waypoints_and_paths_repair_functions_t::IsInvalidWaitTime(int wpt_index, bool simplified_report, bool log_in_file)
{
	// waypoint isn't valid so no point checking it
	if (wpt_index == NO_VAL)
		return false;

	char msg[256];
	char reset_time_msg[] = "    Use 'wpt reset time' on this waypoint to fix that.\n";

	// reset all the counters
	wptoutputer.ResetCounters();

	if (wptmanager.IsWaypoint(wpt_index, WptT::ammobox))
	{
		sprintf(msg, "BUG: There should be NO wait time on ammobox waypoint no. %d.\n", wpt_index + 1);

		if (simplified_report)
		{
			wptoutputer.AddError();
			wptoutputer.ProcessIt(msg, log_in_file);
		}
		else
		{
			// in order to maintain correct console output we must pass the right amount of text lines,
			// because of the additional message following this error
			// (ie. this will ensure that we will either print both lines or none)
			wptoutputer.AddError(2);
			wptoutputer.ProcessIt(msg, log_in_file);
			// additional message
			wptoutputer.ProcessIt(reset_time_msg, log_in_file);
		}

		// to know that this is an error
		return true;
	}

	if (wptmanager.IsWaypoint(wpt_index, WptT::duckjump))
	{
		sprintf(msg, "BUG: There should be NO wait time on duckjump waypoint no. %d.\n", wpt_index + 1);

		if (simplified_report)
		{
			wptoutputer.AddError();
			wptoutputer.ProcessIt(msg, log_in_file);
		}
		else
		{
			wptoutputer.AddError(2);
			wptoutputer.ProcessIt(msg, log_in_file);
			wptoutputer.ProcessIt(reset_time_msg, log_in_file);
		}

		return true;
	}

	if (wptmanager.IsWaypoint(wpt_index, WptT::jump))
	{
		sprintf(msg, "BUG: There should be NO wait time on jump waypoint no. %d.\n", wpt_index + 1);

		if (simplified_report)
		{
			wptoutputer.AddError();
			wptoutputer.ProcessIt(msg, log_in_file);
		}
		else
		{
			wptoutputer.AddError(2);
			wptoutputer.ProcessIt(msg, log_in_file);
			wptoutputer.ProcessIt(reset_time_msg, log_in_file);
		}

		return true;
	}

	if (wptmanager.IsWaypoint(wpt_index, WptT::ladder))
	{
		sprintf(msg, "BUG: There should be NO wait time on ladder waypoint no. %d.\n", wpt_index + 1);

		if (simplified_report)
		{
			wptoutputer.AddError();
			wptoutputer.ProcessIt(msg, log_in_file);
		}
		else
		{
			wptoutputer.AddError(2);
			wptoutputer.ProcessIt(msg, log_in_file);
			wptoutputer.ProcessIt(reset_time_msg, log_in_file);
		}

		return true;
	}

	if (wptmanager.IsWaypoint(wpt_index, WptT::sprint))
	{
		sprintf(msg, "BUG: There should be NO wait time on sprint waypoint no. %d.\n", wpt_index + 1);

		if (simplified_report)
		{
			wptoutputer.AddError();
			wptoutputer.ProcessIt(msg, log_in_file);
		}
		else
		{
			wptoutputer.AddError(2);
			wptoutputer.ProcessIt(msg, log_in_file);
			wptoutputer.ProcessIt(reset_time_msg, log_in_file);
		}

		return true;
	}

	return false;
}


/*
* looks for specific waypoint type in given path type (eg. goback waypoint on one-way path)
* returns true if such problem combination is found
* can also be used to look for not desired combinations (eg. parachute waypoint on one-way path)
*/
bool waypoints_and_paths_repair_functions_t::IsInvalidCombinationOfWaypointAndPath(int path_index, PathT path_type, WptT waypoint_type)
{
	// does this path match the one we are looking for
	if (wptmanager.IsPath(path_index, path_type))
	{
		// then see if there is a problem combination in it
		W_PATH* p = wptmanager.GetWaypointTypePointer(waypoint_type, path_index);

		// we found such combination
		if (p != NULL)
		{
			// now we just need to ignore team limited case, because the bot will also ignore such waypoint there
			// (eg. a goback waypoint with red team priority == 0 on red team only path is valid waypoint placement)
			if ((wptmanager.IsPath(path_index, PathT::team_one) && (waypoints[p->wpt_index].red_priority == 0)) ||
				(wptmanager.IsPath(path_index, PathT::team_two) && (waypoints[p->wpt_index].blue_priority == 0)))
				return false;

			return true;
		}
	}

	return false;
}


/*
* checks either end of the path for correct ending
* pretty much the same as Repair InvalidPathEnd() but we don't repair anything
* but we check things to a greater depth here and report them all
* returns -1 if some error occured, 0 if the path was okay
* returns 1 if there was invalid path end found
*/
int waypoints_and_paths_repair_functions_t::CheckInvalidPathEnd(int path_index, bool log_in_file)
{
	if ((path_index == NO_VAL) || (w_paths[path_index] == NULL))
		return -1;

	int end_waypoint = NO_VAL;
	char msg[256];
	const char* hint1 = "        It could be missing goback waypoint or small cross waypoint range.\n";

	// first reset the error counters
	wptoutputer.ResetCounters();

	if (wptmanager.IsPath(path_index, PathT::one_way))
	{
		end_waypoint = wptmanager.GetPathEnd(path_index);

		if (end_waypoint == NO_VAL)
			return -1;

		if (wptmanager.IsWaypoint(end_waypoint, WptT::goback))
		{
			sprintf(msg, "BUG: One-way path no. %d ends with a goback waypoint!\n", path_index + 1);

			// this will increase the amount of found bugs as well as lines of text needed to be printed on screen
			wptoutputer.AddError();

			// this will check whether the bug can still be printed on screen and will also handle writing it to error log if needed be
			wptoutputer.ProcessIt(msg, log_in_file);

			return 1;
		}

		if (wptmanager.FindConnectedCross(waypoints[end_waypoint].origin) != NO_VAL)
		{
			return 0;
		}

		if (wptmanager.FindNearestOrdinaryWaypoint(end_waypoint, path_index) != NO_VAL)
		{
			sprintf(msg, "WARNING: One-way path no. %d doesn't end at cross waypoint, but there seems to be a way from there. Check it!\n", path_index + 1);
			wptoutputer.AddError();
			wptoutputer.ProcessIt(msg, log_in_file);

			return 1;
		}
		else
		{
			sprintf(msg, "BUG: One-way path no. %d ends in a void ie. there's no cross waypoint or any other way out of there!\n", path_index + 1);
			wptoutputer.AddError();
			wptoutputer.ProcessIt(msg, log_in_file);

			return 1;
		}

		sprintf(msg, "Unknown state of one-way path no. %d Check it!\n", path_index + 1);
		wptoutputer.AddError();
		wptoutputer.ProcessIt(msg, log_in_file);

		return 10;
	}

	// two way or patrol path ... so let's check path start first	
	end_waypoint = wptmanager.GetPathStart(path_index);

	if (end_waypoint == NO_VAL)
		return -1;

	bool problem_found = false;

	if (waypoints[end_waypoint].flags & PATH_TURNBACK)
	{
		if (((wptmanager.IsPath(path_index, PathT::team_one, PathT::both_teams) && (wptmanager.GetWaypointPriority(end_waypoint, teamONE.GetTeamId()) == 0)) ||
			(wptmanager.IsPath(path_index, PathT::team_two, PathT::both_teams) && (wptmanager.GetWaypointPriority(end_waypoint, teamTWO.GetTeamId()) == 0))) &&
			(wptmanager.FindConnectedCross(waypoints[end_waypoint].origin) == NO_VAL) && (wptmanager.FindNearestOrdinaryWaypoint(end_waypoint, path_index) == NO_VAL))
		{
			sprintf(msg, "BUG: At least for one team path no. %d starts in a void ie. there's no cross waypoint or any other way to follow there!\n", path_index + 1);
			wptoutputer.AddError();
			wptoutputer.ProcessIt(msg, log_in_file);

			problem_found = true;
		}
	}
	else
	{
		if (wptmanager.FindConnectedCross(waypoints[end_waypoint].origin) == NO_VAL)
		{
			if (wptmanager.FindNearestOrdinaryWaypoint(end_waypoint, path_index) != NO_VAL)
			{
				sprintf(msg, "WARNING: Path no. %d doesn't start at cross waypoint, but there seems to be other way from there. Check it!\n", path_index + 1);
				wptoutputer.AddWarning();
				wptoutputer.ProcessIt(msg, log_in_file);

				// print the hint messages only into console, don't log them in file
				// so in order to always print correct output we'll have to handle these hints manually

				// to ensure that we'll print either both lines or no hint at all we must first let the system know how many text lines will be needed
				wptoutputer.IncPrintedLines(2);

				// now we can finally process them one by one
				sprintf(msg, "%s", hint1);
				wptoutputer.ProcessIt(msg);

				sprintf(msg, "        Or there are doors or breakable object between cross waypoint and paths' 1st waypoint.\n");
				wptoutputer.ProcessIt(msg);


				//sprintf(msg, "        It could be missing goback waypoint or small cross waypoint range.\n");
				//conOutput.Notify(msg);
				//sprintf(msg, "        Or there are doors or breakable object between cross waypoint and paths' 1st waypoint.\n");
				//conOutput.Notify(msg);


				problem_found = true;
			}
			else
			{
				sprintf(msg, "BUG: Path no. %d starts in a void ie. there's no cross waypoint or any other way to follow there!\n", path_index + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);

				problem_found = true;
			}
		}
	}

	// now check the path end
	end_waypoint = wptmanager.GetPathEnd(path_index);

	if (end_waypoint == NO_VAL)
		return -1;

	if (waypoints[end_waypoint].flags & PATH_TURNBACK)
	{
		if (((wptmanager.IsPath(path_index, PathT::team_one, PathT::both_teams) && (wptmanager.GetWaypointPriority(end_waypoint, teamONE.GetTeamId()) == 0)) ||
			(wptmanager.IsPath(path_index, PathT::team_two, PathT::both_teams) && (wptmanager.GetWaypointPriority(end_waypoint, teamTWO.GetTeamId()) == 0))) &&
			(wptmanager.FindConnectedCross(waypoints[end_waypoint].origin) == NO_VAL) && (wptmanager.FindNearestOrdinaryWaypoint(end_waypoint, path_index) == NO_VAL))
		{
			sprintf(msg, "BUG: At least for one team path no. %d ends in a void ie. there's no cross waypoint or any other way to follow there!\n", path_index + 1);
			wptoutputer.AddError();
			wptoutputer.ProcessIt(msg, log_in_file);

			problem_found = true;
		}
	}
	else
	{
		if (wptmanager.FindConnectedCross(waypoints[end_waypoint].origin) == NO_VAL)
		{
			if (wptmanager.FindNearestOrdinaryWaypoint(end_waypoint, path_index) != NO_VAL)
			{
				sprintf(msg, "WARNING: Path no. %d doesn't end at cross waypoint, but there seems to be other way from there. Check it!\n", path_index + 1);
				wptoutputer.AddWarning();
				wptoutputer.ProcessIt(msg, log_in_file);

				//sprintf(msg, "        It could be missing goback waypoint or small cross waypoint range.\n");
				//conOutput.Notify(msg);
				//sprintf(msg, "        Or there is breakable object or doors between cross and last path waypoint.\n");
				//conOutput.Notify(msg);

				wptoutputer.IncPrintedLines(2);

				sprintf(msg, "%s", hint1);
				wptoutputer.ProcessIt(msg);

				sprintf(msg, "        Or there are doors or breakable object between cross waypoint and last path waypoint.\n");
				wptoutputer.ProcessIt(msg);

				problem_found = true;
			}
			else
			{
				sprintf(msg, "BUG: Path no. %d ends in a void ie. there's no cross waypoint, no goback waypoint or any other way to follow there!\n", path_index + 1);
				wptoutputer.AddError();
				wptoutputer.ProcessIt(msg, log_in_file);

				problem_found = true;
			}
		}
	}

	if (problem_found)
		return 1;

	// everything was okay
	return 0;
}


/*
* checks all loose path end waypoints surrounding the cross waypoint
* in order to increase its range to reach all available ones
* returns new cross waypoint range
* returns zero range when the cross waypoint isn't valid
*/
float waypoints_and_paths_repair_functions_t::SelfControlledCrossWaypointRangeIncrease(int crosswpt_index, int& ignored_wpt)
{
	int path_end_wpt = NO_VAL;
	int path_start_wpt = NO_VAL;
	float distance_to_end_wpt = MAX_WPT_DIST;
	float distance_to_start_wpt = MAX_WPT_DIST;
	float max_range = 0.0f;
	TraceResult tr;

	if (ignored_wpt != NO_VAL)
		ALERT(at_console, "CrossWaypointRangeIncrease() called for waypoint no. %d (Ignoring wpt no. %d)\n", crosswpt_index + 1, ignored_wpt + 1);
	else
		ALERT(at_console, "CrossWaypointRangeIncrease() called for waypoint no. %d\n", crosswpt_index + 1);

	// we'll return zero range as an error if there's invalid cross waypoint index
	if (crosswpt_index == NO_VAL)
		return max_range;

	// go through all paths
	for (int path_index = 0; path_index < num_w_paths; path_index++)
	{
		// skip deleted paths
		if (w_paths[path_index] == NULL)
			continue;

		// get this path last waypoint
		path_end_wpt = wptmanager.GetPathEnd(path_index);

		if (path_end_wpt != NO_VAL)
		{
			// and its distance to this cross waypoint
			distance_to_end_wpt = wptmanager.GetDistanceBetweenWaypoints(path_end_wpt, crosswpt_index);
		}

		// now do the same for path start waypoint
		path_start_wpt = wptmanager.GetPathStart(path_index);

		if (path_start_wpt != NO_VAL)
		{
			distance_to_start_wpt = wptmanager.GetDistanceBetweenWaypoints(path_start_wpt, crosswpt_index);
		}

		// this end of this path isn't connected to any cross waypoint and
		// this path also doesn't end on one of turn back markers and
		// we are NOT going to ignore this waypoint and
		// this cross waypoint is the nearest one to this path end waypoint
		if ((path_end_wpt != NO_VAL) && (wptmanager.FindConnectedCross(waypoints[path_end_wpt].origin, true) == NO_VAL) &&
			(wptmanager.IsWaypoint(path_end_wpt, WptT::goback, WptT::ammobox, WptT::use) == false) &&
			(path_end_wpt != ignored_wpt) && (crosswpt_index == wptmanager.FindNearestCross(waypoints[path_end_wpt].origin, true)))
		{
			// path end waypoint is past the range of this cross waypoint
			// but close enough to be reachable for this cross waypoint
			if ((distance_to_end_wpt < MAX_WPT_DIST) && (distance_to_end_wpt > waypoints[crosswpt_index].range))
			{
				// check whether the waypoint is reachable from this cross waypoint
				UTIL_TraceLine(waypoints[crosswpt_index].origin, waypoints[path_end_wpt].origin, ignore_monsters, NULL, &tr);

				// the waypoint must be reachable and also it must be closer than the path start waypoint
				// (i.e. by default we should NOT connect both path ends to the same cross waypoint
				// although bot navigation can handle such case too)
				if ((tr.flFraction >= 1.0f) && (distance_to_end_wpt < distance_to_start_wpt))
				{
					ALERT(at_console, "checking waypoint no. %d (PATH END)\n", path_end_wpt + 1);

					// so let's increase cross waypoint range to reach this path end waypoint
					max_range = truncf(distance_to_end_wpt + 1.0f);
				}
			}
		}

		// do the same checks also for path start waypoint
		if ((path_start_wpt != NO_VAL) && (wptmanager.FindConnectedCross(waypoints[path_start_wpt].origin, true) == NO_VAL) &&
			(wptmanager.IsWaypoint(path_start_wpt, WptT::goback, WptT::ammobox, WptT::use) == false) &&
			(path_start_wpt != ignored_wpt) && (crosswpt_index == wptmanager.FindNearestCross(waypoints[path_start_wpt].origin, true)))
		{
			if ((distance_to_start_wpt < MAX_WPT_DIST) && (distance_to_start_wpt > waypoints[crosswpt_index].range))
			{
				UTIL_TraceLine(waypoints[crosswpt_index].origin, waypoints[path_start_wpt].origin, ignore_monsters, NULL, &tr);

				if ((tr.flFraction >= 1.0f) && (distance_to_start_wpt < distance_to_end_wpt))
				{
					ALERT(at_console, "checking waypoint no. %d (PATH START)\n", path_start_wpt + 1);

					max_range = truncf(distance_to_start_wpt + 1.0f);
				}
			}
		}
	}

	return max_range;
}


/*
* tries to reduce this cross waypoint range to a minimum value while
* keeping all valid path ends still connected to this cross waypoint
* also tries to disconnect the farther path end waypoint in cases
* when there is a path that has both its ends connected to this cross waypoint
* resets the ignored_wpt if it fails to exclude it from the range
* returns new cross waypoint range
* returns zero range when the cross waypoint isn't valid
*/
float waypoints_and_paths_repair_functions_t::SelfControlledCrossWaypointRangeDecrease(int crosswpt_index, int& ignored_wpt)
{
	float distance;
	float distance_to_ignored_wpt = 0.0f;
	float max_range = 0.0f;

	if (ignored_wpt != NO_VAL)
		ALERT(at_console, "CrossWaypointRangeDecrease() called for waypoint no. %d (Ignoring wpt no. %d)\n", crosswpt_index + 1, ignored_wpt + 1);
	else
		ALERT(at_console, "CrossWaypointRangeDecrease() called for waypoint no. %d\n", crosswpt_index + 1);

	// we'll return zero range as an error if there's invalid cross waypoint index
	if (crosswpt_index == NO_VAL)
		return max_range;

	// go through all waypoints
	for (int w_index = 0; w_index < num_waypoints; w_index++)
	{
		// ignore these
		if (wptmanager.IsWaypoint(w_index, WptT::aim, WptT::cross, WptT::deleted))
			continue;

		// get the distance to this cross waypoint
		distance = wptmanager.GetDistanceBetweenWaypoints(w_index, crosswpt_index);

		// see if this waypoint is within the range for this cross waypoint (ie. is connected to this cross waypoint)
		if (distance < waypoints[crosswpt_index].range)
		{
			// check for cases when there are paths that both start and end at this cross waypoint
			int farthest_wpt_to_ignore = wptmanager.AreBothPathEndsConnectedToOneCrossWaypoint(w_index);

			ALERT(at_console, "processing waypoint no. %d (its distance is: %.1f)\n", w_index + 1, distance);

			// is there a path that both starts and ends at this cross waypoint?
			if (farthest_wpt_to_ignore != NO_VAL)
			{
				// remember the distance and index of the farthest path end waypoint that
				// we need to remove from this cross waypoint range
				if (distance > distance_to_ignored_wpt)
				{
					distance_to_ignored_wpt = distance;
					ignored_wpt = farthest_wpt_to_ignore;

					if (ignored_wpt == w_index)
					{
						ALERT(at_console, "setting waypoint no. %d as the one to be excluded (its distance is: %.1f)\n",
							w_index + 1, distance);
					}
				}
			}

			// and find the farthest waypoint from all path end waypoints around this cross waypoint
			// to figure out this cross waypoint max range based on its distance
			// with the exception of the waypoint that is meant to be removed from this cross waypoint range
			if (wptmanager.IsWaypointAtPathEnd(w_index) && (distance > max_range) && (w_index != ignored_wpt))
			{
				max_range = truncf(distance + 1.0f);

				ALERT(at_console, "new maxRange is: %.1f based on waypoint no. %d\n", max_range, w_index + 1);
			}
		}
	}

	// if there's at least one path end waypoint that is farther than
	// the path end that we tried to exclude
	// then we are unable to tweak the range so we have to reset the marker
	if ((max_range > distance_to_ignored_wpt) && (ignored_wpt != NO_VAL))
		ignored_wpt = NO_VAL;

	return max_range;
}


/*
* runs several tracelines in order to move the waypoint away from obstacles (walls, sandbags etc.)
* also it does reduce its range if reposition alone cannot handle the range intersecting the obstacle
*/
void waypoints_and_paths_repair_functions_t::SelfControlledWaypointReposition(float& the_range, Vector& new_origin, float move_d, float dec_r, bool dont_move, edict_t* pentIgnore)
{
	Vector start, end;
	TraceResult tr, otr;
	bool stop = false;

	// the X-coordinate
	do
	{
		// we are tracing the range so 'start' is origin - range and the 'end' is origin + range
		start = new_origin - Vector(the_range, 0, 0);
		end = new_origin + Vector(the_range, 0, 0);

		// we have to trace the range to both sides
		// we are going from the waypoint origin (the centre) towards both edges of the range
		// the origin must always be free so the trace will always be valid
		UTIL_TraceLine(new_origin, start, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &tr);
		UTIL_TraceLine(new_origin, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &otr);

		// is the range free?
		if ((tr.flFraction == 1.0f) && (otr.flFraction == 1.0f))
		{
			stop = true;
		}
		// did we hit anything then see if we can reposition the waypoint or if we can only reduce the range
		else
		{
			// we can ignore the hit only if the entity is not solid or it's a trigger
			if ((tr.pHit->v.solid <= SOLID_BBOX) || (otr.pHit->v.solid < SOLID_BBOX))
				stop = true;
			else
			{
				if ((tr.flFraction == 1.0f) && (otr.flFraction != 1.0f) && !dont_move)
				{
					// we can ignore the hit if it is another player entity
					if (util.IsEntityName(otr.pHit, "player"))											// NEW CODE 095
					{
						stop = true;
					}
					else
					{
						// move the waypoint origin towards start because there was everything okay
						new_origin = new_origin - Vector(move_d, 0, 0);

						// and try a new traceline using the new origins this time
						start = new_origin - Vector(the_range, 0, 0);
						end = new_origin + Vector(the_range, 0, 0);

						UTIL_TraceLine(new_origin, start, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &tr);
						UTIL_TraceLine(new_origin, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &otr);

						// if all was okay then we can stop checking the x coord
						if ((tr.flFraction == 1.0f) && (otr.flFraction == 1.0f))
						{
							stop = true;
						}
						// otherwise reduce the range and try again
						else
						{
							the_range -= dec_r;
						}
					}

					continue;
				}
				else if ((tr.flFraction != 1.0f) && (otr.flFraction == 1.0f) && !dont_move)
				{
					if (util.IsEntityName(tr.pHit, "player"))											// NEW CODE 095
					{
						stop = true;
					}
					else
					{
						// move the waypoint origin towards end because there was everything okay
						new_origin = new_origin + Vector(move_d, 0, 0);

						start = new_origin - Vector(the_range, 0, 0);
						end = new_origin + Vector(the_range, 0, 0);

						UTIL_TraceLine(new_origin, start, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &tr);
						UTIL_TraceLine(new_origin, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &otr);

						if ((tr.flFraction == 1.0f) && (otr.flFraction == 1.0f))
						{
							stop = true;
						}
						else
						{
							the_range -= dec_r;
						}
					}

					continue;
				}
				// both sides are blocked ... we can only reduce the range in this case
				else
				{
					the_range -= dec_r;
				}
			}
		}

		// stop it when the range is just too small
		if (the_range <= dec_r)
			stop = true;

	} while (!stop);

	// reset the stop state before starting the other coordinate do-while cycle
	stop = false;
	// the Y-coordinate
	do
	{
		start = new_origin - Vector(0, the_range, 0);
		end = new_origin + Vector(0, the_range, 0);

		UTIL_TraceLine(new_origin, start, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &tr);
		UTIL_TraceLine(new_origin, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &otr);

		// is the range free?
		if ((tr.flFraction == 1.0f) && (otr.flFraction == 1.0f))
			stop = true;
		// did we hit anything
		else
		{
			// we can ignore the hit only if the entity is not solid or it's a trigger
			if ((tr.pHit->v.solid <= SOLID_BBOX) || (otr.pHit->v.solid < SOLID_BBOX))
				stop = true;
			else
			{
				if ((tr.flFraction == 1.0f) && (otr.flFraction != 1.0f) && !dont_move)
				{
					if (util.IsEntityName(otr.pHit, "player"))											// NEW CODE 095
					{
						stop = true;
					}
					else
					{
						// move the waypoint origin towards start because there was everything okay
						new_origin = new_origin - Vector(0, move_d, 0);

						start = new_origin - Vector(0, the_range, 0);
						end = new_origin + Vector(0, the_range, 0);

						UTIL_TraceLine(new_origin, start, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &tr);
						UTIL_TraceLine(new_origin, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &otr);

						if (tr.flFraction == 1.0 && otr.flFraction == 1.0)
						{
							stop = true;
						}
						else
						{
							the_range -= dec_r;
						}
					}

					continue;
				}
				else if ((tr.flFraction != 1.0f) && (otr.flFraction == 1.0f) && !dont_move)
				{
					if (util.IsEntityName(tr.pHit, "player"))											// NEW CODE 095
					{
						stop = true;
					}
					else
					{
						// move the waypoint origin towards end because there was everything okay
						new_origin = new_origin + Vector(0, move_d, 0);

						start = new_origin - Vector(0, the_range, 0);
						end = new_origin + Vector(0, the_range, 0);

						UTIL_TraceLine(new_origin, start, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &tr);
						UTIL_TraceLine(new_origin, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &otr);

						if ((tr.flFraction == 1.0f) && (otr.flFraction == 1.0f))
						{
							stop = true;
						}
						else
						{
							the_range -= dec_r;
						}
					}

					continue;
				}
				else
				{
					the_range -= dec_r;
				}
			}
		}

		// stop it when the range is just too small
		if (the_range <= dec_r)
			stop = true;

	} while (!stop);

	stop = false;
	// the diamond part 1 (bottom right and top left lines)
	do
	{
		start = new_origin + Vector(the_range, 0, 0);
		end = new_origin + Vector(0, the_range, 0);
		UTIL_TraceLine(start, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &tr);

		start = new_origin - Vector(the_range, 0, 0);
		end = new_origin - Vector(0, the_range, 0);
		UTIL_TraceLine(start, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &otr);

		// is the range free?
		if ((tr.flFraction == 1.0f) && (otr.flFraction == 1.0f))
			stop = true;
		// did we hit anything
		else
		{
			// we can ignore the hit only if the entity is not solid or it's a trigger
			if ((tr.pHit->v.solid <= SOLID_BBOX) || (otr.pHit->v.solid < SOLID_BBOX))
				stop = true;
			else
			{
				if ((tr.flFraction == 1.0f) && (otr.flFraction != 1.0f) && !dont_move)
				{
					if (util.IsEntityName(otr.pHit, "player"))											// NEW CODE 095
					{
						stop = true;
					}
					else
					{
						// move the waypoint origin towards start because there was everything okay
						new_origin = new_origin + Vector(move_d, move_d, 0);

						start = new_origin + Vector(the_range, 0, 0);
						end = new_origin + Vector(0, the_range, 0);
						UTIL_TraceLine(start, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &tr);

						start = new_origin - Vector(the_range, 0, 0);
						end = new_origin - Vector(0, the_range, 0);
						UTIL_TraceLine(start, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &otr);

						if ((tr.flFraction == 1.0f) && (otr.flFraction == 1.0f))
						{
							stop = true;
						}
						else
						{
							the_range -= dec_r;
						}
					}

					continue;
				}
				else if ((tr.flFraction != 1.0f) && (otr.flFraction == 1.0f) && !dont_move)
				{
					if (util.IsEntityName(tr.pHit, "player"))											// NEW CODE 095
					{
						stop = true;
					}
					else
					{
						// move the waypoint origin towards end because there was everything okay
						new_origin = new_origin - Vector(move_d, move_d, 0);

						start = new_origin + Vector(the_range, 0, 0);
						end = new_origin + Vector(0, the_range, 0);
						UTIL_TraceLine(start, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &tr);

						start = new_origin - Vector(the_range, 0, 0);
						end = new_origin - Vector(0, the_range, 0);
						UTIL_TraceLine(start, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &otr);

						if ((tr.flFraction == 1.0f) && (otr.flFraction == 1.0f))
						{
							stop = true;
						}
						else
						{
							the_range -= dec_r;
						}
					}

					continue;
				}
				else
				{
					the_range -= dec_r;
				}
			}
		}

		// stop it when the range is just too small
		if (the_range <= dec_r)
			stop = true;

	} while (!stop);


#ifdef _DEBUG
	// @@@@@@@@@@@@@@@@@@@@@		SHOW ME THE LINES
	if (((pentIgnore->v.origin - new_origin).Length2D() < 200.0f) && util.IsInViewCone(&new_origin, pentIgnore))
	{
		start = new_origin + Vector(the_range, 0, 0);
		end = new_origin + Vector(0, the_range, 0);
		DevDrawBeam(pentIgnore, start, end, 255, 255, 0, 100);

		start = new_origin - Vector(the_range, 0, 0);
		end = new_origin - Vector(0, the_range, 0);
		DevDrawBeam(pentIgnore, start, end, 125, 125, 0, 100);
	}
#endif


	stop = false;
	// the diamond part 2 (top right and bottom left lines ... if looking from x coordinate)
	do
	{
		start = new_origin + Vector(-the_range, 0, 0);
		end = new_origin + Vector(0, the_range, 0);
		UTIL_TraceLine(start, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &tr);

		start = new_origin - Vector(-the_range, 0, 0);
		end = new_origin - Vector(0, the_range, 0);
		UTIL_TraceLine(start, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &otr);

		// is the range free?
		if ((tr.flFraction == 1.0f) && (otr.flFraction == 1.0f))
			stop = true;
		// did we hit anything
		else
		{
			// we can ignore the hit only if the entity is not solid or it's a trigger
			if ((tr.pHit->v.solid <= SOLID_BBOX) || (otr.pHit->v.solid < SOLID_BBOX))
				stop = true;
			else
			{
				if ((tr.flFraction == 1.0f) && (otr.flFraction != 1.0f) && !dont_move)
				{
					if (util.IsEntityName(otr.pHit, "player"))											// NEW CODE 095
					{
						stop = true;
					}
					else
					{
						// move the waypoint origin towards start because there was everything okay
						new_origin = new_origin + Vector(-move_d, move_d, 0);

						start = new_origin + Vector(-the_range, 0, 0);
						end = new_origin + Vector(0, the_range, 0);
						UTIL_TraceLine(start, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &tr);

						start = new_origin - Vector(-the_range, 0, 0);
						end = new_origin - Vector(0, the_range, 0);
						UTIL_TraceLine(start, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &otr);

						if ((tr.flFraction == 1.0f) && (otr.flFraction == 1.0f))
						{
							stop = true;
						}
						else
						{
							the_range -= dec_r;
						}
					}

					continue;
				}
				else if ((tr.flFraction != 1.0f) && (otr.flFraction == 1.0f) && !dont_move)
				{
					if (util.IsEntityName(tr.pHit, "player"))											// NEW CODE 095
					{
						stop = true;
					}
					else
					{
						// move the waypoint origin towards end because there was everything okay
						new_origin = new_origin - Vector(-move_d, move_d, 0);

						start = new_origin + Vector(-the_range, 0, 0);
						end = new_origin + Vector(0, the_range, 0);
						UTIL_TraceLine(start, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &tr);

						start = new_origin - Vector(-the_range, 0, 0);
						end = new_origin - Vector(0, the_range, 0);
						UTIL_TraceLine(start, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &otr);

						if ((tr.flFraction == 1.0f) && (otr.flFraction == 1.0f))
						{
							stop = true;
						}
						else
						{
							the_range -= dec_r;
						}
					}

					continue;
				}
				else
				{
					the_range -= dec_r;
				}
			}
		}

		// stop it when the range is just too small
		if (the_range <= dec_r)
			stop = true;

	} while (!stop);


#ifdef _DEBUG
	// @@@@@@@@@@@@@@@@@@@@@		SHOW ME THE LINES
	if (((pentIgnore->v.origin - new_origin).Length2D() < 200.0f) && util.IsInViewCone(&new_origin, pentIgnore))
	{
		start = new_origin + Vector(-the_range, 0, 0);
		end = new_origin + Vector(0, the_range, 0);
		DevDrawBeam(pentIgnore, start, end, 255, 255, 125, 100);

		start = new_origin - Vector(-the_range, 0, 0);
		end = new_origin - Vector(0, the_range, 0);
		DevDrawBeam(pentIgnore, start, end, 255, 125, 125, 100);
	}
#endif


	stop = false;
	// the diagonal part 1 (top left and bottom right quadrant ... if looking from x coordinate)
	do
	{
		start = new_origin - Vector(the_range, the_range, 0);
		end = new_origin + Vector(the_range, the_range, 0);

		UTIL_TraceLine(new_origin, start, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &tr);
		UTIL_TraceLine(new_origin, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &otr);

		// is the range free?
		if ((tr.flFraction == 1.0f) && (otr.flFraction == 1.0f))
			stop = true;
		// did we hit anything
		else
		{
			// we can ignore the hit only if the entity is not solid or it's a trigger
			if ((tr.pHit->v.solid <= SOLID_BBOX) || (otr.pHit->v.solid < SOLID_BBOX))
				stop = true;
			else
			{
				if ((tr.flFraction == 1.0f) && (otr.flFraction != 1.0f) && !dont_move)
				{
					if (util.IsEntityName(otr.pHit, "player"))											// NEW CODE 095
					{
						stop = true;
					}
					else
					{
						// move the waypoint origin towards start because there was everything okay
						new_origin = new_origin - Vector(move_d, move_d, 0);

						start = new_origin - Vector(the_range, the_range, 0);
						end = new_origin + Vector(the_range, the_range, 0);

						UTIL_TraceLine(new_origin, start, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &tr);
						UTIL_TraceLine(new_origin, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &otr);

						if ((tr.flFraction == 1.0f) && (otr.flFraction == 1.0f))
						{
							stop = true;
						}
						else
						{
							the_range -= dec_r;
						}
					}

					continue;
				}
				else if ((tr.flFraction != 1.0f) && (otr.flFraction == 1.0f) && !dont_move)
				{
					if (util.IsEntityName(tr.pHit, "player"))											// NEW CODE 095
					{
						stop = true;
					}
					else
					{
						// move the waypoint origin towards end because there was everything okay
						new_origin = new_origin + Vector(move_d, move_d, 0);

						start = new_origin - Vector(the_range, the_range, 0);
						end = new_origin + Vector(the_range, the_range, 0);

						UTIL_TraceLine(new_origin, start, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &tr);
						UTIL_TraceLine(new_origin, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &otr);

						if ((tr.flFraction == 1.0f) && (otr.flFraction == 1.0f))
						{
							stop = true;
						}
						else
						{
							the_range -= dec_r;
						}
					}

					continue;
				}
				else
				{
					the_range -= dec_r;
				}
			}
		}

		// stop it when the range is just too small
		if (the_range <= dec_r)
			stop = true;

	} while (!stop);


#ifdef _DEBUG
	// @@@@@@@@@@@@@@@@@@@@@		SHOW ME THE LINES
	if (((pentIgnore->v.origin - new_origin).Length2D() < 200.0f) && util.IsInViewCone(&new_origin, pentIgnore))
	{
		start = new_origin - Vector(the_range, the_range, 0);
		end = new_origin + Vector(the_range, the_range, 0);
		DevDrawBeam(pentIgnore, start, end, 255, 125, 125, 100);
	}
#endif


	stop = false;
	// the diagonal part 2 (top right and bottom left quadrant ... if looking from x coordinate)
	do
	{
		start = new_origin + Vector(the_range, -the_range, 0);
		end = new_origin - Vector(the_range, -the_range, 0);

		UTIL_TraceLine(new_origin, start, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &tr);
		UTIL_TraceLine(new_origin, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &otr);

		// is the range free?
		if ((tr.flFraction == 1.0f) && (otr.flFraction == 1.0f))
			stop = true;
		// did we hit anything
		else
		{
			// we can ignore the hit only if the entity is not solid or it's a trigger
			if ((tr.pHit->v.solid <= SOLID_BBOX) || (otr.pHit->v.solid < SOLID_BBOX))
				stop = true;
			else
			{
				if ((tr.flFraction == 1.0f) && (otr.flFraction != 1.0f) && !dont_move)
				{
					if (util.IsEntityName(otr.pHit, "player"))											// NEW CODE 095
					{
						stop = true;
					}
					else
					{
						// move the waypoint origin towards start because there was everything okay
						new_origin = new_origin + Vector(move_d, -move_d, 0);

						start = new_origin + Vector(the_range, -the_range, 0);
						end = new_origin - Vector(the_range, -the_range, 0);

						UTIL_TraceLine(new_origin, start, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &tr);
						UTIL_TraceLine(new_origin, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &otr);

						if ((tr.flFraction == 1.0f) && (otr.flFraction == 1.0f))
						{
							stop = true;
						}
						else
						{
							the_range -= dec_r;
						}
					}

					continue;
				}
				else if ((tr.flFraction != 1.0f) && (otr.flFraction == 1.0f) && !dont_move)
				{
					if (strcmp(STRING(tr.pHit->v.classname), "player") == 0)											// NEW CODE 095
					{
						stop = true;
					}
					else
					{
						// move the waypoint origin towards end because there was everything okay
						new_origin = new_origin - Vector(move_d, -move_d, 0);

						start = new_origin + Vector(the_range, -the_range, 0);
						end = new_origin - Vector(the_range, -the_range, 0);

						UTIL_TraceLine(new_origin, start, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &tr);
						UTIL_TraceLine(new_origin, end, dont_ignore_monsters, dont_ignore_glass, pentIgnore, &otr);

						if ((tr.flFraction == 1.0f) && (otr.flFraction == 1.0f))
						{
							stop = true;
						}
						else
						{
							the_range -= dec_r;
						}
					}

					continue;
				}
				else
				{
					the_range -= dec_r;
				}
			}
		}

		// stop it when the range is just too small
		if (the_range <= dec_r)
			stop = true;

	} while (!stop);


#ifdef _DEBUG
	// @@@@@@@@@@@@@@@@@@@@@		SHOW ME THE LINES
	if (((pentIgnore->v.origin - new_origin).Length2D() < 200.0f) && util.IsInViewCone(&new_origin, pentIgnore))
	{
		start = new_origin + Vector(the_range, -the_range, 0);
		end = new_origin - Vector(the_range, -the_range, 0);
		DevDrawBeam(pentIgnore, start, end, 195, 125, 125, 100);
	}
#endif


	return;
}


/*
* checks the waypoint whether it has wait time and a sniper tag/flag
* also whether there is an aim waypoint nearby
* returns -1 if the waypoint or path isn't valid
* returns 0 if there's nothing to be fixed
* returns 1 if there was something fixed
*/
int waypoints_and_paths_repair_functions_t::FixSniperSpot(int path_index, int wpt_index, bool repair_it, bool log_in_file)
{
	// first check the validity
	if ((wpt_index == NO_VAL) || (path_index == NO_VAL))
		return -1;

	bool missing_aim_waypoint = false;		// when there is no 'aim' waypoint around
	bool repair_done = false;				// when there was something fixed (gets set even in the checking mode)
	bool report_it = false;					// for the checking mode in order to use just one message for multiple spots
	char msg[256];							// for the checking mode, the error/warning message

	// reset all the counters
	wptoutputer.ResetCounters();

	// see if there already is a wait time
	if ((wptmanager.GetWaypointWaitTime(wpt_index, teamONE.GetTeamId()) > 0.0f) || (wptmanager.GetWaypointWaitTime(wpt_index, teamTWO.GetTeamId()) > 0.0f))
	{
		// if this waypoint misses sniper flag then set it
		if (wptmanager.IsWaypoint(wpt_index, WptT::sniper) == false)
		{
			// are we allowed to repair it?
			if (repair_it)
			{
				waypoints[wpt_index].flags |= W_FL_SNIPER;
			}
			// or are we just checking for issues?
			else
			{
				sprintf(msg, "WARNING: Waypoint no. %d is missing the 'sniper' flag/tag.\n", wpt_index + 1);
				wptoutputer.AddWarning();
				wptoutputer.ProcessIt(msg, log_in_file);
			}

			repair_done = true;
		}

		// check for missing aim waypoint
		if (wptmanager.FindAimingAround(wpt_index) == NO_VAL)
		{
			missing_aim_waypoint = true;
		}
	}
	// there is no wait time on this waypoint
	else
	{
		// so let's check if there is an aim waypoint nearby
		if (wptmanager.FindAimingAround(wpt_index) != NO_VAL)
		{
			// has this waypoint the sniper flag?
			if (wptmanager.IsWaypoint(wpt_index, WptT::sniper))
			{
				if (repair_it)
				{
					// okay both are present then this waypoint was meant to be a sniper spot
					// therefore we must add some wait time on it
					waypoints[wpt_index].red_time = 25.0f;
					waypoints[wpt_index].blue_time = 25.0f;
				}
				else
					report_it = true;

				repair_done = true;
			}
			else
			{
				if (repair_it)
				{
					// there's no sniper flag on this waypoint so let's add only a short wait time,
					// because we can't be sure this was really meant to be a sniper spot
					waypoints[wpt_index].red_time = 10.0f;
					waypoints[wpt_index].blue_time = 10.0f;
				}
				else
					report_it = true;

				repair_done = true;
			}
		}
		// there's no aim waypoint around ...
		else
		{
			// but if this waypoint has a sniper flag ...
			if (wptmanager.IsWaypoint(wpt_index, WptT::sniper))
			{
				if (repair_it)
				{
					// we will add short wait time and ...
					waypoints[wpt_index].red_time = 10.0f;
					waypoints[wpt_index].blue_time = 10.0f;
				}
				else
					report_it = true;

				repair_done = true;

				// we'll try to add also the aim waypoint
				missing_aim_waypoint = true;
			}
		}

		// use longer wait time if we are on a sniper or machinegunner paths
		if (repair_it && (wptmanager.IsPath(path_index, PathT::sniper_class) || wptmanager.IsPath(path_index, PathT::mgunner_class)))
		{
			waypoints[wpt_index].red_time = 60.0f;
			waypoints[wpt_index].blue_time = 60.0f;
		}

		// are we in the checking mode and do we need to report an issue?
		if (report_it)
		{
			sprintf(msg, "BUG: Waypoint no. %d is missing the 'wait time'.\n", wpt_index + 1);
			wptoutputer.AddError();
			wptoutputer.ProcessIt(msg, log_in_file);
		}
	}

	if (missing_aim_waypoint)
	{
		if (repair_it)
		{
			// get the next waypoint from this path
			int next_waypoint = wptmanager.GetPathNextWaypoint(wpt_index, path_index);

			// no next waypoint on this path? (e.g. the waypoint must be at the end of this path)
			if (next_waypoint == NO_VAL)
			{
				// then get previous waypoint from this path
				next_waypoint = wptmanager.GetPathPreviousWaypoint(wpt_index, path_index);
			}

			if (next_waypoint != NO_VAL)
			{
				// use the next waypoint to get a direction where the bot should aim to
				Vector aim_vec = (waypoints[wpt_index].origin - waypoints[next_waypoint].origin).Normalize();

				// now use the normalized aiming vector to test free space up to 110 units from this path end waypoint position
				// 100 units is the max range at which the aiming waypoints are searched so we'll check it a little further
				Vector aim_origin = waypoints[wpt_index].origin + aim_vec * (MAX_AIM_WPT_DIST + 10.0f);

				// we'll use eyes origins because sandbag or window would invalidate the trace line using standard origins
				// we don't want the bot facing a wall while having open backs
				Vector head_wpt = waypoints[wpt_index].origin + Vector(0, 0, 24);
				Vector head_aim = aim_origin + Vector(0, 0, 24);

				TraceResult tr;
				UTIL_TraceLine(head_wpt, head_aim, ignore_monsters, NULL, &tr);

				// if there isn't free space for the new aiming waypoint ...
				if (tr.flFraction != 1.0f)
				{
					// then make the aim vector opposite ... ie. the bot would face the next path waypoint
					// no traceline is needed now, because there is the path so there must be free space
					aim_vec = (waypoints[next_waypoint].origin - waypoints[wpt_index].origin).Normalize();
					aim_origin = waypoints[wpt_index].origin + aim_vec * 75;
				}
				// there is no obstacle blocking the new aiming waypoint so ...
				else
				{
					// use the normalized aiming vector to place new aiming waypoint in range of 75 units
					// from the end waypoint position
					aim_origin = waypoints[wpt_index].origin + aim_vec * 75;
				}

				// finally add the aiming waypoint and set 'repair done'
				if (wpteditor.AddType(aim_origin, WptT::aim) == WptT::aim)
					repair_done = true;
			}
		}
		else
		{
			sprintf(msg, "WARNING: There is no 'aim' waypoint for waypoint no. %d.\n", wpt_index + 1);
			wptoutputer.AddWarning();
			wptoutputer.ProcessIt(msg, log_in_file);
		}
	}

	if (repair_done)
		return 1;

	return 0;
}


/*
* merges two paths into one where path2_index path will be appended to the end of path1_index path
* reverse_order == false means path1 (start->end) + path2 (start->end)
* reverse_order == true means path1(start->end) + path2 (end->start)
* path2_index flags are discarded and the path basically gets deleted after the merge
* returns path1_index if everything is okay otherwise it returns -1 as error
*/
int waypoints_and_paths_repair_functions_t::MergePaths(int path1_index, int path2_index, bool path2_in_reverse_order)
{
	if ((path1_index == NO_VAL) || (path2_index == NO_VAL))
		return -1;

	int path2_waypoint = NO_VAL;

	// first we must set this otherwise the Continue Current Path method would return error 
	internals.SetPathToContinue(path1_index);

	// path2_index path is added to the end of path1_index path in order from its start to its end
	if (path2_in_reverse_order == false)
	{
		// get the 1st waypoint from the other path
		path2_waypoint = wptmanager.GetPathStart(path2_index);

		// now keep removing the start waypoint from the path2_index path and add it to the end of path1_index path
		// untill there's nothing left in path2_index path
		while (path2_waypoint != NO_VAL)
		{
			// remove the waypoint from the path2_index path
			ExcludeFromPath(path2_waypoint, path2_index);

			// add it to path1_index path
			ContinueCurrPath(path2_waypoint);

			// get the new start waypoint ...
			path2_waypoint = wptmanager.GetPathStart(path2_index);
		}
	}
	// path2_index path is added to the end of path1_index path in reversed order (ie. from its end to its start)
	else
	{
		// get the last waypoint from the other path
		path2_waypoint = wptmanager.GetPathEnd(path2_index);

		while (path2_waypoint != NO_VAL)
		{
			ExcludeFromPath(path2_waypoint, path2_index);
			ContinueCurrPath(path2_waypoint);
			path2_waypoint = wptmanager.GetPathEnd(path2_index);
		}
	}

	// we're done here so we must reset it
	internals.ResetPathToContinue();

	// we should also check our path for errors
	if (ValidatePath(path1_index) != -1)
	{
		// and let it update its status seeing we just added some new waypoints in it
		UpdatePathStatus(path1_index);

		return path1_index;
	}

	return -1;
}


/*
* merges two paths into one where path2_index path will be inserted to the beginning of path1_index path
* reverse_order == false means path2 (start->end) + path1 (start->end)
* reverse_order == true means path2 (end->start) + path1 (start->end)
* path2_index flags are discarded and the path basically gets deleted after the merge
* returns path1_index if everything is okay otherwise it returns -1 as error
*/
int waypoints_and_paths_repair_functions_t::MergePathsInverted(int path1_index, int path2_index, bool path2_in_reverse_order)
{
	if ((path1_index == NO_VAL) || (path2_index == NO_VAL))
		return -1;

	int path2_waypoint = NO_VAL;
	// the method for inserting waypoint into path works with string arguments
	// we must convert them first
	char path_index1_as_char[6];
	char path2_waypoint_as_char[6];

	sprintf(path_index1_as_char, "%d", path1_index + 1);

	// the order of the final path will be (path2 start -> path2 end -> path1 start -> path1 end)
	if (path2_in_reverse_order == false)
	{
		// to keep the desired order of paths we have to go from the end of the 2nd path
		// because we are inserting them one after another to the start of path1_index path
		path2_waypoint = wptmanager.GetPathEnd(path2_index);

		while (path2_waypoint != NO_VAL)
		{
			ExcludeFromPath(path2_waypoint, path2_index);

			sprintf(path2_waypoint_as_char, "%d", path2_waypoint + 1);
			patheditor.InsertWaypoint(path2_waypoint_as_char, path_index1_as_char, "start", NULL);

			path2_waypoint = wptmanager.GetPathEnd(path2_index);
		}
	}
	// the order of the final path will be (path2 end -> path2 start -> path1 start -> path1 end)
	else
	{
		path2_waypoint = wptmanager.GetPathStart(path2_index);

		while (path2_waypoint != NO_VAL)
		{
			ExcludeFromPath(path2_waypoint, path2_index);

			sprintf(path2_waypoint_as_char, "%d", path2_waypoint + 1);
			patheditor.InsertWaypoint(path2_waypoint_as_char, path_index1_as_char, "start", NULL);

			path2_waypoint = wptmanager.GetPathStart(path2_index);
		}
	}

	if (ValidatePath(path1_index) != -1)
	{
		UpdatePathStatus(path1_index);

		return path1_index;
	}

	return -1;
}


/*
* cleans the path from unwanted or invalid waypoints
* that means aim and cross waypoint types as well as any waypoint that has been added twice
* returns -1 if invalid path index is passed or if there is error that cannot be fixed
* returns 0 if the path was okay or if it doesn't even exist
* returns 1 if there was error that has been fixed
* returns 2 if it needs to be called again after successful path repair
*/
int waypoints_and_paths_repair_functions_t::PurifyPath(int path_index)
{
	W_PATH* p;
	int safety_stop = 0;
	static bool there_was_error = false;			// needed to remember we did some repairs in previous call
	bool error_in_path = false;						// used to track unfixable problem

	// stop it if the index isn't valid
	if (path_index == NO_VAL)
		return 0;

	p = w_paths[path_index];

	while (p)
	{
		safety_stop++;
		if (safety_stop > LINKEDLIST_LOOPS_THRESHOLD)
			LinkedListError("Purify Path", path_index);

		// cross or aim waypoint cannot be part of any path (this shouldn't happen, because it should be handled in other functions,
		// but if the waypointer manually changed waypoint type to one of these then they may be present on a path)
		if (wptmanager.IsWaypoint(p->wpt_index, WptT::aim, WptT::cross))
		{
			// we must do this first so that we know there is some error in this path
			there_was_error = error_in_path = true;

			if (ExcludeFromPath(p, path_index))
				return 2;
		}

		// check if this waypoint has been added more than once to this path so ...
		// start on the next path waypoint ...
		W_PATH* rp = p->next;
		int stop = 0;

		while (rp)
		{
			// and go through the rest of the path looking for a match
			if (rp->wpt_index == p->wpt_index)
			{
				there_was_error = error_in_path = true;

				// if we found any other addition of this waypoint and successfully removed it then call this function again
				// in order to catch all problems, because this waypoint may have been added several times to this path, not just twice				
				if (ExcludeFromPath(rp, path_index))
					return 2;
			}

			rp = rp->next;

			stop++;
			if (stop > LINKEDLIST_LOOPS_THRESHOLD)
				LinkedListError("Is Path Okay (nested search)", path_index);
		}

		// go to/check the next node
		p = p->next;
	}

	// there is an error in this path that cannot be fixed so we have to return 'bug'
	if (error_in_path)
	{
		// reset the static error tracker before leaving
		there_was_error = false;

		return -1;
	}

	// there was some error that had been fixed so we have to return 'warning'
	if (there_was_error)
	{
		there_was_error = false;

		return 1;
	}

	return 0;
}


/*
* will scan the surrounding of the pushpoint/flag waypoints for the pushpoint entity
* if we find it then we'll set appropriate goal path type for this path
*/
void waypoints_and_paths_repair_functions_t::UpdatePathStatusPushpoint(int wpt_index, int path_index)
{
	if ((wptmanager.IsWaypoint(wpt_index, WptT::pushpoint) == false) || (path_index == NO_VAL))
		return;

	// get an origin of the flag waypoint
	Vector wpt_origin = waypoints[wpt_index].origin;

	// invalid waypoint
	if (wpt_origin == Vector(0, 0, 0))
		return;

	edict_t* pent = NULL;
	bool is_caparea = false;
	bool is_conpoint = false;
	int caparea_array_index = CAPTUREPOINTS_ERROR_VAL;
	int conpoint_array_index = CAPTUREPOINTS_ERROR_VAL;
	int team = teamNULL;

	// search for any capture point around this waypoint
	while ((pent = util.FindEntityInSphere(pent, wpt_origin, WPT_RANGE)) != NULL)
	{
		if (util.IsEntityName(pent, "dod_capture_area"))
		{
			is_caparea = true;

			caparea_array_index = dodCaptureArea->FindPointInArray(pent);
			
			// capture area enetity doesn't have/expose its owner so we'll get the team from the array of capture areas
			team = dodCaptureArea->GetOwnedByTeam(caparea_array_index);

			break;
		}
		else if (util.IsEntityName(pent, "dod_control_point"))
		{
			is_conpoint = true;

			conpoint_array_index = ControlPoints->FindPointInArray(pent);
			
			// we have to use the array here too, because trying to get the team through the body variable on this entity didn't return correct value on some maps,
			// ie. the value in body was always zero no matter who owned the flag
			team = ControlPoints->GetOwnedByTeam(conpoint_array_index);

			break;

			/*/
			// see which team owns this control point...
			// (body == 0 means axis flag, body == 1 is american, body == 2 is british and body == 3 means neither team owns it)
			team = pent->v.body;

			// and convert it to standard team values
			if ((team == 1) || (team == 2))
			{
				team = teamONE.GetTeamId();
			}
			else if (team == 0)
			{
				team = teamTWO.GetTeamId();
			}
			else
				team = 0;	// neither team

			break;
			/**/
		}
	}

	// did we find valid control point entity to get the data from?
	if (pent)
	{
		// allies own this capture point so...
		if (team == teamONE.GetTeamId())
		{
			// set the goal for axis team if they are allowed to capture it
			if ((is_caparea && dodCaptureArea->GetTeamTwoAllowedToCapture(caparea_array_index)) || (is_conpoint && ControlPoints->GetTeamTwoAllowedToCapture(conpoint_array_index)))
				patheditor.SetAutoTag(PathT::goal_team_two_tag, path_index);

			// and remove allied team goal tag from this path
			patheditor.ResetAutoTag(PathT::goal_team_one_tag, path_index);
		}
		else if (team == teamTWO.GetTeamId())
		{
			if ((is_caparea && dodCaptureArea->GetTeamOneAllowedToCapture(caparea_array_index)) || (is_conpoint && ControlPoints->GetTeamOneAllowedToCapture(conpoint_array_index)))
				patheditor.SetAutoTag(PathT::goal_team_one_tag, path_index);

			patheditor.ResetAutoTag(PathT::goal_team_two_tag, path_index);
		}
		// neither team owns this capture point so set the goal for both teams
		else
		{
			if ((is_caparea && dodCaptureArea->GetTeamOneAllowedToCapture(caparea_array_index)) || (is_conpoint && ControlPoints->GetTeamOneAllowedToCapture(conpoint_array_index)))
				patheditor.SetAutoTag(PathT::goal_team_one_tag, path_index);
			if ((is_caparea && dodCaptureArea->GetTeamTwoAllowedToCapture(caparea_array_index)) || (is_conpoint && ControlPoints->GetTeamTwoAllowedToCapture(conpoint_array_index)))
				patheditor.SetAutoTag(PathT::goal_team_two_tag, path_index);
		}
	}
}


/*
* will check neighbouring waypoints on the path for the other roadblock waypoint and then scan the area between those waypoints first for a direct block on the way
* and then whether there is a solid ground between both waypoints so that the bot can safely move from one to the other
* status lock switch prevents assigning wrong data when there are more than one roadblock waypoints pair on the path (without it the first pair would have set path as blocked
* and the second pair would have then set path as free so the path would have been marked as available despite the fact that there's not a free way between
* the first pair of roadblock waypoints)
*/
void waypoints_and_paths_repair_functions_t::UpdatePathStatusRoadblock(int wpt_index, int path_index, bool& status_lock)
{
	TraceResult tr;
	static int the_second_roadblock = NO_VAL;
	bool path_fully_passable = false;
	bool check_solid_ground = true;
	static bool already_blocked = false;	// to remember the path status in case there are more roadblock pairs on this path

	if ((wptmanager.IsWaypoint(wpt_index, WptT::roadblock) == false) || (path_index == NO_VAL))
		return;

	// don't scan the same pair of roadblock waypoints again - UpdatePathStatus goes through the path waypoint by waypoint and because roadblock waypoints must be direct neighbours and seeing this function
	// remembers the index of the second roadblock waypoint then if the indexes match, we know we have already checked this waypoint
	if (the_second_roadblock == wpt_index)
		return;

	// reset static memory if we are checking this path for the first time in current UpdatePathStatus cycle
	if (status_lock == false)
		already_blocked = false;

	// get next waypoint on this path
	the_second_roadblock = wptmanager.GetPathNextWaypoint(wpt_index, path_index);

	// is it NOT the second roadblock waypoint?
	if (wptmanager.IsWaypoint(the_second_roadblock, WptT::roadblock) == false)
	{
		// then check previous waypoint on this path
		the_second_roadblock = wptmanager.GetPathPreviousWaypoint(wpt_index, path_index);

		// even this one is NOT the second roadblock we are looking for?
		if (wptmanager.IsWaypoint(the_second_roadblock, WptT::roadblock) == false)
		{
			// if this path has no other pair of roadblocks then this roadblock waypoint is the only one on the whole path and
			// we have to mark this path usable (ie. fully available on cross waypoint), this can happen if waypoints were saved while still editing them (eg autosave feature)
			if (already_blocked == false)
				patheditor.ResetAutoTag(PathT::roadblocked_tag, path_index);

			// there's nothing else to be done here
			return;
		}
	}

	// first try direct line between these two waypoints so is the second roadblock waypoint reachable?
	if (util.IsPointReachable(waypoints[wpt_index].origin, waypoints[the_second_roadblock].origin, true))
		path_fully_passable = true;

	// next see whether we need to check also for solid ground between the two waypoints
	// there is an option to use priority 1 setting on the pair of roadblock waypoints to disable the need for checking for solid ground between this pair
	// (ie. if the obstacle blocking the pass is just a breakable wall then there is no point checking the ground for dangerous pits ... where is a risk of deathfall)
	if (((waypoints[wpt_index].red_priority == 1) || (waypoints[wpt_index].blue_priority == 1)) && ((waypoints[the_second_roadblock].red_priority == 1) || (waypoints[the_second_roadblock].blue_priority == 1)))
		check_solid_ground = false;

	// is there nothing blocking the second roadblock waypoint? and do we have to check also for a solid ground between the two roadblocks?
	// (ie. check cases like moving bridge or avalanche pit where the bot may die due to a deathfall)
	if (path_fully_passable && check_solid_ground)
	{
		const float max_dist_for_just_two_tracelines = 200.0f;	// tracelining just 2 spots between the two waypoints instead of 4 will save some CPU time
		float sector = 0.0f;
		bool sector1_okay = true;
		bool sector2_okay = true;
		bool sector3_okay = true;
		bool sector4_okay = true;
		Vector start, end;
		float depth_check = WPT_RANGE * 2.0f;		// how far below the horizontal traceline will we check for solid ground ie. how deep pit will be considered as safe

		// make the vector to the second roadblock
		Vector vec = waypoints[the_second_roadblock].origin - waypoints[wpt_index].origin;
		
		Vector dir = vec.Normalize();

		// get the distance between the two waypoints
		float distance = vec.Length();
		
		// are the two waypoints are quite close to each other?
		if (distance <= max_dist_for_just_two_tracelines)
			// then make only 2 reference points on the way between both waypoints to check for solid ground beneath
			sector = distance / 3.0f;
		else
			// otherwise make all 4 reference points
			sector = distance / 5.0f;

		// the position of the first reference point
		start = waypoints[wpt_index].origin + dir * sector;
		// checking below it
		end = start - Vector(0, 0, depth_check);
		util.TraceLineIgnoringPlayers(start, end, &tr);

		// if we reached the end point then there's a free space so not a solid ground
		if (tr.flFraction >= 1.0f)
			sector1_okay = false;	// so this point isn't safe

		start = waypoints[wpt_index].origin + dir * (sector * 2.0f);
		end = start - Vector(0, 0, depth_check);
		util.TraceLineIgnoringPlayers(start, end, &tr);

		if (tr.flFraction >= 1.0f)
			sector2_okay = false;

		// we will send additional tracelines only when the waypoints are quite far from each other
		if (distance > max_dist_for_just_two_tracelines)
		{
			start = waypoints[wpt_index].origin + dir * (sector * 3.0f);
			end = start - Vector(0, 0, depth_check);
			util.TraceLineIgnoringPlayers(start, end, &tr);

			if (tr.flFraction >= 1.0f)
				sector3_okay = false;

			start = waypoints[wpt_index].origin + dir * (sector * 4.0f);
			end = start - Vector(0, 0, depth_check);
			util.TraceLineIgnoringPlayers(start, end, &tr);

			if (tr.flFraction >= 1.0f)
				sector4_okay = false;
		}

		// if all sectors are safe (ie. there's a solid ground below them) then bot can safely reach the second roadblock waypoint
		if (sector1_okay && sector2_okay && sector3_okay && sector4_okay)
			path_fully_passable = true;
		// otherwise there isn't safe way between the two waypoints
		else
			path_fully_passable = false;
	}

	if ((path_fully_passable) && (already_blocked == false))
		patheditor.ResetAutoTag(PathT::roadblocked_tag, path_index);	// mark this path usable (ie. fully available on cross waypoint)
	else
	{
		patheditor.SetAutoTag(PathT::roadblocked_tag, path_index);		// mark this path unusable (ie. unavailable on cross waypoint)

		// also remember that this path is blocked in case there are more roadblock pairs on it
		already_blocked = true;
	}

	// we just checked one roadblock pair on this path in current UpdatePathStatus cycle so we have to lock the static memory value
	if (status_lock == false)
		status_lock = true;
}


/*
* returns the number of flags set on this waypoint
*/
int waypoints_and_paths_managing_functions_t::CountWaypointFlags(int wpt_index)
{
	int the_count = 0;

	// handle errors
	if (wpt_index == NO_VAL)
		return NO_VAL;

	// increase the count with each valid flag
	if (waypoints[wpt_index].flags & W_FL_STD)
		the_count++;
	if (waypoints[wpt_index].flags & W_FL_CROUCH)
		the_count++;
	if (waypoints[wpt_index].flags & W_FL_PRONE)
		the_count++;
	if (waypoints[wpt_index].flags & W_FL_JUMP)
		the_count++;
	if (waypoints[wpt_index].flags & W_FL_DUCKJUMP)
		the_count++;
	if (waypoints[wpt_index].flags & W_FL_SPRINT)
		the_count++;
	if (waypoints[wpt_index].flags & W_FL_AMMOBOX)
		the_count++;
	if (waypoints[wpt_index].flags & W_FL_BANDAGE)
		the_count++;
	if (waypoints[wpt_index].flags & W_FL_DOOR)
		the_count++;
	if (waypoints[wpt_index].flags & W_FL_DOORUSE)
		the_count++;
	if (waypoints[wpt_index].flags & W_FL_LADDER)
		the_count++;
	if (waypoints[wpt_index].flags & W_FL_USE)
		the_count++;
	if (waypoints[wpt_index].flags & W_FL_CHUTE)
		the_count++;
	if (waypoints[wpt_index].flags & W_FL_MINE)
		the_count++;
	if (waypoints[wpt_index].flags & W_FL_PUSHPOINT)
		the_count++;
	if (waypoints[wpt_index].flags & W_FL_TRIGGER)
		the_count++;
	if (waypoints[wpt_index].flags & W_FL_ROADBLOCK)
		the_count++;
	if (waypoints[wpt_index].flags & W_FL_GOBACK)
		the_count++;
	if (waypoints[wpt_index].flags & W_FL_DELETED)
		the_count++;
	// keep these special flags here so we can find possible problems
	if (waypoints[wpt_index].flags & W_FL_AIMING)
		the_count++;
	if (waypoints[wpt_index].flags & W_FL_CROSS)
		the_count++;

	return the_count;
}


/*
* tries to find any aiming waypoint (for sniper aiming) around given waypoint
* returns found waypoint index or "no value" (ie. -1) if none is found
*/
int waypoints_and_paths_managing_functions_t::FindAimingAround(int source_waypoint_index)
{
	// check validity first
	if ((source_waypoint_index == NO_VAL) || IsWaypoint(source_waypoint_index, WptT::deleted))
		return NO_VAL;

	// go through all waypoints
	for (int index = 0; index < num_waypoints; index++)
	{
		// skip any NON aiming waypoint
		if (IsWaypoint(index, WptT::aim) == false)
			continue;

		float distance = GetDistanceBetweenWaypoints(source_waypoint_index, index);

		// if this aiming waypoint is nearby then return its index
		if (distance <= MAX_AIM_WPT_DIST)
		{
			return index;
		}
	}

	return NO_VAL;
}


/*
* tries to find given waypoint in given radius from given position/origin
*/
int waypoints_and_paths_managing_functions_t::FindWaypointOfTypeAround(const Vector& source_origin, WptT wpt_type, float distance)
{
	// go through all waypoints
	for (int index = 0; index < num_waypoints; index++)
	{
		// skip any waypoint that doesn't match given type
		if (IsWaypoint(index, wpt_type) == false)
			continue;

		float wpt_distance = (source_origin - waypoints[index].origin).Length();

		// if this waypoint is within given radius then return its index
		if (wpt_distance <= distance)
		{
			return index;
		}
	}

	return NO_VAL;
}


/*
* tries to find the cross waypoint that is connected to given waypoint index (i.e. the waypoint is inside that cross waypoint range)
* return -1 if none is found
*/
int waypoints_and_paths_managing_functions_t::FindConnectedCross(int source_waypoint_index)
{
	return FindConnectedCross(waypoints[source_waypoint_index].origin, false);
}


/*
* tries to find the cross waypoint that is connected to given position/origin (i.e. the origin is inside that cross waypoint range)
* return -1 if none is found
*/
int waypoints_and_paths_managing_functions_t::FindConnectedCross(const Vector& source_origin)
{
	return FindConnectedCross(source_origin, false);
}

/*
* tries to find the cross waypoint that is connected to given position/origin (i.e. the origin is inside that cross waypoint range)
* return -1 if none is found
* Overloaded to allow ignoring only certain entities (we need to be able to see through doors and breakables and players)
*/
int waypoints_and_paths_managing_functions_t::FindConnectedCross(const Vector& source_origin, bool see_through_doors)
{
	TraceResult tr;
	float distance, min_distance = MAX_WPT_DIST + 1.0f;
	int min_index = NO_VAL;

	// go through all waypoints
	for (int cross = 0; cross < num_waypoints; cross++)
	{
		// work only with cross waypoints
		if (IsWaypoint(cross, WptT::cross))
		{
			distance = (source_origin - waypoints[cross].origin).Length();

			// is the given origin in this cross waypoint range
			if ((distance < min_distance) && (distance <= waypoints[cross].range))
			{
				if (see_through_doors)
				{
					// make sure the traceline matches the direction of the traceline in 'check_cross' (i.e. starts at the cross waypoint)
					// otherwise it may lead to strange errors if there's a glitch on the map (usually invisible glitch),
					// such glitch can cause that a traceline from start to end point will pass, but opposite traceline (end -> start) will fail
					UTIL_TraceLine(waypoints[cross].origin, source_origin, dont_ignore_monsters, NULL, &tr);

					// is this cross waypoint reachable
					if (tr.flFraction >= 1.0f)
					{
						// remember this cross waypoint
						min_distance = distance;
						min_index = cross;
					}
					// we need to check for doors and breakables now, because the waypoint could be on the other side
					else
					{
						if (util.IsDoorEntity(tr.pHit) || util.IsEntityName(tr.pHit, "func_breakable") || util.IsEntityName(tr.pHit, "player"))
						{
							// do a second traceline and ignore the previously hit door or breakable entity
							UTIL_TraceLine(waypoints[cross].origin, source_origin, dont_ignore_monsters, tr.pHit, &tr);

							if (tr.flFraction >= 1.0f)
							{
								// remember this cross waypoint
								min_distance = distance;
								min_index = cross;
							}
						}
					}
				}
				else
				{
					UTIL_TraceLine(waypoints[cross].origin, source_origin, ignore_monsters, NULL, &tr);

					if (tr.flFraction >= 1.0f)
					{
						// remember this cross waypoint
						min_distance = distance;
						min_index = cross;
					}
				}
			}
		}
	}

	return min_index;
}


/*
* tries to find the nearest cross waypoint to given position/origin
* returns the index of the closest one even beyond its range (i.e. "disconnected cross" but still reachable)
* return -1 if none is found within max waypoint distance
*/
int waypoints_and_paths_managing_functions_t::FindNearestCross(const Vector& source_origin, bool see_through_doors)
{
	TraceResult tr;
	float distance, min_distance = MAX_WPT_DIST + 1.0f;
	int min_index = NO_VAL;

	// go through all waypoints
	for (int cross = 0; cross < num_waypoints; cross++)
	{
		// work only with cross waypoints
		if (IsWaypoint(cross, WptT::cross))
		{
			distance = (source_origin - waypoints[cross].origin).Length();

			if (distance < min_distance)
			{
				if (see_through_doors)
				{
					// make sure the traceline matches the direction of the traceline in 'check_cross' (i.e. starts at the cross waypoint)
					// otherwise it may lead to strange errors if there's a glitch on the map (usually invisible glitch),
					// such glitch can cause that a traceline from start to end point will pass, but opposite traceline (end -> start) will fail
					UTIL_TraceLine(waypoints[cross].origin, source_origin, dont_ignore_monsters, NULL, &tr);

					// is this cross waypoint reachable?
					if (tr.flFraction >= 1.0f)
					{
						// remember this cross waypoint
						min_distance = distance;
						min_index = cross;
					}
					// we need to check for doors and breakables now, because the waypoint could be on the other side
					else
					{
						if (util.IsDoorEntity(tr.pHit) || util.IsEntityName(tr.pHit, "func_breakable") || util.IsEntityName(tr.pHit, "player"))
						{
							// do a second traceline and ignore the previously hit door or breakable entity
							UTIL_TraceLine(waypoints[cross].origin, source_origin, dont_ignore_monsters, tr.pHit, &tr);

							if (tr.flFraction >= 1.0f)
							{
								min_distance = distance;
								min_index = cross;
							}
						}
					}
				}
				else
				{
					UTIL_TraceLine(waypoints[cross].origin, source_origin, ignore_monsters, NULL, &tr);

					if (tr.flFraction >= 1.0)
					{
						// remember this cross waypoint
						min_distance = distance;
						min_index = cross;
					}
				}
			}
		}
	}

	return min_index;
}


/*
* tries to find the nearest ordinary waypoint (ie. of any type except for aiming and cross) from given path end waypoint
* returns found waypoint index or -1 if none is found
*/
int waypoints_and_paths_managing_functions_t::FindNearestOrdinaryWaypoint(int end_waypoint, int path_index)
{
	// check validity first
	if ((end_waypoint == NO_VAL) || (path_index == NO_VAL))
		return NO_VAL;

	int found_waypoint = NO_VAL;
	float min_distance = MAX_WPT_DIST / 2.0f;

	// go through all waypoints
	for (int i = 0; i < num_waypoints; i++)
	{
		// skip these
		if (IsWaypoint(i, WptT::aim, WptT::cross, WptT::deleted))
			continue;

		// skip all waypoints on this path
		if (IsWaypointOnPath(i, path_index))
			continue;

		// see how far is this waypoint from our waypoint ...
		float distance = GetDistanceBetweenWaypoints(i, end_waypoint);

		// because we are looking only for close waypoints
		if (distance < min_distance)
		{
			int num_paths_on_this_waypoint = 0;

			// check if we can use this waypoint so go through all paths ... 
			for (int this_path = 0; this_path < num_w_paths; this_path++)
			{
				// to see if this waypoint is on any path if so then ...
				if (IsWaypointOnPath(i, this_path))
				{
					num_paths_on_this_waypoint++;

					// see if this path is accessible
					// without exact data about bot team and class we can only do general comparison ...
					// that means we can only access path that is equally or less restrictive than our current one
					// (eg. from red team path to another red team or to default both team path)
					// therefore we have to assume our current path is the more restrictive path
					if (CanSwitchToThisPathDueToTeamLimitingFactors(this_path, path_index) &&
						CanSwitchToThisPathDueToClassLimitingFactors(this_path, path_index))
					{
						// this waypoint is the end waypoint of this path ... we cannot access it then
						if (IsPath(this_path, PathT::one_way) && (i == GetPathEnd(this_path)))
							continue;
						else
						{
							TraceResult tr;
							UTIL_TraceLine(waypoints[i].origin, waypoints[end_waypoint].origin, ignore_monsters, NULL, &tr);

							// check if this waypoint is reachable
							if (tr.flFraction >= 1.0f)
							{
								min_distance = distance;
								found_waypoint = i;
							}
						}
					}
				}
			}

			// if there is no path on this waypoint then we may use it too ...
			if (num_paths_on_this_waypoint < 1)
			{
				// ... unless it's a solitary goback waypoint
				// (this probably happened when the waypointer tried to change the path end waypoint into a goback,
				// but used command to add a new waypoint instead of using change waypoint type command)
				if (IsWaypoint(i, WptT::goback) == false)
				{
					TraceResult tr;
					UTIL_TraceLine(waypoints[i].origin, waypoints[end_waypoint].origin, ignore_monsters, NULL, &tr);

					// if it is reachable of course
					if (tr.flFraction >= 1.0f)
					{
						min_distance = distance;
						found_waypoint = i;
					}
				}
			}
		}
	}

	return found_waypoint;
}


/*
* tries to find the nearest waypoint of any type to the player and returns its index
* returns -1 if none is found
*/
int waypoints_and_paths_managing_functions_t::FindNearestWaypointToPlayer(edict_t* pEntity, float range, int team)
{
	int i, min_index;
	float distance;
	float min_distance;
	TraceResult tr;

	if (num_waypoints < 1)
		return NO_VAL;

	// find the nearest waypoint
	min_index = NO_VAL;
	min_distance = 9999.0f;

	for (i = 0; i < num_waypoints; i++)
	{
		// skip any deleted waypoints
		if (IsWaypoint(i, WptT::deleted))
			continue;

		distance = (waypoints[i].origin - pEntity->v.origin).Length();

		if ((distance < min_distance) && (distance < range))
		{
			// if waypoint is visible from current position (even behind head)...
			UTIL_TraceLine(pEntity->v.origin + pEntity->v.view_ofs, waypoints[i].origin, ignore_monsters, pEntity->v.pContainingEntity, &tr);

			if (tr.flFraction >= 1.0f)
			{
				min_index = i;
				min_distance = distance;
			}
		}
	}

	return min_index;
}


/*
* searches for the nearest waypoint of specified type to the player and returns its index
* returns -1 if no such waypoint was found
*/
int waypoints_and_paths_managing_functions_t::FindNearestWaypointOfTypeToPlayer(edict_t* pEntity, float range, WptT wpt_type)
{
	int i, min_index;
	float distance, min_distance;
	TraceResult tr;

	if (num_waypoints < 1)
		return NO_VAL;

	// find the nearest waypoint
	min_index = NO_VAL;
	min_distance = 9999.0f;

	for (i = 0; i < num_waypoints; i++)
	{
		// skip all not matching waypoints
		if (IsWaypoint(i, wpt_type) == false)
			continue;

		distance = (waypoints[i].origin - pEntity->v.origin).Length();

		if ((distance < min_distance) && (distance < range))
		{
			// if waypoint is visible from current position (even behind head)
			UTIL_TraceLine(pEntity->v.origin + pEntity->v.view_ofs, waypoints[i].origin, ignore_monsters, pEntity->v.pContainingEntity, &tr);

			if (tr.flFraction >= 1.0f)
			{
				min_index = i;
				min_distance = distance;
			}
		}
	}

	return min_index;
}


/*
* goes through all paths and searches for the one that contains given waypoint
* returns the index of a path that has this waypoint or -1 if the waypoint isn't in any path
*/
int waypoints_and_paths_managing_functions_t::FindPath(int wpt_index)
{
	W_PATH* p;
	int path_index;

	int safety_stop = 0;

	for (path_index = 0; path_index < num_w_paths; path_index++)
	{
		// skip free slots
		if (w_paths[path_index] == NULL)
			continue;

		safety_stop = 0;

		p = w_paths[path_index];

		// search whole path for current wpt
		while (p)
		{
			safety_stop++;
			if (safety_stop > LINKEDLIST_LOOPS_THRESHOLD)
				LinkedListError("Find Path", path_index);

			if (p->wpt_index == wpt_index)
				return path_index;

			p = p->next;	// check next node
		}
	}

	return NO_VAL;	// this wpt isn't in any path
}


/*
* tries to find the nearest accessible waypoint to the bot (allows ignoring particular index)
* and returns its index
* returns -1 if waypoint was NOT found
*/
int waypoints_and_paths_managing_functions_t::FindNewWaypointForBot(bot_t* pBot, int skip_this_index)
{
	int i, min_index;
	float distance;
	float min_distance;
	TraceResult tr;
	edict_t* pEdict = pBot->pEdict;

	if (num_waypoints < 1)
		return NO_VAL;

	// find the nearest waypoint
	min_index = NO_VAL;
	min_distance = MAX_WPT_DIST + 1.0f;

	for (i = 0; i < num_waypoints; i++)
	{
		// skip any aiming or deleted waypoints
		if (IsWaypoint(i, WptT::aim, WptT::deleted))
			continue;

		// skip waypoints with no priority
		if (GetWaypointPriority(i, pBot->GetBotTeam()) == 0)
			continue;

		// check if we need to ignore particular waypoint index
		// (bot had some problems at that waypoint, ie most probably bot got stuck there)
		// also skip previously visited waypoints
		if ((i == skip_this_index) || pBot->prev_wpt_index.check(i))
		{
			continue;
		}

		// is this waypoint origin higher than max jump height then skip it
		if (waypoints[i].origin.z > (pEdict->v.origin.z + 45.0f))
			continue;

		distance = (waypoints[i].origin - pEdict->v.origin).Length();

		// we are trying to find the nearest waypoint to the bot
		if (distance < min_distance)
		{
			// is this waypoint accessible
			// ie is there at least one path which can be used by this bot
			if (wptmanager.IsWaypointAccessibleForThisBot(pBot, i))
			{
				// is this waypoint visible from current position (even behind head)?
				UTIL_TraceLine(pEdict->v.origin + pEdict->v.view_ofs, waypoints[i].origin, ignore_monsters, pEdict->v.pContainingEntity, &tr);

				if (tr.flFraction >= 1.0f)
				{
					// then store it
					min_index = i;
					min_distance = distance;
				}
			}
		}
	}

	return min_index;
}


/*
* tries to find a new waypoint to head towards when the bot reached the end of his current path
* first looks for a cross waypoint if none is found then looks for ordinary waypoint
* returns found waypoint index
* returns -1 if none is found
*/
int waypoints_and_paths_managing_functions_t::FindNewWaypointForBotAtPathEnd(bot_t* pBot, int wpt_index)
{
	TraceResult tr;
	float distance, min_distance;
	int min_index;

	// check for problem stuff
	if (wpt_index == NO_VAL)
		return NO_VAL;

	// first we try to find a cross waypoint connected to given waypoint
	min_index = FindConnectedCross(waypoints[wpt_index].origin);

	// if we found any then return it
	if (min_index != NO_VAL)
		return min_index;

	// we need to init these
	min_distance = MAX_WPT_DIST;
	min_index = NO_VAL;

	// go through all waypoints
	for (int index = 0; index < num_waypoints; index++)
	{
		// skip all aiming and deleted waypoints
		if (IsWaypoint(index, WptT::aim, WptT::deleted))
			continue;

		// skip all waypoints on current path
		if (IsWaypointOnPath(index, pBot->curr_path_index))
			continue;

		distance = GetDistanceBetweenWaypoints(wpt_index, index);

		// is this waypoint in current waypoint range OR in half of the maximum reachable range
		// this prevents checking high amount of waypoints --> lower CPU usage
		if ((distance <= waypoints[wpt_index].range) || (distance < (float)(MAX_WPT_DIST / 2.0)))
		{
			// is this waypoint accessible
			// ie is there at least one path that can be used by this bot
			if (IsWaypointAccessibleForThisBot(pBot, index))
			{
				// is this waypoint closer than previous "in range" waypoint
				if (distance < min_distance)
				{
					UTIL_TraceLine(waypoints[wpt_index].origin, waypoints[index].origin, ignore_monsters, NULL, &tr);

					// check if this waypoint is reachable
					if (tr.flFraction >= 1.0f)
					{
						// we will return the nearest waypoint
						min_distance = distance;
						min_index = index;
					}
				}
			}
		}
	}

	// if we found any waypoint then return its index
	// if not then "no value" is returned
	return min_index;
}

int waypoints_and_paths_managing_functions_t::FindNextWaypointOnShortestPath(int startingWaypoint, const Vector& goal) {
	int goalWaypoint = NO_VAL;
	float goalWaypointDistance = 9999.f;
	for(int iWaypoint = 0; iWaypoint < num_waypoints; iWaypoint++) {
		float distance = (waypoints[iWaypoint].origin - goal).Length();
		if(distance < goalWaypointDistance) {
			goalWaypointDistance = distance;
			goalWaypoint = iWaypoint;
		}
	}

	if(goalWaypoint == NO_VAL)
		return NO_VAL;

	static bool visitedWaypoints[MAX_WAYPOINTS];
	memset(visitedWaypoints, false, sizeof(visitedWaypoints));
	static short heapWaypoints[MAX_WAYPOINTS];
	static float heapDistances[MAX_WAYPOINTS];
	heapWaypoints[0] = goalWaypoint;
	heapDistances[0] = goalWaypointDistance;
	int heapSize = 1;

	while(heapSize > 0) {
		heapSize--;
		int iWaypoint = heapWaypoints[0];
		float distance = heapDistances[0];
		for(int i = 0; i < heapSize; i++) {
			heapWaypoints[i] = heapWaypoints[i + 1];
			heapDistances[i] = heapDistances[i + 1];
		}
		if(visitedWaypoints[iWaypoint])
			continue;

		visitedWaypoints[iWaypoint] = true;

		for(int iNeighbor = 0; iNeighbor < num_inv_neighbors[iWaypoint]; iNeighbor++) {
			int neighborWaypoint = inv_neighbors[iWaypoint][iNeighbor];
			if(neighborWaypoint == startingWaypoint)
				return iWaypoint;

			if(visitedWaypoints[neighborWaypoint])
				continue;

			float neighborDistance = (waypoints[neighborWaypoint].origin - waypoints[iWaypoint].origin).Length();

			if((waypoints[neighborWaypoint].flags | waypoints[iWaypoint].flags) & W_FL_CROUCH)
				neighborDistance *= 2.f;

			float totalDistance = distance + neighborDistance + waypoint_penalty[neighborWaypoint];
			int iHeap = heapSize;
			while(iHeap > 0 && heapDistances[iHeap - 1] >= totalDistance) {
				heapWaypoints[iHeap] = heapWaypoints[iHeap - 1];
				heapDistances[iHeap] = heapDistances[iHeap - 1];
				iHeap--;
			}
			heapWaypoints[iHeap] = neighborWaypoint;
			heapDistances[iHeap] = totalDistance;
			heapSize++;
		}
	}
	return NO_VAL;
}

/*
* finds the next waypoint for bot to head towards
* cross waypoint decision making is handled here
* fixed by Alexander
*/
int waypoints_and_paths_managing_functions_t::FindNextWaypointForBot(bot_t* pBot)
{
	// maximum of waypoints around cross waypoint that are taken to final decision routine
	const int max_at_cross = 8;

	int i, j, high_prior_count, current_wpt, the_found, priority, num_found_wpt;
	int w_index[max_at_cross]{}, w_prior[max_at_cross]{};
	float distance;
	TraceResult tr;

	if (num_waypoints < 1)
		return NO_VAL;

	// get current waypoint index
	current_wpt = pBot->curr_wpt_index;

	// is current waypoint a goback waypoint?
	if (IsWaypoint(current_wpt, WptT::goback))
	{
		// take the previous waypoint as the waypoint to continue to (ie turn back), but only if team priority isn't set to no priority
		if (GetWaypointPriority(current_wpt, pBot->GetBotTeam()) != 0)
		{
			// get last visited waypoint
			int wpt_index = pBot->prev_wpt_index.get();

			// clear visited waypoints history
			pBot->prev_wpt_index.clear();

			return wpt_index;
		}
	}

	// is current waypoint a crossroad waypoint?
	if (IsWaypoint(current_wpt, WptT::cross))
	{
		// initialize possible waypoints arrays
		for (i = 0; i < max_at_cross; i++)
		{
			w_index[i] = NO_VAL;
			w_prior[i] = MAX_WPT_PRIOR;
		}

		num_found_wpt = 0;

		// go through all waypoints
		for (i = 0; i < num_waypoints; i++)
		{
			// skip all "invalid" waypoints
			if (IsWaypoint(i, WptT::aim, WptT::cross, WptT::deleted))
				continue;

			// skip current waypoint and waypoints the bot already visited
			if ((i == current_wpt) || pBot->prev_wpt_index.check(i))
			{
				continue;
			}

			// get distance to this waypoint
			distance = GetDistanceBetweenWaypoints(i, current_wpt);

			// waypoint must be far enough, but not too far ie. must be in the range of this cross waypoint
			if ((distance > 5.0f) && (distance <= waypoints[current_wpt].range))
			{
				// get the priority of this waypoint
				priority = GetWaypointPriority(i, pBot->GetBotTeam());

				// skip all waypoints with no priority
				if (priority == 0)
					continue;

				// skip all waypoints that are on path that doesn't match bot team and/or class
				if ((IsWaypointAccessibleForThisBot(pBot, i, true)) == false)
					continue;

				// make sure the waypoint is reachable (i.e. not behind wall, rock, etc.)
				UTIL_TraceLine(waypoints[current_wpt].origin, waypoints[i].origin, ignore_monsters, ignore_glass, pBot->pEdict, &tr);

				// line of sight is not established so try to find another waypoint
				if (tr.flFraction != 1.0f)
					continue;

				// find a free position and store this waypoint on it
				for (j = 0; j < max_at_cross; j++)
				{
					if (w_index[j] == NO_VAL)
					{
						w_index[j] = i;
						w_prior[j] = priority;

						// store this waypoint just once
						break;
					}
				}

				// count the found waypoints
				num_found_wpt++;
			}
		}

		// take only max_at_cross (8) waypoints
		if (num_found_wpt > max_at_cross)
			num_found_wpt = max_at_cross;

		if (botdebugger.IsDebugCross())
		{
			char msg[256];

			sprintf(msg, "\n<<CROSS>>Number of found wpts is %d || The wpts are (index|priority): #%d|%d #%d|%d #%d|%d #%d|%d #%d|%d #%d|%d #%d|%d #%d|%d\n",
				num_found_wpt, w_index[0] + 1, w_prior[0], w_index[1] + 1, w_prior[1], w_index[2] + 1, w_prior[2], w_index[3] + 1, w_prior[3], w_index[4] + 1, w_prior[4], w_index[5] + 1, w_prior[5],
				w_index[6] + 1, w_prior[6], w_index[7] + 1, w_prior[7]);
			conOutput.Print(conInput.GetCommandInvoker(), msg);
		}

		// make sure the bot won't take a waypoint that would get him back to a place he just was (eg. a path starting and ending at the same cross waypoint) therefore
		// we have to reduce the priority of the path starting waypoint even below the default (lowest) value
		if (pBot->prev_path_index != NO_VAL)
		{
			for (i = 0; i < num_found_wpt; i++)
			{
				// see if one of the found waypoints is part of the path the bot just followed
				if (IsWaypointOnPath(w_index[i], pBot->prev_path_index))
				{
					// if so then set super low priority so this waypoint can barely be taken
					w_prior[i] = MAX_WPT_PRIOR + 1;

					if (botdebugger.IsDebugCross())
					{
						char msg[256];
						char wpt_flags[128];
						GetWaypointName(w_index[i], wpt_flags);

						sprintf(msg, "<<CROSS>>Lowering the chance to pick wpt #%d<%s> -> it has been used on current path #%d\n", w_index[i] + 1, wpt_flags, pBot->prev_path_index + 1);
						conOutput.Print(conInput.GetCommandInvoker(), msg);
					}
				}
			}
		}

		// was at least one waypoint found?
		if (num_found_wpt > 0)
		{
			int choice;
			WAYPOINT_VALUE wpt_value[max_at_cross]{};
			int best_wpt = NO_VAL;
			bool run_value_based_decision = false;
			bool act_predictable = false;

			// go through found waypoints to find their value
			for (i = 0; i < num_found_wpt; i++)
			{
				// set this slot waypoint index to no value first so we can check for it later on
				wpt_value[i].wpt_index = NO_VAL;

				// set the usefulness value for this waypoint
				WaypointSetValue(pBot, w_index[i], &wpt_value[i]);

				if (wpt_value[i].wpt_index != NO_VAL)
					run_value_based_decision = true;
			}


#ifdef DEBUG
			//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
			//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
			if (botdebugger.IsDebugCross())
			{
				char msg[256];

				sprintf(msg, "<<DevCROSS>>(runValueBasedDecision?=%d)Value array wpts are (index|value): #%d|%d #%d|%d #%d|%d #%d|%d #%d|%d #%d|%d #%d|%d #%d|%d\n", run_value_based_decision,
					(wpt_value[0].wpt_value > 0 ? wpt_value[0].wpt_index + 1 : 0), wpt_value[0].wpt_value,
					(wpt_value[1].wpt_value > 0 ? wpt_value[1].wpt_index + 1 : 0), wpt_value[1].wpt_value,
					(wpt_value[2].wpt_value > 0 ? wpt_value[2].wpt_index + 1 : 0), wpt_value[2].wpt_value,
					(wpt_value[3].wpt_value > 0 ? wpt_value[3].wpt_index + 1 : 0), wpt_value[3].wpt_value,
					(wpt_value[4].wpt_value > 0 ? wpt_value[4].wpt_index + 1 : 0), wpt_value[4].wpt_value,
					(wpt_value[5].wpt_value > 0 ? wpt_value[5].wpt_index + 1 : 0), wpt_value[5].wpt_value,
					(wpt_value[6].wpt_value > 0 ? wpt_value[6].wpt_index + 1 : 0), wpt_value[6].wpt_value,
					(wpt_value[7].wpt_value > 0 ? wpt_value[7].wpt_index + 1 : 0), wpt_value[7].wpt_value);
				conOutput.Print(conInput.GetCommandInvoker(), msg);
			}
#endif // DEBUG




			// were there any "valuable waypoints" at all?
			if (run_value_based_decision)
			{
				int last_value = -1;

				// set the chance to pick a waypoint that hasn't the highest value
				choice = RANDOM_LONG(1, 100);

				// go through found waypoints again and compare their values, we should pick the one with highest value in most of the time
				for (i = 0; i < num_found_wpt; i++)
				{
					// are there two same weighted waypoints then pick one of them randomly
					if ((last_value == wpt_value[i].wpt_value) && (choice < 50))
					{
						best_wpt = wpt_value[i].wpt_index;
						last_value = wpt_value[i].wpt_value;
					}
					// otherwise try to always pick the most valued waypoint,
					// however there's a small chance to ignore it and either pick different valued waypoint (if exists) or revert to standard priority based choice
					else if ((last_value < wpt_value[i].wpt_value) || (choice < 5))
					{
						best_wpt = wpt_value[i].wpt_index;
						last_value = wpt_value[i].wpt_value;
					}
				}

				// we found the best waypoint for current situation so return it ie. skip the priority based decision
				if (best_wpt != NO_VAL)
				{
					if (botdebugger.IsDebugCross())
					{
						char msg[256];
						char wpt_flags[128];
						GetWaypointName(best_wpt, wpt_flags);

						sprintf(msg, "<<CROSS>>Taking wpt based on current NEEDS -> it's wpt #%d<%s>\n", best_wpt + 1, wpt_flags);
						conOutput.Print(conInput.GetCommandInvoker(), msg);
					}

					return best_wpt;
				}
			}

			// order found waypoints by priority
			for (i = 0; i < (num_found_wpt - 1); i++)
			{
				for (j = i + 1; j < num_found_wpt; j++)
				{
					// is this waypoint priority lower than previous waypoint priority
					if (w_prior[i] > w_prior[j])
					{
						// swap indexes
						high_prior_count = w_index[i];
						w_index[i] = w_index[j];
						w_index[j] = high_prior_count;

						// swap priorities
						high_prior_count = w_prior[i];
						w_prior[i] = w_prior[j];
						w_prior[j] = high_prior_count;
					}
				}
			}

			// count waypoints that have equal priority as the "first" waypoint, because we know that that waypoint has the highest priority around this cross waypoint
			i = 0;
			high_prior_count = 1;

			while ((i < (num_found_wpt - 1)) && (w_prior[i] == w_prior[i + 1]))
			{
				i++;
				high_prior_count++;
			}

			// make a choice percentage for picking the waypoint
			choice = RANDOM_LONG(1, 100);

			// set how often the bot should act predictable (ie. behaviour based on waypoint priorities)
			// priority == 5 case cannot fall directly to completely random choice
			// because that would completely disable 'super low priority' system (above) for this priority setting
			// and this system prevents from using the same waypoint & path again where possible

			// if there is at least one priority == 1 (highest) waypoint then in 90% of time we're acting predictable
			if ((w_prior[0] == 1) && (choice > 9))
				act_predictable = true;
			// if there is/are only priority == 2 waypoint/s then act predictable in 80% of time
			else if ((w_prior[0] == 2) && (choice > 19))
				act_predictable = true;
			// is/are there only priority == 3 waypoint/s ... then act predictable in 70% of time
			else if ((w_prior[0] == 3) && (choice > 29))
				act_predictable = true;
			// is/are only priority == 4 waypoint/s then act predictable in 60% of time
			else if ((w_prior[0] == 4) && (choice > 39))
				act_predictable = true;
			// finally there is/are only priority == 5 waypoint/s then act predictable only in 50% of time
			else if ((w_prior[0] == 5) && (choice > 49))
				act_predictable = true;

			if (act_predictable)
			{
				// if there is only one waypoint with the highest priority then take it
				if (high_prior_count == 1)
				{
					the_found = w_index[0];

					if (botdebugger.IsDebugCross())
					{
						char msg[256];
						char wpt_flags[128];
						GetWaypointName(the_found, wpt_flags);

						sprintf(msg, "<<CROSS>>Taking wpt based on priority -> HIGHEST ONE is wpt #%d<%s>\n", the_found + 1, wpt_flags);
						conOutput.Print(conInput.GetCommandInvoker(), msg);
					}
				}
				// there are more waypoints with the same priority 
				else
				{
					// so pick one of them
					choice = RANDOM_LONG(1, high_prior_count);
					the_found = w_index[choice - 1];

					if (botdebugger.IsDebugCross())
					{
						char msg[256];
						char wpt_flags[128];
						GetWaypointName(the_found, wpt_flags);

						sprintf(msg, "<<CROSS>>Taking wpt based on priority -> MULTIPLE same priority wpts -> picking wpt #%d<%s>\n", the_found + 1, wpt_flags);
						conOutput.Print(conInput.GetCommandInvoker(), msg);
					}
				}
			}
			// otherwise the bot is acting unpredictably (or at least less predictable)
			else
			{
				// see if there is at least one waypoint with higher priority (but not highest) that means ...
				// 1st position cannot be lowest priority because then there would have been only lowest priority waypoints there
				// also position right behind the highest priority waypoint/waypoints cannot be the default priority
				// because then there would have been only highest priority and the rest would be default priority waypoints
				if ((w_prior[0] != MAX_WPT_PRIOR) && (w_prior[high_prior_count] != MAX_WPT_PRIOR) && (RANDOM_LONG(1, 100) > 30))
				{
					// ignore highest priority waypoints, but still try to find a waypoint with higher priority
					// (ie. look for waypoints with priorities between the highest and the default/lowest one)
					// so start at the position right behind the highest priority waypoint
					i = high_prior_count;
					priority = 0;	// it's used as a counter

					while ((i < (num_found_wpt - 1)) && (w_prior[i] == w_prior[i + 1]))
					{
						i++;
						priority++;
					}

					// there's just one waypoint with 2nd highest priority
					if (priority == 0)
					{
						the_found = w_index[high_prior_count];

						if (botdebugger.IsDebugCross())
						{
							char msg[256];
							char wpt_flags[128];
							GetWaypointName(the_found, wpt_flags);

							sprintf(msg, "<<CROSS>>Ignoring ONLY highest priority -> picking wpt #%d<%s>\n", the_found + 1, wpt_flags);
							conOutput.Print(conInput.GetCommandInvoker(), msg);
						}
					}
					// there must be more of them
					else
					{
						choice = high_prior_count + RANDOM_LONG(0, priority);
						the_found = w_index[choice];

						if (botdebugger.IsDebugCross())
						{
							char msg[256];
							char wpt_flags[128];
							GetWaypointName(the_found, wpt_flags);

							sprintf(msg, "<<CROSS>>Ignoring ONLY highest priority -> picking wpt #%d<%s> from total of %d like waypoints\n", the_found + 1, wpt_flags, priority + 1);
							conOutput.Print(conInput.GetCommandInvoker(), msg);
						}
					}

					// check for error in the code and "patch" it on the fly
					if (the_found == NO_VAL)
					{
						// this one has to exist
						the_found = w_index[0];

						util.DebugInFile("<<BUG>> Cross waypoint decision based on priority -> ignoring highest priority -> refered to non-existent waypoint\n");
					}

				}
				// finally if everything failed then just pick one waypoint randomly
				else
				{
					// pick from all waypoints found around this cross waypoint
					choice = RANDOM_LONG(1, num_found_wpt);
					the_found = w_index[choice - 1];

					if (botdebugger.IsDebugCross())
					{
						char msg[256];
						char wpt_flags[128];
						GetWaypointName(the_found, wpt_flags);

						if (num_found_wpt == 1)
							sprintf(msg, "<<CROSS>>Ignoring priorities -> The ONLY wpt is #%d<%s>\n", the_found + 1, wpt_flags);
						else
							sprintf(msg, "<<CROSS>>Ignoring all priorities -> entirely RANDOM PICK of wpt #%d<%s>\n", the_found + 1, wpt_flags);

						conOutput.Print(conInput.GetCommandInvoker(), msg);
					}
				}
			}

			return the_found;
		}
	}
	// otherwise go towards next waypoint in row
	else
	{
		bool wpt_visible;
		float the_nearest = MAX_WPT_DIST;
		w_index[0] = NO_VAL;

		edict_t* pEdict = pBot->pEdict;

		// pick from neighbors
		if(current_wpt != NO_VAL && num_neighbors[current_wpt] > 0) {
			edict_t* pGoal = pBot->GetGoal();
			if(!pGoal || (pGoal->v.owner && pGoal->v.owner != pEdict)) {
				pGoal = NULL;
				int numGoals = 0;

				edict_t* pGoalCandidate = NULL;
				while((pGoalCandidate = util.FindEntityByClassname(pGoalCandidate, "bot_goal")))
					if(!pGoalCandidate->v.owner || pGoalCandidate->v.owner == pEdict)
						numGoals++;

				int randomGoal = RANDOM_LONG(0, numGoals - 1);
				while((pGoalCandidate = util.FindEntityByClassname(pGoalCandidate, "bot_goal"))) {
					if(pGoalCandidate->v.owner && pGoalCandidate->v.owner != pEdict)
						continue;

					randomGoal--;
					if(randomGoal >= 0)
						continue;

					pGoal = pGoalCandidate;
					break;
				}
				pBot->SetGoal(pGoal);
			}

			if(pGoal) {
				auto nextShortestPathWaypoint = FindNextWaypointOnShortestPath(current_wpt, pGoal->v.origin);
				if(nextShortestPathWaypoint) {
					waypoint_penalty[nextShortestPathWaypoint] += 10.f;
					return nextShortestPathWaypoint;
				}
			}
			else {
				// pick random neighbor
				return neighbors[current_wpt][RANDOM_LONG(0, num_neighbors[current_wpt] - 1)];
			}
		}

		// find the nearest waypoint
		for (i = 0; i < num_waypoints; i++)
		{
			// skip this waypoint and all the waypoints the bot already visited
			if ((i == current_wpt) || pBot->prev_wpt_index.check(i))
			{
				continue;
			}

			// skip the aim and deleted waypoints
			if (IsWaypoint(i, WptT::aim, WptT::deleted))
				continue;

			// skip ladder waypoints if on end ladder
			if (pBot->HasReachedEndOfLadder() && IsWaypoint(i, WptT::ladder))
				continue;

			distance = GetDistanceBetweenWaypoints(i, current_wpt);

			// we are looking only for the nearest waypoint
			if (distance > the_nearest)
				continue;

			// is the bot going to leave a ladder so don't check visibility just target the nearest
			if (pBot->HasReachedEndOfLadder())
				wpt_visible = TRUE;
			// is this the first waypoint we are trying to find then we don't need to face it yet
			else if (pBot->prev_wpt_index.get() == NO_VAL)
			{
				wpt_visible = util.IsVisible(waypoints[i].origin, pEdict);
			}
			else
				wpt_visible = (util.IsInViewCone(&waypoints[i].origin, pEdict) && util.IsVisible(waypoints[i].origin, pEdict));

			// waypoint must be visible/reachable
			if (wpt_visible)
			{
				// skip all waypoints that are on path that doesn't match bot team and/or class
				if (IsWaypointAccessibleForThisBot(pBot, i))
				{
					the_nearest = distance;
					w_index[0] = i;
				}
			}
		}

		if (w_index[0] != NO_VAL)
		{
			if (botdebugger.IsDebugWaypoints())
			{
				char msg[256];
				char wpt_flags[128];
				GetWaypointName(w_index[0], wpt_flags);

				sprintf(msg, "ONLY WPT NAV - Heading towards to waypoint #%d<%s> (curr wpt #%d)\n",	w_index[0] + 1, wpt_flags, pBot->curr_wpt_index + 1);
				conOutput.Print(NULL, msg);
			}

			return w_index[0];	// now the bot has next waypoint
		}
	}

	return NO_VAL;
}


/*
* tries to find nearby "special" aiming waypoints
* returns -1 if none is found (the return value isn't checked anywhere though)
*/
int waypoints_and_paths_managing_functions_t::FindAimingWaypointsForBot(bot_t* pBot, int wpt_index)
{
	int index;
	float distance;

	// go through all waypoints...
	for (index = 0; index < num_waypoints; index++)
	{
		// skip all waypoints that are NOT an aiming waypoint
		if (IsWaypoint(index, WptT::aim) == false)
			continue;

		// skip aim waypoints with NO PRIORITY for this bot team
		if (GetWaypointPriority(index, pBot->GetBotTeam()) == 0)
			continue;

		// get the distance from given waypoint
		distance = GetDistanceBetweenWaypoints(wpt_index, index);

		// is this aiming waypoint within the range?
		if (distance <= MAX_AIM_WPT_DIST)
		{
			// then make the bot know about this aim waypoint
			pBot->Aims.AddNewAimWpt(index);
		}
	}

	return pBot->Aims.Get(0);
}


/*
* returns distance to the waypoint passed by the index
* if either the edict or the waypoint isn't valid it will return 9999.0
*/
float waypoints_and_paths_managing_functions_t::GetDistanceToWaypoint(edict_t* pEdict, int wpt_index)
{
	if ((pEdict != NULL) && IsValidWaypointIndex(wpt_index))
		return (pEdict->v.origin - waypoints[wpt_index].origin).Length();

	return 9999.0f;
}


/*
* returns distance between two waypoints passed by their indexes
* if either of the indexes isn't valid it will return 9999.0
*/
float waypoints_and_paths_managing_functions_t::GetDistanceBetweenWaypoints(int wpt1_index, int wpt2_index)
{
	if (IsValidWaypointIndex(wpt1_index) && IsValidWaypointIndex(wpt2_index))
		return (waypoints[wpt1_index].origin - waypoints[wpt2_index].origin).Length();
	
	return 9999.0f;
}


/*
* returns correct waypoint priority based on team
* also works with the trigger waypoint priorities
*/
int waypoints_and_paths_managing_functions_t::GetWaypointPriority(int wpt_index, int team)
{
	if (IsValidWaypointIndex(wpt_index))
	{
		if (IsWaypoint(wpt_index, WptT::trigger))
			return GetTriggerWaypointPriority(wpt_index, team);

		if (team == teamONE.GetTeamId())
			return waypoints[wpt_index].red_priority;

		if (team == teamTWO.GetTeamId())
			return waypoints[wpt_index].blue_priority;
	}

	return 0;			// like no priority
}


/*
* returns correct priority for trigger waypoint based on the state of game events
*/
int waypoints_and_paths_managing_functions_t::GetTriggerWaypointPriority(int wpt_index, int team)
{
	int return_no_priority = 0;

	if (wpt_index == NO_VAL)
		return return_no_priority;				// like no priority

	if (IsWaypoint(wpt_index, WptT::trigger))
	{
		TriggerId trigger_on = waypoints[wpt_index].trigger_event_on;
		TriggerId trigger_off = waypoints[wpt_index].trigger_event_off;

		float trigger_on_time, trigger_off_time;
		trigger_on_time = trigger_off_time = 0.0f;

		if (trigger_on != TriggerId::trigger_none)
		{
			for (int i = 0; i < MAX_TRIGGERS; i++)
			{
				// this trigger slot is used and the game event linked to it has already been triggered
				if (trigger_gamestate[i].GetUsed() && trigger_gamestate[i].GetTriggered())
				{
					// then check if this trigger event is set for the waypoint and get the time of the trigger_on event (ie. when it happened)
					if (trigger_on == trigger_gamestate[i].GetName())
						trigger_on_time = trigger_gamestate[i].GetTime();

					// get the time of the trigger_off event (if it is used on the waypoint)
					if (trigger_off != TriggerId::trigger_none &&		// is there any trigger_off event set for this waypoint
						(trigger_off == trigger_gamestate[i].GetName()))
						trigger_off_time = trigger_gamestate[i].GetTime();
				}
			}

			// if the trigger_on event happened after the trigger_off event
			// then we have to return the trigger priorities, because this waypoints is triggered on
			if (trigger_on_time > trigger_off_time)
			{
				// so we must return the trigger priorities instead of standard ones
				if (team == teamONE.GetTeamId())
					return waypoints[wpt_index].trigger_red_priority;
				if (team == teamTWO.GetTeamId())
					return waypoints[wpt_index].trigger_blue_priority;

				return return_no_priority;		// just for sure if something went wrong
			}
		}
	}

	// return normal priority because of:
	// 1) this waypoint is not a trigger waypoint or
	// 2) the trigger_off event has been called (ie. happened) so the trigger waypoint is being turned off and thus we have to use the normal priorities again
	if (team == teamONE.GetTeamId())
		return waypoints[wpt_index].red_priority;
	if (team == teamTWO.GetTeamId())
		return waypoints[wpt_index].blue_priority;

	return return_no_priority;		// just for sure if something went wrong
}


/*
* returns correct waypoints wait time based on team
*/
float waypoints_and_paths_managing_functions_t::GetWaypointWaitTime(int wpt_index, int team)
{
	if (wpt_index == NO_VAL)
		return 0.0f;			// like no wait time

	if (team == teamONE.GetTeamId())
		return waypoints[wpt_index].red_time;

	if (team == teamTWO.GetTeamId())
		return waypoints[wpt_index].blue_time;

	return 0.0f;
}


/*
* converts the waypoint flag of given wpt_index to a string ie. to a waypoint name
* returns "unknown" if waypoint is invalid or there is unknown flag on it
*/
void waypoints_and_paths_managing_functions_t::GetWaypointName(int wpt_index, char* wpt_names)
{
	int flags;

	// just in case
	if (wpt_index == NO_VAL)
	{
		strcpy(wpt_names, "unknown");
		return;
	}

	// get the waypoint flag
	flags = waypoints[wpt_index].flags;

	// init this string
	strcpy(wpt_names, "");

	if (flags & W_FL_SNIPER)
		strcat(wpt_names, "sniper ");
	if (flags & W_FL_FIRE)
		strcat(wpt_names, "shoot ");
	if (flags & W_FL_AIMING)
		strcat(wpt_names, "aim ");
	if (flags & W_FL_AMMOBOX)
		strcat(wpt_names, "ammobox ");
	if (flags & W_FL_BANDAGE)
		strcat(wpt_names, "bandages ");
	if (flags & W_FL_CHUTE)
		strcat(wpt_names, "parachute ");
	if (flags & W_FL_CROSS)
		strcat(wpt_names, "cross ");
	if (flags & W_FL_CROUCH)
		strcat(wpt_names, "crouch ");
	if (flags & W_FL_DOOR)
		strcat(wpt_names, "door ");
	if (flags & W_FL_DOORUSE)
		strcat(wpt_names, "usedoor ");
	if (flags & W_FL_GOBACK)
		strcat(wpt_names, "goback ");
	if (flags & W_FL_JUMP)
		strcat(wpt_names, "jump ");
	if (flags & W_FL_DUCKJUMP)
		strcat(wpt_names, "duckjump ");
	if (flags & W_FL_LADDER)
		strcat(wpt_names, "ladder ");
	if (flags & W_FL_MINE)
		strcat(wpt_names, "claymore ");
	if (flags & W_FL_PRONE)
		strcat(wpt_names, "prone ");
	if (flags & W_FL_PUSHPOINT)
		strcat(wpt_names, "flag ");
	if (flags & W_FL_ROADBLOCK)
		strcat(wpt_names, "roadblock ");
	if (flags & W_FL_SPRINT)
		strcat(wpt_names, "sprint ");
	if (flags & W_FL_TRIGGER)
		strcat(wpt_names, "trigger ");
	if (flags & W_FL_USE)
		strcat(wpt_names, "use ");

	if (flags & W_FL_STD)
	{
		// if this is a crouch or prone waypoint don't print this flag
		if (flags & (W_FL_CROUCH | W_FL_PRONE))
			;
		else
			strcat(wpt_names, "normal ");
	}

	if (flags & W_FL_DELETED)
		strcat(wpt_names, "deleted/erased ");

	// if there are no flags on this waypoint set unknown flag
	if (CountWaypointFlags(wpt_index) < 1)
		strcat(wpt_names, "unknown ");

	// cut the space at the end of the string
	int length = strlen(wpt_names);
	wpt_names[length - 1] = '\0';

	return;
}


/*
* returns the waypoint type based on waypoint name
* invalid or unknown name will result in returning the value of the scrapped waypoint type
*/
WptT waypoints_and_paths_managing_functions_t::GetWaypointTypeFromName(const char* waypoint_name)
{
	// except for the normal waypoint the rest is in alphabetical order of the waypoint names
	if (FStrEq(waypoint_name, "normal"))
		return WptT::normal;
	else if (FStrEq(waypoint_name, "aim"))
		return WptT::aim;
	else if (FStrEq(waypoint_name, "ammobox"))
		return WptT::ammobox;
	else if (FStrEq(waypoint_name, "claymore"))
		return WptT::claymore;
	else if (FStrEq(waypoint_name, "cross"))
		return WptT::cross;
	else if (FStrEq(waypoint_name, "crouch"))
		return WptT::crouch;
	else if (FStrEq(waypoint_name, "door"))
		return WptT::door;
	else if (FStrEq(waypoint_name, "duckjump"))
		return WptT::duckjump;
	else if (FStrEq(waypoint_name, "goback"))
		return WptT::goback;
	else if (FStrEq(waypoint_name, "jump"))
		return WptT::jump;
	else if (FStrEq(waypoint_name, "ladder"))
		return WptT::ladder;
	else if (FStrEq(waypoint_name, "parachute"))
		return WptT::parachute;
	else if (FStrEq(waypoint_name, "prone"))
		return WptT::prone;
	else if ((FStrEq(waypoint_name, "pushpoint")) || (FStrEq(waypoint_name, "flag")))
		return WptT::pushpoint;
	else if (FStrEq(waypoint_name, "roadblock"))
		return WptT::roadblock;
	else if (FStrEq(waypoint_name, "shoot"))
		return WptT::shoot;
	else if (FStrEq(waypoint_name, "sprint"))
		return WptT::sprint;
	else if (FStrEq(waypoint_name, "sniper"))
		return WptT::sniper;
	else if (FStrEq(waypoint_name, "trigger"))
		return WptT::trigger;
	else if (FStrEq(waypoint_name, "use"))
		return WptT::use;
	else if (FStrEq(waypoint_name, "usedoor"))
		return WptT::dooruse;

	return WptT::scrapped_flagtype;
}


/*
* returns a pointer to the waypoint if it is on the path otherwise returns NULL
*/
W_PATH* waypoints_and_paths_managing_functions_t::GetWaypointPointer(int wpt_index, int path_index)
{
	if ((path_index < 0) || (path_index >= MAX_W_PATHS))
		return NULL;

	W_PATH* p = w_paths[path_index];
	int safety_stop = 0;

	while (p)
	{
		// is the path wpt index same to searched wpt index
		if (p->wpt_index == wpt_index)
			return p;

		p = p->next;

		safety_stop++;
		if (safety_stop > LINKEDLIST_LOOPS_THRESHOLD)
			LinkedListError("Get Waypoint Pointer", path_index);
	}

	return NULL;
}


/*
* returns a pointer to a waypoint of given type if it is on the path otherwise returns NULL
*/
W_PATH* waypoints_and_paths_managing_functions_t::GetWaypointTypePointer(WptT wpt_type, int path_index)
{
	if ((path_index < 0) || (path_index >= MAX_W_PATHS))
		return NULL;

	W_PATH* p = w_paths[path_index];
	int safety_stop = 0;

	while (p)
	{
		// is this path waypoint the type we are looking for?
		if (IsWaypoint(p->wpt_index, wpt_type))
			return p;

		p = p->next;

		safety_stop++;
		if (safety_stop > LINKEDLIST_LOOPS_THRESHOLD)
			LinkedListError("Get Waypoint Type Pointer", path_index);
	}

	return NULL;
}


/*
* returns the class of the path passed by path_index
* returns "unknown" if class is invalid or there is unknown flag
*/
void waypoints_and_paths_managing_functions_t::GetPathClass(int path_index, char* the_class)
{
	// just in case
	if (path_index == NO_VAL)
	{
		strcpy(the_class, "unknown");
		return;
	}

	// init this string
	strcpy(the_class, "");

	if (IsPath(path_index, PathT::all_classes))
		strcpy(the_class, "all classes");
	else
	{
		if (IsPath(path_index, PathT::sniper_class))
		{
			strcpy(the_class, "snipers");
		}
		if (IsPath(path_index, PathT::mgunner_class))
		{
			if (strlen(the_class) > 1)
				strcat(the_class, " & ");

			strcat(the_class, "mgunners");
		}
		if (IsPath(path_index, PathT::antiarmor_class))
		{
			if (strlen(the_class) > 1)
				strcat(the_class, " & ");

			strcat(the_class, "anti-armor specs");
		}

		// check for the longest class tag name and if the string is shorter than that then it's a path for only one particular class so "mark" it that way
		if (strlen(the_class) < 18)
			strcat(the_class, " only!");
	}
	/*/


		if (IsPath(path_index, PathT::sniper_class) && IsPath(path_index, PathT::mgunner_class))
			strcpy(the_class, "snipers & mgunners");
		else
		{
			if (IsPath(path_index, PathT::sniper_class))
				strcpy(the_class, "snipers only!");
			else if (IsPath(path_index, PathT::mgunner_class))
				strcpy(the_class, "mgunners only!");
			else if (IsPath(path_index, PathT::antiarmor_class))
				strcpy(the_class, "anti-armor only!");
			else
				strcpy(the_class, "unknown");
		}
	}
	/**/

	// no known tag found?
	if (strlen(the_class) < 1)
		strcpy(the_class, "unknown");// then return unknown path type

	// add the new line character at the end of the string
	strcat(the_class, "\n");

	return;
}


/*
* returns path length (ie. total amount of nodes/waypoints)
* returns -1 if the path doesn't exist
*/
int waypoints_and_paths_managing_functions_t::GetPathLength(int path_index)
{
	int the_length;
	W_PATH* p;
	int safety_stop = 0;

	// check if path exist
	if (w_paths[path_index] == NULL)
		return -1;

	// init length
	the_length = 0;

	// set the pointer to the head node
	p = w_paths[path_index];

	// go through whole path and increase the_length
	while (p)
	{
		the_length++;

		p = p->next;

		safety_stop++;
		if (safety_stop > LINKEDLIST_LOOPS_THRESHOLD)
			LinkedListError("Get Path Length", path_index);
	}

	return the_length;
}


/*
* returns path start (ie. the first waypoint index on the path)
* returns "no value" (ie. -1) if the path doesn't exist
*/
int waypoints_and_paths_managing_functions_t::GetPathStart(int path_index)
{
	// set the pointer to the head node
	W_PATH* p = w_paths[path_index];

	// get the first waypoint
	if (p)
	{
		return p->wpt_index;
	}

	return NO_VAL;
}


/*
* returns path end (ie. the last waypoint index on the path)
* returns -1 if the path doesn't exist
*/
int waypoints_and_paths_managing_functions_t::GetPathEnd(int path_index)
{
	int the_end_wpt = NO_VAL;
	W_PATH* p;
	int safety_stop = 0;

	// check if path exist
	if (w_paths[path_index] == NULL)
		return NO_VAL;

	// set the pointer to the head node
	p = w_paths[path_index];

	// go through whole path and update the_end_wpt with path wpt_index
	while (p)
	{
		the_end_wpt = p->wpt_index;

		p = p->next;

		safety_stop++;
		if (safety_stop > LINKEDLIST_LOOPS_THRESHOLD)
			LinkedListError("Get Path End", path_index);
	}

	return the_end_wpt;
}


/*
* returns next waypoint to given waypoint on given path if exists of course otherwise returns -1
* works only with start to end direction
*/
int waypoints_and_paths_managing_functions_t::GetPathNextWaypoint(int wpt_index, int path_index)
{
	// check validity first
	if ((wpt_index == NO_VAL) || (path_index == NO_VAL))
		return NO_VAL;

	W_PATH* p = w_paths[path_index];
	int safety_stop = 0;

	while (p)
	{
		// is the path waypoint index same to the waypoint we are looking for ...
		if (p->wpt_index == wpt_index)
		{
			W_PATH* next = p->next;

			// check if there is a next waypoint and return it
			if (next)
			{
				return next->wpt_index;
			}
		}

		p = p->next;

		safety_stop++;
		if (safety_stop > LINKEDLIST_LOOPS_THRESHOLD)
			LinkedListError("Get Path Next Waypoint", path_index);
	}

	return NO_VAL;
}


/*
* returns previous waypoint to given waypoint on given path if exists of course otherwise returns -1
* works only with start to end direction
*/
int waypoints_and_paths_managing_functions_t::GetPathPreviousWaypoint(int wpt_index, int path_index)
{
	// check validity first
	if ((wpt_index == NO_VAL) || (path_index == NO_VAL))
		return NO_VAL;

	W_PATH* p = w_paths[path_index];
	int safety_stop = 0;

	while (p)
	{
		// is the path waypoint index same to the waypoint we are looking for ...
		if (p->wpt_index == wpt_index)
		{
			W_PATH* prev = p->prev;

			// check if there is a previous waypoint and return it
			if (prev)
			{
				return prev->wpt_index;
			}
		}

		p = p->next;

		safety_stop++;
		if (safety_stop > LINKEDLIST_LOOPS_THRESHOLD)
			LinkedListError("Get Path Previous Waypoint", path_index);
	}

	return NO_VAL;
}


/*
* returns TRUE if this waypoint is of given type
*/
bool waypoints_and_paths_managing_functions_t::IsWaypoint(int wpt_index, WptT wpt_type)
{
	if ((IsValidWaypointIndex(wpt_index) == false) || (wpt_type <= WptT::scrapped_flagtype))
		return false;

	int flag = 0;

	// convert the wpt_type to a real waypoint flag
	// the order is set by how much are particular types used
	if (wpt_type == WptT::normal)
		flag = W_FL_STD;
	else if (wpt_type == WptT::crouch)
		flag = W_FL_CROUCH;
	else if (wpt_type == WptT::prone)
		flag = W_FL_PRONE;
	else if (wpt_type == WptT::cross)
		flag = W_FL_CROSS;
	else if (wpt_type == WptT::aim)
		flag = W_FL_AIMING;
	else if (wpt_type == WptT::deleted)
		flag = W_FL_DELETED;
	else if (wpt_type == WptT::goback)
		flag = W_FL_GOBACK;
	else if (wpt_type == WptT::sniper)
		flag = W_FL_SNIPER;
	else if (wpt_type == WptT::jump)
		flag = W_FL_JUMP;
	else if (wpt_type == WptT::duckjump)
		flag = W_FL_DUCKJUMP;
	else if (wpt_type == WptT::ammobox)
		flag = W_FL_AMMOBOX;
	else if (wpt_type == WptT::ladder)
		flag = W_FL_LADDER;
	else if (wpt_type == WptT::sprint)
		flag = W_FL_SPRINT;
	else if (wpt_type == WptT::shoot)
		flag = W_FL_FIRE;
	else if (wpt_type == WptT::claymore)
		flag = W_FL_MINE;
	else if (wpt_type == WptT::parachute)
		flag = W_FL_CHUTE;
	else if (wpt_type == WptT::pushpoint)
		flag = W_FL_PUSHPOINT;
	else if (wpt_type == WptT::trigger)
		flag = W_FL_TRIGGER;
	else if (wpt_type == WptT::cover)
		flag = W_FL_COVER;
	else if (wpt_type == WptT::bandage)
		flag = W_FL_BANDAGE;
	else if (wpt_type == WptT::roadblock)
		flag = W_FL_ROADBLOCK;
	else if (wpt_type == WptT::use)
		flag = W_FL_USE;
	else if (wpt_type == WptT::door)
		flag = W_FL_DOOR;
	else if (wpt_type == WptT::dooruse)
		flag = W_FL_DOORUSE;

	// is the flag set on this waypoint?
	if (waypoints[wpt_index].flags & flag)
		return true;

	return false;
}


/*
* returns TRUE if this waypoint matches one of given types
*/
bool waypoints_and_paths_managing_functions_t::IsWaypoint(int wpt_index, WptT wpt_type1, WptT wpt_type2)
{
	if (IsWaypoint(wpt_index, wpt_type1) || IsWaypoint(wpt_index, wpt_type2))
		return true;

	return false;
}


/*
* returns TRUE if this waypoint matches one of given types
*/
bool waypoints_and_paths_managing_functions_t::IsWaypoint(int wpt_index, WptT wpt_type1, WptT wpt_type2, WptT wpt_type3)
{
	if (IsWaypoint(wpt_index, wpt_type1) || IsWaypoint(wpt_index, wpt_type2) || IsWaypoint(wpt_index, wpt_type3))
		return true;

	return false;
}


/*
* returns TRUE if either priority on given waypoint matches given value
* doesn't handle trigger priorities
* doesn't allow checks for non zero priority
*/
bool waypoints_and_paths_managing_functions_t::IsWaypointPriority(int wpt_index, int searched_priority)
{
	if (IsValidWaypointIndex(wpt_index) == false)
		return false;

	if ((waypoints[wpt_index].red_priority == searched_priority) || (waypoints[wpt_index].blue_priority == searched_priority))
		return true;

	return false;
}


/*
* returns TRUE if given waypoint index matches given type and matches given priority for given team
* also allows validate checks for non zero priority
*/
bool waypoints_and_paths_managing_functions_t::IsWaypointTypeTeamPriority(int wpt_index, WptT wpt_type, int searched_priority, int for_team)
{
	if (IsValidWaypointIndex(wpt_index) == false)
		return false;

	if (IsWaypoint(wpt_index, wpt_type))
	{
		// this allows handle statements "if priority != 0 then do something"
		if (searched_priority == NON_ZERO_WPT_PRIORITY)
		{
			if ((GetWaypointPriority(wpt_index, for_team) > MIN_WPT_PRIOR) && (GetWaypointPriority(wpt_index, for_team) <= MAX_WPT_PRIOR))
				return true;
		}

		if (GetWaypointPriority(wpt_index, for_team) == searched_priority)
			return true;
	}

	return false;
}


/*
* returns TRUE if given waypoint is used for navigation (not an aim waypoint) and is it one of those where the bot doesn't use the range ie. where bot always heads towards real waypoint position
*/
bool waypoints_and_paths_managing_functions_t::IsNoRangeWaypoint(int wpt_index)
{
	return IsWaypoint(wpt_index, WptT::door, WptT::dooruse, WptT::ladder);
}


/*
* looks up given waypoint type on given path and returns TRUE if it matches given priority for given team
* also allows validate checks for non zero priority
*/
bool waypoints_and_paths_managing_functions_t::IsPathWaypointTypeTeamPriority(int path_index, WptT wpt_type, int searched_priority, int for_team)
{
	W_PATH* p = GetWaypointTypePointer(wpt_type, path_index);

	if (p)
	{
		if (searched_priority == NON_ZERO_WPT_PRIORITY)
		{
			if ((GetWaypointPriority(p->wpt_index, for_team) > MIN_WPT_PRIOR) && (GetWaypointPriority(p->wpt_index, for_team) <= MAX_WPT_PRIOR))
				return true;
		}

		if (GetWaypointPriority(p->wpt_index, for_team) == searched_priority)
			return true;
	}

	return false;
}


/*
* looks up given waypoint type on given path, then checks whether the next path waypoint is the same type as well and returns TRUE if such pair exists and both waypoints match given priority for given team
* also allows validate checks for non zero priority
*/
bool waypoints_and_paths_managing_functions_t::IsPathWaypointPairTypeTeamPriority(int path_index, WptT wpt_type, int searched_priority, int for_team)
{
	W_PATH* p = GetWaypointTypePointer(wpt_type, path_index);

	if (p)
	{
		int the_other_wpt_from_pair = GetPathNextWaypoint(p->wpt_index, path_index);

		if ((the_other_wpt_from_pair != NO_VAL) && IsWaypoint(the_other_wpt_from_pair, wpt_type))
		{
			if (searched_priority == NON_ZERO_WPT_PRIORITY)
			{
				if ((GetWaypointPriority(p->wpt_index, for_team) > MIN_WPT_PRIOR) && (GetWaypointPriority(p->wpt_index, for_team) <= MAX_WPT_PRIOR) &&
					(GetWaypointPriority(the_other_wpt_from_pair, for_team) > MIN_WPT_PRIOR) && (GetWaypointPriority(the_other_wpt_from_pair, for_team) <= MAX_WPT_PRIOR))
					return true;
			}

			if ((GetWaypointPriority(p->wpt_index, for_team) == searched_priority) && (GetWaypointPriority(the_other_wpt_from_pair, for_team) == searched_priority))
				return true;
		}
	}

	return false;
}


/*
* returns TRUE if the waypoint (via wpt_index) is on path (via path_index)
* returns FALSE if waypoint is NOT on that path or waypoint/path is invalid
*/
bool waypoints_and_paths_managing_functions_t::IsWaypointOnPath(int wpt_index, int path_index)
{
	if ((wpt_index == NO_VAL) || (path_index < 0) || (path_index >= MAX_W_PATHS))
		return false;

	if (GetWaypointPointer(wpt_index, path_index) != NULL)
		return true;

	return false;
}


/*
* returns TRUE if there is given waypoint type in path (path_index)
* returns FALSE if waypoint is NOT in path or waypoint/path is invalid
*/
bool waypoints_and_paths_managing_functions_t::IsWaypointTypeOnPath(WptT wpt_type, int path_index)
{
	if ((path_index < 0) || (path_index >= MAX_W_PATHS))
		return false;

	if (GetWaypointTypePointer(wpt_type, path_index) != NULL)
		return true;

	return false;
}


/*
* returns TRUE if waypoint (wpt_index) is on path and it's on its start or end
* returns FALSE if waypoint is NOT at either of path ends or is NOT on path at all
*/
bool waypoints_and_paths_managing_functions_t::IsWaypointAtPathEnd(int wpt_index, int path_index)
{
	if (wpt_index == NO_VAL)
		return false;

	// if path was not specified then try to find it
	if (path_index < 0)
		path_index = FindPath(wpt_index);

	if (path_index == NO_VAL)
		return false;

	// does the waypoint match the waypoint at one or the other end of this path?
	if ((wpt_index == GetPathStart(path_index)) || (wpt_index == GetPathEnd(path_index)))
		return true;

	return false;
}


/*
* returns TRUE if waypoint (wpt_index) is closer to end of the path (path_index)
*/
bool waypoints_and_paths_managing_functions_t::IsWaypointCloserToPathEnd(int wpt_index, int path_index)
{
	if ((wpt_index == NO_VAL) || (path_index == NO_VAL))
		return false;

	W_PATH* p = w_paths[path_index];
	int safety_stop = 0, nodes_count = 0, wpt_position = 0;

	while (p)
	{
		// is the path wpt index same to the searched wpt index
		if (p->wpt_index == wpt_index)
			wpt_position = nodes_count;

		p = p->next;

		nodes_count++;

		safety_stop++;
		if (safety_stop > LINKEDLIST_LOOPS_THRESHOLD)
			LinkedListError("Is Path End Closer", path_index);
	}

	// is waypoint position before the centre of this path then return FALSE
	if (wpt_position < int(nodes_count / 2))
		return false;
	// otherwise the waypoint is closer to path end
	else
		return true;

	return false;
}


/*
* returns TRUE if previous or next waypoint to a waypoint passed as a pointer on path is of given type
*/
bool waypoints_and_paths_managing_functions_t::IsWaypointTypeNeighbourOnPath(W_PATH* wpt_pointer, WptT wpt_type)
{
	if (wpt_pointer == NULL)
		return false;

	if (wpt_pointer->next && IsWaypoint(wpt_pointer->next->wpt_index, wpt_type))
		return true;

	if (wpt_pointer->prev && IsWaypoint(wpt_pointer->prev->wpt_index, wpt_type))
		return true;

	return false;
}


/*
* returns TRUE if the path is of given type
*/
bool waypoints_and_paths_managing_functions_t::IsPath(int path_index, PathT path_type)
{
	if ((path_index == NO_VAL) || (path_type <= PathT::scrapped_flagtype))
		return false;

	if (w_paths[path_index] == NULL)
		return false;

	int flag = 0;

	if (path_type == PathT::both_teams)
		flag = P_FL_TEAM_NO;
	else if (path_type == PathT::team_one)
		flag = P_FL_TEAM_RED;
	else if (path_type == PathT::team_two)
		flag = P_FL_TEAM_BLUE;
	else if (path_type == PathT::one_way)
		flag = P_FL_WAY_ONE;
	else if (path_type == PathT::two_way)
		flag = P_FL_WAY_TWO;
	else if (path_type == PathT::patrol_cycle)
		flag = P_FL_WAY_PATROL;
	else if (path_type == PathT::all_classes)
		flag = P_FL_CLASS_ALL;
	else if (path_type == PathT::sniper_class)
		flag = P_FL_CLASS_SNIPER;
	else if (path_type == PathT::mgunner_class)
		flag = P_FL_CLASS_MGUNNER;
	else if (path_type == PathT::antiarmor_class)
		flag = P_FL_CLASS_ANTIARMOR;
	else if (path_type == PathT::ammo_tag)
		flag = P_FL_MISC_AMMO;
	else if (path_type == PathT::bandages_tag)
		flag = P_FL_MISC_BANDAGES;
	else if (path_type == PathT::roadblocked_tag)
		flag = P_FL_MISC_ROADBLOCKED;
	else if (path_type == PathT::goal_team_one_tag)
		flag = P_FL_MISC_GOAL_RED;
	else if (path_type == PathT::goal_team_two_tag)
		flag = P_FL_MISC_GOAL_BLUE;
	else if (path_type == PathT::goal_explosives_tag)
		flag = P_FL_MISC_GEXPLOSIVES;
	else if (path_type == PathT::avoid_far_enemy)
		flag = P_FL_MISC_AVOID;
	else if (path_type == PathT::ignore_the_enemy)
		flag = P_FL_MISC_IGNORE;
	else if (path_type == PathT::carry_goal_item)
		flag = P_FL_MISC_GITEM;

	// is the flag set on this path?
	if (w_paths[path_index]->flags & flag)
		return true;

	return false;
}


/*
* returns TRUE if the path matches one of given types
*/
bool waypoints_and_paths_managing_functions_t::IsPath(int path_index, PathT path_type1, PathT path_type2)
{
	if (IsPath(path_index, path_type1) || IsPath(path_index, path_type2))
		return true;

	return false;
}


/*
* returns TRUE if the path matches one of given types
*/
bool waypoints_and_paths_managing_functions_t::IsPath(int path_index, PathT path_type1, PathT path_type2, PathT path_type3)
{
	if (IsPath(path_index, path_type1) || IsPath(path_index, path_type2) || IsPath(path_index, path_type3))
		return true;

	return false;
}


/*
* checks if this waypoint is possible for the bot
* returns TRUE if bot team & class matches restrictions of at least one path on this waypoint
* if there is no path on this waypoint then it is possible as well
*/
bool waypoints_and_paths_managing_functions_t::IsWaypointAccessibleForThisBot(bot_t* pBot, int wpt_index, bool is_called_at_crosswpt)
{
	// aim and deleted waypoints are always forbidden
	if (IsWaypoint(wpt_index, WptT::aim, WptT::deleted))
		return false;

	// cross waypoints are always possible
	if (IsWaypoint(wpt_index, WptT::cross))
		return true;

	// if there are no paths at all then all waypoints are accessible
	if (num_w_paths < 1)
		return true;

	// to know that there are any paths on this waypoint
	int num_of_paths_on_wpt = 0;

	// go through all paths
	// first we must check if the waypoint is on path to get correct amount of paths on it and then check if that/those path/paths is/are accessible for this bot
	for (int path_index = 0; path_index < num_w_paths; path_index++)
	{
		// skip free path slots
		if (w_paths[path_index] == NULL)
			continue;

		// is the waypoint (wpt_index) on this path
		if (IsWaypointOnPath(wpt_index, path_index))
		{
			// mark this waypoint as a pathwpt ie there's some path on it
			num_of_paths_on_wpt++;

			// is this path possible for this bot ie do bot's team & class and path team & class restriction match
			if (IsPathAccessibleForThisBot(pBot, path_index))
			{
				// is this waypoint the ending waypoint of one-way path
				if (IsPath(path_index, PathT::one_way) && (wpt_index == GetPathEnd(path_index)))
					continue;
				// is the bot unable to fulfill the map goal that is on this path
				else if (is_called_at_crosswpt &&
					((IsPath(path_index, PathT::goal_team_one_tag) && pBot->IsBotTeam(teamONE.GetTeamId())) || (IsPath(path_index, PathT::goal_team_two_tag) && pBot->IsBotTeam(teamTWO.GetTeamId()))) &&
					(IsPushpointGoalOnPathReachableForThisBot(pBot, path_index) == false))
					continue;
				else
					return true;
			}
		}
	}

	// there are paths on this waypoint, but this bot doesn't match their restrictions so this waypoint cannot be used
	if (num_of_paths_on_wpt > 0)
		return false;

	// there is no path on this waypoint so it can be used
	return true;
}


/*
* checks if this bot can use the path (ie does bot team & class match path type)
* returns TRUE if bot can use it
* returns FALSE if he can't
*/
bool waypoints_and_paths_managing_functions_t::IsPathAccessibleForThisBot(bot_t* pBot, int path_index)
{
	if (path_index == NO_VAL)
		return false;

	// is the path blocked right now?
	if (IsPath(path_index, PathT::roadblocked_tag) && (IsRoadblockedPathException(pBot, path_index) == false))
		return false;

	// is it a patrol type path AND bot is NOT defender (ie only defenders can use this path)
	if (IsPath(path_index, PathT::patrol_cycle) && (pBot->IsBehaviour(DEFENDER) == false))
		return false;

	// is it a team restricted path AND bot team matches this path team restriction
	if ((IsPath(path_index, PathT::team_one) && pBot->IsBotTeam(teamONE.GetTeamId())) || (IsPath(path_index, PathT::team_two) && pBot->IsBotTeam(teamTWO.GetTeamId())))
	{
		// is this path all class path (ie no class restriction)
		if (IsPath(path_index, PathT::all_classes))
			return true;

		// is this path for snipers AND bot is sniper
		if (IsPath(path_index, PathT::sniper_class) && pBot->IsBehaviour(SNIPER))
			return true;
		// is this path for mgunners AND bot is mgunner
		if (IsPath(path_index, PathT::mgunner_class) && pBot->IsBehaviour(MGUNNER))
			return true;
		// is this path for anti-armor specialist AND bot is a-aspecialist
		if (IsPath(path_index, PathT::antiarmor_class) && pBot->IsBehaviour(AASPEC))
			return true;
	}

	// or is this path for both teams
	else if (IsPath(path_index, PathT::both_teams))
	{
		// is this path all class path (ie no class restriction)
		if (IsPath(path_index, PathT::all_classes))
			return true;

		// is this path only for some classes and has the bot that assignment?

		// is this path for snipers
		if (IsPath(path_index, PathT::sniper_class))
		{
			//  and bot is sniper OR CQuarter OR Common soldier with Defensive behaviour
			if (pBot->IsBehaviour(SNIPER) || pBot->IsBehaviour(CQUARTER) || (pBot->IsBehaviour(COMMON) && pBot->IsBehaviour(DEFENDER)))
				return true;
		}
		// is this path for mgunners
		if (IsPath(path_index, PathT::mgunner_class))
		{
			//  and bot is mgunner OR CQuarter OR Common soldier with Defensive behaviour
			if (pBot->IsBehaviour(MGUNNER) || pBot->IsBehaviour(CQUARTER) || (pBot->IsBehaviour(COMMON) && pBot->IsBehaviour(DEFENDER)))
				return true;
		}
		// is this path for anti-armor specialists
		if (IsPath(path_index, PathT::antiarmor_class))
		{
			// NOTE: Unsure whether those so called 'camper hunter' bots should be able to access this path too. For now they won't be!
			
			if (pBot->IsBehaviour(AASPEC))
				return true;
		}
	}

	return false;
}


/*
* returns TRUE if this bot meets requirements to ignore the roadblocked tag and can enter on given path
*/
inline bool waypoints_and_paths_managing_functions_t::IsRoadblockedPathException(bot_t* pBot, int path_index)
{
	// is it a non one-way path where is a goal placed claymore waypoint and this bot carries needed explosives? OR
	// is it a non one-way anti-armor specialist path where is a pair of roadblock waypoints with priority 1 setting and this bot is an anti-armor specialist and has ammo for the rocket/grenade launcher?
	return ((IsPath(path_index, PathT::goal_explosives_tag) && pBot->IsEquippedWithExplosiveCharge() &&	(IsPath(path_index, PathT::one_way) == false) &&
		IsPathWaypointTypeTeamPriority(path_index, WptT::claymore, 1, pBot->GetBotTeam())) ||
		(IsPath(path_index, PathT::antiarmor_class) && pBot->IsBehaviour(AASPEC) && (pBot->IsNoAmmoForMainWeapon() == false) && (IsPath(path_index, PathT::one_way) == false) &&
			IsPathWaypointPairTypeTeamPriority(path_index, WptT::roadblock, 1, pBot->GetBotTeam())));
}


/*
* checks the next waypoint on the path whether it matches given type and returns TRUE if nothing blocks in reaching it
* (all player entities are ignored though so a stuck teammate may eventually prevent this bot in reaching the next waypoint)
*/
bool waypoints_and_paths_managing_functions_t::IsPairedWaypointReachableForThisBot(bot_t* pBot, WptT wpt_type)
{
	int paired_wpt_index = pBot->GetNextWaypointOnPath();

	if ((paired_wpt_index != NO_VAL) && IsWaypoint(paired_wpt_index, wpt_type) && util.IsPointReachable(waypoints[pBot->curr_wpt_index].origin, waypoints[paired_wpt_index].origin, true))
		return true;
	
	return false;
}


/*
* checks whether the last visited patrol waypoint is still reachable for this bot
* returns FALSE if not
*/
bool waypoints_and_paths_managing_functions_t::IsPatrolWaypointReachableForThisBot(bot_t* pBot)
{
	edict_t* pEdict;
	TraceResult tr;

	// check for problem stuff
	if ((num_waypoints < 1) || (pBot->IsPatrolPathWaypoint() == false))
		return false;

	pEdict = pBot->pEdict;

	// find how far away is the patrol waypoint from bot
	float distance = (pEdict->v.origin - waypoints[pBot->GetPatrolPathWaypoint()].origin).Length();

	// is it still in reachable range?
	if (distance <= MAX_WPT_DIST)
	{
		// is the waypoint visible from bot's current position
		UTIL_TraceLine(pEdict->v.origin, waypoints[pBot->GetPatrolPathWaypoint()].origin, ignore_monsters, pEdict->v.pContainingEntity, &tr);

		if (tr.flFraction >= 1.0f)
		{
			return true;
		}
	}

	return false;
}


/*
* scans the surroundings of a pushpoint on given path for Capture Area entity and checks whether it requires explosives charge to capture
* if so and the bot has explosives charge then it returns TRUE
* also returns TRUE if it is a standard "capture the flag" Capture Area or if there is no Capture Area around the pushpoint
*/
bool waypoints_and_paths_managing_functions_t::IsPushpointGoalOnPathReachableForThisBot(bot_t* pBot, int path_index)
{
	int caparea_array_index = CAPTUREPOINTS_ERROR_VAL;

	// find the pushpoint waypoint on this path
	W_PATH* p = wptmanager.GetWaypointTypePointer(WptT::pushpoint, path_index);

	if (p)
	{
		edict_t* pent = NULL;

		// search for the Capture Area around this waypoint
		while ((pent = util.FindEntityInSphere(pent, waypoints[p->wpt_index].origin, WPT_RANGE)) != NULL)
		{
			if (util.IsEntityName(pent, "dod_capture_area"))
			{
				caparea_array_index = dodCaptureArea->FindPointInArray(pent);

				break;
			}
		}

		// did we find any Capture Area near the pushpoint waypoint?
		if (caparea_array_index != CAPTUREPOINTS_ERROR_VAL)
		{
			// is it a standard flag type Capture Area OR does it require goal item (eg. explosives charge) and this bot has a goal item right now?
			if ((dodCaptureArea->GetDodObjectRequired(caparea_array_index) == false) || (dodCaptureArea->GetDodObjectRequired(caparea_array_index) && pBot->IsTask(TASK_GOALITEM)))
				return true;	// then this goal is reachable for this bot
		}
		// otherwise it must be a Control Point based goal so the bot can reach it...
		else
		{
			// unless this waypoint is also a claymore in which case the bot must carry the explosives charge in order to reach this goal
			// eg. The Bridge on map Escape, where the Axis bot needs the TNT charge to destroy it, without the charge there's no point for the bot to go there
			if (wptmanager.IsWaypoint(p->wpt_index, WptT::claymore) && (pBot->IsEquippedWithExplosiveCharge() == false))
				return false;
			else
				return true;
		}
	}

	return false;
}


/*
* checks whether there is accessible path on wpt_index
* if there are multiple of them then it tries to find the best path for the bot
* returns TRUE when such path is assigned as bot's current path
*/
bool waypoints_and_paths_managing_functions_t::WasPossiblePathForBotFoundOnWaypoint(bot_t* pBot, int wpt_index)
{
	// waypoint is NOT valid or are there NO paths?
	if ((IsValidWaypointIndex(wpt_index) == false) || (num_w_paths < 1))
		return false;

	int path_index = NO_VAL;
	const int processed_paths_limit = 8;			// just pick some decent number
	PATH_VALUE processed_paths[processed_paths_limit]{};

	pathvaluer.SetPathValueArraySize(processed_paths_limit);
	pathvaluer.InitPathValueArray(processed_paths);

	// go through all paths
	for (path_index = 0; path_index < num_w_paths; path_index++)
	{
		// skip free slots
		if (w_paths[path_index] == NULL)
			continue;

		// is this path possible for bot (ie do team and class restriction match)
		if (IsPathAccessibleForThisBot(pBot, path_index))
		{
			// is the waypoint (wpt_index) in this path
			if (IsWaypointOnPath(wpt_index, path_index))
			{
				// skip all one-way paths that end on this waypoint
				if (IsPath(path_index, PathT::one_way) && (wpt_index == GetPathEnd(path_index)))
					continue;

				// now we will set appropriate value/weight for this path based on path flags and bot behaviour, needs or tasks
				for (int i = 0; i < processed_paths_limit; i++)
				{
					if (processed_paths[i].path_index == NO_VAL)
					{
						pathvaluer.SetValue(pBot, path_index, &processed_paths[i]);
						break;
					}
				}
				
			}
		}
	}

	if (botdebugger.IsDebugPaths())
	{
		char msg[256];

		sprintf(msg, "<<PATHS>>Available paths (index|value): #%d|%d #%d|%d #%d|%d #%d|%d #%d|%d #%d|%d #%d|%d #%d|%d\n",
			processed_paths[0].path_index+1, processed_paths[0].path_value, processed_paths[1].path_index+1, processed_paths[1].path_value,	processed_paths[2].path_index+1, processed_paths[2].path_value,
			processed_paths[3].path_index+1, processed_paths[3].path_value,	processed_paths[4].path_index+1, processed_paths[4].path_value, processed_paths[5].path_index+1, processed_paths[5].path_value,
			processed_paths[6].path_index+1, processed_paths[6].path_value, processed_paths[7].path_index+1, processed_paths[7].path_value);
		conOutput.Print(NULL, msg);
	}

	// did we find any path?
	if (pathvaluer.IsAnyPathAvailable(processed_paths))
	{
		// did we find any valuable paths? (ie. instead of just common paths)
		if (pathvaluer.IsAnyValuedPath(processed_paths))
		{
			path_index = pathvaluer.GetMostValuedPath(processed_paths);

			if (botdebugger.IsDebugCross())
			{
				char msg[TEXT_MSG_SIZE];
				
				if (pathvaluer.GetValuedPathsCount() > 1)
					sprintf(msg, "<<PATHS>>Picking path #%d based on current NEEDS on waypoint #%d from %d like paths\n", path_index + 1, wpt_index + 1, pathvaluer.GetValuedPathsCount());
				else
					sprintf(msg, "<<PATHS>>Picking path #%d based on current NEEDS on waypoint #%d\n", path_index + 1, wpt_index + 1);

				conOutput.Print(conInput.GetCommandInvoker(), msg);
			}
		}
		// otherwise if there are just common paths then pick one randomly
		else
		{
			path_index = pathvaluer.GetRandomPath(processed_paths);

			if (botdebugger.IsDebugCross())
			{
				char msg[TEXT_MSG_SIZE];

				if (pathvaluer.GetAvailablePathsCount() > 1)
					sprintf(msg, "<<PATHS>>Picking path #%d from total of %d paths on waypoint #%d\n", path_index + 1, pathvaluer.GetAvailablePathsCount(), wpt_index + 1);
				else
					sprintf(msg, "<<PATHS>>Picking path #%d on waypoint #%d\n", path_index + 1, wpt_index + 1);

				conOutput.Print(conInput.GetCommandInvoker(), msg);
			}
		}

		//@@@@@@@@@@@@@@@@@@
		if (w_paths[path_index] == NULL)
		{
#ifdef _DEBUG
			char msg[128];
			sprintf(msg, "<wpt.cpp | WasPossiblePathForBotFoundOnWpt() -> Oooops the path points to a null path!!\n");
			util.DebugDev(msg, wpt_index, path_index);
#endif

			return false;
		}

		// update path history
		pBot->prev_path_index = pBot->curr_path_index;

		// set the new path
		pBot->curr_path_index = path_index;

		// is waypoint closer to the path end AND NOT on one-way path? then set opposite direction path moves
		if ((IsWaypointCloserToPathEnd(wpt_index, path_index)) && (IsPath(path_index, PathT::one_way) == false))
			pBot->SetTask(TASK_OPPOSITEPATHDIR);

		// for debugging
		if (botdebugger.IsDebugPaths())
		{
			char msg[64];
			sprintf(msg, "<<PATHS>> ***Got new path! (index=%d)\n", pBot->curr_path_index + 1);
			conOutput.Print(NULL, msg);
		}

		return true;
	}

	return false;
}


/*
* checks both ends of a path whether they are connected to the same cross waypoint
* if no path is given then it will check all paths
* returns the index of the waypoint that is farther away from the cross waypoint
* returns -1 if nothing like that is found or given waypoint or path are not valid
*/
int waypoints_and_paths_managing_functions_t::AreBothPathEndsConnectedToOneCrossWaypoint(int wpt_index, int path_index)
{
	int wpt_on_other_pathend = NO_VAL;
	int cross_wpt1 = NO_VAL;
	int cross_wpt2 = NO_VAL;
	float distance1, distance2;


	// TODO:	Find a good way to handle One-way paths.
	//			Currently it doesn't check that at all, but generally it would be better
	//			if the path start had the priority even if it was farther away from the cross waypoint.
	//			However in some cases like bottom of the cliff or in water or respawn area for example
	//			the start of such One-way path is purposely made loose in order to get the bots from
	//			player-spawn entity or area they landed on to the first junction (cross wpt).



	// deal with invalid or deleted waypoints
	if ((wpt_index == NO_VAL) || IsWaypoint(wpt_index, WptT::deleted))
		return NO_VAL;

	// are we checking just one specific path
	if (path_index != NO_VAL)
	{
		if (w_paths[path_index] == NULL)
			return NO_VAL;

		// is the waypoint at the start of the path?
		if (wpt_index == GetPathStart(path_index))
		{
			// get the index of connected cross waypoint
			cross_wpt1 = FindConnectedCross(waypoints[wpt_index].origin);

			// and get the index of the waypoint at the other end of this path
			wpt_on_other_pathend = GetPathEnd(path_index);
		}

		// or is it at the end of this path?
		if (wpt_index == GetPathEnd(path_index))
		{
			cross_wpt1 = FindConnectedCross(waypoints[wpt_index].origin);
			wpt_on_other_pathend = GetPathStart(path_index);
		}

		// if the waypoint is on a path then we must try to find the index of the cross waypoint that
		// could be connected to the waypoint on the other end of this path
		if (wpt_on_other_pathend != NO_VAL)
			cross_wpt2 = FindConnectedCross(waypoints[wpt_on_other_pathend].origin);

		// are both ends of this path connected to a cross waypoint AND is it the same cross waypoint?
		if ((cross_wpt1 != NO_VAL) && (cross_wpt2 != NO_VAL) && (cross_wpt1 == cross_wpt2))
		{
			// get the distance to both ends of this path ...
			distance1 = GetDistanceBetweenWaypoints(wpt_index, cross_wpt1);
			distance2 = GetDistanceBetweenWaypoints(wpt_on_other_pathend, cross_wpt1);

			// and return the index of the waypoint that is farther away from the cross waypoint

			if (distance1 > distance2)
				return wpt_index;

			return wpt_on_other_pathend;
		}
	}
	// otherwise go through all paths
	else
	{
		for (path_index = 0; path_index < num_w_paths; path_index++)
		{
			if (w_paths[path_index] == NULL)
				continue;

			if (wpt_index == GetPathStart(path_index))
			{
				cross_wpt1 = FindConnectedCross(waypoints[wpt_index].origin);
				wpt_on_other_pathend = GetPathEnd(path_index);
			}

			if (wpt_index == GetPathEnd(path_index))
			{
				cross_wpt1 = FindConnectedCross(waypoints[wpt_index].origin);
				wpt_on_other_pathend = GetPathStart(path_index);
			}

			if (wpt_on_other_pathend != NO_VAL)
				cross_wpt2 = FindConnectedCross(waypoints[wpt_on_other_pathend].origin);

			if ((cross_wpt1 != NO_VAL) && (cross_wpt2 != NO_VAL) && (cross_wpt1 == cross_wpt2))
			{
				distance1 = GetDistanceBetweenWaypoints(wpt_index, cross_wpt1);
				distance2 = GetDistanceBetweenWaypoints(wpt_on_other_pathend, cross_wpt1);

				if (distance1 > distance2)
					return wpt_index;

				return wpt_on_other_pathend;
			}
		}
	}

	return NO_VAL;
}


/*
* compares two paths and returns TRUE if both path1 and path2 have given path type (ie are of the same type)
*/
bool waypoints_and_paths_managing_functions_t::AreBothPathsOfSameType(int path_index1, int path_index2, PathT path_type)
{
	if ((path_index1 == NO_VAL) || (path_index2 == NO_VAL) || (path_type <= PathT::scrapped_flagtype))
		return false;

	if (IsPath(path_index1, path_type) && IsPath(path_index2, path_type))
		return true;

	return false;
}


/*
* compares two paths based on team limits
* returns TRUE if the checked path is less than or equally limitative to the restrictive path
*/
bool waypoints_and_paths_managing_functions_t::CanSwitchToThisPathDueToTeamLimitingFactors(int checked_path, int restrictive_path)
{
	// from red team only path ...
	if (IsPath(restrictive_path, PathT::team_one))
	{
		// we can always access any red team only path or any path for both teams
		if (AreBothPathsOfSameType(restrictive_path, checked_path, PathT::team_one) || IsPath(checked_path, PathT::both_teams))
			return true;
	}

	if (IsPath(restrictive_path, PathT::team_two))
	{
		if (AreBothPathsOfSameType(restrictive_path, checked_path, PathT::team_two) || IsPath(checked_path, PathT::both_teams))
			return true;
	}

	// from both team path ...
	if (IsPath(restrictive_path, PathT::both_teams))
	{
		// we can always access only another path for both teams
		if (AreBothPathsOfSameType(restrictive_path, checked_path, PathT::both_teams))
			return true;
	}

	return false;
}


/*
* compares two paths based on class limits
* returns TRUE if the checked path is less than or equally limitative to the restrictive path
*/
bool waypoints_and_paths_managing_functions_t::CanSwitchToThisPathDueToClassLimitingFactors(int checked_path, int restrictive_path)
{
	// from sniper only path we can always ...
	if (IsPath(restrictive_path, PathT::sniper_class))
	{
		// access another sniper path or path for all classes
		if (AreBothPathsOfSameType(restrictive_path, checked_path, PathT::sniper_class) || IsPath(checked_path, PathT::all_classes))
			return true;
	}

	if (IsPath(restrictive_path, PathT::mgunner_class))
	{
		if (AreBothPathsOfSameType(restrictive_path, checked_path, PathT::mgunner_class) || IsPath(checked_path, PathT::all_classes))
			return true;
	}

	if (IsPath(restrictive_path, PathT::antiarmor_class))
	{
		if (AreBothPathsOfSameType(restrictive_path, checked_path, PathT::antiarmor_class) || IsPath(checked_path, PathT::all_classes))
			return true;
	}

	if (IsPath(restrictive_path, PathT::all_classes))
	{
		if (AreBothPathsOfSameType(restrictive_path, checked_path, PathT::all_classes))
			return true;
	}

	return false;
}




trigger_event_gamestate_t::trigger_event_gamestate_t()
{
	name = TriggerId::trigger_none;
	used = false;
	triggered = false;
	time = 0.0f;
}


void trigger_event_gamestate_t::Init(int index)
{
	name = TriggerIndexToId(index);
	used = false;
	triggered = false;
	time = 0.0f;
}


void LinkedListError(const char *location, int path_index)
{
   int y = 1, x = 1;
   char message[256];

   if (path_index != NO_VAL)
	   sprintf(message,"LinkedListError @ %s for path no. <%d>!!!\n", location, path_index + 1);
   else
	   sprintf(message, "LinkedListError @ %s!!!\n", location);

   util.DebugInFile(message);

   x = x - 1;  // x is zero
   y = y / x;  // cause an divide by zero exception

   return;
}


/*
* frees all trigger event messages
*/
void FreeAllTriggers(void)
{
	for (int trigger_index = 0; trigger_index < MAX_TRIGGERS; trigger_index++)
	{
		trigger_events[trigger_index].name = TriggerIndexToId(trigger_index);
		trigger_events[trigger_index].message[0] = '\0';
		// we also have to initialize the current game state of the triggers
		trigger_gamestate[trigger_index].Init(trigger_index);
	}
}


void ResetTriggersOnRoundEnd(void)
{
	if (botdebugger.IsDebugWaypoints())
		conOutput.Notify("All triggers have been set back to \"not triggered\"\n");

	for (int i = 0; i < MAX_TRIGGERS; i++)
	{
		trigger_gamestate[i].SetTriggered(false);
		trigger_gamestate[i].SetTime();
	}
}


/*
* checks the message for double space or new line characters as these would invalidate the string comparison
*/
bool IsMessageValid(const char *msg)
{
	// just in case it's not handled before calling this
	if (msg == NULL)
		return false;

	int length = strlen(msg);
	for (int i = 0; i < length; i++)
	{
		if (msg[i] == '\n')
			return false;

		if ((msg[i] == ' ') && (msg[i+1] == ' '))
			return false;
	}

	return true;
}


/*
* compares the text with trigger messages and searches for matching one
*/
bool IsMatchingTriggerMessage(char *the_text)
{
	if (the_text == NULL)
		return false;

	//int len;

	//@@@@@@@@@@@@@@@
	/*/
	#ifdef _DEBUG
	//if (botdebugger.IsDebugWaypoints())
		ALERT(at_console, "IsMatchingTriggerMSG() -> the msg before while statement is: (%s)\n", the_text);
	//util.DebugDev(the_text, -100, -100);
	#endif
	/**/

	//	DOESN'T SEEM TO BE NEEDED IN DoD BECAUSE OF THE WAY HOW DO WE HAVE TO WORK WITH IT - THE MESSAGE IS ALREADY PREFORMATTED SO IT'S NICE AND CLEAN
	/*/
	// see if the message is nice and clean ie. no additional space or new line characters
	while (IsMessageValid(the_text) == false)
	{
		len = strlen(the_text);
		for (int ch = 0; ch < len; ch++)
		{
			// replace new line character with a space
			if (the_text[ch] == '\n')
				the_text[ch] = ' ';

			// is there a space following another space then we have probably touched the string already
			// so we need to remove the additional space by shifting the rest of the string
			if ((the_text[ch] == ' ') && (the_text[ch+1] == ' '))
			{
				for (int s = ch; s < len; s++)
					the_text[s] = the_text[s+1];
				len--;
			}
		}
	}
	/**/

	//@@@@@@@@@@@@@@@
	/*/
	#ifdef _DEBUG
	//if (botdebugger.IsDebugWaypoints())
		ALERT(at_console, "IsMatchingTriggerMSG() -> the final msg is: (%s)\n", the_text);
	//util.DebugDev(the_text, -100, -100);
	#endif
	/**/

	// then compare it with the trigger events
	for (int i = 0; i < MAX_TRIGGERS; i++)
	{
		// is this slot used and does the text match stored trigger message?
		if ((trigger_events[i].name == trigger_gamestate[i].GetName()) && trigger_gamestate[i].GetUsed() && (strstr(the_text, trigger_events[i].message) != NULL))
		{
			if (botdebugger.IsDebugWaypoints())
				ALERT(at_console, "Trigger%d has been triggered\n", i+1);

			// then mark it as event happened
			trigger_gamestate[i].SetTriggered(true);
			// also store the time of this event
			trigger_gamestate[i].SetTime();

			return true;
		}
	}

	return false;
}


/*
* converts trigger name (string) to integer value based on waypoint_triggers_names array
* returns -1 (ie. NO VAL) if no match is found
*/
int TriggerNameToInt(const char* trigger_name)
{
	// we could start on index = 1 to skip the "no_event", but if someone changed the order of both the enum and the array with names then this function wouldn't work correctly
	for (int index = 0; index < (MAX_TRIGGERS + 1); index++)
	{
		if (FStrEq(trigger_name, waypoint_triggers_names[index]))
			return index;
	}

	return NO_VAL;
}


/*
* converts trigger name (string) to trigger events/gamestate array index
* returns -1 (ie. NO VAL) if no match is found
*/
int TriggerNameToIndex(const char *trigger_name)
{
	int array_index = TriggerNameToInt(trigger_name);

	if (array_index != NO_VAL)
		return array_index - 1;// we need array index so "trigger1" must return 0 not 1

	return NO_VAL;
}


/*
* converts trigger name (string) to trigger ID
*/
TriggerId TriggerNameToId(const char *trigger_name)
{
	int enum_order = 0;

	enum_order = TriggerNameToInt(trigger_name);

	if (enum_order != NO_VAL)
		return static_cast<TriggerId>(enum_order);

	return TriggerId::trigger_none;
}


/*
* converts trigger events/gamestate array index to trigger ID
*/
TriggerId TriggerIndexToId(int i)
{
	if ((i >= 0) && (i < MAX_TRIGGERS))
		return static_cast<TriggerId>(i + 1);
	
	return TriggerId::trigger_none;
}


/*
* converts trigger ID to integer value
*/
int TriggerIdToInt(TriggerId triggerId)
{
	return static_cast<int>(triggerId);
}


/*
* converts integer to trigger ID
* needed for old waypoints conversion, because old waypoint structure worked with integer value, now we use trigger IDs everywhere
* no point to rewrite this function to universal state, because the number of triggers cannot be changed in old waypoints
*/
TriggerId IntToTriggerId(int i)
{
	if (i == 1)
		return TriggerId::trigger1;
	else if (i == 2)
		return TriggerId::trigger2;
	else if (i == 3)
		return TriggerId::trigger3;
	else if (i == 4)
		return TriggerId::trigger4;
	else if (i == 5)
		return TriggerId::trigger5;
	else if (i == 6)
		return TriggerId::trigger6;
	else if (i == 7)
		return TriggerId::trigger7;
	else if (i == 8)
		return TriggerId::trigger8;
	else
		return TriggerId::trigger_none;
}


/*
* creates completely new path starting on given waypoint
*/
bool StartNewPath(int wpt_index, int array_slot)
{
	W_PATH *p;
	int free_path_index;

	// did we reach the maximum of w_paths?
	if (num_w_paths >= MAX_W_PATHS)
	{
		ALERT(at_error, "MarineBot - you've reached the maximum of paths!\n");

		return false;
	}

	// end it if there's one of these issues
	if ((wpt_index == NO_VAL) || wptmanager.IsWaypoint(wpt_index, WptT::aim, WptT::cross, WptT::deleted))
		return false;
	
	free_path_index = array_slot;

	// find first available slot for the new path
	while (free_path_index < num_w_paths)
	{
		if (w_paths[free_path_index] == NULL)
			break;

		free_path_index++;
	}

	p = (W_PATH *)malloc(sizeof(W_PATH));	// create new head node

	if (p == NULL)
	{
		ALERT(at_error, "MarineBot - Error allocating memory for path!\n");

		return false;
	}

	p->wpt_index = wpt_index;	// save the wpt index to first/head node

	p->prev = NULL;		// no previous node (this one it the first - the head node)
	p->next = NULL;		// no next node yet

	w_paths[free_path_index] = p;	// store the pointer to new path

	// init the flags value just for sure
	w_paths[free_path_index]->flags = 0;

	// set the default flags
	w_paths[free_path_index]->flags |= P_FL_TEAM_NO | P_FL_CLASS_ALL | P_FL_WAY_TWO;

	internals.SetPathToContinue(free_path_index);	// save path index to continue in

	// increase total number of paths if adding at the end of the array
	if (free_path_index == num_w_paths)
		num_w_paths++;

	ALERT(at_console, "starting new path ...\n");

	return true;
}


/*
* continues in current path
* returns -3 if invalid wpt (ie this wpt type can't be in path)
* returns -2 if no path to continue (ie internals::path to continue is NO_VAL)
* returns -1 if not enough memory
* returns 0 if path doesn't exist (not sure if this can happen)
* returns 1 if everything is OK
* returns 2 if the wpt is already present in our path
*/
int ContinueCurrPath(int wpt_index, bool check_presence)
{
	int path_index;

	// don't add these waypoint types into paths
	if ((wpt_index == NO_VAL) || wptmanager.IsWaypoint(wpt_index, WptT::aim, WptT::cross, WptT::deleted))
		return -3;

	path_index = internals.GetPathToContinue();

	if (path_index == NO_VAL)
	{
		// print this only when auto waypointing is enabled
		if (wptser.IsAutoWaypointing())
			ALERT(at_console, "there's no path to continue on waypoint no. %d\n", wpt_index + 1);

		return -2;			// no path to continue
	}

	// if the path is valid add wpt_index to the end of that path
	if (w_paths[path_index])
	{
		W_PATH *p = w_paths[path_index];	// set the pointer to the head node
		W_PATH *prev_node = NULL;			// temp pointer to previous node
		int safety_stop = 0;

		while (p)
		{
			// by default check if the new waypoint isn't already in this path
			// (ie. the same waypoint would have been added twice in one path), because
			// the bot would then enter infinite loop on such path
			// so all we do here is that we simply skip adding it into the path
			// this check is disabled on map load
			if ((p->wpt_index == wpt_index) && check_presence)
				return 2;

			prev_node = p;	// save the previous node in linked list

			p = p->next;	// go to next node in linked list

			safety_stop++;
			if (safety_stop > LINKEDLIST_LOOPS_THRESHOLD)
				LinkedListError("Continue Curr Path", path_index);
		}

		p = (W_PATH *)malloc(sizeof(W_PATH));	// create new node

		if (p == NULL)
		{
			ALERT(at_error, "MarineBot - Error allocating memory for path!\n");

			return -1;		// no memory for path
		}
		
		p->wpt_index = wpt_index;	// store new wpt index
		
		p->next = NULL;			// NULL next node

		if (prev_node != NULL)
		{
			p->prev = prev_node;	// save pointer to prev node in linked list

			prev_node->next = p;	// link new node into existing list
		}

	}
	else
		return 0;	// path isn't valid

	return 1;	// everything is OK
}


/*
* removes waypoint (passed by index - wpt_index) from specified (path_index) path
*/
bool ExcludeFromPath(int wpt_index, int path_index)
{
	// check if waypoint and path_index are valid
	if ((wpt_index == NO_VAL) || (path_index == NO_VAL))
		return false;

	// does the path exist
	if (w_paths[path_index] == NULL)
		return false;

	W_PATH* p = wptmanager.GetWaypointPointer(wpt_index, path_index);

	return ExcludeFromPath(p, path_index);
}


/*
* removes waypoint (passed by pointer) from specified (path_index) path
*/
bool ExcludeFromPath(W_PATH *p, int path_index)
{
	// check validity first
	if ((p == NULL) || (path_index == NO_VAL))
		return false;

	// exist prev node as well as next node
	if ((p->prev) && (p->next))
	{
		p->prev->next = p->next;	// link previous node (its next pointer) to next node
	}
	// if exist only prev node (removing last wpt from ll)
	else if (p->prev)
		p->prev->next = NULL;	// NULL prev pointer for next node

	// exist prev node as well as next node
	if ((p->next) && (p->prev))
	{
		p->next->prev = p->prev;	// link next node (its prev pointer) to previous node
	}
	// if exist only next node (removing first wpt from ll)
	else if (p->next)
	{
		p->next->prev = NULL;	// NULL prev pointer for next node

		// temp store path flags for transfer
		int path_flags = w_paths[path_index]->flags;

		w_paths[path_index] = p->next;	// update paths array

		// write them back to path
		w_paths[path_index]->flags = path_flags;
	}
	// if exist only head node ie only one wpt in whole path
	else if ((p->next == NULL) && (p->prev == NULL))
	{
		// free this path slot
		w_paths[path_index] = NULL;
	}

	// free/destroy this node
	free(p);

	return true;	// everything is OK
}


/*
* removes whole path (frees linked list) and nulls the pointer in w_paths array
*/
bool DeleteWholePath(int path_index)
{
	// check again if the path is valid (ie if exist)
	if ((path_index == NO_VAL) || (w_paths[path_index] == NULL))
	{
		return false;
	}
	
	W_PATH *p = w_paths[path_index];	// set the pointer to the head node
	W_PATH *p_next;

	int safety_stop = 0;

	while (p)
	{
		p_next = p->next;	// save the link to next
		p->prev = NULL;		// clear pointer on prev node
		free(p);			// free this node
		p = p_next;			// update the head node

		safety_stop++;
		if (safety_stop > LINKEDLIST_LOOPS_THRESHOLD)
			LinkedListError("Delete Whole Path", path_index);
	}

	w_paths[path_index] = NULL;

	return true;
}


/*
* scans all paths on the waypoint and returns appropriate value of the waypoint in terms of usefulness that fits best current bot behaviour, needs or tasks
*/
void WaypointSetValue(bot_t *pBot, int wpt_index, WAYPOINT_VALUE *wpt_value)
{
	if (wpt_index == NO_VAL)
		return;

	int chance = RANDOM_LONG(1, 100);
	int last_good = NO_VAL;

	for (int path_index = 0; path_index < num_w_paths; path_index++)
	{
		// skip free slots
		if (w_paths[path_index] == NULL)
			continue;

		// this waypoint is not on this path then skip it
		if (wptmanager.IsWaypointOnPath(wpt_index, path_index) == false)
			continue;

		// skip paths dedicated to opposite team
		if ((wptmanager.IsPath(path_index, PathT::team_one) && pBot->IsBotTeam(teamTWO.GetTeamId())) ||	(wptmanager.IsPath(path_index, PathT::team_two) && pBot->IsBotTeam(teamONE.GetTeamId())))
			continue;
		
		// skip blocked paths ... with the exception of a non one-way path where is a goal placed claymore waypoint and this bot carries needed explosives and decided to reach the goal of the map
		if (wptmanager.IsPath(path_index, PathT::roadblocked_tag) && (wptmanager.IsRoadblockedPathException(pBot, path_index) == false))
			continue;

		// skip the last visited path
		if (path_index == pBot->prev_path_index)
			continue;

		// skip one-way paths ending on this waypoint
		if (wptmanager.IsPath(path_index, PathT::one_way) && (wpt_index == wptmanager.GetPathEnd(path_index)))
			continue;

		// the order is based on importance

		if (wptmanager.IsPath(path_index, PathT::carry_goal_item) && pBot->IsTask(TASK_GOALITEM) &&	(chance < 95))
		{
			wpt_value->wpt_index = wpt_index;
			wpt_value->wpt_value = 95;

			return;
		}

		if (wptmanager.IsPath(path_index, PathT::goal_explosives_tag) && pBot->IsNeed(NEED_GOAL) && pBot->IsEquippedWithExplosiveCharge() &&
			wptmanager.IsPathWaypointTypeTeamPriority(path_index, WptT::claymore, 1, pBot->GetBotTeam()) &&	(chance < 95))
		{
			wpt_value->wpt_index = wpt_index;
			wpt_value->wpt_value = 90;

			return;
		}

		if (((wptmanager.IsPath(path_index, PathT::goal_team_one_tag) && pBot->IsBotTeam(teamONE.GetTeamId())) ||
			(wptmanager.IsPath(path_index, PathT::goal_team_two_tag) && pBot->IsBotTeam(teamTWO.GetTeamId()))) &&
			pBot->IsNeed(NEED_GOAL) && (chance < 95) && wptmanager.IsPushpointGoalOnPathReachableForThisBot(pBot, path_index))
		{
			wpt_value->wpt_index = wpt_index;
			
			if (internals.IsMapGoalBasedOnExplosives())
				wpt_value->wpt_value = 95;
			else
				wpt_value->wpt_value = 90;

			return;
		}

		if (wptmanager.IsPath(path_index, PathT::ammo_tag) && pBot->IsNeed(NEED_EXLOSIVESCHARGE) &&
			wptmanager.IsPathWaypointTypeTeamPriority(path_index, WptT::ammobox, NON_ZERO_WPT_PRIORITY, pBot->GetBotTeam()) && (chance < 90))
		{
			wpt_value->wpt_index = wpt_index;
			
			if (internals.IsMapGoalBasedOnExplosives())
				wpt_value->wpt_value = 91;
			else
				wpt_value->wpt_value = 85;

			return;
		}

		if (wptmanager.IsPath(path_index, PathT::bandages_tag) && pBot->IsNeed(NEED_BANDAGES) &&
			wptmanager.IsPathWaypointTypeTeamPriority(path_index, WptT::bandage, NON_ZERO_WPT_PRIORITY, pBot->GetBotTeam()) && (chance < 90))
		{
			wpt_value->wpt_index = wpt_index;
			wpt_value->wpt_value = 85;

			return;
		}

		if (wptmanager.IsPath(path_index, PathT::sniper_class) && pBot->IsBehaviour(SNIPER) && (chance < 75))
		{
			// if there already is a path that fits bots needs then use random chance to decide if we overwrite the last values
			if ((last_good == NO_VAL) || (RANDOM_LONG(1, 100) < 50))
			{
				wpt_value->wpt_index = wpt_index;
				wpt_value->wpt_value = 75;
				last_good = wpt_index;
			}
		}

		if (wptmanager.IsPath(path_index, PathT::mgunner_class) && pBot->IsBehaviour(MGUNNER) && (chance < 75))
		{
			if ((last_good == NO_VAL) || (RANDOM_LONG(1, 100) < 50))
			{
				wpt_value->wpt_index = wpt_index;
				wpt_value->wpt_value = 75;
				last_good = wpt_index;
			}
		}

		if (wptmanager.IsPath(path_index, PathT::antiarmor_class) && pBot->IsBehaviour(AASPEC) && (chance < 75))
		{
			if ((last_good == NO_VAL) || (RANDOM_LONG(1, 100) < 50))
			{
				wpt_value->wpt_index = wpt_index;
				wpt_value->wpt_value = 75;
				last_good = wpt_index;
			}
		}

		if (wptmanager.IsPath(path_index, PathT::patrol_cycle) && pBot->IsBehaviour(DEFENDER) && (chance < 50))
		{
			if ((last_good == NO_VAL) || (RANDOM_LONG(1, 100) < 25))
			{
				wpt_value->wpt_index = wpt_index;
				wpt_value->wpt_value = 50;
				last_good = wpt_index;
			}
		}
	}

	return;
}


/*
* returns ending waypoint index of current ladder
* if none is found return -1
*/
// CURRENTLY THIS ISN'T USED AT ALL (There's a call for this in the code, but that call cannot happen, because the condition isn't met)
int FindRightLadderWpt(bot_t* pBot)
{

	// TODO: This needs to be changed to work for both cases. 1) when the ladder is waypointed with the use of ladder type waypoint,
	//		 2) when the ladder is done using just normal waypoints.
	//		 Also it needs to check for waypoints that aren't just straight up or down, because some ladders aren't straight either.


	int i, curr_index;
	float x_curr, y_curr, z_curr;		// current or nearest ladder wpt (start destination)
	float x_end, y_end, z_end;			// end ladder wpt (end destination)

	bool have_it;
	TraceResult tr;

	if (num_waypoints < 1)
		return -1;

	have_it = FALSE;

	// find the nearest ladder wpt
	curr_index = wptmanager.FindNearestWaypointOfTypeToPlayer(pBot->pEdict, 50.0f, WptT::ladder);

	// is there any ladder wpt nearby
	if (curr_index != -1)
	{
		x_curr = waypoints[curr_index].origin.x;
		y_curr = waypoints[curr_index].origin.y;
		z_curr = waypoints[curr_index].origin.z;
	}
	else
	{
		x_curr = 0.0;
		y_curr = 0.0;
		z_curr = 0.0;
	}

	for (i = 0; i < num_waypoints; i++)
	{
		if (waypoints[i].flags & W_FL_DELETED)
			continue;

		// skip all non ladder wpts
		if ((waypoints[i].flags & W_FL_LADDER) == 0)
			continue;

		x_end = waypoints[i].origin.x;
		y_end = waypoints[i].origin.y;
		z_end = waypoints[i].origin.z;

		// skip the same
		if (z_end == z_curr)
			continue;

		UTIL_TraceLine(Vector(x_curr, y_curr, y_curr), Vector(x_end, y_end, z_end), ignore_monsters, pBot->pEdict, &tr);

		if (tr.flFraction >= 1.0)
		{
			have_it = TRUE;
			break;
		}
	}

	if (have_it)
	{
		//pBot->end_wpt_index = i;

		//@@@@@@@@@@
		//char ms[80];
		//sprintf(ms, "endwpt=%d\n", i+1);
		//ALERT(at_console, ms);

		return i;
	}

	//@@@@@@@
	//ALERT(at_console, "PROBLEM NO LADDER END\n");

	return -1;
}


/*
* move all waypoints (its origin) in given path by value units in given coordinate
* coord values are 1 for x coord, 2 for y coord and 3 for z coord (height)
*/
bool WaypointMoveWholePath(int path_index, float value, int coord)
{
#ifdef _DEBUG
	// validity check
	if ((path_index == -1) || (w_paths[path_index] == NULL))
		return FALSE;

	W_PATH *p = w_paths[path_index];

	while (p)
	{
		switch (coord)
		{
			case 1:
				waypoints[p->wpt_index].origin.x += value;
				break;
			case 2:
				waypoints[p->wpt_index].origin.y += value;
				break;
			case 3:
				waypoints[p->wpt_index].origin.z += value;
				break;
			default:
				return FALSE;
		}

		p = p->next;
	}
#endif

	return TRUE;
}


/*
*
* NOTE: NOT USED - PROBABLY USELESS
*
* load raw waypoint data
*/
/*
bool WaypointRawLoad(edict_t *pEntity, bool flags, bool priority, bool time, bool class_preference)
{
	extern bool is_dedicated_server;

	char mapname[64];
	char filename[256];
	RAW_FILE_HDR header;
	char msg[80];
	int index;

	strcpy(mapname, STRING(gpGlobals->mapname));
	strcat(mapname, ".raw");

	if (internals.IsCustomWaypoints())
		util.MarineBotFileName(filename, "customwpts", mapname);
	else
		util.MarineBotFileName(filename, "defaultwpts", mapname);

	if (is_dedicated_server)
		printf("loading raw file: %s\n", filename);

	FILE *bfp = fopen(filename, "rb");

	// if file exists, read the waypoint structure from it
	if (bfp != NULL)
	{
		fread(&header, sizeof(header), 1, bfp);

		header.filetype[7] = 0;
		if (strcmp(header.filetype, "FAM_bot") == 0)
		{
			if (header.waypoint_file_version != WAYPOINT_VERSION)
			{
				if (pEntity)
					ClientPrint(pEntity, HUD_PRINTNOTIFY,
					"Older MarineBot waypoint file version!\n");
			}

			header.mapname[31] = 0;

			if (strcmp(header.mapname, STRING(gpGlobals->mapname)) == 0)
			{
				wpteditor.InitAll();  // remove any existing waypoints

				for (index = 0; index < header.number_of_waypoints; index++)
				{
					// at first init all new values to default
					waypoints[index].red_priority = MAX_WPT_PRIOR;
					waypoints[index].red_time = 0.0;
					waypoints[index].blue_priority = MAX_WPT_PRIOR;
					waypoints[index].blue_time = 0.0;
					waypoints[index].class_pref = FA_CLASS_NONE;
					waypoints[index].range = WPT_RANGE;
					waypoints[index].misc = 0;

					// read waypoint data from .raw file
					fread(&waypoints[index].origin, sizeof(waypoints[0].origin), 1, bfp);

					if (flags)
						fread(&waypoints[index].flags, sizeof(waypoints[0].flags), 1, bfp);
					if (priority)
					{
						fread(&waypoints[index].red_priority, sizeof(waypoints[0].red_priority), 1, bfp);
						fread(&waypoints[index].blue_priority, sizeof(waypoints[0].blue_priority), 1, bfp);
					}
					if (time)
					{
						fread(&waypoints[index].red_time, sizeof(waypoints[0].red_time), 1, bfp);
						fread(&waypoints[index].blue_time, sizeof(waypoints[0].blue_time), 1, bfp);
					}
					if (class_preference)
						fread(&waypoints[index].class_pref, sizeof(waypoints[0].class_pref), 1, bfp);
					num_waypoints++;

					/*	Already init at top so keep this only for sure and after successful test delete this
					// init all new values to default
					if ((flags == FALSE) && (priority == FALSE) && (time == FALSE) &&
						(class_preference == FALSE))
					{
						waypoints[index].flags = W_FL_STD;
						waypoints[index].red_priority = MAX_WPT_PRIOR;
						waypoints[index].red_time = 0.0;
						waypoints[index].blue_priority = MAX_WPT_PRIOR;
						waypoints[index].blue_time = 0.0;
						waypoints[index].class_pref = FA_CLASS_NONE;
					}
					// init flags and priority
					if ((flags == FALSE) && (priority == FALSE))
					{
						waypoints[index].flags = W_FL_STD;
						waypoints[index].red_priority = MAX_WPT_PRIOR;
						waypoints[index].blue_priority = MAX_WPT_PRIOR;
					}
					// init flags and time
					if ((flags == FALSE) && (time == FALSE))
					{
						waypoints[index].flags = W_FL_STD;
						waypoints[index].red_time = 0.0;
						waypoints[index].blue_time = 0.0;
					}
					// init flags and class_preference
					if ((flags == FALSE) && (class_preference == FALSE))
					{
						waypoints[index].flags = W_FL_STD;
						waypoints[index].class_pref = FA_CLASS_NONE;
					}
					// init priority and time and class_preference
					if ((priority == FALSE) && (time == FALSE) && (class_preference == FALSE))
					{
						waypoints[index].red_priority = MAX_WPT_PRIOR;
						waypoints[index].red_time = 0.0;
						waypoints[index].blue_priority = MAX_WPT_PRIOR;
						waypoints[index].blue_time = 0.0;
						waypoints[index].class_pref = FA_CLASS_NONE;
					}
					// init priority and time
					if ((priority == FALSE) && (time == FALSE))
					{
						waypoints[index].red_priority = MAX_WPT_PRIOR;
						waypoints[index].red_time = 0.0;
						waypoints[index].blue_priority = MAX_WPT_PRIOR;
						waypoints[index].blue_time = 0.0;
					}
					// init priority and class_preference
					if ((priority == FALSE) && (class_preference == FALSE))
					{
						waypoints[index].red_priority = MAX_WPT_PRIOR;
						waypoints[index].blue_priority = MAX_WPT_PRIOR;
						waypoints[index].class_pref = FA_CLASS_NONE;
					}
					// init time and class_preference
					if ((time == FALSE) && (class_preference))
					{
						waypoints[index].red_time = 0.0;
						waypoints[index].blue_time = 0.0;
						waypoints[index].class_pref = FA_CLASS_NONE;
					}
					// init only flags
					if (flags == FALSE)
					{
						waypoints[index].flags = W_FL_STD;
					}
					// init only priority
					if (priority == FALSE)
					{
						waypoints[index].red_priority = MAX_WPT_PRIOR;
						waypoints[index].blue_priority = MAX_WPT_PRIOR;
					}
					// init only time
					if (time == FALSE)
					{
						waypoints[index].red_time = 0.0;
						waypoints[index].blue_time = 0.0;
					}
					// init only class_preference
					if (class_preference == FALSE)
					{
						waypoints[index].class_pref = FA_CLASS_NONE;
					}*//*
				}

				header.author[31] = 0;
				// if no author so set it to unknown
				if (strcmp(header.author, "") == 0)
					strcpy(wpt_author, "unknown");
				else
					strcpy(wpt_author, header.author);

				header.modified_by[31] = 0;
				// if not specified guy who modify wpts so set it to unknown
				if (strcmp(header.modified_by, "") == 0)
					strcpy(wpt_modified, "unknown");
				else
					strcpy(wpt_modified, header.modified_by);
			}
			else
			{
				if (pEntity)
				{
					sprintf(msg, "%s MarineBot waypoints are not for this map!\n", filename);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
				}

				fclose(bfp);
				return FALSE;
			}
/*
			header.author[31] = 0;
			// if no author so set it to unknown
			if (strcmp(header.author, "") == 0)
				strcpy(wpt_author, "unknown");
			else
				strcpy(wpt_author, header.author);

			header.modified_by[31] = 0;
			// if not specified guy who modify wpts so set it to unknown
			if (strcmp(header.modified_by, "") == 0)
				strcpy(wpt_modified, "unknown");
			else
				strcpy(wpt_modified, header.modified_by);*//*
		}
		else
		{
			if (pEntity)
			{
				sprintf(msg, "%s is not a MarineBot waypoint file!\n", filename);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}

			fclose(bfp);
			return FALSE;
		}

		fclose(bfp);
	}
	else
	{
		if (pEntity)
		{
			sprintf(msg, "Waypoint file %s does not exist!\n", filename);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
		}

		if (is_dedicated_server)
			printf("waypoint file %s not found!\n", filename);

		return FALSE;
	}

	return TRUE;
}
*/

/*
*
* NOTE: NOT USED - PROBABLY USELESS
*
* save raw waypoint data
*/
/*
void WaypointRawSave(bool flags, bool priority, bool time, bool class_preference)
{
	char filename[256];
	char mapname[64];
	RAW_FILE_HDR header;
	int index;

	strcpy(header.filetype, "FAM_bot");

	header.waypoint_file_version = WAYPOINT_VERSION;

	header.transfer_flags = (int)flags;

	header.transfer_priority = (int)priority;

	header.transfer_time = (int)time;

	header.transfer_class = (int)class_preference;	

	header.number_of_waypoints = num_waypoints;

	memset(header.mapname, 0, sizeof(header.mapname));
	strncpy(header.mapname, STRING(gpGlobals->mapname), 31);
	header.mapname[31] = 0;

	// write authors signature
	memset(header.author, 0, sizeof(header.author));
	if (wpt_author[0] == 0)
		strncpy(header.author, "unknown", 31);
	else
		strncpy(header.author, wpt_author, 31);
	header.author[31] = 0;

	// write the one who modified them
	memset(header.modified_by, 0, sizeof(header.modified_by));
	if (wpt_modified[0] == 0)
		strncpy(header.modified_by, "unknown", 31);
	else
		strncpy(header.modified_by, wpt_modified, 31);
	header.modified_by[31] = 0;

	strcpy(mapname, STRING(gpGlobals->mapname));
	strcat(mapname, ".raw");

	if (internals.IsCustomWaypoints())
		util.MarineBotFileName(filename, "customwpts", mapname);
	else
		util.MarineBotFileName(filename, "defaultwpts", mapname);

	FILE *bfp = fopen(filename, "wb");

	// write the waypoint header to the file
	fwrite(&header, sizeof(header), 1, bfp);

	// write the waypoint data to the file...
	for (index=0; index < num_waypoints; index++)
	{
		fwrite(&waypoints[index].origin, sizeof(waypoints[0].origin), 1, bfp);
		if (flags)
			fwrite(&waypoints[index].flags, sizeof(waypoints[0].flags), 1, bfp);
		if (priority)
		{
			fwrite(&waypoints[index].red_priority, sizeof(waypoints[0].red_priority), 1, bfp);
			fwrite(&waypoints[index].blue_priority, sizeof(waypoints[0].blue_priority), 1, bfp);
		}
		if (time)
		{
			fwrite(&waypoints[index].red_time, sizeof(waypoints[0].red_time), 1, bfp);
			fwrite(&waypoints[index].blue_time, sizeof(waypoints[0].blue_time), 1, bfp);
		}
		if (class_preference)
			fwrite(&waypoints[index].class_pref, sizeof(waypoints[0].class_pref), 1, bfp);
	}
	
	fclose(bfp);
}
*/



/*
* checks if waypoint is reachable
*/
bool WaypointReachable(Vector v_src, Vector v_dest, edict_t *pEntity)
{
   TraceResult tr;
   float curr_height, last_height;

   float distance = (v_dest - v_src).Length();

   // is the destination close enough?
   if (distance < MAX_WPT_DIST)
   {
      // check if this waypoint is "visible"...

      UTIL_TraceLine( v_src, v_dest, ignore_monsters, pEntity->v.pContainingEntity, &tr );

      // if waypoint is visible from current position (even behind head)...
      if (tr.flFraction >= 1.0f)
      {
         // check for special case of both waypoints being underwater...
         if ((POINT_CONTENTS( v_src ) == CONTENTS_WATER) && (POINT_CONTENTS( v_dest ) == CONTENTS_WATER))
         {
            return true;
         }

         // check for special case of waypoint being suspended in mid-air...

         // is dest waypoint higher than src? (45 is max jump height)
         if (v_dest.z > (v_src.z + 45.0f))
         {
            Vector v_new_src = v_dest;
            Vector v_new_dest = v_dest;

            v_new_dest.z = v_new_dest.z - 50.0f;  // straight down 50 units

            UTIL_TraceLine(v_new_src, v_new_dest, dont_ignore_monsters, pEntity->v.pContainingEntity, &tr);

            // check if we didn't hit anything, if not then it's in mid-air
            if (tr.flFraction >= 1.0f)
            {
               return false;  // can't reach this one
            }
         }

         // check if distance to ground increases more than jump height
         // at points between source and destination...

         Vector v_direction = (v_dest - v_src).Normalize();  // 1 unit long
         Vector v_check = v_src;
         Vector v_down = v_src;

         v_down.z = v_down.z - 1000.0f;  // straight down 1000 units

         UTIL_TraceLine(v_check, v_down, ignore_monsters, pEntity->v.pContainingEntity, &tr);

         last_height = tr.flFraction * 1000.0f;  // height from ground

         distance = (v_dest - v_check).Length();  // distance from goal

         while (distance > 10.0f)
         {
            // move 10 units closer to the goal...
            v_check = v_check + (v_direction * 10.0f);

            v_down = v_check;
            v_down.z = v_down.z - 1000.0f;  // straight down 1000 units

            UTIL_TraceLine(v_check, v_down, ignore_monsters, pEntity->v.pContainingEntity, &tr);

            curr_height = tr.flFraction * 1000.0f;  // height from ground

            // is the difference in the last height and the current height
            // higher that the jump height?
            if ((last_height - curr_height) > 45.0f)
            {
               // can't get there from here...
               return false;
            }

            last_height = curr_height;

            distance = (v_dest - v_check).Length();  // distance from goal
         }

         return true;
      }
   }

   return false;
}


/*
* sets correct size/height of the beam based on waypoint type
* keep this order to ensure that the waypoint will be shown correctly
* bandages flag/tag doesn't alter waypoint appearance in either way
*/
void SetWaypointSize(int wpt_index, Vector &start, Vector &end)
{
	if (waypoints[wpt_index].flags & (W_FL_CROSS | W_FL_AIMING))
	{
		start = waypoints[wpt_index].origin;
		end = start + Vector(0, 0, 34);
		
		return;
	}
	
	if (waypoints[wpt_index].flags & W_FL_STD)
	{
		start = waypoints[wpt_index].origin - Vector(0, 0, 34);
		end = start + Vector(0, 0, 68);
	}
	// if this is "shoot" waypoint override the setting to prevent confusion when user sees bot shooting while standing at normal waypoint
	if (waypoints[wpt_index].flags & (W_FL_FIRE | W_FL_SPRINT))
	{
		start = waypoints[wpt_index].origin;
		end = start + Vector(0, 0, 34);
	}
	if (waypoints[wpt_index].flags & W_FL_SNIPER)
	{
		start = waypoints[wpt_index].origin - Vector(0, 0, 17);
		end = start + Vector(0, 0, 34);
	}
	if (waypoints[wpt_index].flags & W_FL_GOBACK)
	{
		start = waypoints[wpt_index].origin - Vector(0, 0, 34);
		end = start + Vector(0, 0, 68);
	}
	if (waypoints[wpt_index].flags & W_FL_CROUCH)
	{
		start = waypoints[wpt_index].origin - Vector(0, 0, 17);
		end = start + Vector(0, 0, 34);
	}
	else if (waypoints[wpt_index].flags & W_FL_PRONE)
	{
		start = waypoints[wpt_index].origin - Vector(0, 0, 17);
		end = start + Vector(0, 0, 34);
	}
	if (waypoints[wpt_index].flags & W_FL_AMMOBOX)
	{
		start = waypoints[wpt_index].origin;
		end = start + Vector(0, 0, 34);
	}
	else if (waypoints[wpt_index].flags & (W_FL_DOOR | W_FL_DOORUSE))
	{
		start = waypoints[wpt_index].origin - Vector(0, 0, 34);
		end = start + Vector(0, 0, 68);
	}
	if (waypoints[wpt_index].flags & W_FL_MINE)
	{
		start = waypoints[wpt_index].origin - Vector(0, 0, 17);
		end = start + Vector(0, 0, 34);
	}
	else if (waypoints[wpt_index].flags & W_FL_USE)
	{
		start = waypoints[wpt_index].origin - Vector(0, 0, 34);
		end = start + Vector(0, 0, 68);
	}
	if (waypoints[wpt_index].flags & W_FL_JUMP)
	{
		start = waypoints[wpt_index].origin;
		end = start + Vector(0, 0, 34);
	}
	else if (waypoints[wpt_index].flags & W_FL_DUCKJUMP)
	{
		start = waypoints[wpt_index].origin - Vector(0, 0, 34);
		end = start + Vector(0, 0, 68);
	}
	if (waypoints[wpt_index].flags & (W_FL_PUSHPOINT | W_FL_TRIGGER | W_FL_ROADBLOCK))
	{
		start = waypoints[wpt_index].origin - Vector(0, 0, 34);
		end = start + Vector(0, 0, 68);
	}
	if (waypoints[wpt_index].flags & W_FL_LADDER)
	{
		start = waypoints[wpt_index].origin;
		end = start + Vector(0, 0, 34);
	}
	if (waypoints[wpt_index].flags & W_FL_CHUTE)
	{
		start = waypoints[wpt_index].origin - Vector(0, 0, 17);
		end = start + Vector(0, 0, 34);
	}
}

/*
* sets waypoint color based on waypoint type, the color is returned as vector of RGB values
* we have to keep this order to show correct color due to the fact that there can be more flag/tag bits on the waypoint
* bandages flag/tag doesn't alter waypoint appearance in either way
*/
Vector SetWaypointColor(int wpt_index)
{
	// init waypoint color values (white isn't used for any waypoint so we can easily see if there is any problem)
	Vector color_code_rgb = Vector(255, 255, 255);

	// the shades aren't always accurate to color names (at least not according to HTML color table)
	Vector color_orange = Vector(255, 128, 0);		// more like dark orange
	Vector color_yellow = Vector(255, 255, 0);
	Vector color_red = Vector(255, 0, 0);
	Vector color_cyan = Vector(0, 255, 255);
	Vector color_green = Vector(0, 255, 0);			// this shade is actually called lime
	Vector color_magenta = Vector(255, 0, 255);
	Vector color_blue = Vector(0, 0, 255);
	Vector color_purple = Vector(128, 0, 255);		// more like dark violet
	Vector color_turquoise = Vector(175, 238, 238);		// actually pale turquoise
	Vector color_slate = Vector(128, 128, 255);		// light bluish purple											TODO: this one is prepared for cover wpt and/or teammate wpt

	if (waypoints[wpt_index].flags & (W_FL_STD | W_FL_SPRINT)) { color_code_rgb = color_orange; }
	if (waypoints[wpt_index].flags & W_FL_FIRE) { color_code_rgb = color_yellow; }
	if (waypoints[wpt_index].flags & W_FL_SNIPER) { color_code_rgb = color_red; }
	if (waypoints[wpt_index].flags & W_FL_GOBACK) { color_code_rgb = color_cyan; }
	if (waypoints[wpt_index].flags & W_FL_CROUCH) { color_code_rgb = color_orange; }
	if (waypoints[wpt_index].flags & (W_FL_JUMP | W_FL_DUCKJUMP | W_FL_PRONE)) { color_code_rgb = color_green; }
	if (waypoints[wpt_index].flags & W_FL_LADDER) { color_code_rgb = color_cyan; }
	if (waypoints[wpt_index].flags & (W_FL_AIMING | W_FL_MINE | W_FL_DOOR | W_FL_DOORUSE)) { color_code_rgb = color_magenta; }
	if (waypoints[wpt_index].flags & W_FL_CHUTE) { color_code_rgb = color_yellow; }
	if (waypoints[wpt_index].flags & W_FL_CROSS) { color_code_rgb = color_blue; }
	if (waypoints[wpt_index].flags & (W_FL_AMMOBOX | W_FL_USE)) { color_code_rgb = color_red; }
	if (waypoints[wpt_index].flags & W_FL_PUSHPOINT) { color_code_rgb = color_yellow; }
	if (waypoints[wpt_index].flags & W_FL_ROADBLOCK) { color_code_rgb = color_turquoise; }
	if (waypoints[wpt_index].flags & W_FL_TRIGGER) { color_code_rgb = color_purple; }

	return color_code_rgb;
}


/*
* sets path texture based on its direction
* the texture is returned as an int variable that has been precached in DispatchSpawn() in dll.cpp
*/
int SetPathTexture(int path_index)
{
	// default path sprite
	int sprite = m_spriteTexturePath2;

	// use different sprite for one-way paths, this should make checking path direction a little easier
	if (w_paths[path_index]->flags & P_FL_WAY_ONE)
		sprite = m_spriteTexturePath1;

	// use different sprite for paths with additional flags
	if (w_paths[path_index]->flags & (P_FL_MISC_AVOID | P_FL_MISC_IGNORE | P_FL_MISC_GITEM))
		sprite = m_spriteTexturePath3;

	return sprite;
}


/*
* sets path color based on its type, the color is returned as vector of RGB values
* we have to keep this order to show correct color due to the fact that there are more flag/tag bits on the path
*/
Vector SetPathColor(int path_index)
{
	Vector color_code_rgb = Vector(255, 255, 255);

	// both team - purple color
	if (w_paths[path_index]->flags & P_FL_TEAM_NO) { color_code_rgb = Vector(128, 0, 255); }
	// red team - red color
	else if (w_paths[path_index]->flags & P_FL_TEAM_RED) { color_code_rgb = teamONE.GetTeamPathColor(); }
	// blue team - blue color
	else if (w_paths[path_index]->flags & P_FL_TEAM_BLUE) { color_code_rgb = teamTWO.GetTeamPathColor(); }
	
	// sniper path - light green color
	if (w_paths[path_index]->flags & P_FL_CLASS_SNIPER) { color_code_rgb = Vector(128, 255, 128); }
	// mgunner path - dark green color
	else if (w_paths[path_index]->flags & P_FL_CLASS_MGUNNER) { color_code_rgb = Vector(0, 128, 0); }
	// anti-armor specialist path - green color with bluish tint
	else if (w_paths[path_index]->flags & P_FL_CLASS_ANTIARMOR) { color_code_rgb = Vector(0, 255, 128); }

	// patrol path - yellow color
	if (w_paths[path_index]->flags & P_FL_WAY_PATROL) { color_code_rgb = Vector(255, 255, 0); }

	return color_code_rgb;
}


display_path_beams_t::display_path_beams_t()
{
	start = new display_beam_t;

	/*/
#ifdef DEBUG
	FILE* f = fopen("!debug1.txt", "a");
	fprintf(f, "consctructor\n");
	fclose(f);
#endif // DEBUG
	/**/
}


display_path_beams_t::~display_path_beams_t()
{
	display_beam_t* cur;
	display_beam_t* tmp;

	cur = start;

	while (cur)
	{
		tmp = cur;
		cur = cur->next;
		delete tmp;
	}

	delete cur;

	start = NULL;

	/*/
#ifdef DEBUG
	FILE* f = fopen("!debug1.txt", "a");
	fprintf(f, "desctructor\n");
	fclose(f);
#endif // DEBUG
	/**/
}


void display_path_beams_t::AddNewBeam(int beam_start_point, int beam_end_point, int beam_sprite, Vector beam_color)
{
	display_beam_t* beam = start;
	display_beam_t* new_beam;

	// go through existing beams...
	while (beam)
	{
		// till we get to the end of the list...
		if (beam->start_point == NO_VAL)
		{
			// in order to store data and...
			beam->start_point = beam_start_point;
			beam->end_point = beam_end_point;
			beam->sprite = beam_sprite;
			beam->color = beam_color;

			// also create a new node
			new_beam = new display_beam_t;

			// and link the new node to the end of the list
			new_beam->prev = beam;
			beam->next = new_beam;

			break;
		}
		else
		{
			beam = beam->next;
		}
	}

	return;
}


int display_path_beams_t::GetMatchingBeamsCount(int beam_start_point, int beam_end_point, int beam_sprite, Vector beam_color)
{
	display_beam_t* beam = start;
	int the_count = 0;

	// go through existing beams...
	while (beam)
	{
		// and count the beams that start and end at given waypoints and also use given sprite and given color
		if ((beam->start_point == beam_start_point) && (beam->end_point == beam_end_point) && (beam->sprite == beam_sprite) && (beam->color == beam_color))
			the_count++;

		beam = beam->next;
	}

	return the_count;
}


bool display_path_beams_t::CanBeDisplayed(int beam_start_point, int beam_end_point, int beam_sprite, Vector beam_color)
{
	// don't draw more than defined limit of the same path beams between the same two waypoints in either direction
	if (GetMatchingBeamsCount(beam_start_point, beam_end_point, beam_sprite, beam_color) + GetMatchingBeamsCount(beam_end_point, beam_start_point, beam_sprite, beam_color) > max_shown_beams)
		return false;

	return true;
}


void display_path_beams_t::DrawIt(edict_t* pEdict, Vector beam_start_point, Vector beam_end_point, int beam_sprite, Vector beam_color, float duration)
{
	if (pEdict == NULL)
		return;

	int beam_life = duration * 10;

	DrawTheBeam(pEdict, beam_start_point, beam_end_point, beam_sprite, beam_life, 20, 2, beam_color.x, beam_color.y, beam_color.z, 200, 10);
}


void WaypointBeam(edict_t* pEntity, Vector start, Vector end, int width, int noise, Vector color, int brightness, int speed, float duration)
{
	if (pEntity == NULL)
		return;

	int beam_life = duration * 10;

	DrawTheBeam(pEntity, start, end, m_spriteTexture, beam_life, width, noise, color.x, color.y, color.z, brightness, speed);
}


/*/
void PathBeam(edict_t *pEntity, Vector start, Vector end, int sprite, int width, int noise, int red, int green, int blue, int brightness, int speed)
{
	if (pEntity == NULL)
		return;

	DrawTheBeam(pEntity, start, end, sprite, width, noise, red, green, blue, brightness, speed);
}
/**/


void DrawTheBeam(edict_t* pEntity, Vector start, Vector end, int sprite, int life, int width, int noise, int red, int green, int blue, int brightness, int speed)
{
	// just for sure
	if ((pEntity == NULL) || (util.GetBotIndex(pEntity) > -1))
		return;

	MESSAGE_BEGIN(MSG_ONE, SVC_TEMPENTITY, NULL, pEntity);
	WRITE_BYTE(TE_BEAMPOINTS);
	WRITE_COORD(start.x);
	WRITE_COORD(start.y);
	WRITE_COORD(start.z);
	WRITE_COORD(end.x);
	WRITE_COORD(end.y);
	WRITE_COORD(end.z);
	WRITE_SHORT(sprite);	// a texture the bream uses
	WRITE_BYTE(1); // framestart
	WRITE_BYTE(10); // framerate
	WRITE_BYTE(life); // life in 0.1's
	WRITE_BYTE(width); // width
	WRITE_BYTE(noise);  // noise

	WRITE_BYTE(red);   // r, g, b
	WRITE_BYTE(green);   // r, g, b
	WRITE_BYTE(blue);   // r, g, b

	WRITE_BYTE(brightness);   // brightness
	WRITE_BYTE(speed);    // speed
	MESSAGE_END();
}

// used in various debugging tools
void DrawBeam(edict_t* pEntity, Vector start, Vector end, int life, int red, int green, int blue, int speed)
{
	if ((pEntity == NULL) || (util.GetBotIndex(pEntity) > -1))
		return;

	MESSAGE_BEGIN(MSG_ONE, SVC_TEMPENTITY, NULL, pEntity);
	WRITE_BYTE(TE_BEAMPOINTS);
	WRITE_COORD(start.x);
	WRITE_COORD(start.y);
	WRITE_COORD(start.z);
	WRITE_COORD(end.x);
	WRITE_COORD(end.y);
	WRITE_COORD(end.z);
	WRITE_SHORT(m_spriteTexture);
	WRITE_BYTE(1); // framestart
	WRITE_BYTE(10); // framerate
	WRITE_BYTE(life); // life in 0.1's
	WRITE_BYTE(10); // width
	WRITE_BYTE(0);  // noise

	WRITE_BYTE(red);   // r, g, b
	WRITE_BYTE(green);   // r, g, b
	WRITE_BYTE(blue);   // r, g, b

	WRITE_BYTE(250);   // brightness
	WRITE_BYTE(speed);    // speed
	MESSAGE_END();
}


/*
* handles real-time waypoints data updating upon reaching map objectives
*/
void UpdateWaypointData(void)
{
	for (int path_index = 0; path_index < num_w_paths; path_index++)
		wptfixer.UpdatePathStatus(path_index);
}


/*
* handles all waypoint and path drawing (redrawing)
* also handles autowaypoint and autoadding to path features and some special show features like waypoint connections to a cross, path highlighting etc.
*/
void WaypointThink(edict_t *pEntity)
{
	float distance, min_distance;
	Vector start, end;
	int i;

#ifdef DEBUG
	// in this function we have to temporary disable the feature to display Tracelines, because we do use them a lot here so we would overload the engine and end up on title screen loosing all current data
	devTool.SetOverrideDisplayTL();
#endif // DEBUG

	// is auto waypoint on
	if (wptser.IsAutoWaypointing())
	{
		// find the distance from the last used waypoint
		distance = (last_waypoint - pEntity->v.origin).Length();

		// is this waypoint far enough to add new wpt
		if (distance > wptser.GetAutoWaypointingDistance())
		{
			min_distance = 9999.0f;

			// check that no other reachable waypoints are nearby
			for (i = 0; i < num_waypoints; i++)
			{
				// skip these waypoints
				if (wptmanager.IsWaypoint(i, WptT::aim, WptT::deleted))
					continue;

				if (WaypointReachable(pEntity->v.origin, waypoints[i].origin, pEntity))
				{
					distance = (waypoints[i].origin - pEntity->v.origin).Length();

					// if the distance to this cross waypoint is at least half of current auto waypointing distance then ignore this cross waypoint
					if (wptmanager.IsWaypoint(i, WptT::cross) && (distance > (wptser.GetAutoWaypointingDistance() / 2.0f)))
						continue;

					if (distance < min_distance)
						min_distance = distance;
				}
			}

			// make sure nearest waypoint is far enough away
			if (min_distance >= wptser.GetAutoWaypointingDistance())
			{
				// is the edict NOT a bot
				if (!(pEntity->v.flags & FL_FAKECLIENT))
				{
					// is the edict in crouch so place crouch waypoint (DoD sets ducking flag even when player lays prone so we must check for that case)
					if ((pEntity->v.flags & FL_DUCKING) && (util.IsEdictProne(pEntity) == false))
						wpteditor.Add(pEntity, "crouch");
					// otherwise place normal/standing
					else
						wpteditor.Add(pEntity, "normal");
				}
			}
		}
	}

	// is path autoadding on
	if (wptser.IsAutoAddToPath())
	{
		// is the edict NOT a bot
		if (!(pEntity->v.flags & FL_FAKECLIENT))
		{
			// search through all waypoints
			for (i = 0; i < num_waypoints; i++)
			{
				// continue current path function can return even -1, but if the 'not enough memory' error happens then things wouldn't work at all so this is a safe value
				int result = -1;

				distance = (waypoints[i].origin - pEntity->v.origin).Length();

				// if player "touch" the waypoint
				if (distance <= AUTOADD_DISTANCE)
				{
					// add the waypoint to actual path
					result = ContinueCurrPath(i);
				}

				// if waypoint was added successfully play snd_done
				if (result == 1)
				{
					// however sound confirmation function isn't known here so we have to do it manually
					EMIT_SOUND_DYN2(pEntity, CHAN_WEAPON, "plats/elevbell1.wav", 1.0, ATTN_NORM, 0, 100);
				}
				// if we are autowaypoing and this is the first waypoint that should be added to a path...
				else if (wptser.IsAutoWaypointing() && (result == -2))
				{
					// then we must start a new path
					if (StartNewPath(i))
					{
						EMIT_SOUND_DYN2(pEntity, CHAN_WEAPON, "plats/elevbell1.wav", 1.0, ATTN_NORM, 0, 100);
					}
				}
			}
		}
	}

	// display the waypoints if turned on
	if (wptser.IsShowWaypoints())
	{
		bool can_display_it;
		float the_range;
		Vector color;
		TraceResult tr;

		// is it time to refresh waypoints?
		if ((wp_display_time + wptser.GetWaypointsDisplayTime()) < gpGlobals->time)
		{
			// go through all waypoints
			for (i = 0; i < num_waypoints; i++)
			{
				// skip all deleted waypoints
				if (wptmanager.IsWaypoint(i, WptT::deleted))
					continue;

				can_display_it = true;

				// get the distance to this waypoint
				distance = (waypoints[i].origin - pEntity->v.origin).Length();

				// is this waypoint close enough AND is it visible from current location?
				if ((distance < wptser.GetWaypointsDrawDistance()) && util.IsWaypointVisible(waypoints[i].origin, pEntity))
				{
					// is a specific (by its index) path highlighting feature activated AND this waypoint is NOT on it?
					if (wptser.IsPathToHighlightAPathIndex() && (wptmanager.IsWaypointOnPath(i, wptser.GetPathToHighlight()) == false))
					{
						// then do NOT draw it (ie. display only waypoints that are on the path that the user wants to see)
						can_display_it = false;
					}

					if (can_display_it)
					{
						// get size/hight of the beam based on waypoint type
						SetWaypointSize(i, start, end);

						// use temp vector to set color for this waypoint
						color = SetWaypointColor(i);

						// draw the waypoint
						WaypointBeam(pEntity, start, end, 30, 0, color, 250, 5, wptser.GetWaypointsDisplayTime());
					}

					// now handle the additional features...

					// can we display the waypoint range?
					if (wptser.IsCheckRanges())
					{
						// skip all deleted, aim, cross, ladder and door waypoints (some aren't used for navigation and others don't use range setting)
						if (wptmanager.IsWaypoint(i, WptT::deleted, WptT::aim, WptT::cross) || wptmanager.IsNoRangeWaypoint(i))
							can_display_it = false;

						if (can_display_it && (distance <= WPT_CROSS_RANGE))
						{
							// get the range for this waypoint
							the_range = waypoints[i].range;
							color = Vector(250, 250, 250);

							// draw x-coord white beam
							start = waypoints[i].origin - Vector(the_range, 0, 0);
							end = waypoints[i].origin + Vector(the_range, 0, 0);
							WaypointBeam(pEntity, start, end, 10, 2, color, 200, 10, wptser.GetWaypointsDisplayTime());

							// draw y-coord white beam
							start = waypoints[i].origin - Vector(0, the_range, 0);
							end = waypoints[i].origin + Vector(0, the_range, 0);
							WaypointBeam(pEntity, start, end, 10, 2, color, 200, 10, wptser.GetWaypointsDisplayTime());
						}
					}

					// can we display connections between waypoint with wait time and its aim waypoints?
					if (wptser.IsCheckAims())
					{
						// process only the aim waypoints
						if (wptmanager.IsWaypoint(i, WptT::aim) && can_display_it)
						{
							// check all waypoints again for any possible "waittimed" waypoint
							for (int in_range = 0; in_range < num_waypoints; in_range++)
							{
								// skip current aim waypoint and also any other aim or cross waypoint
								if ((in_range == i) || wptmanager.IsWaypoint(in_range, WptT::aim, WptT::cross, WptT::deleted))
									continue;

								// draw the connections only for waypoints with time tag on and also for all shoot waypoints
								if ((waypoints[in_range].red_time != 0.0f) || (waypoints[in_range].blue_time != 0.0f) || wptmanager.IsWaypoint(in_range, WptT::shoot))
								{
									// is this waypoint "in radius" of the current aim waypoint (ie is the aim waypoint in radius of 100 units around this waypoint)?
									if (wptmanager.GetDistanceBetweenWaypoints(in_range, i) <= MAX_AIM_WPT_DIST)
									{
										start = waypoints[i].origin;
										end = waypoints[in_range].origin;

										// check if the waypoint isn't blocked
										UTIL_TraceLine(start, end, ignore_monsters, ignore_glass, NULL, &tr);

										// did the Traceline reach it?
										if (tr.flFraction >= 1.0f)
										{
											// then draw the connection ie. a pink/magenta beam
											WaypointBeam(pEntity, start, end, 10, 2, Vector(255, 128, 255), 200, 10, wptser.GetWaypointsDisplayTime());
										}
									}
								}
							}
						}
					}

					// can we display connections between a cross waypoint and all waypoints inside its range?
					if (wptser.IsCheckCross())
					{
						// process only the cross waypoints
						if (wptmanager.IsWaypoint(i, WptT::cross) && can_display_it)
						{
							// check all waypoints again
							for (int in_range = 0; in_range < num_waypoints; in_range++)
							{
								// skip current cross waypoint and also any other cross or aim waypoint
								if ((in_range == i) || wptmanager.IsWaypoint(in_range, WptT::aim, WptT::cross, WptT::deleted))
									continue;

								// is this waypoint in range of current cross waypoint?
								if (wptmanager.GetDistanceBetweenWaypoints(in_range, i) <= waypoints[i].range)
								{
									start = waypoints[i].origin;
									end = waypoints[in_range].origin;

									// check if the waypoint isn't blocked
									UTIL_TraceLine(start, end, ignore_monsters, NULL, &tr);

									// did the Traceline reach it?
									if (tr.flFraction >= 1.0f)
									{
										// is it a waypoint with zero (no) priority OR a trigger waypoint with all 4 priorities set to zero?
										if (((wptmanager.IsWaypoint(in_range, WptT::trigger) == false) && (waypoints[in_range].red_priority == 0) && (waypoints[in_range].blue_priority == 0)) ||
											(wptmanager.IsWaypoint(in_range, WptT::trigger) && (waypoints[in_range].red_priority == 0) && (waypoints[in_range].blue_priority == 0) &&
												(waypoints[in_range].trigger_red_priority == 0) && (waypoints[in_range].trigger_blue_priority == 0)))
										{
											// those are ignored in the decision making code so draw red connection to it to make it clear at the first sight
											color = Vector(255, 0, 0);
										}
										else
										{
											// draw standard cyan colored connection
											color = Vector(0, 255, 255);
										}

										// draw the beam
										WaypointBeam(pEntity, start, end, 10, 2, color, 200, 10, wptser.GetWaypointsDisplayTime());
									}
								}
							}
						}
					}

					// can we display connections between a shoot waypoint and all or one particular breakable object/s within its reach?
					if (wptser.IsCheckShoot())
					{
						// process only the shoot waypoints
						if (wptmanager.IsWaypoint(i, WptT::shoot) && can_display_it)
						{
							int aim_wpt_index = wptmanager.FindAimingAround(i);

							// added vector simulates player view offset (ie. where are his eyes) in order to make the Tracelines return correct results like if the bot was really there
							start = waypoints[i].origin + Vector(0, 0, 22);
							
							if (aim_wpt_index != NO_VAL)
							{
								if ((waypoints[aim_wpt_index].red_priority == 1) || (waypoints[aim_wpt_index].blue_priority == 1))
									end = start + (waypoints[aim_wpt_index].origin - start).Normalize() * (EXTENDED_SEARCH_RADIUS * 2.0f);
								else
								{
									end = start + (waypoints[aim_wpt_index].origin - start).Normalize() * EXTENDED_SEARCH_RADIUS;
									// in this case bot aims straight forward ie. ignores the z-coord of the aim waypoint so the connection must do the same
									end.z = start.z;
								}

								// let's test whether the aim waypoint points to a breakable object so that we can give the user feedback in form of colored connection where...
								UTIL_TraceLine(start, end, dont_ignore_monsters, pEntity, &tr);

								// if the Traceline reached breakable object then we will use light green for the connection
								if (util.IsEntityName(tr.pHit, "func_breakable") && (util.NotBreakableByGunfire(tr.pHit) == false))
									color = Vector(50, 255, 50);
								// otherwise we'll use the color of standard aim waypoint connection ... like that the bot looked there and found nothing
								else
									color = Vector(255, 128, 255);

								WaypointBeam(pEntity, start, end, 10, 2, color, 200, 10, wptser.GetWaypointsDisplayTime());
							}
							// no aim waypoint? then scan the surroundings
							else
							{
								edict_t* pent = NULL;
								
								while ((pent = util.FindEntityInSphere(pent, start, EXTENDED_SEARCH_RADIUS)) != NULL)
								{
									if ((util.IsEntityName(pent, "func_breakable") == false) || (pent->v.spawnflags & (SF_BREAK_TRIGGER_ONLY | SF_BREAK_OBJECT_CAP_ONLY)))
										continue;

									end = util.VecBModelOrigin(pent);

									UTIL_TraceLine(start, end, dont_ignore_monsters, pEntity, &tr);

									if (util.IsEntityName(tr.pHit, "func_breakable") || (tr.flFraction == 1.0f))
									{
										if (util.NotBreakableByGunfire(pent))
											color = Vector(255, 128, 255);
										else
											color = Vector(50, 255, 50);

										WaypointBeam(pEntity, start, end, 10, 2, color, 200, 10, wptser.GetWaypointsDisplayTime());
									}
								}
							}
						}
					}
				}
			}

			wp_display_time = gpGlobals->time;
		}
	}

	// display pathbeams (ie connections between waypoints) if turned on
	if (wptser.IsShowPaths())
	{
		int path_index;

		// is it time to refresh paths?
		if ((f_path_time + wptser.GetPathsDisplayTime()) < gpGlobals->time)
		{
			display_path_beams_t DisplayPathBeams;

			// go through all paths
			for (path_index = 0; path_index < num_w_paths; path_index++)
			{
				// if the user wanted to see just one specific path then we must show only that path and skip all other
				if ((wptser.IsPathToHighlightAPathIndex()) && (path_index != wptser.GetPathToHighlight()))
					continue;

				// does this path exist?
				if (w_paths[path_index] != NULL)
				{
					W_PATH* p, * prev;
					float path_wpt_dist;	// distance between path waypoint and player

					// do we highlight red team accessible paths AND this path is NOT for red team
					if ((wptser.GetPathToHighlight() == HIGHLIGHT_TEAMONE) && !(w_paths[path_index]->flags & (P_FL_TEAM_NO | P_FL_TEAM_RED)))
						continue;	// then skip it
					// do we highlight blue team accessible paths AND this path is NOT for blue team
					else if ((wptser.GetPathToHighlight() == HIGHLIGHT_TEAMTWO) && !(w_paths[path_index]->flags & (P_FL_TEAM_NO | P_FL_TEAM_BLUE)))
						continue;
					// do we highlight one-way paths AND this path is NOT one-way
					else if ((wptser.GetPathToHighlight() == HIGHLIGHT_ONEWAY) && !(w_paths[path_index]->flags & P_FL_WAY_ONE))
						continue;
					// do we want to see just sniper paths AND this one isn't snipers only path
					else if ((wptser.GetPathToHighlight() == HIGHLIGHT_SNIPER) && !(w_paths[path_index]->flags & P_FL_CLASS_SNIPER))
						continue;
					// same as above, but for machine gunner
					else if ((wptser.GetPathToHighlight() == HIGHLIGHT_MGUNNER) && !(w_paths[path_index]->flags & P_FL_CLASS_MGUNNER))
						continue;
					// same as above, but for anti-armor specialist
					else if ((wptser.GetPathToHighlight() == HIGHLIGHT_ANTIARMOR) && !(w_paths[path_index]->flags & P_FL_CLASS_ANTIARMOR))
						continue;

					int sprite = SetPathTexture(path_index);
					Vector color = SetPathColor(path_index);

					p = w_paths[path_index];
					prev = NULL;

					// go through this path
					while (p != NULL)
					{
						path_wpt_dist = (waypoints[p->wpt_index].origin - pEntity->v.origin).Length();

						// is player close enough for us to show him the path beam from this waypoint AND is this waypoint visible either directly or through door?
						if ((path_wpt_dist < wptser.GetWaypointsDrawDistance()) && (util.IsWaypointVisible(waypoints[p->wpt_index].origin, pEntity) ||
							util.IsWaypointVisibleThroughDoor(waypoints[p->wpt_index].origin, prev ? waypoints[prev->wpt_index].origin : g_vecZero, pEntity)))
						{
							// if both exist
							if ((p != NULL) && (prev != NULL))
							{
								DisplayPathBeams.AddNewBeam(prev->wpt_index, p->wpt_index, sprite, color);

								// can we still add another path beam between these two waypoints? (ie. we are trying to display only limited amount of same looking path beams between each two waypoints in order
								// to prevent overloading the engine and so keep flickering waypoints at bay)
								if (DisplayPathBeams.CanBeDisplayed(prev->wpt_index, p->wpt_index, sprite, color))
								{
									Vector v_src = waypoints[prev->wpt_index].origin;
									Vector v_dest = waypoints[p->wpt_index].origin;

									// draw the path beam between those waypoints
									DisplayPathBeams.DrawIt(pEntity, v_src, v_dest, sprite, color, wptser.GetPathsDisplayTime());
								}

							}
						}

						prev = p;		// store pointer to previous node (ie waypoint)
						p = p->next;	// go to next node in linked list
					}
				}
			}

			f_path_time = gpGlobals->time;
		}
	}

	// display the waypoint compass if turned on
	if (wptser.GetCompassIndex() != NO_VAL)
	{
		// is it time to refresh the compass, the delay is purposely higher than the beam life otherwise the compass beam wouldn't blink
		if ((f_compass_time + 0.5f) < gpGlobals->time)
		{
			start = waypoints[wptser.GetCompassIndex()].origin;
			end = pEntity->v.origin;

			// draw red line
			DrawBeam(pEntity, start, end, 2, 255, 5, 5, 125);

			f_compass_time = gpGlobals->time;
		}
	}

#ifdef DEBUG
	devTool.ResetOverrideDisplayTL();
#endif // DEBUG
}


#ifdef _DEBUG
/*
* dumps all waypoints with their basic info into debugging file
*/
void Wpt_Dump(void)
{
	extern void DumpVector(FILE *f, Vector vec);

	FILE *f;

	util.DebugDev("***New dump call***", -100, -100);

	f = fopen(debug_fname, "a");

	for (int i = 0; i < num_waypoints; i++)
	{
		fprintf(f, "WPT #%d -- flag  %d", i, waypoints[i].flags);

		if (waypoints[i].flags & (W_FL_STD | W_FL_CROUCH | W_FL_PRONE | W_FL_JUMP | W_FL_DUCKJUMP | W_FL_SPRINT | W_FL_AMMOBOX | W_FL_BANDAGE | W_FL_DOOR | W_FL_DOORUSE | \
			W_FL_LADDER | W_FL_USE | W_FL_CHUTE | W_FL_SNIPER | W_FL_MINE | W_FL_FIRE | W_FL_AIMING | W_FL_PUSHPOINT | W_FL_TRIGGER | W_FL_ROADBLOCK | W_FL_CROSS | W_FL_GOBACK))
		{
			char flags[256];
			wptmanager.GetWaypointName(i, flags);
			fprintf(f, "   --->> Known flags (%s)\n", flags);
		}
		else if (waypoints[i].flags & W_FL_DELETED)
			fprintf(f, "   --->> known DELETE flag\n");
		else
			fprintf(f, "   --->> UNKNOWN >>> ERROR\n");

		fprintf(f, "WPT #%d -- origin ", i);
		DumpVector(f, waypoints[i].origin);
	}

	fclose(f);
}

/*
* dumps all waypoints with their basic info into debugging file
*/
void Pth_Dump(void)
{
	FILE *f;

	util.DebugDev("***New dump call***", -100, -100);

	f = fopen(debug_fname, "a");

	for (int i = 0; i < num_w_paths; i++)
	{
		fprintf(f, "PTH #%d", i);

		if (w_paths[i] == NULL)
		{
			fprintf(f, " -- NOT USED/DELETED\n");
			continue;
		}

		fprintf(f, " -- flag  %d", w_paths[i]->flags);

		if (w_paths[i]->flags & (P_FL_TEAM_NO | P_FL_TEAM_RED | P_FL_TEAM_BLUE | P_FL_WAY_ONE | P_FL_WAY_TWO | P_FL_WAY_PATROL | P_FL_CLASS_ALL | \
			P_FL_CLASS_SNIPER | P_FL_CLASS_MGUNNER | P_FL_CLASS_ANTIARMOR))
		{
			char the_type[256];

			int flags = w_paths[i]->flags;
			strcpy(the_type, "");

			if (flags & P_FL_TEAM_NO)
				strcat(the_type, "both teams ");
			if (flags & P_FL_TEAM_RED)
				strcat(the_type, "team red ");
			if (flags & P_FL_TEAM_BLUE)
				strcat(the_type, "team blue ");
			if (flags & P_FL_WAY_ONE)
				strcat(the_type, "one-way ");
			if (flags & P_FL_WAY_TWO)
				strcat(the_type, "two-way ");
			if (flags & P_FL_WAY_PATROL)
				strcat(the_type, "patrol ");
			if (flags & P_FL_CLASS_ALL)
				strcat(the_type, "all classes ");
			if (flags & P_FL_CLASS_SNIPER)
				strcat(the_type, "snipers ");
			if (flags & P_FL_CLASS_MGUNNER)
				strcat(the_type, "mgunners ");
			if (flags & P_FL_CLASS_ANTIARMOR)
				strcat(the_type, "anti-armor ");

			//if (Wpt_CountFlags(wpt_index) < 1)
			//	strcat(the_type, "unknown ");

			int length = strlen(the_type);
			the_type[length-1] = '\0';

			fprintf(f, "   --->> Known flags (%s)\n", the_type);
		}
		else
			fprintf(f, "   --->> UNKNOWN >>> ERROR\n");
	}

	fclose(f);

}

/*
* just testing method no real usage
*/
void Wpt_Check(void)
{
	int messed = 0;
	FILE *f = NULL;
	bool init = FALSE;

	for (int i = 0; i < num_waypoints; i++)
	{
		if ((waypoints[i].flags & W_FL_DELETED) && (waypoints[i].flags & W_FL_STD))
		{
			messed++;

			if (init == FALSE)
			{
				util.DebugDev("***New dump call***", -100, -100);
				init = TRUE;
			}

			f = fopen(debug_fname, "a");
			fprintf(f, "WPT #%d -- flag  %d\n", i, waypoints[i].flags);
		}
	}

	if (f)
		fclose(f);

	ALERT(at_console, "There's %d messed wpts\n", messed);
}

void DevDrawBeam(edict_t* pEntity, Vector start, Vector end, int red, int green, int blue, int life, int speed)
{
	if ((pEntity == NULL) || (devTool.IsDisplayTracelines() == false))
		return;

	if (life < 1)
		life = devTool.GetTLBeamDuration();

	DrawBeam(pEntity, start, end, life, red, green, blue, speed);
}

// moves all waypoints by given vector
void ShiftWpts(int x, int y, int z)
{
	for (int i = 0; i < num_waypoints; i++)
	{
		if ((waypoints[i].flags == 0) || (waypoints[i].flags == W_FL_DELETED))
			continue;

		waypoints[i].origin = waypoints[i].origin + Vector(x, y, z);
	}
}

#endif	// _DEBUG
