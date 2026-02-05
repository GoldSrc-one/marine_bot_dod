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
// bot_manager.cpp
// 
////////////////////////////////////////////////////////////////////////////////////////////////

#pragma warning( disable: 4005 91 )

#include "extdll.h"

#pragma warning( default: 4005 91 )

#include <cstring>

#include "bot_manager.h"

client_t::client_t()
{
	pEntity = NULL;
	client_is_human = false;
	client_bleeds = false;
	max_speed_time = -1.0f;
}

// set all botmanager variables to defaults
botmanager_t::botmanager_t()
{
	ResetTeamsBalanceNeeded();
	ResetOverrideTeamsBalance();
	ResetTimeOfTeamsBalanceCheck();
	ResetTeamsBalanceValue();
	ResetListenServerFilling();
	ResetBotCheckTime();
	ResetBotsToBeAdded();
}

// set all botdebugger variables to default values
botdebugger_t::botdebugger_t()
{
	ResetObserverMode();
	ResetFreezeMode();
	ResetDontShoot();
	ResetDontShootFirearm();
	ResetIgnoreAll();
	ResetDebugAims();
	ResetDebugActions();
	ResetDebugCross();
	ResetDebugPaths();
	ResetDebugStance();
	ResetDebugStuck();
	ResetDebugWaypoints();
	ResetDebugWeapons();
}

// what we do here is that we set all external variables to their default values
externals_t::externals_t()
{
	ResetIsLogging();
	ResetRandomSkill();
	ResetSpawnSkill();
	ResetReactionTime();
	ResetBalanceTime();
	ResetMinBots();
	ResetMaxBots();
	ResetInfoTime();
	ResetPresentationTime();
	ResetDontSpeak();
	ResetDontChat();
	ResetDontChatToBots();
	ResetRichNames();
	ResetCustomHeadshotPercentage();
	ResetGrenadeUsePercentage();
	ResetAlternativeStartPositions();
	ResetModifiedCaptureAreas();
}

// resets the dynamically assigned variables (eg. alternative start points) on map change
void externals_t::ResetOnMapChange(void)
{
	ResetAlternativeStartPositions();
	ResetModifiedCaptureAreas();
}

// what we do here is that we set all internal variables to their default values
internals_t::internals_t()
{
	ResetIsEnemyDistanceLimit();
	ResetEnemyDistanceLimit();
	ResetChangeStartPositions();
	ResetChangeCaptureAreas();
	ResetHUDMessageTime();
	ResetMeleeOnlyMode();
	ResetIsCustomWaypoints();
	ResetWaypointsAutoSave();
	ResetCustomDefaultWaypointRange();
	ResetPathToContinue();
	ResetIsWaypointConversionUnfinished();
	ResetUpdateWaypointDataTime();
	ResetMBFolderName();
	ResetInternalMessage();
	ResetOverrideClientPrint();
	ResetNullEngineTextMsgState();
	ResetRoundState();
	ResetIsBritishTeam();
	ResetAmerNamesCount();
	ResetBritNamesCount();
	ResetGerNamesCount();
	ResetMapGoalBasedOnExplosives();
	ResetCheckTriggerCapMessage();
	ResetBuildCaptureAreasFile();
	ResetIsFixParticleManagerCrash();
}

// allows setting defaults on map change
void internals_t::ResetOnMapChange(void)
{
	ResetIsEnemyDistanceLimit();
	ResetEnemyDistanceLimit();
	ResetHUDMessageTime();
	ResetIsCustomWaypoints();
	ResetWaypointsAutoSave();
	ResetCustomDefaultWaypointRange();
	ResetPathToContinue();
	ResetIsWaypointConversionUnfinished();
	ResetUpdateWaypointDataTime();
	ResetInternalMessage();
	ResetOverrideClientPrint();
	ResetNullEngineTextMsgState();
	ResetRoundState();
	ResetIsBritishTeam();
	ResetCheckTriggerCapMessage();
	ResetIsFixParticleManagerCrash();
}

// set all variables to defaults
unified_error_messages_system_t::unified_error_messages_system_t()
{
	ResetErrorCodes();
	ResetCopyOfErCodes();
	ResetMessageTime();
	ResetHistoryOfLastAddedErCode();
	ResetError();
	ResetWarning();
}

