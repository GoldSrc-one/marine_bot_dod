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
// bot_client.cpp
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
#include "bot_client.h"
#include "bot_weapons.h"

// types of damage to ignore
#define IGNORE_DAMAGE (DMG_CRUSH | DMG_FREEZE | DMG_FALL | DMG_SHOCK | DMG_NERVEGAS | DMG_RADIATION | DMG_ACID | DMG_SLOWBURN | DMG_SLOWFREEZE | 0xFF000000)

bot_weapon_t weapon_defs[MAX_WEAPONS]; // array of weapon definitions

/*
* this message is sent when the Firearms VGUI menu is displayed
*/
void BotClient_VGUI(void *p, int bot_index)
{
	static int state = 0;   // current state machine state
	static int iValue1;		// the first byte is the most important value in vgui message

	// we need to process the whole message first
	if (state == 0)
	{
		state++;
		iValue1 = *(int *)p;	// store this away we need it
	}
	else if (state == 1)
	{
		state = 0;				// reset the counter


		// now process the important values...

		//is it a team select menu?
		if (iValue1 == 2)
			bots[bot_index].start_action = MSG_VGUI_TEAM_SELECT;
		// is it a class selection menu?
		else if ((iValue1 == 10) || (iValue1 == 11))
			bots[bot_index].start_action = MSG_VGUI_CLASS_SELECT_US;
		else if (iValue1 == 12)
			bots[bot_index].start_action = MSG_VGUI_CLASS_SELECT_BRITS;
		else if (iValue1 == 13)
			bots[bot_index].start_action = MSG_VGUI_CLASS_SELECT_AXIS;
		else if (iValue1 == 14)
			bots[bot_index].start_action = MSG_VGUI_CLASS_SELECT_AXIS_PARA;
	}
}

/*
* this message is sent when a client joins the game
* all of the weapons are sent with the weapon ID and information about what ammo is used
*/
void BotClient_WeaponList(void* p, int bot_index)
{
	static int state = 0;   // current state machine state
	static bot_weapon_t bot_weapon;

	if (state == 0)
	{
		state++;
		bot_weapon.iAmmo1 = *(int*)p;		// ammo index 1
	}
	else if (state == 1)
	{
		state++;
		bot_weapon.iAmmo1Max = *(int*)p;	// max ammo1
	}
	else if (state == 2)
	{
		state++;
		bot_weapon.iAmmo2 = *(int*)p;		// ammo index 2
	}
	else if (state == 3)
	{
		state++;
		bot_weapon.iAmmo2Max = *(int*)p;	// max ammo2
	}
	else if (state == 4)
	{
		state++;
		bot_weapon.iSlot = *(int*)p;		// slot for this weapon ???
	}
	else if (state == 5)
	{
		state++;
		bot_weapon.iPosition = *(int*)p;	// position in slot ???
	}
	else if (state == 6)
	{
		state++;
		bot_weapon.iId = *(int*)p;			// weapon ID
	}
	else if (state == 7)
	{
		state++;
		bot_weapon.iDunno = *(int*)p;		// ???
	}
	else if (state == 8)
	{
		state = 0;

		bot_weapon.iFlags = *(int*)p;		// looks like a base clip size, probably because of enfield special reloading (???)

		// store away this weapon with it's ammo information
		weapon_defs[bot_weapon.iId] = bot_weapon;
	}
}

/*
* this message is sent when a weapon is selected (either by the bot choosing a weapon or by the server auto assigning the bot a weapon)
*/
void BotClient_CurrentWeapon(void *p, int bot_index)
{
	static int state = 0;   // current state machine state
	static int iState;
	static int iId;
	static int iClip;

	if (state == 0)
	{
		state++;
		iState = *(int*)p;  // state of the current weapon (ie. currently selected/active/visible on screen)
	}
	else if (state == 1)
	{
		state++;
		iId = *(int*)p;  // weapon ID of current weapon
	}
	else if (state == 2)
	{
		state = 0;

		iClip = *(int*)p;	// current number of rounds in the clip

		if (iId <= 31)
		{
			if (bot_index != -1)
			{
				bots[bot_index].bot_weapons |= (1 << iId);  // set this weapon bit

				if (iState == 1)
				{
					if(bots[bot_index].current_weapon.iId <= 0 && !IsPrimary(iId)) {
						bots[bot_index].main_weapon = NO_VAL;
						bots[bot_index].SetWeaponStatus(WS_NOAMMOFORMAIN);

						if(IsHandgun(iId)) {
							bots[bot_index].UseWeapon(uWeapon::backup);
						}
						else {
							bots[bot_index].backup_weapon = NO_VAL;
							bots[bot_index].SetWeaponStatus(WS_NOAMMOFORBACKUP);
							bots[bot_index].UseWeapon(uWeapon::knife);
						}
					}

					bots[bot_index].current_weapon.isActive = iState;
					bots[bot_index].current_weapon.iId = iId;
					bots[bot_index].current_weapon.iClip = iClip;

					// update the ammo counts for this weapon
					bots[bot_index].current_weapon.iAmmo1 =	bots[bot_index].curr_rgAmmo[weapon_defs[iId].iAmmo1];
					bots[bot_index].current_weapon.iAmmo2 =	bots[bot_index].curr_rgAmmo[weapon_defs[iId].iAmmo2];

					if(bots[bot_index].main_weapon != iId && IsPrimary(iId)) {
						bots[bot_index].main_weapon = iId;
						bots[bot_index].UseWeapon(uWeapon::main);
					}
				}
			}
		}
	}
}

