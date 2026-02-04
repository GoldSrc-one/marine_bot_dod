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
// config.cpp
// 
////////////////////////////////////////////////////////////////////////////////////////////////

#include <cctype>
#include <cstdlib>
#include <cstring>

#include "bot_config.h"


config_t::config_t()
{
	fp = NULL;
	entry_name[0] = 0;
	ResetScope();
	cvar_name[0] = 0;
	out_of_scope = false;
	ResetReadError();
	ResetSectionError();
	ResetErrorMessage();
}


bool config_t::OpenConfigFile(const char* filename)
{
	fp = fopen(filename, "r");

	if (fp)
		return true;

	return false;
}


void config_t::CloseConfigFile(void)
{
	fclose(fp);
	fp = NULL;
}


void config_t::Rewind(void)
{
	rewind(fp);
	out_of_scope = false;
}


void config_t::ResetConfigHistory(void)
{
	entry_name[0] = 0;
	ResetScope();
	out_of_scope = false;
	ResetReadError();
	ResetSectionError();
	ResetErrorMessage();
}


/*
* looks up given string in the file while ignoring comments
*/
bool config_t::FindKey(const char* searched_key_name)
{
	if (fp != NULL)
	{
		int c;
		char the_entry[ceSize]{};

		the_entry[0] = 0;

		// scan strings one by one
		while (fscanf(fp, "%s", the_entry) != EOF)
		{
			// terminate it
			the_entry[sizeof(the_entry) - 1] = '\0';

			// when we find a comment then skip the rest of that line
			if ((strstr(the_entry, "#") != NULL) || (strstr(the_entry, "//") != NULL))
			{
				while (((c = getc(fp)) != '\n') && (c != EOF))
					;
			}

			// see if this is the key we are looking for
			if (strcmp(the_entry, searched_key_name) == 0)
			{
				// store this key name
				SetEntryName(the_entry);

				return true;
			}
		}

		// there must have been some syntax error in the file so report the problem
		CreateErrorMessage("missing key entry", searched_key_name);
	}

	// the config file isn't open or doesn't exist
	// or the key we are looking for is completely missing so
	// set a read error and return false
	
	SetReadError();

	return false;
}


/*
* looks up given string in the file while ignoring comments
* and keeping the scope using the 'scope end' variable
*/
bool config_t::FindKeyScope(const char* searched_key_name)
{
	if ((fp != NULL) && (IsOutOfScope() == false))
	{
		int c;
		char the_entry[ceSize]{};

		the_entry[0] = 0;

		while (fscanf(fp, "%s", the_entry) != EOF)
		{
			the_entry[sizeof(the_entry) - 1] = '\0';

			if ((strstr(the_entry, "#") != NULL) || (strstr(the_entry, "//") != NULL))
			{
				while (((c = getc(fp)) != '\n') && (c != EOF))
					;
			}

			if (strcmp(the_entry, searched_key_name) == 0)
			{
				SetEntryName(the_entry);
				return true;
			}

			// we got too far in the file
			// reading further would lead to getting an invalid data
			if (strcmp(the_entry, scope_end) == 0)
			{
				// to know something went wrong so
				// we can eventually rewind the file
				SetOutOfScope();
				
				SetReadError();
				CreateErrorMessage("missing key entry", searched_key_name);

				return false;
			}
		}

		CreateErrorMessage("missing key entry", searched_key_name);
	}

	SetReadError();
	return false;
}


/*
* reads one entry from the file while ignoring the comments
*/
bool config_t::ReadEntry(char* entry_string)
{
	entry_string[0] = 0;

	if (fp != NULL)
	{
		int c;
		char the_entry[ceSize]{};

		the_entry[0] = 0;

		while (fscanf(fp, "%s", the_entry) != EOF)
		{
			the_entry[sizeof(the_entry) - 1] = '\0';

			// skip all comments
			if ((strstr(the_entry, "#") != NULL) || (strstr(the_entry, "//") != NULL))
			{
				while (((c = getc(fp)) != '\n') && (c != EOF))
					;

				// and null it, because it's a comment not a valid entry
				the_entry[0] = 0;
			}

			// did we find a valid string?
			if (the_entry[0])
			{
				// then return it for further processing
				sprintf(entry_string, "%s", the_entry);
				return true;
			}
		}
	}

	return false;
}