// adds given bit to the message bitmap, also sets this flag to 1 step history
void unified_error_messages_system_t::AddErrorCode(int msg_flag)
{
	messages_bitmap = messages_bitmap | msg_flag;

	last_added_message_flag = msg_flag;
}

// removes given bit from the messages bitmap, checking for presence first
void unified_error_messages_system_t::DeleteErrorCode(int msg_flag)
{
	if (messages_bitmap & msg_flag)
		messages_bitmap = messages_bitmap & ~msg_flag;
}

// removes given bit from the copy of messages bitmap, checking for presence first
void unified_error_messages_system_t::DeleteFromCopyOfErCodes(int msg_flag)
{
	if (mbitmap_copy & msg_flag)
		mbitmap_copy = mbitmap_copy & ~msg_flag;
}

void unified_error_messages_system_t::MakeCopyOfErCodes(void)
{
	mbitmap_copy = messages_bitmap;

	// remove useless messages from the copy
	DeleteFromCopyOfErCodes(UEMS_WELCOME1);
	DeleteFromCopyOfErCodes(UEMS_WELCOME2);
	DeleteFromCopyOfErCodes(UEMS_WELCOME3);
	DeleteFromCopyOfErCodes(UEMS_ALLWSENT);
}

// builds the error and warning messages based on the message flag
void unified_error_messages_system_t::PrepareErrorAndWarning(int msg_flag)
{
	char the_error[sizeof(error_message)]{};
	char the_warning[sizeof(warning_message)]{};

	the_error[0] = 0;
	the_warning[0] = 0;

	// first clear them to prevent printing invalid messages
	ResetError();
	ResetWarning();

	// if there is no given flag then try to use the last added message flag
	if (msg_flag == 0)
		msg_flag = last_added_message_flag;

	// if there is still nothing then quit
	if (msg_flag == 0)
		return;

	if (msg_flag == UEMS_ER_CFG)
	{
		// we read the constant message from its beginning up to the exclamation mark
		UTIL_StringFromBuffer(the_error, uems_cfg, -1, '!');
		// now we read the second part of the constant message
		UTIL_StringFromBuffer(the_warning, uems_cfg, '\n', '\0');
	}
	else if (msg_flag == UEMS_ER_WPNDEF)
	{
		UTIL_StringFromBuffer(the_error, uems_wpndef, -1, '!');
		UTIL_StringFromBuffer(the_warning, uems_wpndef, '\n', '\0');
	}
	else if (msg_flag == UEMS_ER_WPNLNK)
	{
		UTIL_StringFromBuffer(the_error, uems_wpnlink, -1, '!');
		UTIL_StringFromBuffer(the_warning, uems_wpnlink, '\n', '\0');
	}
	else if (msg_flag == UEMS_WARN_CFG_CMDS)
	{
		UTIL_StringFromBuffer(the_error, uems_cfgcmds, -1, '!');
		UTIL_StringFromBuffer(the_warning, uems_cfgcmds, '\n', '\0');
	}
	else if (msg_flag == UEMS_WARN_CFG_CVARS)
	{
		UTIL_StringFromBuffer(the_error, uems_cfgcvars, -1, '!');
		UTIL_StringFromBuffer(the_warning, uems_cfgcvars, '\n', '\0');
	}
	else if (msg_flag == UEMS_WARN_CFG_MISSCC)
	{
		UTIL_StringFromBuffer(the_error, uems_cfgmisscc, -1, '!');
		UTIL_StringFromBuffer(the_warning, uems_cfgmisscc, '\n', '\0');
	}
	else if (msg_flag == UEMS_WARN_CFG_NULLCC)
	{
		UTIL_StringFromBuffer(the_error, uems_cfgnullcc, -1, '!');
		UTIL_StringFromBuffer(the_warning, uems_cfgnullcc, '\n', '\0');
	}
	else if (msg_flag == UEMS_WARN_CFG_REC)
	{
		UTIL_StringFromBuffer(the_error, uems_cfgrec, -1, '!');
		UTIL_StringFromBuffer(the_warning, uems_cfgrec, '\n', '\0');
	}
	else if (msg_flag == UEMS_WARN_BNAME)
	{
		UTIL_StringFromBuffer(the_error, uems_bname, -1, '!');
		UTIL_StringFromBuffer(the_warning, uems_bname, '\n', '\0');
	}
	else if (msg_flag == UEMS_WARN_WPT)
	{
		UTIL_StringFromBuffer(the_error, uems_wpt, -1, '!');
		UTIL_StringFromBuffer(the_warning, uems_wpt, '\n', '\0');
	}
	else if (msg_flag == UEMS_WARN_PTH)
	{
		UTIL_StringFromBuffer(the_error, uems_pth, -1, '!');
		UTIL_StringFromBuffer(the_warning, uems_pth, '\n', '\0');
	}
	else if (msg_flag == UEMS_WARN_TARGOFS)
	{
		UTIL_StringFromBuffer(the_error, uems_targofs, -1, '!');
		UTIL_StringFromBuffer(the_warning, uems_targofs, '\n', '\0');
	}

	if (the_error[0] && the_warning[0])
	{
		// add new line at the end of both messages
		strcat(the_error, "\n");
		strcat(the_warning, "\n");

		// and store them
		SetError(the_error);
		SetWarning(the_warning);
	}
}


