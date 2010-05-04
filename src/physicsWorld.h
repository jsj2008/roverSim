/*
 *  physicsWorld.h
 *  RoverSimulation
 *
 *  Created by Matt Roman on 9/28/09.
 *  Copyright 2009 University of Oklahoma. All rights reserved.
 *
 */

#ifndef PHYSICSWORLD_H
#define PHYSICSWORLD_H

#include <QObject>
#include "utility/definitions.h"

#include <LinearMath/btVector3.h>
#include <LinearMath/btAlignedObjectArray.h>
#include <LinearMath/btTransform.h>


#define BT_EULER_DEFAULT_ZYX   // yaw, pitch, roll about Z, Y, X axis

class	btBroadphaseInterface;
class	btCollisionObject;
class	btCollisionShape;
class	btCollisionDispatcher;
class	btConstraintSolver;
class	btDynamicsWorld;
class	btRigidBody;
class	btDefaultCollisionConfiguration;

class physicsWorld : public QObject
{
private:
    bool                m_idle;
    bool                m_draw;
    btVector3           m_worldSize;
    float               m_worldBoundary;

protected:
    btAlignedObjectArray<btCollisionShape*>     m_obstacleShapes;
	btAlignedObjectArray<btCollisionObject*>	m_obstacleObjects;
	btAlignedObjectArray<btCollisionShape*>		m_ghostShapes;
	btAlignedObjectArray<btCollisionObject*>	m_ghostObjects;

    btBroadphaseInterface*						m_broadphase;
    btCollisionDispatcher*						m_dispatcher;
    btDynamicsWorld*                            m_dynamicsWorld;
    btConstraintSolver*                         m_solver;
    btDefaultCollisionConfiguration*			m_collisionConfiguration;

    physicsWorld(float x, float y, float z, float boundary);

    static physicsWorld *m_pWorld;

public:
    float   simTimeStep;
    float   simFixedTimeStep;
    int     simSubSteps;

	btAlignedObjectArray<btCollisionObject*>* getObstacleObjectArray() { return &m_obstacleObjects; }
	btAlignedObjectArray<btCollisionObject*>* getGhostObjectArray() { return &m_ghostObjects; }

    static physicsWorld *initialize(float x, float y, float z, float boundary) {
        if(!m_pWorld){
            m_pWorld = new physicsWorld(x, y, z, boundary);
        }
        return m_pWorld;
    }

    virtual ~physicsWorld();

    static physicsWorld *instance() { return m_pWorld; }
    static physicsWorld *destroy() {
        delete m_pWorld;
        m_pWorld = 0;
        return 0;
    }

    btDynamicsWorld* getDynamicsWorld() { return m_dynamicsWorld; }
    bool isIdle() const { return m_idle; }
	void toggleIdle();
	void idle() { m_idle = true; }
	
    bool canDraw() const { return m_draw; }
	void setDraw(bool d) { m_draw = d; }
    void setWorldSize(btVector3 xyz);
    btVector3 worldSize() { return m_worldSize; }
    float worldBoundary() { return m_worldBoundary; }

	void deleteObstacleGroup();
	void deleteGhostGroup();
    void resetBroadphaseSolver();
    void setGravity(btVector3 gv);
    
    void simulatStep();
    void resetWorld();

    btCollisionShape* createShape(int shapeTyp, btVector3 lwh);
    void createObstacleShape(int shapeTyp, btVector3 lwh);

    btRigidBody* createRigidBody(float mass, btTransform trans, btCollisionShape* cShape);
    btRigidBody* placeShapeAt(btCollisionShape* bodyShape, btVector3 pos, float yaw, float massval);
    void placeObstacleShapeAt(btVector3 pos,float yaw, float massval);
	void createGhostShape(btCollisionObject* bodyObj);
};

#endif // PHYSICSWORLD_H
