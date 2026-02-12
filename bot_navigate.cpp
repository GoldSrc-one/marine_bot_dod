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
// bot_navigate.cpp
// 
////////////////////////////////////////////////////////////////////////////////////////////////

#include <string.h>

#pragma warning( disable: 4005 91 )

#include "extdll.h"
#include "util.h"
#include "cbase.h"

#pragma warning( default: 4005 91 )

#include "bot.h"
#include "bot_func.h"
#include "bot_manager.h"
#include "bot_weapons.h"
#include "console_output.h"
#include "waypoint.h"


// few function prototypes used in this file
void UpdateWptHistory(bot_t *pBot);
Vector GenerateOrigin(int wpt_index);
float ModifyWaypointWaitTimeByBotBehaviour(bot_t* pBot);
bool IsPathEndException(int current_wpt_index, int bot_team);
int ForwardPathMove(bot_t *pBot, W_PATH *p, int &path_next_wpt, bool &path_end);
int BackwardPathMove(bot_t *pBot, W_PATH *p, int &path_next_wpt, bool &path_end);
void AssignPathBasedBehaviour(bot_t *pBot, int wpt_index);
void AssignWaypointBasedBehaviourUponReachingIt(bot_t* pBot, bool &waypoint_found);
//int getNextWptInPath(bot_t *pBot);		// not used
bool TraceForward(bot_t *pBot, bool check_head, bool check_feet, int dist, TraceResult *tr);
bool TraceJumpUp(bot_t *pBot, int height, bool duckjump = false);


/*
* clears all waypoint and path related variables including various waypoint based actions
*/
void bot_t::ResetWaypointBasedNavigation(void)
{
	curr_wpt_index = NO_VAL;
	curr_wpt_fake_position = g_vecZero;
	prev_distance_to_curr_wpt = 9999.0f;
	RemoveBehaviour(BOT_PRECISION);
	time_to_reach_curr_wpt = 0.0f;
	time_to_face_waypoint = 0.0f;
	prev_wpt_index.clear();
	
	RemoveNeed(NEED_NEXTWPT);
	RemoveNeed(NEED_RESETNAVIG);
	RemoveTask(TASK_SPRINT);
	RemoveTask(TASK_WPTACTION);
	wpt_action_time = 0.0f;
	bot_wait_time = 0.0f;

	curr_path_index = NO_VAL;
	prev_path_index = NO_VAL;
	patrol_path_waypoint = NO_VAL;
	RemoveTask(TASK_OPPOSITEPATHDIR);
	RemoveTask(TASK_BACKTOPATROL);
	RemoveTask(TASK_AVOID_ENEMY);
	RemoveTask(TASK_IGNORE_ENEMY);
}


/*
* sets given waypoint as current waypoint to head towards to
* also generates its fake origin for human like movements
* and marks precise movement behaviour if needed
* then sets the time to reach this waypoint
* finally sets the time to face this waypoint if it isn't in FOV
*/
void bot_t::SetCurrentWaypoint(int wpt_index)
{
	curr_wpt_index = wpt_index;
	
	if (wpt_index != NO_VAL)
	{
		curr_wpt_fake_position = GenerateOrigin(wpt_index);

		if (waypoints[wpt_index].range < WPT_RANGE)
			SetBehaviour(BOT_PRECISION);
		else
			RemoveBehaviour(BOT_PRECISION);
	}
	
	time_to_reach_curr_wpt = gpGlobals->time;

	// set this time to prevent unwanted search for different waypoint while bot needs to do bigger turning at current waypoint (ie if new waypoint is NOT in his view cone)
	if (util.IsInViewCone(GetPointerToCurrWptPosition(), pEdict) == false)
		SetFaceWaypointTime(0.6f);


#ifdef _DEBUG
	//if (botdebugger.IsDebugStuck() || botdebugger.IsDebugPaths() || botdebugger.IsDebugWaypoints())
		//conOutput.Notify("(nav.cpp) SetCurrWpt() -> setting the time to reach this waypoint now!!!\n");
#endif
}


void bot_t::ClearCurrentWaypoint(void)
{
	curr_wpt_index = NO_VAL;
	curr_wpt_fake_position = g_vecZero;
	RemoveBehaviour(BOT_PRECISION);
	SetFaceWaypointTime(-1.0f);
}


/*
* returns the next waypoint index from a path the bot is using
* allows checking things outside standard path navigation
* not meant for navigation, but only for specific checks/cases
*/
int bot_t::GetNextWaypointOnPath(void)
{
	if ((curr_wpt_index != NO_VAL) && (curr_path_index != NO_VAL))
	{
		W_PATH* cur_w_path = w_paths[curr_path_index];

		while (cur_w_path)
		{
			if (cur_w_path->wpt_index == curr_wpt_index)
			{
				// the bot is using the path from start to end so return next
				if ((IsTask(TASK_OPPOSITEPATHDIR) == false) && (cur_w_path->next))
					return cur_w_path->next->wpt_index;

				// the bot is using the path from end to start so return previous
				if (IsTask(TASK_OPPOSITEPATHDIR) && (cur_w_path->prev))
					return cur_w_path->prev->wpt_index;
			}

			cur_w_path = cur_w_path->next;
		}
	}

	return NO_VAL;
}


/*
* checks whether bot is on two-way or patrol type path and changes the direction he moves on it
* returns false if it cannot be done (ie. bot is on one-way path)
* not used in standard navigation, but allows to handle special cases like when bot gets stuck somewhere
*/
bool bot_t::ReturnBackToPathStart(const char* loc)
{
	// is the bot already evading planted explosives charge? then don't allow him change path direction again otherwise he would run back into the area of explosion
	if (IsTask(TASK_CLAY_EVADE) && IsNeed(NEED_NEXTWPT))
		return false;

	// first see whether the waypoint the bot is currently by isn't the path start or end waypoint, because in such case bot cannot return even further "back" to path start since he's right there
	if (wptmanager.IsWaypointAtPathEnd(curr_wpt_index, curr_path_index))
	{
		// so try to find connected cross waypoint or search for any waypoint around ... in some weird case where this path start/end isn't connected to a cross waypoint
		// basically we are calling the same function that gets normally called when the bot reaches the end of his current path, however
		// standard navigation isn't able to handle it in this case so we have to do it manually
		int next_wpt = wptmanager.FindNewWaypointForBotAtPathEnd(this, curr_wpt_index);

		// is there any new waypoint in range?
		if (next_wpt != NO_VAL)
		{
			SetCurrentWaypoint(next_wpt);

			return true;
		}
	}
	else if (wptmanager.IsPath(curr_path_index, PathT::two_way, PathT::patrol_cycle))
	{
		// simply change the direction the bot moves on this path
		if (IsTask(TASK_OPPOSITEPATHDIR))
			RemoveTask(TASK_OPPOSITEPATHDIR);
		else
			SetTask(TASK_OPPOSITEPATHDIR);

		return true;
	}

	return false;
}


void bot_t::MakeRandomTurn(void)
{
	SetMoveSpeed(MoveSpeed::stop);		// don't move while turning

	if (RANDOM_LONG(1, 100) <= 10)
	{
		// 10 percent of the time turn completely around...
		pEdict->v.ideal_yaw += 180.0f;
	}
	else
	{
		// turn randomly between 30 and 60 degress
		if (wander_direction == SIDE_LEFT)
			pEdict->v.ideal_yaw += RANDOM_LONG(30, 60);
		else
			pEdict->v.ideal_yaw -= RANDOM_LONG(30, 60);
	}

	BotFixIdealYaw(pEdict);
}


/*
* returns TRUE if the bot is close to a waypoint with small range (ie. range < 20)
*/
bool bot_t::IsInCrampedSpace(void)
{
	// current waypoint range is small AND the bot is no more than 75 units away from this waypoint
	if ((waypoints[curr_wpt_index].range < WPT_RANGE_SMALL) && (wptmanager.GetDistanceToWaypoint(pEdict, curr_wpt_index) < (float) (WPT_RANGE * 1.5f)))
		return true;

	// previous waypoint had small range AND the bot is currently heading to a cross or goback waypoint
	if ((waypoints[prev_wpt_index.get()].range < WPT_RANGE_SMALL) && wptmanager.IsWaypoint(curr_wpt_index, WptT::cross, WptT::goback))
		return true;

	return false;
}

void bot_t::IncWaypointPenalty(float penalty) {
	if(GetCurrentWaypoint() != NO_VAL)
		waypoint_penalty[GetCurrentWaypoint()] += penalty;
}

void BotFixIdealPitch(edict_t *pEdict)
{
	// check for wrap around of angle
	if (pEdict->v.idealpitch > 180)
		pEdict->v.idealpitch -= 360;

	if (pEdict->v.idealpitch < -180)
		pEdict->v.idealpitch += 360;
}


float BotChangePitch( bot_t *pBot, float speed )
{
	edict_t *pEdict = pBot->pEdict;
	float ideal;
	float current;
	float current_180;  // current +/- 180 degrees
	float diff;

	float bipod_top_limit = 20.0;
	float bipod_bottom_limit = 45.0;

	// turn from the current v_angle pitch to the idealpitch by selecting the quickest way to turn to face that direction

	current = pEdict->v.v_angle.x;
	
	ideal = pEdict->v.idealpitch;

	// is bot using bipod so limit his turn angles
	if (pBot->IsTask(TASK_BIPOD))
	{
		// in pEdict->v.vuser1 is stored v_view angle when player started using bipod (ie the direction player faced when he used "bipod" command)

		// is current angle bigger than top limit angle (while the bot is looking upwards) OR is current angle bigger than bottom limit angle (while the bot is looking downwards)
		if (((fabs(current) > bipod_top_limit) && (current < 0)) || ((fabs(current) > bipod_bottom_limit) && (current > 0)))
			current = pEdict->v.vuser1.x;	// reset to original value
	}

	// find the difference in the current and ideal angle
	diff = fabsf(current - ideal);

	// check if the bot is already facing the idealpitch direction
	if (diff <= 1.0f)
		return diff;  // return number of degrees turned

	// check if difference is less than the max degrees per turn
	if (diff < speed)
		speed = diff;  // just need to turn a little bit (less than max)

	// we should make some difference between bots if the bot is trying to face an enemy ie. we need to slow down worse bots more than better bots
	if (pBot->IsSubTask(ST_FACEENEMY))
	{
		float speed_tweak = 0.0f;

		// so we just take some constant and multiply it by the skill which means no difference for best bots because skill is zero based variable
		speed_tweak = 4.0f * (float) pBot->GetBotSkill();

		// and then we can modify the turning speed
		speed = speed - speed_tweak;

		if (speed <= 0.0f)
			speed = 1.0f;
	}

	// here we have four cases, both angle positive, one positive and the other negative, one negative and the other positive, or both negative.  handle each case separately

	if ((current >= 0.0f) && (ideal >= 0.0f))  // both positive
	{
		if (current > ideal)
			current -= speed;
		else
			current += speed;
	}
	else if ((current >= 0.0f) && (ideal < 0.0f))
	{
		current_180 = current - 180.0f;

		if (current_180 > ideal)
			current += speed;
		else
			current -= speed;
	}
	else if ((current < 0.0f) && (ideal >= 0.0f))
	{
		current_180 = current + 180.0f;
		if (current_180 > ideal)
			current += speed;
		else
			current -= speed;
	}
	else  // (current < 0) && (ideal < 0)  both negative
	{
		if (current > ideal)
			current -= speed;
		else
			current += speed;
	}

	// check for wrap around of angle
	if (current > 180.0f)
		current -= 360.0f;
	if (current < -180.0f)
		current += 360.0f;

	pEdict->v.v_angle.x = current;

	return speed;  // return number of degrees turned
}


void BotFixIdealYaw(edict_t *pEdict)
{
	// check for wrap around of angle
	if (pEdict->v.ideal_yaw > 180.0f)
		pEdict->v.ideal_yaw -= 360.0f;

	if (pEdict->v.ideal_yaw < -180.0f)
		pEdict->v.ideal_yaw += 360.0f;
}


float BotChangeYaw( bot_t *pBot, float speed )
{
	edict_t *pEdict = pBot->pEdict;
	float ideal;
	float current;
	float current_180;  // current +/- 180 degrees
	float diff;

	// turn from the current v_angle yaw to the ideal_yaw by selecting the quickest way to turn to face that direction

	current = pEdict->v.v_angle.y;

	ideal = pEdict->v.ideal_yaw;

	if (pBot->IsTask(TASK_BIPOD))
	{
		float bipod_limit = 45.0f;

		// is bot trying to turn more than bipod allows
		if (fabsf(pBot->GetBipodYawAngle() - current) > bipod_limit)
		{
			current = pBot->GetBipodYawAngle();
		}
	}

	if (pBot->IsSubTask(ST_FACEENEMY))
	{
		float speed_tweak = 0.0f;

		speed_tweak = 4.0f * (float) pBot->GetBotSkill();

		speed = speed - speed_tweak;

		if (speed <= 0.0f)
			speed = 1.0f;
	}

	diff = fabsf(current - ideal);

	// the bot is basically facing the direction we wanted so we can remove the flag
	if (diff < 4.0f)
	{
		// we do it only here, because yaw is the more important part in making the bot reactions human like
		pBot->RemoveSubTask(ST_FACEENEMY);
	}

	if (diff <= 1.0f)
	{
		return diff;
	}

	if (diff < speed)
		speed = diff;

	if ((current >= 0.0f) && (ideal >= 0.0f))
	{
		if (current > ideal)
			current -= speed;
		else
			current += speed;
	}
	else if ((current >= 0.0f) && (ideal < 0.0f))
	{
		current_180 = current - 180.0f;

		if (current_180 > ideal)
			current += speed;
		else
			current -= speed;
	}
	else if ((current < 0.0f) && (ideal >= 0.0f))
	{
		current_180 = current + 180.0f;
		if (current_180 > ideal)
			current += speed;
		else
			current -= speed;
	}
	else  // (current < 0) && (ideal < 0)  both negative
	{
		if (current > ideal)
			current -= speed;
		else
			current += speed;
	}
	
	if (current > 180.0f)
		current -= 360.0f;
	if (current < -180.0f)
		current += 360.0f;

	pEdict->v.v_angle.y = current;

	return speed;  // return number of degrees turned
}


/*
* remove oldest history record and move all others by one (to make free slot for new wpt)
*/
void UpdateWptHistory(bot_t *pBot)
{
	// don't update history if nothing changed
	if (pBot->prev_wpt_index.get() == pBot->curr_wpt_index)
	{
		return;
	}

	pBot->prev_wpt_index.push(pBot->curr_wpt_index);
}


/*
* creates fake waypoint origin somewhere inside its range
* to make bots moves more random (human like)
* real waypoint origin stays untouched
* we are working only in planar dimension (ie ingoring z-coord)
*/
Vector GenerateOrigin(int wpt_index)
{
	// if really small range OR the waypoint is cross OR ladder OR door waypoint use real origin
	if ((waypoints[wpt_index].range <= 5.0f) ||//		was 15
		(waypoints[wpt_index].flags & (W_FL_CROSS | W_FL_LADDER | W_FL_DOOR | W_FL_DOORUSE)))
	{
		return waypoints[wpt_index].origin;
	}
	
	float d_x, d_y, wpt_range;
	Vector new_origin = waypoints[wpt_index].origin;

	// get the range out of this wpt
	wpt_range = waypoints[wpt_index].range;

	// make the new x,y position
	d_x = RANDOM_FLOAT(1, wpt_range - 1.0f);
	d_y = RANDOM_FLOAT(1, wpt_range - 1.0f);

	// in 50% of time use also negative values for fake position
	if (RANDOM_LONG(1, 100) < 50)
		d_x *= -1.0f;

	if (RANDOM_LONG(1, 100) < 50)
		d_y *= -1.0f;

	// change waypoint x and y value
	new_origin.x += d_x;
	new_origin.y += d_y;

	// return the new fake origin
	return new_origin;
}

/*
* modifies the wait time for bots with attacker and defender behaviour
* attackers will get reduced wait time while defenders will get increased wait time
* waypoints with priority == 1 will NOT be modified as they serve as goal waypoints
* so the exact wait time set on such waypoint is used then
*/
float ModifyWaypointWaitTimeByBotBehaviour(bot_t* pBot)
{
	// first get waypoint wait time
	float original_wait_time = wptmanager.GetWaypointWaitTime(pBot->curr_wpt_index, pBot->GetBotTeam());

	// modify the wait time based on bot behaviour only for waypoints with priority lower than highest one, because waypoints with highest priority and wait time have a special purpose 
	// (i.e. a goal waypoints where we must use the exact time that is set on the waypoint, otherwise things may not work correctly - for example the explosives on obj_bocage
	// that need specific time value to appear and become active)
	if (wptmanager.GetWaypointPriority(pBot->curr_wpt_index, pBot->GetBotTeam()) != 1)
	{
		if (pBot->IsBehaviour(ATTACKER))
		{
			original_wait_time *= 0.65f;
		}
		else if (pBot->IsBehaviour(DEFENDER))
		{
			original_wait_time *= 1.35f;
		}
	}

	return original_wait_time;
}


/*
* returns true if the waypoint is an ammobox or use waypoint AND isn't disabled by zero priority
*/
bool IsPathEndException(int current_wpt_index, int bot_team)
{
	if (wptmanager.IsWaypoint(current_wpt_index, WptT::ammobox, WptT::use) && (wptmanager.GetWaypointPriority(current_wpt_index, bot_team) != 0))
		return true;

	return false;
}


