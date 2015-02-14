/*
 * EntityTest.h
 *
 *  Created on: 14.02.2015
 *      Author: aschaeffer
 */

#ifndef SRC_ENGINE_ENTITY_ENTITYTEST_H_
#define SRC_ENGINE_ENTITY_ENTITYTEST_H_

#include "EntitySystemBase.h"
#include "EntitySystem.h"

namespace inexor {
namespace entity {

class EntityTest
{
    public:
        EntityTest();
        virtual ~EntityTest();
        void PrintStats();
        void RunTests();
        void CleanupTests();
        void SubsystemTest();
        void AttributeTest();
        void InstanceCreationTest();
        void TypeCreationTest();
        void LowLevelTypeCreationTest();

    private:

        // Include the default reference counting implementation.
        IMPLEMENT_REFCOUNTING(EntityTest);

};

}
}

#endif /* SRC_ENGINE_ENTITY_ENTITYTEST_H_ */