// returns the first error message found in the message bitmap that wasn't displayed yet
bool unified_error_messages_system_t::GetHUDErrorMessage(char* message)
{
	// we can't display any message until we are done with the standard welcome messages
	if (IsErrorCode(UEMS_ALLWSENT) == false)
		return false;

	message[0] = 0;

	// check whether the copy of the bitmap is empty
	if (IsCopyOfErCodesEmpty())
	{
		// if so then make a new copy of the bitmap to work with
		MakeCopyOfErCodes();
	}

	if (IsInCopyOfErCodes(UEMS_ER_CFG))
	{
		// prepare the message for this error
		sprintf(message, "%s %s", uems_header, uems_cfg);

		// and clear this bit to know that we have displayed this message
		// ie. so that we can display another message next time
		DeleteFromCopyOfErCodes(UEMS_ER_CFG);
	}
	else if (IsInCopyOfErCodes(UEMS_ER_WPNDEF))
	{
		sprintf(message, "%s %s", uems_header, uems_wpndef);
		DeleteFromCopyOfErCodes(UEMS_ER_WPNDEF);
	}
	else if (IsInCopyOfErCodes(UEMS_ER_WPNLNK))
	{
		sprintf(message, "%s %s", uems_header, uems_wpnlink);
		DeleteFromCopyOfErCodes(UEMS_ER_WPNLNK);
	}
	else if (IsInCopyOfErCodes(UEMS_WARN_CFG_CMDS))
	{
		sprintf(message, "%s %s", uems_header, uems_cfgcmds);
		DeleteFromCopyOfErCodes(UEMS_WARN_CFG_CMDS);
	}
	else if (IsInCopyOfErCodes(UEMS_WARN_CFG_CVARS))
	{
		sprintf(message, "%s %s", uems_header, uems_cfgcvars);
		DeleteFromCopyOfErCodes(UEMS_WARN_CFG_CVARS);
	}
	else if (IsInCopyOfErCodes(UEMS_WARN_CFG_MISSCC))
	{
		sprintf(message, "%s %s", uems_header, uems_cfgmisscc);
		DeleteFromCopyOfErCodes(UEMS_WARN_CFG_MISSCC);
	}
	else if (IsInCopyOfErCodes(UEMS_WARN_CFG_NULLCC))
	{
		sprintf(message, "%s %s", uems_header, uems_cfgnullcc);
		DeleteFromCopyOfErCodes(UEMS_WARN_CFG_NULLCC);
	}
	else if (IsInCopyOfErCodes(UEMS_WARN_CFG_REC))
	{
		sprintf(message, "%s %s", uems_header, uems_cfgrec);
		DeleteFromCopyOfErCodes(UEMS_WARN_CFG_REC);
	}
	else if (IsInCopyOfErCodes(UEMS_WARN_BNAME))
	{
		sprintf(message, "%s %s", uems_header, uems_bname);
		DeleteFromCopyOfErCodes(UEMS_WARN_BNAME);
	}
	// it makes more sense to have missing waypoint file before missing path file
	// instead of following alphabetical order of the flags
	else if (IsInCopyOfErCodes(UEMS_WARN_WPT))
	{
		sprintf(message, "%s %s", uems_header, uems_wpt);
		DeleteFromCopyOfErCodes(UEMS_WARN_WPT);
	}
	else if (IsInCopyOfErCodes(UEMS_WARN_PTH))
	{
		sprintf(message, "%s %s", uems_header, uems_pth);
		DeleteFromCopyOfErCodes(UEMS_WARN_PTH);
	}
	else if (IsInCopyOfErCodes(UEMS_WARN_TARGOFS))
	{
		sprintf(message, "%s %s", uems_header, uems_targofs);
		DeleteFromCopyOfErCodes(UEMS_WARN_TARGOFS);
	}

	// did we find any error to show on screen?
	if (message[0])
	{
		return true;
	}

	return false;
}

