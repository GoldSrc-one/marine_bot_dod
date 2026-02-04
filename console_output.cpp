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
// console_output.cpp
// 
////////////////////////////////////////////////////////////////////////////////////////////////

#include "defines.h"

#pragma warning( disable: 4005 91 )

#include "extdll.h"
#include "util.h"
#include "cbase.h"

#pragma warning( default: 4005 91 )

#include "bot.h"
#include "bot_func.h"
#include "bot_manager.h"
#include "console_output.h"

//temp till the console input class gets its own .h (& .cpp) then remove it and replace it with the console input header
#include "client_commands.h"
//end temp

extern bool is_dedicated_server;


console_output_t::console_output_t()
{
}


/*
* prints info message to appropriate output using given format
*/
void console_output_t::Print(edict_t* pEdict, const char* message, MType msg_type)
{
	char output[1024]{};

	// print it into console of this user
	if (pEdict)
	{
		//ClientPrint(pEdict, HUD_PRINTNOTIFY, message);
		PrintToClient(pEdict, HUD_PRINTNOTIFY, message);
	}
	// print it into dedicated console
	else if (is_dedicated_server)
	{
		switch (msg_type)
		{
		case MType::msg_null:
			sprintf(output, "%s", message);
			break;
		case MType::msg_info:
			sprintf(output, "MARINE_BOT INFO - %s", message);
			break;
		case MType::msg_warning:
			sprintf(output, "MARINE_BOT WARNING - %s", message);
			break;
		case MType::msg_error:
			sprintf(output, "MARINE_BOT ERROR - %s", message);
			break;
		case MType::msg_critical_error:
			sprintf(output, "MARINE_BOT CRITICAL ERROR - %s", message);
			break;
		case MType::msg_cfg_passed:
			sprintf(output, "MARINE_BOT CFG FILE INIT - %s", message);
			break;
		case MType::msg_cfg_failed:
			sprintf(output, "MARINE_BOT CFG FILE ERROR - %s", message);
			break;
		default:
			sprintf(output, "MARINE_BOT - %s", message);
			break;
		}

		if (externals.GetIsLogging())
			LogPrint(output);
		else
		{
			// allows printing to the GUI Steam Console on Windows HLDS
			bool is_steam_gui_console = false;

#ifndef __linux__
			if (is_steam)
				is_steam_gui_console = true;
#endif

			if (is_steam_gui_console)
				EchoConsole(output);
			else
				printf(output);
		}
	}
	// print it into console while the map loads
	else
	{
		if (externals.GetIsLogging())
			LogPrint((char*)message);
		else
			ALERT(at_console, (char*)message);
	}
}


/*
* Overloaded to allow additional argument with the name of the variable the message refers to
*/
void console_output_t::Print(edict_t* pEdict, const char* message, const char* var_name, MType msg_type)
{
	if (var_name)
	{
		char merged_message[1024]{};	// NOTE: not sure about the sizes ... dunno how many characters can be printed at once to either console, NEEDS some testing
		char start_msg[512]{};
		char end_msg[512]{};
		int pos = 0;
		int pos2 = 0;
		bool formatting_found = false;

		while ((message[pos] != '\0') && (message[pos] != '%'))
		{
			start_msg[pos] = message[pos];
			pos++;
		}

		if (message[pos] == '%')
		{
			formatting_found = true;

			// terminate start message
			start_msg[pos] = '\0';

			// skip the %s signs
			pos += 2;
		}

		// we found specific formatting for this message...
		if (formatting_found)
		{
			while (message[pos] != '\0')
			{
				end_msg[pos2] = message[pos];

				pos++;
				pos2++;
			}

			end_msg[pos2 + 1] = '\0';

			// so let's build the message according to the formatting
			sprintf(merged_message, "%s %s %s", start_msg, var_name, end_msg);

			return Print(pEdict, merged_message, msg_type);
		}
		// no formatting was used so let's add the variable name to the beginning of the message
		else
		{
			sprintf(merged_message, "%s %s", var_name, message);

			return Print(pEdict, merged_message, msg_type);
		}
	}

	return Print(pEdict, message, msg_type);
}


