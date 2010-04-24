#include "autoCode.h"
/*
*	Edited and ported by Matt Roman 4/17/10
*	Created by David P. Miller on 7/22/08.
*  	Copyright 2008 University of Oklahoma. All rights reserved.
*/

autoCode::autoCode(SR2rover *bot,QList<WayPoint> *list)
:
sr2(bot)
{	
	POINTTURNSPEED = 6;
	POINTTURNANGLE = DEGTORAD(30);
	TURNMULTIPLIER = 6;
	CLOSEENOUGH = 1;
	TURNACCURACYLIMIT = DEGTORAD(4);
	ROVERCRUISESPEED = 10;
	BODYDIST = 1.5;
	PROFILEOBSTACLEMAX = 0.15;
	MAXPITCH = DEGTORAD(15.0);
	MAXROLL = DEGTORAD(20.0);
	PITCHDOWNIGNOREDISTOBSTACLES = DEGTORAD(10);
	BODYTOOCLOSE = 0.5;
	PANELOBSTACLEMAX = 0.15;
	TURNFACTOR = 0.5;
	GOPASTDISTANCE = 3;
	PATHEFFICIENCY = 0.3; 
	
	wpIndex = 0;
	WPlist = list;
	
	stopAutonomous(RSInTeleopMode);
	error = RENone;
}

autoCode::~autoCode()
{
}

void autoCode::callForHelp(RoverError errCode)
{
	error = errCode;
	stopAutonomous(RSCallingForHelp);
}
// Convert from compass angles where N=0 and E=90 to Cartesian where N=90 and E=0
float autoCode::compassToCartRadians(float rad)
{
	rad = TWOPI - (rad-HALFPI);
	if(rad > TWOPI) rad -= TWOPI;
	return rad;
}
// finds the acute half angle between two angles IN RADIANS
float autoCode::avgAngle(float rad1, float rad2)
{
	float avgAngle;
	avgAngle = (rad1 + rad2)/2;
	if((MAXIMUM(rad1,rad2)-MINIMUM(rad1,rad2)) > PI){ // two heading are obtuse
		avgAngle += PI;	// compute the acute angle
		if(avgAngle >= TWOPI) avgAngle -= TWOPI;
	}
	return avgAngle;
}
// Returns the heading angle to the waypoint in compass RADIANS
float autoCode::compassWaypointHeading()
{
	float xDiff = currentWaypoint.position.x - sr2->position.x();
	float yDiff = currentWaypoint.position.y - sr2->position.y();
	if(xDiff == 0 && yDiff == 0) return sr2->heading;	// if the rover is on the waypoint return rovers current heading
	return compassToCartRadians(atan2(yDiff,xDiff));
}
// Retruns the angle in RADIANS to the waypoint relative to the rover
float autoCode::roverWaypointHeading()
{
	float absHeading = compassWaypointHeading();
	float relHeading = absHeading - sr2->heading;
	if(relHeading < -PI) return relHeading+TWOPI;
	if(relHeading > PI) return relHeading-TWOPI;
	return relHeading;
}

float autoCode::distanceToWaypoint(int i)
{
	Vertex rover;
	rover.x = sr2->position.x();
	rover.y = sr2->position.y();
	rover.z = sr2->position.z();
	return distBtwVerts(WPlist->at(i).position,rover);
}

void autoCode::goAutonomous()
{
	if(WPlist->isEmpty()) return;
	if(wpIndex > WPlist->size()) wpIndex = WPlist->size()-1;
	
	currentWaypoint = WPlist->at(wpIndex);
	currentWaypoint.state = WPstateCurrent;
	WPlist->replace(wpIndex, currentWaypoint);
	
	connect(sr2, SIGNAL(updated()),this, SLOT(moveToWaypoint()));
	
	currentWaypoint = WPlist->at(wpIndex);
	running = true;
	state = RSMovingTowardsWaypoint; // reset the rover state
	error = RENone;	// reset the rover error
	lastBlockedDirection = 0;
	expectedDistance = sr2->odometer + (1+PATHEFFICIENCY) * distanceToWaypoint(wpIndex); // reset the expected distance
}

void autoCode::stopAutonomous(RoverState rs)
{
	sr2->stopRobot();
	// disconnect from sr2 update signal
	disconnect(sr2, SIGNAL(updated()),this,SLOT(moveToWaypoint()));
	
	running = false;
	state = rs;	
}

void autoCode::toggleAutonomous()
{
	if(running) 
		stopAutonomous(RSInTeleopMode);
	else
		goAutonomous();
}

