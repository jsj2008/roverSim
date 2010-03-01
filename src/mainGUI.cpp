#include "mainGUI.h"
#include "camera.h"
#include "terrain.h"
#include "sr2rover.h"

MainGUI::MainGUI(QWidget *parent)
:
QMainWindow(parent),
m_oTool(this),
m_simTool(this),
m_tTool(this)
{
    qDebug("UI setup");
    setupUi(this);
	move(240,22);
    resize(1200,700);
	
    m_oTool.close();
    m_simTool.close();
    m_tTool.close();

	SController = new simControl(glView);
	
    // menu bar connections
// world menu
    connect(actionSim_Timing, SIGNAL(triggered()), &m_simTool, SLOT(show()));
// rover menu
	connect(actionNew_Rover, SIGNAL(triggered()), this, SLOT(newRover()));
	// setup the rover view menu
	connect(actionFree_View, SIGNAL(triggered()), this, SLOT(cameraFreeView()));
	connect(actionRover_Center, SIGNAL(triggered()), this, SLOT(cameraRoverCenter()));
	connect(actionRover_Follow, SIGNAL(triggered()), this, SLOT(cameraRoverFollow()));
	connect(actionRover_View, SIGNAL(triggered()), this, SLOT(cameraRoverView()));
	connect(actionRover_PanCam, SIGNAL(triggered()), this, SLOT(cameraRoverPanCam()));
	menuRoverView->setEnabled(false);
// obstacles menu
    connect(actionRandomize_Obstacles, SIGNAL(triggered()), SController, SLOT(generateObstacles()));
    connect(actionRemove_Obstacles, SIGNAL(triggered()), SController, SLOT(removeObstacles()));
    connect(actionObstacle_Parameters, SIGNAL(triggered()), &m_oTool, SLOT(show()));
// terrain menu
    connect(actionOpen_Terrain, SIGNAL(triggered()), this, SLOT(openGround()));
    connect(actionSave_Terrain, SIGNAL(triggered()), this, SLOT(saveGround()));
    connect(actionFlatten_Terrain, SIGNAL(triggered()), this, SLOT(flattenGround()));
   	connect(actionTerrain_Parameters, SIGNAL(triggered()), &m_tTool, SLOT(show()));

    // tool bar Simulation timing
    connect(&m_simTool, SIGNAL(paramUpdate()), this, SLOT(stepTimevals()));
    // tool bar obstacle generation button signal
    connect(&m_oTool, SIGNAL(regenerateObstacles()), this, SLOT(generateObstacles()));
    // tool bar terrain gravity update
    connect(&m_tTool, SIGNAL(gravityUpdate()), this, SLOT(simGravity()));
    // tool bar terrain scale update
    connect(&m_tTool, SIGNAL(scaleUpdate()), this, SLOT(rescaleGround()));

// server connections
	connect(&m_tcpServer, SIGNAL(newConnection()),this, SLOT(serverAcceptConnect()));
	m_tcpSocket = NULL;
	this->serverStart();
	
    connect(glView, SIGNAL(refreshView()), this, SLOT(updateGUI()));
	
    labelTerrainFilename->setText(SController->getGround()->terrainFilename());
    
    m_tTool.setScale(SController->getGround()->terrainScale());

	glView->setFocusPolicy(Qt::ClickFocus);
	textConsole->setFocusPolicy(Qt::ClickFocus);
}

MainGUI::~MainGUI()
{
    qDebug("deleting UI");
}

void MainGUI::closeEvent(QCloseEvent *event)
{
	m_tcpSocket->disconnectFromHost();
    delete SController;
    event->accept();
}

void MainGUI::stepTimevals()
{
    SController->stepTimevals(m_simTool.getTimeStep(),m_simTool.getFixedTimeStep(),m_simTool.getSubSteps());
}

void MainGUI::simGravity()
{
    SController->setGravity(m_tTool.gravity());
}

void MainGUI::openGround()
{
	// open an Open File dialog to look for a PNG image to represent a height map
    QString filename = QFileDialog::getOpenFileName(this,tr("Open Terrain"), tr("/Users"),tr("Image File (*.png)"));
	if(filename == NULL) return; // if cancel is pressed dont do anything
	SController->openNewGround(filename);
	labelTerrainFilename->setText(SController->getGround()->terrainFilename());
	m_tTool.setScale(SController->getGround()->terrainScale());
	menuRoverView->setEnabled(false);
}

void MainGUI::saveGround()
{
	// open a Save File dialog and select location and filename
	QString filename = QFileDialog::getSaveFileName(this,tr("Save Terrain PNG"), tr("/Users"),tr("Image File (*.png)"));
	if(filename == NULL) return; // if cancel is pressed dont do anything

    if(!filename.endsWith(".png")) filename.append(".png");
	SController->getGround()->saveTerrain(filename);
}

void MainGUI::flattenGround()
{
	SController->flattenGround();
	menuRoverView->setEnabled(false);
}

void MainGUI::rescaleGround()
{
    SController->rescaleGround(m_tTool.scale());
    SController->setGravity(m_tTool.gravity());
	menuRoverView->setEnabled(false);
}

