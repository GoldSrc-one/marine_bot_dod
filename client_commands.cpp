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
// client_commands.cpp
// 
////////////////////////////////////////////////////////////////////////////////////////////////

#include "defines.h"

#pragma warning( disable: 4005 91 )

#include "extdll.h"
#include "util.h"
#include "cbase.h"

#pragma warning( default: 4005 91 )

#include <cctype>

#include "bot.h"
#include "bot_func.h"
#include "bot_manager.h"
#include "waypoint.h"
#include "client_commands.h"
#include "console_output.h"

#ifdef DEBUG
#include "bot_weapons.h"
#endif // DEBUG


#define MENU_NONE		0
#define MENU_MAIN		1
// bot menu & submenus
#define MENU_1			2
#define MENU_1_9		3
#define MENU_1_9_1		4
#define MENU_1_9_6		5
// waypoint menu & submenus
#define MENU_2			6
#define MENU_2_1AND2_P1	7
#define MENU_2_1AND2_P2	8
#define MENU_2_1AND2_P3	9
#define MENU_2_3		10
#define MENU_2_4		11
#define MENU_2_5		12
#define MENU_2_6		13
#define MENU_2_6_3		14
#define MENU_2_7		15
#define MENU_2_7_9_P1	16
#define MENU_2_7_9_P2	17

#define MENU_2_9		24
#define MENU_2_9_2		25
#define MENU_2_9_2_5	26
#define MENU_2_9_2_9	27
#define MENU_2_9_8		28
// misc menu & submenus
#define MENU_3			66
// special case - team menu
#define MENU_99			99

int g_menu_state = 0;			// position in menu (which menu is curr shown)
int g_menu_team = 0;			// for team based options
int g_menu_next_state = 0;		// in cases that one step between curr and next is needed

// 
//		Max length of the string the HUD menu can take is 192 characters including all escape and terminating characters.
//		Longer message will crash the game to the Main menu with error at console.
// 
// main menu
char* show_menu_main =
{ "MarineBot Main menu\n\n1. Bot menu\n2. Waypoint menu\n3. Misc\n4. CANCEL" };
// bot menu & all (possible) submenus
char* show_menu_1 =
{ "MarineBot Bot menu\n\n1. Add allied marine\n2. Add axis marine\n3. Fill server\n4. Kick marine\n5. Kill marine\n6. Balance teams\n\n\n9. Settings\n0. CANCEL" };
char* show_menu_1_9 =
{ "MarineBot Bot Settings menu\n\n1. SpawnSkill\n2. BotSkill (bots in game)\n3. BotSkillUp\n4. BotSkillDown\n5. AimSkill\n6. Reactions\n7. Random SpawnSkill\n\n9. BACK\n0. CANCEL" };
char* show_menu_1_9_1 =
{ "MarineBot Skill Levels menu\n\n1. level 1(best)\n2. level 2\n3. level 3(default)\n4. level 4\n5. level 5(worst)\n\n\n\n9. BACK\n0. CANCEL" };
char* show_menu_1_9_6 =
{ "MarineBot Bot Reactions menu\n\n1. 0.0 s(best)\n2. 0.1 s\n3. 0.2 s\n4. 0.5 s(default)\n5. 1.0 s\n6. 1.5 s\n7. 2.5 s\n8. 5.0 s(worst)\n9. BACK\n0. CANCEL" };
// waypoint menu & all (possible) submenus
char* show_menu_2 =
{ "MarineBot Waypoint menu\n\n1. Placing\n2. Changing\n3. Priority\n4. Time\n5. Range\n6. Autowaypoint\n7. Paths\n\n9. Service\n0. CANCEL" };
char* show_menu_2a =
{ "MarineBot Waypoint menu\n\n1. Placing\n2. unavailable\n3. unavailable\n4. unavailable\n5. unavailable\n6. Autowaypoint\n7. Paths\n\n9. Service\n0. CANCEL" };
char* show_menu_2_1 =
{ "MarineBot Waypoint menu\n\n1. Normal/Std\n2. Delete\n3. Crouch\n4. Prone\n5. Cross\n6. GoBack\n7. Sniper\n8. Sprint\n9. NEXT\n0. CANCEL" };
char* show_menu_2_2 =
{ "MarineBot Waypoint menu\n\n1. Normal/Std\n\n3. Crouch\n4. Prone\n5. Cross\n6. GoBack\n7. Sniper\n8. Sprint\n9. NEXT\n0. CANCEL" };
char* show_menu_2_1and2_p2 =
{ "MarineBot Waypoint menu\n\n1. Aim\n2. Jump\n3. DuckJump\n4. Claymore\n5. Shoot\n6. PushPoint/Flag\n7. Ammobox\n8. Use\n9. NEXT\n0. CANCEL" };
char* show_menu_2_1and2_p3 =
{ "MarineBot Waypoint menu\n\n1. Roadblock\n2. Trigger\n3. Ladder\n4. Parachute\n5. Door\n6. UseDoor\n\n\n9. BACKTOTOP\n0. CANCEL" };
char* show_menu_2_3 =
{ "MarineBot Priority menu\n\n1. level 1(highest)\n2. level 2\n3. level 3\n4. level 4\n5. level 5(lowest/default)\n6. level 0(no)\n\n\n9. BACK\n0. CANCEL" };
char* show_menu_2_4 =
{ "MarineBot Time menu\n\n1. 1 second\n2. 2 seconds\n3. 3 seconds\n4. 4 seconds\n5. 5 seconds\n6. 10 seconds\n7. 20 seconds\n8. 30 seconds\n9. BACK\n0. CANCEL" };
char* show_menu_2_5 =
{ "MarineBot Range menu\n\n1. 5 units\n2. 20 units\n3. 50 units\n4. 75 units\n5. 100 units\n6. 150 units\n7. 250 units\n8. Increase\n9. Decrease\n0. CANCEL" };
char* show_menu_2_6 =
{ "MarineBot Autowaypoint menu\n\n1. Start autowaypoint\n2. Stop autowaypoint\n3. Set distance\n\n\n\n\n\n\n0. CANCEL" };
char* show_menu_2_6_3 =
{ "MarineBot Distance menu\n\n1. 80 units(minimum)\n2. 100 units\n3. 120 units\n4. 160 units\n5. 200 units(default)\n6. 280 units\n7. 340 units\n8. 400 units(maximum)\n9. BACK\n0. CANCEL" };
char* show_menu_2_7 =
{ "MarineBot Path menu\n\n1. Show/Hide paths\n2. Start path\n3. Stop path\n4. Continue path\n5. Add to\n6. Remove from\n7. Delete path\n8. AutoAdding on/off\n9. Path tags\n0. CANCEL" };
char* show_menu_2_7a =
{ "MarineBot Path menu\n\n1. Show/Hide paths\n2. unavailable\n3. Stop path\n4. unavailable\n5. unavailable\n6. unavailable\n7. unavailable\n8. AutoAdding on/off\n9. Path tags\n0. CANCEL" };
char* show_menu_2_7b =
{ "MarineBot Path menu\n\n1. Show/Hide paths\n2. unavailable\n3. unavailable\n4. unavailable\n5. unavailable\n6. unavailable\n7. unavailable\n8. AutoAdding on/off\n9. unavailable\n0. CANCEL" };
char* show_menu_2_7_9_p1 =
{ "MarineBot Path Tags menu\n\n1. One-way\n2. Two-way\n3. Patrol type\n4. Allied team\n5. Axis team\n6. Both teams\n7. Snipers only\n8. Machine gunners only\n9. NEXT\n0. CANCEL" };
char* show_menu_2_7_9_p2 =
{ "MarineBot Path Tags menu\n\n1. Anti-armor specialists only\n2. All classes\n3. Avoid enemy\n4. Ignore enemy\n5. Carry item\n\n\n\n9. BACK\n0. CANCEL" };
char* show_menu_2_9 =
{ "MarineBot Waypoints Service menu\n\n1. Show/Hide\n2. Adv. debugging\n3. Load\n4. Save\n5. Convert older\n6. Clear (deletes from map)\n7. Destroy (erases from drive)\n8. Change folder\n\n0. CANCEL" };
char* show_menu_2_9_2 =
{ "MarineBot Waypoints Debugging menu\n\n1. Check cross\n2. Check aim\n3. Check range\n4. Check shoot\n5. Path highlight\n\n\n\n9. Draw distance\n0. CANCEL" };
char* show_menu_2_9_2_5 =
{ "MarineBot Paths Highlight menu\n\n1. Allied team\n2. Axis team\n3. One-way path\n4. Sniper path\n5. Machine gunner path\n6. Anti-armor specialist path\n\n\n9. Turn off\n0. CANCEL" };
char* show_menu_2_9_2_9 =
{ "MarineBot Waypoints Draw Distance menu\n\n1. 400 units\n2. 500 units\n3. 600 units\n4. 700 units\n5. 800 units(default)\n6. 900 units\n7. 1000 units\n8. 1200 units\n9. 1400 units(maximum)\n0. CANCEL" };
char* show_menu_2_9_8 =
{ "MarineBot Waypoints Folder Select menu\n\n1. Default\n2. Custom\n\n\n\n\n\n\n9. BACK\n0. CANCEL" };
// misc menu & all (possible) submenus
char* show_menu_3 =
{ "MarineBot Misc menu\n\n1. Observer\n2. MrFreeze\n3. BotDontShoot\n4. BotDontShootFirearm\n5. BotIgnoreAll\n6. MBNoClip\n7. BotDontSpeak\n8. BotDontChat\n9. BotDontChatToBots\n0. CANCEL" };
// special case - team menu
char* show_menu_99 =
{ "MarineBot Team Select menu\n\n1. Allied team\n2. Axis team\n\n\n\n\n\n\n\n0. CANCEL" };
char* show_menu_99a =
{ "MarineBot Team Select menu\n\n1. Allied team\n2. Axis team\n3. Both teams\n\n\n\n\n\n\n0. CANCEL" };


extern bool is_dedicated_server;
extern char mb_version_info[32];

// function prototypes used in this file

void PrintBasicBotInfo(edict_t* pEdict, int bot_array_index);


#ifdef _DEBUG

extern int debug_engine;

extern botname_t bot_names_american[MAX_BOT_NAMES];			// array of all names read from external file
extern botname_t bot_names_british[MAX_BOT_NAMES];
extern botname_t bot_names_german[MAX_BOT_NAMES];

inline bool CheckForSomeDebuggingCommands(edict_t* pEntity, const char* pcmd, const char* arg1, const char* arg2, const char* arg3, const char* arg4, const char* arg5);// debugging commands
#endif


console_input_t::console_input_t()
{
	used_cmd_name[0] = '\0';
	keyword[0] = '\0';
	missing_argument = false;
	range_from = 0.0f;
	range_to = 0.0f;
	arg_value = 0.0f;
	valid_index = NO_VAL;
	pCmdInvoker = NULL;
}


void console_input_t::Reset(void)
{
	used_cmd_name[0] = '\0';
	ResetKeyWord();
	missing_argument = false;
	range_from = 0.0f;
	range_to = 0.0f;
	arg_value = 0.0f;
	ResetValidIndex();
}


/*
* returns true if the command matches the name or alternate name
*/
bool console_input_t::IsCommand(const char* the_command, const char* command_name, const char* alt_cmd_name)
{
	if ((the_command != NULL) && (the_command[0] != 0))
	{
		// remember the command that has been used
		SetCmdName(the_command);

		if (strcmp(the_command, command_name) == 0)
			return true;

		if (alt_cmd_name && (strcmp(the_command, alt_cmd_name) == 0))
			return true;
	}

	return false;
}


bool console_input_t::IsCommand(const char* the_command, const char* command_name, const char* alt_cmd_name1, const char* alt_cmd_name2)
{
	if ((the_command != NULL) && (the_command[0] != 0))
	{
		SetCmdName(the_command);

		if (strcmp(the_command, command_name) == 0)
			return true;

		if (alt_cmd_name1 && (strcmp(the_command, alt_cmd_name1) == 0))
			return true;

		if (alt_cmd_name2 && (strcmp(the_command, alt_cmd_name2) == 0))
			return true;
	}

	return false;
}


/*/																			NOT BEING USED
bool console_input_t::IsValidBooleanValue(const char* the_argument)
{
	// is there any argument at all? 
	if ((the_argument != NULL) && (the_argument[0] != 0))
	{
		// is it a yes or no string?
		if ((strcmp(the_argument, "yes") == 0) || (strcmp(the_argument, "no") == 0))
			return true;
	}
	else
		SetMissingArgument();

	return false;
}
/**/


bool console_input_t::IsValidFloatValue(const char* the_argument, float range_from, float range_to, float unique_val)
{
	// does the argument exist and is it a number?
	if ((the_argument != NULL) && (the_argument[0] != 0) && isdigit(the_argument[0]))
	{
		// convert it to float and store it
		arg_value = strtof(the_argument, NULL);

		// set the range of validity
		SetRange(range_from, range_to);

		// finally check whether it fits it
		if (IsInRange(arg_value) || ((unique_val != (float)val_not_set) && (arg_value == unique_val)))
			return true;
	}
	else
		SetMissingArgument();

	return false;
}


bool console_input_t::IsValidIntegerValue(const char* the_argument, int range_from, int range_to, int unique_val)
{
	if ((the_argument != NULL) && (the_argument[0] != 0) && isdigit(the_argument[0]))
	{
		arg_value = atoi(the_argument);

		SetRange(range_from, range_to);

		if (IsInRange(arg_value) || ((unique_val != val_not_set) && ((int)arg_value == unique_val)))
			return true;
	}
	else
		SetMissingArgument();

	return false;
}


/*
* checks the passed argument whether it exists and whether it is some word
* the switch allows validate even a missing argument, but then it's flagged as a missing argument
*/
bool console_input_t::IsValidKeyWord(const char* the_argument, bool allow_missing_argument)
{
	if ((the_argument != NULL) && (the_argument[0] != 0))
	{
		// is the first character a letter?
		if (isalpha(the_argument[0]))
		{
			// then we have a valid keyword
			SetKeyWord(the_argument);
			return true;
		}
		// otherwise it must be a number or something else and that is NOT a valid keyword
		else
			return false;
	}

	if (allow_missing_argument)
	{
		SetMissingArgument();

		return true;
	}
	
	return false;
}


bool console_input_t::IsValidArrayIndex(const char* the_argument, int array_size)
{
	if ((the_argument != NULL) && (the_argument[0] != 0) && isdigit(the_argument[0]))
	{
		arg_value = atoi(the_argument) - 1;

		SetRange(0, array_size - 1);

		if (IsInRange(arg_value))
		{
			SetValidIndex(arg_value);
			return true;
		}
	}
	else
		SetMissingArgument();

	return false;
}


/*
* checks the passed argument whether it exists, whether it is a number and whether it is valid path index or unique keyword
* however it does NOT check whether such path really exists (ie. it could be a deleted path)
* also allows validate when no argument was given in which case the valid index is set to "no value" ie. -1
*/
bool console_input_t::IsValidPathIndex(const char* the_argument, bool allow_missing_argument, const char* unique_keyword)
{
	if ((the_argument != NULL) && (the_argument[0] != 0))
	{
		// first check if the argument is a number
		if (isdigit(the_argument[0]))
		{

#ifdef DEBUG
			ALERT(at_console, "ValidatePthInx -> arg is a digit\n");
#endif // DEBUG



			// paths are stored in an array so we must make it zero based in order to store correct value
			arg_value = atoi(the_argument) - 1;

			if ((arg_value >= 0) && (arg_value < num_w_paths))
			{

#ifdef DEBUG
				ALERT(at_console, "ValidatePthInx -> arg is a digit -> and is valid PthInx\n");
#endif // DEBUG



				SetValidIndex(arg_value);
				return true;
			}
		}
		// check for the special case keywords (eg. nearby)
		else if ((unique_keyword != NULL) && (unique_keyword[0] != 0) && (strcmp(the_argument, unique_keyword) == 0))
		{


#ifdef DEBUG
			ALERT(at_console, "ValidatePthInx -> arg is a unique keyword\n");
#endif // DEBUG



			// all waypoint/path dealing functions do check for validity so if they get -1 value the processing will stop right away
			// and if there's an unsafe function it'll cause crash which is probably better solution than working with wrong waypoint/path
			ResetValidIndex();

			return true;
		}
	}
	// allows validate when no argument was given (no arg means empty)
	else if (allow_missing_argument && (the_argument != NULL) && (the_argument[0] == 0))
	{


#ifdef DEBUG
		ALERT(at_console, "ValidatePthInx -> arg is missing but Validated (this function allows it)\n");
#endif // DEBUG



		// set "no value" to the valid index
		ResetValidIndex();
		// but we also make sure the system knows there was no argument used
		SetMissingArgument();

		return true;
	}
	else
		SetMissingArgument();


#ifdef DEBUG
	ALERT(at_console, "ValidatePthInx -> arg FAILED to be validated\n");
#endif // DEBUG


	return false;
}


/*
* checks the passed argument whether it exists, whether it is a number and whether it is valid waypoint index or unique keyword
* however it does NOT check whether such waypoint really exists (ie. it could be a deleted waypoint)
* also allows validate when no argument was given in which case the valid index is set to "no value" ie. -1
*/
bool console_input_t::IsValidWaypointIndex(const char* the_argument, bool allow_missing_argument, const char* unique_keyword)
{
	if ((the_argument != NULL) && (the_argument[0] != 0))
	{
		if (isdigit(the_argument[0]))
		{

#ifdef DEBUG
			ALERT(at_console, "ValidateWPTInx -> arg is a digit\n");
#endif // DEBUG



			// waypoints are stored in an array so we must make it zero based in order to store correct value
			arg_value = atoi(the_argument) - 1;

			if ((arg_value >= 0) && (arg_value < num_waypoints))
			{


#ifdef DEBUG
				ALERT(at_console, "ValidateWPTInx -> arg is a digit -> and is valid WPTInx\n");
#endif // DEBUG


				SetValidIndex(arg_value);
				return true;
			}
		}
		else if ((unique_keyword != NULL) && (unique_keyword[0] != 0) && (strcmp(the_argument, unique_keyword) == 0))
		{


#ifdef DEBUG
			ALERT(at_console, "ValidateWPTInx -> arg is a unique keyword\n");
#endif // DEBUG


			// all waypoint/path dealing functions do check for validity so if they get -1 value the processing will stop right away
			// and if there's an unsafe function it'll cause crash which is probably better solution than working with wrong waypoint/path
			ResetValidIndex();

			return true;
		}
	}
	// allows validate when no argument was given
	else if (allow_missing_argument && (the_argument != NULL) && (the_argument[0] == 0))
	{


#ifdef DEBUG
		ALERT(at_console, "ValidateWPTInx -> arg is missing but Validated (this function allows it)\n");
#endif // DEBUG



		ResetValidIndex();
		SetMissingArgument();

		return true;
	}
	else
		SetMissingArgument();


#ifdef DEBUG
	ALERT(at_console, "ValidateWPTInx -> arg FAILED to be validated\n");
#endif // DEBUG


	return false;
}


/*
* checks the passed argument whether it exists and whether it is a valid team keyword
* the switch allows validate even a number that represents the team when used as argument
*/
bool console_input_t::IsValidTeam(const char* the_argument, bool allow_numbers, bool allow_keyword_both)
{
	if ((the_argument != NULL) && (the_argument[0] != 0))
	{
		if ((strcmp(the_argument, teamONE.GetTeamName()) == 0) ||
			(allow_numbers && (strcmp(the_argument, teamONE.GetTeamIdAsString()) == 0)))
		{
			SetKeyWord(teamONE.GetTeamName());
			arg_value = teamONE.GetTeamId();
			return true;
		}

		if ((strcmp(the_argument, teamTWO.GetTeamName()) == 0) ||
			(allow_numbers && (strcmp(the_argument, teamTWO.GetTeamIdAsString()) == 0)))
		{
			SetKeyWord(teamTWO.GetTeamName());
			arg_value = teamTWO.GetTeamId();
			return true;
		}

		if (allow_keyword_both && (strcmp(the_argument, "both") == 0))
		{
			SetKeyWord("both");
			return true;
		}
	}
	else
		SetMissingArgument();

	return false;
}


void console_input_t::SetRange(float from, float to)
{
	range_from = from;
	range_to = to;
}


bool console_input_t::IsInRange(float value)
{
	if ((value >= range_from) && (value <= range_to))
		return true;

	return false;
}


/*/																			NOT BEING USED
bool console_input_t::IsOutOfRange(float value)
{
	return !IsInRange(value);
}
/**/


/*
* makes the work little easier when there isn't set strict order of the arguments
* (ie. both index and then keyword as well as keyword and then index)
* checks both arguments and sets the keyword and the valid index if both are present in the arguments
* TODO: Try to implement option for a word 'nearby' that could be used instead of the waypoint index number.
*/
void console_input_t::ProcessWaypointInput(const char* arg1, const char* arg2)
{

#ifdef DEBUG
	//ALERT(at_console, "arg1=<%s> | arg2=<%s>\n", arg1, arg2);
#endif // DEBUG
	
	// do both arguments exist?
	if ((arg1 != NULL) && (*arg1 != 0) && (arg2 != NULL) && (*arg2 != 0))
	{
		// both arguments are numbers
		if (isdigit(arg1[0]) && isdigit(arg2[0]))
		{
			// try to find at least a valid waypoint index in either of those two numbers
			if (IsValidWaypointIndex(arg1) == false)
				IsValidWaypointIndex(arg2);

			return;
		}

		// arg1 is number and arg2 is some word
		if (isdigit(arg1[0]) && isalpha(arg2[0]))
		{
			IsValidWaypointIndex(arg1);
			SetKeyWord(arg2);
			return;
		}

		// arg1 is some word and arg2 is number
		if (isalpha(arg1[0]) && isdigit(arg2[0]))
		{
			SetKeyWord(arg1);
			IsValidWaypointIndex(arg2);
			return;
		}
	}

	// there must be just one argument so ...
	if ((arg1 != NULL) && (*arg1 != 0))
	{
		// is it some word?
		if (isalpha(arg1[0]))
		{
			SetKeyWord(arg1);
			SetMissingArgument();
		}
		// must be number then so let's check whether it is a valid index,
		// also don't set missing argument here, because empty keyword won't cause
		// issues when checked for existence
		else
			IsValidWaypointIndex(arg1);
	}
	// this is a must to allow checking for both the invalid argument as well as completely missing argument
	else
	{
		SetMissingArgument();
	}

	return;
}



/*
* converts the array based index to real world value with the exception of "no value" which will remain -1
*/
int console_input_t::PrintValidIndex(void)
{
	if (valid_index == NO_VAL)
		return valid_index;

	return valid_index + 1;
}


/*
* all Marine Bot listen server commands
* you must end each main 'if' branch with return otherwise you'll get 'unknown command' error in game
*/
bool CustomClientCommands(edict_t* pEntity, const char* pcmd, const char* arg1, const char* arg2, const char* arg3, const char* arg4, const char* arg5)
{
	char msg[TEXT_MSG_SIZE]{};

	// first reset console input
	conInput.Reset();

	if ((FStrEq(pcmd, "help")) || (FStrEq(pcmd, "?")))
	{
		if (FStrEq(arg1, "botsetup"))
		{
			// we don't need to show help in notify area (top left corner on the screen) therefore we send them directly to console
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "\n***Additional bot settings***\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[kick <\"name\">] kicks bot with specified name (name must be in double quotes)\n");
			sprintf(msg, "[killbot <arg>] 'arg' can be <all> - kills all bots, <%s/%s> - kills all bots on given team\n", teamONE.GetTeamName(), teamTWO.GetTeamName());
			ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[randomskill] toggles between using default bot skill and generating the skill randomly for each bot\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[spawnskill <number>] sets default bot skill on join (1-5 where 1=best, no effect to bots already in game)\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[setbotskill <number>] sets bot skill level for all bots already in game (1-5 where 1=best)\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[botskillup] increases bot skill level for all bots already in game (bots will be better)\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[botskilldown] decreases bot skill level for all bots already in game (bots will be worse)\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[setaimskill <number>] sets aim skill level for all bots already in game (1-5 where 1=best)\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[botskillview] prints botskill & aimskill for all bots\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[reactiontime <number>] sets bot reaction time (0-50 where 1 will be converted to 0.1s and 50 to 5.0s)\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[rangelimit <number>] sets the max distance of enemy the bot can see & attack (500-7500 units)\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[botdontspeak <off>] bots will not use Voice&Radio commands, off parameter returns to normal\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[botdontchat <off>] bots will not use say&say_team commands, off parameter returns to normal\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[botdontchattobots <off>] bots will not use say&say_team commands to other bots, off parameter returns to normal\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[meleeonly <off>] bots will use only melee weapons, off parameter returns to normal\n");
		}
		else if (FStrEq(arg1, "cheats"))
		{
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "\n***Should help in troubles***\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[observer <off>] bot ignores you, off parameter returns to normal\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[mrfreeze <off>] bot doesnt move, off parameter returns to normal\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[botdontshoot <off>] bot doesnt shoot, off parameter returns to normal\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[botdontshootfirearm <off>] bot doesnt shoot any firearm, but uses grenades & knife normally, off returns to normal usage\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[botignoreall <off>] bot ignores all enemies, off parameter returns to normal\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[mbnoclip <off>] you will be able to fly through all on the map, off parameter returns to normal\n");
		}
		else if (FStrEq(arg1, "misc"))
		{
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "\n***Miscellaneous commands***\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[checkaims] connects 'wait timed' waypoint with its valid aim waypoints\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[checkcross] connects cross waypoint with all waypoints in range\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[checkranges] draws (highlight) waypoint range\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[checkshoot] connects shoot waypoint with one particular or all breakable objects within its reach\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[checkbots <arg>] prints basic info about bot based on 'arg', which can be 'all' or 'used' or number or name of the bot\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[getwptsystem] prints the system the waypoint file is created in\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[gettexturename] prints the name of the texture in front of you\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[changestartpositions <off>] allows using the alternative start points from files in 'mapcfgs'\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[changecaptureareas <off>] allows using the alternative data for capture areas from files in 'mapcfgs'\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[generatecaptureareasfile] builds the file with capture areas data inside 'mapcfgs'\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[checkcaptureareas <arg>] prints info about Capture Areas, 'arg' can be 'all' or 'used' or number to view that one area\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[checkcontrolpoints <arg>] prints info about Control Points, 'arg' can be 'all' or 'used' or number to view that one point\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[mbmenushortcutwpt] shortcut to waypoint menu\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[mbmenushortcutwpttags1] shortcut to waypoint tags menu page 1\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[mbmenushortcutpath] shortcut to path menu\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[mbmenushortcutpathtags1] shortcut to path tags menu page 1\n");
		}
		else
		{
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "\n-------------------------------------------\nMarineBot console commands help\n-------------------------------------------\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[recruit <team> <class> <skill> <name>] adds bot with specified params, if no params all except skill is taken randomly\n");
			sprintf(msg, "[kickbot <arg>] 'arg' can be <all> - kicks all bots, <%s/%s> - kicks one bot on given team off the game\n", teamONE.GetTeamName(), teamTWO.GetTeamName());
			ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[balanceteams <off>] tries balancing teams on server (moves bots to weaker team), 'off' disables autobalancing\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[fillserver <number>] fills the server with bots up to maxplayers limit or up to 'number' limit\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[marinebotmenu] shows MB HUD menu\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[directory <name>] toggles through waypoint directories or directly sets one via 'name'\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[versioninfo] prints MB version\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "\n<><><><><><><><><><><><><><><><><><><<><><><><><><>\nfor additional help write one of following commands (without brackets)\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "<><><><><><><><><><><><><><><><><><><<><><><><><><>\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[help botsetup] botsetting help | [help cheats] few cheats | [help misc] various settings help\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[wpt help] waypointing help | [autowpt help] autowaypointing help | [pathwpt help] pathwaypointing help\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[debug help] debugging help | [test help] test help\n");
		}

		return true;
	}
	// adds one bot to a game (allows full bot customization)
	else if (conInput.IsCommand(pcmd, "recruit", "addmarine", "addbot"))
	{
		BotCreate(pEntity, arg1, arg2, arg3, arg4, NULL);
		botmanager.SetBotCheckTime(gpGlobals->time + 2.5);

		return true;
	}
	// will be automatically adding bots one by one till maxplayers or given amount is reached
	else if (FStrEq(pcmd, "fillserver"))
	{
		int total_clients;

		total_clients = 0;
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (clients[i].pEntity != NULL)
				total_clients++;
		}

		if (total_clients >= gpGlobals->maxClients)
		{
			botmanager.ResetListenServerFilling();

			PlaySoundConfirmation(pEntity, SND_FAILED);
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "error - server full\n");
		}
		else
		{
			botmanager.SetListeServerFilling(true);
			botmanager.SetBotCheckTime(gpGlobals->time + 0.5f);

			// check if there is specified bot ammount to add (i.e. "arg filling")
			if ((arg1 != NULL) && (*arg1 != 0))
			{
				int temp = atoi(arg1);

				if ((temp >= 1) && (temp < gpGlobals->maxClients - total_clients))
				{
					botmanager.SetBotsToBeAdded(temp);

					PlaySoundConfirmation(pEntity, SND_DONE);
					sprintf(msg, "%d bots will be added\n", botmanager.GetBotsToBeAdded());
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
				}
				else
				{
					botmanager.ResetListenServerFilling();

					PlaySoundConfirmation(pEntity, SND_FAILED);
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "error - invalid argument or over maxplayers limit!\n");

					return true;
				}
			}

			ClientPrint(pEntity, HUD_PRINTCONSOLE, "filling the server...\n");
		}

		return true;
	}
	// allows to kick bots from the game based on given argument
	else if (conInput.IsCommand(pcmd, "kickbot", "kick_bot"))
	{
		KickBotCommand(pEntity, arg1);

		return true;
	}
	// allows to kill bots that are present in the game based on given argument
	else if (conInput.IsCommand(pcmd, "killbot", "kill_bot"))
	{
		KillBotCommand(pEntity, arg1);

		return true;
	}
	// allows balancing the teams i.e. both will have the same player count
	else if (conInput.IsCommand(pcmd, "balanceteams", "balance_teams"))
	{
		// turn off autobalance for this map, it will be turned back on after a map change
		if (FStrEq(arg1, "off"))
		{
			// disable it only once
			if (botmanager.IsTeamsBalanceNeeded() && (botmanager.IsOverrideTeamsBalance() == false))
			{
				botmanager.SetOverrideTeamsBalance(true);

				ClientPrint(pEntity, HUD_PRINTCONSOLE, "autobalance is temporary DISABLED\n");
				PlaySoundConfirmation(pEntity, SND_DONE);
			}
			else
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "autobalance is already DISABLED\n");

			return true;
		}

		botmanager.SetTeamsBalanceValue(util.TeamsBalanceCheck());

		if (botmanager.GetTeamsBalanceValue() == -1)
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "teams balanced\n");
		else if (botmanager.GetTeamsBalanceValue() > 100)
		{
			sprintf(msg, "kick %d bots from %s and add them to %s\n", botmanager.GetTeamsBalanceValue() - 100, teamONE.GetTeamName2wordsFUC(), teamTWO.GetTeamName2wordsFUC());
			ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
		}
		else if ((botmanager.GetTeamsBalanceValue() > 0) && (botmanager.GetTeamsBalanceValue() < 100))
		{
			sprintf(msg, "kick %d bots from %s and add them to %s\n", botmanager.GetTeamsBalanceValue(), teamTWO.GetTeamName2wordsFUC(), teamONE.GetTeamName2wordsFUC());
			ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
		}
		else if (botmanager.GetTeamsBalanceValue() < -1)
		{
			PlaySoundConfirmation(pEntity, SND_FAILED);
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "ERROR\n");
		}

		return true;
	}
	// determines whether the spawning bots will use default skill or pick the skill randomly
	else if (conInput.IsCommand(pcmd, "randomskill", "random_skill"))
	{
		RandomSkillCommand(pEntity, arg1);

		return true;
	}
	// sets the default bot skill that will be used when he is added to the game
	else if (conInput.IsCommand(pcmd, "spawnskill", "spawn_skill"))
	{
		SpawnSkillCommand(pEntity, arg1);

		return true;
	}
	// allows changing the bot skill of all the bots that are present in the game
	else if (conInput.IsCommand(pcmd, "setbotskill", "set_botskill"))
	{
		SetBotSkillCommand(pEntity, arg1);

		return true;
	}
	// increases the bot skill of all the bots that are present in the game (i.e. they'll be better)
	else if (conInput.IsCommand(pcmd, "botskillup", "botskill_up"))
	{
		BotSkillUpCommand(pEntity, arg1);

		return true;
	}
	// decreases the bot skill of all the bots that are present in the game (i.e. they'll be worse)
	else if (conInput.IsCommand(pcmd, "botskilldown", "botskill_down"))
	{
		BotSkillDownCommand(pEntity, arg1);

		return true;
	}
	// allows changing the aiming skill of all the bots that are present in the game
	else if (conInput.IsCommand(pcmd, "setaimskill", "set_aimskill"))
	{
		SetAimSkillCommand(pEntity, arg1);

		return true;
	}
	// prints the botskill & aimskill of all the bots that are present in the game
	else if (FStrEq(pcmd, "botskillview"))
	{
		int i, j;
		char client_name[BOT_NAME_LEN + 1];

		ClientPrint(pEntity, HUD_PRINTCONSOLE, "\n-----------------------------\n");

		for (i = 0; i < MAX_CLIENTS; i++)
		{
			if (bots[i].is_used == FALSE)
				continue;

			for (j = 0; j < MAX_CLIENTS; j++)
			{
				if (bots[i].pEdict == clients[j].pEntity)
					break;
			}

			strcpy(client_name, STRING(clients[j].pEntity->v.netname));

			sprintf(msg, "%s's bot_skill:%d and aim_skill:%d\n", client_name, bots[i].GetBotSkill() + 1, bots[i].GetAimSkill() + 1);
			ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
		}
		ClientPrint(pEntity, HUD_PRINTCONSOLE, "-----------------------------\n\n");

		return true;
	}
	// allows changing the reaction time of the bots, it has instant effect
	else if (conInput.IsCommand(pcmd, "reactiontime", "reaction_time"))
	{
		SetReactionTimeCommand(pEntity, arg1);

		return true;
	}
	// sets the max distance of an enemy the bots can see and attack it
	else if (conInput.IsCommand(pcmd, "rangelimit", "range_limit"))
	{
		RangeLimitCommand(pEntity, arg1);

		return true;
	}
	// allows changing the waypoint folder based on the argument
	else if (FStrEq(pcmd, "directory"))
	{
		SetWaypointDirectoryCommand(pEntity, arg1);

		return true;
	}
	else if (FStrEq(pcmd, "marinebotmenu"))				// MarineBot HUD based command menu
	{
		g_menu_state = MENU_MAIN;
		util.ShowMenu(pEntity, 0x1F, -1, FALSE, show_menu_main);

		return true;
	}
	else if (FStrEq(pcmd, "mbmenushortcutwpt"))			// shortcut to waypoint menu
	{
		g_menu_state = MENU_2;
		int nearby_wpt_index = wptmanager.FindNearestWaypointToPlayer(pEntity);

		if (nearby_wpt_index != NO_VAL)
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2);
		else
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2a);

		return true;
	}
	else if (FStrEq(pcmd, "mbmenushortcutwpttags1"))		// shortcut to waypoint tags menu (1st page)
	{
		g_menu_state = MENU_2_1AND2_P1;

		int nearby_wpt_index = wptmanager.FindNearestWaypointToPlayer(pEntity);

		if (nearby_wpt_index != NO_VAL)
		{
			g_menu_next_state = 129;		// to know that it's a waypoint tag change
			// but we have to show the "placing" menu here, because by the logic of things we also need to have an option to delete such waypoint
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_1);
			// but in order to make things work the way we want, we must also use the help of this dummy value, else the deletion wouldn't pass
			g_menu_team = -1;
		}
		else
		{
			g_menu_next_state = 128;		// to know that it's a waypoint addition
			// but we have to show the "changing" menu here, because you can't delete nonexistent waypoint so we can't show it as an option either
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_2);
			g_menu_team = -1;
		}

		return true;
	}
	else if (FStrEq(pcmd, "mbmenushortcutpath"))		// shortcut to path menu
	{
		g_menu_state = MENU_2_7;
		int nearby_wpt_index = wptmanager.FindNearestWaypointToPlayer(pEntity);

		if (nearby_wpt_index != NO_VAL)
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_7);
		else if (internals.IsPathToContinue())
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_7a);
		else
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_7b);

		return true;
	}
	else if (FStrEq(pcmd, "mbmenushortcutpathtags1"))	// shortcut to path tags menu (1st page)
	{
		g_menu_state = MENU_2_7_9_P1;
		int nearby_wpt_index = wptmanager.FindNearestWaypointToPlayer(pEntity);

		if ((nearby_wpt_index != NO_VAL) || internals.IsPathToContinue())
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_7_9_p1);

		return true;
	}
	else if (conInput.IsCommand(pcmd, "versioninfo", "version_info"))	// version info
	{
		sprintf(msg, "You are running MarineBot in version %s\n", mb_version_info);

		// display it also on HUD using same style as MB intro messages
		Vector color1 = Vector(200, 50, 0);
		Vector color2 = Vector(0, 250, 0);
		CustHudMessage(pEntity, msg, color1, color2, 2, 8);

		ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);

		return true;
	}
	else if (FStrEq(pcmd, "observer"))		// bots ignore player
	{
		if (botdebugger.IsObserverMode() || FStrEq(arg1, "off"))
		{
			botdebugger.ResetObserverMode();
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "observer mode DISABLED\n");
		}
		else
		{
			botdebugger.SetObserverMode(true);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "observer mode ENABLED\n");
		}

		return true;
	}
	else if (FStrEq(pcmd, "mrfreeze"))		// bot is stood/don't move
	{
		if (botdebugger.IsFreezeMode() || FStrEq(arg1, "off"))
		{
			botdebugger.ResetFreezeMode();
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "freeze mode DISABLED\n");
		}
		else
		{
			botdebugger.SetFreezeMode(true);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "freeze mode ENABLED\n");
		}

		return true;
	}
	else if (FStrEq(pcmd, "botdontshoot"))	// bot will NOT press trigger
	{
		if (botdebugger.IsDontShoot() || FStrEq(arg1, "off"))
		{
			botdebugger.ResetDontShoot();
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "botdontshoot mode DISABLED\n");
		}
		else
		{
			botdebugger.SetDontShoot(true);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "botdontshoot mode ENABLED\n");
		}

		return true;
	}
	else if (FStrEq(pcmd, "botdontshootfirearm"))	// bot will NOT press trigger while using any firearm (grenades and knife are used normally)
	{
		if (botdebugger.IsDontShootFirearm() || FStrEq(arg1, "off"))
		{
			botdebugger.ResetDontShootFirearm();
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "botdontshootfirearm mode DISABLED\n");
		}
		else
		{
			botdebugger.SetDontShootFirearm(true);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "botdontshootfirearm mode ENABLED\n");
		}

		return true;
	}
	else if (FStrEq(pcmd, "botignoreall"))	// bot ignore all opponents
	{
		if (botdebugger.IsIgnoreAll() || FStrEq(arg1, "off"))
		{
			botdebugger.ResetIgnoreAll();
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "botignoreall mode DISABLED\n");
		}
		else
		{
			botdebugger.SetIgnoreAll(true);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "botignoreall mode ENABLED\n");
		}

		return true;
	}
	// bot will not use Voice&Radio cmds
	else if (conInput.IsCommand(pcmd, "botdontspeak", "dont_speak"))
	{
		DontSpeakCommand(pEntity, arg1);

		return true;
	}
	// bot will/won't use say and say_team commands
	else if (conInput.IsCommand(pcmd, "botdontchat", "dont_chat"))
	{
		DontChatCommand(pEntity, arg1);

		return true;
	}
	// bot will/won't use say and say_team commands related to other bots (ie. will chat freely to all clients or to human clients only)
	else if (conInput.IsCommand(pcmd, "botdontchattobots", "dont_chat_tobots"))
	{
		DontChatToBotsCommand(pEntity, arg1);

		return true;
	}
	// bot will switch to melee only weapons
	else if (conInput.IsCommand(pcmd, "meleeonly", "melee_only"))
	{
		MeleeOnlyCommand(pEntity, arg1);

		return true;
	}
	// client who use this command will switch to no clipping mode
	else if (FStrEq(pcmd, "mbnoclip"))
	{
		if ((pEntity->v.movetype == MOVETYPE_NOCLIP) || (FStrEq(arg1, "off")))
		{
			pEntity->v.movetype = MOVETYPE_WALK;
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "noclipping mode DISABLED\n");
		}
		else
		{
			pEntity->v.movetype = MOVETYPE_NOCLIP;
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "noclipping mode ENABLED\n");
		}

		return true;
	}
	// show valid connections
	else if (conInput.IsCommand(pcmd, "checkaims", "check_aims"))
	{
		// turn off these first to prevent overload on netchannel
		wptser.ResetCheckRanges();
		wptser.ResetCheckShoot();

		if (wptser.IsCheckAims() || FStrEq(arg1, "off"))
		{
			wptser.ResetCheckAims();
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "DISABLED!\n", conInput.GetCmdName());
		}
		else
		{
			wptser.SetCheckAims(true);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "ENABLED!\n", conInput.GetCmdName());
		}

		return true;
	}
	// show valid connections
	else if (conInput.IsCommand(pcmd, "checkcross", "check_cross"))
	{
		wptser.ResetCheckRanges();
		wptser.ResetCheckShoot();

		if (wptser.IsCheckCross() || FStrEq(arg1, "off"))
		{
			wptser.ResetCheckCross();
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "DISABLED!\n", conInput.GetCmdName());
		}
		else
		{
			wptser.SetCheckCross(true);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "ENABLED!\n", conInput.GetCmdName());
		}

		return true;
	}
	// highlight waypoint ranges
	else if (conInput.IsCommand(pcmd, "checkranges", "check_ranges"))
	{
		wptser.ResetCheckAims();
		wptser.ResetCheckCross();
		wptser.ResetCheckShoot();

		if (wptser.IsCheckRanges() || FStrEq(arg1, "off"))
		{
			wptser.ResetCheckRanges();
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "DISABLED!\n", conInput.GetCmdName());
		}
		else
		{
			wptser.SetCheckRanges(true);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "ENABLED!\n", conInput.GetCmdName());
		}

		return true;
	}
	else if (conInput.IsCommand(pcmd, "checkshoot", "check_shoot"))
	{
		wptser.ResetCheckAims();
		wptser.ResetCheckCross();
		wptser.ResetCheckRanges();

		if (wptser.IsCheckShoot() || FStrEq(arg1, "off"))
		{
			wptser.ResetCheckShoot();
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "DISABLED!\n", conInput.GetCmdName());
		}
		else
		{
			wptser.SetCheckShoot(true);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "ENABLED!\n", conInput.GetCmdName());
		}

		return true;
	}
	else if (conInput.IsCommand(pcmd, "checkbots", "check_bots"))
	{
		if ((arg1 != NULL) && (*arg1 != 0))
		{
			// we're checking for any string long enough to know it is a word, because string with length of 2 characters can still be a number
			if (!isdigit(arg1[0]) && (strlen(arg1) > 2))
			{
				if (FStrEq(arg1, "all") || FStrEq(arg1, "used"))
				{
					for (int bot_index = 0; bot_index < MAX_CLIENTS; bot_index++)
					{
						if (bots[bot_index].is_used)
							PrintBasicBotInfo(pEntity, bot_index);
					}

					ClientPrint(pEntity, HUD_PRINTCONSOLE, "\n***If you see no data then there are no bots in game.***\n");
				}
				else
				{
					int bot_index = util.FindBotByName(arg1);
					if (bot_index != -1)
						PrintBasicBotInfo(pEntity, bot_index);
					else
					{
						ClientPrint(pEntity, HUD_PRINTCONSOLE, "no bot with such name\n");
						PlaySoundConfirmation(pEntity, SND_FAILED);
					}
				}
			}
			else
			{
				if (conInput.IsValidArrayIndex(arg1, MAX_CLIENTS))
				{
					if (bots[conInput.GetValidIndex()].is_used)
						PrintBasicBotInfo(pEntity, conInput.GetValidIndex());
					else
					{
						ClientPrint(pEntity, HUD_PRINTCONSOLE, "no such bot in game\n");
						PlaySoundConfirmation(pEntity, SND_FAILED);
					}
				}
				else
				{
					sprintf(msg, "Valid value is 1 - %d\n", MAX_CLIENTS);
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
					PlaySoundConfirmation(pEntity, SND_FAILED);
				}
			}
		}
		else
		{
			if (bots[0].is_used)
			{
				PrintBasicBotInfo(pEntity, 0);
			}
			else
			{
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "no bot in game\n");
				PlaySoundConfirmation(pEntity, SND_FAILED);
			}

		}

		return true;
	}
	// print the waypoint system version
	else if (conInput.IsCommand(pcmd, "getwptsystem", "get_wpt_system"))
	{
		int version = wptser.GetWaypointsSystemVersion();

		if (version == -1)
			sprintf(msg, "file doesn't exist or not a MB waypoint file!\n");
		else
			sprintf(msg, "this waypoint file uses waypoint system version %d.0\ncurrent waypoint system is in version %d.0\n", version, WAYPOINT_VERSION);

		ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);

		return true;
	}
	else if (conInput.IsCommand(pcmd, "gettexturename", "get_texture_name"))
	{
		UTIL_MakeVectors(pEntity->v.v_angle);

		Vector vecStart = pEntity->v.origin + pEntity->v.view_ofs;
		Vector vecEnd = vecStart + gpGlobals->v_forward * 100;

		TraceResult tr;
		UTIL_TraceLine(vecStart, vecEnd, dont_ignore_monsters, dont_ignore_glass, pEntity, &tr);

		const char* texture = g_engfuncs.pfnTraceTexture(tr.pHit, vecStart, vecEnd);

		if (texture == NULL)
			sprintf(msg, "unable to get the texture name or you are too far!\n");
		else
			sprintf(msg, "the name of the texture in front is \"%s\"\n", texture);
		ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

		return true;
	}
	else if (conInput.IsCommand(pcmd, "changestartpositions", "change_start_positions"))
	{
		ChangeStarPositionsCommand(pEntity, arg1);

		return true;
	}
	else if (conInput.IsCommand(pcmd, "changecaptureareas", "change_capture_areas"))
	{
		ChangeCaptureAreasCommand(pEntity, arg1);

		return true;
	}
	else if (conInput.IsCommand(pcmd, "generatecaptureareasfile", "generate_capture_areas_file", "buildcapareas"))
	{
		if (internals.IsBuildCaptureAreasFile() || FStrEq(arg1, "off"))
		{
			internals.ResetBuildCaptureAreasFile();
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "DISABLED!\n", conInput.GetCmdName());
		}
		else
		{
			internals.SetBuildCaptureAreasFile(true);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "ENABLED!\n", conInput.GetCmdName());
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "***TIP: the data for the file will be gathered while map loads so use 'restart' command now***\n");
		}

		return true;
	}
	else if (conInput.IsCommand(pcmd, "checkcaptureareas", "check_capture_areas", "checkcapareas"))
	{
		if ((arg1 != NULL) && (*arg1 != 0))
		{
			if (FStrEq(arg1, "all") || FStrEq(arg1, "used"))
			{
				for (int i = 0; i < MAX_CAPTUREPOINTS; i++)
				{
					if (FStrEq(arg1, "used") && (strcmp(dodCaptureArea->GetPointName(i), "dod_carea_name") == 0))
						continue;

					sprintf(msg, "CaptureAreas slot [%d] objective name is: <%s>\n", i + 1, dodCaptureArea->GetPointName(i));
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
					if (dodCaptureArea->GetPointObjListIndex(i) < 0)
						sprintf(msg, "CaptureAreas slot [%d] is NOT on the list of objectives at top left\n", i + 1);
					else
						sprintf(msg, "CaptureAreas slot [%d] position on top left list of objectives is: <%d>\n", i + 1, dodCaptureArea->GetPointObjListIndex(i) + 1);
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
					if (ControlPoints->FindPointByLinkName(dodCaptureArea->GetPointName(i)) != CAPTUREPOINTS_ERROR_VAL)
					{
						sprintf(msg, "CaptureAreas slot [%d] is linked to Control Point named: <%s>\n", i + 1, ControlPoints->GetPointName(ControlPoints->FindPointByLinkName(dodCaptureArea->GetPointName(i))));
						ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
					}
					sprintf(msg, "CaptureAreas slot [%d] objective is owned by: <%d> (0=neither team, 1=allies, 2=axis)\n", i + 1, dodCaptureArea->GetOwnedByTeam(i));
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
				}

				if (FStrEq(arg1, "used"))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "\n***If you see no data then there are no Capture Areas used on this map.***\n");
			}
			else
			{
				if (conInput.IsValidArrayIndex(arg1, MAX_CAPTUREPOINTS))
				{
					sprintf(msg, "CaptureAreas - objective name is: <%s>\n", dodCaptureArea->GetPointName(conInput.GetValidIndex()));
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
					if (dodCaptureArea->GetPointObjListIndex(conInput.GetValidIndex()) < 0)
						ClientPrint(pEntity, HUD_PRINTCONSOLE, "CaptureAreas - this one is NOT on the list of objectives at top left\n");
					else
					{
						sprintf(msg, "CaptureAreas - position on top left list of objectives is: <%d>\n", dodCaptureArea->GetPointObjListIndex(conInput.GetValidIndex()) + 1);
						ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
					}
					if (ControlPoints->FindPointByLinkName(dodCaptureArea->GetPointName(conInput.GetValidIndex())) != CAPTUREPOINTS_ERROR_VAL)
					{
						sprintf(msg, "CaptureAreas - is linked to Control Point named: <%s>\n",
							ControlPoints->GetPointName(ControlPoints->FindPointByLinkName(dodCaptureArea->GetPointName(conInput.GetValidIndex()))));
						ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
					}
					if (dodCaptureArea->GetDodObjectRequired(conInput.GetValidIndex()))
						ClientPrint(pEntity, HUD_PRINTCONSOLE, "CaptureAreas - objective requires some object or explosives charge to capture successfully\n");
					else
						ClientPrint(pEntity, HUD_PRINTCONSOLE, "CaptureAreas - objective doesn't require any object or explosives charge, it's a standard flag type capture area\n");
					sprintf(msg, "CaptureAreas - objective is owned by: <%d> (0=neither team, 1=allies, 2=axis)\n", dodCaptureArea->GetOwnedByTeam(conInput.GetValidIndex()));
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
					sprintf(msg, "CaptureAreas - objective allows to be captured by %s? <%d> (0=no, 1=yes)\n", teamONE.GetTeamName2wordsLC(), dodCaptureArea->GetTeamOneAllowedToCapture(conInput.GetValidIndex()));
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
					sprintf(msg, "CaptureAreas - objective needs <%d> %s member/s to capture\n", dodCaptureArea->GetTeamOnePlayersToCapture(conInput.GetValidIndex()), teamONE.GetTeamName2wordsLC());
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
					sprintf(msg, "CaptureAreas - objective currently has <%d> %s member/s present\n", dodCaptureArea->GetTeamOnePlayersCurrPresent(conInput.GetValidIndex()), teamONE.GetTeamName2wordsLC());
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
					sprintf(msg, "CaptureAreas - objective allows to be captured by %s? <%d> (0=no, 1=yes)\n", teamTWO.GetTeamName2wordsLC(), dodCaptureArea->GetTeamTwoAllowedToCapture(conInput.GetValidIndex()));
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
					sprintf(msg, "CaptureAreas - objective needs <%d> %s member/s to capture\n", dodCaptureArea->GetTeamTwoPlayersToCapture(conInput.GetValidIndex()), teamTWO.GetTeamName2wordsLC());
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
					sprintf(msg, "CaptureAreas - objective currently has <%d> %s member/s present\n", dodCaptureArea->GetTeamTwoPlayersCurrPresent(conInput.GetValidIndex()), teamTWO.GetTeamName2wordsLC());
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
					sprintf(msg, "CaptureAreas - objective needs <%.1f> second/s to finish the capture\n", dodCaptureArea->GetTimeToCapture(conInput.GetValidIndex()));
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
				
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "\n***The currently present team members show valid data only on multi-man areas.***\n");
				}
				else
				{
					sprintf(msg, "Valid value is 1 - %d\n", MAX_CAPTUREPOINTS);
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
					PlaySoundConfirmation(pEntity, SND_FAILED);
				}
			}
		}
		else
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "Write number of the array slot to see its data or write 'all' to see whole array or 'used' to see just the found ones.\n");

		return true;
	}
	else if (conInput.IsCommand(pcmd, "checkcontrolpoints", "check_control_points", "checkconpoints"))
	{
		if ((arg1 != NULL) && (*arg1 != 0))
		{
			if (FStrEq(arg1, "all") || FStrEq(arg1, "used"))
			{
				for (int i = 0; i < MAX_CAPTUREPOINTS; i++)
				{
					if (FStrEq(arg1, "used") && (strcmp(ControlPoints->GetPointName(i), "dod_cpoint_name") == 0))
						continue;

					sprintf(msg, "ControlPoints slot [%d] objective name is: <%s>\n", i + 1, ControlPoints->GetPointName(i));
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
					if (ControlPoints->GetPointObjListIndex(i) < 0)
						sprintf(msg, "ControlPoints slot [%d] is NOT on the list of objectives at top left\n", i + 1);
					else
						sprintf(msg, "ControlPoints slot [%d] position on top left list of objectives is: <%d>\n", i + 1, ControlPoints->GetPointObjListIndex(i) + 1);
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
					sprintf(msg, "ControlPoints slot [%d] point is owned by: <%d> (0=neither team, 1=allies, 2=axis)\n", i + 1, ControlPoints->GetOwnedByTeam(i));
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
				}
			}
			else
			{
				if (conInput.IsValidArrayIndex(arg1, MAX_CAPTUREPOINTS))
				{
					sprintf(msg, "ControlPoints - objective name is: <%s>\n", ControlPoints->GetPointName(conInput.GetValidIndex()));
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
					if (ControlPoints->GetPointObjListIndex(conInput.GetValidIndex()) < 0)
						ClientPrint(pEntity, HUD_PRINTCONSOLE, "ControlPoints - this one is NOT on the list of objectives at top left\n");
					else
					{
						sprintf(msg, "ControlPoints - position on top left list of objectives is: <%d>\n", ControlPoints->GetPointObjListIndex(conInput.GetValidIndex()) + 1);
						ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
					}
					sprintf(msg, "ControlPoints - objective is owned by: <%d> (0=neither team, 1=allies, 2=axis)\n", ControlPoints->GetOwnedByTeam(conInput.GetValidIndex()));
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
					sprintf(msg, "ControlPoints - objective allows to be captured by %s? <%d> (0=no, 1=yes)\n", teamONE.GetTeamName2wordsLC(), ControlPoints->GetTeamOneAllowedToCapture(conInput.GetValidIndex()));
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
					sprintf(msg, "ControlPoints - objective allows to be captured by %s? <%d> (0=no, 1=yes)\n", teamTWO.GetTeamName2wordsLC(), ControlPoints->GetTeamTwoAllowedToCapture(conInput.GetValidIndex()));
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
					sprintf(msg, "ControlPoints - objective origin (position on the map) is: x=<%.1f> y=<%.1f> z=<%.1f>\n",
						ControlPoints->GetPointOrigin(conInput.GetValidIndex()).x, ControlPoints->GetPointOrigin(conInput.GetValidIndex()).y, ControlPoints->GetPointOrigin(conInput.GetValidIndex()).z);
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
				}
				else
				{
					sprintf(msg, "Valid value is 1 - %d\n", MAX_CAPTUREPOINTS);
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
					PlaySoundConfirmation(pEntity, SND_FAILED);
				}
			}
		}
		else
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "Write number of the array slot to see its data or write 'all' to see whole array or 'used' to see just the found ones.\n");

		return true;
	}
	else if (conInput.IsCommand(pcmd, "fixparticlemanagercrash", "fix_particle_manager_crash", "fixparticlemancrash"))
	{
		if (internals.IsFixParticleManagerCrash() || FStrEq(arg1, "off"))
		{
			internals.ResetIsFixParticleManagerCrash();
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "DISABLED!\n", conInput.GetCmdName());
		}
		else
		{
			internals.SetIsFixParticleManagerCrash(true);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "ENABLED!\n", conInput.GetCmdName());
		}

		return true;
	}
	/*/
	else if (conInput.IsCommand(pcmd, "testparticlemanfix"))//																TODO: ERASE THIS - it's here only to test this feature
	{
		if (internals.IsFixParticleManagerCrash())
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "is currently ENABLED!\n", conInput.GetCmdName());
			else
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "is currently DISABLED!\n", conInput.GetCmdName());

		return true;
	}
	/**/
	else if (FStrEq(pcmd, "debug"))
	{
		if (FStrEq(arg1, "help") || FStrEq(arg1, "?"))
		{
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "\nAll valid debugging commands you can use\n***TIP: best results are when used with just one bot in game!***\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "--------------------------------\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[debug_actions] prints info about actions the bot does\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[debug_aim_targets] prints valid aim indexes\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[debug_cross] prints details how is bot deciding at cross waypoint\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[debug_paths] prints info about path the bot is following\n");
			//ClientPrint(pEntity, HUD_PRINTCONSOLE, "[debug_stuck] prints info when the bot gets stuck\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[debug_waypoints] prints info about waypoints the bot is heading towards (when not following a path)\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[debug_weapons] prints all weapon indexes the bot spawned with\n");
			//ClientPrint(pEntity, HUD_PRINTCONSOLE, "[] \n");

#ifdef _DEBUG
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[debug_menu] prints info about current state of the botmenu\n");
			//ClientPrint(pEntity, HUD_PRINTCONSOLE, "[debug_engine] \n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[debug_stance] prints info when the bot changes his stance\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[debug_stuck] prints info when the bot gets stuck\n");
#endif
		}

		return true;
	}
	else if (conInput.IsCommand(pcmd, "debugactions", "debug_actions"))
	{
		if (botdebugger.IsDebugActions() || FStrEq(arg1, "off"))
		{
			botdebugger.ResetDebugActions();
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "DISABLED!\n", conInput.GetCmdName());
		}
		else
		{
			botdebugger.SetDebugActions(true);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "ENABLED!\n", conInput.GetCmdName());
		}

		return true;
	}
	else if (conInput.IsCommand(pcmd, "debugaimtargets", "debug_aim_targets"))
	{
		if (botdebugger.IsDebugAims() || FStrEq(arg1, "off"))
		{
			botdebugger.ResetDebugAims();
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "DISABLED!\n", conInput.GetCmdName());
		}
		else
		{
			botdebugger.SetDebugAims(true);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "ENABLED!\n", conInput.GetCmdName());
		}

		return true;
	}
	else if (conInput.IsCommand(pcmd, "debugcross", "debug_cross"))
	{
		if (botdebugger.IsDebugCross() || FStrEq(arg1, "off"))
		{
			botdebugger.ResetDebugCross();
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "DISABLED!\n", conInput.GetCmdName());
		}
		else
		{
			botdebugger.SetDebugCross(true);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "ENABLED!\n", conInput.GetCmdName());
		}

		return true;
	}
	else if (conInput.IsCommand(pcmd, "debugpaths", "debug_paths"))
	{
		if (botdebugger.IsDebugPaths() || FStrEq(arg1, "off"))
		{
			botdebugger.ResetDebugPaths();
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "DISABLED!\n", conInput.GetCmdName());
		}
		else
		{
			botdebugger.SetDebugPaths(true);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "ENABLED!\n", conInput.GetCmdName());
		}

		return true;
	}
	else if (conInput.IsCommand(pcmd, "debugstance", "debug_stance"))
	{
		if (botdebugger.IsDebugStance() || FStrEq(arg1, "off"))
		{
			botdebugger.ResetDebugStance();
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "DISABLED!\n", conInput.GetCmdName());
		}
		else
		{
			botdebugger.SetDebugStance(true);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "ENABLED!\n", conInput.GetCmdName());
		}

		return true;
	}
	else if (conInput.IsCommand(pcmd, "debugstuck", "debug_stuck"))
	{
		if (botdebugger.IsDebugStuck() || FStrEq(arg1, "off"))
		{
			botdebugger.ResetDebugStuck();
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "DISABLED!\n", conInput.GetCmdName());
		}
		else
		{
			botdebugger.SetDebugStuck(true);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "ENABLED!\n", conInput.GetCmdName());
		}

		return true;
	}
	else if (conInput.IsCommand(pcmd, "debugwaypoints", "debug_waypoints"))
	{
		if (botdebugger.IsDebugWaypoints() || FStrEq(arg1, "off"))
		{
			botdebugger.ResetDebugWaypoints();
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "DISABLED!\n", conInput.GetCmdName());
		}
		else
		{
			botdebugger.SetDebugWaypoints(true);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "ENABLED!\n", conInput.GetCmdName());
		}

		return true;
	}
	else if (conInput.IsCommand(pcmd, "debugweapons", "debug_weapons"))
	{
		if (botdebugger.IsDebugWeapons() || FStrEq(arg1, "off"))
		{
			botdebugger.ResetDebugWeapons();
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "DISABLED!\n", conInput.GetCmdName());
		}
		else
		{
			if (FStrEq(arg1, "more"))
				botdebugger.SetDebugWeapons(true, 1);
			else if (FStrEq(arg1, "full"))
				botdebugger.SetDebugWeapons(true, 2);
			else
				botdebugger.SetDebugWeapons(true);

			ClientPrint(pEntity, HUD_PRINTNOTIFY, "ENABLED!\n", conInput.GetCmdName());
		}

		return true;
	}
	else if (FStrEq(pcmd, "test"))
	{
		if (FStrEq(arg1, "help") || FStrEq(arg1, "?"))
		{
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "\nAll valid test commands\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[aimcode] toggles standard/new aim code; the new aim code isn't fully finished\n");
		}
		else if ((FStrEq(arg1, "aimcode")) || (FStrEq(arg1, "aim_code")))
		{
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "This command isn't available in current Marine Bot version!\n");

			/*/
			extern bool g_test_aim_code;

			if (g_test_aim_code)
			{
				g_test_aim_code = false;
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "experimental aim code DISABLED!\n");
			}
			else
			{
				g_test_aim_code = true;
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "experimental aim code ENABLED!\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "This new aim code isn't finished yet so you may experience a lot of misses and buggy behaviour!\n");
			}
			/**/
		}
		else
		{
			conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "***TIP: use 'test help' for help***\n");
		}

		return true;
	}
	else if (FStrEq(pcmd, "wpt"))
	{
		if (FStrEq(arg1, "help") || FStrEq(arg1, "?"))
		{
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "\nAll waypoint commands\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[types] for help with the different types of waypoint\t\t[commands] for a list of all the commands you can use on a waypoint\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "--------------------------------\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[on] shows waypoints\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[off] hides waypoints\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[show] toggles show/hide waypoints\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[autosave] toggles auto saving waypoints & paths to special files\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[save] saves waypoints & paths to appropriate files\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[load] loads waypoints & paths from HDD\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[load <name>] loads waypoints & paths from specified 'name' files\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[loadunsupported] loads older waypoints & paths from HDD (it's usually done automatically at map start)\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[loadunsupportedversion6] loads old waypoints & paths made for MB0.9b (i.e. waypoint system version 6)\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[loadfirearmsversion <name>] loads and converts waypoints & paths created for Firearms version of 'name' map\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[author] shows waypoints creator signature\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[subscribe <name>] adds name (up to 32 chars - no spaces) signature to waypoints\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[modified <name>] adds name (up to 32 chars - no spaces) signature to waypoints\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[count] info about total amounts of waypoints\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[clear] deletes all waypoints (can be still loaded back)\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[destroy] deletes all waypoints & prepares for new creation (can NOT be loaded back)\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[displaytime <num>] sets the time for all waypoints, ranges & aim/cross connections to be redrawn on screen\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[drawdistance <num>] sets the max distance the waypoint can be to show on screen\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[setdefaultrange <num>] sets the default waypoint range for newly added waypoints (0-400)\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[printall] prints all used waypoints and their flags; repeat this command to list through them\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[checkproblems <save>] prints problems with waypoints (use 'wpt checkproblems help' for more info)\n");
		}
		else if (FStrEq(arg1, "types"))
		{
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "\nAll valid waypoint types\n--------------------------------\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[normal] default wpt.................[aim] aiming vector\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[ammobox] ammobox use.........[claymore] plant mine\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[cross] crossroad.....................[crouch] duck moves\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[door] autoopen doors..............[dooruse] manuallyopen doors\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[flag] pushpoint position............[goback] turnback&continue\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[jump] jump forward................[duckjump] jump&crouch\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[ladder] ladder down&top.........[parachute] check&jump out\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[prone] go prone......................[roadblock] check forward\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[shoot] fire weapon..................[sniper] don't move&snipe\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[sprint] sprinting.......................[trigger] dynamic gate\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[use] use something\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "\n***TIP: Some of these types can be combined together.***\n");
		}
		else if (FStrEq(arg1, "commands"))
		{
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "\nList of all the commands you can use on a waypoint\n--------------------------------\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[add <type>] adds 'type' waypoint\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[delete] deletes close waypoint\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[change <arg1> <arg2>] changes close or index waypoint to specified type (both args can work as either index or type)\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[setpriority <num> <team>] changes 'team' (not specified=both) wpt priority to 'num' (0-5) where 0=no, 1=highest, 5=lowest\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[settime <num> <team>] changes 'team' (not specified=both) wpt time to 'num' (0-600)\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[setrange <num>] sets waypoint range (0-400)\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[rangeincrease <num>] increases waypoint range by given 'num' (0-50), where 0=use of dynamic values 5 or 10\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[rangedecrease <num>] decreases waypoint range, same rules as above\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[move <index> <dir> <num>] moves nearby or specified waypoint to player position or in one 'direction' by 'num' (1-400) units\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[reset <arg1> <arg2> <arg3> <arg4>] resets values specified in args back to default (use 'wpt reset help' for more info)\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[info <arg1> <arg2>] info about close or 'arg' waypoint; 'more' additional info; 'full' all info\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[compass <arg>] shows rapidly blinking beam that'll lead you to 'arg' waypoint; 'off' will turn it off\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[detect <index>] prints all entities like buttons, bandages etc. that are around 'index' waypoint\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[position <index>] prints the position of 'index' waypoint and yours too\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[repair <arg1> <arg2>] automatic waypoint repair tools (use 'wpt repair help' for more info)\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[triggerevent <arg1> <arg2> <arg3>] allows dynamic priority gating (use 'wpt triggerevent help' for more info)\n");
		}
		else if (FStrEq(arg1, "on"))
		{
			wptser.SetShowWaypoints(true);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoints are ON\n");
		}
		else if (FStrEq(arg1, "off"))
		{
			wptser.ResetShowWaypoints();
			// turn paths off too (we don't want to see path beams when waypoints are hidden)
			wptser.ResetShowPaths();

			ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoints are OFF\n");
		}
		else if (FStrEq(arg1, "show"))
		{
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "***toggle mode used***\n");

			if (wptser.IsShowWaypoints())
			{
				wptser.ResetShowWaypoints();
				wptser.ResetShowPaths();
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoints are OFF\n");
			}
			else
			{
				wptser.SetShowWaypoints(true);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoints are ON\n");
			}
		}
		else if (FStrEq(arg1, "autosave"))		// store waypoint data to the file
		{
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "***toggle mode used***\n");
			
			if (internals.IsWaypointsAutoSave())
			{
				internals.ResetWaypointsAutoSave();
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "auto saving waypoints DISABLED\n");

			}
			else
			{
				internals.SetWaypoitsAutoSave(true);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "auto saving waypoints ENABLED\n");
			}
		}
		else if (FStrEq(arg1, "save"))		// store waypoint data to the file
		{
			if (wpteditor.SaveWaypoints(NULL))
			{
				if (patheditor.SavePaths(NULL))
				{
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoints and paths were successfully saved\n");
					PlaySoundConfirmation(pEntity, SND_DONE);
				}
				// there was some error - paths weren't saved
				else
				{
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "some error occurred - only waypoints were saved successfully!\n");
					PlaySoundConfirmation(pEntity, SND_FAILED);
				}
			}
			// there was some error - waypoints as well as paths weren't saved
			else
			{
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "some error occurred - waypoints weren't saved!\n");
				PlaySoundConfirmation(pEntity, SND_FAILED);
			}
		}
		else if (FStrEq(arg1, "load"))		// load waypoints
		{
			if ((arg2 != NULL) && (*arg2 != 0))
			{
				char temp[64];

				strcpy(temp, arg2);

				if (wpteditor.LoadWaypoints(pEntity, temp) > 0)
				{
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoints loaded\n");

					if (patheditor.LoadPaths(pEntity, temp) > 0)
					{
						ClientPrint(pEntity, HUD_PRINTNOTIFY, "paths loaded\n");
						PlaySoundConfirmation(pEntity, SND_DONE);
					}
					else
					{
						ClientPrint(pEntity, HUD_PRINTNOTIFY, "no paths or invalid path file\n");
						PlaySoundConfirmation(pEntity, SND_FAILED);
					}
				}
			}
			else
			{
				if (wpteditor.LoadWaypoints(pEntity, NULL) > 0)
				{
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoints loaded\n");

					if (patheditor.LoadPaths(pEntity, NULL) > 0)
					{
						ClientPrint(pEntity, HUD_PRINTNOTIFY, "paths loaded\n");
						PlaySoundConfirmation(pEntity, SND_DONE);
					}
					else
					{
						ClientPrint(pEntity, HUD_PRINTNOTIFY, "no paths or invalid path file\n");
						PlaySoundConfirmation(pEntity, SND_FAILED);
					}
				}
			}
		}
		else if (FStrEq(arg1, "loadunsupported"))	// convert old waypoint file to new standards
		{
			if (wpteditor.LoadUnsupportedWaypoints(pEntity))
			{
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "older waypoints data loaded\n");

				if (patheditor.LoadUnsupportedPaths(pEntity))
				{
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "older paths data loaded\n");
					PlaySoundConfirmation(pEntity, SND_DONE);
				}
				else
				{
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "older paths weren't loaded\n");
					PlaySoundConfirmation(pEntity, SND_FAILED);
				}
			}
			else
			{
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "older waypoints weren't loaded\n");
				PlaySoundConfirmation(pEntity, SND_FAILED);
			}
		}
		// convert waypoints made in version 6 ... waypoint system in MB0.9b
		else if (conInput.IsCommand(arg1, "loadunsupportedver6", "loadunsupportedversion6"))
		{
			if (wpteditor.LoadUnsupportedWaypointsVersion6(pEntity))
			{
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "old waypoints data loaded\n");

				if (patheditor.LoadUnsupportedPathsVersion6(pEntity))
				{
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "old paths data loaded\n");
					PlaySoundConfirmation(pEntity, SND_DONE);
				}
				else
				{
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "old paths weren't loaded\n");
					PlaySoundConfirmation(pEntity, SND_FAILED);
				}
			}
			else
			{
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "old waypoints weren't loaded\n");
				PlaySoundConfirmation(pEntity, SND_FAILED);
			}
		}
		else if (conInput.IsCommand(arg1, "loadfirearmsversion", "loadfaver"))	// convert waypoints made in Firearms mod
		{
			if ((arg2 != NULL) && (*arg2 != 0))
			{
				char temp[64]{};

				strcpy(temp, arg2);

				if (wpteditor.LoadFirearmsWaypoints(pEntity, temp))
				{
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "Firearms waypoints data loaded\n");

					if (patheditor.LoadFirearmsPaths(pEntity, temp))
					{
						ClientPrint(pEntity, HUD_PRINTNOTIFY, "Firearms paths data loaded\n");
						PlaySoundConfirmation(pEntity, SND_DONE);
					}
					else
					{
						ClientPrint(pEntity, HUD_PRINTNOTIFY, "Firearms paths weren't loaded\n");
						PlaySoundConfirmation(pEntity, SND_FAILED);
					}
				}
				else
				{
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "Firearms waypoints weren't loaded\n");
					PlaySoundConfirmation(pEntity, SND_FAILED);
				}
			}
			else
				conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);
		}
		else if (FStrEq(arg1, "author"))	// print waypoints author
		{
			char author[32];
			char modified_by[32];

			wptser.PrintWaypointsAuthors(author, modified_by);

			if (FStrEq(author, "nofile"))
			{
				PlaySoundConfirmation(pEntity, SND_FAILED);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "no waypoint file!\n");
			}
			else
			{
				if (FStrEq(author, "noauthor") && FStrEq(modified_by, "nosig"))
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoints aren't subscribed!\n");
				else if (FStrEq(author, "noauthor") && !FStrEq(modified_by, "nosig"))
				{
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoints author is unknown!\n");

					sprintf(msg, "waypoints were modified by %s\n", modified_by);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
				}
				else if (FStrEq(modified_by, "nosig"))
				{
					if (util.IsOfficialWaypoints(author))
						sprintf(msg, "official MarineBot waypoints by %s\n", author);
					else
						sprintf(msg, "waypoints were created by %s\n", author);

					// display it also on HUD using same style as MB intro messages
					Vector color1 = Vector(200, 50, 0);
					Vector color2 = Vector(0, 250, 0);
					CustHudMessage(pEntity, msg, color1, color2, 2, 8);

					ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
				}
				else
				{
					if (util.IsOfficialWaypoints(author) || util.IsOfficialWaypoints(modified_by))
						sprintf(msg, "official MarineBot waypoints by %s\nmodified by %s\n", author, modified_by);
					else
						sprintf(msg, "waypoints by %s\nmodified by %s\n", author, modified_by);

					Vector color1 = Vector(200, 50, 0);
					Vector color2 = Vector(0, 250, 0);
					CustHudMessage(pEntity, msg, color1, color2, 2, 8);

					ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
				}
			}
		}
		else if (FStrEq(arg1, "subscribe"))		// subscribe waypoint file
		{
			if ((arg2) && (*arg2))
			{
				char temp[32];

				strncpy(temp, arg2, 31);
				temp[31] = 0;

				if (wpteditor.Subscribe(temp, true))
				{
					wpteditor.SaveWaypoints(NULL);		// save them

					// are there any paths then save them too
					if (num_w_paths > 0)
						patheditor.SavePaths(NULL);

					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoints subscription SUCCESSFUL\n");
				}
				else
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoints subscription FAILED (probably already subsribed or waypoint file doesn't exist yet)\n");
				}
			}
			else
			{
				conOutput.PrintErrorMessage(conOutErrMsg::mis_arg, pEntity);
			}
		}
		else if (FStrEq(arg1, "modified"))	// subscribe modified waypoint file
		{
			if ((arg2) && (*arg2))
			{
				if ((FStrEq(arg2, "help")) || (FStrEq(arg2, "?")))
				{
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "\nAll valid options\n---------------------------\n");
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "[clear] removes/erases current 'modified by' tag from waypoint file\n");
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "[fill_your_name_here] assigns given name to 'modified by' tag in waypoint file\n");
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "[help] shows this help\n");
				}
				else
				{
					char temp[32];

					strncpy(temp, arg2, 31);
					temp[31] = 0;

					if (wpteditor.Subscribe(temp, false))
					{
						wpteditor.SaveWaypoints(NULL);		// save them

						if (num_w_paths > 0)
							patheditor.SavePaths(NULL);

						PlaySoundConfirmation(pEntity, SND_DONE);
						ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoints subscription SUCCESSFUL\n");
					}
					else
					{
						PlaySoundConfirmation(pEntity, SND_FAILED);
						ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoints subscription FAILED\n");
					}
				}
			}
			else
			{
				conOutput.PrintErrorMessage(conOutErrMsg::mis_arg, pEntity);
			}
		}
		else if (FStrEq(arg1, "count"))			// print number of available wpts
		{
			sprintf(msg, "waypoints already used= %d still could use= %d out of %d\n", num_waypoints, MAX_WAYPOINTS - num_waypoints, MAX_WAYPOINTS);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
		}
		else if (FStrEq(arg1, "clear"))		// delete all waypoints
		{
			wpteditor.InitAll();

			ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoints cleared\n");
		}
		else if (FStrEq(arg1, "destroy"))	// delete all waypoints & free the file
		{
			wpteditor.WipeAll();

			ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoints destroyed\n");
		}
		else if (conInput.IsCommand(arg1, "displaytime", "display_time"))	// change display time
		{
			if (conInput.IsValidFloatValue(arg2, 0.2f, 5.0f))
			{
				wptser.SetWaypointsDisplayTime(conInput.GetFloatValue());

				PlaySoundConfirmation(pEntity, SND_DONE);
				sprintf(msg, "waypoints display time set to %.1f\n", wptser.GetWaypointsDisplayTime());
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
			else
			{
				PlaySoundConfirmation(pEntity, SND_FAILED);
				conOutput.PrintErrorMessage(wptser.GetWaypointsDisplayTime(), pEntity);
			}
		}
		else if (conInput.IsCommand(arg1, "drawdistance", "draw_distance"))	// change draw distance
		{
			if (conInput.IsValidFloatValue(arg2, 100.0f, 1400.0f))
			{
				wptser.SetWaypointsDrawDistance(conInput.GetFloatValue());

				PlaySoundConfirmation(pEntity, SND_DONE);
				sprintf(msg, "waypoints draw distance set to %.1f\n", wptser.GetWaypointsDrawDistance());
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
			else
			{
				PlaySoundConfirmation(pEntity, SND_FAILED);
				conOutput.PrintErrorMessage(wptser.GetWaypointsDrawDistance(), pEntity);
			}
		}
		else if (FStrEq(arg1, "setdefaultrange"))	// user defined default range for newly added waypoints
		{
			if (FStrEq(arg2, "current"))
			{
				sprintf(msg, "current default waypoint range is %.1f\n", internals.GetCustomDefaultWaypointRange());
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
			else if (FStrEq(arg2, "reset"))
			{
				internals.ResetCustomDefaultWaypointRange();

				PlaySoundConfirmation(pEntity, SND_DONE);
				sprintf(msg, "default waypoint range reset to %.1f\n", internals.GetCustomDefaultWaypointRange());
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
			else if (conInput.IsValidFloatValue(arg2, 0.0f, MAX_WPT_DIST))
			{
				internals.SetCustomDefaultWaypointRange(conInput.GetFloatValue());

				PlaySoundConfirmation(pEntity, SND_DONE);
				sprintf(msg, "custom value for default waypoint range set to %.1f\n", internals.GetCustomDefaultWaypointRange());
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
			else
				conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);
		}
		else if (FStrEq(arg1, "printall"))
		{
			wptser.PrintAllWaypoints(pEntity);
		}
		else if (conInput.IsCommand(arg1, "checkproblems", "checkforproblems"))
		{
			if (FStrEq(arg2, "help") || FStrEq(arg2, "?"))
			{
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[bugsonly] will print only findings marked as a bug\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[save] will write all findings in MB error log file\n");
			}
			else
			{
				if (FStrEq(arg2, "save"))
					wptfixer.CheckWaypointsForProblems(true);
				else
				{
					if (FStrEq(arg2, "bugsonly") || FStrEq(arg2, "errorsonly"))
						wptoutputer.SetPrintErrorsOnly();
					else
						wptoutputer.ResetPrintErrorsOnly();

					wptfixer.CheckWaypointsForProblems(false);
				}

				if (wptoutputer.GetAmountOfFoundIssues() == 0)
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "all waypoints successfully passed\n");
				}
				else
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);

					sprintf(msg, "total of %d bugs/possible problems have been found\n", wptoutputer.GetAmountOfFoundIssues());
					ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

					// if there're more issues than what the console can display then print a simple note so that the user knows about it
					if (wptoutputer.CanStillPrintIt() == false)
					{
						ClientPrint(pEntity, HUD_PRINTCONSOLE, "There are more errors. Fix the listed ones and repeat the command to see them.\n");
					}

					ClientPrint(pEntity, HUD_PRINTCONSOLE, "***TIP: use 'wpt compass' to locate the problem waypoints***\n");
				}
			}

			ClientPrint(pEntity, HUD_PRINTCONSOLE, "***TIP: use 'pathwpt checkproblems' to check for paths related problems***\n");
		}
		else if (FStrEq(arg1, "add"))	// place a new waypoint
		{
			int result = NO_VAL;

			if (conInput.IsValidKeyWord(arg2, true))
			{
				// if this function doesn't return any error and the waypoints aren't visible yet then they will automatically show
				result = wpteditor.Add(pEntity, conInput.GetKeyWord());
			}

			if (result == NO_VAL)
			{
				conOutput.PrintErrorMessage(conOutErrMsg::inv_arg, pEntity);
			}
			else if (result == -2)
			{
				// this is the only error where we need to display the waypoints if they are still hidden
				wptser.SetShowWaypoints(true);

				ClientPrint(pEntity, HUD_PRINTNOTIFY, "there already is a waypoint at this place!\nplace your new waypoint a few steps away\n");
			}
			else if (result == -3)
			{
				PlaySoundConfirmation(pEntity, SND_FAILED);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "ERROR - too many waypoints\n");
			}
			else
			{
				// we must handle the no argument case here
				if (conInput.IsMissingArgument())
					sprintf(msg, "'normal' waypoint added SUCCESSFULLY\n");
				else
					sprintf(msg, "'%s' waypoint added SUCCESSFULLY\n", conInput.GetKeyWord());

				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
		}
		else if (FStrEq(arg1, "delete"))	// remove waypoint
		{
			if (wptser.IsShowWaypoints() == false)
				wptser.SetShowWaypoints(true);

			wpteditor.Delete(pEntity);
		}
		else if (conInput.IsCommand(arg1, "change"))	// change waypoint flag (or type if you want)
		{
			int result;

			conInput.ProcessWaypointInput(arg2, arg3);

#ifdef DEBUG
			//ALERT(at_console, "AfterPocessing: KeyWord=<%s> | Index=<%d>\n", conInput.GetKeyWord(), conInput.PrintValidIndex());
#endif // DEBUG


			result = wpteditor.ChangeType(pEntity, conInput.GetKeyWord(), conInput.GetValidIndex());

			if (result == NO_VAL)
			{
				conOutput.PrintErrorMessage(conOutErrMsg::no_wpt_nrb, pEntity);
			}
			else if (result == -2)
			{
				conOutput.PrintErrorMessage(conOutErrMsg::inv_wpt, pEntity);
			}
			else if (result == -3)
			{
				PlaySoundConfirmation(pEntity, SND_FAILED);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "missing argument or unknown waypoint type!\n");
			}
			else if (result == -4)
			{
				PlaySoundConfirmation(pEntity, SND_FAILED);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "invalid combination of waypoint types!\n");
			}
			else if (result == -5)
			{
				PlaySoundConfirmation(pEntity, SND_DONE);
				sprintf(msg, "changed to '%s' waypoint\n", conInput.GetKeyWord());
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
			else if (result == -6)
			{
				PlaySoundConfirmation(pEntity, SND_DONE);
				sprintf(msg, "'%s' flag was successfully removed\n", conInput.GetKeyWord());
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
			else
			{
				PlaySoundConfirmation(pEntity, SND_DONE);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "unknown error\n");
			}
		}
		else if (FStrEq(arg1, "setpriority"))	// change waypoint priority
		{
			int result;

			result = wpteditor.ChangePriority(pEntity, arg2, arg3);

			if (result == -4)
			{
				conOutput.PrintErrorMessage(conOutErrMsg::no_wpt_nrb, pEntity);
			}
			else if (result == -3)
			{
				PlaySoundConfirmation(pEntity, SND_FAILED);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "invalid priority value (0-5)!\n");
			}
			else if (result == -2)
			{
				conOutput.PrintErrorMessage(conOutErrMsg::inv_team_val, pEntity);
			}
			else if (result == -1)
			{
				PlaySoundConfirmation(pEntity, SND_DONE);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "priority/priorities already is/are on the waypoint\n");
			}
			else if (result == 0)
			{
				PlaySoundConfirmation(pEntity, SND_DONE);

				if ((arg3 != NULL) && (*arg3 != 0))
					sprintf(msg, "priority changed to NO priority!\n");
				else
					sprintf(msg, "both priorities changed to NO priority!\nWARNING - this waypoint is ignored by all bots!\n");

				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
			else if (result > 10)
			{
				PlaySoundConfirmation(pEntity, SND_DONE);
				sprintf(msg, "both priorities set to %d\n", result - 10);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
			else if (result == -100)
			{
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "unknown event in 'setpriority'\n");
			}
			else
			{
				PlaySoundConfirmation(pEntity, SND_DONE);

				if ((arg3 != NULL) && (*arg3 != 0))
					sprintf(msg, "priority changed to %d\n", result);
				else
					sprintf(msg, "both priorities changed to %d\n", result);

				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
		}
		else if (FStrEq(arg1, "settime"))	// change waypoint time
		{
			float result;

			result = wpteditor.ChangeTime(pEntity, arg2, arg3);

			if (result == -4.0f)
			{
				conOutput.PrintErrorMessage(conOutErrMsg::no_wpt_nrb, pEntity);
			}
			else if (result == -3.0f)
			{
				PlaySoundConfirmation(pEntity, SND_FAILED);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "invalid time value (0-600 seconds)!\n");
			}
			else if (result == -2.0f)
			{
				conOutput.PrintErrorMessage(conOutErrMsg::inv_team_val, pEntity);
			}
			else if (result == -1.0f)
			{
				PlaySoundConfirmation(pEntity, SND_DONE);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "time/times already is/are on the waypoint\n");
			}
			else if (result > 1000.0f)
			{
				PlaySoundConfirmation(pEntity, SND_DONE);
				
				result -= 1000.0f;
				sprintf(msg, "both times set to %.1f\n", result);
				
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
			else if (result == -100.0f)
			{
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "unknown event in 'settime'\n");
			}
			else
			{
				PlaySoundConfirmation(pEntity, SND_DONE);

				if ((arg3 != NULL) && (*arg3 != 0))
					sprintf(msg, "time changed to %.1f\n", result);
				else
					sprintf(msg, "both times changed to %.1f\n", result);

				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
		}
		else if (FStrEq(arg1, "setrange"))	// change waypoint range
		{
			float result;

			result = wpteditor.ChangeRange(pEntity, arg2);

			if (result == -3.0f)
			{
				PlaySoundConfirmation(pEntity, SND_DONE);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "this range already is on the waypoint\n");
			}
			else if (result == -2.0f)
			{
				conOutput.PrintErrorMessage(conOutErrMsg::no_wpt_nrb, pEntity);
			}
			else if (result == -1.0f)
			{
				conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);
			}
			else
			{
				PlaySoundConfirmation(pEntity, SND_DONE);
				sprintf(msg, "waypoint range changed to %.1f\n", result);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
		}
		else if (conInput.IsCommand(arg1, "rangeincrease", "rangeup"))
		{
			float result;

			result = wpteditor.ChangeRangeByConstantValue(pEntity, arg2);

			if (result == -4.0f)
			{
				PlaySoundConfirmation(pEntity, SND_FAILED);
				sprintf(msg, "range cannot exceed %.1f\n", MAX_WPT_DIST);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
			else if (result == -2.0f)
			{
				conOutput.PrintErrorMessage(conOutErrMsg::no_wpt_nrb, pEntity);
			}
			else if (result == -1.0f)
			{
				conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);
			}
			else
			{
				PlaySoundConfirmation(pEntity, SND_DONE);
				sprintf(msg, "waypoint range changed to %.1f\n", result);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
		}
		else if (conInput.IsCommand(arg1, "rangedecrease", "rangedown"))
		{
			float result;

			result = wpteditor.ChangeRangeByConstantValue(pEntity, arg2, true);

			if (result == -3.0f)
			{
				PlaySoundConfirmation(pEntity, SND_FAILED);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "range cannot be negative!\n");
			}
			else if (result == -2.0f)
			{
				conOutput.PrintErrorMessage(conOutErrMsg::no_wpt_nrb, pEntity);
			}
			else if (result == -1.0f)
			{
				conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);
			}
			else
			{
				PlaySoundConfirmation(pEntity, SND_DONE);
				sprintf(msg, "waypoint range changed to %.1f\n", result);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
		}
		else if (FStrEq(arg1, "move"))	// move/reposition waypoint to new position
		{
			if (conInput.IsValidWaypointIndex(arg2, true) && wpteditor.ChangePosition(pEntity, conInput.GetValidIndex(), arg3, arg4))
			{
				PlaySoundConfirmation(pEntity, SND_DONE);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoint successfully moved to new position\n");
			}
			else
			{
				PlaySoundConfirmation(pEntity, SND_FAILED);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "invalid waypoint index or no waypoint close enough or you can't move it or invalid additional argument\n");
			}
		}
		else if (FStrEq(arg1, "reset"))	// reset all waypoint additional info (priority, time etc.)
		{
			if ((FStrEq(arg2, "help")) || (FStrEq(arg2, "?")))
			{
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "All valid reset options\n---------------------------\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[all] resets all values on nearby waypoint\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[type] or [flag] or [tag] resets the waypoint type back to just 'normal' waypoint\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[priority] resets the priority value for both teams\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[time] resets the wait time value for both teams\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[range] resets the range to default value for current waypoint type\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[triggerpriority] resets the trigger priority value for both teams\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[triggerevent] resets both trigger event values (i.e. 'on' as well as 'off')\n");
			}
			else
			{
				int result = wpteditor.ResetData(pEntity, arg2, arg3, arg4, arg5);

				if (result == 1)
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "all or selected waypoint values have been reset back to default\n");
				}
				else if (result == NO_VAL)
					conOutput.PrintErrorMessage(conOutErrMsg::no_wpt_nrb, pEntity);
				else
					conOutput.PrintErrorMessage(conOutErrMsg::inv_arg, pEntity);
			}
		}
		else if (FStrEq(arg1, "info"))
		{
			wptser.PrintWaypointInfo(pEntity, arg2, arg3);
		}
		else if (FStrEq(arg1, "compass"))
		{
			if (wptser.ShowCompass(pEntity, arg2))
				PlaySoundConfirmation(pEntity, SND_DONE);
			else
			{
				// something went wrong so turn it off
				wptser.ResetCompassIndex();

				PlaySoundConfirmation(pEntity, SND_FAILED);
			}
		}
		else if (conInput.IsCommand(arg1, "detect"))	// print all entities around waypoint
		{
			if (conInput.IsValidWaypointIndex(arg2))
			{
				if (wptmanager.IsWaypoint(conInput.GetValidIndex(), WptT::deleted))
				{
					conOutput.PrintErrorMessage(conOutErrMsg::inv_wpt, pEntity);
				}
				else
				{
					edict_t* pent = NULL;
					bool no_entities = true;

					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "Checking entities around ...\n");

					while ((pent = util.FindEntityInSphere(pent, waypoints[conInput.GetValidIndex()].origin, STANDARD_SEARCH_RADIUS)) != NULL)
					{
						no_entities = false;

						sprintf(msg, "found: %s\n", STRING(pent->v.classname));
						ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
					}

					if (no_entities)
					{
						ClientPrint(pEntity, HUD_PRINTNOTIFY, "found nothing!\n");
					}
				}
			}
			else
			{
				conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);
			}
		}
		else if (conInput.IsCommand(arg1, "position", "pos"))	// print waypoint origin
		{
			if (conInput.IsValidWaypointIndex(arg2))
			{
				if (wptmanager.IsWaypoint(conInput.GetValidIndex(), WptT::deleted))
				{
					conOutput.PrintErrorMessage(conOutErrMsg::inv_wpt, pEntity);
				}
				else
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "Waypoint position is:\n");

					sprintf(msg, "X-coord: %.2f \t\t Y-coord: %.2f \t\t Z-coord (height): %.2f \n", waypoints[conInput.GetValidIndex()].origin.x, waypoints[conInput.GetValidIndex()].origin.y,
						waypoints[conInput.GetValidIndex()].origin.z);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

					ClientPrint(pEntity, HUD_PRINTNOTIFY, "Your position is:\n");

					sprintf(msg, "X-coord: %.2f \t\t Y-coord: %.2f \t\t Z-coord (height): %.2f \n", pEntity->v.origin.x, pEntity->v.origin.y, pEntity->v.origin.z);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
				}
			}
			else
			{
				conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);
			}
		}
		else if (FStrEq(arg1, "repair"))	// handle waypoint repair functions
		{
			if ((FStrEq(arg2, "help")) || (FStrEq(arg2, "?")))
			{
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "\nAll waypoint repair commands\n---------------------------\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[masterrepair] applies all available fixes except for range and position and swap teams\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[range <index>] will fix range on all waypoints or on the 'index' waypoint\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[range_and_position <index>] will fix range and position on all waypoints or on the 'index' waypoint\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[pathend <index>] will fix missing goback on 'index' path end or all paths if no index specified\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[pathmerge <index>] will fix invalid merge of paths on 'index' path or all paths if no index specified\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[sniperspot <index>] will fix wrongly set sniper spot at the end of 'index' path or all paths if no index specified\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[crosswpt <index>] will fix range on all cross waypoints or on the 'index' cross waypoint\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[disabledwaittime <index>] will fix wait time disabled by zero priority on all waypoints or on the 'index' waypoint\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[swapteams] swaps team based values for all waypoints and paths as if teams changed sides\n");
			}
			else if (FStrEq(arg2, "masterrepair"))
			{
				wptfixer.RepairCrossWaypointRange();
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "any wrongly set range on any cross waypoint that was found was repaired ...\n");

				wptfixer.RepairInvalidPathMerge();
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "all paths were checked for invalid merge and found problems were repaired ...\n");

				wptfixer.RepairInvalidPathEnd();
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "all paths were checked for missing goback and all available fixes were applied ...\n");

				wptfixer.RepairSniperSpot();
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "any wrongly set sniper spot that was found was repaired ...\n");

				wptfixer.RepairInvalidCombinationOfWaypointPriorityAndTime();
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "any disabled wait time that was found was repaired ...\n");

				wptfixer.ValidatePath();
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "all paths were checked for validity and all available fixes were applied ...\n");

				PlaySoundConfirmation(pEntity, SND_DONE);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoint master repair DONE!\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "***TIP: use 'wpt save' command if you want to keep all these changes***\n");
			}
			else if (conInput.IsCommand(arg2, "range"))
			{
				if (conInput.IsValidWaypointIndex(arg3))
				{
					if (wptfixer.RepairWaypointRangeAndPosition(conInput.GetValidIndex(), pEntity, true))
					{
						PlaySoundConfirmation(pEntity, SND_DONE);
						ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoint range repair done\n");
					}
					else
					{
						conOutput.PrintErrorMessage(conOutErrMsg::inv_wpt, pEntity);
					}
				}
				else
				{
					wptfixer.RepairWaypointRangeAndPosition(pEntity, true);

					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "range was checked on all waypoints and those with incorrect range were repaired\n");
				}
			}
			else if (conInput.IsCommand(arg2, "range_and_position", "rangeandposition", "rangeandpos"))
			{
				if (conInput.IsValidWaypointIndex(arg3))
				{
					if (wptfixer.RepairWaypointRangeAndPosition(conInput.GetValidIndex(), pEntity))
					{
						PlaySoundConfirmation(pEntity, SND_DONE);
						ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoint range and position repair done\n");
					}
					else
					{
						conOutput.PrintErrorMessage(conOutErrMsg::inv_wpt, pEntity);
					}
				}
				else
				{
					wptfixer.RepairWaypointRangeAndPosition(pEntity);

					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "all waypoints were checked and the bad ones were repaired\n");
				}
			}
			else if (conInput.IsCommand(arg2, "pathend"))
			{
				if (conInput.IsValidPathIndex(arg3))
				{
					int result = wptfixer.RepairInvalidPathEnd(conInput.GetValidIndex());

					switch (result)
					{
					case -1:
						PlaySoundConfirmation(pEntity, SND_FAILED);
						sprintf(msg, "that path doesn't exist or unable to read it\n");
						break;
					case 0:
						PlaySoundConfirmation(pEntity, SND_DONE);
						sprintf(msg, "there is nothing to be fixed on this path\n");
						break;
					case 1:
						PlaySoundConfirmation(pEntity, SND_DONE);
						sprintf(msg, "path #%d was repaired\n", conInput.PrintValidIndex());
						break;
					case 10:
						PlaySoundConfirmation(pEntity, SND_FAILED);
						sprintf(msg, "unable to repair path #%d\n", conInput.PrintValidIndex());
						break;
					}

					ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
				}
				else
				{
					wptfixer.RepairInvalidPathEnd();

					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "all paths were checked for missing goback and all available fixes were applied\n");
				}
			}
			else if (conInput.IsCommand(arg2, "pathmerge"))
			{
				if (conInput.IsValidPathIndex(arg3))
				{
					int result = wptfixer.RepairInvalidPathMerge(conInput.GetValidIndex());

					switch (result)
					{
					case -1:
						PlaySoundConfirmation(pEntity, SND_FAILED);
						sprintf(msg, "that path doesn't exist or unable to read it\n");
						break;
					case 0:
						PlaySoundConfirmation(pEntity, SND_DONE);
						sprintf(msg, "there is nothing to be fixed on this path\n");
						break;
					case 1:
						PlaySoundConfirmation(pEntity, SND_DONE);
						sprintf(msg, "path #%d was repaired\n", conInput.PrintValidIndex());
						break;
					case 2:
						PlaySoundConfirmation(pEntity, SND_FAILED);
						//sprintf(msg, "found suspicious path connection on path #%d\n", conInput.PrintValidIndex());	no need to repeat the same output
						msg[0] = '\0';
						break;
					case 3:
						PlaySoundConfirmation(pEntity, SND_DONE);
						sprintf(msg, "path #%d was repaired, but there is also some suspicious path connection on this path\n", conInput.PrintValidIndex());
						break;
					case 4:
						PlaySoundConfirmation(pEntity, SND_FAILED);
						msg[0] = '\0';
						break;
					}

					ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
				}
				else
				{
					wptfixer.RepairInvalidPathMerge();

					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "all paths were checked for invalid merge and found problems were repaired\n");
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "***TIP: all findings are logged to MB error log file***\n");
				}
			}
			else if (conInput.IsCommand(arg2, "sniperspot", "camperspot"))
			{
				if (conInput.IsValidPathIndex(arg3))
				{
					int result = wptfixer.RepairSniperSpot(conInput.GetValidIndex());

					switch (result)
					{
					case -1:
						PlaySoundConfirmation(pEntity, SND_FAILED);
						sprintf(msg, "that path doesn't exist or unable to read it\n");
						break;
					case 0:
						PlaySoundConfirmation(pEntity, SND_DONE);
						sprintf(msg, "there is nothing to be fixed on this path\n");
						break;
					case 1:
						PlaySoundConfirmation(pEntity, SND_DONE);
						sprintf(msg, "sniper spot on path #%d was repaired\n", conInput.PrintValidIndex());
						break;
					}

					ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
				}
				else
				{
					wptfixer.RepairSniperSpot();

					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "any wrongly set sniper spot that was found was repaired!\n");
				}
			}
			else if (conInput.IsCommand(arg2, "crosswpt", "crossrange"))
			{
				if (conInput.IsValidWaypointIndex(arg3))
				{
					float original_range = waypoints[conInput.GetValidIndex()].range;

					float result = wptfixer.RepairCrossWaypointRange(conInput.GetValidIndex());

					if (result == -1.0f)
					{
						conOutput.PrintErrorMessage(conOutErrMsg::inv_wpt, pEntity);
					}
					else if (original_range == result)
					{
						PlaySoundConfirmation(pEntity, SND_DONE);
						ClientPrint(pEntity, HUD_PRINTNOTIFY, "there is nothing to be fixed on this cross waypoint\n");
					}
					else
					{
						PlaySoundConfirmation(pEntity, SND_DONE);

						sprintf(msg, "range on cross waypoint #%d was repaired to connect to all free path ends nearby\n", conInput.PrintValidIndex());
						ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
					}
				}
				else
				{
					wptfixer.RepairCrossWaypointRange();

					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "any wrongly set range on any cross waypoint that was found was repaired!\n");
				}
			}
			else if (conInput.IsCommand(arg2, "disabledwaittime", "disabled_wait_time"))
			{
				if (conInput.IsValidWaypointIndex(arg3))
				{
					int result = wptfixer.RepairInvalidCombinationOfWaypointPriorityAndTime(conInput.GetValidIndex(), true, false);

					if (result == 1)
					{
						PlaySoundConfirmation(pEntity, SND_DONE);
						ClientPrint(pEntity, HUD_PRINTNOTIFY, "disabled wait time was repaired on this waypoint\n");
					}
					else if (result == NO_VAL)
						conOutput.PrintErrorMessage(conOutErrMsg::inv_wpt, pEntity);
					else
						ClientPrint(pEntity, HUD_PRINTNOTIFY, "there is nothing to repaired on this waypoint\n");
				}
				else
				{
					wptfixer.RepairInvalidCombinationOfWaypointPriorityAndTime();

					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "any found disabled wait time was repaired!\n");
				}
			}
			else if (FStrEq(arg2, "swapteams"))
			{
				wptfixer.SwapTeamsInWaypoints();
				PlaySoundConfirmation(pEntity, SND_DONE);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "all team based values for both the waypoints as well as the paths were swapped!\n");
			}
			else
			{
				conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "***TIP: use 'wpt repair help' for help***\n");
			}
		}
		else if (conInput.IsCommand(arg1, "triggerevent", "trigger_event"))	// handle the triggers
		{
			if (FStrEq(arg2, "help") || FStrEq(arg2, "?"))
			{
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "\nAll valid trigger event options\n--------------------------------\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[add <trigger_name> <message>] adds a message to given trigger slot\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[delete <trigger_name>] erases given trigger slot\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[setpriority <prior> <team>] sets the second (ie. trigger) priority, works same as standard priority\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[settrigger <trigger_name> <state>] connects appropriate trigger msg to close waypoint, state means the 'on' or 'off' event\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[removetrigger <state>] removes the trigger message from close waypoint based on the state value\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[info <current>] prints info about the trigger waypoint, current shows just current priority based on triggered state\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[showall] prints all trigger event messages\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[test] makes a simple test of functionality\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[checkcapturemessage] allows printing the capture messages in order to create a trigger event message\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[help] shows this help\n");
			}
			else if (FStrEq(arg2, "add"))
			{
				int result = wpteditor.AddTriggerEvent(arg3, arg4);

				if (result == 1)
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "trigger event set correctly\n");
				}
				else
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);

					if (result == -1)
					{
						conOutput.PrintErrorMessage(conOutErrMsg::mis_arg, pEntity, true);
					}
					else if (result == -2)
					{
						conOutput.PrintErrorMessage(conOutErrMsg::inv_trig_name, pEntity, true);
					}
					else if (result == -3)
					{
						ClientPrint(pEntity, HUD_PRINTNOTIFY, "this trigger is already in use!\n");
					}
					else if (result == -4)
					{
						ClientPrint(pEntity, HUD_PRINTNOTIFY, "the trigger message is too long, shorten the message!\n");
					}
				}
			}
			else if (FStrEq(arg2, "delete"))
			{
				int result = wpteditor.DeleteTriggerEvent(arg3);

				if (result == 1)
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "trigger event successfully removed\n");
				}
				else
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);

					if (result == -1)
					{
						conOutput.PrintErrorMessage(conOutErrMsg::mis_arg, pEntity, true);
					}
					else if (result == -2)
					{
						conOutput.PrintErrorMessage(conOutErrMsg::inv_trig_name, pEntity, true);
					}
					else if (result == -3)
					{
						ClientPrint(pEntity, HUD_PRINTNOTIFY, "this trigger is already free!\n");
					}
				}
			}
			else if (FStrEq(arg2, "setpriority"))
			{
				int result = wpteditor.ChangeTriggerPriority(pEntity, arg3, arg4);

				if (result == -4)
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "NO trigger waypoint close enough\n");
				}
				else if (result == -3)
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "invalid priority value (0-5)!\n");
				}
				else if (result == -2)
				{
					conOutput.PrintErrorMessage(conOutErrMsg::inv_team_val, pEntity);
				}
				else if (result == -1)
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "priority/priorities already is/are on the waypoint\n");
				}
				else if (result == 0)
				{
					PlaySoundConfirmation(pEntity, SND_DONE);

					if ((arg4 != NULL) && (*arg4 != 0))
						sprintf(msg, "priority changed to NO priority!\n");
					else
						sprintf(msg, "both priorities changed to NO priority!\nWARNING - this waypoint is ignored by all bots!\n");
					
					ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
				}
				else if (result > 10)
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
					sprintf(msg, "both priorities set to %d\n", result - 10);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
				}
				else
				{
					PlaySoundConfirmation(pEntity, SND_DONE);

					if ((arg4 != NULL) && (*arg4 != 0))
						sprintf(msg, "priority changed to %d\n", result);
					else
						sprintf(msg, "both priorities changed to %d\n", result);

					ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
				}
			}
			else if (FStrEq(arg2, "settrigger"))
			{
				int result = wpteditor.ConnectTriggerEvent(pEntity, arg3, arg4);

				if (result == 1)
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "trigger set successfully\n");
				}
				else
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);

					if (result == -1)
					{
						conOutput.PrintErrorMessage(conOutErrMsg::mis_arg, pEntity, true);
					}
					else if (result == -2)
					{
						conOutput.PrintErrorMessage(conOutErrMsg::inv_trig_name, pEntity, true);
					}
					else if (result == -3)
					{
						conOutput.PrintErrorMessage(conOutErrMsg::inv_trig_state, pEntity, true);
					}
					else if (result == -4)
					{
						ClientPrint(pEntity, HUD_PRINTNOTIFY, "no trigger waypoint around!\n");
					}
				}
			}
			else if (FStrEq(arg2, "removetrigger"))
			{
				int result = wpteditor.RemoveTriggerEvent(pEntity, arg3);

				if (result == 1)
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "trigger successfully removed\n");
				}
				else
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);

					if (result == -1)
					{
						conOutput.PrintErrorMessage(conOutErrMsg::mis_arg, pEntity, true);
					}
					else if (result == -2)
					{
						conOutput.PrintErrorMessage(conOutErrMsg::inv_trig_state, pEntity, true);
					}
					else if (result == -3)
					{
						ClientPrint(pEntity, HUD_PRINTNOTIFY, "no trigger waypoint around!\n");
					}
				}
			}
			else if (FStrEq(arg2, "info"))
			{
				wptser.PrintTriggerWaypointInfo(pEntity, arg3);
			}
			else if (FStrEq(arg2, "showall"))
			{
				char msg[512]{};

				for (int index = 0; index < MAX_TRIGGERS; index++)
				{
					if (trigger_gamestate[index].GetUsed())
					{
						sprintf(msg, "trigger%d holds \"%s\"\n", index + 1, trigger_events[index].message);
					}
					else
						sprintf(msg, "trigger%d is free\n", index + 1);

					// just in case seeing the trigger message can be long
					conOutput.PrintToClient(pEntity, HUD_PRINTCONSOLE, msg);
				}
			}
			else if (FStrEq(arg2, "test"))
			{
				for (int index = 0; index < MAX_TRIGGERS; index++)
				{
					if (trigger_gamestate[index].GetUsed())
					{
						if (trigger_gamestate[index].GetTriggered())
							sprintf(msg, "trigger%d has been activated: 'YES'\n", index + 1);
						else
							sprintf(msg, "trigger%d has been activated: 'not yet'\n", index + 1);
					}
					else
						sprintf(msg, "trigger%d is free\n", index + 1);

					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
				}

				ClientPrint(pEntity, HUD_PRINTCONSOLE, "\nNote: Editing the waypoints, changing the teams or your death can affect the results.\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "To get valid results type 'restart' into the console to start afresh on this map.\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "But first make sure the waypoints are saved if you were editing them.\n");
			}
			else if (conInput.IsCommand(arg2, "checkcapturemessage", "checkcapmsg"))
			{
				if (internals.IsCheckTriggerCapMessage() || FStrEq(arg1, "off"))
				{
					internals.ResetCheckTriggerCapMessage();
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "DISABLED!\n", conInput.GetCmdName());
				}
				else
				{
					internals.SetCheckTriggerCapMessage(true);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "ENABLED!\n", conInput.GetCmdName());
				}
			}
			else
			{
				conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);
			}
		}
		else
		{
			conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "***TIP: use 'wpt help' for help***\n");

			if (wptser.IsShowWaypoints())
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoints are ON\n");
			else
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoints are OFF\n");
		}

		return true;
	}
	else if (FStrEq(pcmd, "autowpt"))
	{
		if (FStrEq(arg1, "help") || FStrEq(arg1, "?"))
		{
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "\nAll autowaypointing commands\n------------------------------\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[on] starts autowaypointing\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[off] stops autowaypointing\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[start] toggles start/stop autowaypointing\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[distance <number>] sets distance (80-400) between two waypoints for autowaypointing\n");
		}
		else if (FStrEq(arg1, "on"))
		{
			wpteditor.StartAutoWaypointg(true);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "autowaypointing is ON\n");
		}
		else if (FStrEq(arg1, "off"))
		{
			wpteditor.StartAutoWaypointg(false);

			ClientPrint(pEntity, HUD_PRINTNOTIFY, "autowaypointing is OFF\n");
		}
		else if (FStrEq(arg1, "start"))
		{
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "***toggle mode used***\n");

			if (wptser.IsAutoWaypointing())
			{
				// turn autowaypointing off
				wpteditor.StartAutoWaypointg(false);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "autowaypointing is OFF\n");
			}
			else
			{
				wpteditor.StartAutoWaypointg(true);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "autowaypointing is ON\n");
			}
		}
		// sets the distance between two waypoints for autowaypointing
		else if (conInput.IsCommand(arg1, "distance"))
		{
			if (conInput.IsValidFloatValue(arg2, MIN_WPT_DIST, MAX_WPT_DIST))
			{
				wptser.SetAutoWaypointingDistance(conInput.GetFloatValue());

				PlaySoundConfirmation(pEntity, SND_DONE);
				sprintf(msg, "autowaypointing distance set to %.1f\n", wptser.GetAutoWaypointingDistance());
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
			else
			{
				PlaySoundConfirmation(pEntity, SND_FAILED);
				conOutput.PrintErrorMessage(wptser.GetAutoWaypointingDistance(), pEntity);
			}
		}
		else
		{
			conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "***TIP: use 'autowpt help' for help***\n");

			if (wptser.IsAutoWaypointing())
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "autowaypointing is ON\n");
			else
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "autowaypointing is OFF\n");
		}

		return true;
	}
	else if (FStrEq(pcmd, "pathwpt"))
	{
		if (FStrEq(arg1, "help") || FStrEq(arg1, "?"))
		{
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "\nAll pathwaypointing commands\n----------------------------------\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[commands] path creation commands help\n--------------------------------\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[on] shows paths\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[off] hides paths\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[show] toggles show/hide paths\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[displaytime <num>] sets the time for all paths to be redrawn on screen\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[save] saves all paths to the file\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[load] loads all paths from file\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[load <mapname>] loads all paths from 'mapname' file\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[count] prints info about total amounts of paths\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[info <index>] prints info about 'index' path or path on close waypoint\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[printall <arg>] prints info about all paths or paths on 'arg' waypoint or close wpt if arg is 'nearby'\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[printpath <index>] prints all waypoints and their types from 'index' path\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[checkinvalid <info>] will fix or remove all invalid paths eg. with length=1; if 'info' it will print more info\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[checkproblems <arg>] prints problems with paths (use 'pathwpt checkproblems help' for more info)\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[compass <arg>] shows rapidly blinking beam that'll direct you to the beginning of 'arg' path\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[highlight <arg>] shows only path or paths matching the 'arg' filter; used without 'arg' will turn it off\n");
#ifdef _DEBUG
			//!!!!!!!!!!!!!!!!!!!!!!!!!
			//TEMP
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "\n!!!Following cmds are for dev purposes!!!\n");
			//ClientPrint(pEntity, HUD_PRINTNOTIFY, "NOT DONE YET\n");
			//END TEMP
#endif
		}
		else if (FStrEq(arg1, "commands"))
		{
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "\nAll edit commands\n--------------------------------\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[start <index>] starts path on 'index' or close waypoint\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[stop] finishes the path that is currently worked on\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[continue <index>] marks 'index' path or path on close waypoint if no 'index' as the currently worked on path\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[add <index>] adds 'index' or close waypoint to currently worked on path\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[autoadd <on/off>] auto adds every waypoint that the player \"touches\"\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[insert <wpt#> <path#> <between1> <between2>] inserts waypoint into path between its two waypoints\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[remove <wpt#> <path#>] removes 'wpt#' waypoint from 'path#' path\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[split <path#> <wpt#>] splits 'path#' path on two parts on 'wpt#' waypoint\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[reverse <index>] reorders the waypoints on 'index' path or path on close waypoint (from start->end to end->start)\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[delete <index>] deletes whole 'index' path or path on close waypoint\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[reset <index>] resets 'index' path or path on close waypoint back to default\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[setteam <team> <index>] sets team for 'index' path or path on close waypoint\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[setclass <class> <index>] sets class for 'index' path or path on close waypoint\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[setdirection <dir> <index>] sets direction for 'index' path or path on close waypoint\n");
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "[setmisc <misc> <index>] sets additional flags for 'index' path or path on close waypoint\n");
		}
		else if (FStrEq(arg1, "on"))
		{
			wptser.SetShowPaths(true);		// turn paths on
			wptser.SetShowWaypoints(true);	// we must turn waypoints on too

			ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoint paths are ON\n");
		}
		else if (FStrEq(arg1, "off"))
		{
			wptser.ResetShowPaths();

			ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoint paths are OFF\n");
		}
		else if (FStrEq(arg1, "show"))
		{
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "***toggle mode used***\n");

			if (wptser.IsShowPaths())
			{
				wptser.ResetShowPaths();	// turn paths off
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoint paths are OFF\n");
			}
			else
			{
				wptser.SetShowPaths(true);
				wptser.SetShowWaypoints(true);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoint paths are ON\n");
			}
		}
		else if (conInput.IsCommand(arg1, "displaytime", "display_time"))
		{
			if (conInput.IsValidFloatValue(arg2, 0.2f, 5.0f))
			{
				wptser.SetPathsDisplayTime(conInput.GetFloatValue());

				PlaySoundConfirmation(pEntity, SND_DONE);
				sprintf(msg, "paths display time set to %.1f\n", wptser.GetPathsDisplayTime());
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
			else
			{
				PlaySoundConfirmation(pEntity, SND_FAILED);
				conOutput.PrintErrorMessage(wptser.GetPathsDisplayTime(), pEntity);
			}
		}
		else if (FStrEq(arg1, "save"))
		{
			if (patheditor.SavePaths(NULL))
			{
				PlaySoundConfirmation(pEntity, SND_DONE);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "paths successfully saved\n");
			}
		}
		else if (FStrEq(arg1, "load"))
		{
			if ((arg2 != NULL) && (*arg2 != 0))
			{
				char temp[64];

				strcpy(temp, arg2);

				if (patheditor.LoadPaths(pEntity, temp))
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "paths successfully loaded\n");
				}
			}
			else
			{
				if (patheditor.LoadPaths(pEntity, NULL))
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "paths successfully loaded\n");
				}
			}
		}
		else if (FStrEq(arg1, "count"))
		{
			sprintf(msg, "paths already used= %d still could use= %d out of %d\n", num_w_paths, MAX_W_PATHS - num_w_paths, MAX_W_PATHS);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
		}
		else if (conInput.IsCommand(arg1, "info"))
		{
			if (conInput.IsValidPathIndex(arg2, true))
			{
				bool result = false;

				result = wptser.PrintPathInfo(pEntity, conInput.GetValidIndex());

				// if that path doesn't exist print warning
				if (result == false)
				{
					conOutput.PrintErrorMessage(conOutErrMsg::no_pth, pEntity);
				}
			}
		}
		else if (conInput.IsCommand(arg1, "printall"))
		{
			if (conInput.IsValidWaypointIndex(arg2, true, "nearby"))
			{
				if (FStrEq(arg2, "nearby"))
				{
					if (wptser.PrintAllPaths(pEntity, -10) == false)
					{
						// print warning if there was an error
						PlaySoundConfirmation(pEntity, SND_FAILED);
						ClientPrint(pEntity, HUD_PRINTNOTIFY, "not close enough to valid path waypoint!\n");
					}
				}
				else
				{
					if (wptser.PrintAllPaths(pEntity, conInput.GetValidIndex()) == false)
						conOutput.PrintErrorMessage(conOutErrMsg::inv_wpt, pEntity);
				}
			}
			else
			{
				conOutput.PrintErrorMessage(conOutErrMsg::inv_arg, pEntity);
			}
		}
		else if (FStrEq(arg1, "printpath"))
		{
			if (conInput.IsValidPathIndex(arg2))
			{
				if (wptser.PrintWholePath(pEntity, conInput.GetValidIndex()))
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
				}
				else
				{
					conOutput.PrintErrorMessage(conOutErrMsg::no_pth, pEntity);
				}
			}
			else
			{
				conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);
			}
		}
		else if (FStrEq(arg1, "checkinvalid"))
		{
			int result;

			if (FStrEq(arg2, "info"))
				result = wptfixer.DeleteInvalidPaths(true);
			else
				result = wptfixer.DeleteInvalidPaths(false);

			if (result == 0)
			{
				PlaySoundConfirmation(pEntity, SND_DONE);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "all paths are valid\n");
			}
			else if (result > 0)
			{
				int unfixed_paths = 0;

				while (result >= 600)
				{
					result -= 600;
					unfixed_paths++;
				}

				if (unfixed_paths == 0)
				{
					PlaySoundConfirmation(pEntity, SND_DONE);

					sprintf(msg, "total of %d invalid paths has been successfully fixed or completely removed\n", result);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
				}
				else
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);

					sprintf(msg, "total of %d invalid paths has been successfully fixed or completely removed\n", result);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

					sprintf(msg, "there is total of %d invalid paths left in the waypoints that cannot be automatically fixed or removed\n", unfixed_paths);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
				}
			}
		}
		else if (conInput.IsCommand(arg1, "checkproblems", "checkforproblems"))
		{
			if (FStrEq(arg2, "help") || FStrEq(arg2, "?"))
			{
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[bugsonly] will print only findings marked as a bug\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[save] will write all findings in MB error log file\n");
			}
			else
			{
				if (FStrEq(arg2, "save"))
					wptfixer.CheckPathsForProblems(true);
				else
				{
					if (FStrEq(arg2, "bugsonly") || FStrEq(arg2, "errorsonly"))
						wptoutputer.SetPrintErrorsOnly();
					else
						wptoutputer.ResetPrintErrorsOnly();

					wptfixer.CheckPathsForProblems(false);
				}

				if (wptoutputer.GetAmountOfFoundIssues() == 0)
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "all paths successfully passed\n");
				}
				else
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);

					sprintf(msg, "total of %d bugs/possible problems have been found\n", wptoutputer.GetAmountOfFoundIssues());
					ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

					// if there're more issues than what the console can display then print a simple note so that the user knows about it
					if (wptoutputer.CanStillPrintIt() == false)
					{
						ClientPrint(pEntity, HUD_PRINTCONSOLE, "There are more errors. Fix the listed ones and repeat the command to see them.\n");
					}

					ClientPrint(pEntity, HUD_PRINTCONSOLE, "***TIP: use 'pathwpt highlight' or 'pathwpt compass' or 'wpt compass' to locate the problem paths or waypoints***\n");
				}
			}

			ClientPrint(pEntity, HUD_PRINTCONSOLE, "***TIP: use 'wpt checkproblems' to check for waypoints related problems***\n");
		}
		else if (conInput.IsCommand(arg1, "compass"))
		{
			if (conInput.IsValidPathIndex(arg2, false, "off"))
			{
				if (FStrEq(arg2, "off"))
				{
					wptser.ResetCompassIndex();

					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "DISABLED!\n", conInput.GetCmdName());
				}
				else
				{
					if (w_paths[conInput.GetValidIndex()] != NULL)
					{
						char index[5]{};

						// convert path first waypoint index to string
						sprintf(index, "%d", w_paths[conInput.GetValidIndex()]->wpt_index + 1);

						if (wptser.ShowCompass(pEntity, (const char*)index))
							PlaySoundConfirmation(pEntity, SND_DONE);
						else
							wptser.ResetCompassIndex();
					}
					else
					{
						// turn the compass off
						wptser.ResetCompassIndex();
						conOutput.PrintErrorMessage(conOutErrMsg::no_pth, pEntity);
					}

				}
			}
			else
			{
				wptser.ResetCompassIndex();
				conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);
			}
		}
		else if (FStrEq(arg1, "highlight"))
		{
			if ((arg2 != NULL) && (*arg2 != 0))
			{
				int path = NO_VAL;

				if (FStrEq(arg2, teamONE.GetTeamName()))
					path = HIGHLIGHT_TEAMONE;
				else if (FStrEq(arg2, teamTWO.GetTeamName()))
					path = HIGHLIGHT_TEAMTWO;
				else if ((FStrEq(arg2, "oneway")) || (FStrEq(arg2, "one-way")) || (FStrEq(arg2, "one_way")))
					path = HIGHLIGHT_ONEWAY;
				else if ((FStrEq(arg2, "sniper")) || (FStrEq(arg2, "sniperonly")) || (FStrEq(arg2, "sniper-only")))
					path = HIGHLIGHT_SNIPER;
				else if ((FStrEq(arg2, "mgunner")) || (FStrEq(arg2, "mgunneronly")) || (FStrEq(arg2, "mgunner-only")))
					path = HIGHLIGHT_MGUNNER;
				else if ((FStrEq(arg2, "antiarmor")) || (FStrEq(arg2, "antiarmoronly")) || (FStrEq(arg2, "antiarmor-only")) ||
					(FStrEq(arg2, "anti-armor")) || (FStrEq(arg2, "anti-armoronly")) || (FStrEq(arg2, "anti-armor-only")))
					path = HIGHLIGHT_ANTIARMOR;
				else
					path = atoi(arg2) - 1;

				if ((path == HIGHLIGHT_TEAMONE) || (path == HIGHLIGHT_TEAMTWO) || (path == HIGHLIGHT_ONEWAY) || (path == HIGHLIGHT_SNIPER) || (path == HIGHLIGHT_MGUNNER) || (path == HIGHLIGHT_ANTIARMOR) ||
					((path >= 0) && (path < MAX_W_PATHS)))
				{
					char* start_msg = "path highlighting is ON!";
					char* end_msg = "paths will be drawn";

					if (path == HIGHLIGHT_TEAMONE)
					{
						wptser.SetPathToHighlight(path);
						wptser.SetShowWaypoints(true);			// force displaying waypoints
						wptser.SetShowPaths(true);				// and paths too

						PlaySoundConfirmation(pEntity, SND_DONE);
						sprintf(msg, "%s\nall %s possible %s\n", start_msg, teamONE.GetTeamName2wordsLC(), end_msg);
						ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
					}
					else if (path == HIGHLIGHT_TEAMTWO)
					{
						wptser.SetPathToHighlight(path);
						wptser.SetShowWaypoints(true);
						wptser.SetShowPaths(true);

						PlaySoundConfirmation(pEntity, SND_DONE);
						sprintf(msg, "%s\nall %s possible %s\n", start_msg, teamTWO.GetTeamName2wordsLC(), end_msg);
						ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
					}
					else if (path == HIGHLIGHT_ONEWAY)
					{
						wptser.SetPathToHighlight(path);
						wptser.SetShowWaypoints(true);
						wptser.SetShowPaths(true);

						PlaySoundConfirmation(pEntity, SND_DONE);
						sprintf(msg, "%s\nall one-way %s\n", start_msg, end_msg);
						ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
					}
					else if (path == HIGHLIGHT_SNIPER)
					{
						wptser.SetPathToHighlight(path);
						wptser.SetShowWaypoints(true);
						wptser.SetShowPaths(true);

						PlaySoundConfirmation(pEntity, SND_DONE);
						sprintf(msg, "%s\nall sniper %s\n", start_msg, end_msg);
						ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
					}
					else if (path == HIGHLIGHT_MGUNNER)
					{
						wptser.SetPathToHighlight(path);
						wptser.SetShowWaypoints(true);
						wptser.SetShowPaths(true);

						PlaySoundConfirmation(pEntity, SND_DONE);
						sprintf(msg, "%s\nall machine gunner %s\n", start_msg, end_msg);
						ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
					}
					else if (path == HIGHLIGHT_ANTIARMOR)
					{
						wptser.SetPathToHighlight(path);
						wptser.SetShowWaypoints(true);
						wptser.SetShowPaths(true);

						PlaySoundConfirmation(pEntity, SND_DONE);
						sprintf(msg, "%s\nall anti-armor specialist %s\n", start_msg, end_msg);
						ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
					}
					else if (w_paths[path] != NULL)
					{
						wptser.SetPathToHighlight(path);
						wptser.SetShowWaypoints(true);
						wptser.SetShowPaths(true);

						PlaySoundConfirmation(pEntity, SND_DONE);
						sprintf(msg, "%s\npath no. %d is now the only drawn path\n", start_msg, path + 1);
						ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
					}
					else
					{
						conOutput.PrintErrorMessage(conOutErrMsg::no_pth, pEntity);
					}
				}
				else
				{
					conOutput.PrintErrorMessage(conOutErrMsg::inv_arg, pEntity);
					sprintf(msg, "valid arguments are: %s, %s, oneway, sniper, mgunner, antiarmor and index (index is path number from 1 to %d)\n",	teamONE.GetTeamName(), teamTWO.GetTeamName(), MAX_W_PATHS);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
				}
			}
			else
			{
				// turn it off
				wptser.ResetPathToHighlight();

				PlaySoundConfirmation(pEntity, SND_DONE);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "path highlighting is OFF!\n");
			}
		}
		else if (FStrEq(arg1, "start"))
		{
			// turn paths on
			wptser.SetShowPaths(true);
			wptser.SetShowWaypoints(true);  // and waypoints must be displayed too

			if (conInput.IsValidWaypointIndex(arg2, true) && patheditor.Create(pEntity, conInput.GetValidIndex()))
			{
				PlaySoundConfirmation(pEntity, SND_DONE);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "new path successfully started\n");
			}
			else
			{
				PlaySoundConfirmation(pEntity, SND_FAILED);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "path cannot be started or not close enough to any waypoint!\n");
			}
		}
		else if (FStrEq(arg1, "stop"))
		{
			if (patheditor.Finish(pEntity))
			{
				PlaySoundConfirmation(pEntity, SND_DONE);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "current path was finished\n");
			}
			else
			{
				PlaySoundConfirmation(pEntity, SND_FAILED);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "there was no path to finish!\n");
			}
		}
		else if (FStrEq(arg1, "continue"))
		{
			if (conInput.IsValidPathIndex(arg2, true) && patheditor.Continue(pEntity, conInput.GetValidIndex()))
			{
				PlaySoundConfirmation(pEntity, SND_DONE);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "path is actual and ready to continue in\n");
			}
			else
			{
				conOutput.PrintErrorMessage(conOutErrMsg::no_pth_no_wpt_nrb, pEntity);
			}
		}
		else if (FStrEq(arg1, "add"))
		{
			if (conInput.IsValidWaypointIndex(arg2, true) && patheditor.AddWaypoint(pEntity, conInput.GetValidIndex()))
			{
				PlaySoundConfirmation(pEntity, SND_DONE);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoint added to actual path\n");
			}
			else
			{
				PlaySoundConfirmation(pEntity, SND_FAILED);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "no path or invalid waypoint or not close enough to allowed waypoint or it already is in this path!\n");
			}
		}
		else if (FStrEq(arg1, "autoadd"))
		{
			// check if exist arg2 and is valid
			if (FStrEq(arg2, "on"))
				wptser.SetAutoAddToPath(true);
			else if (FStrEq(arg2, "off"))
				wptser.ResetAutoAddToPath();
			else
			{
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "***toggle mode used***\n");

				if (wptser.IsAutoAddToPath())
					wptser.ResetAutoAddToPath();	// turn auto adding off
				else
					wptser.SetAutoAddToPath(true);
			}

			// print proper msg
			if (wptser.IsAutoAddToPath())
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "autoadd to path is ON\n");
			else
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "autoadd to path is OFF\n");
		}
		else if (FStrEq(arg1, "insert"))
		{
			if ((FStrEq(arg2, "help")) || (FStrEq(arg2, "?")))
			{
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "Insert command usage:\n---------------------------\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "'pathwpt insert <wpt_index> <path_index> <arg3> <arg4>\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "---------------------------\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "<wpt_index> is the unique no. of the waypoint you want to insert into existing path\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "<path_index> is the unique no. of existing path\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "<arg3> can be unique no. of a waypoint that already is in the path\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "<arg3> can be a word 'start' when the waypoint is going to be inserted to the beginning of the path\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "<arg3> can be a word 'end' when the waypoint is going to be inserted to the end of the path\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "<arg4> is unique no. of a waypoint that already is in the path and is a direct neighbour of arg3 waypoint\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "\nFew examples:\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "'pathwpt insert 123 50 start' will insert new waypoint no. 123 to the beginning of path no. 50\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "'pathwpt insert 123 50 end' will insert new waypoint no. 123 to the end of path no. 50\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "'pathwpt insert 123 50 400 660' will insert new waypoint no. 123 into path no. 50 between its wpts no. 400 and 660\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "'pathwpt insert 123 50 660 400' will do exactly the same thing as the command from above\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "path wpts no. 400 & 660 must be direct neighbours in the path (eg. pathwpt <-> pathwpt <-> 400 <-> 660 <-> pathwpt)\n");
			}
			else
			{
				int result = patheditor.InsertWaypoint(arg2, arg3, arg4, arg5);

				if (result == -6)
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "can't insert a waypoint that already is in this path!\n");
				}
				else if (result == -5)
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "both path waypoints are same, unknown position for insertion!\n");
				}
				else if (result == -4)
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "invalid or missing waypoint for insertion!\n");
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "***TIP: use 'pathwpt insert help' for help!***\n");

				}
				else if (result == -3)
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "invalid or missing path!\n");
				}
				else if (result == -2)
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "invalid or missing path waypoint1 index or this waypoint isn't in this path!\n");
				}
				else if (result == -1)
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "invalid or missing path waypoint2 index or this waypoint isn't in this path!\n");
				}
				else if (result == 0)
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoint can't be inserted pathwaypoints aren't neighbours!\n");
				}
				else if (result == 1)
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoint successfully inserted\n");
				}
				else
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "unknown error!\n");
				}
			}
		}
		else if (FStrEq(arg1, "remove"))
		{
			int wpt_index = NO_VAL;

			if (conInput.IsValidWaypointIndex(arg2, true, "nearby"))
				wpt_index = conInput.GetValidIndex();

			if (conInput.IsValidPathIndex(arg3, true) && patheditor.RemoveWaypoint(pEntity, wpt_index, conInput.GetValidIndex()))
			{
				PlaySoundConfirmation(pEntity, SND_DONE);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoint successfully removed from path\n");
			}
			else
			{
				conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);
			}
		}
		else if (FStrEq(arg1, "split"))
		{
			if ((FStrEq(arg2, "help")) || (FStrEq(arg2, "?")))
			{
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "Split command usage:\n---------------------------\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "'pathwpt split <path_index> <wpt_index>'\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "---------------------------\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "<path_index> is unique number of path you want to divide into two parts\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "<wpt_index> is the waypoint at which will the path be divided (ie. the future end point of this path)\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "Note: You can't split path which length is less than 5 (ie. path with less than 5 waypoints).\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "It's due to the fact that each valid path must be at least 2 waypoints long.\n");
			}
			else
			{
				int result = patheditor.Split(pEntity, arg2, arg3);

				if (result == -1)
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "not close enough to any waypoint or there's no path on that waypoint!\n");
				}
				else if (result == -2)
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "the path is too short for splitting (ie. the path has less than 5 waypoints)!\n");
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "There must stay at least 2 waypoints on either part of the path after the split.\n");
				}
				else if (result == -3)
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "that waypoint isn't in this path!\n");
				}
				else if (result == -4)
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "can't split the path at this position!\n");
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "There must stay at least 2 waypoints on either part of the path after the split.\n");
				}
				else if (result == -5)
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "unable to split the path, because max. amount of paths has been reached!\n");
				}
				else if (result == -6)
				{
					PlaySoundConfirmation(pEntity, SND_FAILED);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "one or both parts of the path aren't valid\n");
				}
				else if (result == -7)
				{
					conOutput.PrintErrorMessage(conOutErrMsg::no_pth, pEntity);
				}
				else if (result == -8)
				{
					conOutput.PrintErrorMessage(conOutErrMsg::inv_wpt, pEntity);
				}
				else if ((result >= 0) && (result < MAX_W_PATHS))
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "path successfully splitted\n");

					char details[128]{};
					sprintf(details, "the other part of the path is now a path no.%d\n", result + 1);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, details);
				}
			}
		}
		else if (FStrEq(arg1, "reverse"))
		{
			if (conInput.IsValidPathIndex(arg2, true))
			{
				if (patheditor.Reverse(pEntity, conInput.GetValidIndex()))
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "path successfully reversed\n");
				}
				else
					conOutput.PrintErrorMessage(conOutErrMsg::no_pth_no_wpt_nrb, pEntity);
			}
			else
				conOutput.PrintErrorMessage(conOutErrMsg::inv_arg, pEntity);
		}
		else if (FStrEq(arg1, "delete"))
		{
			if (conInput.IsValidPathIndex(arg2, true))
			{
				if (patheditor.Delete(pEntity, conInput.GetValidIndex()))
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "path successfully deleted\n");
				}
				else
					conOutput.PrintErrorMessage(conOutErrMsg::no_pth_no_wpt_nrb, pEntity);
			}
			else
				conOutput.PrintErrorMessage(conOutErrMsg::inv_arg, pEntity);
		}
		else if (FStrEq(arg1, "reset"))
		{
			if (conInput.IsValidPathIndex(arg2, true))
			{
				if (patheditor.ResetToDefaults(pEntity, conInput.GetValidIndex()))
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "path successfully reset\n");
				}
				else
					conOutput.PrintErrorMessage(conOutErrMsg::no_pth_no_wpt_nrb, pEntity);
			}
			else
				conOutput.PrintErrorMessage(conOutErrMsg::inv_arg, pEntity);
		}
		else if (FStrEq(arg1, "setteam"))
		{
			if ((FStrEq(arg2, "help")) || (FStrEq(arg2, "?")))
			{
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "All valid team options\n---------------------------\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[both] both teams will use it - DEFAULT\n");
				sprintf(msg, "[%s] only %s bots will use it\n", teamONE.GetTeamName(), teamONE.GetTeamName2wordsLC());
				ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
				sprintf(msg, "[%s] only %s bots will use it\n", teamTWO.GetTeamName(), teamTWO.GetTeamName2wordsLC());
				ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
			}
			else
			{
				// set the initial value to something that cannot happen
				int result = -128;

				if (conInput.IsValidPathIndex(arg3, true))
					result = patheditor.ChangeTeam(pEntity, arg2, conInput.GetValidIndex());

				if (result == 0)
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "path already has this team tag\n");
				}
				else if (result == 1)
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "path team tag successfully changed\n");
				}
				else if (result == -2)
				{
					conOutput.PrintErrorMessage(conOutErrMsg::no_pth_no_wpt_nrb, pEntity);
				}
				else
				{
					conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "***TIP: use 'setteam help' for help!***\n");
				}
			}
		}
		else if (FStrEq(arg1, "setclass"))
		{
			if ((FStrEq(arg2, "help")) || (FStrEq(arg2, "?")))
			{
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "All valid class options\n-----------------------------\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[all] all bots will use it - DEFAULT\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[sniper] only snipers will use it\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[mgunner] only mgunners will use it\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[antiarmor] only anti-armor specialists (i.e. equipped with a rocket launcher) will use it\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "Note: 'sniper' and 'mgunner' tags can be together on one specific path. Both classes will then be able to use such path.\n");
			}
			else
			{
				int result = -128;

				if (conInput.IsValidPathIndex(arg3, true))
					result = patheditor.ChangeClass(pEntity, arg2, conInput.GetValidIndex());

				if (result == 1)
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "path class tag successfully changed\n");
				}
				else if (result == -2)
				{
					conOutput.PrintErrorMessage(conOutErrMsg::no_pth_no_wpt_nrb, pEntity);
				}
				else
				{
					conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "***TIP: use 'setclass help' for help!***\n");
				}
			}
		}
		else if (FStrEq(arg1, "setdirection"))
		{
			if ((FStrEq(arg2, "help")) || (FStrEq(arg2, "?")))
			{
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "All valid way/direction options\n----------------------------------------\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[one] only one way path (start-end)\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[two] two way path (start-end as well as end-start) - DEFAULT\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[patrol] patrol type path (cycle start-end-start)\n");
			}
			else
			{
				int result = -128;

				if (conInput.IsValidPathIndex(arg3, true))
					result = patheditor.ChangeDirection(pEntity, arg2, conInput.GetValidIndex());

				if (result == 0)
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "path already has this direction tag\n");
				}
				else if (result == 1)
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "path direction tag successfully changed\n");
				}
				else if (result == -2)
				{
					conOutput.PrintErrorMessage(conOutErrMsg::no_pth_no_wpt_nrb, pEntity);
				}
				else
				{
					conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "***TIP: use 'setdirection help' for help!***\n");
				}
			}
		}
		else if (FStrEq(arg1, "setmisc"))
		{
			if ((FStrEq(arg2, "help")) || (FStrEq(arg2, "?")))
			{
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "All valid miscellaneous/additional options\n-----------------------------\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[avoid_enemy] bot won't attack distant enemies\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[ignore_enemy] bot won't attack enemies unless one is right next to him\n");
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "[carry_item] bot will search for these paths when carrying a goal item\n");
			}
			else
			{
				int result = -128;

				if (conInput.IsValidPathIndex(arg3, true))
					result = patheditor.ChangeMisc(pEntity, arg2, conInput.GetValidIndex());

				if (result == 1)
				{
					PlaySoundConfirmation(pEntity, SND_DONE);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "miscellaneous/additional path tag successfully changed\n");
				}
				else if (result == -2)
				{
					conOutput.PrintErrorMessage(conOutErrMsg::no_pth_no_wpt_nrb, pEntity);
				}
				else
				{
					conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "***TIP: use 'setmisc help' for help!***\n");
				}
			}
		}


#ifdef _DEBUG
		// ONLY FOR TESTS
		else if (FStrEq(arg1, "movepathz"))
		{
			if ((arg2 != NULL) && (*arg2 != 0))
			{
				int path_index = atoi(arg2) - 1;

				if ((path_index >= 0) && (path_index < num_w_paths - 1))
				{
					float value = 0.0;

					if ((arg3 != NULL) && (*arg3 != 0))
					{
						value = atof(arg3);

						if (WaypointMoveWholePath(path_index, value, 3))
						{
							PlaySoundConfirmation(pEntity, SND_DONE);
							ClientPrint(pEntity, HUD_PRINTNOTIFY, "whole path has been repositioned\n");
						}
						else
						{
							conOutput.PrintErrorMessage(conOutErrMsg::inv_arg, pEntity);
						}
					}
				}
			}
		}

#endif //_DEBUG

		else
		{
			conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "***TIP: use 'pathwpt help' for help***\n");

			if (wptser.IsShowPaths())
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoint paths are ON\n");
			else
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoint paths are OFF\n");
		}

		return true;
	}
	else if (FStrEq(pcmd, "menuselect") && (g_menu_state != MENU_NONE))
	{
		// whole marinebot menu
		MBMenuSystem(pEntity, arg1);
	}
	else
	{
#ifdef _DEBUG
		// nearly all debugging commands
		if (CheckForSomeDebuggingCommands(pEntity, pcmd, arg1, arg2, arg3, arg4, arg5))
			return true;
#endif
	}

	return false;
}


#ifdef _DEBUG
inline bool CheckForSomeDebuggingCommands(edict_t* pEntity, const char* pcmd, const char* arg1, const char* arg2, const char* arg3, const char* arg4, const char* arg5)
{
	char msg[256]{};

	if (FStrEq(pcmd, "toggledev"))
	{
		float dev_state = CVAR_GET_FLOAT("developer");

		if (dev_state == 0)
			CVAR_SET_FLOAT("developer", 1);
		else
			CVAR_SET_FLOAT("developer", 0);

		ClientPrint(pEntity, HUD_PRINTNOTIFY, "*** developer mode changed ***\n");

		return TRUE;
	}
	else if (FStrEq(pcmd, "cleardf") || FStrEq(pcmd, "destroydf") || FStrEq(pcmd, "erasedebugfile"))
	{
		char filename[256];

		util.MarineBotFileName(filename, PUBLIC_DEBUG_FILE, NULL);
		remove(filename);

		return true;
	}
	else if (FStrEq(pcmd, "settest") || FStrEq(pcmd, "testsetting") || FStrEq(pcmd, "testsettings"))
	{
		CVAR_SET_FLOAT("mp_teamlimits", 6);
		//CVAR_SET_FLOAT("developer", 1);
		botmanager.SetOverrideTeamsBalance(true);
		
		internals.SetIsCustomWaypoints(true);
		wpteditor.LoadWaypoints(pEntity, NULL);
		patheditor.LoadPaths(pEntity, NULL);

		//botdebugger.SetObserverMode(true);
		//externals.SetSpawnSkill(1);
		//botdebugger.SetDebugActions(true);
		//botdebugger.SetDebugStuck(true);
		//botdebugger.SetDebugPaths(true);
		//botdebugger.SetDebugWaypoints(true);
		//botdebugger.SetDebugCross(true);
		//botdebugger.SetDebugWeapons(true, 1);
		//botdebugger.SetDontShootFirearm(true);
		wptser.SetShowPaths(true);
		wptser.SetShowWaypoints(true);

		ClientPrint(pEntity, HUD_PRINTCONSOLE, "test setting ENABLED!\n");
		return true;
	}
	else if (FStrEq(pcmd, "showtl") || FStrEq(pcmd, "showtraceline") || FStrEq(pcmd, "showtracelines") || FStrEq(pcmd, "displaytracelines") || FStrEq(pcmd, "showmethelines"))
	{
		if (devTool.IsDisplayTracelines())
		{
			devTool.SetDisplayTracelines(false);
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "displaying TraceLines DISABLED!\n");
		}
		else
		{
			devTool.SetDisplayTracelines(true);
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "displaying TraceLines ENABLED!\n");
		}

		return true;
	}
	else if (FStrEq(pcmd, "settracelineduration") || FStrEq(pcmd, "setbeamduration") ||	FStrEq(pcmd, "settracelinelife") || FStrEq(pcmd, "settllife") || FStrEq(pcmd, "setbeamlife"))
	{
		if ((arg1 != NULL) && (*arg1 != 0))
		{
			devTool.SetTLBeamDuration(atoi(arg1));

			sprintf(msg, "TraceLine display duration set to: %d\n", devTool.GetTLBeamDuration());
			ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
		}
		else
		{
			sprintf(msg, "TraceLine display duration is: %d\n", devTool.GetTLBeamDuration());
			ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
		}

		return true;
	}
	else if (FStrEq(pcmd, "settracelinecolor") || FStrEq(pcmd, "settlcolor") || FStrEq(pcmd, "setbeamcolor"))
	{
		devTool.SetTLBeamColor(arg1);

		if ((arg1 == NULL) || (*arg1 == 0))
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "TraceLine color has been reset to DEFAULT!\n");


		return true;
	}
	else if ((strcmp(pcmd, "debugbot") == 0) || (strcmp(pcmd, "debug_bot") == 0))
	{
		if ((arg1 == NULL) || (*arg1 == 0))
		{
			conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);

			return true;
		}
		else if (FStrEq(arg1, "off"))
		{
			devTool.ResetSpecificBotDebugging();
			devTool.ResetPointerToSpecificBot();
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "debugging of certain bot has been DISABLED!\n");

			return true;
		}

		int i = util.FindBotByName(arg1);

		if (i != -1)
		{
			devTool.SetSpecificBotDebugging(true);
			devTool.SetPointerToSpecificBot(bots[i].pEdict);
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "debugging of this bot is ENABLED!\n");
		}
		else
		{
			devTool.ResetSpecificBotDebugging();
			devTool.ResetPointerToSpecificBot();
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "debugging of certain bot has been DISABLED!\n");
		}

		return true;
	}
	else if ((strcmp(pcmd, "debugengine") == 0) || (strcmp(pcmd, "debug_engine") == 0))
	{
		if (debug_engine)
		{
			debug_engine = 0;
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "debug_engine DISABLED!\n");
		}
		else
		{
			debug_engine = 1;
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "debug_engine ENBLED!\n");
		}

		return true;
	}
	else if ((strcmp(pcmd, "testbalance") == 0) || (strcmp(pcmd, "testteams") == 0))
	{
		int i, actual_pl, bot_count, reds, blues, diff;
		actual_pl = bot_count = reds = blues = 0;

		for (i = 0; i < MAX_CLIENTS; i++)
		{
			if (clients[i].pEntity == NULL)
				continue;
			else
			{
				++actual_pl;

				if (clients[i].pEntity->v.flags & FL_FAKECLIENT)
					++bot_count;

				if (util.GetTeam(clients[i].pEntity) == teamONE.GetTeamId())
					reds++;
				if (util.GetTeam(clients[i].pEntity) == teamTWO.GetTeamId())
					blues++;

				sprintf(msg, "%s is in team: %d\n", STRING(clients[i].pEntity->v.netname), util.GetTeam(clients[i].pEntity));
				ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
			}
		}

		diff = reds - blues;

		sprintf(msg, "Testing team balance - clients %d | bots %d | reds %d | blues %d | diff %d\n", actual_pl, bot_count, reds, blues, diff);
		ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

		return true;
	}
	else if ((strcmp(pcmd, "test_dumpedict") == 0) || (strcmp(pcmd, "testdumpedict") == 0) || (strcmp(pcmd, "test_dumpclient") == 0) || (strcmp(pcmd, "testdumpclient") == 0))
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (clients[i].pEntity)
				util.DumpEdictToFile(clients[i].pEntity);
		}

		return true;
	}
	else if ((strcmp(pcmd, "test_clientdetection") == 0) || (strcmp(pcmd, "testclientdetection") == 0))
	{
		int cl_count, bot_count, human_count;
		cl_count = bot_count = human_count = 0;

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (clients[i].pEntity != NULL)
			{
				cl_count++;

				if (clients[i].pEntity->v.flags & FL_FAKECLIENT)
					bot_count++;
				else
					human_count++;
			}
		}

		sprintf(msg, "total clients on server - (by client array: %d) (counted now: %d)\n", clients[0].ClientCount(), cl_count);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);

		sprintf(msg, "number of bots - (by cl array: %d) (counted now: %d)\n", clients[0].BotCount(), bot_count);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);

		sprintf(msg, "number of humans: (by cl array: %d) (counted now: %d)\n",	clients[0].HumanCount(), human_count);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);

		return true;
	}
	//TEMP: need cmd to change the default max effective range modifier for all weapons
	else if (FStrEq(pcmd, "trng"))
	{
		extern float rg_modif;

		if ((arg1 != NULL) && (*arg1 != 0))
		{
			int temp = atoi(arg1);

			rg_modif = (float)temp;
			rg_modif /= 100;

			ALERT(at_console, "range modifier changed - it's %.2f of original range now, reinit all weapons...\n", rg_modif);

			char filename[128]{};
			char FA_version_string[16]{};

			if (g_mod_version == DOD_13)
				strcpy(FA_version_string, "1_3");
			else
				strcpy(FA_version_string, "NONE-ERROR");

			util.MarineBotFileName(filename, "weapons", FA_version_string);

			BotWeaponArraysInit(filename);
		}

		return true;
	}
	//TEMP: need cmd to check the weapon arrays values
	else if (FStrEq(pcmd, "twav"))
	{
		char weapon_id[5]{};

		for (int index = 0; index < MAX_WEAPONS; ++index)
		{
			sprintf(weapon_id, "ID%d", index);

			sprintf(msg, "WID=%s | minDist=%.2f | maxDist=%.2f | baseDelay=%.2f | minDelay[%.2f %.2f %.2f %.2f %.2f] | maxDelay[%.2f %.2f %.2f %.2f %.2f]\n",
				weapon_id, bot_weapon_select[index].min_safe_distance, bot_weapon_select[index].max_effective_distance, bot_fire_delay[index].primary_base_delay,
				bot_fire_delay[index].primary_min_delay[0], bot_fire_delay[index].primary_min_delay[1], bot_fire_delay[index].primary_min_delay[2],
				bot_fire_delay[index].primary_min_delay[3], bot_fire_delay[index].primary_min_delay[4],
				bot_fire_delay[index].primary_max_delay[0], bot_fire_delay[index].primary_max_delay[1], bot_fire_delay[index].primary_max_delay[2],
				bot_fire_delay[index].primary_max_delay[3], bot_fire_delay[index].primary_max_delay[4]);

			ALERT(at_console, msg);
		}

		if (CVAR_GET_FLOAT("developer") < 1.0f)
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "Make sure to use developer mode, because standard console print would get overloaded by this!\n");

		return true;
	}
	//TEMP: need cmd to test the reading of external file with target offsets
	else if (FStrEq(pcmd, "ttbo"))
	{
		/*/
		char filename[128];
		filename[0] = 0;

		util.MarineBotFileName(filename, "weapons", "targetbodyoffsets");

		sprintf(msg, "loading target body offsets: %s\n", filename);
		conOutput.Print(NULL, msg, MType::msg_info);

		if (BotTargetOffsetsArrayInit(filename) == false)
		{
			errormsgs.PrepareErrorAndWarning();
			conOutput.Print(NULL, errormsgs.GetError(), MType::msg_error);
			conOutput.Print(NULL, errormsgs.GetWarning(), MType::msg_warning);
		}
		/**/

		char msg[512]{};
		for (int index = 0; index < BOT_SKILL_LEVELS; index++)
		{
			sprintf(msg, "AimSkill%d | Xaxis=%.2f | Yaxis=%.2f | Zaxis=%.2f | Xsniper=%.2f | Ysniper=%.2f | Zsniper=%.2f\n",
				index+1, bot_target_offset[index].x_axis, bot_target_offset[index].y_axis, bot_target_offset[index].z_axis,
				bot_target_offset[index].x_axis_sniper, bot_target_offset[index].y_axis_sniper, bot_target_offset[index].z_axis_sniper);

			ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
		}

		return true;
	}
	//TEMP: need cmd to test new name validation util
	else if (FStrEq(pcmd, "debug_name"))
	{
		int used = 0;
		int free = 0;

		for (int i = 0; i < internals.GetAmerNamesCount(); i++)
		{
			if (bot_names_american[i].is_used)
				used++;
			else
				free++;
		}
		sprintf(msg, "AMER names: USED=%d | FREE=%d | TOTAL=%d\n", used, free, used + free);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);

		used = free = 0;

		for (int i = 0; i < internals.GetBritNamesCount(); i++)
		{
			if (bot_names_british[i].is_used)
				used++;
			else
				free++;
		}
		sprintf(msg, "BRIT names: USED=%d | FREE=%d | TOTAL=%d\n", used, free, used + free);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);

		used = free = 0;

		for (int i = 0; i < internals.GetGerNamesCount(); i++)
		{
			if (bot_names_german[i].is_used)
				used++;
			else
				free++;
		}
		sprintf(msg, "GER names: USED=%d | FREE=%d | TOTAL=%d\n", used, free, used + free);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);

		return true;
	}
	//TEMP: need cmd to test new mod team definition class
	else if (FStrEq(pcmd, "testteamsname"))
	{
		sprintf(msg, "team ONE ID=%d | name=<%s> | IDasSTR=<%s> | name2wordsFirstCaps=<%s>\n",
			teamONE.GetTeamId(), teamONE.GetTeamName(), teamONE.GetTeamIdAsString(), teamONE.GetTeamName2wordsFUC());
		ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
		
		sprintf(msg, "team TWO ID=%d | name=<%s> | IDasSTR=<%s> | name2wordsFirstCaps=<%s>\n",
			teamTWO.GetTeamId(), teamTWO.GetTeamName(), teamTWO.GetTeamIdAsString(), teamTWO.GetTeamName2wordsFUC());
		ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);

		return true;
	}
	//TEMP: need cmd to test in combat weapon manipulation
	else if (FStrEq(pcmd, "tbco1"))
	{
		extern bool in_bot_dev_level1;

		if (in_bot_dev_level1)
		{
			in_bot_dev_level1 = false;
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "in bot dev level 1 turned off\n");
		}
		else
		{
			in_bot_dev_level1 = true;
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "in bot dev level 1 turned on\n");
		}

		return true;
	}

	//TEMP: need cmd to test getting effective range of bot's weapon
	else if (FStrEq(pcmd, "tbger"))
	{
		if ((arg1 == NULL) || (*arg1 == 0))
		{
			conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);
			return true;
		}

		int index = util.FindBotByName(arg1);

		if (index != -1)
		{
			sprintf(msg, "The effective range of bot's main weapon is %.2f\n", bots[index].GetWeaponEffectiveRange());
			ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
		}
		else
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "no bot with that name!\n");

		return true;
	}

	//TEMP: need cmd to test combat movements
	else if (FStrEq(pcmd, "tbc"))
	{
		int start, end;

		if ((arg1 != NULL) && (*arg1 != 0))
		{
			int i = util.FindBotByName(arg1);

			if (i != -1 && bots[i].is_used)
			{
				start = i;
				end = i + 1;
			}
		}
		else
		{
			start = 0;
			end = MAX_CLIENTS;
		}

		for (int i = start; i < end; i++)
		{
			if ((bots[i].is_used) && devTool.IsCommandValidForThisBot(bots[i].pEdict))
			{
				sprintf(msg, "<%s>AdvanceTowardEnemyT %.1f | OverrideAdvanceT %.1f | ReloadT %.1f | globtime %.1f\n",
					bots[i].name, bots[i].GetAdvanceTowardEnemyTime(), bots[i].GetOverrideAdvanceTime(), bots[i].GetWeaponReloadTime(), gpGlobals->time);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

				sprintf(msg, "SnipeT %.1f | WaitT %.1f | isDontMove %d | WaitForEnemyT %.1f | BotHideT %.1f\n", bots[i].GetSnipeTime(), bots[i].GetWaitTime(),
					bots[i].IsTask(TASK_DONTMOVEINCOMBAT), bots[i].GetWaitForEnemyTime(), bots[i].GetBotHideTime());
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

				sprintf(msg, "CheckStanceT %.1f | ChangeStanceT %.1f\n", bots[i].GetCheckStanceTime(), bots[i].GetStanceChangeTime());
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

				if (bots[i].IsBehaviour(BOT_PRONED))
					sprintf(msg, "Proned\n");
				if (bots[i].IsBehaviour(BOT_CROUCHED))
					sprintf(msg, "Crouched\n");
				if (bots[i].IsBehaviour(BOT_STANDING))
					sprintf(msg, "Standing\n");
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

				if (bots[i].IsProne())
				{
					sprintf(msg, "<%d> edict iuser3 has a \"prone\" or \"prone with bipod\" flag set\n", i);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
				}
			}
		}

		return true;
	}

	//TEMP: need cmd to test grenades
	else if (FStrEq(pcmd, "tgr"))
	{
		extern bot_weapon_t weapon_defs[MAX_WEAPONS];

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (bots[i].is_used)
			{
				int gren = bots[i].grenade_slot;

				sprintf(msg, "usedWeap <%s> | grenade slot ID %d | isAvailable %d | isDepleted %d | isPinPulled %d\n",
					util.ConvertUsedWeaponToString(&bots[i]), gren, bots[i].IsWeaponStatus(WS_GRENADEAVAILABLE), bots[i].IsWeaponStatus(WS_GRENADEDEPLETED), bots[i].IsWeaponStatus(WS_GRENADEPINPULLED));
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

				sprintf(msg, "f_shootT %.2f | grenT %.2f | currentT %.2f\n", bots[i].f_shoot_time, bots[i].GetGrenadeUseTime(), gpGlobals->time);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

				if (gren != NO_VAL)
				{
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "Showing records for current grenade ...\n");
					ALERT(at_console, "Current Ammo1 %d | Ammo1Max %d | Current Ammo2 %d | Ammo2Max %d | Weapon Flags %d\n",
						bots[i].curr_rgAmmo[weapon_defs[gren].iAmmo1], weapon_defs[gren].iAmmo1Max,	bots[i].curr_rgAmmo[weapon_defs[gren].iAmmo2], weapon_defs[gren].iAmmo2Max,	weapon_defs[gren].iFlags);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
				}

				if ((bots[i].bot_weapons & (1 << gren)) == false)
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "Both grenades already used\n");
				else
					ClientPrint(pEntity, HUD_PRINTNOTIFY, "Still has at least one grenade\n");
			}
		}

		return true;
	}

	//TEMP: need cmd to test grenades
	else if (FStrEq(pcmd, "tgrenaim"))
	{
		if (bots[0].is_used)
		{
			Vector start, end;
			Vector origin = bots[0].pEdict->v.origin;

			// draw x-coord white beam
			start = origin + Vector(0, 0, 144) - Vector(10, 0, 0);
			end = origin + Vector(0, 0, 144) + Vector(10, 0, 0);
			DrawBeam(pEntity, start, end, 30, 255, 255, 255, 10);
			// draw y-coord white beam
			start = origin + Vector(0, 0, 144) - Vector(0, 10, 0);
			end = origin + Vector(0, 0, 144) + Vector(0, 10, 0);
			DrawBeam(pEntity, start, end, 30, 255, 255, 255, 10);

			// draw x-coord white beam
			start = origin + Vector(0, 0, 216) - Vector(10, 0, 0);
			end = origin + Vector(0, 0, 216) + Vector(10, 0, 0);
			DrawBeam(pEntity, start, end, 30, 255, 255, 255, 10);
			// draw y-coord white beam
			start = origin + Vector(0, 0, 216) - Vector(0, 10, 0);
			end = origin + Vector(0, 0, 216) + Vector(0, 10, 0);
			DrawBeam(pEntity, start, end, 30, 255, 255, 255, 10);

			// draw x-coord white beam
			start = origin + Vector(0, 0, 288) - Vector(10, 0, 0);
			end = origin + Vector(0, 0, 288) + Vector(10, 0, 0);
			DrawBeam(pEntity, start, end, 30, 255, 255, 255, 10);
			// draw y-coord white beam
			start = origin + Vector(0, 0, 288) - Vector(0, 10, 0);
			end = origin + Vector(0, 0, 288) + Vector(0, 10, 0);
			DrawBeam(pEntity, start, end, 30, 255, 255, 255, 10);

			// draw x-coord white beam
			start = origin + Vector(0, 0, 360) - Vector(10, 0, 0);
			end = origin + Vector(0, 0, 360) + Vector(10, 0, 0);
			DrawBeam(pEntity, start, end, 30, 255, 255, 255, 10);
			// draw y-coord white beam
			start = origin + Vector(0, 0, 360) - Vector(0, 10, 0);
			end = origin + Vector(0, 0, 360) + Vector(0, 10, 0);
			DrawBeam(pEntity, start, end, 30, 255, 255, 255, 10);

			// draw x-coord white beam
			start = origin + Vector(0, 0, 432) - Vector(10, 0, 0);
			end = origin + Vector(0, 0, 432) + Vector(10, 0, 0);
			DrawBeam(pEntity, start, end, 30, 255, 255, 255, 10);
			// draw y-coord white beam
			start = origin + Vector(0, 0, 432) - Vector(0, 10, 0);
			end = origin + Vector(0, 0, 432) + Vector(0, 10, 0);
			DrawBeam(pEntity, start, end, 30, 255, 255, 255, 10);

			// draw x-coord white beam
			start = origin + Vector(0, 0, 504) - Vector(10, 0, 0);
			end = origin + Vector(0, 0, 504) + Vector(10, 0, 0);
			DrawBeam(pEntity, start, end, 30, 255, 255, 255, 10);
			// draw y-coord white beam
			start = origin + Vector(0, 0, 504) - Vector(0, 10, 0);
			end = origin + Vector(0, 0, 504) + Vector(0, 10, 0);
			DrawBeam(pEntity, start, end, 30, 255, 255, 255, 10);

			// draw x-coord white beam
			start = origin + Vector(0, 0, 504) - Vector(10, 0, 0);
			end = origin + Vector(0, 0, 504) + Vector(10, 0, 0);
			DrawBeam(pEntity, start, end, 30, 0, 255, 255, 10);
			// draw y-coord white beam
			start = origin + Vector(0, 0, 504) - Vector(0, 10, 0);
			end = origin + Vector(0, 0, 504) + Vector(0, 10, 0);
			DrawBeam(pEntity, start, end, 30, 0, 255, 255, 10);

			sprintf(msg, "dist to bot: %.1f\n", (bots[0].pEdict->v.origin - pEntity->v.origin).Length());
			ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
		}

		return true;
	}

	//TEMP: need cmd to test bot ammo array
	else if (FStrEq(pcmd, "tbaa"))
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (bots[i].is_used)
			{
				ALERT(at_console, "Printing curr rgAmmo for %s ...\n", bots[i].name);
				for (int j = 0; j < MAX_AMMO_SLOTS; j++)
				{
					sprintf(msg, "slot #%d holds %d | ", j, bots[i].curr_rgAmmo[j]);
					ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
				}
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "\n");
			}
		}

		return true;
	}

	//TEMP: need cmd to test weaponarrays
	else if (FStrEq(pcmd, "checkw"))
	{
		bot_weapon_select_t* pSelect = NULL;
		bot_fire_delay_t* pDelay = NULL;

		pSelect = &bot_weapon_select[0];
		pDelay = &bot_fire_delay[0];

		if ((arg1 != NULL) && (*arg1 != 0))
		{
			int temp = atoi(arg1);

			ALERT(at_console, "pSelect min_safe_distance for it is %.0f\n", pSelect[temp].min_safe_distance);
			ALERT(at_console, "pSelect max_effective_distance for it is %.0f\n", pSelect[temp].max_effective_distance);
			ALERT(at_console, "pDelay ID for it is %d\n", pDelay[temp].iId);
			ALERT(at_console, "pDelay primary_base_delay for it is %.2f\n", pDelay[temp].primary_base_delay);
			ALERT(at_console, "pDelay primary_min_delay[4] for it is %.2f\n", pDelay[temp].primary_min_delay[4]);
			ALERT(at_console, "pDelay primary_max_delay[4] for it is %.2f\n", pDelay[temp].primary_max_delay[4]);
		}

		return true;
	}

	//TEMP: just to mark point in public debugging file
	else if (FStrEq(pcmd, "dumphi") || FStrEq(pcmd, "dumphello") || FStrEq(pcmd, "dumphere"))
	{
		util.DebugInFile("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ Start here! @@@@@@@@@@@@@@@@@@@@@@\n");

		return true;
	}


	//TEMP: need cmd to test weaponarrays
	else if (FStrEq(pcmd, "dumpw"))
	{
		bot_weapon_select_t* pSelect = NULL;
		bot_fire_delay_t* pDelay = NULL;

		pSelect = &bot_weapon_select[0];
		pDelay = &bot_fire_delay[0];

		FILE* fp;

		util.DebugDev("***New Dump***", -100, -100);

		fp = fopen(debug_fname, "a");

		if (fp)
		{
			for (int i = 0; i < MAX_WEAPONS; i++)
			{
				fprintf(fp, "Weapons ID %d\n", pSelect[i].iId);
				fprintf(fp, "pSelect min_safe_distance for it is %.2f\n", pSelect[i].min_safe_distance);
				fprintf(fp, "pSelect max_effective_distance for it is %.2f\n", pSelect[i].max_effective_distance);
				fprintf(fp, "pDelay ID for it is %d\n", pDelay[i].iId);
				fprintf(fp, "pDelay primary_base_delay for it is %.2f\n", pDelay[i].primary_base_delay);
				fprintf(fp, "pDelay primary_min_delay[4] for it is %.2f\n", pDelay[i].primary_min_delay[4]);
				fprintf(fp, "pDelay primary_max_delay[4] for it is %.2f\n", pDelay[i].primary_max_delay[4]);
			}

			fclose(fp);
		}

		return true;
	}

	//TEMP: need cmd to test follow team leader ability
	else if (FStrEq(pcmd, "tftl"))
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (bots[i].is_used)
			{
				char leader_name[32];
				char target_name[32];

				if (bots[i].pTeamLeader != NULL)
					strcpy(leader_name, STRING(bots[i].pTeamLeader->v.netname));
				else
					strcpy(leader_name, "no leader");

				if (bots[i].pBotEnemy != NULL)
					strcpy(target_name, STRING(bots[i].pBotEnemy->v.netname));
				else
					strcpy(target_name, "no enemy");

				ALERT(at_console, "botSeeTeamLeaderT=%.2f | TeamLeader=<%s> | enemy=<%s> | globT=%.2f\n", bots[i].GetSeeTeamLeaderTime(), leader_name, target_name, gpGlobals->time);
			}
		}

		return true;
	}

	//TEMP: need cmd to test wpts
	else if (FStrEq(pcmd, "checkwpt"))
	{
		extern void Wpt_Check(void);

		Wpt_Check();

		return true;
	}

	//TEMP: need cmd to test targeted aim wpts
	else if (FStrEq(pcmd, "tba"))
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (bots[i].is_used)
			{
				sprintf(msg, "CurrA %d | Aims1 %d | Aims2 %d | Aims3 %d | Aims4 %d | IsEmpty(bool so 1=true) %d | Count %d\n",
					bots[i].GetCurrentAimWaypoint() + 1, bots[i].Aims.Print(0), bots[i].Aims.Print(1), bots[i].Aims.Print(2), bots[i].Aims.Print(3),
					bots[i].Aims.IsEmpty(), bots[i].Aims.Count());
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
		}

		return true;
	}

	//TEMP: need cmd to test targeted aim wpts
	else if (FStrEq(pcmd, "tbas"))
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (bots[i].is_used)
			{
				if ((arg1 != NULL) && (*arg1 != 0))
				{
					bots[i].Aims.AddNewAimWpt( atoi(arg1) );
				}
			}
		}

		return true;
	}

	//TEMP: need cmd to test targeted aim wpts
	else if (FStrEq(pcmd, "tbac"))
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (bots[i].is_used)
			{
				bots[i].Aims.Clear();
			}
		}

		return true;
	}

	//TEMP: need cmd to test targeted aim wpts
	else if (FStrEq(pcmd, "tbag"))
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (bots[i].is_used)
			{
				if ((arg1 != NULL) && (*arg1 != 0))
				{
					ALERT(at_console, "GetAims(slot = arg1) %d | GetAims(not specified = should get the arrayslot[0]) %d | GetAims(invalid arrayslot = it should be -1) %d\n",
						bots[i].Aims.Get(atoi(arg1)), bots[i].Aims.Get(), bots[i].Aims.Get(7));
					ALERT(at_console, "Warning: All of those are real values (wpt indexes), ie. not modified by +1 as what would Print() do\n");
				}
			}
		}

		return true;
	}

	//TEMP: need cmd to test targeted aim wpts
	else if (FStrEq(pcmd, "tbagr"))
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (bots[i].is_used)
			{
				ALERT(at_console, "GetRandomAims(index from random array slot that is NOT EMPTY) %d\n", bots[i].Aims.GetRandom());
				ALERT(at_console, "Warning: This is a real value (wpt index), ie. not modified by +1 as what would Print() do\n");
			}
		}

		return true;
	}


	//TEMP: need cmd to test target distance
	else if (FStrEq(pcmd, "tbd"))
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (bots[i].is_used)
			{
				if (bots[i].pBotEnemy)
				{
					sprintf(msg, "3D Distance to my enemy %.0f || 2D Distance to my enemy %.0f || yaw_speed %.2f\n", (bots[i].pEdict->v.origin - bots[i].pBotEnemy->v.origin).Length(),
						(bots[i].pEdict->v.origin - bots[i].pBotEnemy->v.origin).Length2D(), bots[i].pEdict->v.yaw_speed);

					ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
				}
			}
		}

		return true;
	}

	//TEMP: need cmd to test drop ammo
	else if (FStrEq(pcmd, "tbdropa"))
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if ((bots[i].is_used) && devTool.IsCommandValidForThisBot(bots[i].pEdict))
			{
				FakeClientCommand(bots[i].pEdict, "dropammo", NULL, NULL);
			}
		}

		return true;
	}

	//TEMP: need cmd to test bipod
	else if (FStrEq(pcmd, "tbbipod"))
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if ((bots[i].is_used) && devTool.IsCommandValidForThisBot(bots[i].pEdict))
			{
				sprintf(msg, "canUseBipodNow(bool) %d | isInStandingBipodSpot(bool) %d | weapon_action %d | isHandlingBipodNow(bool) %d\n",
					CanDeployBipod(bots[i].pEdict), IsInStandingBipodSpot(bots[i].pEdict), bots[i].weapon_action, (bots[i].IsNotDeployingBipod() == false));
				ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
			}
		}

		return true;
	}

	else if ((FStrEq(pcmd, "tbclay")) || (FStrEq(pcmd, "tbmine")) || (FStrEq(pcmd, "tbbomb")))
	{
		int start, end;

		if ((arg1 != NULL) && (*arg1 != 0))
		{
			int i = util.FindBotByName(arg1);

			if (i != -1 && bots[i].is_used)
			{
				start = i;
				end = i + 1;
			}
		}
		else
		{
			start = 0;
			end = MAX_CLIENTS;
		}

		for (int i = start; i < end; i++)
		{
			if ((bots[i].is_used) && devTool.IsCommandValidForThisBot(bots[i].pEdict))
			{
				{
					sprintf(msg, "hasExplosiveCharge(bool) %d | foundAnyBreakableAround(bool) %d | weapon_action %d | shootT %.2f\n",
						bots[i].IsEquippedWithExplosiveCharge(), (bots[i].HasNoGEnt() == false), bots[i].weapon_action, bots[i].f_shoot_time);
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);

					sprintf(msg, "timeToReachCurrWpt %.1f | wpt_wait %.1f | wpt_action %.1f | globtime %.1f\n",
						bots[i].GetTimeToReachCurrWaypoint(), bots[i].GetWaitTime(), bots[i].GetActionTime(), gpGlobals->time);
					ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
				}
			}
		}

		return true;
	}

	else if (FStrEq(pcmd, "tbptge") || FStrEq(pcmd, "tbpointerttogent"))
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if ((bots[i].is_used) && devTool.IsCommandValidForThisBot(bots[i].pEdict))
			{
				if (bots[i].HasNoGEnt())
					sprintf(msg, "no pointer to game entity!\n");
				else
					sprintf(msg, "classname of pointer to game entity is: (%s)\n", STRING(bots[i].GetPointerToGEnt()->v.classname));
				
				ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
			}
		}

		return true;
	}

	else if (FStrEq(pcmd, "tbdtge") || FStrEq(pcmd, "tbdistancetogent"))
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if ((bots[i].is_used) && devTool.IsCommandValidForThisBot(bots[i].pEdict))
			{
				sprintf(msg, "the distance to game entity is: <%.2f>\n", bots[i].GetDistanceToGEnt());

				ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
			}
		}

		return true;
	}

	else if (FStrEq(pcmd, "tbv"))
	{
		if (bots[0].is_used)
		{
			sprintf(msg, "bot v.velocity (length2D) value is: %.2f\n", bots[0].pEdict->v.velocity.Length2D());
			ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
		}

		return true;
	}

	else if (FStrEq(pcmd, "tbvc"))
	{
		if (bots[0].is_used)
		{
			util.Voice(bots[0].pEdict, voiceCmd::fire_in_the_hole);
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "bot used voice command now!\n");
		}

		return true;
	}

	//TEMP: need cmd to test weapon usage
	else if (FStrEq(pcmd, "tbw") || FStrEq(pcmd, "checkbotweapon") || FStrEq(pcmd, "checkbotweapons"))
	{
		int start, end;

		if ((arg1 != NULL) && (*arg1 != 0))
		{
			int i = util.FindBotByName(arg1);

			if (i != -1 && bots[i].is_used)
			{
				start = i;
				end = i + 1;
			}
		}
		else
		{
			start = 0;
			end = MAX_CLIENTS;
		}

		for (int i = start; i < end; i++)
		{
			if ((bots[i].is_used) && devTool.IsCommandValidForThisBot(bots[i].pEdict))
			{
				char w_using[16]{};
				char w_action[16]{};

				if (bots[i].IsUsedWeaponMain())
					strcpy(w_using, "MAIN");
				else if (bots[i].IsUsedWeaponBackup())
					strcpy(w_using, "BACKUP");
				else if (bots[i].IsUsedWeaponKnife())
					strcpy(w_using, "KNIFE");
				else if (bots[i].IsUsedWeaponGrenade())
					strcpy(w_using, "GRENADE");
				else if (bots[i].IsUsedWeaponClaymoreMine())
					strcpy(w_using, "CLAYMORE_MINE");
				else
					strcpy(w_using, "NONE-ERROR");

				if (bots[i].weapon_action == W_LOCKED)
					strcpy(w_action, "LOCKED");
				else if (bots[i].IsWeaponReady())
					strcpy(w_action, "READY");
				else if (bots[i].weapon_action == W_TAKEOTHER)
					strcpy(w_action, "TAKEOTHER");
				else if (bots[i].weapon_action == W_INCHANGE)
					strcpy(w_action, "INCHANGE");
				else if (bots[i].weapon_action == W_INHANDS)
					strcpy(w_action, "INHANDS");
				else if (bots[i].weapon_action == W_INRELOAD)
					strcpy(w_action, "INRELOAD");
				else if (bots[i].weapon_action == W_INMERGEMAGS)
					strcpy(w_action, "INMERGEMAGS");
				else
					strcpy(w_action, "NONE-ERROR");

				sprintf(msg, "<%d>WeaponID %d | iAttach %d | WeapSecondaryModeActive %d | iuser3 %d | BotFOV %1.f\n",
					i, bots[i].current_weapon.iId, bots[i].current_weapon.iAttachment, bots[i].IsWeaponSecondaryModeActive(), bots[i].pEdict->v.iuser3, bots[i].pEdict->v.fov);
				ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);

				sprintf(msg, "<%d>CurrWeapon: Ammo in chamber/clip %d | Primary reserve ammo/clips %d | Secondary reserve ammo/clips %d\n",
					i, bots[i].current_weapon.iClip, bots[i].current_weapon.iAmmo1, bots[i].current_weapon.iAmmo2);
				ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);

				sprintf(msg, "<%d>Using %s | WeaponAction %s | MainNoAMMO %d | BackupNoAMMO %d | EnemyDist %.1f\n",
					i, w_using, w_action, bots[i].IsNoAmmoForMainWeapon(), bots[i].IsNoAmmoForBackupWeapon(), bots[i].GetPrevDistanceToEnemy());
				ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);

				sprintf(msg, "<%d>MainW ammo/clips needed to be taken %d | BackupW ammo/clips needed to be taken %d\n",
					i, bots[i].GetTakeAmmoForMainWeapon(), bots[i].GetTakeAmmoForBackupWeapon());
				ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);

				sprintf(msg, "<%d>Is Bipod %d | MainW ID %d | BackupW ID %d\n", i, bots[i].IsTask(TASK_BIPOD), bots[i].main_weapon, bots[i].backup_weapon);
				ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);

				int mainW_ammoID = -1;
				int backupW_ammoID = -1;
				int nade_ammoID = -1;
				extern bot_weapon_t weapon_defs[MAX_WEAPONS];

				if (bots[i].main_weapon != -1)
					mainW_ammoID = weapon_defs[bots[i].main_weapon].iAmmo1;
				if (bots[i].backup_weapon != -1)
					backupW_ammoID = weapon_defs[bots[i].backup_weapon].iAmmo1;
				if (bots[i].grenade_slot != -1)
					nade_ammoID = weapon_defs[bots[i].grenade_slot].iAmmo1;

				sprintf(msg, "<%d>MainW ID %d | MainW AmmoID %d | BackupW ID %d | BackupW AmmoID %d | Gren ID %d | Gren AmmoID %d\n",
					i, bots[i].main_weapon, mainW_ammoID, bots[i].backup_weapon, backupW_ammoID, bots[i].grenade_slot, nade_ammoID);
				ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
			}
		}

		return true;
	}

	//TEMP: need cmd to set weapon usage to main weapon
	else if (FStrEq(pcmd, "buwm") || FStrEq(pcmd, "forcedweaponselectmain") || FStrEq(pcmd, "forcedselectweaponmain") || FStrEq(pcmd, "forcedweaponusemain"))
	{
		int i = 0;

		if ((arg1 != NULL) && (*arg1 != 0))
		{
			i = util.FindBotByName(arg1);
		}

		if (i != -1 && bots[i].is_used)
		{
			bots[i].UseWeapon(uWeapon::main);
			bots[i].RemoveWeaponStatus(WS_NOAMMOFORMAIN);

			ClientPrint(pEntity, HUD_PRINTCONSOLE, "Weapon usage set to main weapon\n");
		}

		return true;
	}
	//TEMP: need cmd to set weapon usage to backup weapon
	else if (FStrEq(pcmd, "buwb") || FStrEq(pcmd, "forcedweaponselectbackup") || FStrEq(pcmd, "forcedselectweaponbackup") || FStrEq(pcmd, "forcedweaponusebackup"))
	{
		int i = 0;

		if ((arg1 != NULL) && (*arg1 != 0))
		{
			i = util.FindBotByName(arg1);
		}

		if (i != -1 && bots[i].is_used)
		{
			bots[i].UseWeapon(uWeapon::backup);
			bots[i].SetWeaponStatus(WS_NOAMMOFORMAIN);

			ClientPrint(pEntity, HUD_PRINTCONSOLE, "Weapon usage set to backup weapon (main no ammo set too)\n");
		}

		return true;
	}
	//TEMP: need cmd to set weapon usage to knife
	else if (FStrEq(pcmd, "buwk") || FStrEq(pcmd, "forcedweaponselectknife") || FStrEq(pcmd, "forcedselectweaponknife") || FStrEq(pcmd, "forcedweaponuseknife"))
	{
		int i = 0;

		if ((arg1 != NULL) && (*arg1 != 0))
		{
			i = util.FindBotByName(arg1);
		}

		if (i != -1 && bots[i].is_used)
		{
			bots[i].UseWeapon(uWeapon::knife);
			bots[i].SetWeaponStatus(WS_NOAMMOFORMAIN);
			bots[i].SetWeaponStatus(WS_NOAMMOFORBACKUP);
			bots[i].weapon_action = W_TAKEOTHER;

			ClientPrint(pEntity, HUD_PRINTCONSOLE, "Weapon usage set to knife (main+backup no ammo set too)\n");
		}


		return true;
	}

	// TEMP: need cmd to set weapon usage to greanade
	else if (FStrEq(pcmd, "buwg") || FStrEq(pcmd, "forcedweaponselectgrenade") || FStrEq(pcmd, "forcedselectweapongrenade") || FStrEq(pcmd, "forcedweaponusegrenade"))
	{
		int i = 0;

		if ((arg1 != NULL) && (*arg1 != 0))
		{
			i = util.FindBotByName(arg1);
		}

		if (i != -1 && bots[i].is_used)
		{
			bots[i].UseWeapon(uWeapon::grenade);
			bots[i].weapon_action = W_TAKEOTHER;

			ClientPrint(pEntity, HUD_PRINTCONSOLE, "Weapon usage set to grenade\n");
		}

		return true;
	}

	// TEMP: need cmd to set weapon usage to claymore mine
	else if (FStrEq(pcmd, "buwc") || FStrEq(pcmd, "forcedweaponselectclaymore") || FStrEq(pcmd, "forcedselectweaponclaymore") || FStrEq(pcmd, "forcedweaponuseclaymore"))
	{
		int i = 0;

		if ((arg1 != NULL) && (*arg1 != 0))
		{
			i = util.FindBotByName(arg1);
		}

		if (i != -1 && bots[i].is_used)
		{
			bots[i].UseWeapon(uWeapon::claymoremine);
			bots[i].SetWeaponStatus(WS_NOAMMOFORMAIN);
			bots[i].f_shoot_time = gpGlobals->time;
			bots[i].SetClaymoreMinePlantTime(5.0f);
			bots[i].weapon_action = W_TAKEOTHER;

			ClientPrint(pEntity, HUD_PRINTCONSOLE, "Weapon usage set to claymore mine (main no ammo set too)\n");
		}

		return true;
	}

	//TEMP: need cmd to force weapon reload
	else if (FStrEq(pcmd, "fr") || FStrEq(pcmd, "forcedreload"))
	{
		int i = 0;

		if ((arg1 != NULL) && (*arg1 != 0))
		{
			i = util.FindBotByName(arg1);
		}

		if (i != -1 && bots[i].is_used)
		{
			bots[i].weapon_action = W_INRELOAD;
			bots[i].SetWeaponStatus(WS_NOTEMPTYMAG);

			ClientPrint(pEntity, HUD_PRINTCONSOLE, "forced reload initialized (partly used magazine set too)\n");
		}

		return true;
	}

	//TEMP: need cmd to disable checking for ammo so that the forced no ammo flags don't get reset
	else if (FStrEq(pcmd, "fdca") || FStrEq(pcmd, "forceddontcheckammo"))
	{
		int i = 0;

		if ((arg1 != NULL) && (*arg1 != 0))
		{
			i = util.FindBotByName(arg1);
		}

		if (i != -1 && bots[i].is_used)
		{
			if (bots[i].IsWeaponStatus(WS_TEST_DONTCHECKAMMO))
			{
				bots[i].RemoveWeaponStatus(WS_TEST_DONTCHECKAMMO);
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "forced 'dont check ammo' DISABLED\n");
			}
			else
			{
				bots[i].SetWeaponStatus(WS_TEST_DONTCHECKAMMO);
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "forced 'dont check ammo' ENABLED (noammo for main and/or backup won't get reset now)\n");
			}
		}

		return true;
	}

	else if (FStrEq(pcmd, "gbe"))
	{
		int start, end;

		if ((arg1 != NULL) && (*arg1 != 0))
		{
			int i = util.FindBotByName(arg1);

			if (i != -1 && bots[i].is_used)
			{
				start = i;
				end = i + 1;
			}
		}
		else
		{
			start = 0;
			end = MAX_CLIENTS;
		}

		for (int i = start; i < end; i++)
		{
			if ((bots[i].is_used) && devTool.IsCommandValidForThisBot(bots[i].pEdict))
			{
				if (bots[i].pBotEnemy)
				{
					ALERT(at_console, "(%s) My enemy name is %s || My target ent is %s || My target globname is %s || Distance to enemy is %.1f\n",
						bots[i].name, STRING(bots[i].pBotEnemy->v.netname), STRING(bots[i].pBotEnemy->v.classname), STRING(bots[i].pBotEnemy->v.globalname),
						bots[i].GetPrevDistanceToEnemy());
				}
				else
					ALERT(at_console, "Have no enemy at the moment\n");
			}
		}

		return true;
	}

	// TEMP: need cmd to write wpt and origins history!!!!!!!!!!
	else if (FStrEq(pcmd, "tbot"))
	{
		int start, end;

		if ((arg1 != NULL) && (*arg1 != 0))
		{
			int i = util.FindBotByName(arg1);

			if (i != -1 && bots[i].is_used)
			{
				start = i;
				end = i + 1;
			}
		}
		else
		{
			start = 0;
			end = MAX_CLIENTS;
		}

		for (int i = start; i < end; i++)
		{
			if ((bots[i].is_used) && devTool.IsCommandValidForThisBot(bots[i].pEdict))
			{
				sprintf(msg, "***<%s>\n", bots[i].name);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

				ALERT(at_console, "curwpt = %d | prevwpt0 = %d | prevwpt1 = %d | prevwpt2 = %d | prevwpt3 = %d | prevwpt4 = %d |\ncurwpt.x = %d | curwpt.y = %d | curwpt.z = %d| curpath = %d | prevpath = %d\n",
					bots[i].curr_wpt_index + 1, bots[i].prev_wpt_index.get() + 1, bots[i].prev_wpt_index.get(1) + 1, bots[i].prev_wpt_index.get(2) + 1,
					bots[i].prev_wpt_index.get(3) + 1, bots[i].prev_wpt_index.get(4) + 1, (int)bots[i].GetCurrWptPosition().x, (int)bots[i].GetCurrWptPosition().y,
					(int)bots[i].GetCurrWptPosition().z, bots[i].curr_path_index + 1, bots[i].prev_path_index + 1);

				//sprintf(msg, "curr wpt %d | ladder end wpt %d | op_path_dir %d || stuck_time %.2f | #OfTriesToUnstuck %d\n",
				sprintf(msg, "curr wpt %d | curr path %d | isTask(OppositePathDir) %d || stuck_time %.2f | #OfTriesToUnstuck %d\n", bots[i].curr_wpt_index + 1, bots[i].curr_path_index + 1,
					bots[i].IsTask(TASK_OPPOSITEPATHDIR), bots[i].GetGotStuckTime(), bots[i].GetUnstuckAttempts());
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

				sprintf(msg, "timeToReachCurrWpt %.1f | wpt_wait %.1f | wpt_action %.1f | globtime %.1f\n",
					bots[i].GetTimeToReachCurrWaypoint(), bots[i].GetWaitTime(), bots[i].GetActionTime(), gpGlobals->time);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

				ALERT(at_console, "bot team %d (model says %s)| bot class %d | bot buttons %d\n",
					bots[i].GetBotTeam(), STRING(bots[i].pEdict->v.model), bots[i].GetBotClass(), bots[i].pEdict->v.button);

				ALERT(at_console, "bot start action %d | notJoinedGame %d | respawnTryItAgin %d | bot velocity %.1f\n",
					bots[i].start_action, bots[i].IsBotFlag(BF_NOT_JOINED_GAME), bots[i].IsBotFlag(BF_RESPAWN_TRY_IT_AGAIN), bots[i].pEdict->v.velocity.Length2D());

				ALERT(at_console, "Acti_t %.1f | TANK %d | Bandages %d\n", bots[i].GetActionTime(), bots[i].IsTask(TASK_USETANK), bots[i].GetAmountOfBandages());

				sprintf(msg, "bot chute %d | chute used %d | chute time %.1f | flags %d\n",
					bots[i].IsTask(TASK_PARACHUTE), bots[i].IsSubTask(ST_PARACHUTE_USED), bots[i].GetParachuteUseTime(), bots[i].pEdict->v.flags);
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
		}

		return true;
	}

	//TEMP: need cmd to test health and healing abilities
	else if (FStrEq(pcmd, "tbh"))
	{
		char values[96]{};
		
		int start, end;

		if ((arg1 != NULL) && (*arg1 != 0))
		{
			int i = util.FindBotByName(arg1);

			if (i != -1 && bots[i].is_used)
			{
				start = i;
				end = i + 1;
			}
		}
		else
		{
			start = 0;
			end = MAX_CLIENTS;
		}

		for (int i = start; i < end; i++)
		{
			if ((bots[i].is_used) && devTool.IsCommandValidForThisBot(bots[i].pEdict))
			{
				strcpy(values, "<");

				if (bots[i].IsTask(TASK_BLEEDING))
					strcat(values, " bleeding ");
				if (bots[i].IsNeed(NEED_AIR))
					strcat(values, " air (aka drowning) ");
				if (bots[i].IsTask(TASK_IGNORE_ENEMY))
					strcat(values, " ignore_enemy ");

				strcat(values, ">");

				sprintf(msg, "bot <%s> task/need flags: %s | valTasks %d valNeeds %d\n", bots[i].name, values, bots[i].PrintTasks(), bots[i].PrintNeeds());
				ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);

				sprintf(msg, "bandageT %.1f | pausedT %.1f | speakT %.1f | globT %.1f\n", bots[i].GetBandageTime(), bots[i].GetPausedTime(), bots[i].GetSpeakTime(), gpGlobals->time);
				ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);

				sprintf(msg, "bothealth %d | prev_health %d | botbandages %d\n", bots[i].GetHealth(), bots[i].GetPrevHealth(), bots[i].GetAmountOfBandages());
				ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
			}
		}
		
		return true;
	}

	//TEMP: need cmd to test max speed value
	else if (FStrEq(pcmd, "tbms"))
	{
		int start, end;

		if ((arg1 != NULL) && (*arg1 != 0))
		{
			int i = util.FindBotByName(arg1);

			if (i != -1 && bots[i].is_used)
			{
				start = i;
				end = i + 1;
			}
		}
		else
		{
			start = 0;
			end = MAX_CLIENTS;
		}

		for (int i = start; i < end; i++)
		{
			if ((bots[i].is_used) && devTool.IsCommandValidForThisBot(bots[i].pEdict))
			{
				sprintf(msg, "max_speed is: %.2f\n", bots[i].GetMaxSpeed());
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
		}

		return true;
	}

	//TEMP: need cmd to test view angle value
	else if (FStrEq(pcmd, "tbva"))
	{
		int start, end;

		if ((arg1 != NULL) && (*arg1 != 0))
		{
			int i = util.FindBotByName(arg1);

			if (i != -1 && bots[i].is_used)
			{
				start = i;
				end = i + 1;
			}
		}
		else
		{
			start = 0;
			end = MAX_CLIENTS;
		}

		for (int i = start; i < end; i++)
		{
			if ((bots[i].is_used) && devTool.IsCommandValidForThisBot(bots[i].pEdict))
			{
				sprintf(msg, "viewAngle.x=%.2f | viewAngle.y=%.2f | current move_speed is: %d\n", bots[i].pEdict->v.v_angle.x, bots[i].pEdict->v.v_angle.y, bots[i].GetMoveSpeed());
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
		}

		return true;
	}

	//TEMP: remove it
	else if (FStrEq(pcmd, "tbf"))
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (bots[i].is_used)
			{
				ALERT(at_console, "bot <%s> client flags (as string representation):\n", bots[i].name);

				if (bots[i].pEdict->v.flags & FL_SPECTATOR)
					ALERT(at_console, "<fl spectator>");
				if (bots[i].pEdict->v.flags & FL_CLIENT)
					ALERT(at_console, "<fl client>");
				if (bots[i].pEdict->v.flags & FL_FAKECLIENT)
					ALERT(at_console, "<fl fakeclient>");
				if (bots[i].pEdict->v.flags & FL_NOTARGET)
					ALERT(at_console, "<fl notarget>");
			}
		}

		return true;
	}

	//TEMP: need cmd to test behaviour
	else if (FStrEq(pcmd, "tbb") || FStrEq(pcmd, "checkbehaviour"))
	{
		char bmsg[256]{};

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if ((bots[i].is_used) && devTool.IsCommandValidForThisBot(bots[i].pEdict))
			{
				sprintf(bmsg, "bot <%s> behaviour flags (as string representation):\n",	bots[i].name);
				ClientPrint(pEntity, HUD_PRINTCONSOLE, bmsg);

				if (bots[i].IsBehaviour(STANDARD))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<standard>");
				if (bots[i].IsBehaviour(ATTACKER))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<attacker>");
				if (bots[i].IsBehaviour(DEFENDER))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<defender>");

				if (bots[i].IsBehaviour(COMMON))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<common soldier>");
				if (bots[i].IsBehaviour(CQUARTER))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<close quarter>");
				if (bots[i].IsBehaviour(MGUNNER))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<mgunner>");
				if (bots[i].IsBehaviour(SNIPER))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<sniper>");
				if (bots[i].IsBehaviour(AASPEC))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<anti-armor specialist>");
				
				if (bots[i].IsBehaviour(BOT_PRECISION))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<bot precision>");

				if (bots[i].IsBehaviour(BOT_DONTGOPRONE))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<bot dont goprone>");
				if (bots[i].IsBehaviour(GOTO_STANDING))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<goto standing>");
				if (bots[i].IsBehaviour(GOTO_CROUCH))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<goto crouch>");
				if (bots[i].IsBehaviour(GOTO_PRONE))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<goto prone>");
				if (bots[i].IsBehaviour(BOT_STANDING))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<bot standing>");
				if (bots[i].IsBehaviour(BOT_CROUCHED))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<bot crouched>");
				if (bots[i].IsBehaviour(BOT_PRONED))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<bot proned>");

				sprintf(bmsg, "\n and behaviour as the number: %d\n", bots[i].PrintBehaviour());
				ClientPrint(pEntity, HUD_PRINTCONSOLE, bmsg);
			}
		}

		return true;
	}

	//TEMP: need cmd to test tasks
	else if (FStrEq(pcmd, "tbt") || FStrEq(pcmd, "checktasks"))
	{
		char tmsg[256]{};

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if ((bots[i].is_used) && devTool.IsCommandValidForThisBot(bots[i].pEdict))
			{
				sprintf(tmsg, "bot <%s> all his tasks as strings\n", bots[i].name);
				ClientPrint(pEntity, HUD_PRINTCONSOLE, tmsg);

				if (bots[i].IsTask(TASK_OPPOSITEPATHDIR))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<oppositepathdir>");
				if (bots[i].IsTask(TASK_DEATHFALL))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<deathfall>");
				if (bots[i].IsTask(TASK_DONTMOVEINCOMBAT))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<dontmoveincombat>");
				if (bots[i].IsTask(TASK_IGNOREWPTNAV))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<ignorewptnav>");
				if (bots[i].IsTask(TASK_FIRE))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<fire>");
				if (bots[i].IsTask(TASK_IGNOREAIMWPTS))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<ignoreaims>");
				if (bots[i].IsTask(TASK_PRECISEAIM))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<preciseaim>");
				if (bots[i].IsTask(TASK_CHECKAMMO))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<checkammo>");
				if (bots[i].IsTask(TASK_GOALITEM))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<goalitem>");
				if (bots[i].IsTask(TASK_SETCLAYMORE))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<setclaymore>");
				if (bots[i].IsTask(TASK_NOJUMP))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<nojump>");
				if (bots[i].IsTask(TASK_SPRINT))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<sprint>");
				if (bots[i].IsTask(TASK_WPTACTION))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<wptaction>");
				if (bots[i].IsTask(TASK_BACKTOPATROL))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<backtopatrol>");
				if (bots[i].IsTask(TASK_PARACHUTE))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<parachute>");
				if (bots[i].IsTask(TASK_BLEEDING))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<bleeding>");
				if (bots[i].IsTask(TASK_SPEAK))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<speak>");
				if (bots[i].IsTask(TASK_HEALHIM))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<healhim>");
				if (bots[i].IsTask(TASK_MEDEVAC))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<medevac>");
				if (bots[i].IsTask(TASK_FIND_ENEMY))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<find enemy>");
				if (bots[i].IsTask(TASK_USETANK))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<usetank>");
				if (bots[i].IsTask(TASK_BIPOD))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<bipod>");
				if (bots[i].IsTask(TASK_CLAY_IGNORE))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<clay ignore>");
				if (bots[i].IsTask(TASK_CLAY_EVADE))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<clay evade>");
				if (bots[i].IsTask(TASK_GOPRONE))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<goprone>");
				if (bots[i].IsTask(TASK_AVOID_ENEMY))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<avoidenemy>");
				if (bots[i].IsTask(TASK_IGNORE_ENEMY))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<ignoreenemy>");

				sprintf(tmsg, "\n and as the number %d\n", bots[i].PrintTasks());
				ClientPrint(pEntity, HUD_PRINTCONSOLE, tmsg);
			}
		}

		return true;
	}


	//TEMP: need cmd to test subtasks
	else if (FStrEq(pcmd, "tbst") || FStrEq(pcmd, "checksubtasks"))
	{
		char stmsg[256]{};

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if ((bots[i].is_used) && devTool.IsCommandValidForThisBot(bots[i].pEdict))
			{
				sprintf(stmsg, "bot <%s> all his subtasks as strings\n", bots[i].name);
				ClientPrint(pEntity, HUD_PRINTCONSOLE, stmsg);

				if (bots[i].IsSubTask(ST_AIM_GETAIMWPT))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<aim get aimwpt>");
				if (bots[i].IsSubTask(ST_AIM_FACEAIMWPT))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<aim face aimwpt>");
				if (bots[i].IsSubTask(ST_AIM_SETTIME))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<aim set time>");
				if (bots[i].IsSubTask(ST_AIM_ADJUSTAIM))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<aim adjust aim>");
				if (bots[i].IsSubTask(ST_AIM_DONE))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<aim done>");
				if (bots[i].IsSubTask(ST_FACEGENT_DONE))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<face gent done>");
				if (bots[i].IsSubTask(ST_FACEPOINTIS_DONE))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<face pointis done>");
				if (bots[i].IsSubTask(ST_BUTTON_USED))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<button_used>");
				if (bots[i].IsSubTask(ST_PARACHUTE_USED))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<parachute_used>");
				if (bots[i].IsSubTask(ST_MEDEVAC_ST))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<medevac stomach>");
				if (bots[i].IsSubTask(ST_MEDEVAC_H))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<medevac head>");
				if (bots[i].IsSubTask(ST_MEDEVAC_F))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<medevac feet>");
				if (bots[i].IsSubTask(ST_MEDEVAC_DONE))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<medevac done>");
				if (bots[i].IsSubTask(ST_TREAT))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<treat>");
				if (bots[i].IsSubTask(ST_HEAL))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<heal>");
				if (bots[i].IsSubTask(ST_GIVE))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<give>");
				if (bots[i].IsSubTask(ST_HEALED))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<healed>");
				if (bots[i].IsSubTask(ST_SAY_CEASEFIRE))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<say cease fire>");
				if (bots[i].IsSubTask(ST_FACEENEMY))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<face enemy>");
				if (bots[i].IsSubTask(ST_DOOR_OPEN))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<door open>");
				if (bots[i].IsSubTask(ST_TANK_SHORT))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<tank short>");
				if (bots[i].IsSubTask(ST_RANDOMCENTRE))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<random centre>");
				if (bots[i].IsSubTask(ST_USEEYESORIGIN))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<eyes origin>");
				if (bots[i].IsSubTask(ST_NOOTHERNADE))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<no other grenade>");
				if (bots[i].IsSubTask(ST_W_CLIP))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<weapon clip>");
				if (bots[i].IsSubTask(ST_CANTPRONE))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<cant prone>");
				if (bots[i].IsSubTask(ST_EVASIONSTARTED))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<evasion started>");

				if (bots[i].IsSubTask(ST_INAREA))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<in area>");
				if (bots[i].IsSubTask(ST_GOALITEM_BOMB))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<goal item bomb>");

				sprintf(stmsg, "\n and as number %d\n", bots[i].PrintSubTasks());
				ClientPrint(pEntity, HUD_PRINTCONSOLE, stmsg);
			}
		}

		return true;
	}

	//TEMP: need cmd to test needs
	else if (FStrEq(pcmd, "tbn") || FStrEq(pcmd, "checkneeds"))
	{
		char nmsg[256]{};

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if ((bots[i].is_used) && devTool.IsCommandValidForThisBot(bots[i].pEdict))
			{
				sprintf(nmsg, "bot <%s> all his needs as strings\n", bots[i].name);
				ClientPrint(pEntity, HUD_PRINTCONSOLE, nmsg);

				if (bots[i].IsNeed(NEED_POSTSPAWN_DECISIONS))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<postSpawn decisions>");
				if (bots[i].IsNeed(NEED_GOAL))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<goal>");
				if (bots[i].IsNeed(NEED_AMMO))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<ammo>");
				if (bots[i].IsNeed(NEED_NEXTWPT))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<next waypoint>");
				if (bots[i].IsNeed(NEED_RESETNAVIG))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<reset navigation>");
				if (bots[i].IsNeed(NEED_RESETCLAYMORE))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<reset claymore>");
				if (bots[i].IsNeed(NEED_BANDAGES))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<bandages>");
				if (bots[i].IsNeed(NEED_BANDAGES_NOT))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<bandages not>");
				if (bots[i].IsNeed(NEED_COMMITSUICIDE))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<commit suicide>");
				if (bots[i].IsNeed(NEED_AIR))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<air aka drowning>");
				if (bots[i].IsNeed(NEED_FIRETEAM))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<fireteam>");
				if (bots[i].IsNeed(NEED_FIRETEAM_NOT))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<fireteam not>");

				if (bots[i].IsNeed(NEED_RESETPARACHUTE))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<reset parachute>");

				sprintf(nmsg, "\n and then as the number %d\n", bots[i].PrintNeeds());
				ClientPrint(pEntity, HUD_PRINTCONSOLE, nmsg);
			}
		}

		return true;
	}


	//TEMP: need cmd to test weapon status
	else if (FStrEq(pcmd, "tbws") || FStrEq(pcmd, "checkweaponstatus"))
	{
		char wsmsg[512]{};
		
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if ((bots[i].is_used) && devTool.IsCommandValidForThisBot(bots[i].pEdict))
			{
				sprintf(wsmsg, "bot <%s> all his weapon status bits as strings\n", bots[i].name);
				ClientPrint(pEntity, HUD_PRINTCONSOLE, wsmsg);

				if (bots[i].IsWeaponStatus(WS_CHECKWEAPON))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<check weapon>");
				if (bots[i].IsWeaponStatus(WS_SILENCERCHECKED))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<silencer checked>");

				if (bots[i].IsWeaponStatus(WS_MOUNTSILENCER))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<mount silencer>");
				if (bots[i].IsWeaponStatus(WS_PRESSRELOAD))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<press reload btn>");
				if (bots[i].IsWeaponStatus(WS_INVALID))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<invalidated reloading/switching weapon>");
				if (bots[i].IsWeaponStatus(WS_NOTEMPTYMAG))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<not empty magazine>");
				if (bots[i].IsWeaponStatus(WS_MERGEMAGS1))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<merge magazines1>");
				if (bots[i].IsWeaponStatus(WS_MERGEMAGS2))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<merge magazines2>");
				if (bots[i].IsWeaponStatus(WS_NOAMMOFORMAIN))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<no ammo for main>");
				if (bots[i].IsWeaponStatus(WS_NOAMMOFORBACKUP))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<no ammo for backup>");
				if (bots[i].IsWeaponStatus(WS_SECONDARYMODEACTIVE))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<secondary mode active>");
				if (bots[i].IsWeaponStatus(WS_RELOADSECONDARY))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<reload secondary/GL attachement>");
				if (bots[i].IsWeaponStatus(WS_CANTBIPOD))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<can't bipod>");
				if (bots[i].IsWeaponStatus(WS_CLAYMOREGOAL))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<claymore goal>");
				if (bots[i].IsWeaponStatus(WS_CLAYMORETRIP))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<claymore trip>");
				if (bots[i].IsWeaponStatus(WS_CLAYMOREDONE))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<claymore done>");
				if (bots[i].IsWeaponStatus(WS_GRENADEAVAILABLE))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<grenade available>");
				if (bots[i].IsWeaponStatus(WS_GRENADEDEPLETED))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<grenade depleted>");
				if (bots[i].IsWeaponStatus(WS_GRENADEPINPULLED))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<grenade pin pulled>");

				if (bots[i].IsWeaponStatus(WS_DONTSWITCHTOOTHER))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<dont switch to other>");
				if (bots[i].IsWeaponStatus(WS_DROPAMMO))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<drop ammo>");


				sprintf(wsmsg, "\n and then as the number %d\n", bots[i].PrintWeaponStatus());
				ClientPrint(pEntity, HUD_PRINTCONSOLE, wsmsg);
			}
		}

		return true;
	}

	else if (FStrEq(pcmd, "tbbf") || FStrEq(pcmd, "checkbotflags"))
	{
		char bfmsg[256]{};

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if ((bots[i].is_used) && devTool.IsCommandValidForThisBot(bots[i].pEdict))
			{
				sprintf(bfmsg, "bot <%s> all his bot flags as strings\n", bots[i].name);
				ClientPrint(pEntity, HUD_PRINTCONSOLE, bfmsg);

				if (bots[i].IsBotFlag(BF_NOT_JOINED_GAME))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<not joined game>");
				if (bots[i].IsBotFlag(BF_MUST_BE_INITIALIZED))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<must be initialized>");
				if (bots[i].IsBotFlag(BF_RESPAWN_AT_ROUND_END))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<respawn at round end>");
				if (bots[i].IsBotFlag(BF_RESPAWN_TRY_IT_AGAIN))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "<respawn try it again>");


				sprintf(bfmsg, "\n and then as the number %d\n", bots[i].PrintBotFlags());
				ClientPrint(pEntity, HUD_PRINTCONSOLE, bfmsg);
			}
		}

		return true;
	}

	else if (FStrEq(pcmd, "checkbandages"))
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if ((bots[i].is_used) && devTool.IsCommandValidForThisBot(bots[i].pEdict))
			{
				sprintf(msg, "bandage count is %d (is bot bleeding? %d)\n", bots[i].GetAmountOfBandages(), bots[i].IsTask(TASK_BLEEDING));
				ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
			}
		}

		return true;

	}

	//TEMP: need cmd to test cramped space
	else if (FStrEq(pcmd, "tbcs"))
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (bots[i].is_used)
			{
				float currwptrange = 0.0f;
				if (bots[i].curr_wpt_index != NO_VAL)
					currwptrange = waypoints[bots[i].curr_wpt_index].range;

				float prevwptrange = 0.0f;
				if (bots[i].prev_wpt_index.get() != NO_VAL)
					prevwptrange = waypoints[bots[i].prev_wpt_index.get()].range;

				ALERT(at_console, "bot <%s> IsInCramped Space() #%d(1=true) | curr_wpt #%d | curr_wpt_range %.2f | prev_wpt #%d | prav_wpt_range %.2f | move speed %d | distToCurrWpt %.2f\n",
					bots[i].name, bots[i].IsInCrampedSpace(), bots[i].curr_wpt_index + 1, currwptrange,
					bots[i].prev_wpt_index.get() + 1, prevwptrange, bots[i].GetPrevMoveSpeed(), wptmanager.GetDistanceToWaypoint(bots[i].pEdict, bots[i].curr_wpt_index));
			}
		}

		return true;
	}

	//TEMP: need cmd to test path goal setting
	else if (FStrEq(pcmd, "tpg"))
	{
		if ((arg1 != NULL) && (*arg1 != 0))
		{
			int path_index = atoi(arg1);

			if (w_paths[path_index] == NULL)
			{
				if (w_paths[path_index]->flags & P_FL_MISC_GOAL_RED)
					ALERT(at_console, "Path #%d has a RED team goal priority\n", path_index + 1);
				if (w_paths[path_index]->flags & P_FL_MISC_GOAL_BLUE)
					ALERT(at_console, "Path #%d has a BLUE team goal priority\n", path_index + 1);
			}
		}
		else
		{
			for (int path_index = 0; path_index < num_w_paths; path_index++)
			{
				if (w_paths[path_index] == NULL)
					continue;
				if (w_paths[path_index]->flags & P_FL_MISC_GOAL_RED)
					ALERT(at_console, "Path #%d has a RED team goal priority\n", path_index + 1);
				if (w_paths[path_index]->flags & P_FL_MISC_GOAL_BLUE)
					ALERT(at_console, "Path #%d has a BLUE team goal priority\n", path_index + 1);
			}
		}

		ALERT(at_console, "\n");

		return true;
	}

	// TEMP: remove it
	else if (FStrEq(pcmd, "ttf"))
	{
		UTIL_MakeVectors(pEntity->v.v_angle);

		Vector vecStart = pEntity->v.origin + pEntity->v.view_ofs;
		Vector vecEnd = vecStart + gpGlobals->v_forward * 100;

		TraceResult tr;

		UTIL_TraceLine(vecStart, vecEnd, dont_ignore_monsters, dont_ignore_glass, pEntity, &tr);

		const char* texture = g_engfuncs.pfnTraceTexture(tr.pHit, vecStart, vecEnd);

		ALERT(at_console, "The name of the texture in front is %s\n", texture);

		return true;
	}

	
	else if (FStrEq(pcmd, "tcmval"))
	{
		float low_val = -1000;
		float hi_val = -1000;

		if ((arg1 != NULL) && (*arg1 != 0))
			low_val = atoi(arg1);

		if ((arg2 != NULL) && (*arg2 != 0))
			hi_val = atoi(arg2);

		if (low_val != -1000 && hi_val != -1000)
		{
			// make them float values
			low_val = low_val / 100;
			hi_val = hi_val / 100;

			float half_val = (low_val + hi_val) / 2;

			sprintf(msg, "The centre point between bottom value %.2f and top value %.2f is >> %.2f\nCentre-bottom is %.2f\nTop-centre is %.2f\n",
				low_val, hi_val, half_val, fabs(fabs(half_val) - fabs(low_val)), fabs(fabs(hi_val) - fabs(half_val)));
		}
		else
			sprintf(msg, "some error occured!\n");

		ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

		return true;
	}

	// TEMP: remove it
	else if (FStrEq(pcmd, "tud"))
	{
		sprintf(msg, "user dmg_inflictor %p\n", pEntity->v.dmg_inflictor);
		ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

		return true;
	}

	// TEMP to test breakable safe distance: remove it
	else if (FStrEq(pcmd, "tudfb"))
	{
		Vector temp;
		edict_t* efb = NULL;
		efb = util.FindEntityByClassname(efb, "func_breakable");

		// search the surrounding for entities
		while ((efb = util.FindEntityInSphere(efb, pEntity->v.origin, EXTENDED_SEARCH_RADIUS)) != NULL)
		{
			if (util.IsEntityName(efb, "func_breakable") == false)
				continue;

			temp = efb->v.origin - pEntity->v.origin;

			sprintf(msg, "user distance to nearby func breakable is %.3f\n", temp.Length());
			ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
		}

		return true;
	}

	// TEMP to test distance to given entity: remove it
	else if (FStrEq(pcmd, "tude"))
	{
		if ((arg1 != NULL) && (*arg1 != 0))
		{
			Vector temp;
			edict_t* ab = NULL;

			ab = util.FindEntityByClassname(ab, arg1);

			if (ab != NULL)
			{
				if (ab->v.origin == g_vecZero)
					temp = util.VecBModelOrigin(ab) - pEntity->v.origin;
				else
					temp = ab->v.origin - pEntity->v.origin;

				sprintf(msg, "user distance to %s is %.3f\n", arg1, temp.Length());
				ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);
			}
			else
			{
				ClientPrint(pEntity, HUD_PRINTNOTIFY, "there's no entity with such classname!\n");
			}
		}
		else
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "missing argument (entity classname)!\n");

		return true;
	}

	else if (FStrEq(pcmd, "tufov"))
	{
		sprintf(msg, "user FOV value is: %.2f\n", pEntity->v.fov);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);

		return true;
	}

	else if (FStrEq(pcmd, "tull") || FStrEq(pcmd, "tulightlevel"))
	{
		sprintf(msg, "user light_level value is: %d\n", pEntity->v.light_level);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);

		return true;
	}

	else if (FStrEq(pcmd, "tuv"))
	{
		sprintf(msg, "user v.velocity (length2D) value is: %.2f\n", pEntity->v.velocity.Length2D());
		ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);

		return true;
	}

	else if (FStrEq(pcmd, "tuwa"))
	{
		sprintf(msg, "weaponanimation %d | viewmodel %d | weaponmodel(what others see) %d\n", pEntity->v.weaponanim, pEntity->v.viewmodel, pEntity->v.weaponmodel);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);

		return true;
	}

	//TEMP: need cmd to test getting weapon IDs
	//else if (FStrEq(pcmd, "twid"))
	//{
	//	ALERT(at_console, "%s ID is %d\n", arg1, UTIL_GetIDFromName(arg1));
	//	ALERT(at_console, "weapon_knife ID is %d\n", UTIL_GetIDFromName("weapon_knife"));

	//	return true;
	//}

	// TEMP: remove it
	else if (FStrEq(pcmd, "tuo"))
	{
		char str[256]{};

		Vector vec = pEntity->v.origin + pEntity->v.view_ofs;

		sprintf(str, "Orig_x %0.2f | Orig_y %0.2f | Orig_z %0.2f\n", pEntity->v.origin.x, pEntity->v.origin.y, pEntity->v.origin.z);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

		sprintf(str, "angles_x %0.2f | angles_y %0.2f | angles_z %0.2f\n", pEntity->v.angles.x, pEntity->v.angles.y, pEntity->v.angles.z);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

		sprintf(str, "v_angles_x %0.2f | v_angles_y %0.2f | v_angles_z %0.2f\n", pEntity->v.v_angle.x, pEntity->v.v_angle.y, pEntity->v.v_angle.z);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

		sprintf(str, "Eyes_x %0.2f | Eyes_y %0.2f | Eyes_z %0.2f\n", pEntity->v.view_ofs.x, pEntity->v.view_ofs.y, pEntity->v.view_ofs.z);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, str);


		//sprintf(str, "Final_x %0.2f | Final_y %0.2f | Final_z %0.2f\n", vec.x, vec.y, vec.z);
		//ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

		sprintf(str, "absmin_x %0.2f | absmin_y %0.2f | absmin_z %0.2f\nabsmax_x %0.2f | absmax_y %0.2f | absmax_z %0.2f\n",
			pEntity->v.absmin.x, pEntity->v.absmin.y, pEntity->v.absmin.z, pEntity->v.absmax.x, pEntity->v.absmax.y, pEntity->v.absmax.z);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

		float x = (float)pEntity->v.absmin.x + (float)pEntity->v.absmax.x;
		float y = (float)pEntity->v.absmin.y + (float)pEntity->v.absmax.y;
		float z = (float)pEntity->v.absmin.z + (float)pEntity->v.absmax.z;
		sprintf(str, "absmin+absmax -> x %0.2f | y %0.2f | z %0.2f\n", x, y, z);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

		sprintf(str, "mins_x %0.2f | mins_y %0.2f | mins_z %0.2f\nmaxs_x %0.2f | maxs_y %0.2f | maxs_z %0.2f\n",
			pEntity->v.mins.x, pEntity->v.mins.y, pEntity->v.mins.z, pEntity->v.maxs.x, pEntity->v.maxs.y, pEntity->v.maxs.z);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

		sprintf(str, "size_x %0.2f | size_y %0.2f | size_z %0.2f\n", pEntity->v.size.x, pEntity->v.size.y, pEntity->v.size.z);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

		return true;
	}

	// TEMP: remove it
	else if (FStrEq(pcmd, "tuva"))
	{
		char str[256]{};

		Vector vec = pEntity->v.origin + pEntity->v.view_ofs;

		sprintf(str, "Orig_x %0.2f | Orig_y %0.2f | Orig_z %0.2f | and Eyes_z %0.2f\n",	pEntity->v.origin.x, pEntity->v.origin.y, pEntity->v.origin.z, pEntity->v.view_ofs.z);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

		sprintf(str, "v_angles_x %0.2f | V_ANGLES_Y %0.2f | v_angles_z %0.2f\n", pEntity->v.v_angle.x, pEntity->v.v_angle.y, pEntity->v.v_angle.z);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

		sprintf(str, "origin+view_ofs x %0.2f | y %0.2f | z %0.2f\n", vec.x, vec.y, vec.z);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

		return true;
	}

	// TEMP: remove it
	else if (FStrEq(pcmd, "tu"))
	{
		char str[128]{};

		sprintf(str, "gTime %.2f\n", gpGlobals->time);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

		sprintf(str, "iu1 %d | iu2 %d | iu3 %d | iu4 %d\n", pEntity->v.iuser1, pEntity->v.iuser2, pEntity->v.iuser3, pEntity->v.iuser4);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

		sprintf(str, "fu1 %.3f | fu2 %.3f | fu3 %.3f | fu4 %.3f\n", pEntity->v.fuser1, pEntity->v.fuser2, pEntity->v.fuser3, pEntity->v.fuser4);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

		sprintf(str, "vu1.x %.3f | vu1.y %.3f | vu1.z %.3f\n", pEntity->v.vuser1.x, pEntity->v.vuser1.y, pEntity->v.vuser1.z);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

		sprintf(str, "vu2.x %.3f | vu2.y %.3f | vu2.z %.3f\n", pEntity->v.vuser2.x, pEntity->v.vuser2.y, pEntity->v.vuser2.z);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

		sprintf(str, "vu3.x %.3f | vu3.y %.3f | vu3.z %.3f\n", pEntity->v.vuser3.x, pEntity->v.vuser3.y, pEntity->v.vuser3.z);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

		sprintf(str, "vu4.x %.3f | vu4.y %.3f | vu4.z %.3f\n", pEntity->v.vuser4.x, pEntity->v.vuser4.y, pEntity->v.vuser4.z);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

		sprintf(str, "flags %d || edict %p || pContainingEntity %p\n", pEntity->v.flags, pEntity, pEntity->v.pContainingEntity);
		ClientPrint(pEntity, HUD_PRINTNOTIFY, str);

		sprintf(str, "movetype %d | solid %d\n", pEntity->v.movetype, pEntity->v.solid);
		ClientPrint(pEntity, HUD_PRINTNOTIFY, str);

		sprintf(str, "bInDuck %d | flDucktime %d\n", pEntity->v.bInDuck, pEntity->v.flDuckTime);
		ClientPrint(pEntity, HUD_PRINTNOTIFY, str);

		sprintf(str, "Pitchspeed %.2f\n", pEntity->v.pitch_speed);
		ClientPrint(pEntity, HUD_PRINTNOTIFY, str);
		sprintf(str, "Yawspeed %.2f\n", pEntity->v.yaw_speed);
		ClientPrint(pEntity, HUD_PRINTNOTIFY, str);

		sprintf(str, "In team=%d\n", pEntity->v.team);
		ClientPrint(pEntity, HUD_PRINTNOTIFY, str);

		sprintf(str, "Using class=%d\n", pEntity->v.playerclass);
		ClientPrint(pEntity, HUD_PRINTNOTIFY, str);

		sprintf(str, "DeadFlag=%d\n", pEntity->v.deadflag);
		ClientPrint(pEntity, HUD_PRINTNOTIFY, str);

		return true;
	}

	//TEMP: remove it
	else if (FStrEq(pcmd, "tuwater"))
	{
		sprintf(msg, "current waterlevel value %d\n", pEntity->v.waterlevel);
		ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

		if (pEntity->v.flags & FL_INWATER)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_INWATER\n");

		return true;
	}

	else if (FStrEq(pcmd, "tuonground"))
	{
		ALERT(at_console, "standing still on ground??? <result(bool) %d> <flags %d>\n", (pEntity->v.flags & FL_ONGROUND) == FL_ONGROUND, pEntity->v.flags);

		return true;
	}

	//TEMP: remove it
	else if (FStrEq(pcmd, "tuspec"))
	{
		if (pEntity->v.flags & FL_SPECTATOR)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "fl spectator set\n");
		if (pEntity->v.flags & FL_CLIENT)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "fl client set\n");
		if (pEntity->v.flags & FL_FAKECLIENT)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "fl fakeclient set\n");
		if (pEntity->v.flags & FL_NOTARGET)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "fl notarget set\n");

		return true;
	}

	//TEMP: remove it
	else if (FStrEq(pcmd, "tuf"))
	{
		if (pEntity->v.flags & FL_ONGROUND)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_ONGROUND | ");
		if (pEntity->v.flags & FL_PARTIALGROUND)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_PARTIALGROUND | ");
		if (pEntity->v.flags & FL_INWATER)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_INWATER | ");
		if (pEntity->v.flags & FL_WATERJUMP)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_WATERJUMP | ");
		if (pEntity->v.flags & FL_FROZEN)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_FROZEN | ");
		if (pEntity->v.flags & FL_FLOAT)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_FLOAT | ");
		if (pEntity->v.flags & FL_FLY)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_FLY | ");
		if (pEntity->v.flags & FL_CONVEYOR)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_CONVEYOR | ");
		if (pEntity->v.flags & FL_GRAPHED)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_GRAPHED | ");
		if (pEntity->v.flags & FL_SWIM)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_SWIM | ");
		if (pEntity->v.flags & FL_CLIENT)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_CLIENT | ");
		if (pEntity->v.flags & FL_MONSTER)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_MONSTER | ");
		if (pEntity->v.flags & FL_GODMODE)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_GODMODE | ");
		if (pEntity->v.flags & FL_NOTARGET)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_NOTARGET | ");
		if (pEntity->v.flags & FL_SKIPLOCALHOST)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_SKIPLOCALHOST | ");
		if (pEntity->v.flags & FL_FAKECLIENT)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_FAKECLIENT | ");
		if (pEntity->v.flags & FL_DUCKING)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_DUCKING | ");
		if (pEntity->v.flags & FL_IMMUNE_WATER)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_IMMUNE_WATER | ");
		if (pEntity->v.flags & FL_IMMUNE_SLIME)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_IMMUNE_SLIME | ");
		if (pEntity->v.flags & FL_IMMUNE_LAVA)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_IMMUNE_LAVA | ");
		if (pEntity->v.flags & FL_PROXY)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_PROXY | ");
		if (pEntity->v.flags & FL_ALWAYSTHINK)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_ALWAYSTHINK | ");
		if (pEntity->v.flags & FL_BASEVELOCITY)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_BASEVELOCITY | ");
		if (pEntity->v.flags & FL_MONSTERCLIP)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_MONSTERCLIP | ");
		if (pEntity->v.flags & FL_ONTRAIN)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_ONTRAIN | ");
		if (pEntity->v.flags & FL_WORLDBRUSH)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_WORLDBRUSH | ");
		if (pEntity->v.flags & FL_SPECTATOR)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_SPECTATOR | ");
		if (pEntity->v.flags & (1<<27))
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "1<<27 (something) | ");
		if (pEntity->v.flags & FL_BROKENLEG)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "1<<28 (something2) | ");
		if (pEntity->v.flags & FL_CUSTOMENTITY)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_CUSTOMENTITY | ");
		if (pEntity->v.flags & FL_KILLME)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_KILLME | ");
		if (pEntity->v.flags & FL_DORMANT)
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "FL_DORMANT | ");

		ClientPrint(pEntity, HUD_PRINTNOTIFY, "\n");

		return true;
	}

	else if (FStrEq(pcmd, "twta") || FStrEq(pcmd, "testwpttriggerarray") || FStrEq(pcmd, "testwpttriggerarrays"))
	{
		char str[256]{};

		for (int i = 0; i < MAX_TRIGGERS; i++)
		{
			sprintf(str, "TriggerEvents.name=(%d) vs. TriggerGameState.name=(%d)\n", trigger_events[i].name, trigger_gamestate[i].GetName());
			ClientPrint(pEntity, HUD_PRINTCONSOLE, str);
		}

		return true;
	}

	// bot force (something) commands
	else if (FStrEq(pcmd, "ffs"))
	{
		if (bots[0].is_used == false)
			return true;

		if (bots[0].is_forced)
		{
			bots[0].is_forced = false;
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "forced stance DISABLED\n");
		}
		else
		{
			bots[0].is_forced = true;
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "forced stance ENABLED\n");
		}
		return true;
	}

	else if (FStrEq(pcmd, "fsnow"))
	{
		if (bots[0].is_used)
		{
			bots[0].is_forced = true;
			bots[0].SetStance(bots[0].forced_stance);
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "enforcing forced stance NOW\n");
		}

		return true;
	}

	else if (FStrEq(pcmd, "fss"))
	{
		if (bots[0].is_used)
		{
			bots[0].is_forced = true;
			bots[0].forced_stance = GOTO_STANDING;
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "forced stance set to STANDING\n");
		}

		return true;
	}
	else if (FStrEq(pcmd, "fsc"))
	{
		if (bots[0].is_used)
		{
			bots[0].is_forced = true;
			bots[0].forced_stance = GOTO_CROUCH;
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "forced stance set to CROUCH\n");
		}

		return true;
	}
	else if (FStrEq(pcmd, "fsp"))
	{
		if (bots[0].is_used)
		{
			bots[0].is_forced = true;
			bots[0].forced_stance = GOTO_PRONE;
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "forced stance set to PRONE\n");
		}

		return true;
	}
	else if (FStrEq(pcmd, "frwt") || FStrEq(pcmd, "forcedresetwaittime"))
	{
		if (bots[0].is_used)
		{
			bots[0].SetWaitTime(-0.2f);
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "forced wait time reset done\n");
		}

		return true;
	}
	else if (FStrEq(pcmd, "fupa") || FStrEq(pcmd, "forceduseprimary") || FStrEq(pcmd, "forceduseprimaryattack") || FStrEq(pcmd, "forceduseprimarybutton"))
	{
		if (bots[0].is_used)
		{
			bots[0].SetWeaponStatus(WS_TEST_INATTACK);
			bots[0].f_shoot_time = gpGlobals->time + 2.9f;	// abit longer than what switching to gl takes on m16
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "forced use of primary attack done\n");
		}

		return true;
	}
	else if (FStrEq(pcmd, "fusa") || FStrEq(pcmd, "forcedusesecondary") || FStrEq(pcmd, "forcedusesecondaryattack") || FStrEq(pcmd, "forcedusesecondarybutton"))
	{
		if (bots[0].is_used)
		{
			bots[0].SetWeaponStatus(WS_TEST_INATTACK2);
			bots[0].f_shoot_time = gpGlobals->time + 2.9f;	// abit longer than what switching to gl takes on m16
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "forced use of secondary attack done\n");
		}

		return true;
	}
	else if (FStrEq(pcmd, "dumpuser"))
	{
		util.DumpEdictToFile(pEntity);
		return true;
	}
	else if ((FStrEq(pcmd, "dumpbot")) || (FStrEq(pcmd, "db")))
	{
		if ((arg1 != NULL) && (*arg1 != 0))
		{
			int index = util.FindBotByName(arg1);

			if (index != -1)
				util.DumpEdictToFile(bots[index].pEdict);
		}
		else
		{
			for (int i = 0; i < MAX_CLIENTS; i++)
			{
				if (bots[i].is_used)
					util.DumpEdictToFile(bots[i].pEdict);
			}
		}

		return true;
	}
	else if ((FStrEq(pcmd, "dumpent")) || (FStrEq(pcmd, "dumpentity")))
	{
		edict_t* pent = NULL;
		float radius = STANDARD_SEARCH_RADIUS;
		char str[256]{};

		if ((arg1 != NULL) && (*arg1 != 0))
		{
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "scanning entities in sphere...\n");

			bool noresults = true;

			while ((pent = util.FindEntityInSphere(pent, pEntity->v.origin, radius)) != NULL)
			{
				if (util.IsEntityName(pent, arg1))
				{
					noresults = false;

					sprintf(str, "Found %s (pent %p) has been dumped to file!\n", STRING(pent->v.classname), pent);
					ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

					util.DumpEdictToFile(pent);
				}
			}

			if (noresults)
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "Found nothing here!\n");
		}
		else
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "Nothing. Specify entity name!\n");

		return true;
	}
	// cmd to test clients array
	else if (FStrEq(pcmd, "dumpclients"))
	{
		FILE* cfp;
		cfp = fopen(debug_fname, "a");

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			fprintf(cfp, "\n********Dumping the client_t array slot #%d********\n", i);
			//fprintf(cfp, "pEntity: %x\n", clients[i].pEntity);
			fprintf(cfp, "pEntity: %p\n", clients[i].pEntity);
			fprintf(cfp, "client_is_human(bool): %d\n", clients[i].IsHuman());
			fprintf(cfp, "client_bleeds(bool): %d\n", clients[i].IsBleeding());
			fprintf(cfp, "number of humans in game: %d\n", clients[i].HumanCount());
			fprintf(cfp, "number of bots in game: %d\n", clients[i].BotCount());
			fprintf(cfp, "total number of clients: %d\n", clients[i].ClientCount());
			fprintf(cfp, "*********END OF THIS DUMP*********\n");
		}

		fclose(cfp);

		ClientPrint(pEntity, HUD_PRINTNOTIFY, "Whole clients array has been written to debugging file!\n");

		return true;
	}
	else if (FStrEq(pcmd, "search"))
	{
		edict_t* pent = NULL;
		float radius = STANDARD_SEARCH_RADIUS;
		char str[128]{};

		if (FStrEq(arg1, "1"))
			radius = EXTENDED_SEARCH_RADIUS;
		else if (FStrEq(arg1, "2"))
			radius = STANDARD_SEARCH_RADIUS / 2.0f;

		ClientPrint(pEntity, HUD_PRINTCONSOLE, "searching...\n");

		while ((pent = util.FindEntityInSphere(pent, pEntity->v.origin, radius)) != NULL)
		{
			sprintf(str, "Found %s at %.2f %.2f %.2f\n", STRING(pent->v.classname), pent->v.origin.x, pent->v.origin.y, pent->v.origin.z);
			ClientPrint(pEntity, HUD_PRINTCONSOLE, str);
		}

		ClientPrint(pEntity, HUD_PRINTCONSOLE, "usage: noarg is std radius, arg=1 is extd radius, arg=2 is half of std radius\n");

		return true;
	}
	else if (FStrEq(pcmd, "scan"))
	{
		edict_t* pent = NULL;
		float radius = STANDARD_SEARCH_RADIUS;
		char str[128]{};
		Vector entity_o;

		if (FStrEq(arg1, "1"))
			radius = EXTENDED_SEARCH_RADIUS;
		else if (FStrEq(arg1, "2"))
			radius = STANDARD_SEARCH_RADIUS / 2.0f;

		ClientPrint(pEntity, HUD_PRINTCONSOLE, "scanning...\n");

		while ((pent = util.FindEntityInSphere(pent, pEntity->v.origin, radius)) != NULL)
		{
			entity_o = util.VecBModelOrigin(pent);

			sprintf(str, "Found %s at %.2f %.2f %.2f\n", STRING(pent->v.classname), entity_o.x, entity_o.y, entity_o.z);
			ClientPrint(pEntity, HUD_PRINTCONSOLE, str);
		}

		ClientPrint(pEntity, HUD_PRINTCONSOLE, "usage: noarg is std radius, arg=1 is extd radius, arg=2 is half of std radius\n");

		return true;
	}
	else if (FStrEq(pcmd, "dscan"))
	{
		edict_t* pent = NULL;
		float radius = STANDARD_SEARCH_RADIUS;
		char str[128]{};
		Vector entity_o;

		if ((arg1 != NULL) && (*arg1 != 0))
		{
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "scanning the entity...\n");

			while ((pent = util.FindEntityByClassname(pent, arg1)) != NULL)
			{
				entity_o = util.VecBModelOrigin(pent);

				sprintf(str, "Found %s at %.2f %.2f %.2f (pent %p)\n", STRING(pent->v.classname), entity_o.x, entity_o.y, entity_o.z, pent);
				ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

				sprintf(str, "flags %d || deadflag %d || me (edict) %p\n", pent->v.flags, pent->v.deadflag, pEntity);
				ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

				if (pent->v.model != NULL)
				{
					sprintf(str, "string1 %s\n", STRING(pent->v.model));
					ClientPrint(pEntity, HUD_PRINTCONSOLE, str);
				}


				Vector v_myhead = pEntity->v.origin + pEntity->v.view_ofs;
				TraceResult tr;
				UTIL_TraceLine(v_myhead, entity_o, ignore_monsters, pEntity, &tr);

				sprintf(str, "TraceResult: flfract %.2f | phit name %s\n", tr.flFraction, STRING(tr.pHit->v.classname));
				ClientPrint(pEntity, HUD_PRINTCONSOLE, str);
			}
		}

		return true;
	}
	else if ((FStrEq(pcmd, "scangrenades")) || (FStrEq(pcmd, "scangr")))
	{
		edict_t* pent = NULL;
		edict_t* pEdict = pEntity;
		char msg[128]{};

		while ((pent = util.FindEntityInSphere(pent, pEdict->v.origin, STANDARD_SEARCH_RADIUS)) != NULL)
		{
			if ((util.IsEntityName(pent, "grenade") == false) && (util.IsEntityName(pent, "grenade2") == false))
				continue;

			if ((pent->v.solid == SOLID_NOT) && (pent->v.movetype == MOVETYPE_FOLLOW))
				continue;

			ClientPrint(pEntity, HUD_PRINTCONSOLE, "FOUND some grenade AROUND\n");
			util.DumpEdictToFile(pent);

			sprintf(msg, "grenade's owner team is %d\n", util.GetTeam(pent));
			ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
			break;
		}

		if (pent == NULL)
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "no grenade around\n");

		return true;
	}
	// TEMP: only a test of one of bots routines
	else if (FStrEq(pcmd, "scanbreak"))
	{
		edict_t* pent = NULL;
		edict_t* pEdict = pEntity;
		float radius = STANDARD_SEARCH_RADIUS;
		bool found = false;

		if (FStrEq(arg1, "1"))
			radius = EXTENDED_SEARCH_RADIUS;
		else if (FStrEq(arg1, "2"))
			radius = EXTENDED_SEARCH_RADIUS * 2.0f;
		else if (FStrEq(arg1, "3"))
			radius = STANDARD_SEARCH_RADIUS / 2.0f;

		// search the surrounding for entities
		while ((pent = util.FindEntityInSphere(pent, pEdict->v.origin, radius)) != NULL)
		{
			if ((util.IsEntityName(pent, "func_breakable") == false) && (util.IsEntityName(pent, "fa_sd_object") == false))
				continue;

			if (util.NotBreakableByGunfire(pent) || (pent->v.impulse == 1))
				continue;
			
			Vector entity_origin = util.VecBModelOrigin(pent);

			TraceResult tr;
			Vector v_src = pEdict->v.origin + pEdict->v.view_ofs;

			UTIL_TraceLine(v_src, entity_origin, dont_ignore_monsters, pEdict, &tr);

			if (util.IsEntityName(tr.pHit, "func_breakable") || util.IsEntityName(tr.pHit, "fa_sd_object"))
			{
				found = true;
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "FOUND a BREAKABLE (via classname) somewhere AROUND\n");
			}
			// this should catch object that has a hole in the middle
			else if ((tr.flFraction == 1.0) && (pent->v.solid == SOLID_BSP))
			{
				found = true;
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "FOUND a BREAKABLE (via fraction == 1.0) somewhere AROUND\n");
			}
		}

		if (found == false)
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "no object there OR it cannot be destroyed\n");

		ClientPrint(pEntity, HUD_PRINTCONSOLE, "usage: noarg is std radius, arg=1 is extd radius, arg=2 is doubled extd radius, arg=3 is half of std radius\n");

		return true;
	}
	else if (FStrEq(pcmd, "scansdobject"))
	{
		edict_t* pent = NULL;
		edict_t* pEdict = pEntity;
		bool found = false;

		// search the surrounding for entities
		while ((pent = util.FindEntityInSphere(pent, pEdict->v.origin, EXTENDED_SEARCH_RADIUS)) != NULL)
		{
			if (util.IsEntityName(pent, "fa_sd_object") == false)
				continue;

			if (util.NotBreakableByGunfire(pent) || (pent->v.impulse != 1))
				continue;

			if (((pent->v.team == teamONE.GetTeamId()) && (pEdict->v.team == teamTWO.GetTeamId())) || ((pent->v.team == teamTWO.GetTeamId()) && (pEdict->v.team == teamONE.GetTeamId())))
				continue;


			Vector entity_origin = util.VecBModelOrigin(pent);

			TraceResult tr;
			Vector v_src = pEdict->v.origin + pEdict->v.view_ofs;

			UTIL_TraceLine(v_src, entity_origin, dont_ignore_monsters, pEdict, &tr);

			if (util.IsEntityName(tr.pHit, "fa_sd_object"))
			{
				found = true;
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "FOUND S&D OBJECT (via classname) somewhere AROUND\n");
			}
			
			if ((tr.flFraction == 1.0) && (pent->v.solid == SOLID_BSP))
			{
				found = true;
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "FOUND S&D OBJECT (via fraction == 1.0) somewhere AROUND\n");
			}
		}

		if (found == false)
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "no object there OR it cannot be destroyed (by your team)\n");

		return true;
	}
	else if (FStrEq(pcmd, "tracef"))
	{
		char str[128]{};
		Vector v_src, v_des;
		TraceResult tr;

		UTIL_MakeVectors(pEntity->v.v_angle);

		v_src = pEntity->v.origin + pEntity->v.view_ofs;
		v_des = v_src + gpGlobals->v_forward * EXTENDED_SEARCH_RADIUS;

		if (FStrEq(arg1, "1"))
		{
			UTIL_TraceLine(v_src, v_des, dont_ignore_monsters, dont_ignore_glass, pEntity->v.pContainingEntity, &tr);
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "tracing (dont ignore monsters & glass) forward from eyes...\n");
		}
		else if (FStrEq(arg1, "2"))
		{
			UTIL_TraceLine(v_src, v_des, ignore_monsters, ignore_glass, pEntity->v.pContainingEntity, &tr);
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "tracing (IGNORE monsters and glass) forward from eyes...\n");
		}
		else
		{
			UTIL_TraceLine(v_src, v_des, dont_ignore_monsters, pEntity->v.pContainingEntity, &tr);
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "tracing (dont ignore monsters) forward from eyes...\n");
		}

		sprintf(str, "Result=%.2f (%s)\nsrc_x%.2f src_y%.2f src_z%.2f\ndes_x%.2f des_y%.2f des_z%.2f\n", tr.flFraction, STRING(tr.pHit->v.classname), v_src.x, v_src.y, v_src.z, v_des.x, v_des.y, v_des.z);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

		sprintf(str, "tr.vecEndPos_x:%.2f tr.vecEndPos_y:%.2f tr.vecEndPos_z:%.2f\n", tr.vecEndPos.x, tr.vecEndPos.y, tr.vecEndPos.z);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

		ClientPrint(pEntity, HUD_PRINTCONSOLE, "use options: noarg, arg=1 or arg=2\n");

		return true;
	}
	else if (FStrEq(pcmd, "tracecrouchunder") || FStrEq(pcmd, "traceduckunder") || FStrEq(pcmd, "testcrouch"))
	{
		if (bots[0].is_used)
		{
			if (bots[0].pEdict->v.flags & FL_DUCKING)
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "Bot is fully crouched now\n");

			if (BotCanDuckUnder(&bots[0]))
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "canduckunder check passed\n");
			else
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "canduckunder check failed\n");
		}
		else
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "No bot in game to test Can Duck Under!\n");

		return true;
	}
	else if (FStrEq(pcmd, "tracejump") || FStrEq(pcmd, "tracejumpup") || FStrEq(pcmd, "testjump"))
	{
		if (bots[0].is_used)
		{
			if (bots[0].pEdict->v.flags & FL_DUCKING)
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "Bot is fully crouched now\n");

			if ((arg1 != NULL) && (*arg1 != 0) && FStrEq(arg1, "1"))
			{
				if (BotCanDuckJumpUp(&bots[0]))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "duckjump check passed\n");
				else
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "duckjump check failed\n");
			}

			else if ((arg1 != NULL) && (*arg1 != 0) && FStrEq(arg1, "2"))
			{
				if (BotCanDuckJumpInto(&bots[0]))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "duckjump INTO check passed\n");
				else
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "duckjump INTO check failed\n");
			}

			else
			{
				if (BotCanJumpUp(&bots[0]))
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "jump check passed\n");
				else
					ClientPrint(pEntity, HUD_PRINTCONSOLE, "jump check failed\n");
			}
			
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "\nSo see also the tracelines type showmethelines to turn that feature on!\n");
		}
		else
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "No bot in game to test Trace Jump Up!\n");

		ClientPrint(pEntity, HUD_PRINTCONSOLE, "\nOptions are: no arg = jump up, 1 = duckjump up, 2 = duckjump into\n");

		return true;
	}
	// to test what bot sees when checking for enemy
	else if (FStrEq(pcmd, "testvis"))
	{
		if (bots[0].is_used)
		{
			int visibility = util.IsPlayerVisible(bots[0].pEdict->v.origin, pEntity);

			if (visibility == VIS_YES)
			{
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "My enemy is visible!\n");
			}
			else if (visibility == VIS_FENCE)
			{
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "My enemy is behind FENCE!\n");
			}
			else
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "I don't see my target!!!\n");
		}
		else
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "No bots in game!\n");

		return true;
	}
	// to test what bot sees when checking for enemy
	else if (FStrEq(pcmd, "testvis2"))
	{
		TraceResult tr;

		if (bots[0].is_used)
		{
			UTIL_TraceLine(pEntity->v.origin + pEntity->v.view_ofs, bots[0].pEdict->v.origin + bots[0].pEdict->v.view_ofs, dont_ignore_monsters, dont_ignore_glass, pEntity, &tr);

			char strs[128]{};
			sprintf(strs, "User->Bot Result=%.2f | entity=%s(netname=%s)(target=%s)(targetname=%s)\n",
				tr.flFraction, STRING(tr.pHit->v.classname), STRING(tr.pHit->v.netname), STRING(tr.pHit->v.target), STRING(tr.pHit->v.targetname));
			ClientPrint(pEntity, HUD_PRINTCONSOLE, strs);
		}
		else
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "No bots in game!\n");

		return true;
	}
	else if ((FStrEq(pcmd, "traceentities")) || (FStrEq(pcmd, "traceentity")) || (FStrEq(pcmd, "traceent")) || (FStrEq(pcmd, "scanentities")) || (FStrEq(pcmd, "scanentity")) || (FStrEq(pcmd, "scanent")))
	{
		edict_t* pent = NULL;
		float radius = STANDARD_SEARCH_RADIUS;
		char str[128]{};

		if ((arg1 != NULL) && (*arg1 != 0))
		{
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "scanning the entity...\n");

			bool noresults = true;

			while ((pent = util.FindEntityInSphere(pent, pEntity->v.origin, radius)) != NULL)
			{
				if (util.IsEntityName(pent, arg1))
				{
					noresults = false;

					sprintf(str, "Found %s (pent %p)\ngetting more details about it...\n", STRING(pent->v.classname), pent);
					ClientPrint(pEntity, HUD_PRINTCONSOLE, str);
					
					const char* texture = g_engfuncs.pfnTraceTexture(pent, pEntity->v.origin, pent->v.origin);

					sprintf(str, "(rendermode %d) (renderamt %.2f) (renderfx %d) (health %.2f) (impulse %d)\n", pent->v.rendermode, pent->v.renderamt, pent->v.renderfx, pent->v.health, pent->v.impulse);
					ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

					sprintf(str, "(spawnflags %d) (flags %d) (solid %d) (movetype %d) (texture=<%s>)\n", pent->v.spawnflags, pent->v.flags, pent->v.solid, pent->v.movetype, texture);
					ClientPrint(pEntity, HUD_PRINTCONSOLE, str);
				}
			}

			if (noresults)
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "Found nothing here!\n");
		}
		else
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "Nothing. Specify entity name!\n");

		return true;
	}
	else if ((FStrEq(pcmd, "testentitylength")) || (FStrEq(pcmd, "testentlen")) || (FStrEq(pcmd, "getentlen")) || (FStrEq(pcmd, "getentitydistance")) || (FStrEq(pcmd, "getentdist")))
	{
		edict_t* pent = NULL;
		float radius = STANDARD_SEARCH_RADIUS;
		char str[128]{};

		if ((arg1 != NULL) && (*arg1 != 0))
		{
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "scanning the entity...\n");

			bool noresults = true;

			while ((pent = util.FindEntityInSphere(pent, pEntity->v.origin, radius)) != NULL)
			{
				if (util.IsEntityName(pent, arg1))
				{
					noresults = false;

					Vector entity_origin = util.VecBModelOrigin(pent);
					Vector entity = entity_origin - pEntity->v.origin;

					sprintf(str, "Found %s (pent %p) | distance %.2f\n", STRING(pent->v.classname), pent, entity.Length());
					ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

					// draw a beam to this entity
					util.HighlightTrace(pEntity->v.origin + pEntity->v.view_ofs, entity_origin, pEntity);
				}
			}

			if (noresults)
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "Found nothing here!\n");
		}
		else
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "Nothing. Specify entity name!\n");

		return true;
	}
	else if ((FStrEq(pcmd, "testentitylength2")) || (FStrEq(pcmd, "testentlen2")) || (FStrEq(pcmd, "getentlen2")) || (FStrEq(pcmd, "getentitydistance2")) || (FStrEq(pcmd, "getentdist2")))
	{
		edict_t* pent = NULL;
		float radius = EXTENDED_SEARCH_RADIUS;
		char str[128]{};

		if ((arg1 != NULL) && (*arg1 != 0))
		{
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "scanning the entity...\n");

			bool noresults = true;

			while ((pent = util.FindEntityInSphere(pent, pEntity->v.origin, radius)) != NULL)
			{
				if (util.IsEntityName(pent, arg1))
				{
					noresults = false;

					Vector entity_origin = util.VecBModelOrigin(pent);
					Vector entity = entity_origin - pEntity->v.origin;

					sprintf(str, "Found %s (pent %p) | distance %.2f\n", STRING(pent->v.classname), pent, entity.Length());
					ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

					// draw a beam to this entity
					util.HighlightTrace(pEntity->v.origin + pEntity->v.view_ofs, entity_origin, pEntity);
				}
			}

			if (noresults)
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "Found nothing here!\n");
		}
		else
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "Nothing. Specify entity name!\n");

		return true;
	}
	else if ((FStrEq(pcmd, "testentityorigin")) || (FStrEq(pcmd, "testentorigin")) || (FStrEq(pcmd, "testentityposition")) || (FStrEq(pcmd, "testentpos")))
	{
		edict_t* pent = NULL;
		float radius = EXTENDED_SEARCH_RADIUS;
		char str[128]{};

		if ((arg1 != NULL) && (*arg1 != 0))
		{
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "scanning the entity...\n");

			bool noresults = true;

			while ((pent = util.FindEntityInSphere(pent, pEntity->v.origin, radius)) != NULL)
			{
				if (util.IsEntityName(pent, arg1))
				{
					noresults = false;

					Vector bmodel = util.VecBModelOrigin(pent);
					Vector altpos = util.VecAbsoluteOrigin(pent);

					sprintf(str, "Found %s (pent %p) | origin= (%.1f, %.1f, %.1f)\n", STRING(pent->v.classname), pent, pent->v.origin.x, pent->v.origin.y, pent->v.origin.z);
					ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

					sprintf(str, "BModel(absmin+size)= (%.1f, %.1f, %.1f) | AbsMax+Absmin= (%.1f, %.1f, %.1f)\n", bmodel.x, bmodel.y, bmodel.z, altpos.x, altpos.y, altpos.z);
					ClientPrint(pEntity, HUD_PRINTCONSOLE, str);
				}
			}

			if (noresults)
				ClientPrint(pEntity, HUD_PRINTCONSOLE, "Found nothing here!\n");
		}
		else
			ClientPrint(pEntity, HUD_PRINTCONSOLE, "Nothing. Specify entity name!\n");

		return true;
	}
	/*/
	else if (FStrEq(pcmd, "getmapgoal"))
	{
		sprintf(msg, "IsMapGoalBased on Expl. is <%d> (0=false, 1=true)\n", internals.IsMapGoalBasedOnExplosives());
		ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
		return true;
	}
	// allows us to move with all waypoints to desired direction
	else if (FStrEq(pcmd, "movewptsbyx"))
	{
		if ((arg1 != NULL) && (*arg1 != 0))
		{
			extern void ShiftWpts(int x, int y, int z);

			int temp = atoi(arg1);
			ShiftWpts(temp, 0, 0);
		}
		return true;
	}
	// negative shift
	else if (FStrEq(pcmd, "movewptsbynx"))
	{
		if ((arg1 != NULL) && (*arg1 != 0))
		{
			extern void ShiftWpts(int x, int y, int z);

			int temp = atoi(arg1);
			ShiftWpts(-temp, 0, 0);
		}
		return true;
	}
	else if (FStrEq(pcmd, "movewptsbyy"))
	{
		if ((arg1 != NULL) && (*arg1 != 0))
		{
			extern void ShiftWpts(int x, int y, int z);

			int temp = atoi(arg1);
			ShiftWpts(0, temp, 0);
		}
		return true;
	}
	else if (FStrEq(pcmd, "movewptsbyny"))
	{
		if ((arg1 != NULL) && (*arg1 != 0))
		{
			extern void ShiftWpts(int x, int y, int z);

			int temp = atoi(arg1);
			ShiftWpts(0, -temp, 0);
		}
		return true;
	}
	else if (FStrEq(pcmd, "movewptsbyz"))
	{
		if ((arg1 != NULL) && (*arg1 != 0))
		{
			extern void ShiftWpts(int x, int y, int z);

			int temp = atoi(arg1);
			ShiftWpts(0, 0, temp);
		}
		return true;
	}
	else if (FStrEq(pcmd, "movewptsbynz"))
	{
		if ((arg1 != NULL) && (*arg1 != 0))
		{
			extern void ShiftWpts(int x, int y, int z);

			int temp = atoi(arg1);
			ShiftWpts(0, 0, -temp);
		}
		return true;
	}
	else if (FStrEq(pcmd, "getdist"))
	{
		if ((arg1 != NULL) && (*arg1 != 0) && (arg2 != NULL) && (*arg2 != 0))
		{
			sprintf(msg, "dist=%.2f\n", wptmanager.GetDistanceBetweenWaypoints(atoi(arg1)-1, atoi(arg2)-1));
			ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);
		}
		return true;
	}
	/**/

	/*/
	else if (FStrEq(pcmd, "testhudmsg"))
	{
		if ((arg1 != NULL) && (*arg1 != 0))
		{
			float temp = atoi(arg1);
			///////////////////////////////////////////////
			//
			// example of bot HudMessages by Shrike
			//
			/////////////////////

			// Standard message
			if (temp == 1)
				StdHudMessage(NULL, "Example of MarineBot HudMessages \n by Shrike !!", 1, NULL);

			else if (temp == 2)
				StdHudMessageToAll("Example of MarineBot HudMessages \n by Shrike !!", NULL, 7);

			/////////////////////
			// Custom Message
			//				
			else if (temp > 2)
			{
				Vector red, blue;
				int gfx, time;

				red.x = 250;	//rgb
				red.y = 0;
				red.z = 0;
				blue.x = 0;		//rbg
				blue.y = 0;
				blue.z = 250;
				gfx = 2;		// fade over effect // 0 no effect // 1 flashing
				time = 8;	// hold for 8 seconds 

				if (temp == 3)
					CustHudMessage(pEntity, "Example of MarineBot HudMessages \n by Shrike !!", red, blue, gfx, time);

				else if (temp == 4)
					CustHudMessageToAll("Example of MarineBot HudMessages \n by Shrike !!", red, blue, gfx, time);

				else	// set 1 color and gfx manualy
					CustHudMessageToAll("Example of MarineBot HudMessages \n by Shrike !!", Vector(0, 250, 0), blue, 1, time); // green flashing
			//
			/////////////////////////////////////////////////
			}
		}
		else
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "no arg specified! -> testhudmsg arg, arg is number from 1-5\n");

		return true;
	}
	else if (FStrEq(pcmd, "testcusthudmsg"))
	{
		if ((arg1 != NULL) && (*arg1 != 0) && (arg2 != NULL) && (*arg2 != 0))
		{
			float temp1 = atof(arg1);
			float temp2 = atof(arg2);
			int temp3 = 3;
			char* hudmsg{};

			if ((arg3 != NULL) && (*arg3 != 0))
				temp3 = atoi(arg3);

			if (temp3 == 1)
				hudmsg = "Example of custom Hud Message";
			else if (temp3 == 2)
				hudmsg = "Example of custom Hud Message \n The arguments define the position on the screen";
			else if (temp3 == 3)
				hudmsg = "Example of custom Hud Message \n The arguments define the position on the screen \n Max message length can be 512 characters";
			else
				hudmsg = "Invalid value for 3rd argument (can only be from 1 to 3)";

			sprintf(msg, "The args are: %.2f %.2f\n", temp1, temp2);
			ClientPrint(pEntity, HUD_PRINTCONSOLE, msg);

			CustDisplayMsg(pEntity, hudmsg, temp1, temp2, 5.0f);
		}
		else
			ClientPrint(pEntity, HUD_PRINTNOTIFY, "no args specified! -> takes two float numbers and optional 3rd int value\n");

		return true;
	}
	/**/
	else if (FStrEq(pcmd, "debug_menu"))
	{
		char str[80]{};
		sprintf(str, "menustate=%d menunextstate=%d menuteam=%d\n", g_menu_state, g_menu_next_state, g_menu_team);
		ClientPrint(pEntity, HUD_PRINTCONSOLE, str);

		return true;
	}

	// TEMP: needed some cmd to test mod version
	else if (FStrEq(pcmd, "debug_vers") || FStrEq(pcmd, "testver"))
	{
		sprintf(msg, "ModVersion: %d | IsSteam (bool): %d\n", g_mod_version, is_steam);
		ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

		return true;
	}

	/*/
	else if (FStrEq(pcmd, "generate_names") || FStrEq(pcmd, "generatenames"))
	{
		//const int size = 50;
		char* firstname[] = { "Andreas", "Bernd", "Dieter", "Dietrich", "Eberhard", "Egbert", "Erich", "Ernst", "Erwin", "Fabian", "Felix", "Ferdinand", "Franz", "Fritz",
			"Georg", "Gert", "Gotz", "Gunther", "Hans", "Heinrich", "Helmut", "Hermann", "Horst", "Johann", "Jorg", "Jurgen", "Karl", "Klaus", "Knut", "Konrad", "Kurt",
			"Ludwig", "Luther", "Manfred", "Matthias", "Norbert", "Otto", "Reinhart", "Rolf", "Rudolph", "Sigmund", "Stefan", "Thomas", "Uwe",
			"Verner", "Walter", "Wilbert", "Wilfried", "Wilhelm", "Willi" };
		
		char* surnames[] = { "Abelhard", "Altmann", "Barkhausen", "Baumann", "Bergmann", "Brandt", "Braun", "Buchheim", "Dieffenbach", "Dranckmeister", "von Doberschutz",
			"Falk", "Fuchs", "von Gerstenberg", "Goldstein", "Graf", "Gravenhorst", "Haas", "Hershel", "Hinze", "Hoffmann", "Jung", "Kirchhoff", "Klein", "Kohler", "Kraus",
			"Lehmann", "Lindberg", "Loewe", "Neumann", "Nussbaum", "Meyer", "Muller", "Pfeiffer", "Reichenbach", "Richter",
			"Ritter", "Schmidt", "Schneider", "Schulz", "Schuster", "Sommer", "Stein", "Uhrmann", "Ulrich", "Vogel", "Volker", "Wagner", "Weber", "Zimmermann" };

		const int size = 20;
		char* firstname[] = { "Alden", "Arthur", "Benedict", "Bradley", "Clifford", "Creighton", "Duncan", "Fergus", "Frasier", "Harvey", "Harry", "Holt",
			"Ian", "Lawrence", "Miles", "Nigel", "Roscoe", "Shaw", "Trevor", "Wesley" };
		
		char* surnames[] = { "Agnew", "O'Brien", "Burgess", "Chapman", "Coleman", "Cooke", "Dunn", "Goodwin", "Griffiths", "Harding", "Jenkins", "Kearsley",
			"Kershaw", "Mills", "Moore", "Nicholls", "Pearce", "Reed", "Strefling", "Watkins" };

		char generated_name[BOT_NAME_LEN+1]{};
		
		FILE* f = NULL;
		char filename[256]{};

		util.MarineBotFileName(filename, "brit_names.txt", NULL);
		f = fopen(filename, "a");

		if (f)
		{
			for (int i = 0; i < size; i++)
			{
				sprintf(generated_name, "%s %s\n", firstname[RANDOM_LONG(0, size - 1)], surnames[RANDOM_LONG(0, size - 1)]);
				fprintf(f, "%s", generated_name);
			}

			fclose(f);
		}

		return true;
	}
	/**/

	return false;
}
#endif


// play sound msg confirmation
void PlaySoundConfirmation(edict_t* pEntity, int msg_type)
{
	if (msg_type == SND_DONE)
	{
		EMIT_SOUND_DYN2(pEntity, CHAN_WEAPON, "plats/elevbell1.wav", 1.0, ATTN_NORM, 0, 100);
	}
	else if (msg_type == SND_FAILED)
	{
		EMIT_SOUND_DYN2(pEntity, CHAN_WEAPON, "buttons/button10.wav", 1.0, ATTN_NORM, 0, 100);
	}

	return;
}

void MBMenuSystem(edict_t* pEntity, const char* arg1)
{
	int nearby_wpt_index = wptmanager.FindNearestWaypointToPlayer(pEntity);

	if (g_menu_state == MENU_MAIN)	// in main menu
	{
		if (FStrEq(arg1, "1"))
		{
			g_menu_state = MENU_1;		// switch to bot menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_1);

			return;
		}
		else if (FStrEq(arg1, "2"))
		{
			g_menu_state = MENU_2;		// switch to waypoint menu

			// if no close wpt
			if (nearby_wpt_index == NO_VAL)
				util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2a);
			else
				util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2);

			return;
		}
		else if (FStrEq(arg1, "3"))
		{
			g_menu_state = MENU_3;		// switch to misc menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_3);

			return;
		}
		else if (FStrEq(arg1, "5"))
		{
			g_menu_state = MENU_MAIN;		// stay in this menu
			util.ShowMenu(pEntity, 0x1F, -1, FALSE, show_menu_main);

			return;
		}
	}
	else if (g_menu_state == MENU_1)  // in bot menu
	{
		if (FStrEq(arg1, "1"))
		{
			BotCreate(pEntity, teamONE.GetTeamIdAsString(), NULL, NULL, NULL, NULL);
			botmanager.SetBotCheckTime(gpGlobals->time + 2.5);
		}
		else if (FStrEq(arg1, "2"))
		{
			BotCreate(pEntity, teamTWO.GetTeamIdAsString(), NULL, NULL, NULL, NULL);
			botmanager.SetBotCheckTime(gpGlobals->time + 2.5);
		}
		else if (FStrEq(arg1, "3"))
		{
			botmanager.SetListeServerFilling(true);
			botmanager.SetBotCheckTime(gpGlobals->time + 0.5);
		}
		else if (FStrEq(arg1, "4"))
		{
			g_menu_state = MENU_99;			// switch to team select
			g_menu_next_state = 24;			// to know thats kicking

			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_99a);

			return;
		}
		else if (FStrEq(arg1, "5"))
		{
			g_menu_state = MENU_99;			// switch to team select
			g_menu_next_state = 25;			// to know thats killing

			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_99a);

			return;
		}
		else if (FStrEq(arg1, "6"))
		{
			botmanager.SetTeamsBalanceValue(util.TeamsBalanceCheck());
		}
		else if ((FStrEq(arg1, "6")) || (FStrEq(arg1, "7")) || (FStrEq(arg1, "8")))
		{
			g_menu_state = MENU_1;		// stay in this menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_1);

			return;
		}
		else if (FStrEq(arg1, "9"))
		{
			g_menu_state = MENU_1_9;		// switch to settings menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_1_9);

			return;
		}
	}
	else if (g_menu_state == MENU_1_9)  // in settings menu
	{
		if (FStrEq(arg1, "1"))
		{
			g_menu_state = MENU_1_9_1;	// switch to skill levels menu
			g_menu_next_state = 91;		// to know thats default botskill

			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_1_9_1);

			return;
		}
		else if (FStrEq(arg1, "2"))
		{
			g_menu_state = MENU_1_9_1;	// switch to skill levels menu
			g_menu_next_state = 92;		// to know thats botskill (in game)

			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_1_9_1);

			return;
		}
		else if (FStrEq(arg1, "3"))
			util.ChangeBotSkillLevel(true, 1);
		else if (FStrEq(arg1, "4"))
			util.ChangeBotSkillLevel(true, -1);
		else if (FStrEq(arg1, "5"))
		{
			g_menu_state = MENU_1_9_1;	// switch to skill levels menu
			g_menu_next_state = 95;		// to know thats aimskill

			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_1_9_1);

			return;
		}
		else if (FStrEq(arg1, "6"))
		{
			g_menu_state = MENU_1_9_6;	// switch to reactions menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_1_9_6);

			return;
		}
		else if (FStrEq(arg1, "7"))
		{
			if (externals.GetRandomSkill())
				externals.SetRandomSkill(FALSE);
			else
				externals.SetRandomSkill(TRUE);
		}
		else if (FStrEq(arg1, "8") || FStrEq(arg1, "9"))
		{
			g_menu_state = MENU_1;		// switch back to bot menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_1);

			return;
		}
	}
	else if (g_menu_state == MENU_1_9_1)	// in skill menu
	{
		int skill_level = -1;

		if (FStrEq(arg1, "1"))
		{
			// is next step default botskill
			if (g_menu_next_state == 91)
				externals.SetSpawnSkill(1);
			// is next step botskill or aimskill
			else if ((g_menu_next_state == 92) || (g_menu_next_state == 95))
				skill_level = 1;
		}
		else if (FStrEq(arg1, "2"))
		{
			if (g_menu_next_state == 91)
				externals.SetSpawnSkill(2);
			else if ((g_menu_next_state == 92) || (g_menu_next_state == 95))
				skill_level = 2;
		}
		else if (FStrEq(arg1, "3"))
		{
			if (g_menu_next_state == 91)
				externals.SetSpawnSkill(3);
			else if ((g_menu_next_state == 92) || (g_menu_next_state == 95))
				skill_level = 3;
		}
		else if (FStrEq(arg1, "4"))
		{
			if (g_menu_next_state == 91)
				externals.SetSpawnSkill(4);
			else if ((g_menu_next_state == 92) || (g_menu_next_state == 95))
				skill_level = 4;
		}
		else if (FStrEq(arg1, "5"))
		{
			if (g_menu_next_state == 91)
				externals.SetSpawnSkill(5);
			else if ((g_menu_next_state == 92) || (g_menu_next_state == 95))
				skill_level = 5;
		}
		else if ((FStrEq(arg1, "6")) || (FStrEq(arg1, "7")) || (FStrEq(arg1, "8")))
		{
			g_menu_state = MENU_1_9_1;		// stay in this menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_1_9_1);

			return;
		}
		else if (FStrEq(arg1, "9"))
		{
			g_menu_state = MENU_1_9;		// switch back to settings menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_1_9);

			return;
		}

		// set the bot skill
		if ((skill_level != -1) && (g_menu_next_state == 92))
			util.ChangeBotSkillLevel(false, skill_level);
		// set the aim skill
		else if ((skill_level != -1) && (g_menu_next_state == 95))
			util.ChangeAimSkillLevel(skill_level);
	}
	else if (g_menu_state == MENU_1_9_6)	// in reactions menu
	{
		if (FStrEq(arg1, "1"))
			externals.SetReactionTime(0.0f);
		else if (FStrEq(arg1, "2"))
			externals.SetReactionTime(0.1f);
		else if (FStrEq(arg1, "3"))
			externals.SetReactionTime(0.2f);
		else if (FStrEq(arg1, "4"))
			externals.SetReactionTime(0.5f);
		else if (FStrEq(arg1, "5"))
			externals.SetReactionTime(1.0f);
		else if (FStrEq(arg1, "6"))
			externals.SetReactionTime(1.5f);
		else if (FStrEq(arg1, "7"))
			externals.SetReactionTime(2.5f);
		else if (FStrEq(arg1, "8"))
			externals.SetReactionTime(5.0f);
		else if (FStrEq(arg1, "9"))
		{
			g_menu_state = MENU_1_9;		// switch back to settings
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_1_9);

			return;
		}
	}
	else if (g_menu_state == MENU_2)  // in waypoint menu
	{
		if (FStrEq(arg1, "1"))
		{
			g_menu_state = MENU_2_1AND2_P1;		// switch to wpt list menu
			g_menu_next_state = 128;		// to know that's adding

			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_1);

			return;
		}
		// only if there is some wpt near player
		else if ((FStrEq(arg1, "2")) && (nearby_wpt_index != NO_VAL))
		{
			g_menu_state = MENU_2_1AND2_P1;		// switch to wpt list menu
			g_menu_next_state = 129;		// to know that's changing

			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_2);

			return;
		}
		else if ((FStrEq(arg1, "3")) && (nearby_wpt_index != NO_VAL))
		{
			g_menu_state = MENU_99;		// switch to team select
			g_menu_next_state = MENU_2_3;	// then to priority menu

			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_99);

			return;
		}
		else if ((FStrEq(arg1, "4")) && (nearby_wpt_index != NO_VAL))
		{
			g_menu_state = MENU_99;		// switch to team select
			g_menu_next_state = MENU_2_4;	// then to time menu

			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_99);

			return;
		}
		else if ((FStrEq(arg1, "5")) && (nearby_wpt_index != NO_VAL))
		{
			g_menu_state = MENU_2_5;		// switch to range menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_5);

			return;
		}
		else if (FStrEq(arg1, "6"))
		{
			g_menu_state = MENU_2_6;		// switch to autowpt menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_6);

			return;
		}
		else if (FStrEq(arg1, "7"))
		{
			g_menu_state = MENU_2_7;		// switch to path menu

			if (nearby_wpt_index != NO_VAL)
				util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_7);
			else if (internals.IsPathToContinue())
				util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_7a);
			else
				util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_7b);

			return;
		}
		else if (FStrEq(arg1, "8"))
		{
			g_menu_state = MENU_2;		// stay in this menu

			// if no close wpt
			if (nearby_wpt_index == NO_VAL)
				util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2a);
			else
				util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2);

			return;
		}
		else if (FStrEq(arg1, "9"))
		{
			g_menu_state = MENU_2_9;		// switch to service menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_9);

			return;
		}
	}
	else if (g_menu_state == MENU_2_1AND2_P1)	// in wpt list menu - 1st part
	{
		if (FStrEq(arg1, "1"))
		{
			// is it adding cmd
			if (g_menu_next_state == 128)
			{
				wpteditor.Add(pEntity, "normal");
			}
			// is it changing cmd
			if (g_menu_next_state == 129)
			{
				wpteditor.ChangeType(pEntity, "normal", nearby_wpt_index);
			}
		}
		else if (FStrEq(arg1, "2"))
		{
			// is it standard menu "placing" action OR a menu shortcut to waypoint tags page 1 for a nearby waypoint
			if (((g_menu_next_state == 128) && (g_menu_team == 0)) || ((g_menu_team == -1) && (g_menu_next_state == 129)))
			{
				wpteditor.Delete(pEntity);
			}
			// is it standard menu "changing" action OR a menu shortcut to waypoint tags page 1 when there isn't any nearby waypoint
			else if (((g_menu_next_state == 129) && (g_menu_team == 0)) || ((g_menu_team == -1) && (g_menu_next_state == 128)))
			{
				g_menu_state = MENU_2_1AND2_P1;		// stay in this menu
				util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_2);

				return;
			}
		}
		else if (FStrEq(arg1, "3"))
		{
			if (g_menu_next_state == 128)
			{
				wpteditor.Add(pEntity, "crouch");
			}
			if (g_menu_next_state == 129)
			{
				wpteditor.ChangeType(pEntity, "crouch", nearby_wpt_index);
			}
		}
		else if (FStrEq(arg1, "4"))
		{
			if (g_menu_next_state == 128)
			{
				wpteditor.Add(pEntity, "prone");
			}
			if (g_menu_next_state == 129)
			{
				wpteditor.ChangeType(pEntity, "prone", nearby_wpt_index);
			}
		}
		else if (FStrEq(arg1, "5"))
		{
			if (g_menu_next_state == 128)
			{
				wpteditor.Add(pEntity, "cross");
			}
			if (g_menu_next_state == 129)
			{
				wpteditor.ChangeType(pEntity, "cross", nearby_wpt_index);
			}
		}
		else if (FStrEq(arg1, "6"))
		{
			if (g_menu_next_state == 128)
			{
				wpteditor.Add(pEntity, "goback");
			}
			if (g_menu_next_state == 129)
			{
				wpteditor.ChangeType(pEntity, "goback", nearby_wpt_index);
			}
		}
		else if (FStrEq(arg1, "7"))
		{
			if (g_menu_next_state == 128)
			{
				wpteditor.Add(pEntity, "sniper");
			}
			if (g_menu_next_state == 129)
			{
				wpteditor.ChangeType(pEntity, "sniper", nearby_wpt_index);
			}
		}
		else if (FStrEq(arg1, "8"))
		{
			if (g_menu_next_state == 128)
			{
				wpteditor.Add(pEntity, "sprint");
			}
			if (g_menu_next_state == 129)
			{
				wpteditor.ChangeType(pEntity, "sprint", nearby_wpt_index);
			}
		}
		else if (FStrEq(arg1, "9"))
		{
			g_menu_state = MENU_2_1AND2_P2;		// switch to 2nd part
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_1and2_p2);

			return;
		}
	}
	else if (g_menu_state == MENU_2_1AND2_P2)	// in wpt list menu - 2nd part
	{
		if (FStrEq(arg1, "1"))
		{
			if (g_menu_next_state == 128)
			{
				wpteditor.Add(pEntity, "aim");
			}
			if (g_menu_next_state == 129)
			{
				wpteditor.ChangeType(pEntity, "aim", nearby_wpt_index);
			}
		}
		else if (FStrEq(arg1, "2"))
		{
			if (g_menu_next_state == 128)
			{
				wpteditor.Add(pEntity, "jump");
			}
			if (g_menu_next_state == 129)
			{
				wpteditor.ChangeType(pEntity, "jump", nearby_wpt_index);
			}
		}
		else if (FStrEq(arg1, "3"))
		{
			if (g_menu_next_state == 128)
			{
				wpteditor.Add(pEntity, "duckjump");
			}
			if (g_menu_next_state == 129)
			{
				wpteditor.ChangeType(pEntity, "duckjump", nearby_wpt_index);
			}
		}
		else if (FStrEq(arg1, "4"))
		{
			if (g_menu_next_state == 128)
			{
				wpteditor.Add(pEntity, "claymore");
			}
			if (g_menu_next_state == 129)
			{
				wpteditor.ChangeType(pEntity, "claymore", nearby_wpt_index);
			}
		}
		else if (FStrEq(arg1, "5"))
		{
			if (g_menu_next_state == 128)
			{
				wpteditor.Add(pEntity, "shoot");
			}
			if (g_menu_next_state == 129)
			{
				wpteditor.ChangeType(pEntity, "shoot", nearby_wpt_index);
			}
		}
		else if (FStrEq(arg1, "6"))
		{
			if (g_menu_next_state == 128)
			{
				wpteditor.Add(pEntity, "flag");
			}
			if (g_menu_next_state == 129)
			{
				wpteditor.ChangeType(pEntity, "flag", nearby_wpt_index);
			}
		}
		else if (FStrEq(arg1, "7"))
		{
			if (g_menu_next_state == 128)
			{
				wpteditor.Add(pEntity, "ammobox");
			}
			if (g_menu_next_state == 129)
			{
				wpteditor.ChangeType(pEntity, "ammobox", nearby_wpt_index);
			}
		}
		else if (FStrEq(arg1, "8"))
		{
			if (g_menu_next_state == 128)
			{
				wpteditor.Add(pEntity, "use");
			}
			if (g_menu_next_state == 129)
			{
				wpteditor.ChangeType(pEntity, "use", nearby_wpt_index);
			}
		}
		else if (FStrEq(arg1, "9"))
		{
			g_menu_state = MENU_2_1AND2_P3;		// switch to 3rd part
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_1and2_p3);

			return;
		}
	}
	else if (g_menu_state == MENU_2_1AND2_P3)
	{
		if (FStrEq(arg1, "1"))
		{
			if (g_menu_next_state == 128)
			{
				wpteditor.Add(pEntity, "roadblock");
			}
			if (g_menu_next_state == 129)
			{
				wpteditor.ChangeType(pEntity, "roadblock", nearby_wpt_index);
			}
		}
		else if (FStrEq(arg1, "2"))
		{
			if (g_menu_next_state == 128)
			{
				wpteditor.Add(pEntity, "trigger");
			}
			if (g_menu_next_state == 129)
			{
				wpteditor.ChangeType(pEntity, "trigger", nearby_wpt_index);
			}
		}
		else if (FStrEq(arg1, "3"))
		{
			if (g_menu_next_state == 128)
			{
				wpteditor.Add(pEntity, "ladder");
			}
			if (g_menu_next_state == 129)
			{
				wpteditor.ChangeType(pEntity, "ladder", nearby_wpt_index);
			}
		}
		else if (FStrEq(arg1, "4"))
		{
			if (g_menu_next_state == 128)
			{
				wpteditor.Add(pEntity, "parachute");
			}
			if (g_menu_next_state == 129)
			{
				wpteditor.ChangeType(pEntity, "parachute", nearby_wpt_index);
			}
		}
		else if (FStrEq(arg1, "5"))
		{
			if (g_menu_next_state == 128)
			{
				wpteditor.Add(pEntity, "door");
			}
			if (g_menu_next_state == 129)
			{
				wpteditor.ChangeType(pEntity, "door", nearby_wpt_index);
			}
		}
		else if (FStrEq(arg1, "6"))
		{
			if (g_menu_next_state == 128)
			{
				wpteditor.Add(pEntity, "usedoor");
			}
			if (g_menu_next_state == 129)
			{
				wpteditor.ChangeType(pEntity, "usedoor", nearby_wpt_index);
			}
		}
		else if ((FStrEq(arg1, "7")) || (FStrEq(arg1, "8")))
		{
			g_menu_state = MENU_2_1AND2_P3;		// stay in this menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_1and2_p3);

			return;
		}
		else if (FStrEq(arg1, "9"))
		{
			g_menu_state = MENU_2_1AND2_P1;		// switch to 1st part

			if (g_menu_next_state == 128)
				util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_1);
			else if (g_menu_next_state == 129)
				util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_2);

			return;
		}
	}
	else if (g_menu_state == MENU_2_3)	// in priority menu
	{
		char* the_team;

		if (g_menu_team == teamONE.GetTeamId())
			the_team = teamONE.GetTeamName();
		if (g_menu_team == teamTWO.GetTeamId())
			the_team = teamTWO.GetTeamName();

		if (FStrEq(arg1, "1"))
			wpteditor.ChangePriority(pEntity, "1", the_team);
		else if (FStrEq(arg1, "2"))
			wpteditor.ChangePriority(pEntity, "2", the_team);
		else if (FStrEq(arg1, "3"))
			wpteditor.ChangePriority(pEntity, "3", the_team);
		else if (FStrEq(arg1, "4"))
			wpteditor.ChangePriority(pEntity, "4", the_team);
		else if (FStrEq(arg1, "5"))
			wpteditor.ChangePriority(pEntity, "5", the_team);
		else if (FStrEq(arg1, "6"))
			wpteditor.ChangePriority(pEntity, "0", the_team);
		else if ((FStrEq(arg1, "7")) || (FStrEq(arg1, "8")))
		{
			g_menu_state = MENU_2_3;		// stay in this menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_3);

			return;
		}
		else if (FStrEq(arg1, "9"))
		{
			g_menu_state = MENU_99;		// switch back to team menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_99);

			return;
		}
	}
	else if (g_menu_state == MENU_2_4)	// in time menu
	{
		char* the_team;

		if (g_menu_team == teamONE.GetTeamId())
			the_team = teamONE.GetTeamName();
		if (g_menu_team == teamTWO.GetTeamId())
			the_team = teamTWO.GetTeamName();

		if (FStrEq(arg1, "1"))
			wpteditor.ChangeTime(pEntity, "1", the_team);
		else if (FStrEq(arg1, "2"))
			wpteditor.ChangeTime(pEntity, "2", the_team);
		else if (FStrEq(arg1, "3"))
			wpteditor.ChangeTime(pEntity, "3", the_team);
		else if (FStrEq(arg1, "4"))
			wpteditor.ChangeTime(pEntity, "4", the_team);
		else if (FStrEq(arg1, "5"))
			wpteditor.ChangeTime(pEntity, "5", the_team);
		else if (FStrEq(arg1, "6"))
			wpteditor.ChangeTime(pEntity, "10", the_team);
		else if (FStrEq(arg1, "7"))
			wpteditor.ChangeTime(pEntity, "20", the_team);
		else if (FStrEq(arg1, "8"))
			wpteditor.ChangeTime(pEntity, "30", the_team);
		else if (FStrEq(arg1, "9"))
		{
			g_menu_state = MENU_99;		// switch back to team menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_99);

			return;
		}
	}
	else if (g_menu_state == MENU_2_5)	// in range menu
	{
		if (FStrEq(arg1, "1"))
			wpteditor.ChangeRange(pEntity, "5");
		else if (FStrEq(arg1, "2"))
			wpteditor.ChangeRange(pEntity, "20");
		else if (FStrEq(arg1, "3"))
			wpteditor.ChangeRange(pEntity, "50");
		else if (FStrEq(arg1, "4"))
			wpteditor.ChangeRange(pEntity, "75");
		else if (FStrEq(arg1, "5"))
			wpteditor.ChangeRange(pEntity, "100");
		else if (FStrEq(arg1, "6"))
			wpteditor.ChangeRange(pEntity, "150");
		else if (FStrEq(arg1, "7"))
			wpteditor.ChangeRange(pEntity, "250");
		else if (FStrEq(arg1, "8"))
			wpteditor.ChangeRangeByConstantValue(pEntity, "0");
		else if (FStrEq(arg1, "9"))
			wpteditor.ChangeRangeByConstantValue(pEntity, "0", true);
	}
	else if (g_menu_state == MENU_2_6)	// in autowpt menu
	{
		if (FStrEq(arg1, "1"))
		{
			wpteditor.StartAutoWaypointg(true);
		}
		else if (FStrEq(arg1, "2"))
		{
			wpteditor.StartAutoWaypointg(false);
		}
		else if (FStrEq(arg1, "3"))
		{
			g_menu_state = MENU_2_6_3;		// switch to distance menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_6_3);

			return;
		}
		else if ((FStrEq(arg1, "4")) || (FStrEq(arg1, "5")) || (FStrEq(arg1, "6")) || (FStrEq(arg1, "7")) || (FStrEq(arg1, "8")) || (FStrEq(arg1, "9")))
		{
			g_menu_state = MENU_2_6;		// stay in this menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_6);

			return;
		}
	}
	else if (g_menu_state == MENU_2_6_3)	// in autowpt distance menu
	{
		if (FStrEq(arg1, "1"))
			wptser.SetAutoWaypointingDistance(80.0f);
		else if (FStrEq(arg1, "2"))
			wptser.SetAutoWaypointingDistance(100.0f);
		else if (FStrEq(arg1, "3"))
			wptser.SetAutoWaypointingDistance(120.0f);
		else if (FStrEq(arg1, "4"))
			wptser.SetAutoWaypointingDistance(160.0f);
		else if (FStrEq(arg1, "5"))
			wptser.SetAutoWaypointingDistance(200.0f);
		else if (FStrEq(arg1, "6"))
			wptser.SetAutoWaypointingDistance(280.0f);
		else if (FStrEq(arg1, "7"))
			wptser.SetAutoWaypointingDistance(340.0f);
		else if (FStrEq(arg1, "8"))
			wptser.SetAutoWaypointingDistance(400.0f);
		else if (FStrEq(arg1, "9"))
		{
			g_menu_state = MENU_2_6;		// switch back to autowpt menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_6);

			return;
		}
	}
	else if (g_menu_state == MENU_2_7)	// in path menu
	{
		if (FStrEq(arg1, "1"))
		{
			if (wptser.IsShowPaths())
				wptser.ResetShowPaths();
			else
			{
				wptser.SetShowWaypoints(true);		// turn waypoints on
				wptser.SetShowPaths(true);			// turn paths on
			}
		}
		else if ((FStrEq(arg1, "2")) && (nearby_wpt_index != NO_VAL))	// only if there is any wpt
		{
			wptser.SetShowPaths(true);
			patheditor.Create(pEntity, nearby_wpt_index);
		}
		else if (FStrEq(arg1, "3") && internals.IsPathToContinue())
			patheditor.Finish(pEntity);
		else if ((FStrEq(arg1, "4")) && (nearby_wpt_index != NO_VAL))
			patheditor.Continue(pEntity, NO_VAL);
		else if ((FStrEq(arg1, "5")) && (nearby_wpt_index != NO_VAL))
			patheditor.AddWaypoint(pEntity, nearby_wpt_index);
		else if ((FStrEq(arg1, "6")) && (nearby_wpt_index != NO_VAL))
			patheditor.RemoveWaypoint(pEntity, nearby_wpt_index, NO_VAL);
		else if ((FStrEq(arg1, "7")) && (nearby_wpt_index != NO_VAL))
			patheditor.Delete(pEntity, NO_VAL);
		else if (FStrEq(arg1, "8"))
		{
			if (wptser.IsAutoAddToPath())
				wptser.ResetAutoAddToPath();
			else
				wptser.SetAutoAddToPath(true);
		}
		else if ((FStrEq(arg1, "9")) && ((nearby_wpt_index != NO_VAL) || internals.IsPathToContinue()))
		{
			g_menu_state = MENU_2_7_9_P1;	// switch to path flags menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_7_9_p1);

			return;
		}
	}
	else if (g_menu_state == MENU_2_7_9_P1)	// in path flags menu - 1st part
	{
		if (FStrEq(arg1, "1"))
			patheditor.ChangeDirection(pEntity, "one", NO_VAL);
		else if (FStrEq(arg1, "2"))
			patheditor.ChangeDirection(pEntity, "two", NO_VAL);
		else if (FStrEq(arg1, "3"))
			patheditor.ChangeDirection(pEntity, "patrol", NO_VAL);
		else if (FStrEq(arg1, "4"))
			patheditor.ChangeTeam(pEntity, teamONE.GetTeamName(), NO_VAL);
		else if (FStrEq(arg1, "5"))
			patheditor.ChangeTeam(pEntity, teamTWO.GetTeamName(), NO_VAL);
		else if (FStrEq(arg1, "6"))
			patheditor.ChangeTeam(pEntity, "both", NO_VAL);
		else if (FStrEq(arg1, "7"))
			patheditor.ChangeClass(pEntity, "sniper", NO_VAL);
		else if (FStrEq(arg1, "8"))
			patheditor.ChangeClass(pEntity, "mgunner", NO_VAL);
		else if (FStrEq(arg1, "9"))
		{
			g_menu_state = MENU_2_7_9_P2;		// switch 2nd part
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_7_9_p2);

			return;
		}
	}
	else if (g_menu_state == MENU_2_7_9_P2)	// in path flags menu - 2nd part
	{
		if (FStrEq(arg1, "1"))
			patheditor.ChangeClass(pEntity, "antiarmor", NO_VAL);
		else if (FStrEq(arg1, "2"))
			patheditor.ChangeClass(pEntity, "all", NO_VAL);
		else if (FStrEq(arg1, "3"))
			patheditor.ChangeMisc(pEntity, "avoidenemy", NO_VAL);
		else if (FStrEq(arg1, "4"))
			patheditor.ChangeMisc(pEntity, "ignoreenemy", NO_VAL);
		else if (FStrEq(arg1, "5"))
			patheditor.ChangeMisc(pEntity, "carryitem", NO_VAL);
		else if ((FStrEq(arg1, "6")) || (FStrEq(arg1, "7")) || (FStrEq(arg1, "8")))
		{
			g_menu_state = MENU_2_7_9_P2;		// stay in this menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_7_9_p2);

			return;
		}
		else if (FStrEq(arg1, "9"))
		{
			g_menu_state = MENU_2_7_9_P1;		// switch 1st part
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_7_9_p1);

			return;
		}
	}
	else if (g_menu_state == MENU_2_9)	// in service menu
	{
		if (FStrEq(arg1, "1"))
		{
			if (wptser.IsShowWaypoints())
				wptser.ResetShowWaypoints();
			else
				wptser.SetShowWaypoints(true);
		}
		else if (FStrEq(arg1, "2"))
		{
			g_menu_state = MENU_2_9_2;	// switch to adv. debugging menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_9_2);

			return;
		}
		else if (FStrEq(arg1, "3"))
		{
			wpteditor.LoadWaypoints(pEntity, NULL);
			patheditor.LoadPaths(pEntity, NULL);
		}
		else if (FStrEq(arg1, "4"))
		{
			wpteditor.SaveWaypoints(NULL);

			if (num_w_paths > 0)
				patheditor.SavePaths(NULL);
		}
		else if (FStrEq(arg1, "5"))
		{
			if (wpteditor.LoadUnsupportedWaypoints(pEntity))
			{
				if (patheditor.LoadUnsupportedPaths(pEntity))
				{
					wpteditor.SaveWaypoints(NULL);
					patheditor.SavePaths(NULL);
				}
			}
		}
		else if (FStrEq(arg1, "6"))
			wpteditor.InitAll();
		else if (FStrEq(arg1, "7"))
			wpteditor.WipeAll();
		else if (FStrEq(arg1, "8"))
		{
			g_menu_state = MENU_2_9_8;		// switch to folder select menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_9_8);

			return;
		}
		else if (FStrEq(arg1, "9"))
		{
			g_menu_state = MENU_2_9;		// stay in this menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_9);

			return;
		}
	}
	else if (g_menu_state == MENU_2_9_2)	// in adv. debugging menu
	{
		if (FStrEq(arg1, "1"))
		{
			if (wptser.IsCheckCross())
				wptser.ResetCheckCross();
			else
			{
				wptser.SetCheckCross(true);
				wptser.ResetCheckRanges();
				wptser.ResetCheckShoot();
			}
		}
		else if (FStrEq(arg1, "2"))
		{
			if (wptser.IsCheckAims())
				wptser.ResetCheckAims();
			else
			{
				wptser.SetCheckAims(true);
				wptser.ResetCheckRanges();
				wptser.ResetCheckShoot();
			}
		}
		else if (FStrEq(arg1, "3"))
		{
			if (wptser.IsCheckRanges())
				wptser.ResetCheckRanges();
			else
			{
				wptser.SetCheckRanges(true);
				wptser.ResetCheckAims();
				wptser.ResetCheckCross();
				wptser.ResetCheckShoot();
			}
		}
		else if (FStrEq(arg1, "4"))
		{
			if (wptser.IsCheckShoot())
				wptser.ResetCheckShoot();
			else
			{
				wptser.SetCheckShoot(true);
				wptser.ResetCheckAims();
				wptser.ResetCheckCross();
				wptser.ResetCheckRanges();
			}
		}
		else if (FStrEq(arg1, "5"))
		{
			g_menu_state = MENU_2_9_2_5;	// switch to path highlight menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_9_2_5);

			return;
		}
		else if ((FStrEq(arg1, "6")) || (FStrEq(arg1, "7")) || (FStrEq(arg1, "8")))
		{
			g_menu_state = MENU_2_9_2;		// stay in this menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_9_2);

			return;
		}
		else if (FStrEq(arg1, "9"))
		{
			g_menu_state = MENU_2_9_2_9;		// switch to draw distance menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_9_2_9);

			return;
		}
	}
	else if (g_menu_state == MENU_2_9_2_5)	// in path highlighting menu
	{
		if (FStrEq(arg1, "1"))
		{
			wptser.SetPathToHighlight(HIGHLIGHT_TEAMONE);		// team one only paths

			wptser.SetShowWaypoints(true);		// turn waypoints on in case they aren't shown yet
			wptser.SetShowPaths(true);			// and then show paths too
		}
		else if (FStrEq(arg1, "2"))
		{
			wptser.SetPathToHighlight(HIGHLIGHT_TEAMTWO);		// team two only paths
			wptser.SetShowWaypoints(true);
			wptser.SetShowPaths(true);
		}
		else if (FStrEq(arg1, "3"))
		{
			wptser.SetPathToHighlight(HIGHLIGHT_ONEWAY);	// one-way paths only
			wptser.SetShowWaypoints(true);
			wptser.SetShowPaths(true);
		}
		else if (FStrEq(arg1, "4"))
		{
			wptser.SetPathToHighlight(HIGHLIGHT_SNIPER);	// sniper paths only
			wptser.SetShowWaypoints(true);
			wptser.SetShowPaths(true);
		}
		else if (FStrEq(arg1, "5"))
		{
			wptser.SetPathToHighlight(HIGHLIGHT_MGUNNER);	// machine gunner paths only
			wptser.SetShowWaypoints(true);
			wptser.SetShowPaths(true);
		}
		else if (FStrEq(arg1, "6"))
		{
			wptser.SetPathToHighlight(HIGHLIGHT_ANTIARMOR);	// anti-armor specialist paths only
			wptser.SetShowWaypoints(true);
			wptser.SetShowPaths(true);
		}
		else if ((FStrEq(arg1, "7")) || (FStrEq(arg1, "8")))
		{
			g_menu_state = MENU_2_9_2_5;		// stay in this menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_9_2_5);

			return;
		}
		else if (FStrEq(arg1, "9"))
			wptser.ResetPathToHighlight();		// turn it off i.e. all paths show again
	}
	else if (g_menu_state == MENU_2_9_2_9)	// in waypoints draw distance menu
	{
		if (FStrEq(arg1, "1"))
			wptser.SetWaypointsDrawDistance(400.0f);
		else if (FStrEq(arg1, "2"))
			wptser.SetWaypointsDrawDistance(500.0f);
		else if (FStrEq(arg1, "3"))
			wptser.SetWaypointsDrawDistance(600.0f);
		else if (FStrEq(arg1, "4"))
			wptser.SetWaypointsDrawDistance(700.0f);
		else if (FStrEq(arg1, "5"))
			wptser.SetWaypointsDrawDistance(800.0f);
		else if (FStrEq(arg1, "6"))
			wptser.SetWaypointsDrawDistance(900.0f);
		else if (FStrEq(arg1, "7"))
			wptser.SetWaypointsDrawDistance(1000.0f);
		else if (FStrEq(arg1, "8"))
			wptser.SetWaypointsDrawDistance(1200.0f);
		else if (FStrEq(arg1, "9"))
			wptser.SetWaypointsDrawDistance(1400.0f);
	}
	else if (g_menu_state == MENU_2_9_8)	// in folder select menu
	{
		if (FStrEq(arg1, "1"))
			internals.ResetIsCustomWaypoints();
		else if (FStrEq(arg1, "2"))
			internals.SetIsCustomWaypoints(true);
		else if ((FStrEq(arg1, "3")) || (FStrEq(arg1, "4")) || (FStrEq(arg1, "5")) || (FStrEq(arg1, "6")) || (FStrEq(arg1, "7")) || (FStrEq(arg1, "8")))
		{
			g_menu_state = MENU_2_9_8;		// stay in this menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_9_8);

			return;
		}
		else if (FStrEq(arg1, "9"))
		{
			g_menu_state = MENU_2_9;		// switch back to service menu
			util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_9);

			return;
		}
	}
	else if (g_menu_state == MENU_3)	// in misc menu
	{
		if (FStrEq(arg1, "1"))
		{
			if (botdebugger.IsObserverMode())
				botdebugger.ResetObserverMode();
			else
				botdebugger.SetObserverMode(true);
		}
		else if (FStrEq(arg1, "2"))
		{
			if (botdebugger.IsFreezeMode())
				botdebugger.ResetFreezeMode();
			else
				botdebugger.SetFreezeMode(true);
		}
		else if (FStrEq(arg1, "3"))
		{
			if (botdebugger.IsDontShoot())
				botdebugger.ResetDontShoot();
			else
				botdebugger.SetDontShoot(true);
		}
		else if (FStrEq(arg1, "4"))
		{
			if (botdebugger.IsDontShootFirearm())
				botdebugger.ResetDontShootFirearm();
			else
				botdebugger.SetDontShootFirearm(true);
		}
		else if (FStrEq(arg1, "5"))
		{
			if (botdebugger.IsIgnoreAll())
				botdebugger.ResetIgnoreAll();
			else
				botdebugger.SetIgnoreAll(true);
		}
		else if (FStrEq(arg1, "6"))
		{
			if (pEntity->v.movetype == MOVETYPE_NOCLIP)
				pEntity->v.movetype = MOVETYPE_WALK;
			else
				pEntity->v.movetype = MOVETYPE_NOCLIP;
		}
		else if (FStrEq(arg1, "7"))
		{
			if (externals.GetDontSpeak())
				externals.ResetDontSpeak();
			else
				externals.SetDontSpeak(true);
		}
		else if (FStrEq(arg1, "8"))
		{
			if (externals.GetDontChat())
				externals.ResetDontChat();
			else
				externals.SetDontChat(true);
		}
		else if (FStrEq(arg1, "9"))
		{
			if (externals.GetDontChatToBots())
				externals.ResetDontChatToBots();
			else
				externals.SetDontChatToBots(true);
		}
	}
	else if (g_menu_state == MENU_99)		// in team select menu
	{
		if (FStrEq(arg1, "1"))
		{
			g_menu_team = teamONE.GetTeamId();

			// is next step kick cmd
			if (g_menu_next_state == 24)
				util.KickBot(100 + g_menu_team);
			// is next step kill cmd
			else if (g_menu_next_state == 25)
				util.KillBot(100 + g_menu_team);
			// is next step priority menu
			else if (g_menu_next_state == MENU_2_3)
			{
				g_menu_state = MENU_2_3;
				util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_3);

				return;
			}
			// is next step time menu
			else if (g_menu_next_state == MENU_2_4)
			{
				g_menu_state = MENU_2_4;
				util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_4);

				return;
			}
		}
		else if (FStrEq(arg1, "2"))
		{
			g_menu_team = teamTWO.GetTeamId();

			// is next step kick cmd
			if (g_menu_next_state == 24)
				util.KickBot(100 + g_menu_team);
			// is next step kill cmd
			else if (g_menu_next_state == 25)
				util.KillBot(100 + g_menu_team);
			// is next step priority menu
			else if (g_menu_next_state == MENU_2_3)
			{
				g_menu_state = MENU_2_3;
				util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_3);

				return;
			}
			// is next step time menu
			else if (g_menu_next_state == MENU_2_4)
			{
				g_menu_state = MENU_2_4;
				util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_2_4);

				return;
			}
		}
		else if (FStrEq(arg1, "3"))
		{
			// is next step kick cmd
			if (g_menu_next_state == 24)
				util.KickBot(100);
			// is next step kill cmd
			else if (g_menu_next_state == 25)
				util.KillBot(100);
			// or is "3"-both teams not allowed
			else
			{
				g_menu_state = MENU_99;		// stay in this menu
				util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_99);

				return;
			}
		}
		else if ((FStrEq(arg1, "4")) || (FStrEq(arg1, "5")) || (FStrEq(arg1, "6")) || (FStrEq(arg1, "7")) || (FStrEq(arg1, "8")) || (FStrEq(arg1, "9")))
		{
			g_menu_state = MENU_99;		// stay in this menu

			if ((g_menu_next_state == 24) || (g_menu_next_state == 25))
				util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_99a);
			else
				util.ShowMenu(pEntity, 0x3FF, -1, FALSE, show_menu_99);

			return;
		}
	}

	// clear all menu values
	g_menu_state = MENU_NONE;
	g_menu_next_state = 0;
	g_menu_team = 0;

	return;
}


/*
* the following section is used for commands that are same for both dedicated server as well as listenserver (ie. LAN game)
*/

void KickBotCommand(edict_t* pEntity, const char* arg1)
{
	// no arg then kick random bot
	// we have to check the length, because checking for nullity doesn't seems to work
	if (strlen(arg1) < 1)
	{
		if (util.KickBot(-100))
		{
			conOutput.Print(pEntity, "random bot was kicked\n", MType::msg_default);

			// not a dedicated server then play also sound confirmation
			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else
		{
			conOutput.Print(pEntity, "there are no bots\n", MType::msg_error);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_FAILED);
		}
	}
	else
	{
		if (FStrEq(arg1, "all"))		// kick all bots
		{
			if (util.KickBot(100))
			{
				conOutput.Print(pEntity, "all bots were kicked\n", MType::msg_default);

				if (!is_dedicated_server)
					PlaySoundConfirmation(pEntity, SND_DONE);
			}
			else
			{
				conOutput.Print(pEntity, "there are no bots\n", MType::msg_error);

				if (!is_dedicated_server)
					PlaySoundConfirmation(pEntity, SND_FAILED);
			}
		}
		else if (FStrEq(arg1, teamONE.GetTeamName()))	// kick random red team bot
		{
			if (util.KickBot(100 + teamONE.GetTeamId()))
			{
				conOutput.Print(pEntity, "random %s bot was kicked\n", teamONE.GetTeamName(), MType::msg_default);

				if (!is_dedicated_server)
					PlaySoundConfirmation(pEntity, SND_DONE);
			}
			else
			{
				conOutput.Print(pEntity, "there are no %s bots\n", teamONE.GetTeamName2wordsLC(), MType::msg_error);

				if (!is_dedicated_server)
					PlaySoundConfirmation(pEntity, SND_FAILED);
			}
		}
		else if (FStrEq(arg1, teamTWO.GetTeamName()))	// kick random blue team bot
		{
			if (util.KickBot(100 + teamTWO.GetTeamId()))
			{
				conOutput.Print(pEntity, "random %s bot was kicked\n", teamTWO.GetTeamName(), MType::msg_default);

				if (!is_dedicated_server)
					PlaySoundConfirmation(pEntity, SND_DONE);
			}
			else
			{
				conOutput.Print(pEntity, "there are no %s bots\n", teamTWO.GetTeamName2wordsLC(), MType::msg_error);

				if (!is_dedicated_server)
					PlaySoundConfirmation(pEntity, SND_FAILED);
			}
		}		
		else
		{
			int index = util.FindBotByName(arg1);

			if (index != -1)
			{
				if (util.KickBot(index))
				{
					conOutput.Print(pEntity, "bot was successfully kicked\n", MType::msg_default);

					if (!is_dedicated_server)
						PlaySoundConfirmation(pEntity, SND_DONE);
				}
				//else
				//	sprintf(msg, "");	// null the output
			}
			else
			{
				conOutput.Print(pEntity, "no bot with such name\n", MType::msg_error);

				if (!is_dedicated_server)
					PlaySoundConfirmation(pEntity, SND_FAILED);
			}
		}
	}
}

void KillBotCommand(edict_t* pEntity, const char* arg1)
{
	if (strlen(arg1) < 1)
	{
		conOutput.PrintErrorMessage(conOutErrMsg::mis_arg, pEntity);
	}
	else if (FStrEq(arg1, "all"))		// kill all bots
	{
		if (util.KillBot(100))
		{
			conOutput.Print(pEntity, "all bots were killed\n", MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else
		{
			conOutput.Print(pEntity, "there are no bots\n", MType::msg_error);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_FAILED);
		}
	}
	else if (FStrEq(arg1, teamONE.GetTeamName()))	// kill all red team bots
	{
		if (util.KillBot(100 + teamONE.GetTeamId()))
		{
			conOutput.Print(pEntity, "all %s bots were killed\n", teamONE.GetTeamName2wordsLC(), MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else
		{
			conOutput.Print(pEntity, "there are no %s bots\n", teamONE.GetTeamName2wordsLC(), MType::msg_error);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_FAILED);
		}
	}
	else if (FStrEq(arg1, teamTWO.GetTeamName()))	// kill all blue team bots
	{
		if (util.KillBot(100 + teamTWO.GetTeamId()))
		{
			conOutput.Print(pEntity, "all %s bots were killed\n", teamTWO.GetTeamName2wordsLC(), MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else
		{
			conOutput.Print(pEntity, "there are no %s bots\n", teamTWO.GetTeamName2wordsLC(), MType::msg_error);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_FAILED);
		}
	}
	else
	{
		int index = util.FindBotByName(arg1);

		if (index != -1)
		{
			if (util.KillBot(index))
			{
				conOutput.Print(pEntity, "bot was successfully killed\n", MType::msg_default);

				if (!is_dedicated_server)
					PlaySoundConfirmation(pEntity, SND_DONE);
			}
			//else
			//	sprintf(msg, "");
		}
		else
		{
			conOutput.Print(pEntity, "no bot with such name\n", MType::msg_error);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_FAILED);
		}
	}
}

void RandomSkillCommand(edict_t* pEntity, const char* arg1)
{
	// no argument specified then toggle between on and off
	if (strlen(arg1) < 1)
	{
		// and tell the user that he used toggle mode
		if (!is_dedicated_server)
		{
			PlaySoundConfirmation(pEntity, SND_DONE);
			conOutput.Print(pEntity, "***toggle mode used***\n", MType::msg_null);
		}

		if (externals.GetRandomSkill())
		{
			externals.SetRandomSkill(FALSE);
			conOutput.Print(pEntity, "DISABLED\n", conInput.GetCmdName(), MType::msg_default);
		}
		else
		{
			externals.SetRandomSkill(TRUE);
			conOutput.Print(pEntity, "ENABLED\n", conInput.GetCmdName(), MType::msg_default);
		}
	}
	else
	{
		// if there's some argument and the argument do match current then print current state
		if (strcmp(arg1, "current") == 0)
		{
			if (externals.GetRandomSkill())
				conOutput.Print(pEntity, "is currently ENABLED\n", conInput.GetCmdName(), MType::msg_default);
			else
				conOutput.Print(pEntity, "is currently DISABLED\n", conInput.GetCmdName(), MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		// do we need to turn this on?
		else if (strcmp(arg1, "on") == 0)
		{
			externals.SetRandomSkill(TRUE);
			conOutput.Print(pEntity, "ENABLED\n", conInput.GetCmdName(), MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		// do we need to turn this off?
		else if (strcmp(arg1, "off") == 0)
		{
			externals.SetRandomSkill(FALSE);
			conOutput.Print(pEntity, "DISABLED\n", conInput.GetCmdName(), MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		// otherwise the user must have used wrong/invalid argument
		else
		{
			conOutput.PrintErrorMessage(conOutErrMsg::inv_arg, pEntity);
		}
	}
}

void SpawnSkillCommand(edict_t* pEntity, const char* arg1)
{
	if (conInput.IsValidIntegerValue(arg1, 1, BOT_SKILL_LEVELS))
	{
		char msg[32 + cmd_name_length]{};

		externals.SetSpawnSkill(conInput.GetIntegerValue());

		sprintf(msg, "default %s set to %d\n", conInput.GetCmdName(), externals.GetSpawnSkill());
		conOutput.Print(pEntity, msg, MType::msg_default);

		if (!is_dedicated_server)
			PlaySoundConfirmation(pEntity, SND_DONE);
	}
	else
	{
		conOutput.PrintErrorMessage(externals.GetSpawnSkill(), pEntity);

		if (!is_dedicated_server)
			PlaySoundConfirmation(pEntity, SND_FAILED);
	}
}

void SetBotSkillCommand(edict_t* pEntity, const char* arg1)
{
	if (conInput.IsValidIntegerValue(arg1, 1, BOT_SKILL_LEVELS))
	{
		int result = util.ChangeBotSkillLevel(false, conInput.GetIntegerValue());

		char msg[32]{};
		sprintf(msg, "botskill set to %d\n", result);
		conOutput.Print(pEntity, msg, MType::msg_default);

		if (!is_dedicated_server)
			PlaySoundConfirmation(pEntity, SND_DONE);
	}
	else
	{
		conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);
	}
}

void BotSkillUpCommand(edict_t* pEntity, const char* arg1)
{
	if (util.ChangeBotSkillLevel(true, 1) > 0)
	{
		conOutput.Print(pEntity, "botskill increased by one level\n", MType::msg_default);

		if (!is_dedicated_server)
			PlaySoundConfirmation(pEntity, SND_DONE);
	}
	else
	{
		conOutput.Print(pEntity, "no bot on lower skill levels or no bot in game!\n", MType::msg_error);

		if (!is_dedicated_server)
			PlaySoundConfirmation(pEntity, SND_FAILED);
	}
}

void BotSkillDownCommand(edict_t* pEntity, const char* arg1)
{
	if (util.ChangeBotSkillLevel(true, -1) > 0)
	{
		conOutput.Print(pEntity, "botskill decreased by one level\n", MType::msg_default);

		if (!is_dedicated_server)
			PlaySoundConfirmation(pEntity, SND_DONE);
	}
	else
	{
		conOutput.Print(pEntity, "no bot on higher skill levels or no bot in game!\n", MType::msg_error);

		if (!is_dedicated_server)
			PlaySoundConfirmation(pEntity, SND_FAILED);
	}
}

void SetAimSkillCommand(edict_t* pEntity, const char* arg1)
{
	int temp = 0;

	if (conInput.IsValidIntegerValue(arg1, 1, BOT_SKILL_LEVELS))
	{
		int result = util.ChangeAimSkillLevel(conInput.GetIntegerValue());

		char msg[32]{};
		sprintf(msg, "aimskill set to %d\n", result);
		conOutput.Print(pEntity, msg, MType::msg_default);

		if (!is_dedicated_server)
			PlaySoundConfirmation(pEntity, SND_DONE);
	}
	else
	{
		conOutput.PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEntity);
	}
}

void SetReactionTimeCommand(edict_t* pEntity, const char* arg1)
{
	// the way that is used here actually doesn't need a float variable,
	// because the value is divided by 10 after the conversion in order
	// to move the decimal point to the left
	// basically it's done the same way as before where it was read as an integer
	// I left it that way to keep the compatibility as the users are already
	// used to write 23 to set the reactions to 2.3s for example
	// also it's a little faster as you don't need to type the decimal point character itself
	if (conInput.IsValidFloatValue(arg1, 0.0f, 50.0f))
	{
		externals.SetReactionTime(conInput.GetFloatValue() / 10.0f);

		char msg[32 + cmd_name_length]{};
		sprintf(msg, "%s set to %.1fs\n", conInput.GetCmdName(), externals.GetReactionTime());
		conOutput.Print(pEntity, msg, MType::msg_default);

		if (!is_dedicated_server)
			PlaySoundConfirmation(pEntity, SND_DONE);
	}
	else
	{
		conOutput.PrintErrorMessage(externals.GetReactionTime(), pEntity);
		
		if (!is_dedicated_server)
			PlaySoundConfirmation(pEntity, SND_FAILED);
	}
}

void RangeLimitCommand(edict_t* pEntity, const char* arg1)
{
	if (FStrEq(arg1, "help") || FStrEq(arg1, "?"))
	{
		conOutput.Print(pEntity, "All valid range_limit options\n", MType::msg_null);
		conOutput.Print(pEntity, "[range_limit <number>] the number can be from 500 to 7500\n", MType::msg_null);
		conOutput.Print(pEntity, "[range_limit <default>] sets it to default (ie. to 7500)\n", MType::msg_null);
		//conOutput.Print(pEntity, "[range_limit <current>] returns current value\n", MType::msg_null);
		conOutput.Print(pEntity, "[range_limit <help>] shows this help\n", MType::msg_null);
	}
	else if (FStrEq(arg1, "default"))
	{
		internals.ResetEnemyDistanceLimit();
		internals.ResetIsEnemyDistanceLimit();

		char msg[64 + cmd_name_length]{};
		sprintf(msg, "%s reset to default value (%.1f)\n", conInput.GetCmdName(), internals.GetEnemyDistanceLimit());
		conOutput.Print(pEntity, msg, MType::msg_default);

		if (!is_dedicated_server)
			PlaySoundConfirmation(pEntity, SND_DONE);
	}
	else if (conInput.IsValidFloatValue(arg1, 500.0f, 7500.0f))
	{
			internals.SetEnemyDistanceLimit(conInput.GetFloatValue());
			internals.SetIsEnemyDistanceLimit(true);

			char msg[32 + cmd_name_length]{};
			sprintf(msg, "%s set to %.1f\n", conInput.GetCmdName(), internals.GetEnemyDistanceLimit());
			conOutput.Print(pEntity, msg, MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
	}
	else
	{
		conOutput.PrintErrorMessage(internals.GetEnemyDistanceLimit(), pEntity);

		if (!is_dedicated_server)
			PlaySoundConfirmation(pEntity, SND_FAILED);
	}
}

void SetWaypointDirectoryCommand(edict_t* pEntity, const char* arg1)
{
	if (strlen(arg1) >= 1)
	{
		if (FStrEq(arg1, "help") || FStrEq(arg1, "?"))
		{
			conOutput.Print(pEntity, "All valid directory options\n", MType::msg_null);
			conOutput.Print(pEntity, "[directory <defaultwpts>] points to \"/marine_bot/defaultwpts\" directory\n", MType::msg_null);
			conOutput.Print(pEntity, "[directory <customwpts>] points to \"/marine_bot/customwpts\" directory\n", MType::msg_null);
			conOutput.Print(pEntity, "[directory <current>] returns which directory is used now\n", MType::msg_null);
			conOutput.Print(pEntity, "[directory <help>] shows this help\n", MType::msg_null);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else if (FStrEq(arg1, "customwpts"))
		{
			internals.SetIsCustomWaypoints(true);
			conOutput.Print(pEntity, "waypoint are now loaded from \"customwpts\" directory\n", MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else if (FStrEq(arg1, "defaultwpts"))
		{
			internals.ResetIsCustomWaypoints();
			conOutput.Print(pEntity, "waypoint are now loaded from \"defaultwpts\" directory\n", MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else if (FStrEq(arg1, "current"))
		{
			if (internals.IsCustomWaypoints())
				conOutput.Print(pEntity, "waypoint are now loaded from \"customwpts\" directory\n", MType::msg_default);
			else
				conOutput.Print(pEntity, "waypoint are now loaded from \"defaultwpts\" directory\n", MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else
		{
			conOutput.PrintErrorMessage(conOutErrMsg::inv_arg, pEntity);
		}
	}
	else
	{
		if (!is_dedicated_server)
		{
			PlaySoundConfirmation(pEntity, SND_DONE);
			conOutput.Print(pEntity, "***toggle mode used***\n", MType::msg_null);
		}

		if (internals.IsCustomWaypoints())
		{
			internals.ResetIsCustomWaypoints();
			conOutput.Print(pEntity, "waypoint are now loaded from \"defaultwpts\" directory\n", MType::msg_default);
		}
		else
		{
			internals.SetIsCustomWaypoints(true);
			conOutput.Print(pEntity, "waypoint are now loaded from \"customwpts\" directory\n", MType::msg_default);
		}
	}
}

void DontSpeakCommand(edict_t* pEntity, const char* arg1)
{
	if (strlen(arg1) < 1)
	{
		if (!is_dedicated_server)
		{
			PlaySoundConfirmation(pEntity, SND_DONE);
			conOutput.Print(pEntity, "***toggle mode used***\n", MType::msg_null);
		}

		if (externals.GetDontSpeak())
		{
			externals.SetDontSpeak(FALSE);
			conOutput.Print(pEntity, "DISABLED\n", conInput.GetCmdName(), MType::msg_default);
		}
		else
		{
			externals.SetDontSpeak(TRUE);
			conOutput.Print(pEntity, "ENABLED\n", conInput.GetCmdName(), MType::msg_default);
		}
	}
	else
	{
		if (strcmp(arg1, "current") == 0)
		{
			if (externals.GetDontSpeak())
				conOutput.Print(pEntity, "is currently ENABLED\n", conInput.GetCmdName(), MType::msg_default);
			else
				conOutput.Print(pEntity, "is currently DISABLED\n", conInput.GetCmdName(), MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else if (strcmp(arg1, "on") == 0)
		{
			externals.SetDontSpeak(TRUE);
			conOutput.Print(pEntity, "ENABLED\n", conInput.GetCmdName(), MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else if (strcmp(arg1, "off") == 0)
		{
			externals.SetDontSpeak(FALSE);
			conOutput.Print(pEntity, "DISABLED\n", conInput.GetCmdName(), MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else
		{
			conOutput.PrintErrorMessage(conOutErrMsg::inv_arg, pEntity);
		}

	}
}

void DontChatCommand(edict_t* pEntity, const char* arg1)
{
	if (strlen(arg1) < 1)
	{
		if (!is_dedicated_server)
		{
			PlaySoundConfirmation(pEntity, SND_DONE);
			conOutput.Print(pEntity, "***toggle mode used***\n", MType::msg_null);
		}

		if (externals.GetDontChat())
		{
			externals.SetDontChat(FALSE);
			conOutput.Print(pEntity, "DISABLED\n", conInput.GetCmdName(), MType::msg_default);
		}
		else
		{
			externals.SetDontChat(TRUE);
			conOutput.Print(pEntity, "ENABLED\n", conInput.GetCmdName(), MType::msg_default);
		}
	}
	else
	{
		if (strcmp(arg1, "current") == 0)
		{
			if (externals.GetDontChat())
				conOutput.Print(pEntity, "is currently ENABLED\n", conInput.GetCmdName(), MType::msg_default);
			else
				conOutput.Print(pEntity, "is currently DISABLED\n", conInput.GetCmdName(), MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else if (strcmp(arg1, "on") == 0)
		{
			externals.SetDontChat(TRUE);
			conOutput.Print(pEntity, "ENABLED\n", conInput.GetCmdName(), MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else if (strcmp(arg1, "off") == 0)
		{
			externals.SetDontChat(FALSE);
			conOutput.Print(pEntity, "DISABLED\n", conInput.GetCmdName(), MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else
		{
			conOutput.PrintErrorMessage(conOutErrMsg::inv_arg, pEntity);
		}
	}
}

void DontChatToBotsCommand(edict_t* pEntity, const char* arg1)
{
	if (strlen(arg1) < 1)
	{
		if (!is_dedicated_server)
		{
			PlaySoundConfirmation(pEntity, SND_DONE);
			conOutput.Print(pEntity, "***toggle mode used***\n", MType::msg_null);
		}

		if (externals.GetDontChatToBots())
		{
			externals.SetDontChatToBots(FALSE);
			conOutput.Print(pEntity, "DISABLED\n", conInput.GetCmdName(), MType::msg_default);
		}
		else
		{
			externals.SetDontChatToBots(TRUE);
			conOutput.Print(pEntity, "ENABLED\n", conInput.GetCmdName(), MType::msg_default);
		}
	}
	else
	{
		if (strcmp(arg1, "current") == 0)
		{
			if (externals.GetDontChatToBots())
				conOutput.Print(pEntity, "is currently ENABLED\n", conInput.GetCmdName(), MType::msg_default);
			else
				conOutput.Print(pEntity, "is currently DISABLED\n", conInput.GetCmdName(), MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else if (strcmp(arg1, "on") == 0)
		{
			externals.SetDontChatToBots(TRUE);
			conOutput.Print(pEntity, "ENABLED\n", conInput.GetCmdName(), MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else if (strcmp(arg1, "off") == 0)
		{
			externals.SetDontChatToBots(FALSE);
			conOutput.Print(pEntity, "DISABLED\n", conInput.GetCmdName(), MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else
		{
			conOutput.PrintErrorMessage(conOutErrMsg::inv_arg, pEntity);
		}
	}
}

void MeleeOnlyCommand(edict_t* pEntity, const char* arg1)
{
	if (strlen(arg1) < 1)
	{
		if (!is_dedicated_server)
		{
			PlaySoundConfirmation(pEntity, SND_DONE);
			conOutput.Print(pEntity, "***toggle mode used***\n", MType::msg_null);
		}

		if (internals.IsMeleeOnlyMode())
		{
			internals.SetMeleeOnlyMode(FALSE);
			conOutput.Print(pEntity, "DISABLED\n", conInput.GetCmdName(), MType::msg_default);
		}
		else
		{
			internals.SetMeleeOnlyMode(TRUE);
			conOutput.Print(pEntity, "ENABLED\n", conInput.GetCmdName(), MType::msg_default);
		}
	}
	else
	{
		if (strcmp(arg1, "current") == 0)
		{
			if (internals.IsMeleeOnlyMode())
				conOutput.Print(pEntity, "is currently ENABLED\n", conInput.GetCmdName(), MType::msg_default);
			else
				conOutput.Print(pEntity, "is currently DISABLED\n", conInput.GetCmdName(), MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else if (strcmp(arg1, "on") == 0)
		{
			internals.SetMeleeOnlyMode(TRUE);
			conOutput.Print(pEntity, "ENABLED\n", conInput.GetCmdName(), MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else if (strcmp(arg1, "off") == 0)
		{
			internals.SetMeleeOnlyMode(FALSE);
			conOutput.Print(pEntity, "DISABLED\n", conInput.GetCmdName(), MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else
		{
			conOutput.PrintErrorMessage(conOutErrMsg::inv_arg, pEntity);
		}
	}
}

void ChangeStarPositionsCommand(edict_t* pEntity, const char* arg1)
{
	if (strlen(arg1) < 1)
	{
		if (!is_dedicated_server)
		{
			PlaySoundConfirmation(pEntity, SND_DONE);
			conOutput.Print(pEntity, "***toggle mode used***\n", MType::msg_null);
		}

		if (internals.IsChangeStartPosition())
		{
			internals.SetChangeStartPositions(FALSE);
			conOutput.Print(pEntity, "DISABLED\n", conInput.GetCmdName(), MType::msg_default);
		}
		else
		{
			internals.SetChangeStartPositions(TRUE);
			conOutput.Print(pEntity, "ENABLED\n", conInput.GetCmdName(), MType::msg_default);
		}
	}
	else
	{
		if (strcmp(arg1, "current") == 0)
		{
			if (internals.IsChangeStartPosition())
				conOutput.Print(pEntity, "is currently ENABLED\n", conInput.GetCmdName(), MType::msg_default);
			else
				conOutput.Print(pEntity, "is currently DISABLED\n", conInput.GetCmdName(), MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else if (strcmp(arg1, "on") == 0)
		{
			internals.SetChangeStartPositions(TRUE);
			conOutput.Print(pEntity, "ENABLED\n", conInput.GetCmdName(), MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else if (strcmp(arg1, "off") == 0)
		{
			internals.SetChangeStartPositions(FALSE);
			conOutput.Print(pEntity, "DISABLED\n", conInput.GetCmdName(), MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else
		{
			conOutput.PrintErrorMessage(conOutErrMsg::inv_arg, pEntity);
		}
	}
}

void ChangeCaptureAreasCommand(edict_t* pEntity, const char* arg1)
{
	if (strlen(arg1) < 1)
	{
		if (!is_dedicated_server)
		{
			PlaySoundConfirmation(pEntity, SND_DONE);
			conOutput.Print(pEntity, "***toggle mode used***\n", MType::msg_null);
		}

		if (internals.IsChangeCaptureAreas())
		{
			internals.SetChangeCaptureAreas(FALSE);
			conOutput.Print(pEntity, "DISABLED\n", conInput.GetCmdName(), MType::msg_default);
		}
		else
		{
			internals.SetChangeCaptureAreas(TRUE);
			conOutput.Print(pEntity, "ENABLED\n", conInput.GetCmdName(), MType::msg_default);
		}
	}
	else
	{
		if (strcmp(arg1, "current") == 0)
		{
			if (internals.IsChangeCaptureAreas())
				conOutput.Print(pEntity, "is currently ENABLED\n", conInput.GetCmdName(), MType::msg_default);
			else
				conOutput.Print(pEntity, "is currently DISABLED\n", conInput.GetCmdName(), MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else if (strcmp(arg1, "on") == 0)
		{
			internals.SetChangeCaptureAreas(TRUE);
			conOutput.Print(pEntity, "ENABLED\n", conInput.GetCmdName(), MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else if (strcmp(arg1, "off") == 0)
		{
			internals.SetChangeCaptureAreas(FALSE);
			conOutput.Print(pEntity, "DISABLED\n", conInput.GetCmdName(), MType::msg_default);

			if (!is_dedicated_server)
				PlaySoundConfirmation(pEntity, SND_DONE);
		}
		else
		{
			conOutput.PrintErrorMessage(conOutErrMsg::inv_arg, pEntity);
		}
	}
}




// functions used in this file

void PrintBasicBotInfo(edict_t* pEdict, int bot_array_index)
{
	char msg[256];
	char behaviour1[12];
	char behaviour2[24];
	char behaviour3[TEXT_MSG_SIZE]{};

	if (bots[bot_array_index].IsBehaviour(STANDARD))
		strcpy(behaviour1, "standard");
	else if (bots[bot_array_index].IsBehaviour(ATTACKER))
		strcpy(behaviour1, "attacker");
	else if (bots[bot_array_index].IsBehaviour(DEFENDER))
		strcpy(behaviour1, "defender");

	if (bots[bot_array_index].IsBehaviour(COMMON))
		strcpy(behaviour2, "common soldier");
	else if (bots[bot_array_index].IsBehaviour(CQUARTER))
		strcpy(behaviour2, "close quarter");
	else if (bots[bot_array_index].IsBehaviour(MGUNNER))
		strcpy(behaviour2, "gunner");
	else if (bots[bot_array_index].IsBehaviour(SNIPER))
		strcpy(behaviour2, "sniper");
	else if (bots[bot_array_index].IsBehaviour(AASPEC))
		strcpy(behaviour2, "anti-armor specialist");

	if (bots[bot_array_index].IsNeed(NEED_GOAL))
	{
		if (behaviour3[0] == 0)
			strcpy(behaviour3, " <seek map goals>");
		else
			strcat(behaviour3, " <seek map goals>");
	}

	if (bots[bot_array_index].IsNeed(NEED_EXLOSIVESCHARGE))
	{
		if (behaviour3[0] == 0)
			strcpy(behaviour3, " <seek explosives>");
		else
			strcat(behaviour3, " <seek explosives>");
	}

	if (bots[bot_array_index].IsTask(TASK_GOALITEM))
	{
		if (behaviour3[0] == 0)
			strcpy(behaviour3, " <has goal item>");
		else
			strcat(behaviour3, " <has goal item>");
	}

	if (bots[bot_array_index].IsTask(TASK_AVOID_ENEMY))
	{
		if (behaviour3[0] == 0)
			strcpy(behaviour3, " <avoiding enemy>");
		else
			strcat(behaviour3, " <avoiding enemy>");
	}

	if (bots[bot_array_index].IsTask(TASK_IGNORE_ENEMY))
	{
		if (behaviour3[0] == 0)
			strcpy(behaviour3, " <ignoring enemy>");
		else
			strcat(behaviour3, " <ignoring enemy>");
	}

	sprintf(msg, "[%d]%s - class=%d | behaviour: <%s> <%s>%s\n", bot_array_index + 1, bots[bot_array_index].name, bots[bot_array_index].GetBotClass(), behaviour1, behaviour2, behaviour3);
	conOutput.PrintToClient(pEdict, HUD_PRINTCONSOLE, msg);
}