/* kota@
* this is the standard path move from Bot OnPath()
* I move it into separate function for easy understanding and manipulation
* Function set path_next_wpt, path_end.
* Function return status like Bot OnPath().
* 0 - need continue run Bot OnPath()
* 1 - return Bot OnPath() with the same status.
*/
int ForwardPathMove(bot_t *pBot, W_PATH *p, int &path_next_wpt, bool &path_end)
{
	// this is standard path move (start -> end)
	

	// the path ends AND it's a one way path
	if ((p->next == NULL) && wptmanager.IsPath(pBot->curr_path_index, PathT::one_way))
	{
		// search for waypoints around
		int next_wpt = wptmanager.FindNewWaypointForBotAtPathEnd(pBot, p->wpt_index);

		// is there any new waypoint in range?
		if (next_wpt != NO_VAL)
		{
			pBot->SetCurrentWaypoint(next_wpt);

			if (botdebugger.IsDebugPaths())
			{
				char fpmsg[128]{};
				sprintf(fpmsg, "\n***Reached the end of one-way path! - path #%d | curr_wpt #%d | dir is (start->end) | next wpt #%d\n\n", pBot->curr_path_index + 1, p->wpt_index + 1, pBot->curr_wpt_index + 1);
				conOutput.Notify(fpmsg);
			}

			return 1;
		}
		// otherwise bot reached the end of one-way path and didn't find any waypoint
		else
		{
			// set some wait time
			pBot->SetWaitTime(RANDOM_FLOAT(30.0f, 120.0f));

			// bot still has a waypoint
			pBot->SetTimeToReachCurrWaypoint();

			if (botdebugger.IsDebugPaths())
			{
				char fpemsg[128]{};
				sprintf(fpemsg, "\n\n<<BUG IN WAYPOINTS>>Reached the end of one-way path, but didn't find wpt! - path #%d | last_wpt #%d\n\n\n", pBot->curr_path_index + 1, p->wpt_index + 1);
				conOutput.Notify(fpemsg);
			}

			return -1;
		}
	}
	// the path ends AND it's a two way path
	else if ((p->next == NULL) && wptmanager.IsPath(pBot->curr_path_index, PathT::two_way))
	{
		// does the path end on goback waypoint?
		if (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::goback) && (wptmanager.GetWaypointPriority(pBot->curr_wpt_index, pBot->GetBotTeam()) != 0))
		{
			if (p->prev != NULL)
			{
				// change the direction the bot had
				pBot->SetTask(TASK_OPPOSITEPATHDIR);
					
				// set next waypoint - the previous one in llist
				path_next_wpt = p->prev->wpt_index;
			}
// REMOVE THIS - ONLY TESTING
#ifdef _DEBUG
			else
			{
				ALERT(at_console, "(<<OBSERVE INFO>>)What's the bot doing when this happens?\n");
				util.DebugDev("start->end | two-way | wpt==goback | prev == NULL", pBot->curr_wpt_index, pBot->curr_path_index);
			}
#endif
		}
		// otherwise try to find any new waypoint and path
		else
		{
			int next_wpt = NO_VAL;
			
			// does this path end on ammobox or use waypoint?
			if (IsPathEndException(p->wpt_index, pBot->GetBotTeam()))
			{
				// then look only for connected cross waypoints because if the path ends on ammobox or use waypoint then the bot will leave this path only if this path end is connected to a cross waypoint
				next_wpt = wptmanager.FindConnectedCross(p->wpt_index);
			}
			// otherwise look for any accessible waypoint in the vicinity
			else
				next_wpt = wptmanager.FindNewWaypointForBotAtPathEnd(pBot, p->wpt_index);

			// did we find any waypoint around?
			if (next_wpt != NO_VAL)
			{
				// set the found waypoint as the new waypoint to head towards
				pBot->SetCurrentWaypoint(next_wpt);

				if (botdebugger.IsDebugPaths())
				{
					char fpmsg[128]{};
					sprintf(fpmsg, "\n***Reached the end of path! - path #%d | curr_wpt #%d | dir is (start->end) | next wpt #%d\n\n", pBot->curr_path_index + 1, p->wpt_index + 1, pBot->curr_wpt_index + 1);
					conOutput.Notify(fpmsg);
				}

				return 1;
			}
			// otherwise wait there for a while and then turn back
			else
			{
				// change the direction the bot will move on this path and set previous waypoint as the next waypoint to head to
				if (p->prev != NULL)
				{
					pBot->SetTask(TASK_OPPOSITEPATHDIR);
					path_next_wpt = p->prev->wpt_index;
				}

				// if the path ends on either ammobox or use waypoint then we won't wait here, because these two can automatically work as a turnback waypoint when they are used at the end of the path
				if (IsPathEndException(pBot->curr_wpt_index, pBot->GetBotTeam()))
				{
					return 0;
				}

				// we set some wait time in order to stay at this waypoint before turning back
				pBot->SetWaitTime(RANDOM_FLOAT(15.0f, 60.0f));

				// report this as a bug
				if (botdebugger.IsDebugPaths())
				{
					char fpemsg[128]{};
					sprintf(fpemsg, "\n\n<<BUG IN WAYPOINTS>>Reached the end of this path, but didn't find next wpt! - path #%d | curr_wpt #%d\n\n\n", pBot->curr_path_index + 1, p->wpt_index + 1);
					conOutput.Notify(fpemsg);
				}

				return 1;
			}
		}	
	}
	// the path ends AND it's a patrol path
	else if ((p->next == NULL) && wptmanager.IsPath(pBot->curr_path_index, PathT::patrol_cycle))
	{
		// this statement must be here else we may eventually leave the path through a goback waypoint which would be a bug
		if (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::goback) && (wptmanager.GetWaypointPriority(pBot->curr_wpt_index, pBot->GetBotTeam()) != 0))
			;
		// otherwise check if there is a chance to leave this path
		else if (RANDOM_LONG(1, 100) < 15)
		{
			int next_wpt = wptmanager.FindNewWaypointForBotAtPathEnd(pBot, p->wpt_index);

			if (next_wpt != NO_VAL)
			{
				pBot->SetCurrentWaypoint(next_wpt);

				if (botdebugger.IsDebugPaths())
				{
					char ppmsg[128];
					sprintf(ppmsg, "\n***Reached patrol path end! - path #%d | curr_wpt #%d | dir is (start->end) | next wpt #%d\n\n", pBot->curr_path_index + 1, p->wpt_index + 1, pBot->curr_wpt_index + 1);
					conOutput.Notify(ppmsg);
				}

				return 1;
			}
		}

		// all we need to do in order to keep the bot patrolling is that we simply turn the bot back to continue on this path like if there was always a goback waypoint at the end of the path
		pBot->SetTask(TASK_OPPOSITEPATHDIR);
		path_next_wpt = p->prev->wpt_index;
	}
	// otherwise get next waypoint to continue to
	else
	{
		if (p->next != NULL)
		{
			path_next_wpt = p->next->wpt_index;

			// we didn't reach the end yet
			path_end = FALSE;
		}
	}

	return 0;
}


/* kota@
* this is the standart path move from Bot OnPath().
* I move it into separate function for easy understanding and manipulation
* Function set path_next_wpt, path_end.
* Function return status like Bot OnPath().
* 0 - need continue run Bot OnPath()
* 1 - return Bot OnPath() with the same status.
* -1 - error , return Bot OnPath() with the same status.
*/
int BackwardPathMove(bot_t *pBot, W_PATH *p, int &path_next_wpt, bool &path_end)
{
	// if the bot gets to one-way path
	if (wptmanager.IsPath(pBot->curr_path_index, PathT::one_way))
	{
#ifdef _DEBUG
		ALERT(at_console, "***DAMN there is a problem: Get on One-way path #%d in opposite direction (RESET)\n", pBot->curr_path_index + 1);

		util.DebugDev("ERROR (Bad wpts): Bot entered One-way path from opposite direction!!!", pBot->curr_wpt_index, pBot->curr_path_index);
#endif

		pBot->RemoveTask(TASK_OPPOSITEPATHDIR);

		return -1;
	}

	// have we reached the start of two way path?
	if ((p->prev == NULL) && wptmanager.IsPath(pBot->curr_path_index, PathT::two_way))
	{
		// if it is a goback waypoint then turn the bot back so that he stays on this path
		if (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::goback) && (wptmanager.GetWaypointPriority(pBot->curr_wpt_index, pBot->GetBotTeam()) != 0))
		{
			if (p->next != NULL)
			{
				pBot->RemoveTask(TASK_OPPOSITEPATHDIR);
				path_next_wpt = p->next->wpt_index;
			}
		}
		// otherwise try to find any new waypoint and path
		else
		{
			int next_wpt = NO_VAL;

			if (IsPathEndException(p->wpt_index, pBot->GetBotTeam()))
				next_wpt = wptmanager.FindConnectedCross(p->wpt_index);
			else
				next_wpt = wptmanager.FindNewWaypointForBotAtPathEnd(pBot, p->wpt_index);

			if (next_wpt != NO_VAL)
			{
				pBot->SetCurrentWaypoint(next_wpt);

				if (botdebugger.IsDebugPaths())
				{
					char bpmsg[128];
					sprintf(bpmsg, "\n***Reached path start! - path #%d | curr_wpt #%d | dir is (end->start) | next wpt #%d\n\n", pBot->curr_path_index + 1, p->wpt_index + 1, pBot->curr_wpt_index + 1);
					conOutput.Notify(bpmsg);
				}

				// clear this flag before returning
				// as we always expect standard (start -> end) path navigation when we get on a new path
				pBot->RemoveTask(TASK_OPPOSITEPATHDIR);

				return 1;
			}
			else
			{
				if (p->next != NULL)
				{
					pBot->RemoveTask(TASK_OPPOSITEPATHDIR);
					path_next_wpt = p->next->wpt_index;
				}

				// we're skipping the waiting if this is either ammobox or use waypoint
				if (IsPathEndException(pBot->curr_wpt_index, pBot->GetBotTeam()))
				{
					return 0;
				}

				pBot->SetWaitTime(RANDOM_FLOAT(15.0f, 60.0f));

				if (botdebugger.IsDebugPaths())
				{
					char bpemsg[128];
					sprintf(bpemsg, "\n\n<<BUG IN WAYPOINTS>>Reached the end of this path, but didn't find next wpt! - path #%d | curr_wpt #%d\n\n\n", pBot->curr_path_index + 1, p->wpt_index + 1);
					conOutput.Notify(bpemsg);
				}

				return -1;
			}
		}
	}
	// have we reached the start of patrol path?
	else if ((p->prev == NULL) && wptmanager.IsPath(pBot->curr_path_index, PathT::patrol_cycle))
	{
		// if the path ends on goback waypoint then do nothing (further code does all we need)
		if (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::goback) && (wptmanager.GetWaypointPriority(pBot->curr_wpt_index, pBot->GetBotTeam()) != 0))
			;
		// the chance to leave this path is slightly higher when we are on its start
		else if (RANDOM_LONG(1, 100) < 25)
		{
			int next_wpt = wptmanager.FindNewWaypointForBotAtPathEnd(pBot, p->wpt_index);

			if (next_wpt != NO_VAL)
			{
				pBot->SetCurrentWaypoint(next_wpt);

				if (botdebugger.IsDebugPaths())
				{
					char ppmsg[128];
					sprintf(ppmsg, "\n***Reached patrol path start! - path #%d | curr_wpt #%d | dir is (end->start) | next wpt #%d\n\n", pBot->prev_path_index + 1, p->wpt_index + 1, pBot->curr_wpt_index + 1);
					conOutput.Notify(ppmsg);
				}

				pBot->RemoveTask(TASK_OPPOSITEPATHDIR);

				return 1;
			}
		}
			
		// we keep the bot on patrol so we'll just change the direction the bot had
		pBot->RemoveTask(TASK_OPPOSITEPATHDIR);
		path_next_wpt = p->next->wpt_index;
	}
	// otherwise get next (this time from previous node in llist) waypoint to head to
	else
	{
		if (p->prev != NULL)
		{
			path_next_wpt = p->prev->wpt_index;
			path_end = FALSE;
		}
	}

	return 0;
}


/*
* checks current path flags and sets correct behaviour based on it
*/
void AssignPathBasedBehaviour(bot_t *pBot, int wpt_index)
{
	if ((pBot->curr_path_index == NO_VAL) || (w_paths[pBot->curr_path_index] == NULL))
		return;

	// if actual path is PATROL path store last waypoint index (to return to it after combat)
	if (wptmanager.IsPath(pBot->curr_path_index, PathT::patrol_cycle))
		pBot->SetPatrolPathWaypoint(wpt_index);
	// if actual path isn't PATROL AND the index was set then clear it
	else if (pBot->IsPatrolPathWaypoint() && (wptmanager.IsPath(pBot->curr_path_index, PathT::patrol_cycle) == false))
		pBot->SetPatrolPathWaypoint(NO_VAL);

	// set avoid enemy task if the path forces the bot to do so
	if ((pBot->IsTask(TASK_AVOID_ENEMY) == false) && wptmanager.IsPath(pBot->curr_path_index, PathT::avoid_far_enemy))
	{
		pBot->SetTask(TASK_AVOID_ENEMY);
		
		// keep these messages as alert at console so the user can turn them on/off using the developer 1 command since these are just additional info
		if (botdebugger.IsDebugPaths())
			ALERT(at_console, "<<PATHS>>Bot %s will AVOID all enemies that aren't in current weapon range (path #%d)\n", pBot->name, pBot->curr_path_index + 1);
	}

	// remove avoid enemy task if the path is a "common path"
	if (pBot->IsTask(TASK_AVOID_ENEMY) && (wptmanager.IsPath(pBot->curr_path_index, PathT::avoid_far_enemy) == false))
	{
		pBot->RemoveTask(TASK_AVOID_ENEMY);

		if (botdebugger.IsDebugPaths())
			ALERT(at_console, "<<PATHS>>Bot %s will no longer AVOID enemies! Any visible foe can be a target again (path #%d)\n", pBot->name, pBot->curr_path_index + 1);
	}
	
	// set ignore enemy task if the path forces the bot to do so
	if ((pBot->IsTask(TASK_IGNORE_ENEMY) == false) && wptmanager.IsPath(pBot->curr_path_index, PathT::ignore_the_enemy))
	{
		pBot->SetTask(TASK_IGNORE_ENEMY);
		
		if (botdebugger.IsDebugPaths())
			ALERT(at_console, "<<PATHS>>Bot %s will IGNORE most enemies now (path #%d)\n", pBot->name, pBot->curr_path_index + 1);
	}
	
	// remove ignore enemy task if the path is a "common path" AND the bot doesn't sprint right now
	if (pBot->IsTask(TASK_IGNORE_ENEMY) && (pBot->IsTask(TASK_SPRINT) == false) && (wptmanager.IsPath(pBot->curr_path_index, PathT::ignore_the_enemy) == false))
	{
		pBot->RemoveTask(TASK_IGNORE_ENEMY);

		if (botdebugger.IsDebugPaths())
			ALERT(at_console, "<<PATHS>>Bot %s will no longer IGNORE enemies! Any visible foe can be a target again (path #%d)\n", pBot->name, pBot->curr_path_index + 1);
	}

	return;
}


/*
* path navigation (also handles a path change)
* returns 1 = successful path based navigation,
* 0 = something went wrong but keep path navigation (ie don't try basic wpt-to-wpt navigation),
* -1 = error (try basic waypoint-to-waypoint navigation)
*/
int BotOnPath(bot_t *pBot)
{
	int path_next_wpt = NO_VAL;
	bool found = FALSE;
	bool path_end = TRUE;
	W_PATH *p = NULL;

	// get pointer to path node that holds current waypoint (ie. find current waypoint on path)
	if ((p = wptmanager.GetWaypointPointer(pBot->curr_wpt_index, pBot->curr_path_index)) != NULL)
	{
		found = TRUE;
	}
	// current waypoint isn't on current path (ie bot must have changed path)
	else
	{
		// check all paths for current waypoint
		if (wptmanager.WasPossiblePathForBotFoundOnWaypoint(pBot, pBot->curr_wpt_index))
		{
			if ((p = wptmanager.GetWaypointPointer(pBot->curr_wpt_index, pBot->curr_path_index)) != NULL )
			{
				found = TRUE;
			}
		}
	}

	// we found nothing ie current waypoint isn't on any path
	if (p == NULL)
	{
		// update path history
		pBot->prev_path_index = pBot->curr_path_index;
		pBot->curr_path_index = NO_VAL;	// clear current path index

		return -1;
	}

	// is standard path move (start -> end)
	if ((pBot->IsTask(TASK_OPPOSITEPATHDIR) == false) && (path_next_wpt == NO_VAL))
	{
		int forward_status = ForwardPathMove(pBot, p, path_next_wpt, path_end);
		
		if (forward_status != 0)
		{
			return forward_status;
		}
	}

	// is opposite direction path move (end -> start)
	if (pBot->IsTask(TASK_OPPOSITEPATHDIR) && (path_next_wpt == NO_VAL))
	{
		int backward_status = BackwardPathMove(pBot, p, path_next_wpt, path_end);

		if (backward_status != 0)
		{
			return backward_status;
		}
	}

	// current waypoint has been found on one path AND we also found next waypoint on this path to continue to
	if (found && (path_next_wpt != NO_VAL))
	{
		// is current waypoint a goback waypoint AND we are NOT at the end of the path
		if (wptmanager.IsWaypointTypeTeamPriority(pBot->curr_wpt_index, WptT::goback, NON_ZERO_WPT_PRIORITY, pBot->GetBotTeam()) && (path_end == FALSE))
		{
			// clear opposite path direction flag and set next waypoint in llist
			if (pBot->IsTask(TASK_OPPOSITEPATHDIR) && (p->next != NULL))
			{
				pBot->RemoveTask(TASK_OPPOSITEPATHDIR);
				path_next_wpt = p->next->wpt_index;
			}
			// otherwise set opposite path direction flag and use prev node waypoint
			else if ((pBot->IsTask(TASK_OPPOSITEPATHDIR) == false) && (p->prev != NULL))
			{
				pBot->SetTask(TASK_OPPOSITEPATHDIR);
				path_next_wpt = p->prev->wpt_index;
			}
		}

		// set next path waypoint as a waypoint to head towards
		pBot->SetCurrentWaypoint(path_next_wpt);

		// set correct behaviour based on the path type
		AssignPathBasedBehaviour(pBot, path_next_wpt);

		if (botdebugger.IsDebugPaths())
		{
			char pmsg[128]{};
			if (pBot->IsTask(TASK_OPPOSITEPATHDIR))
				sprintf(pmsg, "<<PATHS>>current path is #%d | current waypoint is #%d | path direction is (end->start)\n", pBot->curr_path_index + 1, pBot->curr_wpt_index + 1);
			else
				sprintf(pmsg, "<<PATHS>>current path is #%d | current waypoint is #%d | path direction is (start->end)\n", pBot->curr_path_index + 1, pBot->curr_wpt_index + 1);
			conOutput.Notify(pmsg);
		}

		return 1;
	}

#ifdef _DEBUG
	char msg[80]{};
	sprintf(msg, "BotOnPath()->unknown event | isTask(OppossitePathDir) %d\n", pBot->IsTask(TASK_OPPOSITEPATHDIR));
	util.DebugDev(msg, pBot->curr_wpt_index, pBot->curr_path_index);
#endif

	return -1;
}


