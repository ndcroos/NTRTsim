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

#include <iostream>

#include "core/tgModel.h"
#include "core/tgSimView.h"
#include "core/tgSimViewGraphics.h"
#include "core/tgSimulation.h"
#include "core/tgWorld.h"
#include "tgcreator/tgRodInfo.h"

#include "BuildTestModel.h"
#include "BuildTestController.h"


int main(int argc, char** argv)
{
    std::cout << "AppBuilderTest" << std::endl;

    // First create the world
    tgWorld world;  
    // Second create the view
    tgSimView view(world);
    // Third create the simulation
    tgSimulation simulation(view);
    // Fourth create the models with their controllers and add the models to the
    // simulation
    BuildTestModel* const myModel = new BuildTestModel("BuildTestModel");
    BuildTestController* const pMuscleControl = new BuildTestController();
    myModel->attach(pMuscleControl);
    simulation.addModel(myModel);
    
    simulation.run(1000);
    //Teardown is handled by delete, so that should be automatic
    return 0;
}