/*
* reads one entry from the file while ignoring the comments and keeping the scope
* designed to be used in cyclic calling where reaching the 'scope end' variable will stop continuous reading
*/
bool config_t::ReadEntryScope(char* entry_string)
{
	entry_string[0] = 0;

	if ((fp != NULL) && (IsOutOfScope() == false))
	{
		int c;
		char the_entry[ceSize]{};

		the_entry[0] = 0;

		while (fscanf(fp, "%s", the_entry) != EOF)
		{
			the_entry[sizeof(the_entry) - 1] = '\0';

			if ((strstr(the_entry, "#") != NULL) || (strstr(the_entry, "//") != NULL))
			{
				while (((c = getc(fp)) != '\n') && (c != EOF))
					;

				the_entry[0] = 0;
			}

			// check if we didn't leave the scope already
			if (strcmp(the_entry, scope_end) == 0)
			{
				// we set only out of scope flag to prevent reading further
				// we don't set read error flag, because this isn't an error case
				// this statement is meant to return the false to break the cycle
				SetOutOfScope();
				return false;
			}

			if (the_entry[0])
			{
				sprintf(entry_string, "%s", the_entry);
				return true;
			}
		}
	}

	return false;
}


/*
* reads one value from the file and strips it from double quotes,
* ignores all comments
*/
bool config_t::ReadValue(char* entry_value)
{
	entry_value[0] = 0;

	if (fp != NULL)
	{
		char buffer[ceSize];
		int c;

		// get one string from the file
		if (fscanf(fp, "%s", buffer) == 1)
		{
			// terminate it
			buffer[sizeof(buffer) - 1] = '\0';

			// when we find a comment then skip the rest of that line
			if ((strstr(buffer, "#") != NULL) || (strstr(buffer, "//") != NULL))
			{
				while (((c = getc(fp)) != '\n') && (c != EOF))
					;

				// also null the buffer
				buffer[0] = 0;
			}

			// strip the value from double quotes
			UTIL_StringFromBuffer(entry_value, buffer, '"', '"');

			// if the value is valid return success
			if (entry_value[0])
				return true;
		}
		
		CreateErrorMessage("missing value for", entry_name);
	}

	SetReadError();
	return false;
}


/*
* reads one value from the file and strips it from double quotes,
* ignores all comments, but keeps the scope
*/
bool config_t::ReadValueScope(char* entry_value)
{
	entry_value[0] = 0;

	if ((fp != NULL) && (IsOutOfScope() == false))
	{
		char buffer[ceSize];
		int c;

		if (fscanf(fp, "%s", buffer) == 1)
		{
			buffer[sizeof(buffer) - 1] = '\0';

			if ((strstr(buffer, "#") != NULL) || (strstr(buffer, "//") != NULL))
			{
				while (((c = getc(fp)) != '\n') && (c != EOF))
					;

				buffer[0] = 0;
			}

			// first check whether we didn't get "out of scope"
			if (strcmp(buffer, scope_end) == 0)
			{
				SetOutOfScope();
				SetReadError();

				CreateErrorMessage("missing value for", entry_name);
				return false;
			}

			UTIL_StringFromBuffer(entry_value, buffer, '"', '"');

			if (entry_value[0])
				return true;
		}
		
		CreateErrorMessage("missing value for", entry_name);
	}

	SetReadError();
	return false;
}


bool config_t::ReadValueWithSpace(char* entry_value, int value_length)
{
	entry_value[0] = 0;

	if (fp != NULL)
	{
		bool end_reading = false;
		char buffer[evSize]{};
		int c, prev_c, len, ret_val, szbuffer, read_chars;

		// init it with something
		prev_c = '\0';

		buffer[0] = 0;
		read_chars = 0;

		// make sure the length of the value string doesn't exceed maximum size of the buffer
		if (value_length > evSize)
			value_length = evSize;

		while (((c = getc(fp)) != '\n') && (c != EOF))
		{
			// did we find a comment?
			if ((c == '#') || ((prev_c == '/') && (c == '/')))
				break;

			len = strlen(buffer);
			szbuffer = sizeof(buffer);

			// we can work only within the array size limits
			// or given max. amount of characters for the key variable (eg. bot name length)
			if (read_chars >= value_length)
			{
				CreateErrorMessage("invalid value (too long) for", entry_name);
				SetReadError();

				return false;
			}

			// copy the character at end of the buffer
			ret_val = snprintf(buffer + len, szbuffer - len, "%c", c);

			// check whether the character was successfully appended
			if ((ret_val > 0) && (ret_val < szbuffer))
			{
				if (c == '"')
				{
					// did we find the ending double quote character?
					if (end_reading)
					{
						// strip the value from double quotes
						UTIL_StringFromBuffer(entry_value, buffer, '"', '"');

						// if the value is valid return success
						if (entry_value[0])
							return true;
					}
					// this must be the opening double quote character so keep reading
					else
						end_reading = true;
				}

				// return invalid value when we are reading some value, but it's not inside double quotes
				// (ie. we are reading alphanumeric character, but we didn't read the opening double quotes character yet)
				if (isalnum(c) && (end_reading == false))
				{
					CreateErrorMessage("invalid value for", entry_name);
					SetReadError();

					return false;
				}
			}
			// if the appending failed then we'll stop it here and return read error
			else
				break;

			// store current character away for tests 
			prev_c = c;

			// increase the number of read characters
			read_chars++;
		}

		CreateErrorMessage("missing value for", entry_name);
	}

	SetReadError();
	return false;
}