/*
* this message is sent whenever ammo ammounts are adjusted (up or down)
*/
void BotClient_AmmoX(void *p, int bot_index)
{
	static int state = 0;   // current state machine state
	static int index;
	static int ammount;
	int ammo_index;

	if (state == 0)
	{
		state++;
		index = *(int *)p;  // ammo index (for type of ammo)
	}
	else if (state == 1)
	{
		state = 0;

		ammount = *(int *)p;  // the ammount of ammo currently available

		bots[bot_index].curr_rgAmmo[index] = ammount;  // store it away

		ammo_index = bots[bot_index].current_weapon.iId;

		// update the ammo counts for this weapon
		bots[bot_index].current_weapon.iAmmo1 = bots[bot_index].curr_rgAmmo[weapon_defs[ammo_index].iAmmo1];
		bots[bot_index].current_weapon.iAmmo2 = bots[bot_index].curr_rgAmmo[weapon_defs[ammo_index].iAmmo2];
	}
}

/*
* this message is sent to assign ammo for mg34 weapon ... instead of standard AmmoX message ... a unique message for just one weapon! OMG! WHY???
*/
void BotClient_AmmoShort(void* p, int bot_index)
{
	static int state = 0;   // current state machine state
	static int index;
	static int ammount;
	int ammo_index;

	if (state == 0)
	{
		state++;
		index = *(int*)p;  // ammo index (for type of ammo)
	}
	else if (state == 1)
	{
		state = 0;

		ammount = *(int*)p;  // the ammount of ammo currently available

		bots[bot_index].curr_rgAmmo[index] = ammount;  // store it away

		ammo_index = bots[bot_index].current_weapon.iId;

		// update the ammo counts for this weapon
		bots[bot_index].current_weapon.iAmmo1 = bots[bot_index].curr_rgAmmo[weapon_defs[ammo_index].iAmmo1];
		bots[bot_index].current_weapon.iAmmo2 = bots[bot_index].curr_rgAmmo[weapon_defs[ammo_index].iAmmo2];
	}
}

/*
* this message is sent when the bot picks up some ammo (AmmoX messages are also sent so this message is probably not really necessary except it
* allows the HUD to draw pictures of ammo that have been picked up the bots don't really need pictures since they don't have any eyes anyway
*/
void BotClient_AmmoPickup(void *p, int bot_index)
{
	static int state = 0;   // current state machine state
	static int index;
	static int ammount;
	int ammo_index;

	if (state == 0)
	{
		state++;
		index = *(int *)p;
	}
	else if (state == 1)
	{
		state = 0;

		ammount = *(int *)p;

		bots[bot_index].curr_rgAmmo[index] = ammount;

		ammo_index = bots[bot_index].current_weapon.iId;

		// update the ammo counts for this weapon
		bots[bot_index].current_weapon.iAmmo1 =	bots[bot_index].curr_rgAmmo[weapon_defs[ammo_index].iAmmo1];
		bots[bot_index].current_weapon.iAmmo2 =	bots[bot_index].curr_rgAmmo[weapon_defs[ammo_index].iAmmo2];
	}
}

/*
* this message gets sent when the bot picks up a weapon
*/
void BotClient_WeaponPickup(void* p, int bot_index)
{
	int index;

	index = *(int*)p;

#ifdef DEBUG

	/*/
	if (botdebugger.IsDebugWeapons())
	{
		char dmsg[128]{};
		sprintf(dmsg, "WeaponPickup() -> index =<%d>\n", index);
		//util.DebugInFile(dmsg);
		ALERT(at_console, dmsg);
	}
	/**/

#endif // DEBUG

	
	// set this weapon bit to indicate that we are carrying this weapon
	bots[bot_index].bot_weapons |= (1 << index);

	/*/
	if (bots[bot_index].bot_weapons & (1 << fa_weapon_claymore))
	{
		if (bots[bot_index].claymore_slot == NO_VAL)
			bots[bot_index].claymore_slot = fa_weapon_claymore;
	}

	if (bots[bot_index].bot_weapons & (1 << fa_weapon_frag))
	{
		// assign it to the grenade slot only if there was no other grenade yet
		if (bots[bot_index].grenade_slot == NO_VAL)
			bots[bot_index].grenade_slot = fa_weapon_frag;

		// grenades are available since now
		bots[bot_index].SetGrenadesAvailable();

		// if the bot picked this grenade up while already in game then let him check for other grenades he may carry
		bots[bot_index].RemoveSubTask(ST_NOOTHERNADE);
	}

	if (bots[bot_index].bot_weapons & (1 << fa_weapon_concussion))
	{
		if (bots[bot_index].grenade_slot == NO_VAL)
			bots[bot_index].grenade_slot = fa_weapon_concussion;

		bots[bot_index].SetGrenadesAvailable();
		bots[bot_index].RemoveSubTask(ST_NOOTHERNADE);
	}

	if (bots[bot_index].bot_weapons & (1 << fa_weapon_flashbang))
	{
		if (bots[bot_index].grenade_slot == NO_VAL)
			bots[bot_index].grenade_slot = fa_weapon_flashbang;

		bots[bot_index].SetGrenadesAvailable();
		bots[bot_index].RemoveSubTask(ST_NOOTHERNADE);
	}

	if (bots[bot_index].bot_weapons & (1 << fa_weapon_stg24))
	{
		if (bots[bot_index].grenade_slot == NO_VAL)
			bots[bot_index].grenade_slot = fa_weapon_stg24;

		bots[bot_index].SetGrenadesAvailable();
		bots[bot_index].RemoveSubTask(ST_NOOTHERNADE);
	}
	/**/
}

