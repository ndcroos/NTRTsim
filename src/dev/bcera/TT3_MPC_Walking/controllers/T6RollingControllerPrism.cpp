/*
 * Copyright © 2012, United States Government, as represented by the
 * Administrator of the National Aeronautics and Space Administration.
 * All rights reserved.
 * 
 * The NASA Tensegrity Robotics Toolkit (NTRT) v1 platform is licensed
 * under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0.
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

/**
 * @file T6RollingControllerPrism.cpp
 * @brief Implementation of the rolling controller.
 * @author Edward Zhu
 * @version 1.0.0
 * $Id$
 */

// This module
#include "T6RollingControllerPrism.h"
// The C++ Standard Library
#include <iostream>
#include <string>
#include <cassert>
#include <algorithm>
#include <math.h>
// Boost Matrix Library
//#include "numeric/ublas/matrix.hpp"
//#include "numeric/ublas/io.hpp" 
#include "assign/list_of.hpp"
// Utility Library
#include "../utility.hpp"

namespace{
  double sf = 30;
  double worldTime = 0;
}

T6RollingController::Config::Config (double gravity, const std::string& mode, int face_goal, int activate_flag, int transfer_flag) : 
  m_gravity(gravity), m_mode(mode), m_face_goal(face_goal), m_activate_flag(activate_flag), m_transfer_flag(transfer_flag)
{
  assert(m_gravity >= 0);
  assert((m_face_goal >= 0) && (m_face_goal <= 19));

  if (m_mode.compare("face") != 0) {
    std::cout << "Config: invalid arguments" << std::endl;
    std::cout << "Usage: first arg is a string for mode ('face', 'path', or 'dr'). Second arg is based on mode, if 'face' was used, then an int between 0 and 7 is expected. If 'dr' was used, then a btVector3 is expected" << std::endl;
    std::cout << "Exiting..." << std::endl;
    exit(EXIT_FAILURE);
  }
}

T6RollingController::Config::Config (double gravity, const std::string& mode, btVector3 dr_goal, int activate_flag, int transfer_flag) :
  m_gravity(gravity), m_mode(mode), m_dr_goal(dr_goal), m_activate_flag(activate_flag), m_transfer_flag(transfer_flag)
{
  assert(m_gravity >= 0);
  if (mode.compare("dr") != 0) {
    std::cout << "Config: invalid arguments" << std::endl;
    std::cout << "Usage: first arg is a string for mode ('face', 'path', or 'dr'). Second arg is based on mode, if 'face' was used, then an int between 0 and 7 is expected. If 'dr' was used, then a btVector3 is expected" << std::endl;
    std::cout << "Exiting..." << std::endl;
    exit(EXIT_FAILURE);
  }
}

T6RollingController::Config::Config (double gravity, const std::string& mode, int *path, int pathSize, int activate_flag, int transfer_flag) :
  m_gravity(gravity), m_mode(mode), m_path(path), m_path_size(pathSize), m_activate_flag(activate_flag), m_transfer_flag(transfer_flag)
{
  assert(m_gravity >= 0);
  if (mode.compare("path") != 0) {
    std::cout << "Config: invalid arguments" << std::endl;
    std::cout << "Usage: first arg is a string for mode ('face', 'path', or 'dr'). Second arg is based on mode, if 'face' was used, then an int between 0 and 7 is expected. If 'dr' was used, then a btVector3 is expected" << std::endl;
    std::cout << "Exiting..." << std::endl;
    exit(EXIT_FAILURE);
  }
}

T6RollingController::T6RollingController(const T6RollingController::Config& config) : m_config(config)
{
  c_mode = config.m_mode;
  c_face_goal = config.m_face_goal;
  c_dr_goal = config.m_dr_goal;
  c_activate_flag = config.m_activate_flag;
  c_transfer_flag = config.m_transfer_flag;

  gravVectWorld.setX(0.0);
  gravVectWorld.setY(-config.m_gravity);
  gravVectWorld.setZ(0.0);
}

T6RollingController::~T6RollingController()
{
  m_controllers.clear();
}

