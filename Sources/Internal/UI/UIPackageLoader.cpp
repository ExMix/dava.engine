/*==================================================================================
    Copyright (c) 2008, binaryzebra
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    * Neither the name of the binaryzebra nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE binaryzebra AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL binaryzebra BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
=====================================================================================*/


#include "UIPackageLoader.h"

#include "Base/ObjectFactory.h"
#include "FileSystem/FilePath.h"
#include "FileSystem/YamlNode.h"
#include "FileSystem/YamlEmitter.h"
#include "UI/UIYamlLoader.h"
#include "UI/UIControl.h"
#include "UI/UIStaticText.h"
#include "UI/UIControlHelpers.h"
#include "UI/UIPackage.h"
#include "UI/Components/UIComponent.h"

namespace DAVA
{

UIPackageLoader::UIPackageLoader(AbstractUIPackageBuilder *builder) : builder(builder)
{
}

UIPackageLoader::~UIPackageLoader()
{
    builder = nullptr;
}

UIPackage *UIPackageLoader::LoadPackage(const FilePath &packagePath)
{
    if (!loadingQueue.empty())
    {
        DVASSERT(false);
        loadingQueue.clear();
    }

    UIPackage *packageInCache = builder->FindInCache(packagePath.GetStringValue());
    if (packageInCache != nullptr)
    {
        RefPtr<UIPackage> package = packageInCache->Clone();
        return SafeRetain(package.Get());
    }

    if (!packagePath.Exists())
        return nullptr;
    
    RefPtr<YamlParser> parser(YamlParser::Create(packagePath));
    
    if (parser.Get() == nullptr)
    {
        return nullptr;
    }

    YamlNode *rootNode = parser->GetRootNode();
    if (!rootNode)//empty yaml equal to empty UIPackage
    {
        RefPtr<UIPackage> package = builder->BeginPackage(packagePath);
        builder->EndPackage();
        return SafeRetain(package.Get());
    }
    
    return LoadPackage(rootNode, packagePath);
}
    
UIPackage *UIPackageLoader::LoadPackage(const YamlNode *rootNode, const FilePath &packagePath)
{
    const YamlNode *headerNode = rootNode->Get("Header");
    if (!headerNode)
        return nullptr;
    
    const YamlNode *versionNode = headerNode->Get("version");
    if (versionNode == nullptr || versionNode->GetType() != YamlNode::TYPE_STRING)
        return nullptr;
    
    RefPtr<UIPackage> package = builder->BeginPackage(packagePath);
    
    const YamlNode *importedPackagesNode = rootNode->Get("ImportedPackages");
    if (importedPackagesNode)
    {
        int32 count = (int32) importedPackagesNode->GetCount();
        for (int32 i = 0; i < count; i++)
            builder->ProcessImportedPackage(importedPackagesNode->Get(i)->AsString(), this);
    }
    
    const YamlNode *controlsNode = rootNode->Get("Controls");
    if (controlsNode)
    {
        int32 count = (int32) controlsNode->GetCount();
        for (int32 i = 0; i < count; i++)
        {
            const YamlNode *node = controlsNode->Get(i);
            QueueItem item;
            item.name = node->Get("name")->AsString();
            item.node = node;
            item.status = STATUS_WAIT;
            loadingQueue.push_back(item);
        }
        
        for (int32 i = 0; i < count; i++)
        {
            if (loadingQueue[i].status == STATUS_WAIT)
            {
                loadingQueue[i].status = STATUS_LOADING;
                LoadControl(loadingQueue[i].node, true);
                loadingQueue[i].status = STATUS_LOADED;
            }
        }
        
        loadingQueue.clear();
    }
    builder->EndPackage();
    
    return SafeRetain(package.Get());
}

    
bool UIPackageLoader::LoadControlByName(const String &name)
{
    size_t size = loadingQueue.size();
    for (size_t index = 0; index < size; index++)
    {
        if (loadingQueue[index].name == name)
        {
            switch (loadingQueue[index].status)
            {
                case STATUS_WAIT:
                    loadingQueue[index].status = STATUS_LOADING;
                    LoadControl(loadingQueue[index].node, true);
                    loadingQueue[index].status = STATUS_LOADED;
                    return true;
                    
                case STATUS_LOADED:
                    return true;
                    
                case STATUS_LOADING:
                    return false;
                    
                default:
                    DVASSERT(false);
                    return false;
            }
        }
    }
    return false;
}

void UIPackageLoader::LoadControl(const YamlNode *node, bool root)
{
    UIControl *control = nullptr;
    const YamlNode *pathNode = node->Get("path");
    const YamlNode *prototypeNode = node->Get("prototype");
    const YamlNode *classNode = node->Get("class");
    const YamlNode *nameNode = node->Get("name");

    //DVASSERT(nameNode || pathNode);
    
    if (pathNode)
    {
        control = builder->BeginControlWithPath(pathNode->AsString());
    }
    else if (prototypeNode)
    {
        const YamlNode *customClassNode = node->Get("customClass");
        const String *customClass = customClassNode == nullptr ? nullptr : &(customClassNode->AsString());
        String controlName = prototypeNode->AsString();
        String packageName = "";
        size_t pos = controlName.find('/');
        if (pos != String::npos)
        {
            packageName = controlName.substr(0, pos);
            controlName = controlName.substr(pos + 1, controlName.length() - pos - 1);
        }
        control = builder->BeginControlWithPrototype(packageName, controlName, customClass, this);
    }
    else if (classNode)
    {
        const YamlNode *customClassNode = node->Get("customClass");
        if (customClassNode)
            control = builder->BeginControlWithCustomClass(customClassNode->AsString(), classNode->AsString());
        else
            control = builder->BeginControlWithClass(classNode->AsString());
    }
    else
    {
        builder->BeginUnknownControl(node);
    }

    if (control)
    {
        if (nameNode)
            control->SetName(nameNode->AsString());
        LoadControlPropertiesFromYamlNode(control, control->GetTypeInfo(), node);
        LoadComponentPropertiesFromYamlNode(control, node);
        LoadBgPropertiesFromYamlNode(control, node);
        LoadInternalControlPropertiesFromYamlNode(control, node);

        // load children
        const YamlNode * childrenNode = node->Get("children");
        if (childrenNode)
        {
            uint32 count = childrenNode->GetCount();
            for (uint32 i = 0; i < count; i++)
                LoadControl(childrenNode->Get(i), false);
        }

        control->LoadFromYamlNodeCompleted();
        if (root)
            control->ApplyAlignSettingsForChildren();
        // yamlLoader->PostLoad(control);

    }
    builder->EndControl(root);
}

void UIPackageLoader::LoadControlPropertiesFromYamlNode(UIControl *control, const InspInfo *typeInfo, const YamlNode *node)
{
    const InspInfo *baseInfo = typeInfo->BaseInfo();
    if (baseInfo)
        LoadControlPropertiesFromYamlNode(control, baseInfo, node);
    
    builder->BeginControlPropertiesSection(typeInfo->Name());
    for (int32 i = 0; i < typeInfo->MembersCount(); i++)
    {
        const InspMember *member = typeInfo->Member(i);

        VariantType res;
        if (node)
            res = ReadVariantTypeFromYamlNode(member, node);
        builder->ProcessProperty(control, member, res);
    }
    builder->EndControlPropertiesSection();
}
    
void UIPackageLoader::LoadComponentPropertiesFromYamlNode(UIControl *control, const YamlNode *node)
{
    Vector<ComponentNode> components = ExtractComponentNodes(node);
    for (auto &nodeDescr : components)
    {
        UIComponent *component = builder->BeginComponentPropertiesSection(nodeDescr.type, nodeDescr.index);
        if (component)
        {
            const InspInfo *insp = component->GetTypeInfo();
            for (int32 j = 0; j < insp->MembersCount(); j++)
            {
                const InspMember *member = insp->Member(j);
                VariantType res = ReadVariantTypeFromYamlNode(member, nodeDescr.node);
                builder->ProcessProperty(control, member, res);
            }
        }
        
        builder->EndComponentPropertiesSection();
    }
}

Vector<UIPackageLoader::ComponentNode> UIPackageLoader::ExtractComponentNodes(const YamlNode *node)
{
    const YamlNode *componentsNode = node ? node->Get("components") : nullptr;

    Vector<ComponentNode> components;

    if (componentsNode)
    {
        const EnumMap *componentTypes = GlobalEnumMap<UIComponent::eType>::Instance();
        
        for (uint32 i = 0; i < componentsNode->GetCount(); i++)
        {
            const String &fullName = componentsNode->GetItemKeyName(i);
            String::size_type lastChar = fullName.find_last_not_of("0123456789");
            String componentName = fullName.substr(0, lastChar + 1);
            uint32 componentIndex = atoi(fullName.substr(lastChar + 1).c_str());
            
            int32 componentType = 0;
            if (componentTypes->ToValue(componentName.c_str(), componentType))
            {
                if (componentType < UIComponent::COMPONENT_COUNT)
                {
                    ComponentNode n;
                    n.node = componentsNode->Get(i);
                    n.type = componentType;
                    n.index = componentIndex;
                    components.push_back(n);
                }
                else
                {
                    DVASSERT(false);
                }
            }
        }
        
        std::stable_sort(components.begin(), components.end(), [](ComponentNode l, ComponentNode r) {
            return l.type == r.type ? l.index < r.index : l.type < r.type;
        });
    }
    return components;
}

void UIPackageLoader::LoadBgPropertiesFromYamlNode(UIControl *control, const YamlNode *node)
{
    const YamlNode *componentsNode = node ? node->Get("components") : nullptr;
    
    for (int32 i = 0; i < control->GetBackgroundComponentsCount(); i++)
    {
        const YamlNode *componentNode = nullptr;
        
        if (componentsNode)
            componentNode = componentsNode->Get(control->GetBackgroundComponentName(i));
        
        UIControlBackground *bg = builder->BeginBgPropertiesSection(i, componentNode != nullptr);
        if (bg)
        {
            const InspInfo *insp = bg->GetTypeInfo();
            for (int32 j = 0; j < insp->MembersCount(); j++)
            {
                const InspMember *member = insp->Member(j);
                VariantType res;
                if (componentNode)
                    res = ReadVariantTypeFromYamlNode(member, componentNode);
                builder->ProcessProperty(control, member, res);
            }
        }
        builder->EndBgPropertiesSection();
    }
}

void UIPackageLoader::LoadInternalControlPropertiesFromYamlNode(UIControl *control, const YamlNode *node)
{
    const YamlNode *componentsNode = node ? node->Get("components") : nullptr;
    for (int32 i = 0; i < control->GetInternalControlsCount(); i++)
    {
        const YamlNode *componentNode = nullptr;
        if (componentsNode)
            componentNode = componentsNode->Get(control->GetInternalControlName(i) + control->GetInternalControlDescriptions());
        
        UIControl *internalControl = builder->BeginInternalControlSection(i, componentNode != nullptr);
        if (internalControl)
        {
            const InspInfo *insp = internalControl->GetTypeInfo();

            for (int32 j = 0; j < insp->MembersCount(); j++)
            {
                const InspMember *member = insp->Member(j);

                VariantType value;
                if (componentNode)
                    value = ReadVariantTypeFromYamlNode(member, componentNode);
                builder->ProcessProperty(control, member, value);
            }
        }
        builder->EndInternalControlSection();
    }
}

VariantType UIPackageLoader::ReadVariantTypeFromYamlNode(const InspMember *member, const YamlNode *node)
{
    const YamlNode *valueNode = node->Get(member->Name());
    if (valueNode)
    {
        return valueNode->AsVariantType(member);
    }
    return VariantType();
}


}