/*
* this message gets sent when the bot picks up an item (like a battery or a healthkit)
*/
void BotClient_ItemPickup(void *p, int bot_index)
{
   // do nothing
}

/*
* this message gets sent when bots health changes
*/
void BotClient_Health(void *p, int bot_index)
{
	bots[bot_index].SetHealth(* (int*)p);  // health amount
}

/*
* this message gets sent when the bots are getting damaged
*/
void BotClient_Damage(void *p, int bot_index)
{
	static int state = 0;   // current state machine state
	static int damage_armor;
	static int damage_taken;
	static int damage_bits;  // type of damage being done
	static Vector damage_origin;

	if (state == 0)
	{
		state++;
		damage_armor = *(int *)p;
	}
	else if (state == 1)
	{
		state++;
		damage_taken = *(int *)p;
	}
	else if (state == 2)
	{
		state++;
		damage_bits = *(int *)p;
	}
	else if (state == 3)
	{
		state++;
		damage_origin.x = *(float *)p;
	}
	else if (state == 4)
	{
		state++;
		damage_origin.y = *(float *)p;
	}
	else if (state == 5)
	{
		state = 0;

		// ignore all types of damage if the bot is in "I'm ignoring all" mode
		if (botdebugger.IsIgnoreAll())
			return;

		damage_origin.z = *(float *)p;

		if ((damage_armor > 0) || (damage_taken > 0))
		{			
			// if bot fall off something and is hurt then update previous health value to prevent false bandage treatment and ignore this damage
			if (damage_bits & DMG_FALL)
			{
				bots[bot_index].UpdatePrevHealth();

				return;
			}

			// set the need of air to know that the bot is drowing
			if (damage_bits & DMG_DROWN)
			{
				bots[bot_index].SetNeed(NEED_AIR);

				// we also have to handle false bahaviour the bot doesn't bleed even if he's losing health and the engine is sending bleeding sound
				bots[bot_index].RemoveTask(TASK_BLEEDING);

				return;
			}

			// reset the need of air to know that the bot is no more drowing
			if (damage_bits & DMG_DROWNRECOVER)
			{
				bots[bot_index].RemoveNeed(NEED_AIR);

				return;
			}

			// ignore certain types of damage
			if (damage_bits & IGNORE_DAMAGE)
				return;

			edict_t *pDmgEnt = bots[bot_index].pEdict->v.dmg_inflictor;

			if (pDmgEnt != NULL)
			{
				// is the bot being hurt by this trigger then ignore it for now prevents the bot to "dance" on such object
				//
				// NOTE: This should be changed to allow the bot to evade such object
				if (util.IsEntityName(pDmgEnt, "trigger_hurt"))
					return;

				// react only on damage taken from another player's gunfire ... at least for now
				if (util.IsEntityName(pDmgEnt, "player"))
				{
					// try to warn the teammate who's shooting at you
					if (util.AreTeammates(bots[bot_index].pEdict->v.dmg_inflictor, bots[bot_index].pEdict) && (RANDOM_LONG(1, 100) <= 35) && util.IsAlive(bots[bot_index].pEdict))
					{
						bots[bot_index].SetSubTask(ST_SAY_CEASEFIRE);
					}
				}
			}

			float curr_enemy_dist, new_enemy_dist;

			curr_enemy_dist = new_enemy_dist = 9999.0f;

			// get the vector to new enemy
			Vector v_new_enemy = damage_origin - bots[bot_index].pEdict->v.origin;

			// get the distance to new enemy
			new_enemy_dist = v_new_enemy.Length();

			// bot already has an enemy so...
			if (bots[bot_index].pBotEnemy)
			{
				// get the distance of current enemy
				curr_enemy_dist = (bots[bot_index].pBotEnemy->v.origin - bots[bot_index].pEdict->v.origin).Length();

				// is current enemy closer than the new enemy AND can bot see him? then ignore new enemy
				if ((curr_enemy_dist < new_enemy_dist) && bots[bot_index].IsNotWaitingForEnemy())
					return;

				// the new enemy must be closer than current enemy so...

				// is bot a sniper AND the new enemy is too close? then keep the current enemy
				// (ie. bot cannot switch to backup weapon/knife that fast so it's better to do as much damage to current enemy as he can)
				if (bots[bot_index].IsBehaviour(SNIPER) && (new_enemy_dist < RANGE_MELEE) && bots[bot_index].IsNotWaitingForEnemy())
					return;

				// if bot has clear view at current enemy AND he probably already shoot at current enemy than in most of the time keep current enemy
				if (bots[bot_index].IsNotWaitingForEnemy() && (bots[bot_index].pBotEnemy->v.health < 50.0f) && (RANDOM_LONG(1, 100) <= 90))
					return;

				// the bot decided to target the new enemy so forget current one
				bots[bot_index].BotForgetEnemy();
			}

			// it's a completely new enemy or bot switched to this enemy so...

			if (bots[bot_index].IsTask(TASK_GOALITEM) && (bots[bot_index].IsSubTask(ST_GOALITEM_BOMB) == false) && (new_enemy_dist > ENEMY_DIST_GOALITEM))
				return;
			
			// face the attacker (ie. the new enemy)
			Vector bot_angles = UTIL_VecToAngles(v_new_enemy);
			bots[bot_index].pEdict->v.ideal_yaw = bot_angles.y;

			BotFixIdealYaw(bots[bot_index].pEdict);

			bots[bot_index].SetSubTask(ST_FACEENEMY);

			// we need to know if bot turned more than a few degree to a side
			// if so then we have to trace that direction immediatelly to see whether there's danger of death fall
			if (bots[bot_index].IsTask(TASK_DEATHFALL) && (bots[bot_index].IsDontMoveTime() == false))
			{
				bots[bot_index].SetMoveSpeed(MoveSpeed::stop);
				bots[bot_index].SetDontMoveTime(1.0f);


				
#ifdef _DEBUG
				//@@@@@@@@@@@@@@@@@@@@@@@@@@@
				if (botdebugger.IsDebugActions() || botdebugger.IsDebugWeapons())
					ALERT(at_console, "BEEN HIT during DEATH FALL danger -->>> STOP\n");
#endif
			}

			// don't look for waypoint while searching the enemy
			bots[bot_index].SetDontLookForWaypoint(0.4f);			// was 0.2

			// if not already bandaging then prevent starting it right away (ie. by resetting it to current time the bot won't be able to start
			// applying bandages in next frame due to the check in bot.cpp so the bot will have time to search for the attacker)
			if (bots[bot_index].IsBandagingAllowed() && (bots[bot_index].IsBandagingNow() == false))
				bots[bot_index].SetBandageTime(-0.2f);
			
			// break waypoint action
			// if the bot is using the TANK (mounted gun) for unlimited period of time then he won't stop it by clearing action time, because he's not using action time in this case
			bots[bot_index].SetActionTime(-0.2f);

#ifdef _DEBUG
			//@@@@@@@@@@@@@@@
			//char dm[256]{};
			//sprintf(dm, "(bot_client.cpp) %s Been damaged by something -> trying to face it\n", bots[bot_index].name);
			//ALERT(at_console, dm); 
			//util.DebugInFile(dm);
#endif


		}
	}
}