void T6RollingController::onSetup(PrismModel& subject)
{
  std::cout << "onSetup: " << c_mode << " mode chosen" << std::endl;
  if (c_mode.compare("face") == 0) {
    std::cout << "onSetup: Goal face: " << c_face_goal << std::endl;
    controller_mode = 1;
    if (!isClosedFace(c_face_goal)) {
      std::cout << "onSetup: Goal face is not a closed face, exiting..." << std::endl;
      exit(EXIT_FAILURE);
    }
  }
  else if (c_mode.compare("dr") == 0) {
    std::cout << "onSetup: Dead reckoning goal: [" << c_dr_goal.x() << ", " 
	      << c_dr_goal.y() << ", " << c_dr_goal.z() << "]" << std::endl;
    controller_mode = 2;
  }
  else {
    std::cout << "onSetup: Path size is " << c_path_size << " elements" << std::endl;
    std::cout << "onSetup: Path: [";
    for (int i = 0; i < c_path_size-1; i++) {
      std::cout << *(c_path+i) << ", ";
    }
    std::cout << *(c_path+c_path_size-1) << "]" << std::endl;
    controller_mode = 3;
  }

  // Retrieve rods from model
  rods = subject.getAllRods();
  //std::cout << "onSetup: Number of rods: " << rods.size() << std::endl;

  // Convert from tgRod objects to btRigidBody objects
  for (size_t i = 0; i < rods.size(); i++) {
    tgRod* rod = rods[i];
    btRigidBody* rodBody = rod->getPRigidBody();
    rodBodies.push_back(rodBody);
  }

  tank = subject.getTank();

  // Retrieve normal vectors from model
  normVects = subject.getNormVects();

  // Find the rest length of the cables
  //restLength = subject.face0.norm();

  //std::cout << "onSetup: Cable rest length: " << restLength << std::endl;

  /*
    std::cout << "Face 0: " << face0Norm << std::endl;
    std::cout << "Face 1: " << face1Norm << std::endl;
    std::cout << "Face 2: " << face2Norm << std::endl;
    std::cout << "Face 3: " << face3Norm << std::endl;
    std::cout << "Face 4: " << face4Norm << std::endl;
    std::cout << "Face 5: " << face5Norm << std::endl;
    std::cout << "Face 6: " << face6Norm << std::endl;
    std::cout << "Face 7: " << face7Norm << std::endl;
    std::cout << "Face 8: " << face8Norm << std::endl;
    std::cout << "Face 9: " << face9Norm << std::endl;
    std::cout << "Face 10: " << face10Norm << std::endl;
    std::cout << "Face 11: " << face11Norm << std::endl;
    std::cout << "Face 12: " << face12Norm << std::endl;
    std::cout << "Face 13: " << face13Norm << std::endl;
    std::cout << "Face 14: " << face14Norm << std::endl;
    std::cout << "Face 15: " << face15Norm << std::endl;
    std::cout << "Face 16: " << face16Norm << std::endl;
    std::cout << "Face 17: " << face17Norm << std::endl;
    std::cout << "Face 18: " << face18Norm << std::endl;
    std::cout << "Face 19: " << face19Norm << std::endl;
  */

  /*
  // Create adjacency matrix (with open triangles connected)
  // 						  Columns: 0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18 19  // rows:
  node0Adj  = boost::assign::list_of(0)(1)(0)(0)(1)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(0)(0); // 0
  node1Adj  = boost::assign::list_of(1)(0)(1)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(0)(0)(0)(0); // 1
  node2Adj  = boost::assign::list_of(0)(1)(0)(1)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(0); // 2
  node3Adj  = boost::assign::list_of(0)(0)(1)(0)(1)(0)(0)(1)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0); // 3
  node4Adj  = boost::assign::list_of(1)(0)(0)(1)(0)(1)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0); // 4
  node5Adj  = boost::assign::list_of(0)(0)(0)(0)(1)(0)(1)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(1); // 5
  node6Adj  = boost::assign::list_of(0)(0)(0)(0)(0)(1)(0)(1)(0)(1)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0); // 6
  node7Adj  = boost::assign::list_of(0)(0)(0)(1)(0)(0)(1)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(1)(0); // 7
  node8Adj  = boost::assign::list_of(0)(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(0)(1)(0)(0)(0)(0)(0)(0)(1); // 8
  node9Adj  = boost::assign::list_of(0)(0)(0)(0)(0)(0)(1)(0)(1)(0)(1)(0)(0)(0)(0)(0)(0)(0)(0)(0); // 9
  node10Adj = boost::assign::list_of(0)(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(1)(0)(0)(0)(0)(0)(0)(1)(0); // 10
  node11Adj = boost::assign::list_of(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(1)(0)(0)(1)(0)(0)(0)(0); // 11
  node12Adj = boost::assign::list_of(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(0)(1)(0)(1)(0)(0)(0)(0)(0)(0); // 12
  node13Adj = boost::assign::list_of(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(1)(0)(1)(0)(0)(0); // 13
  node14Adj = boost::assign::list_of(0)(1)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(1)(0)(0)(0)(0); // 14
  node15Adj = boost::assign::list_of(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(0)(1)(0)(0)(1)(0)(0); // 15
  node16Adj = boost::assign::list_of(1)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(0)(0)(0)(0)(1); // 16
  node17Adj = boost::assign::list_of(0)(0)(1)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(0)(1)(0); // 17
  node18Adj = boost::assign::list_of(0)(0)(0)(0)(0)(0)(0)(1)(0)(0)(1)(0)(0)(0)(0)(0)(0)(1)(0)(0); // 18
  node19Adj = boost::assign::list_of(0)(0)(0)(0)(0)(1)(0)(0)(1)(0)(0)(0)(0)(0)(0)(0)(1)(0)(0)(0); // 19
  */

  // Open triangles not connected
  // 			    Columns: 0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18 19  // rows:
  node0Adj  = boost::assign::list_of(0)(1)(0)(0)(1)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(0)(0); // 0
  node1Adj  = boost::assign::list_of(1)(0)(1)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0); // 1
  node2Adj  = boost::assign::list_of(0)(1)(0)(1)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(0); // 2
  node3Adj  = boost::assign::list_of(0)(0)(1)(0)(0)(0)(0)(1)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0); // 3
  node4Adj  = boost::assign::list_of(1)(0)(0)(0)(0)(1)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0); // 4
  node5Adj  = boost::assign::list_of(0)(0)(0)(0)(1)(0)(1)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(1); // 5
  node6Adj  = boost::assign::list_of(0)(0)(0)(0)(0)(1)(0)(1)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0); // 6
  node7Adj  = boost::assign::list_of(0)(0)(0)(1)(0)(0)(1)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(1)(0); // 7
  node8Adj  = boost::assign::list_of(0)(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(0)(1)(0)(0)(0)(0)(0)(0)(1); // 8
  node9Adj  = boost::assign::list_of(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(1)(0)(0)(0)(0)(0)(0)(0)(0)(0); // 9
  node10Adj = boost::assign::list_of(0)(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(1)(0)(0)(0)(0)(0)(0)(1)(0); // 10
  node11Adj = boost::assign::list_of(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(0)(0)(0)(1)(0)(0)(0)(0); // 11
  node12Adj = boost::assign::list_of(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(0)(0)(0)(1)(0)(0)(0)(0)(0)(0); // 12
  node13Adj = boost::assign::list_of(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(1)(0)(1)(0)(0)(0); // 13
  node14Adj = boost::assign::list_of(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(1)(0)(0)(0)(0); // 14
  node15Adj = boost::assign::list_of(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(0)(1)(0)(0)(1)(0)(0); // 15
  node16Adj = boost::assign::list_of(1)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(0)(0)(0)(0)(0); // 16
  node17Adj = boost::assign::list_of(0)(0)(1)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(1)(0)(0)(0)(0); // 17
  node18Adj = boost::assign::list_of(0)(0)(0)(0)(0)(0)(0)(1)(0)(0)(1)(0)(0)(0)(0)(0)(0)(0)(0)(0); // 18
  node19Adj = boost::assign::list_of(0)(0)(0)(0)(0)(1)(0)(0)(1)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0)(0); // 19

  A.push_back(node0Adj);
  A.push_back(node1Adj);
  A.push_back(node2Adj);
  A.push_back(node3Adj);
  A.push_back(node4Adj);
  A.push_back(node5Adj);
  A.push_back(node6Adj);
  A.push_back(node7Adj);
  A.push_back(node8Adj);
  A.push_back(node9Adj);
  A.push_back(node10Adj);
  A.push_back(node11Adj);
  A.push_back(node12Adj);
  A.push_back(node13Adj);
  A.push_back(node14Adj);
  A.push_back(node15Adj);
  A.push_back(node16Adj);
  A.push_back(node17Adj);
  A.push_back(node18Adj);
  A.push_back(node19Adj);

  /*
  // Adjacency matrix debug print out
  for (size_t i = 0; i < A.size(); i++) {
  std::vector<int> row = A[i];
  for (size_t j = 0; j < row.size(); j++) {
  std::cout << row[j] << ", ";
  }
  std::cout << std::endl;
  }
  */

  // Check if adjacency matrix is square
  int rows = A.size();
  int cols = A[1].size();
  if (rows!= cols) {
    std::cout << "onSetup: Adjacency matrix not square, exiting..." << std::endl;
    //exit(EXIT_FAILURE);
  }

  // Set up controllers for the cables
  m_controllers.clear();
  actuators = subject.getAllActuators();	
  //std::cout << "onSetup: Number of actuators: " << actuators.size() << std::endl;
  for (size_t i = 0; i < actuators.size(); i++) {
    tgBasicActuator* const pActuator = actuators[i];
    assert(pActuator != NULL);
    tgBasicController* m_lenController = new tgBasicController(pActuator, restLength);
    m_controllers.push_back(m_lenController);
    //std::cout << "onSetup: Cable " << i << ": " << pActuator->getCurrentLength() << std::endl;
  }

  // Find the rest length and start length of the cables
  restLength = actuators[0]->getRestLength();
  startLength = actuators[0]->getStartLength();

  // Actuation policy table (With policy for open faces)
  // 						 Columns:  0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16  17  18  19  // rows:
  node0AP  = boost::assign::list_of(-1)( 0)(-1)(-1)(16)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)( 2)(-1)(-1)(-1); // 0
  node1AP  = boost::assign::list_of( 2)(-1)( 3)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1); // 1
  node2AP  = boost::assign::list_of(-1)( 1)(-1)(18)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)( 3)(-1)(-1); // 2
  node3AP  = boost::assign::list_of(-1)(-1)( 1)(-1)(-1)(-1)(-1)(13)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1); // 3
  node4AP  = boost::assign::list_of( 0)(-1)(-1)(-1)(-1)(12)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1); // 4
  node5AP  = boost::assign::list_of(-1)(-1)(-1)(-1)(17)(-1)(12)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(15); // 5
  node6AP  = boost::assign::list_of(-1)(-1)(-1)(-1)(-1)(15)(-1)( 9)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1); // 6
  node7AP  = boost::assign::list_of(-1)(-1)(-1)(19)(-1)(-1)(13)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(14)(-1); // 7
  node8AP  = boost::assign::list_of(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)( 9)(-1)(-1)(23)(-1)(-1)(-1)(-1)(-1)(-1)(11); // 8
  node9AP  = boost::assign::list_of(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(11)(-1)(10)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1); // 9
  node10AP = boost::assign::list_of(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)( 8)(-1)(21)(-1)(-1)(-1)(-1)(-1)(-1)(10)(-1); // 10
  node11AP = boost::assign::list_of(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)( 8)(-1)(-1)(-1)(-1)( 4)(-1)(-1)(-1)(-1); // 11
  node12AP = boost::assign::list_of(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)( 9)(-1)(-1)(-1)(-1)( 5)(-1)(-1)(-1)(-1)(-1)(-1); // 12
  node13AP = boost::assign::list_of(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(22)(-1)( 5)(-1)( 6)(-1)(-1)(-1); // 13
  node14AP = boost::assign::list_of(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)( 6)(-1)( 7)(-1)(-1)(-1)(-1); // 14
  node15AP = boost::assign::list_of(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(20)(-1)(-1)( 4)(-1)(-1)( 7)(-1)(-1); // 15
  node16AP = boost::assign::list_of(16)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(22)(-1)(-1)(-1)(-1)(-1)(-1); // 16
  node17AP = boost::assign::list_of(-1)(-1)(18)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(20)(-1)(-1)(-1)(-1); // 17
  node18AP = boost::assign::list_of(-1)(-1)(-1)(-1)(-1)(-1)(-1)(19)(-1)(-1)(21)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1); // 18
  node19AP = boost::assign::list_of(-1)(-1)(-1)(-1)(-1)(17)(-1)(-1)(23)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1)(-1); // 19

  actuationPolicy.push_back(node0AP);
  actuationPolicy.push_back(node1AP);
  actuationPolicy.push_back(node2AP);
  actuationPolicy.push_back(node3AP);
  actuationPolicy.push_back(node4AP);
  actuationPolicy.push_back(node5AP);
  actuationPolicy.push_back(node6AP);
  actuationPolicy.push_back(node7AP);
  actuationPolicy.push_back(node8AP);
  actuationPolicy.push_back(node9AP);
  actuationPolicy.push_back(node10AP);
  actuationPolicy.push_back(node11AP);
  actuationPolicy.push_back(node12AP);
  actuationPolicy.push_back(node13AP);
  actuationPolicy.push_back(node14AP);
  actuationPolicy.push_back(node15AP);
  actuationPolicy.push_back(node16AP);
  actuationPolicy.push_back(node17AP);
  actuationPolicy.push_back(node18AP);
  actuationPolicy.push_back(node19AP);
}