/*
* find the nearest next waypoint from current waypoint
*/
bool BotFindWaypoint(bot_t *pBot, bool ladder)
{
	int next_waypoint = NO_VAL;

	//																						NOTE: Currently this cannot happen!!!
	//					It should probably be changed to check for movetype == MOVETYPE_FLY
	//					and scrap the bool ladder variable altogether.
	if (ladder)
	{
		next_waypoint = FindRightLadderWpt(pBot);
	}
	else
	{
		// look for nearby waypoints
		next_waypoint = wptmanager.FindNextWaypointForBot(pBot);
	}

	// is there at least one
	if (next_waypoint != NO_VAL)
	{		
		// update visited wpts history; prevents the loop effect
		UpdateWptHistory(pBot);

		pBot->SetCurrentWaypoint(next_waypoint);

		return true;
	}

	return false;
}


/*
* makes bot start particular action or modify behaviour based on waypoint type
* the order of types isn't alphabetical, but first come types that cannot or shouldn't be combined together and then are more universal types that can be combined with other types
*/
void AssignWaypointBasedBehaviourUponReachingIt(bot_t* pBot, bool& waypoint_found)
{
	edict_t* pEdict = pBot->pEdict;
	bool can_reset_aims_now = true;

	// handles the cases that this waypoint is the first waypoint on the path (ie. bot was at cross waypoint and the waypoint he chose is this one)
	// we do not have a path yet, because the cross waypoint kills it, but some waypoint types do need the info from the path here so we'll call the 'give me my path' right here
	// (standard path navigation function would have done it anyway ... just too late for this particular case)
	if ((pBot->curr_path_index == NO_VAL) && (num_w_paths > 0))
	{
		if (wptmanager.WasPossiblePathForBotFoundOnWaypoint(pBot, pBot->curr_wpt_index))
			AssignPathBasedBehaviour(pBot, pBot->curr_wpt_index);
	}

	if ((pBot->IsNeed(NEED_NEXTWPT) == false) && wptmanager.IsWaypointTypeTeamPriority(pBot->curr_wpt_index, WptT::ammobox, NON_ZERO_WPT_PRIORITY, pBot->GetBotTeam()))
	{
		// not carrying the explosives charge AND first time the bot got to this waypoint?
		if ((pBot->IsEquippedWithExplosiveCharge() == false) && (pBot->IsTask(TASK_WPTACTION) == false) && (pBot->IsTask(TASK_CLAY_IGNORE) == false))
		{
			// did we find any explosives charge within ammobox waypoint range?
			if (util.IsExplosivesChargeNearby(pBot, waypoints[pBot->curr_wpt_index].range, waypoints[pBot->curr_wpt_index].origin))
			{
				pBot->SetTask(TASK_WPTACTION);
				pBot->SetTask(TASK_CLAY_IGNORE);
				// set some time to allow the bot move to and step on the explosives charge to get it
				pBot->SetActionTime(10.0f);

				pBot->ResetAims("HeadTowardWpt() -> AMMOBOX wpt");
				pBot->SetTask(TASK_IGNOREAIMWPTS);

				waypoint_found = TRUE;
				pBot->SetNeed(NEED_NEXTWPT);

				if (botdebugger.IsDebugActions() || botdebugger.IsDebugWaypoints())
				{
					char msg[TEXT_MSG_SIZE];
					sprintf(msg, "***Going to get the explosives charge found at ammobox waypoint no. %d\n", pBot->curr_wpt_index + 1);
					conOutput.Notify(msg);
				}
			}
			// print a note to waypoint creator so that he/she knows the range may not be set right ... in case the explosives charges aren't taken by anyone yet
			else if (botdebugger.IsDebugWaypoints())
			{
				char msg[TEXT_MSG_SIZE];
				sprintf(msg, "***Found no explosives charge at ammobox waypoint no. %d\n", pBot->curr_wpt_index + 1);
				conOutput.Notify(msg);
			}
		}
#ifdef _DEBUG
		else
		{
			if (botdebugger.IsDebugActions())
			{
				char dm[128]{};
				sprintf(dm, "HeadTowardWpt() -> AMMOBOX wpt #%d >>>ELSE STATEMENT - (most probably has expl. charge already)<<<\n", pBot->curr_wpt_index + 1);
				conOutput.Notify(dm, pBot);
			}
		}
#endif
	}

	else if (wptmanager.IsWaypointTypeTeamPriority(pBot->curr_wpt_index, WptT::use, NON_ZERO_WPT_PRIORITY, pBot->GetBotTeam()))
	{
		// first time the bot got to this waypoint?
		if ((pBot->IsTask(TASK_WPTACTION) == false) && (pBot->IsNeed(NEED_NEXTWPT) == false))
		{
			// try to use wait time from this waypoint as the action time
			float usewpt_wait_time = wptmanager.GetWaypointWaitTime(pBot->curr_wpt_index, pBot->GetBotTeam());

			// if there's no wait time on this waypoint then generate some action time randomly
			if (usewpt_wait_time == 0.0f)
				pBot->SetActionTime(RANDOM_FLOAT(1.0f, 2.2f));
			// otherwise use it
			else
			{
				pBot->SetActionTime(usewpt_wait_time);
				pBot->SetSubTask(ST_TANK_SHORT);
			}

			pBot->SetTask(TASK_WPTACTION);
			waypoint_found = TRUE;		// prevents finding new waypoint
			pBot->SetNeed(NEED_NEXTWPT);



#ifdef _DEBUG
			//@@@@@@@@@@@@@@@
			if (botdebugger.IsDebugActions())
			{
				char dm[128];
				sprintf(dm, "HeadTowardWpt() -> USE wpt #%d called\n", pBot->curr_wpt_index + 1);
				conOutput.Notify(dm);
			}
#endif


		}
	}

	else if ((pBot->IsNeed(NEED_NEXTWPT) == false) && wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::door, WptT::dooruse))
	{
		// no door possition yet? (ie bot must have just reached this waypoint)
		if (pBot->GetPositionOfPointInSpace() == g_vecZero)
		{
			edict_t* pent = NULL;

#ifdef _DEBUG
			//@@@@@@@@@@@@@@@
			if (botdebugger.IsDebugActions())
			{
				char dm[128]{};
				sprintf(dm, "HeadTowardWpt() -> DOOR wpt #%d called | NextWpt is #%d\n", pBot->curr_wpt_index + 1, pBot->GetNextWaypointOnPath() + 1);
				conOutput.Notify(dm);
			}
#endif

			pBot->f_dont_avoid_wall_time = gpGlobals->time + 2.0;
			pBot->RemoveSubTask(ST_DOOR_OPEN);

			// is the bot at first door wpt (i.e. not past door yet)?
			// it's done this way because of the possibility of a sequence of two 'door' waypoints (two-way both teams paths a.k.a. default paths will probably have 'door' waypoints on both sides of the doors)
			// and we don't want to do anything while the bot is passing the second 'door' waypoint (i.e. when he's already behind the doors) therefore we are checking if previous wpt was NOT a 'door' waypoint
			if (wptmanager.IsWaypoint(pBot->prev_wpt_index.get(), WptT::door, WptT::dooruse) == false)
			{
				while ((pent = util.FindEntityInSphere(pent, pEdict->v.origin, STANDARD_SEARCH_RADIUS)) != NULL)
				{
					if (util.IsDoorEntity(pent))
					{
						// remember this door entity
						pBot->SetPointerToGEnt(pent);
						
						Vector door_position;

						// if the bot is able to get his next waypoint then use the next waypoint as the door origin otherwise use the real door origin
						// this system allows the bot to successfully pass even doors that are partially open or are closing at the moment
						// (using the real door origin in this case would cause that the bot will try facing the doors and will get stuck there)
						int next_waypoint_on_path = pBot->GetNextWaypointOnPath();

						if (next_waypoint_on_path != NO_VAL)
							door_position = waypoints[next_waypoint_on_path].origin;
						else
							// BModels have 0,0,0 for origin so must use VecBModelOrigin
							door_position = util.VecBModelOrigin(pent);

						// store door position
						pBot->SetPositionOfPointInSpace(door_position);

						// do we need to open the door at all?
						if (util.IsDoorOpen(pent, pBot->IsCrouched()) == false)
						{
							// then start facing them
							Vector bot_angles = UTIL_VecToAngles(door_position - pEdict->v.origin);
							pEdict->v.ideal_yaw = bot_angles.y;
							BotFixIdealYaw(pEdict);

							// don't look for next waypoint now
							waypoint_found = TRUE;
							pBot->SetNeed(NEED_NEXTWPT);

							pBot->SetWaitTime(1.0f);
							pBot->SetSubTask(ST_DOOR_OPEN);
						}

						break;
					}
				}
			}
#ifdef DEBUG
			else
			{
				if (botdebugger.IsDebugActions())
				{
					char dm[128]{};
					sprintf(dm, "HeadTowardWpt() -> DOOR wpt #%d called, PREV wpt #%d was DOOR wpt too -> IGNORING (because already past the door)\n",
						pBot->curr_wpt_index + 1, pBot->prev_wpt_index.get() + 1);
					conOutput.Notify(dm);
				}
			}
#endif // DEBUG
		}
	}

	/*/
	else if ((pBot->IsNeed(NEED_NEXTWPT) == false) && wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::parachute))
	{
		// has bot the parachute?
		if (pBot->IsTask(TASK_PARACHUTE))
		{
			float wpt_time = wptmanager.GetWaypointWaitTime(pBot->curr_wpt_index, pBot->GetBotTeam());

			// is there a wait time used on this waypoint?
			if (wpt_time > 0.0f)
			{
				// then use it to prevent the bot look for waypoints
				pBot->SetDontLookForWaypoint(wpt_time);

				// also once the bot jumps out use this time to postpone the moment when he opens the parachute
				pBot->SetParachuteUseTime(wpt_time);
			}
			// otherwise use universal time values
			else
			{
				pBot->SetDontLookForWaypoint(1.5f);//was 2.5
				pBot->SetParachuteUseTime(1.5f);//was 0.7
			}

			// is there a priority 1 setting on this waypoint? // then do nothing in order to allow the bot target next waypoint
			if (wptmanager.GetWaypointPriority(pBot->curr_wpt_index, pBot->GetBotTeam()) == 1)
				;
			// otherwise use default behaviour for parachute waypoint and...
			else
			{
				// make the bot ignore waypoints which is needed to prevent him keep turning to current waypoint because...
				pBot->SetTask(TASK_IGNOREWPTNAV);

				if (pBot->prev_wpt_index.get() != NO_VAL)
				{
					// we will use the position of previous waypoint to make the bot continue moving on a trajectory from previous waypoint through this one (ie. parachute one) and further in that direction
					Vector v_direction = pBot->GetCurrWptPosition() - waypoints[pBot->prev_wpt_index.get()].origin;
					Vector v_angles = UTIL_VecToAngles(v_direction);

					pEdict->v.ideal_yaw = v_angles.y;
					BotFixIdealYaw(pEdict);
				}
			}

			if (botdebugger.IsDebugActions())
				conOutput.Notify("Passed through parachute waypoint -> setting the time to check being in midair to use parachute\n", pBot);
		}
		// bot doesn't have parachute yet
		else
		{
			// if the bot is NOT on one-way path then turnback
			if (pBot->ReturnBackToPathStart())
			{
				pBot->SetNeed(NEED_NEXTWPT);
			}
			// otherwise wait until the parachute spawns again
			else
			{
				// there are no paths or current path isn't two-way path (or doesn't match) so wait at this waypoint
				pBot->SetWaitTime(RANDOM_FLOAT(2.0f, 5.0f));

				// don't look for next waypoint
				waypoint_found = TRUE;

				pBot->SetTimeToReachCurrWaypoint();
			}

			if (botdebugger.IsDebugActions() || botdebugger.IsDebugWaypoints())
				conOutput.Notify("No parachute, cannot pass through\n", pBot);
		}
	}
	/**/

	else if ((pBot->IsNeed(NEED_NEXTWPT) == false) && wptmanager.IsWaypointTypeTeamPriority(pBot->curr_wpt_index, WptT::roadblock, NON_ZERO_WPT_PRIORITY, pBot->GetBotTeam()))
	{
		// is the path blocked right now? (it wasn't when bot picked it on cross, but it got blocked meanwhile) AND is this the first roadblock from the pair?
		if (wptmanager.IsPath(pBot->curr_path_index, PathT::roadblocked_tag) && (wptmanager.IsWaypoint(pBot->prev_wpt_index.get(), WptT::roadblock) == false))
		{
			// is this waypoint a combination of roadblock and claymore AND bot has exlopsives charge AND is there any breakable object OR is bot within the area where the explosives charge activates?
			//if (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::claymore) && pBot->IsEquippedWithExplosiveCharge() && (util.CheckForClaymoreOnlySDObjectAround(pBot) || pBot->IsSubTask(ST_INAREA)))
			if (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::claymore) && pBot->IsEquippedWithExplosiveCharge() &&
				(pBot->IsSubTask(ST_INAREA) || (wptmanager.IsPairedWaypointReachableForThisBot(pBot, WptT::roadblock) == false)))
			{
				pBot->ResetAims("HeadTowardWpt() -> ROADBLOCK + CLAYMORE wpt");

				// set some wait time
				pBot->SetWaitTime(5.0f);

				// we must prevent finding a new waypoint now, because bot needs to plant the explosives charge first before he turns back and leaves this waypoint
				waypoint_found = TRUE;

				// also tell the bot to get away from the blast
				pBot->SetTask(TASK_CLAY_EVADE);
			}
			// is the bot anti-armor specialist on his own class based path AND has enough ammo for the launcher AND knows there is a solid ground in front AND this obstacle is already gone?
			else if (wptmanager.IsPath(pBot->curr_path_index, PathT::antiarmor_class) && pBot->IsBehaviour(AASPEC) && (pBot->IsNoAmmoForMainWeapon() == false) &&
				wptmanager.IsWaypointTypeTeamPriority(pBot->curr_wpt_index, WptT::roadblock, 1, pBot->GetBotTeam()) && wptmanager.IsPairedWaypointReachableForThisBot(pBot, WptT::roadblock))
			{
					// then do nothing and let him pass through this waypoint so that he can try to destroy the other obstacle that is still blocking this pathway
					// this empty statement is mainly needed only to deal with paths with multiple roadblock pairs, without it the bot would return back to path start upon reaching the first roadblock waypoint
					// because the path doesn't lose the roadblocked tag until all the roadblock pairs return the status "area is passable"
			}
			// if the bot is NOT on one-way path then turnback (this gets called if the bot cannot pass through - eg. has no explosives charge)
			else if (pBot->ReturnBackToPathStart("HeadTowardWpt() -> ROADBLOCK wpt"))
			{
				pBot->SetNeed(NEED_NEXTWPT);
			}
			// otherwise wait till the pathway gets free again
			else if (pBot->IsTask(TASK_CLAY_EVADE) == false)
			{
				// there are no paths or current path isn't two-way path (or doesn't match) so wait at this waypoint
				pBot->SetWaitTime(RANDOM_FLOAT(2.0f, 5.0f));

				// don't look for next waypoint
				waypoint_found = TRUE;

				pBot->SetTimeToReachCurrWaypoint();

#ifdef _DEBUG
				//@@@@@@@@@@@@@@@@@@@@@@@
				conOutput.Notify("HeadTowardWpt() -> ROADBLOCK + CLAYMORE -> else (wait till it gets free)\n");
#endif

			}
		}
	}

	else if ((pBot->IsNeed(NEED_NEXTWPT) == false) && wptmanager.IsWaypointTypeTeamPriority(pBot->curr_wpt_index, WptT::shoot, NON_ZERO_WPT_PRIORITY, pBot->GetBotTeam()))
	{
		pBot->CheckAmmoReserves("HeadTowardWpt() -> SHOOT wpt");
		
		// no ammo for either weapon?
		if (pBot->IsNoAmmoForMainWeapon() && pBot->IsNoAmmoForBackupWeapon())
			;// then simply ignore this waypoint
		else
		{
			pBot->ResetAims("HeadTowardWpt() -> SHOOT wpt");
			can_reset_aims_now = false;	// ensures the reset won't be called again in the 'is there a wait time on this waypoint', because then we would lose the task set below

			// force the bot get the aim waypoint "exactly" in the middle of FOV (i.e. in very tight view cone)
			pBot->SetTask(TASK_PRECISEAIM);

			// run the fire task only if there's no wait time on this waypoint otherwise we have different code for that case ... the wait time code will catch it
			if (wptmanager.GetWaypointWaitTime(pBot->curr_wpt_index, pBot->GetBotTeam()) == 0.0f)
			{
				pBot->SetTask(TASK_FIRE);
				pBot->SetNeed(NEED_NEXTWPT);
			}

			waypoint_found = TRUE;		// prevent finding a new waypoint


#ifdef _DEBUG
			//@@@@@@@@@@@@@@@@@@@@@@@
			if (botdebugger.IsDebugActions() || botdebugger.IsDebugWaypoints())
				conOutput.Notify("HeadTowardWpt() -> SHOOT wpt called\n");
#endif


		}
	}

	// is this wpt a claymore waypoint with non zero priority for the team bot is in AND has NOT been "used" yet AND has the bot a claymore mine
	else if ((pBot->IsNeed(NEED_NEXTWPT) == false) && wptmanager.IsWaypointTypeTeamPriority(pBot->curr_wpt_index, WptT::claymore, NON_ZERO_WPT_PRIORITY, pBot->GetBotTeam()) &&	pBot->IsEquippedWithExplosiveCharge())
	{
		// is there any breakable object OR is bot within area where the explosive charge activates?
		if (util.CheckForClaymoreOnlySDObjectAround(pBot) || pBot->IsSubTask(ST_INAREA))
		{
			pBot->ResetAims("HeadTowardWpt() -> CLAYMORE wpt");

			// set some wait time
			pBot->SetWaitTime(5.0f);

			// prevent finding a new waypoint
			pBot->SetNeed(NEED_NEXTWPT);
			pBot->SetTask(TASK_CLAY_EVADE);
			waypoint_found = TRUE;
		}

#ifdef _DEBUG
		//@@@@@@@@@@@@@@@@@@@@@@@
		if (botdebugger.IsDebugActions() || botdebugger.IsDebugWaypoints())
			conOutput.Notify("HeadTowardWpt() -> CLAYMORE wpt called\n");
#endif

	}

	else if (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::ladder))
	{
		// prevents unwanted actions (like turning away from wall etc.)
		pBot->f_dont_avoid_wall_time = gpGlobals->time + 2.0;
		pBot->SetDontCheckStuck("HeadTowardWpt() -> LADDER waypoint", 0.5f);
	}

	// is this DoD specific Area capture control point AND is NOT captured yet?
	else if (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::pushpoint) && pBot->IsSubTask(ST_INAREA) && util.CanBotCaptureTheArea(pBot, true))
	{
		// can bot capture it alone or does he see any teammate nearby? ... we have to do the same checks again or else bot could act incorrectly
		if (util.CanBotCaptureTheArea(pBot))
		{
			// then start waiting in order to capture this point together
			pBot->SetWaitTime(2.0f);

			// set this task to know it's Area capture
			pBot->SetTask(TASK_PARACHUTE);

			pBot->ResetAims("HeadTowardWpt() -> PUSHPOINT wpt - capture it");

			// try to get some cover
			if ((pBot->IsProne() == false) && pBot->IsNotGoingProne())
				pBot->SetStance(GOTO_CROUCH, "HeadTowardWpt() -> PUSHPOINT wpt -> GOTO crouch to cap it");

			// don't look for another waypoint till the point is captured
			waypoint_found = TRUE;
		}
		// otherwise if there is no teammate around, but this bot decided to reach map goal then try to randomly wait for a teammate
		else if (pBot->IsNeed(NEED_GOAL) && (RANDOM_LONG(1, 100) < 33))
		{
			// set some wait time in order to stay in the Area capture zone and wait for a possibility of a teammate coming to capture it too
			pBot->SetWaitTime(RANDOM_LONG(2.0f, 5.0f));

			// allows disabling standard stance control for waiting ie. bot will stay crouched this way even when there isn't a crouch waypoint tag set on current waypoint
			pBot->SetSubTask(ST_PARACHUTE_USED);

			pBot->ResetAims("HeadTowardWpt() -> PUSHPOINT wpt - wait for teammate");

			if ((pBot->IsProne() == false) && pBot->IsNotGoingProne())
				pBot->SetStance(GOTO_CROUCH, "HeadTowardWpt() -> PUSHPOINT wpt -> GOTO crouch while waiting if any teammate shows");
		}
	}

	// following types can be combined with the types above (some of them aren't ideal, but well) so no more 'else if'

	if (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::duckjump, WptT::jump))
	{
		// first we must check for prone because sending IN_JUMP would break it
		if ((pBot->IsBehaviour(BOT_PRONED) == false) && pBot->IsNotGoingProne())
		{
			pEdict->v.button |= IN_JUMP;  // jump here

			if (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::duckjump))
			{
				// also press crouch
				pBot->SetStance(GOTO_CROUCH, "HeadTowardWpt() -> DUCKJUMP wpt -> GOTO crouch");

				pBot->SetDuckJumpTime(1.0f);
			}
		}
	}

	if (wptmanager.IsWaypointTypeTeamPriority(pBot->curr_wpt_index, WptT::sprint, NON_ZERO_WPT_PRIORITY, pBot->GetBotTeam()))
	{
		// the bot is already sprinting so stop it (ie. he got to 2nd sprint waypoint)
		if (pBot->IsTask(TASK_SPRINT))
		{
			pBot->RemoveTask(TASK_SPRINT);


#ifdef _DEBUG
			//@@@@@@@@@@@@@@@@@
			if (botdebugger.IsDebugActions() || botdebugger.IsDebugWaypoints())
				conOutput.Notify("HeadTowardWpt() -> REMOVED the task SPRINT at sprint WPT\n");
#endif


		}
		// not sprinting yet so start it (ie. he got to 1st/the only sprint waypoint)
		else
		{
			pBot->SetTask(TASK_SPRINT);


#ifdef _DEBUG
			//@@@@@@@@@@@@@@@@@
			if (botdebugger.IsDebugActions() || botdebugger.IsDebugWaypoints())
				conOutput.Notify("HeadTowardWpt() -> SET the task SPRINT at sprint WPT\n");
#endif

			// is there assigned a priority 1 for the team this bot joined on this waypoint?
			if (wptmanager.GetWaypointPriority(pBot->curr_wpt_index, pBot->GetBotTeam()) == 1)
			{
				// then make sure that the bot won't stop and shoot while we need him to sprint, he will attack only close enemy in this case
				pBot->SetTask(TASK_IGNORE_ENEMY);


#ifdef _DEBUG
				//@@@@@@@@@@@@@@@@@
				if (botdebugger.IsDebugActions() || botdebugger.IsDebugWaypoints())
					conOutput.Notify("HeadTowardWpt() -> SET the task IGNORE ENEMY at sprint wpt with priority 1 setting\n");
#endif

			}
		}
	}

	if (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::sniper) && (pBot->IsTask(TASK_DONTMOVEINCOMBAT) == false))
	{
		pBot->SetTask(TASK_DONTMOVEINCOMBAT);		// don't move in combat
	}

	if (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::prone) && (pBot->IsBehaviour(BOT_PRONED) == false))
	{
		pBot->GoProne("HeadTowardWpt() -> At PRONE wpt but not in prone");
	}

	// is there some wait time on this waypoint AND safety statement (we won't keep waiting) AND this isn't a parachute waypoint?
	if ((wptmanager.GetWaypointWaitTime(pBot->curr_wpt_index, pBot->GetBotTeam()) > 0.0f) && (pBot->IsNeed(NEED_NEXTWPT) == false) && (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::parachute) == false))
	{
		// set wait time only if path class restriction and bot "class" do match
		// otherwise even the sniper/mgunner chasing bots would camp there, but they should just scout the paths and kill the sniper/mgunner
		if (wptmanager.IsPath(pBot->curr_path_index, PathT::sniper_class, PathT::mgunner_class))
		{
			if (wptmanager.IsPath(pBot->curr_path_index, PathT::sniper_class) && pBot->IsBehaviour(SNIPER))
			{
				pBot->SetWaitTime(ModifyWaypointWaitTimeByBotBehaviour(pBot));
				waypoint_found = TRUE;

				if (botdebugger.IsDebugWaypoints())
					ALERT(at_console, "Bot SNIPER - on sniper path -> going to camp here\n");
			}

			if (wptmanager.IsPath(pBot->curr_path_index, PathT::mgunner_class) && pBot->IsBehaviour(MGUNNER))
			{
				pBot->SetWaitTime(ModifyWaypointWaitTimeByBotBehaviour(pBot));
				waypoint_found = TRUE;

				if (botdebugger.IsDebugWaypoints())
					ALERT(at_console, "Bot MGUNNER - on mgunner path -> going to camp here\n");
			}
		}
		// if no path class restriction or no path at all set wait time
		else
		{
			pBot->SetWaitTime(ModifyWaypointWaitTimeByBotBehaviour(pBot));
			waypoint_found = TRUE;

			if (botdebugger.IsDebugWaypoints())
				ALERT(at_console, "Reached 'wait' time wpt -> going to camp here\n");
		}

		// this waypoint is usable for this bot so ...
		if (waypoint_found)
		{
			// clear the aim waypoint array and stuff
			if (can_reset_aims_now)
				pBot->ResetAims("HeadTowardWpt() -> Reached wpt with WAIT TIME");

			// and set the need for next waypoint to prevent repeating this action again
			pBot->SetNeed(NEED_NEXTWPT);
		}
	}
}