/*
* this message gets sent when the bots get killed
*/
void BotClient_DeathMsg(void *p, int bot_index)
{
	/*											NOT USED

	static int state = 0;   // current state machine state
	static int killer_index;
	static int victim_index;
	static edict_t *victim_edict;
	static int index;

	if (state == 0)
	{
		state++;
		killer_index = *(int *)p;  // ENTINDEX() of killer
	}
	else if (state == 1)
	{
		state++;
		victim_index = *(int *)p;  // ENTINDEX() of victim
	}
	else if (state == 2)
	{
		state = 0;

		victim_edict = INDEXENT(victim_index);

		index = UTIL_GetBotIndex(victim_edict);

		// is this message about a bot being killed?
		if (index != -1)
		{
			if ((killer_index == 0) || (killer_index == victim_index))
			{
				// bot killed by world (worldspawn) or bot killed self
				bots[index].killer_edict = NULL;
			}
			else
			{
				// store edict of player that killed this bot
				bots[index].killer_edict = INDEXENT(killer_index);
			}
		}
	}

	*/
}

/*
* this message gets sent when any text is printed on the client's screen
* like changing to gl attachment and back or player's promotion to Sergeant etc.
*/
void BotClient_TextMsg(void *p, int bot_index)
{
	static int state = 0;   // current state machine state

	// this allows us to deal with variable amount of values/lines that get sent for this message type in DoD
	if (internals.IsNullEngineTextMsgState())
	{
		internals.ResetNullEngineTextMsgState();

		state = 0;
	}

	if (state == 0)
		state++;

	else if (state == 1)
	{
		state++;

		int pos;
		char the_text[256];

		// get the message and its length engine sends
		strcpy(the_text, (char *)p);


#ifdef _DEBUG
		//@@@@@@@@@@@@@@@@@
		ALERT(at_console, "text msg -> <%s>\n", the_text);
#endif

		// here we remove possible newline character from its end
		pos = strlen(the_text);
		if ((pos > 0) && (the_text[pos - 1] == '\n'))
		{
			the_text[pos - 1] = 0;
			pos--;
		}

		// check for uneven team message
		if (strcmp(the_text, "#game_uneven_teams") == 0)
		{
#ifdef _DEBUG
			//@@@@@@@@@@@@@@@@@
			ALERT(at_console, "!!!UNEVEN TEAMS!!!\n");
#endif

			// tell the bot to run the bot start game function again
			bots[bot_index].SetBotFlag(BF_RESPAWN_TRY_IT_AGAIN);
		}
		// check for class limit message
		else if (strcmp(the_text, "#game_class_limit") == 0)
		{
#ifdef _DEBUG
			//@@@@@@@@@@@@@@@@@
			ALERT(at_console, "!!!CLASS LIMIT!!!\n");
#endif

			bots[bot_index].SetBotFlag(BF_RESPAWN_TRY_IT_AGAIN);
			// also reset current class in order to generate a new one
			bots[bot_index].SetBotClass(NO_VAL);
		}

		/*/
		// prevents going prone for some time
		if ((strcmp(the_text, "Not enough room to go prone here!") == 0) || (strcmp(the_text, "Cannot prone on another player!") == 0))
		{
			bots[bot_index].SetSubTask(ST_CANTPRONE);
		}
		/**/
	}
}