void MainGUI::generateObstacles()
{
	SController->m_obstType = m_oTool.obstacleType();
	SController->m_obstCount = m_oTool.obstacleCount();
	SController->m_dropHeight = m_oTool.dropHeight();
	SController->m_minObstSize = btVector3(m_oTool.minOLength(),m_oTool.minOWidth(),m_oTool.minOHeight());
	SController->m_maxObstSize = btVector3(m_oTool.maxOLength(),m_oTool.maxOWidth(),m_oTool.maxOHeight());
	SController->m_minObstYaw = m_oTool.minOYaw();
	SController->m_maxObstYaw = m_oTool.maxOYaw();
	SController->m_obstDensity = m_oTool.density();
	SController->generateObstacles();
}

void MainGUI::removeAllObstacles()
{
    m_oTool.setObstacleCount(0);
	SController->removeObstacles();
}

void MainGUI::newRover()
{
	SController->newRover();
	menuRoverView->setEnabled(true);
}

void MainGUI::cameraFreeView(){glView->getCamera()->cameraFreeView(); glView->setViewAngle(1.0);}
void MainGUI::cameraRoverCenter(){glView->getCamera()->cameraRoverCenter(); glView->setViewAngle(1.0);}
void MainGUI::cameraRoverFollow(){glView->getCamera()->cameraRoverFollow(); glView->setViewAngle(1.0);}
void MainGUI::cameraRoverView(){glView->getCamera()->cameraRoverView(); glView->setViewAngle(0.5);}
void MainGUI::cameraRoverPanCam(){glView->getCamera()->cameraRoverPanCam(); glView->setViewAngle(1.0);}

/////////////////////////////////////////
// user input
/////////////
void MainGUI::keyPressEvent(QKeyEvent *event)
{
	if(!glView->hasFocus()) return;
    switch(event->key()){
        case ' ':
        {
			SController->pauseSim();
            return;
        }
		case 'P':
		{
			qDebug("stop drawing");
			glView->stopDrawing();
			return;
		}
		case 'O':
		{
			qDebug("start drawing");
			glView->startDrawing();
			return;
		}
        case 'V':
        {
            glView->getCamera()->cameraToggleView();
            if(glView->getCamera()->cameraView == RoverView) glView->setViewAngle(0.5);
            else glView->setViewAngle(1.0);
            return;
        }
        case '[':
        {
			SController->getGround()->terrainRaise(glView->getCamera()->cameraDirection(),m_tTool.increment(),m_tTool.diameter());
            return;
        }
        case ']':
        {
            SController->getGround()->terrainLower(glView->getCamera()->cameraDirection(),m_tTool.increment(),m_tTool.diameter());
            return;
        }
        case '-':
            glView->setViewAngle(glView->getViewAngle()-0.1);
            return;
        case '=':
            glView->setViewAngle(glView->getViewAngle()+0.1);
            return;
        default:
            break;
    }

	SR2rover *sr2;
	sr2 = SController->getRover();
    if(sr2){
        switch(event->key()){
        case 'B':
            {
                sr2->resetRobot();
                break;
            }
        case 'F':
            {
                //sr2->m_bodyParts[0]->applyCentralImpulse(btVector3(sin(sr2->heading),cos(sr2->heading),10));
                break;
            }
        case 'L':
            {
                sr2->toggleSensors();
                break;
            }
        case Qt::Key_Up:
            {
                if(event->modifiers() & Qt::ShiftModifier){
                    if(sr2->tiltAngle >= 45) break;
                    sr2->tiltAngle += 0.5;
                }
                else{
                    sr2->incRightSpeed(1);
                    sr2->incLeftSpeed(1);
                }
                break;
            }
        case Qt::Key_Down:
            {
                if(event->modifiers() & Qt::ShiftModifier){
                    if(sr2->tiltAngle <= -45) break;
                    sr2->tiltAngle -= 0.5;
                }
                else{
                    sr2->incRightSpeed(-0.5);
                    sr2->incLeftSpeed(-0.5);
                }
                break;
            }
        case Qt::Key_Right:
            {
                if(event->modifiers() & Qt::ShiftModifier) sr2->panAngle -= 0.5;
                else{
                    //m_oldSpeed = (sr2->leftSpeed>sr2->rightSpeed)? sr2->leftSpeed:sr2->rightSpeed;
                    sr2->incRightSpeed(-0.1);
                    sr2->incLeftSpeed(0.1);
                }
                break;
            }
        case Qt::Key_Left:
            {
                if(event->modifiers() & Qt::ShiftModifier) sr2->panAngle += 0.5;
                else{
                    //m_oldSpeed = (sr2->leftSpeed>sr2->rightSpeed)? sr2->leftSpeed:sr2->rightSpeed;
                    sr2->incRightSpeed(0.1);
                    sr2->incLeftSpeed(-0.1);
                }
                break;
            }

        default:
            break;
        }
    }
}