/*
* head towards to your waypoint if close enough find next one
*/
bool BotHeadTowardWaypoint( bot_t *pBot )
{
	int found;
	float wpt_distance, wpt_range;
	bool in_wpt_range;		// when bot reaches the wpt range
	bool past_the_wpt;		// if bot run past current wpt set this and then double check if really reach its range
	bool is_next_wpt;		// do we have next wpt to head to
	int unreachable_wpt_index = NO_VAL;	// prevents the bot to take the same wpt where he got stuck before
	float wpt_distance2D;	// prevents z-coord problems (like when there's a dead body at the waypoint etc.)
	bool use_only_planar;	// in cases when we need to test just the 2D distance to waypoint (eg. tiny wpt range)
	char dbgmsg[TEXT_MSG_SIZE]{};		// allows printing feedback for waypointer who's testing his work

#ifdef DEBUG
	bool debug_reaching_range = false;	// allows turning reaching waypoint range debugging on easily by just this switch
#endif // DEBUG


	edict_t *pEdict = pBot->pEdict;

	// forget on any waypoint that is unreachable for longer time
	if (pBot->NotReachedCurrWaypointFor(5.0f))
	{
#ifdef _DEBUG
		//@@@@@@@@@@@@@@@@
		if (botdebugger.IsDebugStuck())
		{
			//sprintf(dbgmsg, "(nav.cpp) timeToReachCurrWpt=%3.f < globTIME=%3.f for wpt #%d ---- !!GENERAL!! !!GENERAL!! !!GENERAL!!\n",
			//	pBot->GetTimeToReachCurrWapoint(), gpGlobals->time, pBot->curr_wpt_index+1);
			//conOutput.Notify(dbgmsg);
		}
#endif

		// if bot was stuck in last 5 seconds AND it's NOT too soon after battle,
		// because the bot can move far away from his waypoint while in battle so we must allow him to reset navigation
		// if he isn't able to reach his waypoint anymore (eg. he fell from roof during battle)
		if ((pBot->NotBeenStuckFor(5.0f) == false) && pBot->NotSeenEnemyfor(8.0f) && pBot->NotBeenWaitingForEnemyFor(8.0f))
		{
			if (pBot->IsNeed(NEED_RESETNAVIG) == false)
			{
				// then update the waypoint time to compensate the time he lost trying to free self
				pBot->SetTimeToReachCurrWaypoint();
				// and prepare the navigation reset
				pBot->SetNeed(NEED_RESETNAVIG);

				if (botdebugger.IsDebugPaths() || botdebugger.IsDebugStuck() || botdebugger.IsDebugWaypoints())
				{
					sprintf(dbgmsg, "<<WARNING>> was stuck while heading to waypoint #%d -> setting extra time to reach it!\n", pBot->curr_wpt_index + 1);
					conOutput.Notify(dbgmsg, pBot);
				}
			}
			else
			{
				// the bot already had 15 seconds to reach current waypoint (default 5 seconds, then update of the reach time, and now we're checking for 10 second) so...
				if (pBot->NotReachedCurrWaypointFor(10.0f))
				{
					// if the bot is still stuck while trying to reach this waypoint then mark current waypoint as unreachable waypoint to allow resetting navigation
					unreachable_wpt_index = pBot->curr_wpt_index;

					if (botdebugger.IsDebugPaths() || botdebugger.IsDebugStuck() || botdebugger.IsDebugWaypoints())
					{
						sprintf(dbgmsg, "<<WARNING>> extra stuck time is over & still can't reach the waypoint -> going to RESET NAVIG!\n");
						conOutput.Notify(dbgmsg, pBot);
					}
				}
			}
		}
		// if bot is near door ie handling door or on ladder or in crouch or in prone of moving at slow or even slower speeds (e.g. going down the hill or stairs)
		else if (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::door, WptT::dooruse) || wptmanager.IsWaypoint(pBot->prev_wpt_index.get(), WptT::door, WptT::dooruse) ||
			(pBot->pEdict->v.movetype == MOVETYPE_FLY) || pBot->IsBehaviour(BOT_CROUCHED) || pBot->IsBehaviour(BOT_PRONED) ||
			(pBot->GetMoveSpeed() == MoveSpeed::slow) || (pBot->GetMoveSpeed() == MoveSpeed::slowest))
		{

			//			vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

			// TO DO - Need to test somehow if the bot is moving forward (maybe something like moved_distance in bot.cpp)
			//		before doing any WPT & PATH resetting

			//			^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^



			// let's give bot another 10 seconds to reach this waypoint before resetting the navigation
			if (pBot->NotReachedCurrWaypointFor(15.0f))
			{
				unreachable_wpt_index = pBot->curr_wpt_index;

				if (botdebugger.IsDebugPaths() || botdebugger.IsDebugStuck() || botdebugger.IsDebugWaypoints())
				{
					sprintf(dbgmsg, "<<WARNING>> extra time to reach waypoint #%d is over -> going to RESET NAVIG!\n", pBot->curr_wpt_index + 1);
					conOutput.Notify(dbgmsg, pBot);
				}
			}
#ifdef _DEBUG
			else
			{
				//@@@@@@@@@@@@@@@@
				//if (botdebugger.IsDebugStuck() || botdebugger.IsDebugPaths() || botdebugger.IsDebugWaypoints())
				//	conOutput.Notify("(nav.cpp)UNREACHABLE WPT -> the door OR ladder OR crouch OR prone OR slow/slower moveSpeed -> default (5s) timeToReachIt is over!\n");
			}
#endif
		}
		// is bot heading towards a waypoint with small range (ie. move speed can be slow or slowest)?
		else if (pBot->IsBehaviour(BOT_PRECISION))
		{
			// is he moving down the hill/stairs/whatever (ie. definitely moving at slow/slowest speed now)?
			if ((pBot->IsNeed(NEED_RESETNAVIG) == false) && (pEdict->v.v_angle.x > 0))
			{
				// then by updating the reach time bot gains another 5 seconds so this way there's 15 seconds in total ... like if he got stuck (see above)
				pBot->SetTimeToReachCurrWaypoint();
				pBot->SetNeed(NEED_RESETNAVIG);

				if (botdebugger.IsDebugPaths() || botdebugger.IsDebugStuck() || botdebugger.IsDebugWaypoints())
				{
					sprintf(dbgmsg, "<<WARNING>> moving downhill while heading to waypoint #%d -> setting extra time to reach it!\n", pBot->curr_wpt_index + 1);
					conOutput.Notify(dbgmsg, pBot);
				}
			}
			else
			{
				// bot had 10 seconds to reach this waypoint, but failed, so it's time to reset the navigation
				if (pBot->NotReachedCurrWaypointFor(10.0f))
				{
					unreachable_wpt_index = pBot->curr_wpt_index;

					if (botdebugger.IsDebugPaths() || botdebugger.IsDebugStuck() || botdebugger.IsDebugWaypoints())
					{
						sprintf(dbgmsg, "<<WARNING>> extra 'precision' time to reach waypoint #%d is over -> going to RESET NAVIG!\n", pBot->curr_wpt_index + 1);
						conOutput.Notify(dbgmsg, pBot);
					}
				}
#ifdef _DEBUG
				else
				{
					//@@@@@@@@@@@@@@@@
					//if (botdebugger.IsDebugStuck() || botdebugger.IsDebugPaths() || botdebugger.IsDebugWaypoints())
					//	conOutput.Notify("(nav.cpp)UNREACHABLE WPT -> the PRECISION case -> default (5s) timeToReachIt is over!\n", pBot);
				}
#endif
			}
		}
		// otherwise the bot must be stuck at this waypoint (unable to reach it)
		else
		{
			unreachable_wpt_index = pBot->curr_wpt_index;

#ifdef _DEBUG
			//@@@@@@@@@@@@@@@@
			//if (botdebugger.IsDebugStuck())
			//	conOutput.Notify("(nav.cpp)DEFAULT timeToReachIt(+5.0) is over - can't reach this wpt in TIME -> going to RESET NAVIG!\n", pBot);
#endif

		}

		// is this waypoint unreachable?
		if (unreachable_wpt_index != NO_VAL)
		{
			if (botdebugger.IsDebugPaths() || botdebugger.IsDebugStuck() || botdebugger.IsDebugWaypoints())
			{				
				sprintf(dbgmsg, "<<POSSIBLE BUG IN WAYPOINTS>>Can't get to wpt #%d in time -> Navigation RESET (wpts may be too far from each other)\n", pBot->curr_wpt_index + 1);
				conOutput.Notify(dbgmsg, pBot);
				util.DebugInFile(dbgmsg);
			}

			// bot can't reach this waypoint so clear it
			pBot->ClearCurrentWaypoint();

			// update path history
			pBot->prev_path_index = pBot->curr_path_index;
			pBot->curr_path_index = NO_VAL;

			waypoint_penalty[unreachable_wpt_index] += 100.f;
		}
	}

	// bot has no waypoint?
	if (pBot->curr_wpt_index == NO_VAL)
	{
		// then search for one
		found = wptmanager.FindNewWaypointForBot(pBot, unreachable_wpt_index);

		// didn't find any accessible waypoint?
		if (found == NO_VAL)
		{
			// then clear all previously visited waypoints
			pBot->prev_wpt_index.clear();

			// and try again
			found = wptmanager.FindNewWaypointForBot(pBot, unreachable_wpt_index);

			// still no waypoint?
			if (found == NO_VAL)
			{
				pBot->ClearCurrentWaypoint();
				
				// then we have nothing to head towards and we have to "return error"
				return false;
			}
		}

		if (botdebugger.IsDebugWaypoints() || botdebugger.IsDebugStuck())
		{
			sprintf(dbgmsg, "***Have no waypoint! Found this one (index=%d) as a new waypoint\n", found + 1);
			conOutput.Notify(dbgmsg, pBot);
		}

		pBot->SetCurrentWaypoint(found);
	}

	// the bot already reached his current waypoint where he already finished given action (e.g. waiting)
	// so we must skip standard position checks in order to prevent doing unwanted turn backs in case when he actually ran slightly past the waypoint (for example when he went prone, because going prone
	// isn't instant stop, but the body still has some forward motion, then it looks really weird if the bot does 180 degree turn to crawl back to his waypoint followed by another 180 degree turn
	// to move forward to his next waypoint)
	if (pBot->IsNeed(NEED_NEXTWPT))
	{
		// we just tell him he is exactly in waypoint range
		in_wpt_range = TRUE;
		past_the_wpt = FALSE;

		// and that he is facing his current waypoint by resetting the time
		pBot->SetFaceWaypointTime(-1.0f);
	}
	// otherwise the bot is still heading towards his waypoint so he must keep checking his current position and compare it to waypoint position
	else
	{
		// 3D distance (space - ie we check also z-coord)
		wpt_distance = (pEdict->v.origin - pBot->GetCurrWptPosition()).Length();

		// 2D distance (planar - ie we ignore z-coord)
		wpt_distance2D = (pEdict->v.origin - pBot->GetCurrWptPosition()).Length2D();

		// get this waypoint range, but if it is a ladder then always reset the range to default small range mainly due to the z-coord check, because we always must check in 3D space on this waypoint type
		if (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::ladder))
			wpt_range = WPT_RANGE_SMALL;
		else
			wpt_range = waypoints[pBot->curr_wpt_index].range;

		// if the waypoint range is really small then make it a bit larger for the test and use only planar distance to test whether the bot is in range or not
		// this trick handles the unwanted dancing around waypoints with range of 5 units
		if (wpt_range <= 10.0f)
		{
			wpt_range = 10.0f;
			use_only_planar = true;
		}
		else
			use_only_planar = false;

		// if the bot is jumping over something while running away from planted explosives charge then use only planar distance to prevent returning to the waypoints the bot may have passed while in jump
		if (pBot->IsTask(TASK_CLAY_EVADE) && pBot->IsDoingDuckJumpNow())
			use_only_planar = true;

		// is bot heading towards a claymore waypoint AND isn't in the client area where the charge gets activated? ... then use only planar distance, because in DoD some objects
		// that need to be destroyed can also change map geometry and once they are broken this waypoint may stay high above bot so it's unreachable then (eg. dod_charlie)
		if (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::claymore) && (pBot->IsSubTask(ST_INAREA) == false))
			use_only_planar = true;

		in_wpt_range = FALSE;
		past_the_wpt = FALSE;

		// are we close enough to a target waypoint (ie are we in current waypoint range)
		if (wpt_distance <= wpt_range)
		{
			in_wpt_range = TRUE;



#ifdef _DEBUG
			// this isn't meant to be public message
			if (botdebugger.IsDebugPaths() || botdebugger.IsDebugWaypoints() || botdebugger.IsDebugStuck())
			{
				if (debug_reaching_range)
				{
					sprintf(dbgmsg, "***SET bot IS IN RANGE for wpt #%d DistToWpt=%.2f <= (WptRange=%.2f)@@@ CORRECT RANGE @@@\n", pBot->curr_wpt_index + 1, wpt_distance, wpt_range);
					conOutput.Notify(dbgmsg);
				}
			}
#endif


		}

		// did the bot run past waypoint with quite small range? (prevent the loop-the-loop problem)
		else if ((pBot->GetPrevDistToCurrentWaypoint() > 1.0f) && (wpt_distance > pBot->GetPrevDistToCurrentWaypoint()) && (wpt_range < WPT_RANGE) && (pBot->GetMoveSpeed() == MoveSpeed::max))
		{
			in_wpt_range = TRUE;

			past_the_wpt = TRUE;	// bot just run past his current waypoint


#ifdef _DEBUG
			// this isn't meant to be public message
			if (botdebugger.IsDebugPaths() || botdebugger.IsDebugWaypoints() || botdebugger.IsDebugStuck())
			{
				if (debug_reaching_range)
				{
					sprintf(dbgmsg, "***SET bot run PAST THE WPT for wpt #%d DistToWpt=%.2f > (prevWptDist=%.2f)$$$$$$$$$\n",
						pBot->curr_wpt_index + 1, wpt_distance, pBot->GetPrevDistToCurrentWaypoint());
					conOutput.Notify(dbgmsg);
				}
			}
#endif



		}

		// we are NOT in 3D range, but only in 2D/planar range
		else if ((wpt_distance > wpt_range) && (wpt_distance2D <= wpt_range))
		{
			// if it is a ladder waypoint we are NOT in range yet
			if (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::ladder))
			{
				in_wpt_range = FALSE;
			}
			else
			{
				// check if waypoint z origin (up/down) is in some limits (45 is jump limit)
				// is waypoint higher than jump limit (bot is under waypoint and can't jump to it) OR lower (bot is above the waypoint and must fall to it - a ditch for example)?
				if (((waypoints[pBot->curr_wpt_index].origin.z > (pEdict->v.origin.z + 45.0f)) || (waypoints[pBot->curr_wpt_index].origin.z < (pEdict->v.origin.z - 45.0f))) && (use_only_planar == false))
				{
					in_wpt_range = FALSE;


#ifdef _DEBUG
					// this isn't meant to be public message
					if (botdebugger.IsDebugPaths() || botdebugger.IsDebugWaypoints() || botdebugger.IsDebugStuck())
					{
						if (debug_reaching_range)
						{
							sprintf(dbgmsg, "***IN PLANAR RANGE -> RESET IN RANGE for wpt #%d DistToWpt=%.2f <= (WptRange=%.2f)@@@@@@\n", pBot->curr_wpt_index + 1, wpt_distance, wpt_range);
							conOutput.Notify(dbgmsg);
						}
					}
#endif



				}
				// otherwise bot already is inside those limits
				else
				{
					in_wpt_range = TRUE;



#ifdef _DEBUG
					// this isn't meant to be public message
					if (botdebugger.IsDebugPaths() || botdebugger.IsDebugWaypoints() || botdebugger.IsDebugStuck())
					{
						if (debug_reaching_range)
						{
							sprintf(dbgmsg, "***IN PLANAR RANGE -> SET IN RANGE for wpt #%d DistToWpt=%.2f <= (WptRange=%.2f)@@@ PLANAR RANGE @@@\n", pBot->curr_wpt_index + 1, wpt_distance, wpt_range);
							conOutput.Notify(dbgmsg);
						}
					}
#endif


				}
			}
		}

		// save current distance as previous unless it's a cross waypoint, for cross waypoint we must reset it, because that one has no true range
		if (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::cross))
			pBot->SetPrevDistToCurrentWaypoint(0.0f);
		else
			pBot->SetPrevDistToCurrentWaypoint(wpt_distance);

		// bot reached his waypoint but is still facing it
		if ((pBot->IsNotTurningToFaceWaypoint() == false) && (wpt_distance2D <= wpt_range))
		{
			// stop it
			pBot->SetFaceWaypointTime(-1.0f);

#ifdef _DEBUG
			// this isn't meant to be public message
			if (botdebugger.IsDebugPaths() || botdebugger.IsDebugWaypoints() || botdebugger.IsDebugStuck())
			{
				sprintf(dbgmsg, "***Reached the wpt & Turn time cleared on waypoint %d\n", pBot->curr_wpt_index + 1);
				conOutput.Notify(dbgmsg, pBot);
			}
#endif

		}
	}

	if (in_wpt_range && pBot->IsNotTurningToFaceWaypoint())
	{
		bool waypoint_found = FALSE;

		pBot->SetPrevDistToCurrentWaypoint(0.0f);

		if (past_the_wpt)
		{
			// if this waypoint range is quite small and the bot was moving too fast and ran through the range and is past it now then give him next game frame so he can try it again
			// sadly this will increase the number of cases when the bot is "dancing" around the waypoint, but small range has a purpose so we must ensure the bot is careful there
			if ((wpt_distance > wpt_range) && (wpt_range < WPT_RANGE))
			{
				in_wpt_range = FALSE;
				waypoint_found = TRUE;		// don't look for a new waypoint yet
				pBot->SetMoveSpeed(MoveSpeed::slow);
			}
		}

		if (in_wpt_range && (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::cross) == false))
			AssignWaypointBasedBehaviourUponReachingIt(pBot, waypoint_found);

		// is it time to look for a new waypoint?
		if (waypoint_found == FALSE)
		{
			// is the bot running away from the blast AND did he reach next waypoint? (ie. really got in a range of next waypoint, not just finished the waypoint action and still being at current waypoint)
			if (pBot->IsTask(TASK_CLAY_EVADE) && (pBot->IsNeed(NEED_NEXTWPT) == false))
			{
				// then we can safely reset the evasion task
				pBot->RemoveTask(TASK_CLAY_EVADE);
			}

			// we are going to call for next waypoint so we must reset these needs
			pBot->RemoveNeed(NEED_NEXTWPT);
			pBot->RemoveNeed(NEED_RESETNAVIG);

			//kota@ the bot reaches the next wpt
            //It is the best place update wpt history
			UpdateWptHistory(pBot);

			// init it first
			is_next_wpt = FALSE;
		
			// don't call path navigation for cross waypoints, this saves some CPU time on going through paths trying to find this (cross) waypoint on any of them
			if (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::cross) == false)
			{
				int result;

				result = BotOnPath(pBot);

				if (result != -1)
				{
					is_next_wpt = TRUE;
				}
				else
					is_next_wpt = FALSE;
			}


			// if the bot isn't on any path or is at the end of his current path (is heading towards to cross waypoint) then find a new waypoint
			if (is_next_wpt == FALSE)
			{
				// clear current path index, because the bot isn't on any path now
				if (pBot->curr_path_index != NO_VAL)
					pBot->curr_path_index = NO_VAL;


#ifdef DEBUG
				if (debug_reaching_range)
				{
					char dm[256];
					char wpt_flags[128]{};
					wptmanager.GetWaypointName(pBot->curr_wpt_index, wpt_flags);
					sprintf(dm, "OnPath() returned FALSE for wpt %d <%s> -> calling FindWaypoint() NOW!!!!!!!\n", pBot->curr_wpt_index + 1, wpt_flags);
					conOutput.Notify(dm);
				}
#endif // DEBUG


				is_next_wpt = BotFindWaypoint(pBot, FALSE);
			}

			// NOTE: DON'T DELETE THIS
			// check wpt time and if there is some set it
			//if (waypoints[pBot->prev_wpt_index[0]].flags & W_FL_USE)
			//{
				/*create global array for use wpts and check last time the use wpt was used,
				special slot in wpt struct could hold this use time*/

			//	;	// don't set wpt_wait_time
			//}

			// no waypoint to continue to
			if (is_next_wpt == FALSE)
			{
				pBot->ClearCurrentWaypoint();  // indicate no waypoint found

				// clear all previous waypoints
				pBot->prev_wpt_index.clear();

				if (botdebugger.IsDebugWaypoints() || botdebugger.IsDebugStuck())
					conOutput.Notify("<<BUG IN WAYPOINTS>>***No waypoint to head to - clearing previous waypoints history!\n", pBot);

				return false;
			}
		}
	}

	// check if the bot is proned AND current waypoint is NOT prone waypoint, prevent false prone (the only exception to this is claymore evasion)
	if (pBot->IsBehaviour(BOT_PRONED) && (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::prone, WptT::cross) == false))
	{
		pBot->GoProne("HeadTowardWpt() -> Bot in prone but NOT at PRONE WPT");
	}
	// check if the bot is crouched AND current waypoint is NOT crouch waypoint
	if (pBot->IsBehaviour(BOT_CROUCHED) && (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::crouch, WptT::cross) == false))
	{
		pBot->SetStance(GOTO_STANDING, "bot_navigate.cpp|HeadTowardWpt() -> GOTO standing because NOT at crouch wpt");
	}
	// is bot heading towards the ammobox AND is going to take some magazines from it AND is he quite close to it?
	if (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::ammobox) && (pBot->IsTakeAmmoForMainWeapon() || pBot->IsTakeAmmoForBackupWeapon()) &&
		(pBot->GetMoveSpeed() > MoveSpeed::slow) && (wptmanager.GetDistanceToWaypoint(pEdict, pBot->curr_wpt_index) < (WPT_RANGE * 2.0f)))
	{
		// then slow down
		pBot->SetMoveSpeed(MoveSpeed::slow);
	}

	// is bot heading towards door or shoot or use or ladder (while not on it yet) AND is quite close? then slow down
	if ((wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::door, WptT::dooruse) || wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::shoot, WptT::use) ||
		(wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::ladder) && (pEdict->v.movetype != MOVETYPE_FLY))) &&
		(pBot->GetMoveSpeed() > MoveSpeed::slow) && (wptmanager.GetDistanceToWaypoint(pEdict, pBot->curr_wpt_index) < (WPT_RANGE * 2.0f)))
	{
		pBot->SetMoveSpeed(MoveSpeed::slow);
	}

	// is bot heading to claymore waypoint AND is equipped with a claymore mine AND isn't evading another claymore AND is quite close to the waypoint? then slow down
	if (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::claymore) && pBot->IsEquippedWithExplosiveCharge() && (pBot->IsTask(TASK_CLAY_EVADE) == false) &&
		(pBot->GetMoveSpeed() > MoveSpeed::slow) && (wptmanager.GetDistanceToWaypoint(pEdict, pBot->curr_wpt_index) < (WPT_RANGE * 2.0f)))
	{
		pBot->SetMoveSpeed(MoveSpeed::slow);
	}

	// bot is in DoD specific Client Area...
	if (pBot->IsSubTask(ST_INAREA) && (pBot->IsTask(TASK_CLAY_EVADE) == false))
	{
		// and it's an area of some common breakable object (a must when the range of the waypoint is reaching into the object and the bot isn't able to get to the generated point within this range)
		if (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::claymore) && (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::pushpoint) == false))
		{
			if (pBot->IsEquippedWithExplosiveCharge())
			{
				pBot->ResetAims("HeadTowardWpt() -> Got INAREA while still heading to CLAYMORE wpt");
				pBot->SetWaitTime(5.0f);
				// let the bot know he'll have to run away from this explosives charge and ignore any area he gets into till he reaches next waypoint (the in area HUD icon isn't instant so this task
				// is crucial to make him ignore the in area presence otherwise he would have ran back into the blast, eg. the entrance to supply room on map Glider - two areas right next to each other)
				pBot->SetTask(TASK_CLAY_EVADE);
			}

			// is it a combination of Roadblock and Claymore waypoint AND is the route still blocked? then make the bot return back
			if (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::roadblock) && (wptmanager.IsPairedWaypointReachableForThisBot(pBot, WptT::roadblock) == false))
			{
				if (pBot->ReturnBackToPathStart("HeadTowardWpt() -> Got INAREA while still heading to CLAYMORE wpt"))
					pBot->SetNeed(NEED_NEXTWPT);
			}
		}
		// but it isn't near common breakable object for explosives charge so... check whether bot doesn't do any action yet AND there is a Capture Area AND bot is able to capture it
		// (this filters out cases when bot reached the pushpoint waypoint and already decided about this situation)
		else if (pBot->NotBeenWaitingFor(0.2f) && util.IsCaptureAreaNearby(pBot, STANDARD_SEARCH_RADIUS) && util.CanBotCaptureTheArea(pBot))
		{
			// is the bot on heading to the pushpoint waypoint?
			if (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::pushpoint) && (RANDOM_LONG(1, 100) < 95))
			{
				// by setting zero wait-time we will make him ignore the inArea status in most of the time so that he really starts capturing this objective after reaching the waypoint itself
				pBot->SetWaitTime(0.0f);
			}
			// is the bot on path heading to the pushpoint, but current waypoint isn't pushpoint yet and this capture area needs only one man to capture?
			else if (wptmanager.IsPath(pBot->curr_path_index, PathT::goal_team_one_tag, PathT::goal_team_two_tag) && (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::pushpoint) == false) &&
				(RANDOM_LONG(1, 100) > 50) &&
				(((dodCaptureArea->GetTeamOnePlayersToCapture(dodCaptureArea->FindPointInArray(pBot->GetPointerToGEnt())) == 1) && pBot->IsBotTeam(teamONE.GetTeamId())) ||
					((dodCaptureArea->GetTeamTwoPlayersToCapture(dodCaptureArea->FindPointInArray(pBot->GetPointerToGEnt())) == 1) && pBot->IsBotTeam(teamTWO.GetTeamId()))))
			{
				pBot->SetTask(TASK_CLAY_EVADE); // then we will randomly ignore the presence in the area in order to start capturing this objective when the bot finally gets to the pushpoint
			}
			// otherwise try to help capture it
			else
			{
				pBot->ResetAims("HeadTowardWpt() -> passing by Capture Area");
				pBot->SetWaitTime(dodCaptureArea->GetTimeToCapture(dodCaptureArea->FindPointInArray(pBot->GetPointerToGEnt())));
				pBot->SetTask(TASK_PARACHUTE);

				if ((pBot->IsProne() == false) && pBot->IsNotGoingProne())
					pBot->SetStance(GOTO_CROUCH, "HeadTowardsWpt() -> passing by -> GOTO crouch to capture CP");
			}
		}
	}

	// if next waypoint isn't a sniper waypoint (checking cross waypoint too to carry the "hold position" behaviour over the junction)
	if (pBot->IsTask(TASK_DONTMOVEINCOMBAT) && (wptmanager.IsWaypoint(pBot->curr_wpt_index, WptT::sniper, WptT::cross) == false))
	{
		// did the bot just pass sniper waypoint (is still in its range, but is already heading towards next waypoint)
		if (wptmanager.IsWaypoint(pBot->prev_wpt_index.get(), WptT::sniper) && (wptmanager.GetDistanceToWaypoint(pEdict, pBot->prev_wpt_index.get()) < waypoints[pBot->prev_wpt_index.get()].range))
			;	// then don't remove this task yet
		else
		{
			// the bot isn't in a vicinity of sniper waypoint so clear the task and allow the bot to move in next combat
			pBot->RemoveTask(TASK_DONTMOVEINCOMBAT);

			if (botdebugger.IsDebugActions() || botdebugger.IsDebugStuck())
				conOutput.Notify("***Not at sniper waypoint so free to move in combat again\n", pBot);
		}
	}

	// is bot tasked to ignore waypoint navigation? then do nothing (ie. don't try to face current waypoint)
	if (pBot->IsTask(TASK_IGNOREWPTNAV))
		;
	// is the bot NOT on a ladder? then make him keep turning towards the waypoint (not just turn, but fully aim ... including the pitch angles)
	else if (pEdict->v.movetype != MOVETYPE_FLY)
	{
		Vector v_direction = pBot->GetCurrWptPosition() - pEdict->v.origin;
		Vector v_angles = UTIL_VecToAngles(v_direction);

		pEdict->v.idealpitch = -v_angles.x;
		BotFixIdealPitch(pEdict);

		pEdict->v.ideal_yaw = v_angles.y;
		BotFixIdealYaw(pEdict);
	}
	// is the bot still on the ladder, but already reached its end? then allow him to turn to next waypoint, but don't change the pitch angle yet, because he still needs to keep climbing the ladder to exit it
	else if ((pEdict->v.movetype == MOVETYPE_FLY) && pBot->HasReachedEndOfLadder())
	{
		Vector v_direction = pBot->GetCurrWptPosition() - pEdict->v.origin;
		Vector v_angles = UTIL_VecToAngles(v_direction);

		pEdict->v.ideal_yaw = v_angles.y;
		BotFixIdealYaw(pEdict);
	}

	return true;
}