/*
* this message gets sent when bots FOV (Field Of View) changes for example when the bot switches zoom level on sniper rifle etc.
*/
void BotClient_FOV(void* p, int bot_index)
{
	if (*(int*)p == ZOOM_NO)
	{
		bots[bot_index].DeactivateWeaponSecondaryMode();

		if (util.IsOldVersion())
			bots[bot_index].pEdict->v.fov = ZOOM_NO;
	}
	else if (*(int*)p == ZOOM_1X)
	{
		bots[bot_index].ActivateWeaponSecondaryMode();

		if (util.IsOldVersion())
			bots[bot_index].pEdict->v.fov = ZOOM_1X;
	}
	else if (*(int*)p == 0)
	{
		bots[bot_index].DeactivateWeaponSecondaryMode();

		if (util.IsOldVersion())
			bots[bot_index].pEdict->v.fov = ZOOM_NO;
	}
}

void BotClient_ScreenFade(void *p, int bot_index)
{
	static int state = 0;   // current state machine state
	static int duration;
	static int hold_time;
	static int fade_flags;
	int length;

	if (state == 0)
	{
		state++;
		duration = *(int *)p;
	}
	else if (state == 1)
	{
		state++;
		hold_time = *(int *)p;
	}
	else if (state == 2)
	{
		state++;
		fade_flags = *(int *)p;
	}
	else if (state == 6)
	{
		state = 0;

		length = (duration + hold_time) / 4096;
		bots[bot_index].SetBlindedTime( (float) (length - 2) );
	}
	else
	{
		state++;
	}
}

void BotClient_MOTD(void* p, int bot_index)
{
	static int state = 0;   // current state machine state

	if (state == 0)
	{
		state++;
		int value = *(int*)p;

		if (value == 1)
		{
			bots[bot_index].start_action = MSG_VGUI_MOTD_WINDOW;
		}
	}
	else if (state == 1)
	{
		state = 0;
	}
}

void BotClient_Object(void* p, int bot_index)
{
	char the_string[128]{};
	// get the string the engine sents
	strcpy(the_string, (char*)p);

	// empty string gets sent when the goal item (carried object) is either used or dropped
	if (the_string[0] == 0)
	{
		bots[bot_index].RemoveTask(TASK_GOALITEM);
		// also clear the subtask in case bot just used the explosive charge
		bots[bot_index].RemoveSubTask(ST_GOALITEM_BOMB);
		// let's make the bot check the ammo in order to make him decide whether he will set a new need to seek for another explosives charge seeing he just used the one he had 
		bots[bot_index].SetTask(TASK_CHECKAMMO);

#ifdef DEBUG
		//@@@@@@@@@@@@@@@@
		//ALERT(at_console, "\n%s used or dropped the goalitem/explosives!!!\n", bots[bot_index].name);
#endif // DEBUG


	}
	// bot picked up a satchel/tnt charge and carries it now
	else if ((strstr(the_string, "satchel2") != NULL) || (strstr(the_string, "obj_tnt") != NULL))
	{
		bots[bot_index].SetTask(TASK_GOALITEM);
		bots[bot_index].SetSubTask(ST_GOALITEM_BOMB);
		bots[bot_index].RemoveNeed(NEED_EXLOSIVESCHARGE);


#ifdef DEBUG
		//@@@@@@@@@@@@@@@@
		//ALERT(at_console, "\n%s picked up the exlosives!!!\n", bots[bot_index].name);
#endif // DEBUG


	}
	// was some string sent, but it doesn't match either of the explosives names?
	else if (strlen(the_string) > 1)
	{
		// then bot must have picked up some object other than the explosives (eg. the battle plans on jagd)
		bots[bot_index].SetTask(TASK_GOALITEM);


#ifdef DEBUG
		//@@@@@@@@@@@@@
		//ALERT(at_console, "\n%s picked up some goal item!!!\n", bots[bot_index].name);
#endif // DEBUG


	}
}