/*
* reads yes/no value while keeping scope and sets default value on error
*/
bool config_t::ReadBooleanValue(bool default_value)
{
	char entry_value[evSize]{};
	entry_value[0] = 0;

	if (fp != NULL)
	{
		if (ReadValueScope(entry_value))
		{

			// TODO: Add code to convert the value string to all lowercase in case someone messed that up in the external file
			//		(strlwr is MS only stuff)


			// test the validity of the value
			if (strcmp(entry_value, "yes") == 0)
			{
				return true;
			}
			else if (strcmp(entry_value, "no") == 0)
			{
				return false;
			}
			else
			{
				CreateErrorMessage("invalid value for", entry_name);
			}
		}
	}

	SetReadError();
	return default_value;
}


/*
* reads float value while keeping scope and sets default value on error
*/
float config_t::ReadFloatValue(float default_value)
{
	char entry_value[evSize]{};
	entry_value[0] = 0;

	if (fp != NULL)
	{
		// get the next string from the file
		if (ReadValueScope(entry_value))
		{
			// see if the value is really a number
			// ie. the first character is a decimal number OR
			// it is a minus sign and the second character is decimal number
			if (isdigit(entry_value[0]) || ((entry_value[0] == '-') && isdigit(entry_value[1])))
			{
				// convert the value to a float number and return it
				return strtof(entry_value, NULL);
			}

			CreateErrorMessage("invalid value for", entry_name);
		}
	}

	SetReadError();
	return default_value;
}


/*
* reads integer value while keeping scope and sets default value on error
*/
int config_t::ReadIntegerValue(int default_value)
{
	char entry_value[evSize]{};
	entry_value[0] = 0;

	if (fp != NULL)
	{
		if (ReadValueScope(entry_value))
		{
			if (isdigit(entry_value[0]) || ((entry_value[0] == '-') && isdigit(entry_value[1])))
			{
				// convert the value to an integer number and return it
				return atoi(entry_value);
			}

			CreateErrorMessage("invalid value for", entry_name);
		}
	}

	SetReadError();
	return default_value;
}


void config_t::ReadFloatArray(float the_array[], int array_size, float default_value)
{
	int i;
	char entry_value[evSize]{};
	bool valid_data = true;

	entry_value[0] = 0;

	if (fp != NULL)
	{
		for (i = 0; i < array_size; i++)
		{
			// get the next string following the entry
			if (valid_data && ReadValueScope(entry_value))
			{
				// see if the value is really a number
				if (isdigit(entry_value[0]) || ((entry_value[0] == '-') && isdigit(entry_value[1])))
				{
					the_array[i] = strtof(entry_value, NULL);

				}
				else
				{
					// init this slot to default value
					the_array[i] = default_value;

					// no longer valid data in the file so we must stop reading them
					valid_data = false;

					SetReadError();
					CreateErrorMessage("invalid value for", entry_name);
				}
			}
			// if there are no valid data then init the array to default values
			else
			{
				the_array[i] = default_value;
			}
		}
	}

	return;
}


bool config_t::GetBooleanCVar(const char* cvar_name, bool default_value)
{
	if (fp != NULL)
	{
		// always start at the beginning of the file
		Rewind();

		// we don't need to check for scope so let's set something unrelated
		SetScope("meh_no_point");

		// store the CVar name in order to print proper console output
		SetCVarName(cvar_name);

		// try to find the key string...
		if (FindKey(cvar_name))
		{
			// and read its value
			return ReadBooleanValue(default_value);
		}
	}

	SetReadError();
	return default_value;
}