/////////////////////////////////////////
// Gets the rover turning the requested amount (large turns are done as point turns. Small turns done gradually).
// this function just sets the speeds and returns. relHeading = turn angle relative to the rover.
/////////////
void autoCode::driveToward(float relHeading,float distTo)
{ 
	float leftSpeed,rightSpeed;
	if(fabs(relHeading) < TURNACCURACYLIMIT){ // high speed forward
		distTo -= CLOSEENOUGH;
		if(distTo < 1) leftSpeed = rightSpeed = (ROVERCRUISESPEED - POINTTURNSPEED)*distTo + POINTTURNSPEED;
		else leftSpeed = rightSpeed = ROVERCRUISESPEED;
		
		if(relHeading > 0.0) rightSpeed = rightSpeed - (relHeading/TURNACCURACYLIMIT)*5;
		else leftSpeed = leftSpeed + (relHeading/TURNACCURACYLIMIT)*5;
	}
	else if(fabs(relHeading) < POINTTURNANGLE){ // low speed turn
		leftSpeed = rightSpeed = POINTTURNSPEED;
		if(relHeading > 0.0) rightSpeed = rightSpeed - (relHeading/POINTTURNANGLE)*TURNMULTIPLIER;
		else leftSpeed = leftSpeed + (relHeading/POINTTURNANGLE)*TURNMULTIPLIER;
	}
	else{	// point turn
		leftSpeed = rightSpeed = POINTTURNSPEED;
		if(relHeading > 0.0) rightSpeed = -POINTTURNSPEED;// right turn
		else leftSpeed = -POINTTURNSPEED;// left turn
	}	
	sr2->setRightSpeed(rightSpeed);
	sr2->setLeftSpeed(leftSpeed);	
}

/////////////////////////////////////////
// THIS IS THE HIGHEST LEVEL FUNCTION CALL !!!!
// Moves robot towards waypoint.  When WP is reached it stops rover.
// Note: if WP is almost close enough and rover is avoiding obstacles, that WP will be skipped
/////////////
void autoCode::moveToWaypoint()
{
	float wpRange = distanceToWaypoint(wpIndex);
	
	// if the rover is close enought to the waypoint consider it complete
	// set the waypoint as old and move to the next one
	if(wpRange < CLOSEENOUGH){ 	
		currentWaypoint.state = WPstateOld;
		WPlist->replace(wpIndex, currentWaypoint);
		wpIndex++;
		// get the next waypoint in the list
		if(wpIndex < WPlist->size()) { 
			currentWaypoint = WPlist->at(wpIndex);
			currentWaypoint.state = WPstateCurrent;
			WPlist->replace(wpIndex, currentWaypoint);
			// set the expected drive distance to the new waypoint
			expectedDistance = sr2->odometer + (1.0+PATHEFFICIENCY) * distanceToWaypoint(wpIndex);
			state = RSReachedWaypoint;
			emit stateUpdate();
			return;
		}
		else{ // waypoint list is complete should be at goal or no plan
			wpIndex = WPlist->size() - 1;
			stopAutonomous(RSNoRemainingWaypoints);
			return;
		}
	}
	
// if the rover has driven farther than expected to the next waypoint
	if(expectedDistance < sr2->odometer){
		// stop and call for help
		callForHelp(REProgress);
		return;
	}

	float relTargetHeading = roverWaypointHeading();
		
// check for obstacles, returns false if there was an error
	if(!checkForObstacles(wpRange)) return;
	
	// drive to waypoint
	if(state == RSMovingTowardsWaypoint){
		lastBlockedDirection = 0;
		driveToward(relTargetHeading,wpRange);	
	}
	// drive past obstacle
	else if(state == RSGoingPastObstacles){
		// continue driving to the waypoint if past the obstacle
		if(lastBlockedPosition.distance2(sr2->position) > GOPASTDISTANCE){
			lastBlockedDirection = 0;
			state = RSMovingTowardsWaypoint;
			driveToward(relTargetHeading,wpRange);
		}
		else{
			// no obstacle in view so drive past it
			if(state == RSGoingPastObstacles){
				sr2->setRightSpeed(ROVERCRUISESPEED);
				sr2->setLeftSpeed(ROVERCRUISESPEED);
			}
		}
	}
	// avoid obstacles
	else{
		lastBlockedPosition = sr2->position;  // mark this spot as latest position where obstacle was seen
		avoidingTurn();
	}
	
	if(blockedDirection < 0.0) lastBlockedDirection = -1;
	else lastBlockedDirection = 1;
	
	emit stateUpdate();
}