void T6RollingController::onStep(PrismModel& subject, double dt)
{
  std::cout << std::endl;
  std::cout << "OverallTime: " << worldTime << std::endl;
  std::cout << "RobotState: " << subject.robotState << std::endl;
  worldTime += dt;
  if (dt <= 0.0) {
    throw std::invalid_argument("onStep: dt is not positive");
  }
  //int number;
  //std::cin >> number;
  float step1[24] = {0.2630,0.4828,0.4828,0.3840,0.4360,0.3446,0.2630,0.3223,0.4066,0.3502,0.3395,0.4828,0.2630,0.4828,0.4006,0.2630,0.4828,0.4358,0.2630,0.2630,0.4026,0.4051,0.2630,0.4828};
  float step2[24] = {0.3839,0.3787,0.4139,0.3418,0.3319,0.4139,0.3319,0.4044,0.3319,0.4139,0.3542,0.4139,0.3836,0.3975,0.4044,0.3319,0.4139,0.3319,0.3840,0.3789,0.4139,0.4139,0.3319,0.3936};
  float step3[24] = {0.4343,0.4111,0.4228,0.4369,0.2569,0.4889,0.3446,0.3434,0.4889,0.4293,0.2569,0.2569,0.2569,0.2569,0.4889,0.4889,0.3184,0.4180,0.3023,0.3966,0.2569,0.4889,0.4889,0.2569};
  float step4[24] = {.4145,.4145,.3313,.3313,.3313,.3929,.4145,.4145,.4145,.3313,.3857,.3864,.3814,.3867,.3781,.3973,.3504,.4036,.4044,.3397,.3313,.4145,.4145,.3313};
  float step5[24] = {.4917,.2541,.4917,.2541,.4917,.2541,.4269,.4917,.3300,.4053,.4075,.2541,.3882,.3261,.4028,.2541,.2541,.4917,.3500,.4365,.3451,.4043,.2541,.4917};
  float step6[24] = {.4140,.3318,.4140,.3318,.4140,.3318,.3318,.3938,.3538,.4044,.4140,.3815,.3409,.4040,.4140,.3848,.3854,.3783,.4140,.3318,.4140,.3318,.3860,.3992};
  float step7[24] = {.4191,.3387,.3039,.4958,.2500,.4958,.4223,.2500,.2500,.4958,.4958,.3931,.4409,.3501,.2500,.3000,.4367,.4107,.4958,.2500,.4204,.4958,.2500,.2500};
  float step8[24] = {.3308,.4150,.3499,.4150,.3902,.3999,.4002,.3308,.3870,.3778,.4150,.3404,.3308,.4150,.3308,.4055,.4150,.4150,.3928,.3308,.3308,.4150,.3803,.3875};
  float step9[24] = {.4939,.4231,.2519,.2519,.2519,.2519,.4939,.4939,.4262,.4132,.4180,.4473,.2519,.4939,.3342,.3507,.4939,.2519,.2519,.4939,.4248,.3151,.3963,.3010};
  float step10[24] = {.4149,.3309,.3861,.3838,.3797,.3838,.3773,.3972,.4149,.4149,.3309,.3309,.3309,.3923,.4149,.4149,.4149,.3309,.3309,.4149,.4013,.3826,.3434,.4026};
  float step11[24] = {.3252,.4070,.4133,.2590,.3913,.3144,.4083,.2590,.4868,.2590,.4868,.2590,.4868,.2590,.4314,.4868,.2590,.4314,.4868,.2590,.4868,.2590,.4402,.3488};
  float step12[24] = {.3517,.4042,.4146,.6787,.3449,.4027,.4146,.3819,.4146,.3312,.4146,.3312,.4146,.3312,.3312,.3924,.3312,.4146,.3945,.3840,.3773,.3830,.3312,.4146};
  int swap = 1;
  float sequence[24];
  switch(roll_case){
  case 0:
    std::cout << "Case0" << std::endl;
    std::copy(step1, step1+24, sequence);
    break;
  case 1:
    std::cout << "Case1" << std::endl;
    std::copy(step2, step2+24, sequence);
    break;
  case 2:
    std::cout << "Case2" << std::endl;
    std::copy(step3, step3+24, sequence);
    break;
  case 3:
    std::cout << "Case3" << std::endl;
    std::copy(step4, step4+24, sequence);
    break;
  case 4:
    std::cout << "Case4" << std::endl;
    std::copy(step5, step5+24, sequence);
    break;
  case 5:
    std::cout << "Case5" << std::endl;
    std::copy(step6, step6+24, sequence);
    break;
  case 6:
    std::cout << "Case6" << std::endl;
    std::copy(step7, step7+24, sequence);
    break;
  case 7:
    std::cout << "Case7" << std::endl;
    std::copy(step8, step8+24, sequence);
    break;
  case 8:
    std::cout << "Case8" << std::endl;
    std::copy(step9, step9+24, sequence);
    break;
  case 9:
    std::cout << "Case9" << std::endl;
    std::copy(step10, step10+24, sequence);
    break;
  case 10:
    std::cout << "Case10" << std::endl;
    std::copy(step11, step11+24, sequence);
    break;
  case 11:
    std::cout << "Case11" << std::endl;
    std::copy(step12, step12+24, sequence);
    break;
  }
  for(int i=0;i<24;i++){
    m_controllers[i]->control(dt,sequence[i]*sf); //scale by scaling factor here
    std::cout << "Current Control: " << sequence[i]*sf << ", ";
    std::cout << "Roll Case: " << roll_case << std::endl;
    //std::cout << "Start Length: " << startLength << std::endl;
    //std::cout << "Actuator" <<  i << ": " << actuators[i]->getRestLength() << ", ";
    actuators[i]->moveMotors(dt);
    if(actuators[i]->getRestLength()-sequence[i]*sf > 0.01)
      swap = 0;
    if(actuators[i]->getRestLength()-sequence[i]*sf < -0.01)
      swap = 0;
  }
  std::cout << "WorldTime: " << worldTime << std::endl;
  float time_inc = 1.5;
  if((worldTime-last_step_time)>time_inc){
    roll_case = (roll_case+1)%12;
    last_step_time = worldTime;
  }
  //if(worldTime>0 && worldTime<time_inc) roll_case = 0;
  //if(worldTime>time_inc && worldTime<time_inc*2) roll_case = 1;
  //if(worldTime>time_inc*2 && worldTime<time_inc*3) roll_case = 2;
  //if(worldTime>time_inc*3 && worldTime<time_inc*4) roll_case = 3;
  //if(worldTime>time_inc*4 && worldTime<time_inc*5) roll_case = 4;
  //if(worldTime>time_inc*5 && worldTime<time_inc*6) roll_case = 5;
  //if(worldTime>time_inc*6 && worldTime<time_inc*7) roll_case = 6;
  //if(worldTime>time_inc*7 && worldTime<time_inc*8) roll_case = 7;
  //if(worldTime>time_inc*8 && worldTime<time_inc*9) roll_case = 8;
  //if(worldTime>time_inc*9 && worldTime<time_inc*10) roll_case = 9;
  //if(worldTime>time_inc*10 && worldTime<time_inc*11) roll_case = 10;
  //if(worldTime>time_inc*11 && worldTime<time_inc*12) roll_case = 11;

    
}