/*
* prints a short message (only 128 characters) directly to the notify area (top left corner), can be used even when there isn't pointer to the client because it is using extern pointer to a listen server edict
* on Steam this is just one line and you need developer mode to be turned on otherwise the message goes to console
*/
void console_output_t::Notify(char* msg)
{
#ifdef DEBUG																			// NEW CODE 094 (remove it)

	Notify(msg, true);

#else


	Notify(msg, false);
#endif // DEBUG
}


/*
* prevents repeating the same message over and over again by using a static message memory
* however this static memory can be disabled by the switch
*/
void console_output_t::Notify(char* msg, bot_t* pBot, bool ignore_last_msg_filter)
{
#ifdef DEBUG																			// NEW CODE 094 (remove it)

	Notify(msg, true, pBot, ignore_last_msg_filter);

#else


	Notify(msg, false, pBot, ignore_last_msg_filter);
#endif // DEBUG
}


/*
* Overloaded to allow logging to file
*/
void console_output_t::Notify(char* msg, bool islogging)
{
	extern edict_t* listenserver_edict;

	if (msg == NULL)
		return;

	if (internals.IsOverrideClientPrint())
		return;

	// write it into file first, because this way if something goes wrong we should have the last event logged in the file
	if (islogging)
		util.DebugInFile(msg);

	// printing to notify area allows only 128 characters so we must cut the message here to prevent the overflow otherwise the console won't format the text correctly (no new line character)	
	int length = strlen(msg);
	if (length > 127)
	{
		msg[126] = '\n';
		msg[127] = '\0';
	}

	if (listenserver_edict)
		ClientPrint(listenserver_edict, HUD_PRINTNOTIFY, msg);
}


void console_output_t::Notify(char* msg, bool islogging, bot_t* pBot, bool ignore_last_msg_filter)
{
	extern edict_t* listenserver_edict;

	if (msg == NULL)
		return;

	if (internals.IsOverrideClientPrint())
		return;

#ifdef DEBUG

	// if we are debugging one specific bot and this isn't him then quit and don't print anything
	if (devTool.IsSpecificBotDebugging() && (devTool.GetPointerToSpecificBot() != pBot->pEdict))
		return;

#endif // DEBUG

	static char last_msg[TEXT_MSG_SIZE]{};

	if ((strcmp(msg, last_msg) != 0) || ignore_last_msg_filter)
	{
		if (islogging)
		{
			// log the name of this bot too
			char extended_msg[TEXT_MSG_SIZE + BOT_NAME_LEN + 4]{};
			sprintf(extended_msg, "(%s)%s", pBot->name, msg);
			util.DebugInFile(extended_msg);
		}

		strcpy(last_msg, msg);

		int length = strlen(msg);
		if (length > 127)
		{
			msg[126] = '\n';
			msg[127] = '\0';
		}

		if (listenserver_edict)
			ClientPrint(listenserver_edict, HUD_PRINTNOTIFY, msg);
	}
}


/*
* ensures that the message gets printed correctly even when it is longer than what the engine Text Message used by standard Client Print function allows
* ie. long message gets split in parts that are printed separately
*/
void console_output_t::PrintToClient(edict_t* pEdict, int msg_destination, const char* message)
{
	if (internals.IsOverrideClientPrint())
		return;

	int msg_len = strlen(message);

	// does the message fit into engine Text Message?
	if (msg_len < TEXT_MSG_SIZE)
	{
		// then simply print it as it is
		ClientPrint(pEdict, msg_destination, message);
	}
	// otherwise we will have to split it in parts and print them separately
	else
	{
		char temp_buffer[TEXT_MSG_SIZE]{};
		int tbpos = 0;

		for (int pos = 0; pos < msg_len; pos++)
		{
			// keep filling temporary buffer
			temp_buffer[tbpos] = message[pos];
			tbpos++;

			// print the buffer when it is almost full (we need one slot for the null terminating character) OR when we've reached the end of the message
			if ((tbpos == TEXT_MSG_SIZE - 2) || (pos == msg_len - 1))
			{
				// make sure the buffer is null terminated before printing it in order not to print nonsense from previous fillings
				// we've already increased the position so we are putting the null termination right behind the last copied character now
				temp_buffer[tbpos] = 0;

				ClientPrint(pEdict, msg_destination, temp_buffer);
				tbpos = 0;
			}
		}
	}
}