float config_t::GetFloatCVar(const char* cvar_name, float default_value, float range_from, float range_to, float unique_val)
{
	if (fp != NULL)
	{
		Rewind();
		SetScope("meh_no_point");
		SetCVarName(cvar_name);

		if (FindKey(cvar_name))
		{
			float val = ReadFloatValue(default_value);

			// first check whether the value equals this unique value
			// (it usually allows special functionality such as disabling/enabling)
			if (val == unique_val)
				return val;
			// check whether the value doesn't exceed given limits
			if ((val >= range_from) && (val <= range_to))
				return val;
			else
				CreateErrorMessage("invalid value for", cvar_name);
		}
	}

	SetReadError();
	return default_value;
}


int config_t::GetIntegerCVar(const char* cvar_name, int default_value, int range_from, int range_to)
{
	if (fp != NULL)
	{
		Rewind();
		SetScope("meh_no_point");
		SetCVarName(cvar_name);

		if (FindKey(cvar_name))
		{
			int val = ReadIntegerValue(default_value);

			if ((val >= range_from) && (val <= range_to))
				return val;
			else
				CreateErrorMessage("invalid value for", cvar_name);
		}
	}

	SetReadError();
	return default_value;
}


bool config_t::ReadTeam(char* the_team, const char* allowed_value1, const char* allowed_value2)
{
	the_team[0] = 0;

	if (ReadValueScope(the_team))
	{
		// check whether the team string is a valid team name
		if ((strcmp(the_team, allowed_value1) == 0) || (strcmp(the_team, allowed_value2) == 0))
			return true;
		else
		{
			CreateErrorMessage("invalid team value for", entry_name);
		}
	}

	SetReadError();
	return false;
}


bool config_t::ReadClass(char* the_class, int range_from, int range_to)
{
	int value = ReadIntegerValue(0);

	the_class[0] = 0;

	// check whether the class value is within given limits
	if ((value > 0) && (value >= range_from) && (value <= range_to))
	{
		// convert the class number back to string value
		sprintf(the_class, "%d", value);

		return true;
	}
	else
	{
		CreateErrorMessage("invalid class value for", entry_name);
	}

	SetReadError();
	return false;
}


bool config_t::ReadSkin(char* the_skin, int range_from, int range_to)
{
	int value = ReadIntegerValue(0);

	the_skin[0] = 0;

	if ((value > 0) && (value >= range_from) && (value <= range_to))
	{
		sprintf(the_skin, "%d", value);
		return true;
	}
	else
	{
		CreateErrorMessage("invalid skin value for", entry_name);
	}

	SetReadError();
	return false;
}


bool config_t::ReadSkill(char* the_skill, int range_from, int range_to)
{
	int value = ReadIntegerValue(0);

	the_skill[0] = 0;

	if ((value > 0) && (value >= range_from) && (value <= range_to))
	{
		sprintf(the_skill, "%d", value);
		return true;
	}
	else
	{
		CreateErrorMessage("invalid skill value for", entry_name);
	}

	SetReadError();
	return false;
}


/*
* looks up given class in the configuration file and reads and returns its entries one by one
* always rewinds the file
*/
bool config_t::ReadCustomClass(char* entry_string, int class_number)
{
	// is the configuration file open?
	if (fp != NULL)
	{
		char class_name[ceSize]{};
		static int read_entries_counter = 0;
		int current_entry_counter;

		class_name[0] = 0;
		entry_string[0] = 0;

		// we must always get back to the beginning of the configuration file
		// because we are breaking the reading cycle in the middle of the class
		// so the next reading would have not been able to find the class name
		Rewind();

		// convert class number to a string
		sprintf(class_name, "class%d", class_number);

		// get to the start of this class in the configuration file
		if (FindKey(class_name))
		{
			// set the next class as the end of the scope in order to read valid data
			sprintf(class_name, "class%d", class_number + 1);
			SetScope(class_name);

			current_entry_counter = 0;

			// keep reading the class entries and return them
			while (configFile.ReadEntryScope(entry_string))
			{
				// haven't we read this entry yet
				if (current_entry_counter == read_entries_counter)
				{
					// so increase the read entries counter
					// to prevent reading it again
					read_entries_counter++;

					// and return this entry
					return true;
				}
				// otherwise read next entry from the file
				else
					current_entry_counter++;
			}

			// let the user know if this class is empty
			if (read_entries_counter < 1)
			{
				CreateErrorMessage("missing entries for", entry_name);
				SetReadError();
			}

			// once we are done with the reading reset the static counter of read entries
			read_entries_counter = 0;
		}
	}

	entry_string[0] = 0;

	return false;
}