bool T6RollingController::checkOnGround()
{
  bool onGround = false;
	
  btVector3 rodVel = tank[0]->getLinearVelocity();
  double rodSpeed = rodVel.norm();
  if (abs(rodSpeed) < 0.001)
    onGround = true;

  return onGround;
}

int T6RollingController::contactSurfaceDetection()
{
  // Initialize variables
  double dotProd;
  double maxDotProd = 0;
  int currSurface = -1;

  // Get the gravity vector
  btVector3 robotGravity = getRobotGravity();

  // Find the dot product between the gravity vector and each face
  // As all normal vectors point away from the center of the robot,
  // The larger dot product indicates better alignment
  for (size_t i = 0; i < normVects.size(); i++) {
    dotProd = robotGravity.dot(normVects[i]);
    //std::cout << dotProd << std::endl;
    if (dotProd > maxDotProd) {
      maxDotProd = dotProd;
      currSurface = i;
    }
  }

  // Catch all error state
  if (currSurface == -1) {
    std::cout << "contactSurfaceDetection: No surface found" << std::endl;
  }

  std::cout << "contactSurfaceDetection: Contact surface: " << currSurface << std::endl;

  return currSurface;
}

int T6RollingController::headingSurfaceDetection(btVector3& travelDirWorld, int currFace)
{
  // Initialize variables
  double dotProd;
  double maxDotProd = 0;
  int goalSurface = -1;

  // Get the direction vector in robot frame
  btVector3 travelDirRobot = getRobotDir(travelDirWorld);

  // Find the dot product between the heading vector and each face
  // As all normal vectors point away from the center of the robot,
  // The larger dot product indicates better alignment
  for (size_t i = 0; i < normVects.size(); i++) {
    // if (isAdjacentFace(currFace, i)) {
    if (isClosedFace(i)) {
      dotProd = travelDirRobot.dot(normVects[i]);
      //std::cout << dotProd << std::endl;
      if (dotProd > maxDotProd) {
	maxDotProd = dotProd;
	goalSurface = i;
      }
    }
  }

  // Catch all error state
  if (goalSurface == -1) {
    std::cout << "headingSurfaceDetection: No surface found" << std::endl;
  }

  std::cout << "headingSurfaceDetection: Goal surface: " << goalSurface << std::endl;

  return goalSurface;
}

