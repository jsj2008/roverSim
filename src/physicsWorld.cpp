/*
 *  physicsWorld.cpp
 *  RoverSimulation
 *
 *  Created by Matt Roman on 9/28/09.
 *  Copyright 2009 University of Oklahoma. All rights reserved.
 *
 */

#include "physicsWorld.h"

// Use with Bullet Frameworks
#include <BulletCollision/btBulletCollisionCommon.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <BulletDynamics/ConstraintSolver/btConstraintSolver.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <LinearMath/btDefaultMotionState.h>

physicsWorld *physicsWorld::m_pWorld = 0;

physicsWorld::physicsWorld()
{
	//collision configuration contains default setup for memory, collision setup
	m_collisionConfiguration = new btDefaultCollisionConfiguration();
	//use the default collision dispatcher. For parallel processing you can use a diffent dispatcher (see Extras/BulletMultiThreaded)
	m_dispatcher = new btCollisionDispatcher(m_collisionConfiguration);
	
	// Build the broadphase
    //int maxProxies = 5000;
    //btVector3 worldAabbMin(-m_worldBoundary,-m_worldBoundary,0);
    //btVector3 worldAabbMax = btVector3(m_worldBoundary,m_worldBoundary,0) + m_worldSize;
    //m_broadphase = new btAxisSweep3(worldAabbMin,worldAabbMax,maxProxies);
    m_broadphase = new btDbvtBroadphase();

	// The actual physics solver
	m_solver = new btSequentialImpulseConstraintSolver();
	
	// The Physics World
	m_dynamicsWorld = new btDiscreteDynamicsWorld(m_dispatcher,m_broadphase,m_solver,m_collisionConfiguration);
	
    m_dynamicsWorld->setGravity(btVector3(0,0,-9.8));

	simTimeStep = 0.1;
	simFixedTimeStep = 0.001;
	simSubSteps = 15;
	m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(simulateStep()));
}

physicsWorld::~physicsWorld()
{
	m_timer->stop();
	delete m_timer;
    delete m_dynamicsWorld;
    delete m_solver;
    delete m_broadphase;
    delete m_dispatcher;
    delete m_collisionConfiguration;
}

/////////////////////////////////////////
// Physics simulation timing methods
/////////////
void physicsWorld::startSimTimer(int msec)
{
	m_timer->start(msec);
}
void physicsWorld::stopSimTimer()
{
	m_timer->stop();
}
void physicsWorld::simulateStep()
{
    m_dynamicsWorld->stepSimulation(simTimeStep,simSubSteps,simFixedTimeStep);	///step the simulation
	emit simCycled();
}

// resets the entire world. Stops drawing and simulation resets the broadphase
// to the new size and resets the solver.
void physicsWorld::resetWorld()
{
    // reset the broadphase
    m_broadphase->resetPool(m_dispatcher);
    m_solver->reset();
	m_timer->start();
}

// sets the gravity vector in m/s^2
void physicsWorld::setGravity(btVector3 gv)
{
	m_dynamicsWorld->setGravity(gv);
	for(int i=0; i<m_dynamicsWorld->getCollisionObjectArray().size(); i++)
		m_dynamicsWorld->getCollisionObjectArray()[i]->activate();			// activate all objects incase they move relative to the new gravity
}

// used by convex shapes to create indices of triangles that openGL can use to draw the hull
void physicsWorld::hullCache(btConvexShape* shape, btAlignedObjectArray<ShapeCache*>* cacheArray)
{
	ShapeCache*	sc = new(btAlignedAlloc(sizeof(ShapeCache),16)) ShapeCache(shape);
	
	sc->m_shapehull.buildHull(shape->getMargin());
	cacheArray->push_back(sc);
	shape->setUserPointer(sc);
		
	/* Build edges	*/ 
	const int			ni = sc->m_shapehull.numIndices();
	const int			nv = sc->m_shapehull.numVertices();
	const unsigned int*	pi = sc->m_shapehull.getIndexPointer();
	const btVector3*	pv = sc->m_shapehull.getVertexPointer();
	
	btAlignedObjectArray<ShapeCache::Edge*>	edges;
	sc->m_edges.reserve(ni);
	edges.resize(nv*nv,0);
	for(int i=0;i<ni;i+=3)
	{
		const unsigned int* ti=pi+i;
		const btVector3		nrm=btCross(pv[ti[1]]-pv[ti[0]],pv[ti[2]]-pv[ti[0]]).normalized();
		for(int j=2,k=0;k<3;j=k++)
		{
			const unsigned int	a=ti[j];
			const unsigned int	b=ti[k];
			ShapeCache::Edge*&	e=edges[btMin(a,b)*nv+btMax(a,b)];
			if(!e)
			{
				sc->m_edges.push_back(ShapeCache::Edge());
				e=&sc->m_edges[sc->m_edges.size()-1];
				e->n[0]=nrm;e->n[1]=-nrm;
				e->v[0]=a;e->v[1]=b;
			}
			else
			{
				e->n[1]=nrm;
			}
		}
	}
}

// creates a new physics collision shape, this has to be called
// when a new shape is needed or a different size of the previous shape.
// It only needs to be called once if a bunch of identical shapes are needed.
btCollisionShape* physicsWorld::createShape(int shapeTyp, btVector3 lwh)
{
        btCollisionShape* collShape = NULL;
	
	switch (shapeTyp) {
		case BOX_SHAPE_PROXYTYPE:
			collShape = new btBoxShape(lwh);
			break;
		case SPHERE_SHAPE_PROXYTYPE:
			collShape = new btSphereShape(lwh.x());
			break;
		case CONE_SHAPE_PROXYTYPE:
			collShape = new btConeShapeZ(lwh.y(),lwh.x());
			break;
		case CYLINDER_SHAPE_PROXYTYPE:
			collShape = new btCylinderShapeX(lwh);
			break;
	}
	return collShape;
}

btRigidBody* physicsWorld::createRigidBody(float mass, btTransform trans, btCollisionShape* cShape)
{
	// set the mass of the box
	btScalar massval(mass);
	bool isDynamic = (massval != 0.f);
	
	// set inertia of body and calculate the CG
	btVector3 bodyInertia(0,0,0);
	if(isDynamic) cShape->calculateLocalInertia(massval,bodyInertia);
	
	// using motionstate provides interpolation and only synchronizes 'active' objects
	btDefaultMotionState* bodyMotionState = new btDefaultMotionState(trans);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass,bodyMotionState,cShape,bodyInertia);
	btRigidBody* body = new btRigidBody(rbInfo);
		
	// add the body to the world
	m_dynamicsWorld->addRigidBody(body);
	return body;
}

// Places the last shape created in the physics world with mass and a yaw angle
btRigidBody* physicsWorld::placeShapeAt(btCollisionShape* bodyShape, btVector3 pos,float yaw, float massval)
{
    // set the position of the box
    btTransform startTransform;
    startTransform.setIdentity();
    startTransform.setOrigin(pos);
    startTransform.setRotation(btQuaternion(0,0,DEGTORAD(yaw)));
	
    return createRigidBody(massval,startTransform,bodyShape);
}

/////////////////////////////////////////
// Obstacle hull body creation functions
/////////////
void physicsWorld::createHullObstacleShape(btVector3* pts, int numPoints)
{
	btConvexHullShape* shape = new btConvexHullShape((btScalar*)pts,numPoints);
	hullCache((btConvexShape*) shape, &m_obstacleCaches);
}