void BotClient_ClientAreas(void* p, int bot_index)
{
	static int state = 0;   // current state machine state
	//static int value;

	if (state == 0)
	{
		state++;
	}
	else if (state == 1)
	{
		// first get the value that gets sent, because based on it we will know whether there's 3rd variable for this message or not
		int value = *(int*)p;

		// if the value is -1 then it's a initialization at map start and then there's the 3rd variable, a string, so we must handle it else the decoding would get corrupted
		if (value == -1)
		{
			state++;
		}
		// otherwise there are just two variables and this is the case that we need to process 
		else
		{
			state = 0;

			// bot is within the client area
			if (value == 1)
			{
				bots[bot_index].SetSubTask(ST_INAREA);
			}
			// left it
			else if (value == 0)
			{
				bots[bot_index].RemoveSubTask(ST_INAREA);
			}
		}
	}
	else if (state == 2)
	{
		state = 0;
	}
}


/*
* gets sent at the end of reloading, this is an empty message, no value at all, probably just marks that the weapon is fully usable again
*/
void BotClient_ReloadDone(void* p, int bot_index)
{
	// all we need to do here is reset the reload time, rest will be handled inside bot think function
	// actually we don't reset it instantly, but give the engine some time, just for sure
	bots[bot_index].SetWeaponReloadTime(0.5f);
}


/*
* gets sent when a client uses a hand signal
*/
void BotClient_HandSignal(void* p, int bot_index)
{
	static int state = 0;   // current state machine state
	static int client_index;

	if (state == 0)
	{
		state++;
		client_index = *(int*)p;
	}
	else if (state == 1)
	{
		state = 0;
		int signalId = *(int*)p;

		// get the pointer to the invoker of this signal
		edict_t* pInvoker = INDEXENT(client_index);

		// hold position
		if (signalId == 15)
		{
			for (int index = 0; index < MAX_CLIENTS; index++)
			{
				if (bots[index].is_used)	// is this slot used?
				{
					edict_t* pEdict = bots[index].pEdict;

					// skip "yourself"
					if (pEdict == pInvoker)
						continue;

					// is this bot NOT doing the waiting duties yet AND can see the signal?
					if ((bots[index].IsWaitTime() == false) && util.CanBotSeeThisHandSignal(&bots[index], pInvoker))
					{
						// gunners should try to utilize the bipod
						if (bots[index].IsBehaviour(MGUNNER))
						{
							//TODO: Check for standing bipod spot in vicinity to use it

							if (CanDeployBipod(pEdict))
								BotUseBipod(&bots[index], true, "HandSignalMSG|HoldPosition -> DEPLOY bipod for the waiting");
							else
							{
								if (RANDOM_LONG(1, 100) < (90 - (bots[index].GetBotSkill() * 5)))
									bots[index].SetStance(GOTO_PRONE, "HandSignalMSG|HoldPosition -> GOTO prone for the waiting");
							}
						}
						else
						{
							if (RANDOM_LONG(1, 100) > 50)
								bots[index].SetStance(GOTO_CROUCH, "HandSignalMSG|HoldPosition -> GOTO crouch for the waiting");
						}

						// set the wait time and make the bot hold the position even when he notices an enemy, also make him keep assigned stance while waiting
						bots[index].SetWaitTime(bots[index].GenerateHoldPositionTime());
						bots[index].SetTask(TASK_DONTMOVEINCOMBAT);

						bots[index].BotSpeak(voiceCmd::handsig_yes_sir);

						// stop following the team leader to be able to hold the position
						bots[index].pTeamLeader = NULL;
					}
				}
			}
		}
		// 
		else if (signalId == 0)//todo
		{
			
		}
	}
}




// Messages for all