/*
* appends the string to add before the newline character in the text line
* basically it is a standard strcat function, but can deal with the newline character at the end of the line so it won't corrupt the formatting
*/
void console_output_t::AppendBeforeNewline(char* text_line, const char* string_to_add)
{
	int pos = strlen(text_line);
	bool newline_removed = false;

	if (pos > 0)
	{
		// is there a newline character at the end of the line?
		if (text_line[pos - 1] == '\n')
		{
			// then remove it
			text_line[pos - 1] = 0;
			newline_removed = true;
		}
	}

	strcat(text_line, string_to_add);

	// put the newline character back
	if (newline_removed)
		strcat(text_line, "\n");

	// terminate the line properly
	pos = strlen(text_line);
	text_line[pos] = 0;
}


/*
* ensures that given line of text doesn't break formating or crash the server due to being too long
* number of characters to scan means how far back from given max length position in the text line should this function go to find suitable space for adding given ending string (eg. "..." or "etc.")
* the reason to look for the space character is to keep whole words instead of cutting them randomly
*/
void console_output_t::ShortenLineOfText(char* text_line, const char* ending_string, int max_line_len, int num_chars_to_scan)
{
	int text_line_len = strlen(text_line);
	int ending_string_len = strlen(ending_string) + 1; // for the null terminating character although the Text Message doesn't seem to need it for example
	int pos;

	// does the text line exceed the limits?
	if (text_line_len > (max_line_len - 1))
	{
		// then go backwards through the line of text at most by given number of characters from given max length of the text line in order to...
		for (pos = max_line_len; pos >= (max_line_len - num_chars_to_scan); pos--)
		{
			// find the first space in the line that leaves enough free characters at the end to append given ending formatting string
			if ((text_line[pos] == ' ') && (pos < (max_line_len - ending_string_len)))
				break;
		}

		// terminate the line at found space or if for some reason we didn't find any then we will terminate it at the position we've reached as we were going through the text line backwards
		text_line[pos] = 0;

		// finally add given ending string to inform the user this text line had more information to show, but must have been shortened to prevent crashing the server or corrupt the formating
		strcat(text_line, ending_string);
	}
}


/*
* cuts given string from given text
*/
void console_output_t::RemoveStringFromBuffer(char* the_buffer, const char* string_to_remove)
{
	RemoveStringFromBuffer(the_buffer, string_to_remove, 0);
}


/*
* cuts given string from given text and starts looking for this string from given position in the text
*/
void console_output_t::RemoveStringFromBuffer(char* the_buffer, const char* string_to_remove, int start_at_pos)
{
	// check whether the string to remove actually is in the buffer
	char* result = strstr(&the_buffer[start_at_pos], string_to_remove);

	if (result != NULL)
	{
		int pos = result - the_buffer;	// convert the pointer to the first character of the string to remove to its position (ie. array index) in the buffer
		int num_of_chars_to_remove = strlen(string_to_remove);
		int buffer_len = strlen(the_buffer);

		// move the rest of the buffer behind the string to remove to the position where the string to remove originally started
		memmove(the_buffer + pos, the_buffer + pos + num_of_chars_to_remove, buffer_len - pos - num_of_chars_to_remove);

		// terminate shortened buffer properly else we would be printing even the characters at the end of previous longer buffer
		the_buffer[buffer_len - num_of_chars_to_remove] = 0;
	}
}