/////////////////////////////////////////
// Checks for obstacles and sets the direction the rover is blocked , + is right, - is left
// sets the rover state if avoiding near or far, driving past, or driving to waypoint
/////////////
bool autoCode::checkForObstacles(float distTo)
{
	float criticalDist = distTo;
	int size = sr2->getLaserScanner(PANELLASER)->getDataSize();
	float* heights = sr2->getPanelLaserHeights();
	float* ranges = sr2->getLaserScanner(BODYLASER)->getData();
	int i,aheadBlockedLong=0, aheadBlockedShort=0, instShort=0,instLong=0;
	static int prevBlockedShort=0, prevBlockedLong=0;
	int slopeObstacle = 0;
	
	blockedDirection = 0;
	
	//set to minimum of critDistance and BODYDIST
	criticalDist = (criticalDist < BODYDIST) ? criticalDist : BODYDIST;

// check rover POSE SLOPE, roll and pitch to so it doesn't flip over	
	// roll maxed out by itself is an emergency
	if(fabs(sr2->roll) > MAXROLL){
		blockedDirection = (sr2->roll > 0.0) ? 5 /*rolling LEFT*/ : -5 /*rolling RIGHT*/;
		callForHelp(RERoll);
		return false; // Roll is maxed out!!!!!!
	}
	else if(sr2->pitch > MAXPITCH){
		aheadBlockedLong=1;
		blockedDirection = (sr2->roll > 0.0) ? 5 /*rolling LEFT*/ : -5 /*rolling RIGHT*/;
		slopeObstacle = 1;
	}
	
// look for any BODY blocking obstacles directly in front of the rover
	if(!slopeObstacle || (slopeObstacle && sr2->pitch > PITCHDOWNIGNOREDISTOBSTACLES)){
		for(i=0;i<size;i++){
			// check for near obstacles
			if(ranges[i] < BODYTOOCLOSE){
				if(instShort) aheadBlockedShort = 1;
				else instShort = 1;
				// + is blocked on right, - is blocked on left
				blockedDirection = blockedDirection + 2/(i - 7.5);
			}
			// check for far obstacles
			else if(ranges[i] < criticalDist 
				&& ranges[i+1] < criticalDist
				&& i > 3
				&& i < 13){
				if(instLong) aheadBlockedLong = 1;
				else instLong = 1;
				// + is blocked on right, - is blocked on left
				blockedDirection = blockedDirection + 1/(i - 7.5);
			}
		}
	}
	//something directly ahead turn right
	if(blockedDirection == 0.0 && (aheadBlockedShort || aheadBlockedLong)) blockedDirection = -2.0;
		
// look through PANEL laser points in front of right and left wheels for obstacles
	for(i=1;i<5; i++){	// right wheel
	//is there a rock or hole?
		if((fabs(heights[i]) > PANELOBSTACLEMAX) 
			&& (fabs(heights[i+1]) > PANELOBSTACLEMAX) 
			&& (fabs(heights[i+2]) > PANELOBSTACLEMAX)){
				if(instShort) aheadBlockedShort = 1;
				else instShort = 1;
				// + is blocked on right, - is blocked on left
				blockedDirection = blockedDirection + 1/(7.5 - i);
		}
	}
	for(i=14;i>10; i--){  // left wheel
//is there a rock or hole?
		if((fabs(heights[i]) > PANELOBSTACLEMAX) 
			&& (fabs(heights[i-1]) > PANELOBSTACLEMAX)
			&& (fabs(heights[i-2]) > PANELOBSTACLEMAX)){
				if(instShort)aheadBlockedShort=1;
				else instShort = 1;
				// + is blocked on right, - is blocked on left
				blockedDirection = blockedDirection + 2/(7.5 - i);
		}
	}
	
	// mark that you are avoiding
	if(aheadBlockedLong) state = RSAvoidingDistantObstacles;
	else if(aheadBlockedShort) state = RSAvoidingNearbyObstacles;
	else if(prevBlockedShort || prevBlockedLong) state = RSGoingPastObstacles;
	else if(state != RSGoingPastObstacles) state = RSMovingTowardsWaypoint;
	
	prevBlockedShort = aheadBlockedShort;
	prevBlockedLong = aheadBlockedLong;
	return true;
}

// checks for obstacles without driving to waypoint, used for debugging
void autoCode::quickObstacleCheck()
{
	error = RENone;
	// run the obstacle check function where the waypoint is 3.0 meters away
	checkForObstacles(3.0);
}

/////////////////////////////////////////
// Drives around obstacles if any were detected
// This function turns wide or sharp or extreme as needed
/////////////
void autoCode::avoidingTurn()
{
	float closeTurnFactor = (TURNFACTOR > 0.0)? 0.0 : TURNFACTOR;

	// last block was to the left
	if(lastBlockedDirection < 0){
			// obstacle to the left or far off to the right
		if(blockedDirection < 0.0 || (blockedDirection > 0.0 && state == RSAvoidingDistantObstacles)){
			sr2->setLeftSpeed(ROVERCRUISESPEED);
			sr2->setRightSpeed(ROVERCRUISESPEED*((state == RSAvoidingNearbyObstacles) ? closeTurnFactor : TURNFACTOR));
			return;
		}
			// obstacle close to the right
		else if(blockedDirection > 0.0){
			sr2->setLeftSpeed(0);
			sr2->setRightSpeed(-ROVERCRUISESPEED);
			return;
		}
	}
	else{//last block was on right
			// obstacle to the right or far off to the left
		if(blockedDirection > 0.0 || (blockedDirection < 0.0 && state == RSAvoidingDistantObstacles)){
			sr2->setRightSpeed(ROVERCRUISESPEED);
			sr2->setLeftSpeed(ROVERCRUISESPEED*((state == RSAvoidingNearbyObstacles) ? closeTurnFactor : TURNFACTOR));
			return;
		}
			// obstacle close to the left
		else if(blockedDirection < 0.0){
			sr2->setRightSpeed(0);
			sr2->setLeftSpeed(-ROVERCRUISESPEED);
			return;
		}
	}
}