void MainGUI::keyReleaseEvent(QKeyEvent *event)
{
	if(!glView->hasFocus()) return;
	SR2rover *sr2;
	sr2 = SController->getRover();
 
   if(!sr2) return;	
    switch(event->key()){
        case Qt::Key_Up:
        {
            //sr2->stopRobot();
            break;
        }
        case Qt::Key_Down:
        {
            sr2->stopRobot();	
            break;
        }
        case Qt::Key_Left:
        {
            float avgSpeed = (sr2->rightSpeed + sr2->leftSpeed) / 2;
            sr2->setLeftSpeed(avgSpeed);
            sr2->setRightSpeed(avgSpeed);
            break;
        }
        case Qt::Key_Right:
        {
            float avgSpeed = (sr2->rightSpeed + sr2->leftSpeed) / 2;
            sr2->setRightSpeed(avgSpeed);
            sr2->setLeftSpeed(avgSpeed);
            break;
        }
    }
}

/////////////////////////////////////////
// GUI labels
/////////////
void MainGUI::updateGUI()
{
	SR2rover *sr2;
	sr2 = SController->getRover();
	// GUI SR2 properties
    if(sr2){
        labelRoverPosition->setText(QString("(%1 ,%2)").arg(sr2->position.x(),0,'f',2).arg(sr2->position.y(),0,'f',2));
        labelRoverSpeed->setText(QString("(%1 ,%2)").arg(sr2->leftSpeed,0,'f',2).arg(sr2->rightSpeed,0,'f',2));
		labelDiffAngle->setText(QString().setNum(RADTODEG(sr2->differentialAngle),'f',1));
		labelRoverHeading->setText(QString().setNum(RADTODEG(sr2->heading),'f',2));
        labelRoverPitch->setText(QString().setNum(RADTODEG(sr2->pitch),'f',1));
        labelRoverRoll->setText(QString().setNum(RADTODEG(sr2->roll),'f',1));
        labelLeftEncoder->setText(QString().setNum(sr2->leftEncoder()));
        labelRightEncoder->setText(QString().setNum(sr2->rightEncoder()));
    }

// GUI camera properties
    btVector3 cameraParam = glView->getCamera()->cameraPitchYawZoom();
	labelCameraView->setText(glView->getCamera()->cameraViewName());
    labelCameraYaw->setText(QString().setNum(cameraParam.x(),'f',1));
    labelCameraPitch->setText(QString().setNum(cameraParam.y(),'f',1));
    labelCameraDist->setText(QString().setNum(cameraParam.z(),'f',1));
    labelCameraZoom->setText(QString().setNum(glView->getViewAngle(),'f',1));
    labelCameraCrosshair->setText(QString("(%1 ,%2)").arg(glView->getCamera()->cameraDirection().x(),0,'f',1)
                                  .arg(glView->getCamera()->cameraDirection().y(),0,'f',1));
}

/////////////////////////////////////////
// network functions
/////////////
void MainGUI::serverStart()
{
	if (!m_tcpServer.isListening() && !m_tcpServer.listen()) 
	{
		QMessageBox::StandardButton ret = QMessageBox::critical(this,
			tr("Server error"),
			tr("Unable to start loopback connection: %1.")
			.arg(m_tcpServer.errorString()),
			QMessageBox::Retry
			| QMessageBox::Cancel);
		if (ret == QMessageBox::Cancel)
			return;
	}
	textConsole->append(QString("Server Started at:")+QHostAddress(QHostAddress::LocalHost).toString()+"\n");
}
void MainGUI::serverAcceptConnect()
{
	m_tcpSocket = m_tcpServer.nextPendingConnection();
    connect(m_tcpSocket, SIGNAL(readyRead()),this, SLOT(serverUpdate()));
    connect(m_tcpSocket, SIGNAL(disconnected()), m_tcpSocket, SLOT(deleteLater()));
	m_tcpServer.close();
	m_blockSize = 0;
	inStream.setDevice(m_tcpSocket);
	inStream.setVersion(QDataStream::Qt_4_0);
	textConsole->append(QString("Connection:")+m_tcpSocket->peerAddress().toString()+"\n");
}
void MainGUI::serverUpdate()
{
	qint64 byteAvailable = m_tcpSocket->bytesAvailable();
	
	if (m_blockSize == 0) {
        if (byteAvailable < (int)sizeof(qint64)) return;
        inStream >> m_blockSize;
    }

    if (byteAvailable < m_blockSize) return;

	quint8 command;
	bool state;
	quint8 parameter;
	inStream >> command >> state >> parameter;
	
	m_blockSize -= (sizeof(command) + sizeof(state) + sizeof(parameter));
	
	char *data = NULL;
	if(m_blockSize > 0){
		data = new char[m_blockSize];
		inStream.readRawData(data, m_blockSize);
	}
	
	switch((serverCommand) command){
		case ROBOT:
			//if(state) SController->setRoverData(parameter,data);
			//else SController->getRoverData(parameter,data);
		break;
		case OBSTACLES:
		break;
		case TERRAIN:
		break;
		case SIMULATION:
		break;
		case STRING:
		{
			textConsole->append(QString(QByteArray(data,m_blockSize))+"\n");
			break;
		}
		default:
		break;
	}
	m_blockSize = 0;
	if(data) delete [] data;
}
