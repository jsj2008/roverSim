#ifndef PATHPLAN_H
#define PATHPLAN_H

#include <QList>
#include "simglobject.h"
#include "utility/structures.h"

class btCollisionObject;
class cSpace;

typedef enum _PathDisplay
{
	PP_DEBUG = 0,
	PP_CROWFLY = 1,
	PP_SAVEDPATHS = 2,
	PP_CURRENTSEARCH = 3,
	PP_RANGEFAN = 4,
	PP_BASELINE = 5,
	PP_LIGHTTRAIL = 6
} PathDisplay;

// typedef struct _rankLink
// {
// 	rankPoint	first;
// 	rankPoint	second;
// 	float		length;
// }rankLink;

class cCallback
{
    public:
        virtual void Execute() const =0;
};

template <class objectInst>
class ACallback : public cCallback
{
    public:
		typedef void (objectInst::*functCall)();
		
        ACallback(objectInst  *instPointer, functCall  functPointer)
        {
            inst   = instPointer;
            method 	= functPointer;
        }
        
        virtual void Execute() const 
        {
        	if(method)(inst->*method)();
        }

        void SetCallback (objectInst  *instPointer, functCall  functPointer)
        {
            inst   = instPointer;
            method 	= functPointer;
        }
	private:
        objectInst  *inst;
        functCall  	method;
};

class pathPlan : public simGLObject
{
private:
	cSpace*										m_CS;				// the Configuration Space the path is calculated in
	bool										m_displayCS;		// draws the Configuration Space or not	
	
	QList<rankPoint>							m_pointPath;		// global point containter, used while searching for a path
	QList<goalPath>								m_pathList;			// contains all the paths if they have been saved
	goalPath									m_GP;				// the shortest Goal Path 
	
	rankPoint									m_startPoint;		// start point of the path
	rankPoint									m_midPoint;
	rankPoint									m_goalPoint;		// calculate a path to this point
	float										m_goalDistance;		// straight line distance to the goal from the start
	btCollisionObject*							m_goalOccluded;		// hold the object the goal is inside if the goal is occluded
	
	QList< ACallback<pathPlan> >				m_drawingList;		// holds all drawing callbacks
	QList< ACallback<pathPlan>* >				m_displayList;		// holds a pointer to the objects that should be currently drawn
	
	// debugging
	int											m_linkViewIndex;	// if C key is pressed this holds the view point on the shortest path
	QList<rankPoint>							contactPoints;		// holds all visible points at current linkViewIndex on the shortest path
	QList<btVector3> 							hitPoints;

	void generateCspace();
	bool isGoalInRange();
	void cycleToGoal();
	bool findPathA(float length=0);
	
	btCollisionObject* isRayBlocked(rankPoint from,rankPoint to, btVector3* point = NULL);
	void getExtremes(btCollisionObject* obj, rankPoint pivotPoint, rankPoint* left, rankPoint* right);
	QList<rankPoint> angleBasedRank(QList<rankPoint> list, rankPoint pivotPoint);
	QList<rankPoint> progressAngleBasedRank(QList<rankPoint> list, rankPoint pivotPoint);
	QList<rankPoint> rangeBasedRank(QList<rankPoint> list, rankPoint pivotPoint);
	QList<rankPoint> quickSortRankLessthan(QList<rankPoint> list);
	QList<rankPoint> getVisablePointsFrom(rankPoint here);
	QList<rankPoint> prunePointsFrom(QList<rankPoint> list);
	//bool isNewPoint(rankPoint pt);
	//bool isNewLink(rankLink link);
	
	void drawDebugPath();
	void drawCrowFlyLine();
	void drawSavedPaths();
	void drawCurrentSearchPath();
	void drawRangeFan();
	void drawPathBaseLine();
	void drawLightTrail();
	
public:
	pathPlan(btVector3 start, btVector3 end, goalPath gp, simGLView* glView = NULL);
	~pathPlan();
	
	btVector3 getStartPoint() { return m_startPoint.point; }
	btVector3 getGoalPoint() { return m_goalPoint.point; }
	float getGoalDistance() { return m_goalDistance; }
	goalPath* getShortestPath() { return &m_GP; }
	
	void setColor(btVector3 color) { m_GP.color = color; m_GP.color.m_floats[3] = 0.45; }
	void displayCrowFly(bool x);
	void displaySavedPaths(bool x);
	void displayCurrentSearch(bool x);
	void displayRangeFan(bool x);
	void displayPath(bool x);
	void displayLightTrail(bool x);
	
	void togglePathPoint();
	void toggleCspace();
	void renderGLObject();
};
#endif // PATHPLAN_H