bool unified_error_messages_system_t::IsAnyErrorMessage(void)
{
	if (IsErrorCode(UEMS_ER_CFG) ||
		IsErrorCode(UEMS_ER_WPNDEF) ||
		IsErrorCode(UEMS_ER_WPNLNK))
		return true;

	return false;
}

bool unified_error_messages_system_t::IsAnyWarningMessage(void)
{
	if (IsErrorCode(UEMS_WARN_CFG_CMDS) ||
		IsErrorCode(UEMS_WARN_CFG_CVARS) ||
		IsErrorCode(UEMS_WARN_CFG_MISSCC) ||
		IsErrorCode(UEMS_WARN_CFG_NULLCC) ||
		IsErrorCode(UEMS_WARN_CFG_REC) ||
		IsErrorCode(UEMS_WARN_BNAME) ||
		IsErrorCode(UEMS_WARN_PTH) ||
		IsErrorCode(UEMS_WARN_TARGOFS) ||
		IsErrorCode(UEMS_WARN_WPT))
		return true;

	return false;
}

/*
* resets the messages bitmap (unless there's any error or warning detected)
* resets the copy of the bitmap
* resets the message time
* all of that is needed on a map change
*/
void unified_error_messages_system_t::ResetMessageSystem(void)
{
	// if there was an error detected then prevent resetting the bits
	// and displaying the standard welcome messages
	if (IsAnyErrorMessage() == false)
	{
		// if there was just a warning then prevent only resetting the bits...
		if (IsAnyWarningMessage() == false)
			ResetErrorCodes();
		// but allow standard welcome messages
		else
			DeleteErrorCode(UEMS_ALLWSENT);

		// starts the sequence of standard welcome messages
		AddErrorCode(UEMS_WELCOME1);
	}

	// always reset the copy of the messages bitmap
	ResetCopyOfErCodes();

	ResetMessageTime();
	ResetHistoryOfLastAddedErCode();
}

// used to handle the sequence of welcome messages
void unified_error_messages_system_t::SetNextWelcome(void)
{
	if (IsErrorCode(UEMS_WELCOME1))
	{
		// we set the next message to be displayed
		AddErrorCode(UEMS_WELCOME2);
		// and clear this message
		DeleteErrorCode(UEMS_WELCOME1);
		return;
	}

	if (IsErrorCode(UEMS_WELCOME2))
	{
		AddErrorCode(UEMS_WELCOME3);
		DeleteErrorCode(UEMS_WELCOME2);
		return;
	}

	if (IsErrorCode(UEMS_WELCOME3))
	{
		// we are using 3 welcome messages in total now so
		// we simply set "all done" here to stop the sequence
		AddErrorCode(UEMS_ALLWSENT);
		DeleteErrorCode(UEMS_WELCOME3);
		return;
	}
}


/*
* sets the defaults
*/
development_tools_t::development_tools_t()
{

#ifdef DEBUG

	ResetSpecificBotDebugging();
	ResetPointerToSpecificBot();
	SetDisplayTracelines(false);
	ResetOverrideDisplayTL();
	SetTLBeamDuration(5);
	ResetTLBeamColor();// use team colors if the entity has a team value specified

#endif // DEBUG

}

#ifdef DEBUG

/*
* sets a new color for Trace Lines
*/
void development_tools_t::SetTLBeamColor(const char* newColor)
{
	if (newColor[0] != 0)
	{
		if (strcmp(newColor, "blue") == 0)
			tl_beam_color = tlc_blue;
		else if (strcmp(newColor, "green") == 0)
			tl_beam_color = tlc_green;
	}
	else
		ResetTLBeamColor();
}


/*
* returns color code as a vector
*/
Vector development_tools_t::GetTLBeamColor(bool ignore_default_beam_color)
{
	Vector TLBeamColor;

	// sort of override to default beam color ... using green here, because that color isn't used much
	if (ignore_default_beam_color)
		tl_beam_color = tlc_green;

	switch (tl_beam_color)
	{
	case tlc_blue:
		TLBeamColor = Vector(0, 0, 255);
		break;

	case tlc_green:
		TLBeamColor = Vector(50, 255, 50);
		break;

	default:// we use white in this case, teambased also falls to this, because we can't use modTeams here so it will have to handled manually
		TLBeamColor = Vector(255, 255, 255);
		break;
	}

	return TLBeamColor;
}

