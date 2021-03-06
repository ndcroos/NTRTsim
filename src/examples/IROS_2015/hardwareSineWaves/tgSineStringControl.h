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

#ifndef TG_SINE_STRING_CONTRL_H
#define TG_SINE_STRING_CONTRL_H

#include "core/tgSpringCableActuator.h"

// Forward declarations
class btRigidBody;
class tgImpedanceController;

class tgSineStringControl : public tgObserver<tgSpringCableActuator>
{
public:
 
    tgSineStringControl(const double controlStep,
							tgImpedanceController* p_ipc,
							const double amplitude,
							const double frequency,
							const double phase,
							const double offset,
							const double length);
    
    virtual ~tgSineStringControl();
    
    virtual void onStep(tgSpringCableActuator& subject, double dt);
	

    
    const double getCommandedTension() const
    {
        return m_commandedTension;
    }
	
private:
    /**
     * Member variable for keeping track of how long its been since the 
     * last update step
     */
    double m_controlTime;
    double m_totalTime;
    /**
     * How often this controller updates. Must be non-negative. Zero
     * means it updates every timestep. Units are seconds
     */
    const double m_controlStep;
    
    double m_commandedTension;
    
    /**
     * CPG related parameters.
     */
    const double cpgAmplitude;
    const double cpgFrequency;
    const double phaseOffset;
    const double offsetSpeed;
    /**
     * Phase offsets for the Sine Waves
     */
    
    
    /**
     * Parameters that consolidate the sine wave computations within
     * the update code. Cycle deals with sin(theta), and target handles
     * offset + amplitude * cycle
     */
    double cycle;
    double target;
    
    /**
     * Control parameters unique to this, borrowed from tgBaseCPGNode
     */
    const double  m_controlLength;
    
    tgImpedanceController* m_pMotorControl;
};


#endif