/*
* bots ladder behaviour, all from facing it to leaving it on the other end
*/
bool BotHandleLadder(bot_t *pBot, float moved_distance)
{
	edict_t *pent = NULL;
	edict_t *pEdict = pBot->pEdict;
	int end_wpt_index = NO_VAL;				// index of the waypoint at the other end of the ladder
	float end_wpt_distance;					// distance to that waypoint
	bool bot_stuck_on_ladder;

	// did the bot just touch the ladder?
	if (pBot->LadderDirectionNotDecidedYet() && (pEdict->v.movetype == MOVETYPE_FLY))
		pBot->SetStartOfUsingLadder();	// remember the exact moment in order to deal with the possibility of getting stuck

	// has the bot no ladder end yet?
	if (end_wpt_index == NO_VAL)
	{
		end_wpt_index = pBot->curr_wpt_index;

		/**/
#ifdef _DEBUG
		if (botdebugger.IsDebugPaths() || botdebugger.IsDebugWaypoints())
		{
			char msg[TEXT_MSG_SIZE]{};
			sprintf(msg, "Bot HandleLadder() -> ladder end wpt index: <%d>\n", pBot->curr_wpt_index + 1);
			conOutput.Notify(msg, pBot);
		}
#endif
		/**/
	}

	// the bot doesn't know which direction should he climb this ladder yet
	if (pBot->LadderDirectionNotDecidedYet())
	{
		Vector v_src, v_dest, view_angles;
		TraceResult tr;
		float angle = 0.0f;
		bool done = false;

		// do we have an end point of this ladder?
		if (end_wpt_index != NO_VAL)
		{
			if (waypoints[end_wpt_index].origin.z < pEdict->v.origin.z)
				pBot->SetLadderUseDirection(LadderDir::climb_down);
			else if (waypoints[end_wpt_index].origin.z > pEdict->v.origin.z)
				pBot->SetLadderUseDirection(LadderDir::climb_up);
			else
				pBot->SetLadderUseDirection(LadderDir::unknown);
		}
		// otherwise use the ladder origin to tell the bot the right direction
		else
		{
			while ((pent = util.FindEntityInSphere(pent, pEdict->v.origin, STANDARD_SEARCH_RADIUS)) != NULL)
			{
				if (util.IsEntityName(pent, "func_ladder"))
				{
					Vector ladder_origin = util.VecBModelOrigin(pent);
					
					if (ladder_origin.z < pEdict->v.origin.z)
					{
						pBot->SetLadderUseDirection(LadderDir::climb_down);
					}
					else if (ladder_origin.z > pEdict->v.origin.z)
					{
						pBot->SetLadderUseDirection(LadderDir::climb_up);
					}
					else
					{
						pBot->SetLadderUseDirection(LadderDir::unknown);
					}
					
					break;
				}
			}
		}

/**/
#ifdef _DEBUG
		if (botdebugger.IsDebugPaths() || botdebugger.IsDebugWaypoints())
		{
			if (pBot->LadderDirectionNotDecidedYet())
				conOutput.Notify("Bot HandleLadder() -> unknown ladder direction\n", pBot);
			else if (pBot->IsClimbLadderDown())
				conOutput.Notify("Bot HandleLadder() -> climb it down\n", pBot);
			else if (pBot->IsClimbLadderUp())
				conOutput.Notify("Bot HandleLadder() -> climb it up\n", pBot);
		}
#endif
/**/
		
		// also try to square up the bot on the ladder
		while ((!done) && (angle < 180.0f))
		{
			// try looking in one direction (forward + angle)
			view_angles = pEdict->v.v_angle;
			view_angles.y = pEdict->v.v_angle.y + angle;

			if (view_angles.y < 0.0f)
				view_angles.y += 360.0f;
			if (view_angles.y > 360.0f)
				view_angles.y -= 360.0f;

			UTIL_MakeVectors(view_angles);

			v_src = pEdict->v.origin + pEdict->v.view_ofs;
			v_dest = v_src + gpGlobals->v_forward * 30;

			UTIL_TraceLine( v_src, v_dest, dont_ignore_monsters, pEdict->v.pContainingEntity, &tr);

			if (tr.flFraction < 1.0f)  // hit something?
			{
				if (util.IsEntityName(tr.pHit, "func_ladder"))
				{
					done = true;

					//@@@@@@@
					//ALERT(at_console, "FACE THE LADDER\n");
				}
				//else if (strcmp("func_wall", STRING(tr.pHit->v.classname)) == 0)
				if (util.IsEntityName(tr.pHit, "func_wall"))
				{
					// square up to the wall...
					view_angles = UTIL_VecToAngles(tr.vecPlaneNormal);

					// Normal comes OUT from wall, so flip it around...
					view_angles.y += 180.0f;

					if (view_angles.y > 180.0f)
						view_angles.y -= 360.0f;

					pEdict->v.ideal_yaw = view_angles.y;
					//BotFixIdealYaw(pEdict);
					
					done = true;

					//@@@@@@@
					//ALERT(at_console, "HIT SOMETHING - Flip it around\n");
				}
			}
			else
			{
				// try looking in the other direction (forward - angle)
				view_angles = pEdict->v.v_angle;
				view_angles.y = pEdict->v.v_angle.y - angle;

				if (view_angles.y < 0.0f)
					view_angles.y += 360.0f;
				if (view_angles.y > 360.0f)
					view_angles.y -= 360.0f;

				UTIL_MakeVectors( view_angles );

				v_src = pEdict->v.origin + pEdict->v.view_ofs;
				v_dest = v_src + gpGlobals->v_forward * 30;
				
				UTIL_TraceLine( v_src, v_dest, dont_ignore_monsters, pEdict->v.pContainingEntity, &tr);

				if (tr.flFraction < 1.0f)  // hit something?
				{
					if (util.IsEntityName(tr.pHit, "func_wall"))
					{
						// square up to the wall...
						view_angles = UTIL_VecToAngles(tr.vecPlaneNormal);

						// Normal comes OUT from wall, so flip it around...
						view_angles.y += 180.0f;

						if (view_angles.y > 180.0f)
							view_angles.y -= 360.0f;

						pEdict->v.ideal_yaw = view_angles.y;
						BotFixIdealYaw(pEdict);

						//@@@@@@@
						//ALERT(at_console, "DIDN'T HIT - Flip it around\n");

						done = true;
					}
				}
			}

			angle += 10.0f;
		}

		if (!done)  // if didn't find a wall, just reset ideal_yaw
		{
			// set ideal_yaw to current yaw (so bot won't keep turning)
			pEdict->v.ideal_yaw = pEdict->v.v_angle.y;
			BotFixIdealYaw(pEdict);
		}
	}

	end_wpt_distance = 0.0f;
	bot_stuck_on_ladder = false;

	// the bot must be stuck on ladder
	if (pBot->IsTimeSinceStartOfUsingLadder(2.0f) && (moved_distance <= 1.0f) && (pBot->GetPrevMoveSpeed() != MoveSpeed::stop) && (pBot->GetLadderUseDirection() > LadderDir::unknown))
	{
		// is the bot stuck on this ladder for quite some time?
		// then get off it no matter if that would mean breaking a leg or even death we basically need to free this ladder
		if (pBot->IsTimeSinceStartOfUsingLadder(4.0f))		// was 10.0
		{
			pEdict->v.button |= IN_JUMP;

			if (botdebugger.IsDebugStuck())
			{
				conOutput.Notify("***STUCK*** on ladder --> jumping off it!\n", pBot);
			}
		}

		Vector vecStart, vecEnd;
		TraceResult tr;

		UTIL_MakeVectors(pEdict->v.v_angle);
		vecStart = pEdict->v.origin + pEdict->v.view_ofs;

		// centre and up (ie. above the bot's head)
		if (pBot->IsClimbLadderUp())
			vecEnd = vecStart + gpGlobals->v_up * 50.0f;
		// centre and down (ie. below bot's feet)
		else
			vecEnd = pEdict->v.origin + gpGlobals->v_up * -70.0f;
		
		UTIL_TraceLine(vecStart, vecEnd, dont_ignore_monsters, pEdict, &tr);



		//@@@@@@@@@!!!!!!!!!@@@@@@@@
		/*
		#ifdef _DEBUG
		extern edict_t *listenserver_edict;
		DevDrawBeam(listenserver_edict, vecStart, vecEnd, 10, 250, 10);
		#endif
		/**/




		// check if the centre trace didn't hit anything
		// then we need to check both shoulders/feet to know which side is blocked
		if (tr.flFraction >= 1.0f)
		{
			// right side and up
			if (pBot->IsClimbLadderUp())
			{
				vecStart = pEdict->v.origin + pEdict->v.view_ofs + gpGlobals->v_right * 30.0f;
				vecEnd = vecStart + gpGlobals->v_up * 50.0f;
			}
			// right side and down
			else
			{
				vecStart = pEdict->v.origin + gpGlobals->v_right * 30.0f;
				vecEnd = vecStart + gpGlobals->v_up * -70.0f;
			}

			UTIL_TraceLine(vecStart, vecEnd, dont_ignore_monsters, pEdict, &tr);



			//@@@@@@@@@!!!!!!!!!@@@@@@@@
			/*
			#ifdef _DEBUG
			extern edict_t *listenserver_edict;
			DevDrawBeam(listenserver_edict, vecStart, vecEnd, 10, 250, 10);
			#endif
			/**/

			// didn't the right trace hit anything?
			// then try the other side
			if (tr.flFraction >= 1.0f)
			{
				// left side and up
				if (pBot->IsClimbLadderUp())
				{
					vecStart = pEdict->v.origin + pEdict->v.view_ofs + gpGlobals->v_right * -30.0f;
					vecEnd = vecStart + gpGlobals->v_up * 50.0f;
				}
				// left side and down
				else
				{
					vecStart = pEdict->v.origin + gpGlobals->v_right * -30.0f;
					vecEnd = vecStart + gpGlobals->v_up * -70.0f;
				}

				UTIL_TraceLine(vecStart, vecEnd, dont_ignore_monsters, pEdict, &tr);



				//@@@@@@@@@!!!!!!!!!@@@@@@@@
				/*
				#ifdef _DEBUG
				extern edict_t *listenserver_edict;
				DevDrawBeam(listenserver_edict, vecStart, vecEnd, 10, 250, 10);
				#endif
				/**/


				// didn't the left trace hit anything? ----->>>>> PROBLEM 
				// (maybe we can try to check backs ie. v_forward * -30 and up/down)
				//
				// NOTE: this doesn't seems to be a big problem, bots were able to unstuck in those few tests I ran
				if (tr.flFraction >= 1.0f)
				{


#ifdef _DEBUG
					if (botdebugger.IsDebugPaths() || botdebugger.IsDebugWaypoints())
					{
						char msg[TEXT_MSG_SIZE];
						sprintf(msg, "Bot HandleLadder() - stuck on it -> PROBLEM is all 3 tracers did hit something!!! -> the bot may jump off it\n");
						conOutput.Notify(msg, pBot);
						util.DebugDev(msg, pBot->curr_wpt_index, pBot->curr_path_index);
					}
#endif


				}
				// the left trace did hit something
				else
				{
					// so try to strafe right then
					pBot->StrafeRightFor(0.3f);//		was 0.1
					
					
					//@@@@@@@@@
					//#ifdef _DEBUG
					//ALERT(at_console, "stuck on ladder --- left trace hit something!!!\n");
					//#endif
				}

			}
			// the right trace did hit something
			else
			{
				// so try to strafe left then
				pBot->StrafeLeftFor(0.3f);//			was 0.1


				//@@@@@@@@@
				//#ifdef _DEBUG
				//ALERT(at_console, "stuck on ladder --- right trace hit something!!!\n");
				//#endif
			}
		}
		// the centre trace did hit something
		else
		{
			// can we return back?
			if (pBot->ReturnBackToPathStart())
			{
				// then reset the end waypoint index
				end_wpt_index = NO_VAL;

				// change ladder climb direction
				if (pBot->IsClimbLadderUp())
					pBot->SetLadderUseDirection(LadderDir::climb_down);
				else if (pBot->IsClimbLadderDown())
					pBot->SetLadderUseDirection(LadderDir::climb_up);

				// manually set the last visited waypoint as the one we need to head towards to now, because we know it must be reachable 
				pBot->curr_wpt_index = pBot->prev_wpt_index.get();

				// and break this function right away
				return true;
			}
		}

		bot_stuck_on_ladder = true;
	}

	// did the bot find the end of ladder if so get the distance to the end
	if (end_wpt_index != NO_VAL)
	{
		end_wpt_distance = fabsf(pEdict->v.origin.z - waypoints[end_wpt_index].origin.z);
	}
	/*/
	// otherwise try to get back to prev wpt
	else
	{
		end_wpt_index = pBot->prev_wpt_index.get();

		// if no previous wpt use current wpt
		if (end_wpt_index == NO_VAL)
			end_wpt_index = pBot->curr_wpt_index;
	}
	/**/

	// if bot is close to the end of ladder
	if ((end_wpt_distance > 0.0f) && (end_wpt_distance <= 10.0f))
		pBot->SetReachedEndOfLadder();
	// otherwise bot didn't reach ladder end yet
	else
		pBot->ResetReachedEndOfLadder();

	// climb the ladder up
	if (pBot->IsClimbLadderUp())
	{
		if (wptmanager.IsWaypointTypeTeamPriority(pBot->curr_wpt_index, WptT::ladder, 1, pBot->GetBotTeam()))
		{
			pEdict->v.v_angle.x = -40;
		}
		else
		{
			pEdict->v.v_angle.x = -80;
		}

		pBot->SetMoveSpeed(MoveSpeed::slow);
	}
	// climb the ladder down
	if (pBot->IsClimbLadderDown())
	{
		pEdict->v.v_angle.x = 80;
		pBot->SetMoveSpeed(MoveSpeed::slow);
	}

	// don't move forward when stuck
	if (bot_stuck_on_ladder == FALSE)
		pEdict->v.button |= IN_FORWARD;

	return true;
}