void console_output_t::PrintErrorMessage(conOutErrMsg error_message_type, edict_t* pEdict, bool disable_sound_confirmation)
{
	char msg[68]{};		// if adding new messages then make sure to increase array size so that the longest message can fit in

	if (error_message_type == conOutErrMsg::inv_arg)
		sprintf(msg, "invalid argument!\n");
	else if (error_message_type == conOutErrMsg::inv_mis_arg)
		sprintf(msg, "invalid or missing argument!\n");
	else if (error_message_type == conOutErrMsg::inv_wpt)
		sprintf(msg, "invalid waypoint (deleted/not allowed to work with it here)!\n");
	else if (error_message_type == conOutErrMsg::mis_arg)
		sprintf(msg, "missing argument!\n");
	else if (error_message_type == conOutErrMsg::no_pth)
		sprintf(msg, "that path doesn't exist/was deleted!\n");
	else if (error_message_type == conOutErrMsg::no_pth_no_wpt_nrb)
		sprintf(msg, "that path doesn't exist or not close enough to its waypoint!\n");
	else if (error_message_type == conOutErrMsg::no_wpt_nrb)
		sprintf(msg, "NO waypoint close enough!\n");
	else if (error_message_type == conOutErrMsg::inv_trig_name)
		sprintf(msg, "invalid trigger name!\nvalid names are: trigger1 ... trigger8\n");
	else if (error_message_type == conOutErrMsg::inv_trig_state)
		sprintf(msg, "invalid state value!\nvalid values are 'on' or 'off'\n");
	else if (error_message_type == conOutErrMsg::inv_team_val)
		sprintf(msg, "invalid team value (%s/%s)!\n", teamONE.GetTeamName(), teamTWO.GetTeamName());


	// play sound confirmation if NOT disabled and NOT on Dedicated Server
	if ((pEdict != NULL) && (disable_sound_confirmation == false) && (is_dedicated_server == false))
		PlaySoundConfirmation(pEdict, SND_FAILED);

	Print(pEdict, msg, MType::msg_error);
}


/*/																			NOT BEING USED
void console_output_t::PrintErrorMessage(bool var, edict_t* pEdict)
{
	char msg[32 + cmd_name_length];

	PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEdict);

	if (var)
		sprintf(msg, "current %s value is ENABLED!\n", conInput.GetCmdName());
	else
		sprintf(msg, "current %s value is DISABLED!\n", conInput.GetCmdName());

	Print(pEdict, msg, MType::msg_info);
}
/**/


void console_output_t::PrintErrorMessage(float var, edict_t* pEdict)
{
	char msg[32 + cmd_name_length]{};

	PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEdict);

	sprintf(msg, "current %s value is %.1f\n", conInput.GetCmdName(), var);
	Print(pEdict, msg, MType::msg_info);
}


void console_output_t::PrintErrorMessage(int var, edict_t* pEdict)
{
	char msg[32 + cmd_name_length]{};

	PrintErrorMessage(conOutErrMsg::inv_mis_arg, pEdict);

	sprintf(msg, "current %s value is %d\n", conInput.GetCmdName(), var);
	Print(pEdict, msg, MType::msg_info);
}


/*
* prints the message to steam dedicated server GUI console
*/
void console_output_t::EchoConsole(const char* message)
{
	char msg[1024]{};
	sprintf(msg, "echo %s\n", message);
	SERVER_COMMAND(msg);
}


/*
* works same as UTIL_LogPrintf only adds [MARINE_BOT] header before the logged message
*/
void console_output_t::LogPrint(char* fmt, ...)
{
	va_list argptr;
	static char string[1024];

	va_start(argptr, fmt);
	vsprintf(string, fmt, argptr);
	va_end(argptr);

	// Print to server console with MB header
	ALERT(at_logged, "[MARINE_BOT] %s", string);
}