void BotClient_InitObj(void* p, int bot_index)
{
	static int state = 0;   // current state machine state
	static int number_of_objects_to_read = -1;
	static int number_of_readings_to_do = -1;
	static int state_for_new_object_index_reading = -1;
	static int obj_index = 0;
	static int state_for_new_object_coord_x_reading = -1;
	static float coord_x = 0.0f;

	if (state == 0)
	{
		// get the number of map goal objects
		number_of_objects_to_read = *(int*)p;

		// if the list is empty (ie. the value is 0) then don't update the machine state counter, because there is nothing else in this message 
		if (number_of_objects_to_read > 0)
		{
			// otherwise start reading...
			state++;

			// each map goal object is defined by 9 values so compute the total amount of calls to be read
			number_of_readings_to_do = number_of_objects_to_read * 9;

			// also we have to init the other crucial static values that are needed for correct data reading
			state_for_new_object_index_reading = 2;
			state_for_new_object_coord_x_reading = 8;
		}

#ifdef DEBUG

		char dm[128]{};
		sprintf(dm, "InitObj msg called -> objsToRead <%d> | NumOfReadingsToDo <%d>\n", number_of_objects_to_read, number_of_readings_to_do);
		//util.DebugInFile(dm);

#endif // DEBUG



	}
	else if (state == state_for_new_object_index_reading)
	{
		state++;

		// get the index of this map goal object/control point
		obj_index = *(int*)p;

		// and set the value of the machine state for the next object/control point index reading
		state_for_new_object_index_reading += 9;
	}
	else if (state == state_for_new_object_coord_x_reading)
	{
		state++;

		// get the x-coordinate of this map goal object/control point
		coord_x = *(float*)p;
	}
	else if (state == state_for_new_object_coord_x_reading + 1)
	{
		// get the y-coordinate of this map goal object/control point
		float coord_y = *(float*)p;

		// and set the value of the machine state for the next object/control point x-coordinate reading
		state_for_new_object_coord_x_reading += 9;



#ifdef DEBUG

		char dm[128]{};
		sprintf(dm, "InitObj msg called -> objIndex <%d> | x-coord <%.1f> | y-coord <%.1f> | currState <%d>\n", obj_index, coord_x, coord_y, state);
		//util.DebugInFile(dm);

#endif // DEBUG



		for (int i = 0; i < MAX_CAPTUREPOINTS; i++)
		{
			// see whether we are on one of the maps that have bugged point index values
			if ((ControlPoints->GetPointOrigin(i).x == coord_x) && (ControlPoints->GetPointOrigin(i).y == coord_y) && (ControlPoints->GetPointObjListIndex(i) != obj_index))
			{
				// and fix the index value for this control point
				ControlPoints->SetPointObjListIndex(i, obj_index);

				// also find matching point name in the array of capture areas and fix its index too
				dodCaptureArea->SetPointObjListIndex(dodCaptureArea->FindPointByName(ControlPoints->GetPointLinkName(i)), obj_index);



#ifdef DEBUG

				char dm[128]{};
				sprintf(dm, "InitObj msg called -> !!!detected index mismatch!!! -> fixing it for objIndex <%d> | cPoint <%d> | dodCapArea <%d>\n",
					obj_index, ControlPoints->GetPointObjListIndex(i), dodCaptureArea->GetPointObjListIndex(dodCaptureArea->FindPointByName(ControlPoints->GetPointLinkName(i))));
				//util.DebugInFile(dm);

#endif // DEBUG



			}
		}

		// have we read all data yet? then do a reset
		if (state == number_of_readings_to_do)
		{
			state = 0;
			number_of_readings_to_do = -1;

#ifdef DEBUG

			char dm[128]{};
			sprintf(dm, "InitObj msg called -> all readings done -> reset -> currState <%d>\n", state);
			//util.DebugInFile(dm);

#endif // DEBUG
		}
		// otherwise continue reading
		else
			state++;
	}
	// just in case
	else if (state >= number_of_readings_to_do)
	{
		state = 0;
		number_of_readings_to_do = -1;

#ifdef DEBUG

		char dm[128]{};
		sprintf(dm, "InitObj msg called -> reset of the state machine (FAIL-SAFE) -> currState <%d>\n", state);
		util.DebugInFile(dm);

#endif // DEBUG


	}
	// otherwise keep increasing the state counter (ie. skip values we don't need)
	else
	{
		state++;
	}
}

void BotClient_SetObj(void* p, int bot_index)
{
	static int state = 0;   // current state machine state
	static int obj_index = 0;

	if (state == 0)
	{
		state++;
		obj_index = *(int*)p;	// matches the position on the list of objects at top left corner of the screen ... a zero based index
	}
	else if (state == 1)
	{
		state++;
		int obj_owned_by = *(int*)p;	// team value ... 0 = neither team, 1 = allies, 2 = axis

		// try to find this control point in the array of known points...
		int array_index = ControlPoints->FindPointByObjListIndex(obj_index);

		// and update the team that owns it
		ControlPoints->SetOwnedByTeam(array_index, obj_owned_by);

		// do the same even for capture areas
		array_index = dodCaptureArea->FindPointByObjListIndex(obj_index);
		dodCaptureArea->SetOwnedByTeam(array_index, obj_owned_by);
	}
	else if (state == 2)
	{
		state = 0;
	}
}

void BotClient_PlayersIn(void* p, int bot_index)
{
	static int state = 0;   // current state machine state
	static int obj_index = 0;
	static int team_id = teamNULL;

	if (state == 0)
	{
		state++;
		obj_index = *(int*)p;	// matches the position on the list of objects at top left corner of the screen ... a zero based index
	}
	else if (state == 1)
	{
		state++;
		team_id = *(int*)p;		// the players of this team are present in this capture area
	}
	else if (state == 2)
	{
		state++;
		int players_present = *(int*)p;		// how many of them is present

		// find this capture area in the array
		int array_index = dodCaptureArea->FindPointByObjListIndex(obj_index);

		// and update the players count for appropriate team
		if (team_id == teamONE.GetTeamId())
		{
			dodCaptureArea->SetTeamOnePlayersCurrPresent(array_index, players_present);
		}
		else if (team_id == teamTWO.GetTeamId())
		{
			dodCaptureArea->SetTeamTwoPlayersCurrPresent(array_index, players_present);
		}
		else if (team_id == 0)
		{
			// team 0 means neither team is within the area (all players left it) so we have to reset the amount of present players for both teams,
			// the number of players is zero too in this case, however this also happens when other team enters the area so players of both teams are present in it,
			// when this happens it will kind of bug the behaviour of bots within the area and they will probably leave the area then
			dodCaptureArea->SetTeamOnePlayersCurrPresent(array_index, players_present);
			dodCaptureArea->SetTeamTwoPlayersCurrPresent(array_index, players_present);
		}
	}
	else if (state == 3)
	{
		state = 0;
	}

}