btVector3 T6RollingController::getRobotGravity() 
{
  btTransform worldTrans = tank[0]->getWorldTransform();
  //btTransform worldTrans = rodBodies[2]->getWorldTransform();
  btMatrix3x3 robotToWorld = worldTrans.getBasis();
  //std::cout << robotToWorld.getRow(0) << std::endl;
  //std::cout << robotToWorld.getRow(1) << std::endl;
  //std::cout << robotToWorld.getRow(2) << std::endl;
  // The basis of getWorldTransform() returns the rotation matrix from robot frame
  // to world frame. Invert this matrix to go from world to robot frame
  btMatrix3x3 worldToRobot = robotToWorld.inverse();
  // Transform the gravity vector from world frame to robot frame
  btVector3 gravVectRobot = worldToRobot * gravVectWorld;
  //std::cout << "Gravity vector in robot frame: " << gravVectRobot << std::endl;
  return gravVectRobot;
}

btVector3 T6RollingController::getRobotDir(btVector3 dirVectWorld) 
{
  btTransform worldTrans = tank[0]->getWorldTransform();
  //btTransform worldTrans = rodBodies[2]->getWorldTransform();
  btMatrix3x3 robotToWorld = worldTrans.getBasis();
  // The basis of getWorldTransform() returns the rotation matrix from robot frame
  // to world frame. Invert this matrix to go from world to robot frame
  btMatrix3x3 worldToRobot = robotToWorld.inverse();
  // Transform the gravity vector from world frame to robot frame
  btVector3 dirVectRobot = (worldToRobot * dirVectWorld).normalize();
  //std::cout << "Gravity vector in robot frame: " << gravVectRobot << std::endl;
  return dirVectRobot;
}