/*
* returns TRUE if bot can't do side steps to the left
* we are checking in which Stance the bot is and then
* we are sending tracelines at important body parts for that Stance
*/
bool BotCantStrafeLeft(edict_t *pEdict)
{
	Vector v_src, v_left;
	TraceResult tr;
	bool trace_head, trace_waist;

	UTIL_MakeVectors(pEdict->v.v_angle);

	trace_head = FALSE;
	trace_waist = FALSE;

	// is bot fully proned send only one traceline
	if (util.IsEdictProne(pEdict))
	{
		trace_head = TRUE;
	}
	// otherwise traceline all
	else
	{
		trace_head = TRUE;
		trace_waist = TRUE;
	}

	// do a trace to the left at head level
	if (trace_head)
	{
		v_src = pEdict->v.origin + pEdict->v.view_ofs;
		v_left = v_src + gpGlobals->v_right * -100;  // 100 units to the left

		UTIL_TraceLine(v_src, v_left, dont_ignore_monsters,	pEdict->v.pContainingEntity, &tr);

		// bot's head will something return can't strafe there
		if (tr.flFraction < 1.0f)
			return true;
	}

	// do a trace to the left at waist level
	if (trace_waist)
	{
		v_src = pEdict->v.origin;
		v_left = v_src + gpGlobals->v_right * -100;  // 100 units to the left

		UTIL_TraceLine(v_src, v_left, dont_ignore_monsters,	pEdict->v.pContainingEntity, &tr);

		if (tr.flFraction < 1.0f)
			return true;
	}

	return false;	// the way is clear return can strafe there
}


/*
* returns TRUE if bot can't do side steps to the right
* we are checking in which Stance the bot is and then
* we are sending tracelines at important body parts for that Stance
*/
bool BotCantStrafeRight(edict_t *pEdict)
{
	Vector v_src, v_left;
	TraceResult tr;
	bool trace_head, trace_waist;

	UTIL_MakeVectors(pEdict->v.v_angle);

	trace_head = FALSE;
	trace_waist = FALSE;

	if (util.IsEdictProne(pEdict))
	{
		trace_head = TRUE;
	}
	else
	{
		trace_head = TRUE;
		trace_waist = TRUE;
	}

	if (trace_head)
	{
		v_src = pEdict->v.origin + pEdict->v.view_ofs;
		v_left = v_src + gpGlobals->v_right * 100;

		UTIL_TraceLine(v_src, v_left, dont_ignore_monsters,	pEdict->v.pContainingEntity, &tr);

		if (tr.flFraction < 1.0f)
			return true;
	}

	if (trace_waist)
	{
		v_src = pEdict->v.origin;
		v_left = v_src + gpGlobals->v_right * 100;

		UTIL_TraceLine(v_src, v_left, dont_ignore_monsters,	pEdict->v.pContainingEntity, &tr);

		if (tr.flFraction < 1.0f)
			return true;
	}

	return false;
}