#endif // DEBUG


/*
*/
control_point_t::control_point_t()
{
	pEntity = NULL;
	strcpy(point_name, "dod_cpoint_name");
	strcpy(point_linkname, "dod_cpoint_linkname");		// shouldn't be empty, just in case the game entity had empty targetname, because then we might get invalid results
	point_obj_list_index = 0;
	owned_by_team = 0;
	is_team_one_allowed_to_capture = true;
	is_team_two_allowed_to_capture = true;
	point_origin = Vector(0, 0, 0);
}


/*
*/
void control_point_t::ResetArray(void)
{
	for (int i = 0; i < MAX_CAPTUREPOINTS; i++)
	{
		ControlPoints[i].pEntity = NULL;
		strcpy(ControlPoints[i].point_name, "dod_cpoint_name");
		strcpy(ControlPoints[i].point_linkname, "dod_cpoint_linkname");
		ControlPoints[i].point_obj_list_index = -1;
		ControlPoints[i].owned_by_team = 0;
		ControlPoints[i].is_team_one_allowed_to_capture = true;
		ControlPoints[i].is_team_two_allowed_to_capture = true;
		ControlPoints[i].point_origin = Vector(0, 0, 0);
	}
}


/*
*/
int control_point_t::FindPointInArray(edict_t* pEntity)
{
	for (int i = 0; i < MAX_CAPTUREPOINTS; i++)
	{
		if (ControlPoints[i].pEntity == pEntity)
			return i;
	}

	return CAPTUREPOINTS_ERROR_VAL;
}


/*
*/
int control_point_t::FindPointByObjListIndex(int searched_point_index)
{
	for (int i = 0; i < MAX_CAPTUREPOINTS; i++)
	{
		if (ControlPoints[i].point_obj_list_index == searched_point_index)
			return i;
	}

	return CAPTUREPOINTS_ERROR_VAL;
}


/*
*/
int control_point_t::FindPointByLinkName(const char* name)
{
	for (int i = 0; i < MAX_CAPTUREPOINTS; i++)
	{
		if (strcmp(ControlPoints[i].point_linkname, name) == 0)
			return i;
	}

	return CAPTUREPOINTS_ERROR_VAL;
}


/*
*/
void control_point_t::AddNewPoint(edict_t* pEntity)
{
	// find empty array slot
	int index = FindPointInArray(NULL);

	// break it if there is no empty slot anymore
	if (index == CAPTUREPOINTS_ERROR_VAL)
		return;

	ControlPoints[index].pEntity = pEntity;
}


/*
*/
void control_point_t::SetPointName(edict_t* pEntity, const char* name)
{
	int index = FindPointInArray(pEntity);
	if (index == CAPTUREPOINTS_ERROR_VAL)
		return;

	strcpy(ControlPoints[index].point_name, name);
}


/*
*/
const char* control_point_t::GetPointName(int array_index)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return NULL;

	return ControlPoints[array_index].point_name;
}


/*
*/
void control_point_t::SetPointLinkName(edict_t* pEntity, const char* name)
{
	int index = FindPointInArray(pEntity);
	if (index == CAPTUREPOINTS_ERROR_VAL)
		return;

	strcpy(ControlPoints[index].point_linkname, name);
}


/*
*/
const char* control_point_t::GetPointLinkName(int array_index)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return NULL;

	return ControlPoints[array_index].point_linkname;
}


/*
*/
void control_point_t::SetPointObjListIndex(edict_t* pEntity, int value)
{
	int index = FindPointInArray(pEntity);
	if (index == CAPTUREPOINTS_ERROR_VAL)
		return;

	ControlPoints[index].point_obj_list_index = value;
}


/*
*/
void control_point_t::SetPointObjListIndex(int array_index, int value)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return;

	ControlPoints[array_index].point_obj_list_index = value;
}


/*
*/
int control_point_t::GetPointObjListIndex(edict_t* pEntity)
{
	int index = FindPointInArray(pEntity);
	if (index == CAPTUREPOINTS_ERROR_VAL)
		return CAPTUREPOINTS_ERROR_VAL;

	return ControlPoints[index].point_obj_list_index;
}