std::vector<int> T6RollingController::findPath(std::vector< std::vector<int> >& adjMat, int startNode, int endNode) 
{
  // Check validity of start and end nodes
  int nodes = adjMat.size();
  if (startNode > nodes) {
    std::cout << "findPath: Start node out of bounds, exiting..." << std::endl;
    exit(EXIT_FAILURE);
  }
  else if (endNode > nodes) {
    std::cout << "findPath: End node out of bounds, exiting..." << std::endl;
    exit(EXIT_FAILURE);
  }

  // Initialize status flags for reached destination or no solution
  bool endReached = false;
  bool noSolution = false;

  if (endNode == startNode) {
    endReached = true;
  }

  // Create vectors to hold unvisited and visited sets and distances
  // the unvisited set initializs with all nodes and the visited set
  // initializes as an empty vector
  std::vector<int> unvisited;
  for (int i = 0; i < nodes; i++) {
    unvisited.push_back(i);
  }
  //std::cout << "Unvisited init: ";
  //utility::printVector(unvisited);
  std::vector<int> visited;
  std::vector<int> distances(nodes, 1000); // Intialize distance vector to some arbitrarily large value
  distances[startNode] = 0;

  // Initialize path vectors
  std::vector<int> prev(nodes, -1); // Initialize path pointer vector to -1
  std::vector<int> pathVect;
  pathVect.push_back(endNode); // Last element in pathVect is the destination node

  while ((endReached == false) && (noSolution == false)) {
    // Initialize variables
    bool currNodeFound = false;
    int currNode = -1;
    int minDist = 1000;

    // Find next unvisited node with the shortest tentative distance
    for (size_t uvNode_idx = 0; uvNode_idx < unvisited.size(); uvNode_idx++) {
      if (distances[unvisited[uvNode_idx]] < minDist) {
	minDist = distances[unvisited[uvNode_idx]];
	currNode = unvisited[uvNode_idx];
	currNodeFound = true;
      }
    }
    //std::cout << "Current node : " << currNode << std::endl;

    // Check if new node is found
    if (currNodeFound == false) {
      std::cout << "findPath: No solution found" << std::endl;
      noSolution = true;
    }

    // If node is found, continue with path finding
    if (noSolution == false) {
      // Extract row corresponding to the current node
      std::vector<int> currRow = adjMat[currNode];
      std::vector<int> neighbors;
      std::vector<int> weights;
      // Find its neighbors and their corresponding weights
      for (size_t col_idx = 0; col_idx < currRow.size(); col_idx++) {
	if (currRow[col_idx] > 0) {
	  neighbors.push_back(col_idx);
	  weights.push_back(currRow[col_idx]);
	}
      }
      //utility::printVector(neighbors);
      //utility::printVector(weights);
      // Check if neighbors have been visited already, if not calculate distance
      for (size_t neigh_idx = 0; neigh_idx < neighbors.size(); neigh_idx++) {
	if (find(visited.begin(), visited.end(), neighbors[neigh_idx]) != visited.end()) {
	  continue;
	}
	else if (neighbors[neigh_idx] == endNode) {
	  prev[endNode] = currNode;
	  endReached = true;
	  break;
	}
	else {
	  int tentativeDistance = distances[currNode] + weights[neigh_idx];
	  if (tentativeDistance < distances[neighbors[neigh_idx]]) {
	    distances[neighbors[neigh_idx]] = tentativeDistance;
	    prev[neighbors[neigh_idx]] = currNode;
	  }
	}
      }

      // Finished investigating current node, move it from unvisted to visited
      visited.push_back(currNode);
      unvisited.erase(find(unvisited.begin(), unvisited.end(), currNode));
      //utility::printVector(visited);
      //utility::printVector(unvisited);
    }
  }

  int node = endNode;
  while (node != startNode) {
    pathVect.insert(pathVect.begin(), prev[node]);
    node = prev[node];
  }

  //std::cout << "End Reached: " << endReached << ", No Solution: " << noSolution << std::endl;
  return pathVect;
}

