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

This class is a modified version of btSoftBodyCollisionShape
* from Bullet 2.82. A copy of the z-lib license from the Bullet Physics
* Library is provided below:

Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

/**
 * @file cordeCollisionShape.cpp
 * @brief Collision shape for the Corde Collision object
 * Largely based on bullet's btSoftBodyCollisionShape in btSoftBodyInternals.h
 * @author Brian Mirletz
 * $Id$
 */

#include "cordeCollisionShape.h"

#include "cordeCollisionObject.h"

cordeCollisionShape::cordeCollisionShape(cordeCollisionObject* objectShape) :
localScaling(1, 1, 1),
m_collisionMargin( 0.04), // Default by Bullet's user manual, gets overwritten by cordeCollisionObject
p_objectShape(objectShape)
{
	m_shapeType = SOFTBODY_SHAPE_PROXYTYPE;
}

void cordeCollisionShape::getAabb(const btTransform& t,btVector3& aabbMin,btVector3& aabbMax) const
{
	/* t is usually identity, except when colliding against btCompoundShape. See Issue 512 */
	const btVector3	mins = p_objectShape->m_bounds[0];
	const btVector3	maxs = p_objectShape->m_bounds[1];
	const btVector3	crns[]={t*btVector3(mins.x(),mins.y(),mins.z()),
		t*btVector3(maxs.x(),mins.y(),mins.z()),
		t*btVector3(maxs.x(),maxs.y(),mins.z()),
		t*btVector3(mins.x(),maxs.y(),mins.z()),
		t*btVector3(mins.x(),mins.y(),maxs.z()),
		t*btVector3(maxs.x(),mins.y(),maxs.z()),
		t*btVector3(maxs.x(),maxs.y(),maxs.z()),
		t*btVector3(mins.x(),maxs.y(),maxs.z())};
	aabbMin=aabbMax=crns[0];
	for(int i=1;i<8;++i)
	{
		aabbMin.setMin(crns[i]);
		aabbMax.setMax(crns[i]);
	}
}

void	cordeCollisionShape::setLocalScaling(const btVector3& scaling) 
{ 
	localScaling = scaling;
}

const btVector3& cordeCollisionShape::getLocalScaling() const
{
	return localScaling;
} 