/*
*/
int control_point_t::GetPointObjListIndex(int array_index)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return CAPTUREPOINTS_ERROR_VAL;

	return ControlPoints[array_index].point_obj_list_index;
}


/*
*/
void control_point_t::SetOwnedByTeam(edict_t* pEntity, int team_id)
{
	int index = FindPointInArray(pEntity);
	if (index == CAPTUREPOINTS_ERROR_VAL)
		return;

	ControlPoints[index].owned_by_team = team_id;
}


/*
*/
void control_point_t::SetOwnedByTeam(int array_index, int team_id)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return;

	ControlPoints[array_index].owned_by_team = team_id;
}


/*
*/
int control_point_t::GetOwnedByTeam(edict_t* pEntity)
{
	int index = FindPointInArray(pEntity);
	if (index == CAPTUREPOINTS_ERROR_VAL)
		return CAPTUREPOINTS_ERROR_VAL;

	return ControlPoints[index].owned_by_team;
}


/*
*/
int control_point_t::GetOwnedByTeam(int array_index)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return CAPTUREPOINTS_ERROR_VAL;

	return ControlPoints[array_index].owned_by_team;
}


/*
*/
void control_point_t::SetTeamOneAllowedToCapture(edict_t* pEntity, int value)
{
	int index = FindPointInArray(pEntity);
	if (index == CAPTUREPOINTS_ERROR_VAL)
		return;

	if (value == 0)
		ControlPoints[index].is_team_one_allowed_to_capture = false;
}


/*
*/
void control_point_t::SetTeamOneAllowedToCapture(int array_index, bool value)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return;

	ControlPoints[array_index].is_team_one_allowed_to_capture = value;
}


/*
*/
bool control_point_t::GetTeamOneAllowedToCapture(int array_index)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return false;

	return ControlPoints[array_index].is_team_one_allowed_to_capture;
}


/*
*/
void control_point_t::SetTeamTwoAllowedToCapture(edict_t* pEntity, int value)
{
	int index = FindPointInArray(pEntity);
	if (index == CAPTUREPOINTS_ERROR_VAL)
		return;

	if (value == 0)
		ControlPoints[index].is_team_two_allowed_to_capture = false;
}


/*
*/
void control_point_t::SetTeamTwoAllowedToCapture(int array_index, bool value)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return;

	ControlPoints[array_index].is_team_two_allowed_to_capture = value;
}


/*
*/
bool control_point_t::GetTeamTwoAllowedToCapture(int array_index)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return false;

	return ControlPoints[array_index].is_team_two_allowed_to_capture;
}


/*
* checks if both values are the same and if so then it assigns them to true
* this is needed because some maps dispatch the Control Points with both values set to zero while other maps have them both set to 1 yet the Control Points work the same in game,
* so we basically check whether both values are the same for this Control Point and if so then no matter which value was read from the map we set this point as allowed to capture for both teams,
* and if the values differ then we keep values that we have read from the map
*/
void control_point_t::NormalizeAllowedToCapture(int array_index)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return;

	if (ControlPoints[array_index].is_team_one_allowed_to_capture == ControlPoints[array_index].is_team_two_allowed_to_capture)
	{
		ControlPoints[array_index].is_team_one_allowed_to_capture = true;
		ControlPoints[array_index].is_team_two_allowed_to_capture = true;
	}
}


/*
*/
void control_point_t::SetPointOrigin(edict_t* pEntity, const char* origin_as_string)
{
	int index = FindPointInArray(pEntity);
	if (index == CAPTUREPOINTS_ERROR_VAL)
		return;

	int x = 0, y = 0, z = 0;

	if (sscanf(origin_as_string, "%d %d %d", &x, &y, &z) == 3)
	{
		ControlPoints[index].point_origin.x = x;
		ControlPoints[index].point_origin.y = y;
		ControlPoints[index].point_origin.z = z;
	}
}


/*
*/
Vector control_point_t::GetPointOrigin(int array_index)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return Vector(0, 0, 0);

	return ControlPoints[array_index].point_origin;
}


/*
*/
dod_control_point_through_capture_area_t::dod_control_point_through_capture_area_t()
{
	pEntity = NULL;
	strcpy(point_name, "dod_carea_name");
	point_obj_list_index = 0;
	is_dod_object_required = false;
	owned_by_team = 0;
	time_to_capture = 0.0f;
	team_one_players_currently_present = 0;
	team_two_players_currently_present =0;
	team_one_players_to_capture = 0;
	team_two_players_to_capture = 0;
	is_team_one_allowed_to_capture = false;
	is_team_two_allowed_to_capture = false;
}