bool T6RollingController::stepToFace(double dt)
{
	// Initialize flags
	bool stepFinished = false;
	bool isOnPath = false;
	// Length for cables to retract to
	//double controlLength = 0.2;
	double controlLength = restLength * 0;
	
	int cableToActuate = -1;
	// Get which cable to actuate from actuation policy table
	if (path.size() > 1) {
		cableToActuate = actuationPolicy[path[0]][path[1]];

		// Find current face
		int currFace = contactSurfaceDetection();

		// Perform actuation from one closed face to another
		if (isClosedFace(path[0])) {
			if (cableToActuate >= 0) {
				// path[0] is current face, path[1] is the adjacent open face, 
				// path[2] is the next closed face
				// Check if the robot has reached the next closed face
				if (currFace != path[2]) {
					m_controllers[cableToActuate]->control(dt, controlLength);
					actuators[cableToActuate]->moveMotors(dt);
					// std::cout << "stepToFace: (Closed -> Closed) Stepping..." << std::endl;
					resetCounter++;
					if (resetCounter > 3.0/dt) resetFlag = true;
				}
				// If it has, return all cables to rest length
				else {
					resetFlag = true;
					path.erase(path.begin(),path.begin()+2);
					utility::printVector(path);
					if (path.size() == 1) {
						stepFinished = true;
					}
				}
			}
			// Triggers if element called from actuation policy table is -1
			else {
				std::cout << "stepToFace: No actuation scheme available, exiting..." << std::endl;
				//exit(EXIT_FAILURE);
			}
		}
		// Perfom actuation to get from an open face to a closed face
		else {
			if (cableToActuate >= 0) {
				// Check to see if robot has reached a closed face
				if (!isClosedFace(currFace)) {
					m_controllers[cableToActuate]->control(dt, controlLength);
					actuators[cableToActuate]->moveMotors(dt);
					// std::cout << "stepToFace: (Open -> Closed) Stepping..." << std::endl;
					resetCounter++;
					if (resetCounter > 3.0/dt) resetFlag = true;
				}
				// If it has, return all cables to rest length
				else {
					resetFlag = true;
					path.erase(path.begin());
					utility::printVector(path);
					stepFinished = true;
				}
			}
			// Triggers if element called from actuation policy table is -1
			else {
				std::cout << "stepToFace: No actuation scheme available, exiting..." << std::endl;
				//exit(EXIT_FAILURE);
			}
		}
	}
	else stepFinished = true;

	
	
	return stepFinished;
}

bool T6RollingController::isClosedFace(int desFace)
{
  bool isClosedFace = false;
  std::vector<int> closedFaces;
  closedFaces  = boost::assign::list_of(0)(2)(5)(7)(8)(10)(13)(15);

  for (size_t i = 0; i < closedFaces.size(); i++) {
    if (desFace == closedFaces[i]) {
      isClosedFace = true;
    }
  }

  return isClosedFace;
}

bool T6RollingController::setAllActuators(std::vector<tgBasicController*>& controllers, 
					  std::vector<tgBasicActuator*>& actuators, 
					  double setLength, double dt)
{
  bool returnFin = true;
  for (size_t i = 0; i < actuators.size(); i++) {
    controllers[i]->control(dt, setLength);
    actuators[i]->moveMotors(dt);
    if (actuators[i]->getRestLength()-setLength > 0.01) {
      returnFin = false;
    }
    if (actuators[i]->getRestLength()-setLength < -0.01) {
      returnFin = false;
    }
  }
  std::cout << "Resetting Cable Lengths " << std::endl;
  return returnFin;
}