/*
* returns TRUE if the bot can follow his team leader (ie. the one who "uses" him)
*/
bool BotFollowTeamLeader(bot_t *pBot)
{
	edict_t *pEdict = pBot->pEdict;
	Vector vec_end;
	bool leader_visible = FALSE;
	float f_distance_to_leader;

	// is bot NOT under water?
	if ((pEdict->v.waterlevel != 2) && (pEdict->v.waterlevel != 3))
	{
		// reset pitch to 0 (level horizontally)
		pEdict->v.idealpitch = 0.0f;
		pEdict->v.v_angle.x = 0;
	}
	
	// is team leader dead?
	if (util.IsAlive(pBot->pTeamLeader) == false)
	{
		// no one to follow
		pBot->pTeamLeader = NULL;
		
		return false;
	}

	vec_end = pBot->pTeamLeader->v.origin + pBot->pTeamLeader->v.view_ofs;

	leader_visible = util.IsInViewCone(&vec_end, pEdict) && util.IsVisible(vec_end, pEdict, true);

	// how far away is team leader?
	f_distance_to_leader = (pBot->pTeamLeader->v.origin - pEdict->v.origin).Length();

	// not in FOV and visible so check if team leader is quite close at least
	if (leader_visible == FALSE)
	{
		// if team leader is close then set that he is still visible
		if (f_distance_to_leader < TEAMMATE_SEARCH_RADIUS)
			leader_visible = TRUE;
	}
	
	// check if team leader is still visible
	if (leader_visible || pBot->HasSeenTeamLeaderInLast(2.0f))
	{
		Vector bot_angles, aim_vec;

		// update see team leader time, because team leader is fully visible now
		if (leader_visible)
			pBot->SetSeeTeamLeaderTime();
		
		// don't move if he's close enough
		if (f_distance_to_leader < 115.0f)
		{
			pBot->SetMoveSpeed(MoveSpeed::stop);
			//pBot->SetDontCheckStuck("FollowTeamLeader() -> close enough to him so stop moving");// Too much of spam
			pBot->SetDontCheckStuck();
		}
		// bot got stuck recently, but if he's close enough to team leader then stop (most probably another team member is blocking the way)
		// unstuck code in bot think function has similar condition so we don't want to make conflicts between these two
		else if ((pBot->NotBeenStuckFor(2.0f) == false) && (f_distance_to_leader < 150.0f))
		{
			pBot->SetMoveSpeed(MoveSpeed::stop);
			//pBot->SetDontCheckStuck("FollowTeamLeader() -> got stuck -> but close enough to him so stop moving");// Too much of spam
			pBot->SetDontCheckStuck();
		}
		// walk if not too far from team leader
		else if (f_distance_to_leader < 250.0f)										// try using only 200
			pBot->SetMoveSpeed(MoveSpeed::slow);
		// run to catch team leader
		else
			pBot->SetMoveSpeed(MoveSpeed::max);

		// is bot just standing still?
		if ((pBot->GetMoveSpeed() == MoveSpeed::stop) && (pBot->IsTimeToKeepCurrentAim() == false))
		{
			int direction = RANDOM_LONG(1, 100);

			// we want bot to watch around, but we also want him to never aim at the team leader so...
			// first we will turn the vector from team leader to this bot into angles...
			bot_angles = UTIL_VecToAngles(pEdict->v.origin - pBot->pTeamLeader->v.origin);
			// then make vectors (forward, right) from these angles...
			UTIL_MakeVectors(bot_angles);

			// and now we can make bot...
 
			// turn to right side or
			if (direction <= 33)
			{
				vec_end = pBot->pEdict->v.origin + gpGlobals->v_right * 40;
				aim_vec = vec_end - pBot->pEdict->v.origin;
			}
			// turn to left side or
			else if (direction <= 66)
			{
				// actually there's no left side (only forward, right and up) so we have to use opposite vector to the one we used for right side
				vec_end = pBot->pEdict->v.origin + gpGlobals->v_right * 40;
				aim_vec = pBot->pEdict->v.origin - vec_end;
			}
			// or turn his back to team leader  (... and cover the back)
			else
			{
				vec_end = pBot->pEdict->v.origin + gpGlobals->v_forward * 40;
				aim_vec = vec_end - pBot->pEdict->v.origin;
			}

			// set some time to watch new direction
			pBot->SetTimeToKeepCurrentAim( RANDOM_FLOAT(2.0f, 4.5f) );
			
			// store current aiming vector, we can use current waypoint fake position variable, because we don't use waypoints while following team leader
			pBot->SetCurrWptPosition(aim_vec);
		}
		// otherwise make bot face the team leader in order to follow him
		else if (pBot->GetMoveSpeed() > MoveSpeed::stop)
		{
			aim_vec = pBot->pTeamLeader->v.origin - pEdict->v.origin;
			pBot->SetCurrWptPosition(aim_vec);
		}
		
		bot_angles = UTIL_VecToAngles(pBot->GetCurrWptPosition());
		pEdict->v.ideal_yaw = bot_angles.y;
		BotFixIdealYaw(pEdict);

		return true;
	}
	else
	{
		// team leader has gone out of sight
		pBot->pTeamLeader = NULL;
	}
	
	return false;
}


/*			NOT USING
void BotOnLadder( bot_t *pBot, float moved_distance )
{
   Vector v_src, v_dest, view_angles;
   TraceResult tr;
   float angle = 0.0;
   bool done = FALSE;

   edict_t *pEdict = pBot->pEdict;

   // check if the bot has JUST touched this ladder...
   if (pBot->ladder_dir == LADDER_UNKNOWN)
   {
      // try to square up the bot on the ladder...
      while ((!done) && (angle < 180.0))
      {
         // try looking in one direction (forward + angle)
         view_angles = pEdict->v.v_angle;
         view_angles.y = pEdict->v.v_angle.y + angle;

         if (view_angles.y < 0.0)
            view_angles.y += 360.0;
         if (view_angles.y > 360.0)
            view_angles.y -= 360.0;

         UTIL_MakeVectors( view_angles );

         v_src = pEdict->v.origin + pEdict->v.view_ofs;
         v_dest = v_src + gpGlobals->v_forward * 30;

         UTIL_TraceLine( v_src, v_dest, dont_ignore_monsters,
                         pEdict->v.pContainingEntity, &tr);

         if (tr.flFraction < 1.0)  // hit something?
         {
            if (strcmp("func_wall", STRING(tr.pHit->v.classname)) == 0)
            {
               // square up to the wall...
               view_angles = UTIL_VecToAngles(tr.vecPlaneNormal);

               // Normal comes OUT from wall, so flip it around...
               view_angles.y += 180;

               if (view_angles.y > 180)
                  view_angles.y -= 360;

               pEdict->v.ideal_yaw = view_angles.y;

               BotFixIdealYaw(pEdict);

               done = TRUE;
            }
         }
         else
         {
            // try looking in the other direction (forward - angle)
            view_angles = pEdict->v.v_angle;
            view_angles.y = pEdict->v.v_angle.y - angle;

            if (view_angles.y < 0.0)
               view_angles.y += 360.0;
            if (view_angles.y > 360.0)
               view_angles.y -= 360.0;

            UTIL_MakeVectors( view_angles );

            v_src = pEdict->v.origin + pEdict->v.view_ofs;
            v_dest = v_src + gpGlobals->v_forward * 30;

            UTIL_TraceLine( v_src, v_dest, dont_ignore_monsters,
                            pEdict->v.pContainingEntity, &tr);

            if (tr.flFraction < 1.0)  // hit something?
            {
               if (strcmp("func_wall", STRING(tr.pHit->v.classname)) == 0)
               {
                  // square up to the wall...
                  view_angles = UTIL_VecToAngles(tr.vecPlaneNormal);

                  // Normal comes OUT from wall, so flip it around...
                  view_angles.y += 180;

                  if (view_angles.y > 180)
                     view_angles.y -= 360;

                  pEdict->v.ideal_yaw = view_angles.y;

                  BotFixIdealYaw(pEdict);

                  done = TRUE;
               }
            }
         }

         angle += 10;
      }

      if (!done)  // if didn't find a wall, just reset ideal_yaw...
      {
         // set ideal_yaw to current yaw (so bot won't keep turning)
         pEdict->v.ideal_yaw = pEdict->v.v_angle.y;

         BotFixIdealYaw(pEdict);
      }
   }

   // moves the bot up or down a ladder.  if the bot can't move
   // (i.e. get's stuck with someone else on ladder), the bot will
   // change directions and go the other way on the ladder.

   if (pBot->ladder_dir == LADDER_UP)  // is the bot currently going up?
   {
      pEdict->v.v_angle.x = -60;  // look upwards

      // check if the bot hasn't moved much since the last location...
      if ((moved_distance <= 1) && (pBot->prev_speed >= 1.0))
      {
         // the bot must be stuck, change directions...

         pEdict->v.v_angle.x = 60;  // look downwards
         pBot->ladder_dir = LADDER_DOWN;
      }
   }
   else if (pBot->ladder_dir == LADDER_DOWN)  // is the bot currently going down?
   {
      pEdict->v.v_angle.x = 60;  // look downwards

      // check if the bot hasn't moved much since the last location...
      if ((moved_distance <= 1) && (pBot->prev_speed >= 1.0))
      {
         // the bot must be stuck, change directions...

         pEdict->v.v_angle.x = -60;  // look upwards
         pBot->ladder_dir = LADDER_UP;
      }
   }
   else  // the bot hasn't picked a direction yet, try going up...
   {
      pEdict->v.v_angle.x = -60;  // look upwards
      pBot->ladder_dir = LADDER_UP;
   }

   // move forward (i.e. in the direction the bot is looking, up or down)
   pEdict->v.button |= IN_FORWARD;
}
*/


void BotUnderWater( bot_t *pBot )
{
   bool found_waypoint = FALSE;

   edict_t *pEdict = pBot->pEdict;

   // are there waypoints in this level
   if (num_waypoints > 0)
   {
      // head towards a waypoint
      found_waypoint = BotHeadTowardWaypoint(pBot);
   }

   if (found_waypoint == FALSE)
   {
      // handle movements under water.  right now, just try to keep from
      // drowning by swimming up towards the surface and look to see if
      // there is a surface the bot can jump up onto to get out of the
      // water.  bots DON'T like water!

      Vector v_src, v_forward;
      TraceResult tr;
      int contents;
   
      // swim up towards the surface
      pEdict->v.v_angle.x = -60;  // look upwards
   
      // move forward (i.e. in the direction the bot is looking, up or down)
      pEdict->v.button |= IN_FORWARD;
   
      // set gpGlobals angles based on current view angle (for TraceLine)
      UTIL_MakeVectors( pEdict->v.v_angle );
   
      // look from eye position straight forward (remember: the bot is looking
      // upwards at a 60 degree angle so TraceLine will go out and up...
   
      v_src = pEdict->v.origin + pEdict->v.view_ofs;  // EyePosition()
      v_forward = v_src + gpGlobals->v_forward * 90;
   
      // trace from the bot's eyes straight forward...
      UTIL_TraceLine( v_src, v_forward, dont_ignore_monsters, pEdict->v.pContainingEntity, &tr);
   
      // check if the trace didn't hit anything (i.e. nothing in the way)...
      if (tr.flFraction >= 1.0f)
      {
         // find out what the contents is of the end of the trace...
         contents = UTIL_PointContents( tr.vecEndPos );
   
         // check if the trace endpoint is in open space...
         if (contents == CONTENTS_EMPTY)
         {
            // ok so far, we are at the surface of the water, continue...
   
            v_src = tr.vecEndPos;
            v_forward = v_src;
            v_forward.z -= 90;
   
            // trace from the previous end point straight down...
            UTIL_TraceLine( v_src, v_forward, dont_ignore_monsters, pEdict->v.pContainingEntity, &tr);
   
            // check if the trace hit something...
            if (tr.flFraction < 1.0f)
            {
               contents = UTIL_PointContents( tr.vecEndPos );
   
               // if contents isn't water then assume it's land, jump!
               if (contents != CONTENTS_WATER)
               {
                  pEdict->v.button |= IN_JUMP;
               }
            }
         }
      }
   }
}


/*
void BotUseLift( bot_t *pBot, float moved_distance )
{
   edict_t *pEdict = pBot->pEdict;

   // just need to press the button once, when the flag gets set...
   if (pBot->f_use_button_time == gpGlobals->time)
   {
      pEdict->v.button = IN_USE;

      // face opposite from the button
      pEdict->v.ideal_yaw += 180;  // rotate 180 degrees

      BotFixIdealYaw(pEdict);
   }

   // check if the bot has waited too long for the lift to move...
   if (((pBot->f_use_button_time + 2.0) < gpGlobals->time) &&
       (!pBot->b_lift_moving))
   {
      // clear use button flag
      pBot->b_use_button = FALSE;

      // bot doesn't have to set f_find_item since the bot
      // should already be facing away from the button

      pBot->move_speed = SPEED_MAX;
   }

   // check if lift has started moving...
   if ((moved_distance > 1) && (!pBot->b_lift_moving))
   {
      pBot->b_lift_moving = TRUE;
   }

   // check if lift has stopped moving...
   if ((moved_distance <= 1) && (pBot->b_lift_moving))
   {
      TraceResult tr1, tr2;
      Vector v_src, v_forward, v_right, v_left;
      Vector v_down, v_forward_down, v_right_down, v_left_down;

      pBot->b_use_button = FALSE;

      // TraceLines in 4 directions to find which way to go...

      UTIL_MakeVectors( pEdict->v.v_angle );

      v_src = pEdict->v.origin + pEdict->v.view_ofs;
      v_forward = v_src + gpGlobals->v_forward * 90;
      v_right = v_src + gpGlobals->v_right * 90;
      v_left = v_src + gpGlobals->v_right * -90;

      v_down = pEdict->v.v_angle;
      v_down.x = v_down.x + 45;  // look down at 45 degree angle

      UTIL_MakeVectors( v_down );

      v_forward_down = v_src + gpGlobals->v_forward * 100;
      v_right_down = v_src + gpGlobals->v_right * 100;
      v_left_down = v_src + gpGlobals->v_right * -100;

      // try tracing forward first...
      UTIL_TraceLine( v_src, v_forward, dont_ignore_monsters,
                      pEdict->v.pContainingEntity, &tr1);
      UTIL_TraceLine( v_src, v_forward_down, dont_ignore_monsters,
                      pEdict->v.pContainingEntity, &tr2);

      // check if we hit a wall or didn't find a floor...
      if ((tr1.flFraction < 1.0) || (tr2.flFraction >= 1.0))
      {
         // try tracing to the RIGHT side next...
         UTIL_TraceLine( v_src, v_right, dont_ignore_monsters,
                         pEdict->v.pContainingEntity, &tr1);
         UTIL_TraceLine( v_src, v_right_down, dont_ignore_monsters,
                         pEdict->v.pContainingEntity, &tr2);

         // check if we hit a wall or didn't find a floor...
         if ((tr1.flFraction < 1.0) || (tr2.flFraction >= 1.0))
         {
            // try tracing to the LEFT side next...
            UTIL_TraceLine( v_src, v_left, dont_ignore_monsters,
                            pEdict->v.pContainingEntity, &tr1);
            UTIL_TraceLine( v_src, v_left_down, dont_ignore_monsters,
                            pEdict->v.pContainingEntity, &tr2);

            // check if we hit a wall or didn't find a floor...
            if ((tr1.flFraction < 1.0) || (tr2.flFraction >= 1.0))
            {
               // only thing to do is turn around...
               pEdict->v.ideal_yaw += 180;  // turn all the way around
            }
            else
            {
               pEdict->v.ideal_yaw += 90;  // turn to the LEFT
            }
         }
         else
         {
            pEdict->v.ideal_yaw -= 90;  // turn to the RIGHT
         }

         BotFixIdealYaw(pEdict);
      }

      BotChangeYaw( pBot, pEdict->v.yaw_speed );

      pBot->move_speed = SPEED_MAX;
   }
}
*/


bool BotStuckInCorner( bot_t *pBot )
{
   TraceResult tr;
   Vector v_src, v_dest;
   edict_t *pEdict = pBot->pEdict;
   
   UTIL_MakeVectors( pEdict->v.v_angle );

   // trace 45 degrees to the right...
   v_src = pEdict->v.origin;
   v_dest = v_src + gpGlobals->v_forward*20 + gpGlobals->v_right*20;

   UTIL_TraceLine( v_src, v_dest, dont_ignore_monsters, pEdict->v.pContainingEntity, &tr);

   if (tr.flFraction >= 1.0f)
      return false;  // no wall, so not in a corner

   // trace 45 degrees to the left...
   v_src = pEdict->v.origin;
   v_dest = v_src + gpGlobals->v_forward*20 - gpGlobals->v_right*20;

   UTIL_TraceLine( v_src, v_dest, dont_ignore_monsters, pEdict->v.pContainingEntity, &tr);

   if (tr.flFraction >= 1.0f)
      return false;  // no wall, so not in a corner

   return true;  // bot is in a corner
}