int config_t::CountTheCustomClasses(int class_to_check, int &empty_class_counter)
{
	// custom classes start from 1
	int count = 1;
	char the_entry[ceSize]{};
	char class_entries[ceSize]{};
	int class_entries_counter = 0;

	if (fp != NULL)
	{
		// always start at the beginning of the file
		Rewind();

		// do we need to just check whether current custom classes count still exist?
		// (ie. this configuration file has the same amount of custom classes as the previous configuration file)
		if (class_to_check > 0)
		{
			count = class_to_check;
		}

		// initialize the scope for first reading
		sprintf(the_entry, "class%d", count + 1);
		SetScope(the_entry);
		
		// prepare the entry name
		sprintf(the_entry, "class%d", count);

		// check whether this entry exist in the configuration file
		while (FindKeyScope(the_entry))
		{
			// prepare the next class for checking
			count++;
			sprintf(the_entry, "class%d", count);

			// use the next class as the end of the scope
			SetScope(the_entry);

			class_entries[0] = 0;
			class_entries_counter = 0;

			// keep reading the class entries and count them
			while (ReadEntryScope(class_entries))
			{
				class_entries_counter++;

				// actually we only need to know that there's at least one entry (ie. that this class isn't empty)
				if (class_entries_counter >= 1)
					break;
			}

			// this class has no entries so increase the empty class counter
			if (class_entries_counter < 1)
			{
				empty_class_counter++;
			}

			// we must always return to the beginning of the file before looking for the next class,
			// because the empty class detection may get us past the next class header
			Rewind();
		}

		// the checked class doesn't exist so we must count them all again
		if (class_to_check == count)
			CountTheCustomClasses(0, empty_class_counter);

		// now we must handle a case of a missing class header...

		Rewind();

		// see if we can find next class header
		sprintf(the_entry, "class%d", count + 1);
		if (FindKey(the_entry))
		{
			// next class header exists so we know that current one is missing and we have it stored in read errors history
		}
		else
		{
			Rewind();

			// this will handle a case when someone made bigger error and forgot to add 2 classes in sequence
			sprintf(the_entry, "class%d", count + 2);
			if (FindKey(the_entry))
			{
				// 3 or more missing classes in sequence aren't caught, but the chance to do such mistake is small and
				// if someone just erases several classes without reading the information in the config file then ... well his/her stupidity
			}
			// we reached the end of custom classes list, but doing so we got out of scope, because there's no new class header after the last class of course
			// so we must reset this particular error message here else we would fire false warning
			else
				ResetErrorMessage();
		}
	}

	// return the last existing class number
	// so in case there was none found or the configuration file wasn't open
	// we will correctly return zero amount of custom classes
	return count - 1;
}


/*
* stores the name of the entry that is currently processed
*/
void config_t::SetEntryName(const char* new_entry_name)
{
	entry_name[0] = 0;

	sprintf(entry_name, "%s", new_entry_name);
}


void config_t::ResetScope(void)
{
	scope_end[0] = 0;

	// there must be some string stored else we would get false result
	// when reading an empty line and comparing it to scope end
	sprintf(scope_end, "-empty-aka-no-scope-");
}


/*
* stores the name of the entry that stops the reading to prevent getting invalid data
*/
void config_t::SetScope(const char* string)
{
	// if the string isn't empty then set it
	if (string[0])
	{
		scope_end[0] = 0;

		sprintf(scope_end, "%s", string);
	}
	// otherwise just reset it to prevent false comparison results
	else
		ResetScope();
}


/*
* stores the name of the CVar entry that is currently processed for
* the purpose of printing the console output later on
* as this variable isn't reset and is kept available till it gets rewritten
*/
void config_t::SetCVarName(const char* new_cvar_name)
{
	cvar_name[0] = 0;

	sprintf(cvar_name, "%s", new_cvar_name);
}


void config_t::SetReadError(void)
{
	read_error = true;
	SetSectionError();
}


void config_t::CreateErrorMessage(char* message, const char* entry_name)
{
	// don't overwrite previous error message
	// (ie. always report only the first found error in the config file)
	if (IsErrorMessage())
		return;

	sprintf(er_msg, "%s \"%s\"\n", message, entry_name);
}
