#include "Asset/AssetManager.h"
#include "Engine/Engine.h"
#include "Render/Highlevel/Landscape.h"
#include "Render/Highlevel/LandscapeSubdivision.h"
#include "Scene3D/Components/LandscapeComponent.h"

namespace DAVA
{
DAVA_VIRTUAL_REFLECTION_IMPL(LandscapeComponent)
{
    ReflectionRegistrator<LandscapeComponent>::Begin()[M::CantBeCreatedManualyComponent()]
    .ConstructorByPointer()
    .Field("landscape", &LandscapeComponent::landscape)[M::DisplayName("Landscape")]
    .End();
}

LandscapeComponent::LandscapeComponent()
{
    landscape = new Landscape();
}

LandscapeComponent::~LandscapeComponent()
{
    SafeRelease(landscape);
}

Component* LandscapeComponent::Clone(Entity* toEntity)
{
    LandscapeComponent* newComponent = new LandscapeComponent();
    newComponent->SetEntity(toEntity);

    newComponent->SetLayersCount(GetLayersCount());
    for (uint32 l = 0; l < GetLayersCount(); ++l)
    {
        for (uint32 i = 0; i < 3; ++i)
            newComponent->SetPageMaterialPath(l, i, GetPageMaterialPath(l, i));
    }

    newComponent->SetHeightmapPath(GetHeighmapPath());
    newComponent->SetLandscapeMaterialPath(GetLandscapeMaterialPath());
    newComponent->SetLandscapeSize(GetLandscapeSize());
    newComponent->SetLandscapeHeight(GetLandscapeHeight());
    newComponent->SetTessellationLevelCount(GetTessellationLevelCount());
    newComponent->SetTessellationHeight(GetTessellationHeight());
    newComponent->landscape->SetFlags(landscape->GetFlags());

    for (uint32 i = 0; i < static_cast<uint32>(LandscapeQuality::Count); ++i)
    {
        newComponent->landscape->settings[i].CopySettings(&landscape->settings[i]);
    }
    newComponent->landscape->subdivision->SetMetrics(landscape->subdivision->GetMetrics());

    return newComponent;
}

void LandscapeComponent::Serialize(KeyedArchive* archive, SerializationContext* serializationContext)
{
    Component::Serialize(archive, serializationContext);
    AssetManager* assetManager = GetEngineContext()->assetManager;

    if (landscapeMaterial != nullptr)
    {
        AssetFileInfo fileInfo = assetManager->GetAssetFileInfo(landscapeMaterial);
        archive->SetString("landscapeMaterialPath", FilePath(fileInfo.fileName).GetRelativePathname(serializationContext->GetScenePath()));
    }

    if (!heightmapPath.IsEmpty())
        archive->SetString("heightmapPath", heightmapPath.GetRelativePathname(serializationContext->GetScenePath()));

    archive->SetFloat("landscapeHeight", landscapeHeight);
    archive->SetFloat("landscapeSize", landscapeSize);

    archive->SetUInt32("tessellationLevelCount", tessellationLevelCount);
    archive->SetFloat("tessellationHeight", tessellationHeight);

    archive->SetUInt32("layersCount", GetLayersCount());

    for (uint32 l = 0; l < GetLayersCount(); ++l)
    {
        for (uint32 i = 0; i < 3; ++i)
        {
            AssetFileInfo fileInfo = assetManager->GetAssetFileInfo(layersPageMaterials[l][i]);
            archive->SetString(Format("layer%u_lod%u_materialPath", l, i), FilePath(fileInfo.fileName).GetRelativePathname(serializationContext->GetScenePath()));
        }
    }

    for (uint32 q = 0; q < uint32(LandscapeQuality::Count); ++q)
    {
        ScopedPtr<KeyedArchive> qualityArchive(new KeyedArchive());
        landscape->settings[q].Save(qualityArchive, serializationContext);
        archive->SetArchive(Format("landscape.settings.%d", q), qualityArchive);
    }

    landscape->SaveFlags(archive, serializationContext);
}

void LandscapeComponent::Deserialize(KeyedArchive* archive, SerializationContext* serializationContext)
{
    AssetManager* assetManager = GetEngineContext()->assetManager;

    Component::Deserialize(archive, serializationContext);

    SetHeightmapPath(serializationContext->GetScenePath() + archive->GetString("heightmapPath"));

    SetLandscapeHeight(archive->GetFloat("landscapeHeight"));
    SetLandscapeSize(archive->GetFloat("landscapeSize"));

    SetTessellationLevelCount(archive->GetUInt32("tessellationLevelCount"));
    SetTessellationHeight(archive->GetFloat("tessellationHeight"));

    SetLandscapeMaterialPath(serializationContext->GetScenePath() + archive->GetString("landscapeMaterialPath"));

    SetLayersCount(archive->GetUInt32("layersCount"));
    for (uint32 l = 0; l < GetLayersCount(); ++l)
    {
        for (uint32 i = 0; i < 3; ++i)
            SetPageMaterialPath(l, i, serializationContext->GetScenePath() + archive->GetString(Format("layer%u_lod%u_materialPath", l, i)));
    }

    landscape->settings[uint32(LandscapeQuality::Full)].Load(archive, serializationContext); //back-compatibility
    for (uint32 q = 0; q < uint32(LandscapeQuality::Count); ++q)
    {
        KeyedArchive* qualityArchive = archive->GetArchive(Format("landscape.settings.%d", q));
        if (qualityArchive != nullptr)
            landscape->settings[q].Load(qualityArchive, serializationContext);
    }

    landscape->LoadFlags(archive, serializationContext);

    landscape->ApplyQualitySettings();
}

Landscape* LandscapeComponent::GetLandscape() const
{
    return landscape;
}

void LandscapeComponent::SetLayersCount(uint32 count)
{
    layersPageMaterials.resize(count);
}

uint32 LandscapeComponent::GetLayersCount() const
{
    return uint32(layersPageMaterials.size());
}

void LandscapeComponent::SetPageMaterialPath(uint32 layer, uint32 lod, const FilePath& path)
{
    layersPageMaterials[layer][lod] = GetEngineContext()->assetManager->GetAsset<Material>(Material::PathKey(path), AssetManager::SYNC);
    DVASSERT(layersPageMaterials[layer][lod] != nullptr);
    if (layersPageMaterials[layer][lod] != nullptr)
        landscape->SetPageMaterial(layer, lod, layersPageMaterials[layer][lod]->GetMaterial());
}

FilePath LandscapeComponent::GetPageMaterialPath(uint32 layer, uint32 lod) const
{
    AssetFileInfo info = GetEngineContext()->assetManager->GetAssetFileInfo(layersPageMaterials[layer][lod]);
    return FilePath(info.fileName);
}

void LandscapeComponent::SetHeightmapPath(const FilePath& path)
{
    heightmapPath = path;
    landscape->SetHeightmapPathname(heightmapPath);
}

const FilePath& LandscapeComponent::GetHeighmapPath() const
{
    return heightmapPath;
}

void LandscapeComponent::SetLandscapeMaterialPath(const FilePath& path)
{
    landscapeMaterial = GetEngineContext()->assetManager->GetAsset<Material>(Material::PathKey(path), AssetManager::SYNC);
    DVASSERT(landscapeMaterial != nullptr);
    if (landscapeMaterial != nullptr)
        landscape->SetLandscapeMaterial(landscapeMaterial->GetMaterial());
}

FilePath LandscapeComponent::GetLandscapeMaterialPath() const
{
    AssetFileInfo fileInfo = GetEngineContext()->assetManager->GetAssetFileInfo(landscapeMaterial);
    return FilePath(fileInfo.fileName);
}

void LandscapeComponent::SetLandscapeSize(float32 size)
{
    landscapeSize = size;
    landscape->SetLandscapeSize(landscapeSize);
}

float32 LandscapeComponent::GetLandscapeSize() const
{
    return landscapeSize;
}

void LandscapeComponent::SetLandscapeHeight(float32 height)
{
    landscapeHeight = height;
    landscape->SetLandscapeHeight(landscapeHeight);
}

float32 LandscapeComponent::GetLandscapeHeight() const
{
    return landscapeHeight;
}

void LandscapeComponent::SetTessellationLevelCount(uint32 levelCount)
{
    tessellationLevelCount = levelCount;
    landscape->SetTessellationLevels(tessellationLevelCount);
}

uint32 LandscapeComponent::GetTessellationLevelCount() const
{
    return tessellationLevelCount;
}

void LandscapeComponent::SetTessellationHeight(float32 height)
{
    tessellationHeight = height;
    landscape->SetTessellationHeight(tessellationHeight);
}

float32 LandscapeComponent::GetTessellationHeight() const
{
    return tessellationHeight;
}

void LandscapeComponent::GetDataNodes(Set<DataNode*>& dataNodes)
{
    landscape->GetDataNodes(dataNodes);
}
}