void BotTurnAtWall( bot_t *pBot, TraceResult *tr )
{
   edict_t *pEdict = pBot->pEdict;
   Vector Normal;
   float Y, Y1, Y2, D1, D2, Z;

   // Find the normal vector from the trace result.  The normal vector will
   // be a vector that is perpendicular to the surface from the TraceResult.

   Normal = UTIL_VecToAngles(tr->vecPlaneNormal);

   // Since the bot keeps it's view angle in -180 < x < 180 degrees format,
   // and since TraceResults are 0 < x < 360, we convert the bot's view
   // angle (yaw) to the same format at TraceResult.

   Y = pEdict->v.v_angle.y;
   Y = Y + 180.0f;
   if (Y > 359.0f) Y -= 360.0f;

   // Turn the normal vector around 180 degrees (i.e. make it point towards
   // the wall not away from it.  That makes finding the angles that the
   // bot needs to turn a little easier.

   Normal.y = Normal.y - 180;
   if (Normal.y < 0)
   Normal.y += 360;

   // Here we compare the bots view angle (Y) to the Normal - 90 degrees (Y1)
   // and the Normal + 90 degrees (Y2).  These two angles (Y1 & Y2) represent
   // angles that are parallel to the wall surface, but heading in opposite
   // directions.  We want the bot to choose the one that will require the
   // least amount of turning (saves time) and have the bot head off in that
   // direction.

   Y1 = Normal.y - 90;
   if (RANDOM_LONG(1, 100) <= 50)
   {
      Y1 = Y1 - RANDOM_FLOAT(5.0f, 20.0f);
   }
   if (Y1 < 0.0f) Y1 += 360.0f;

   Y2 = Normal.y + 90;
   if (RANDOM_LONG(1, 100) <= 50)
   {
      Y2 = Y2 + RANDOM_FLOAT(5.0f, 20.0f);
   }
   if (Y2 > 359.0f) Y2 -= 360.0f;

   // D1 and D2 are the difference (in degrees) between the bot's current
   // angle and Y1 or Y2 (respectively).

   D1 = fabs((double) Y - Y1);
   if (D1 > 179.0f) D1 = fabs((double) D1 - 360.0f);
   D2 = fabs((double) Y - Y2);
   if (D2 > 179.0f) D2 = fabs((double) D2 - 360.0f);

   // If difference 1 (D1) is more than difference 2 (D2) then the bot will
   // have to turn LESS if it heads in direction Y1 otherwise, head in
   // direction Y2.  I know this seems backwards, but try some sample angles
   // out on some graph paper and go through these equations using a
   // calculator, you'll see what I mean.

   if (D1 > D2)
      Z = Y1;
   else
      Z = Y2;

   // convert from TraceResult 0 to 360 degree format back to bot's
   // -180 to 180 degree format.

   if (Z > 180.0f)
      Z -= 360.0f;

   // set the direction to head off into...
   pEdict->v.ideal_yaw = Z;

   BotFixIdealYaw(pEdict);
}


/*
* uses some TraceLines to determine if anything is blocking the current path of the bot
* returns TRUE if at least one traceline hit something (ie. bot can't move forward)
*/
bool TraceForward(bot_t *pBot, bool check_head, bool check_feet, int dist, TraceResult *tr)
{
	edict_t *pEdict = pBot->pEdict;	
	Vector v_src, v_forward;
	
	UTIL_MakeVectors( pEdict->v.v_angle );
	
	// first do a trace from the bot's eyes forward (if we need it)
	if (check_head)
	{
		v_src = pEdict->v.origin + pEdict->v.view_ofs;  // EyePosition()
		v_forward = v_src + gpGlobals->v_forward * dist;
		
		// trace from the bot's eyes straight forward...
		UTIL_TraceLine( v_src, v_forward, dont_ignore_monsters, pEdict->v.pContainingEntity, tr);

		// check if the trace hit something...
		if (tr->flFraction < 1.0f)
		{
			return true;  // bot's head will hit something
		}
	}
	
	// bot's head is clear (if we checked it), check at waist level...
	
	v_src = pEdict->v.origin;
	v_forward = v_src + gpGlobals->v_forward * dist;
	
	// trace from the bot's waist straight forward...
	UTIL_TraceLine( v_src, v_forward, dont_ignore_monsters, pEdict->v.pContainingEntity, tr);
	
	// check if the trace hit something...
	if (tr->flFraction < 1.0f)
	{
		return true;  // bot's body will hit something
	}

	// do a trace from the bot's feet forward (if we need it)
	if (check_feet)
	{
		v_src = pEdict->v.origin - pEdict->v.view_ofs;  // FeetPosition
		v_forward = v_src + gpGlobals->v_forward * dist;
		
		// trace from the bot's feet straight forward...
		UTIL_TraceLine( v_src, v_forward, dont_ignore_monsters, pEdict->v.pContainingEntity, tr);

		// check if the trace hit something...
		if (tr->flFraction < 1.0f)
		{
			return true;  // bot's feet will hit something
		}
	}
	
	return false;  // bot can move forward, return false
}


bool BotCantMoveForward( bot_t *pBot, TraceResult *tr )
{
	return TraceForward(pBot, true, false, 40, tr);
}


bool IsForwardBlocked(bot_t *pBot)
{
	TraceResult tr;	// we don't need it here

	return TraceForward(pBot, true, true, 40, &tr);
}


/*
* tracelines the area before the bot to detect if there isn't a danger of a death fall
* the traceline starts a few units in front of the bot and goes straight down
*/
bool IsDeathFall(edict_t *pEdict)
{
	TraceResult tr;
	Vector v_source, v_dest;

	// NOTE: Try to use MakeVectors when the bot goes up the hill
	//UTIL_MakeVectors(pEdict->v.v_angle);

	// we have to trace in front of the bot to allow him to react
	v_source = pEdict->v.origin + pEdict->v.view_ofs + gpGlobals->v_forward * 75;
	
	// 250 units long fall is right about not to break the leg
	v_dest = v_source - pEdict->v.view_ofs + Vector(0.0f, 0.0f, -250.0f);

	UTIL_TraceLine(v_source, v_dest, dont_ignore_monsters, pEdict->v.pContainingEntity, &tr);

#ifdef _DEBUG
	//@@@@@@@@@@@@@@@@@@
	/*
	char msg[256];
	sprintf(msg, "tr %.2f (hitgr %d) (flPlaneDist %.2f)\n", tr.flFraction, tr.iHitgroup, tr.flPlaneDist);
	conOutput.Notify(msg);
	if (tr.pHit)
	{
		sprintf(msg, "pHit (class %s) (net %s) (glob %s)\n", STRING(tr.pHit->v.classname), STRING(tr.pHit->v.netname), STRING(tr.pHit->v.globalname));
		conOutput.Notify(msg);
	}
	*/
#endif

	// if trace hit something, return FALSE ie. no danger of deathfall
	if (tr.flFraction < 1.0f)
		return false;

	// there isn't safe ground in front of the bot (ie. the bot shouldn't continue forward)
	return true;
}


bool TraceJumpUp(bot_t* pBot, int jump_height, bool duckjump)
{
	// What I do here is trace 3 lines straight out, one unit higher than the highest normal jumping distance. I trace once at the center of the body,
	// once at the right side, and once at the left side. If all three of these TraceLines don't hit an obstruction then I know the area to jump to is clear.
	// I then need to trace from head level, above where the bot will jump to, downward to see if there is anything blocking the jump. There could be a narrow opening that the body
	// will not fit into. These horizontal and vertical TraceLines seem to catch most of the problems with falsely trying to jump on something that the bot can not get onto.

	// Note by Frank: Botman's original code is actually changed. The 3 TraceLines to scan the head level, above where the bot will jump to, have swapped source and destination
	//		points now. So that they don't go downward from top, but go upward from the max jump height, because the original code used to return false results. I mean
	//		if the source point started inside solid object then the TraceLine managed to successfully reach the destination point which was within the game world and was
	//		free of any obstruction. So bot then tried to jump even to places where he cannot fit at all ... his head or top of the body would have to be inside solid object
	//		to get to that spot.
	//		Also added correct body height when the bot is crouched or needs to do a duck jump.

	TraceResult tr;
	Vector v_jump, v_source, v_dest;
	edict_t* pEdict = pBot->pEdict;

	// standard height of the player in standing stance
	int body_heigth = 72;

	// but when crouched the body height is halved ... this is needed to get correct results for duckjump checks, because bot can fit into much smaller space that way
	if (duckjump || pBot->IsCrouched())
		body_heigth = 36;

	// convert current view angle to vectors for TraceLine math...
	v_jump = pEdict->v.v_angle;
	v_jump.x = 0;  // reset pitch to 0 (level horizontally)
	v_jump.z = 0;  // reset roll to 0 (straight up and down)

	UTIL_MakeVectors(v_jump);

	// use center of the body first...	

	// the starting point is at the origin that means the centre of the body, so if we need to get on ground level, then we must offset downward by half of the body,
	// and then we can go upward to the jump height level...actually one unit above the jump height
	v_source = pEdict->v.origin + Vector(0, 0, -(body_heigth / 2) + jump_height + 1);
	
	// the end point is on the same level of the jump height but 24 units in front of the bot
	v_dest = v_source + gpGlobals->v_forward * 24;
	
	// trace a line forward at maximum jump height...
	UTIL_TraceLine(v_source, v_dest, dont_ignore_monsters, pEdict->v.pContainingEntity, &tr);

	// if trace hit something, return FALSE
	if (tr.flFraction < 1.0f)
		return false;

	// now check same height to one side of the bot...
	v_source = pEdict->v.origin + gpGlobals->v_right * 16 + Vector(0, 0, -(body_heigth / 2) + jump_height + 1);
	v_dest = v_source + gpGlobals->v_forward * 24;

	UTIL_TraceLine(v_source, v_dest, dont_ignore_monsters, pEdict->v.pContainingEntity, &tr);

	// if trace hit something, return FALSE
	if (tr.flFraction < 1.0f)
		return false;

	// now check same height on the other side of the bot...
	v_source = pEdict->v.origin + gpGlobals->v_right * -16 + Vector(0, 0, -(body_heigth / 2) + jump_height + 1);
	v_dest = v_source + gpGlobals->v_forward * 24;

	UTIL_TraceLine(v_source, v_dest, dont_ignore_monsters, pEdict->v.pContainingEntity, &tr);

	// if trace hit something, return FALSE
	if (tr.flFraction < 1.0f)
		return false;

	// now trace from the jump height level upward to check for obstructions...

	// start of trace is 24 units in front of bot...
	v_source = pEdict->v.origin + gpGlobals->v_forward * 24;

	// and on the jump height level (offset to the max jump height level like in the 1st series of TraceLines)
	v_source = v_source + Vector(0, 0, -(body_heigth / 2) + jump_height + 1);

	// end point of trace is 'body_height' units straight up from start...
	v_dest = v_source + Vector(0, 0, body_heigth);

	// and trace that line...
	UTIL_TraceLine(v_source, v_dest, dont_ignore_monsters, pEdict->v.pContainingEntity, &tr);

	// if trace hit something, return FALSE
	if (tr.flFraction < 1.0f)
		return false;

	// now check the same to one side of the bot...
	v_source = pEdict->v.origin + gpGlobals->v_right * 16 + gpGlobals->v_forward * 24;
	v_source = v_source + Vector(0, 0, -(body_heigth / 2) + jump_height + 1);
	v_dest = v_source + Vector(0, 0, body_heigth);

	UTIL_TraceLine(v_source, v_dest, dont_ignore_monsters, pEdict->v.pContainingEntity, &tr);

	if (tr.flFraction < 1.0f)
		return false;

	// now check the same on the other side of the bot...
	v_source = pEdict->v.origin + gpGlobals->v_right * -16 + gpGlobals->v_forward * 24;
	v_source = v_source + Vector(0, 0, -(body_heigth / 2) + jump_height + 1);
	v_dest = v_source + Vector(0, 0, body_heigth);

	UTIL_TraceLine(v_source, v_dest, dont_ignore_monsters, pEdict->v.pContainingEntity, &tr);

	if (tr.flFraction < 1.0f)
		return false;

	// everything passed so bot can jump to that space and will fit in
	return true;
}


bool BotCanJumpUp(bot_t* pBot)
{
	// maximum jump height is 45 units
	return TraceJumpUp(pBot, 45);
}


bool BotCanDuckJumpUp(bot_t* pBot )
{
	// 62 = 45 (jump height) + 18 (size of "legs" when crouched) - 1 (just for sure)
	return TraceJumpUp(pBot, 62, true);
}


bool BotCanDuckJumpInto(bot_t* pBot)
{
	bool result = false;

	// 36 units is the size of body when fully crouched,
	// also when doing duckjump from crouch stance you won't jump that high so using the size of the body for the height seems just fine
	// and finally testing it on a few manholes worked well with this value
	result = TraceJumpUp(pBot, 36, true);

	// no success?
	if (result == false)
	{
		// then fake the crouch stance and try it again
		pBot->pEdict->v.origin.z -= 18;

		result = TraceJumpUp(pBot, 36, true);

		// last attempt is to try the standard jump height from crouch stance
		if (result == false)
			result = TraceJumpUp(pBot, 45, true);
		
		pBot->pEdict->v.origin.z += 18;
	}

	return result;
}


bool BotCanDuckUnder(bot_t* pBot)
{
	// What I do here is trace 3 lines straight out, one unit higher than the ducking height. I trace once at the center of the body, once at the right side, and once at the left side.
	// If all three of these TraceLines don't hit an obstruction then I know the area to duck to is clear. I then need to trace from the ground up, 72 units, to make sure that there is
	// something blocking the TraceLine. Then we know we can duck under it.

	// Note by Frank: Added correct body height when the bot is crouched.

	TraceResult tr;
	Vector v_duck, v_source, v_dest;
	edict_t* pEdict = pBot->pEdict;

	// standard height of the player in standing stance
	int body_heigth = 72;

	// but when crouched the body height is halved ... this is needed to get correct results
	if (pBot->IsCrouched())
		body_heigth = 36;

	// convert current view angle to vectors for TraceLine math...

	v_duck = pEdict->v.v_angle;
	v_duck.x = 0;  // reset pitch to 0 (level horizontally)
	v_duck.z = 0;  // reset roll to 0 (straight up and down)

	UTIL_MakeVectors(v_duck);

	// use center of the body first...

	// duck height is 36, so check one unit above that (37)
	v_source = pEdict->v.origin + Vector(0, 0, -(body_heigth / 2) + 37);
	v_dest = v_source + gpGlobals->v_forward * 24;

	// trace a line forward at duck height...
	UTIL_TraceLine(v_source, v_dest, dont_ignore_monsters, pEdict->v.pContainingEntity, &tr);

	// if trace hit something, return FALSE
	if (tr.flFraction < 1.0f)
		return false;

	// now check same height to one side of the bot...
	v_source = pEdict->v.origin + gpGlobals->v_right * 16 + Vector(0, 0, -(body_heigth / 2) + 37);
	v_dest = v_source + gpGlobals->v_forward * 24;

	// trace a line forward at duck height...
	UTIL_TraceLine(v_source, v_dest, dont_ignore_monsters, pEdict->v.pContainingEntity, &tr);

	// if trace hit something, return FALSE
	if (tr.flFraction < 1.0f)
		return false;

	// now check same height on the other side of the bot...
	v_source = pEdict->v.origin + gpGlobals->v_right * -16 + Vector(0, 0, -(body_heigth / 2) + 37);
	v_dest = v_source + gpGlobals->v_forward * 24;

	// trace a line forward at duck height...
	UTIL_TraceLine(v_source, v_dest, dont_ignore_monsters, pEdict->v.pContainingEntity, &tr);

	// if trace hit something, return FALSE
	if (tr.flFraction < 1.0f)
		return false;

	// now trace from the ground up to check for object to duck under...

	// offset to feet + 1 unit up
	int just_above_ground = (body_heigth / 2) - 1;

	// start of trace is 24 units in front of bot near ground...
	v_source = pEdict->v.origin + gpGlobals->v_forward * 24;
	v_source.z = v_source.z - just_above_ground;

	// end point of trace is the height of body (72 or 36) units straight up from start...
	v_dest = v_source + Vector(0, 0, body_heigth);

	// trace a line straight up in the air...
	UTIL_TraceLine(v_source, v_dest, dont_ignore_monsters, pEdict->v.pContainingEntity, &tr);

	// if trace didn't hit something, return FALSE
	if (tr.flFraction >= 1.0f)
		return false;

	// now check same height to one side of the bot...
	v_source = pEdict->v.origin + gpGlobals->v_right * 16 + gpGlobals->v_forward * 24;
	v_source.z = v_source.z - just_above_ground;
	v_dest = v_source + Vector(0, 0, body_heigth);

	UTIL_TraceLine(v_source, v_dest, dont_ignore_monsters, pEdict->v.pContainingEntity, &tr);

	if (tr.flFraction >= 1.0f)
		return false;

	// now check same height on the other side of the bot...
	v_source = pEdict->v.origin + gpGlobals->v_right * -16 + gpGlobals->v_forward * 24;
	v_source.z = v_source.z - just_above_ground;
	v_dest = v_source + Vector(0, 0, body_heigth);

	UTIL_TraceLine(v_source, v_dest, dont_ignore_monsters, pEdict->v.pContainingEntity, &tr);

	if (tr.flFraction >= 1.0f)
		return false;

	return true;
}


bool BotCheckWallOnLeft( bot_t *pBot )
{
   edict_t *pEdict = pBot->pEdict;
   Vector v_src, v_left;
   TraceResult tr;

   UTIL_MakeVectors( pEdict->v.v_angle );

   // do a trace to the left...

   v_src = pEdict->v.origin;
   v_left = v_src + gpGlobals->v_right * -40;  // 40 units to the left

   UTIL_TraceLine( v_src, v_left, dont_ignore_monsters, pEdict->v.pContainingEntity, &tr);

   // check if the trace hit something...
   if (tr.flFraction < 1.0f)
   {
      if (pBot->f_wall_on_left < 1.0f)
         pBot->f_wall_on_left = gpGlobals->time;

      return true;
   }

   return false;
}


bool BotCheckWallOnRight( bot_t *pBot )
{
   edict_t *pEdict = pBot->pEdict;
   Vector v_src, v_right;
   TraceResult tr;

   UTIL_MakeVectors( pEdict->v.v_angle );

   // do a trace to the right...

   v_src = pEdict->v.origin;
   v_right = v_src + gpGlobals->v_right * 40;  // 40 units to the right

   UTIL_TraceLine( v_src, v_right, dont_ignore_monsters, pEdict->v.pContainingEntity, &tr);

   // check if the trace hit something...
   if (tr.flFraction < 1.0f)
   {
      if (pBot->f_wall_on_right < 1.0f)
         pBot->f_wall_on_right = gpGlobals->time;

      return true;
   }

   return false;
}