void BotClient_RoundState(void* p, int bot_index)
{
	int value = *(int*)p;
	// note: value == 0 is probably round restart
	//		 value == 1 is probably round in progess (round start - can move/fire/"simply game" since this moment)
	//		 value == 2 dunno
	//		 value == 3 round won by allies
	//		 value == 4 round won by axis

	internals.SetRoundState(value);

	// round restarts so reset the Triggers game state
	if (value == 0)
	{
		// we have to prevent calling Client Print message due to using the debug waypoints in following function which would lead to crashing the game to OS desktop
		internals.SetOverrideClientPrint();

		ResetTriggersOnRoundEnd();

		internals.ResetOverrideClientPrint();
	}
	// the round has been won by one of the teams
	else if (value == 3 || value == 4)
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			// force the bots to call respawn functions, because DOD doesn't seem to kill them
			if (bots[i].is_used)
			{
				bots[i].SetBotFlag(BF_RESPAWN_AT_ROUND_END);
			}
		}
	}

#ifdef DEBUG

	//@@@@@@@@@@
	ALERT(at_console, "RoundState() called with value=%d\n", value);

#endif // DEBUG

}

void BotClient_CapMsg(void* p, int bot_index)
{
	static int state = 0;   // current state machine state
	static int client_ID = 0;
	static char point_name[64]{};

	// the ID of the client who captured the objective, probably used to get the netname of this client in order to display it on the screen
	// on listenserver the value == 1 is the user who started the game, so then the value == 2 and further are the bots who joined after him
	if (state == 0)
	{
		state++;
		client_ID = *(int*)p;
	}
	// the name of the point that has been captured
	else if (state == 1)
	{
		state++;
		strcpy(point_name, (char*)p);
	}
	// the team that captured it
	else if (state == 2)
	{
		state = 0;
		int team_ID = *(int*)p;

		// on map Charlie this capture message gets sent twice where one of the calls has client ID set to zero, it could be some sort of reset or whatever
		// but we don't want to process anything in such case
		if (client_ID > 0)
		{
			char the_team[16]{};
			char the_text[128]{};

			if (team_ID == teamONE.GetTeamId())
				strcpy(the_team, teamONE.GetTeamName());
			else if (team_ID == teamTWO.GetTeamId())
				strcpy(the_team, teamTWO.GetTeamName());

			// build the message using a set format so that we can check for a match with the triggers
			sprintf(the_text, "%s %s", point_name, the_team);

			// first we will update the triggers data ie. switch appropriate trigger waypoints on or off
			IsMatchingTriggerMessage(the_text);

			// then we have to make sure the waypoints data will be updated in next game frame, we cannot call the waypoint update function right here,
			// because that function starts a new engine message and the game would crash if you start a new message before ending this message
			internals.ResetUpdateWaypointDataTime();

			// we can't access the console here else the game would crash so using the internal message will allow printing the capture message to client console
			// in order to give the waypoint creator the data needed for adding the trigger messages
			if (internals.IsCheckTriggerCapMessage())
			{
				char capture_message[256]{};

				sprintf(capture_message, "\nThis text string was built based on this Capture Message: %s\n", the_text);
				internals.SetInternalMessage(capture_message);
			}



#ifdef DEBUG

			//@@@@@@@@@@
			ALERT(at_console, "CapMSG been called updating waypoint data i.e. path goal flags and the triggers\n");

#endif // DEBUG

		}
	}
}




/*
* this message gets sent when any HUD text (the fancy colored and bit transparent one) is printed on the client's screen
* like the messages mappers often send to clients eg. the countdown on obj maps (thanatos uses just these)
*/
void BotClient_FA_HUDMsg(void* p, int bot_index)
{
	/*/
	static int state = 0;   // current state machine state

	while (state < 18)
	{
		state++;

		// the message gets sent as the last thing
		if (state == 17)
		{
			// reset it back to start
			state = 0;

			char the_text[256];
			// get the message the engine sends
			strcpy(the_text, (char*)p);

			// check if the message matches any user-defined trigger event
			if (IsMatchingTriggerMessage(the_text))
			{
				// we will update waypoints as well just to "know" about the latest events
				internals.ResetUpdateWaypointDataTime();

			}

			// we have to break it here otherwise we would loop forever
			break;
		}
	}
	/**/
}