/*
*/
void dod_control_point_through_capture_area_t::ResetArray(void)
{
	for (int i = 0; i < MAX_CAPTUREPOINTS; i++)
	{
		dodCaptureArea[i].pEntity = NULL;
		strcpy(dodCaptureArea[i].point_name, "dod_carea_name");
		dodCaptureArea[i].point_obj_list_index = -1;
		dodCaptureArea[i].is_dod_object_required = false;
		dodCaptureArea[i].owned_by_team = 0;
		dodCaptureArea[i].time_to_capture = 0.0f;
		dodCaptureArea[i].team_one_players_currently_present = 0;
		dodCaptureArea[i].team_two_players_currently_present = 0;
		dodCaptureArea[i].team_one_players_to_capture = 0;
		dodCaptureArea[i].team_two_players_to_capture = 0;
		dodCaptureArea[i].is_team_one_allowed_to_capture = false;
		dodCaptureArea[i].is_team_two_allowed_to_capture = false;
	}
}


/*
*/
int dod_control_point_through_capture_area_t::FindPointInArray(edict_t* pEntity)
{
	for (int i = 0; i < MAX_CAPTUREPOINTS; i++)
	{
		if (dodCaptureArea[i].pEntity == pEntity)
			return i;
	}

	return CAPTUREPOINTS_ERROR_VAL;
}


/*
*/
int dod_control_point_through_capture_area_t::FindPointByObjListIndex(int searched_point_index)
{
	for (int i = 0; i < MAX_CAPTUREPOINTS; i++)
	{
		if (dodCaptureArea[i].point_obj_list_index == searched_point_index)
			return i;
	}
	
	return CAPTUREPOINTS_ERROR_VAL;
}


/*
*/
int dod_control_point_through_capture_area_t::FindPointByName(const char* name)
{
	for (int i = 0; i < MAX_CAPTUREPOINTS; i++)
	{
		if (strcmp(dodCaptureArea[i].point_name, name) == 0)
			return i;
	}

	return CAPTUREPOINTS_ERROR_VAL;
}


/*
*/
void dod_control_point_through_capture_area_t::AddNewPoint(edict_t* pEntity)
{
	int index = FindPointInArray(NULL);

	if (index == CAPTUREPOINTS_ERROR_VAL)
		return;

	dodCaptureArea[index].pEntity = pEntity;
}


/*
*/
void dod_control_point_through_capture_area_t::SetPointName(edict_t* pEntity, const char* name)
{
	int index = FindPointInArray(pEntity);
	if (index == CAPTUREPOINTS_ERROR_VAL)
		return;

	strcpy(dodCaptureArea[index].point_name, name);
}


/*
*/
const char* dod_control_point_through_capture_area_t::GetPointName(int array_index)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return NULL;
	
	return dodCaptureArea[array_index].point_name;
}


/*
*/
void dod_control_point_through_capture_area_t::SetPointObjListIndex(edict_t* pEntity, int value)
{
	int index = FindPointInArray(pEntity);
	if (index == CAPTUREPOINTS_ERROR_VAL)
		return;

	dodCaptureArea[index].point_obj_list_index = value;
}


/*
*/
void dod_control_point_through_capture_area_t::SetPointObjListIndex(int array_index, int value)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return;

	dodCaptureArea[array_index].point_obj_list_index = value;
}


/*
*/
int dod_control_point_through_capture_area_t::GetPointObjListIndex(int array_index)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return CAPTUREPOINTS_ERROR_VAL;

	return dodCaptureArea[array_index].point_obj_list_index;
}


/*
*/
void dod_control_point_through_capture_area_t::SetDodObjectRequired(edict_t* pEntity, bool value)
{
	int index = FindPointInArray(pEntity);
	if (index == CAPTUREPOINTS_ERROR_VAL)
		return;

	dodCaptureArea[index].is_dod_object_required = value;
}


/*
*/
bool dod_control_point_through_capture_area_t::GetDodObjectRequired(int array_index)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return CAPTUREPOINTS_ERROR_VAL;
	
	return dodCaptureArea[array_index].is_dod_object_required;
}


