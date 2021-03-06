#include "stdafx.h"
#include "ColladaMeshInstance.h"

namespace DAVA
{
ColladaMeshInstance::ColladaMeshInstance(ColladaSkinnedMesh* _skinnedMesh)
{
    skinnedMesh = _skinnedMesh;
}

ColladaSkinnedMesh* ColladaMeshInstance::GetSkinnedMesh()
{
    return skinnedMesh;
}

void ColladaMeshInstance::AddPolygonGroupInstance(ColladaPolygonGroupInstance* instance)
{
    polyGroupInstances.push_back(instance);
}

void ColladaMeshInstance::Render()
{
    for (int i = 0; i < (int)polyGroupInstances.size(); ++i)
    {
        polyGroupInstances[i]->Render();
    }
}
};