/*
*/
void dod_control_point_through_capture_area_t::SetOwnedByTeam(edict_t* pEntity, int team_id)
{
	int index = FindPointInArray(pEntity);
	if (index == CAPTUREPOINTS_ERROR_VAL)
		return;

	dodCaptureArea[index].owned_by_team = team_id;
}


/*
*/
void dod_control_point_through_capture_area_t::SetOwnedByTeam(int array_index, int team_id)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return;

	dodCaptureArea[array_index].owned_by_team = team_id;
}


/*
*/
int dod_control_point_through_capture_area_t::GetOwnedByTeam(int array_index)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return CAPTUREPOINTS_ERROR_VAL;

	return dodCaptureArea[array_index].owned_by_team;
}


/*
*/
void dod_control_point_through_capture_area_t::SetTimeToCapture(edict_t* pEntity, float time)
{
	int index = FindPointInArray(pEntity);
	if (index == CAPTUREPOINTS_ERROR_VAL)
		return;

	dodCaptureArea[index].time_to_capture = time;
}


/*
*/
float dod_control_point_through_capture_area_t::GetTimeToCapture(int array_index)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return 0.0f;

	return dodCaptureArea[array_index].time_to_capture;
}


/*
*/
void dod_control_point_through_capture_area_t::SetTeamOnePlayersCurrPresent(int array_index, int number)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return;

	dodCaptureArea[array_index].team_one_players_currently_present = number;
}


/*
*/
int dod_control_point_through_capture_area_t::GetTeamOnePlayersCurrPresent(int array_index)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return 0;

	return dodCaptureArea[array_index].team_one_players_currently_present;
}


/*
*/
void dod_control_point_through_capture_area_t::SetTeamTwoPlayersCurrPresent(int array_index, int number)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return;

	dodCaptureArea[array_index].team_two_players_currently_present = number;
}


/*
*/
int dod_control_point_through_capture_area_t::GetTeamTwoPlayersCurrPresent(int array_index)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return 0;

	return dodCaptureArea[array_index].team_two_players_currently_present;
}


/*
*/
void dod_control_point_through_capture_area_t::SetTeamOnePlayersToCapture(edict_t* pEntity, int number)
{
	int index = FindPointInArray(pEntity);
	if (index == CAPTUREPOINTS_ERROR_VAL)
		return;

	dodCaptureArea[index].team_one_players_to_capture = number;
}


/*
*/
int dod_control_point_through_capture_area_t::GetTeamOnePlayersToCapture(int array_index)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return CAPTUREPOINTS_ERROR_VAL;

	return dodCaptureArea[array_index].team_one_players_to_capture;
}


/*
*/
void dod_control_point_through_capture_area_t::SetTeamTwoPlayersToCapture(edict_t* pEntity, int number)
{
	int index = FindPointInArray(pEntity);
	if (index == CAPTUREPOINTS_ERROR_VAL)
		return;

	dodCaptureArea[index].team_two_players_to_capture = number;
}


/*
*/
int dod_control_point_through_capture_area_t::GetTeamTwoPlayersToCapture(int array_index)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return CAPTUREPOINTS_ERROR_VAL;

	return dodCaptureArea[array_index].team_two_players_to_capture;
}


/*
*/
void dod_control_point_through_capture_area_t::SetTeamOneAllowedToCapture(edict_t* pEntity, int value)
{
	int index = FindPointInArray(pEntity);
	if (index == CAPTUREPOINTS_ERROR_VAL)
		return;

	if (value == 1)
		dodCaptureArea[index].is_team_one_allowed_to_capture = true;
}


/*
*/
bool dod_control_point_through_capture_area_t::GetTeamOneAllowedToCapture(int array_index)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return false;

	return dodCaptureArea[array_index].is_team_one_allowed_to_capture;
}


/*
*/
void dod_control_point_through_capture_area_t::SetTeamTwoAllowedToCapture(edict_t* pEntity, int value)
{
	int index = FindPointInArray(pEntity);
	if (index == CAPTUREPOINTS_ERROR_VAL)
		return;

	if (value == 1)
		dodCaptureArea[index].is_team_two_allowed_to_capture = true;
}


/*
*/
bool dod_control_point_through_capture_area_t::GetTeamTwoAllowedToCapture(int array_index)
{
	if ((array_index < 0) || (array_index >= MAX_CAPTUREPOINTS))
		return false;

	return dodCaptureArea[array_index].is_team_two_allowed_to_capture;
}
