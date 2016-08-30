#include <FlexEngine/Factory/TreeHost.h>

#include <FlexEngine/Core/Attribute.h>
#include <FlexEngine/Factory/ModelFactory.h>

#include <Urho3D/Core/Context.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Graphics/StaticModel.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Scene/Node.h>

namespace FlexEngine
{

namespace
{

static const char* branchDistributionNames[] =
{
    "Alternate",
    0
};

void GenerateChildren(Node& node, TreeHost& host)
{
    for (SharedPtr<Node> child : node.GetChildren())
    {
        PODVector<TreeElement*> elements;
        child->GetDerivedComponents<TreeElement>(elements);
        for (TreeElement* element : elements)
        {
            element->Generate(host);
        }
    }
}

void TriangulateChildren(Node& node, ModelFactory& factory, TreeHost& host)
{
    for (SharedPtr<Node> child : node.GetChildren())
    {
        PODVector<TreeElement*> elements;
        child->GetDerivedComponents<TreeElement>(elements);
        for (TreeElement* element : elements)
        {
            element->Triangulate(factory, host);
        }
    }
}

}

//////////////////////////////////////////////////////////////////////////
TreeHost::TreeHost(Context* context)
    : ProceduralComponent(context)
{
}

TreeHost::~TreeHost()
{
}

void TreeHost::RegisterObject(Context* context)
{
    context->RegisterFactory<TreeHost>(FLEXENGINE_CATEGORY);

    URHO3D_COPY_BASE_ATTRIBUTES(ProceduralComponent);
}

void TreeHost::OnBranchGenerated(const BranchDescription& /*branch*/, const BranchShapeSettings& /*shape*/)
{

}

void TreeHost::OnLeafGenerated(const LeafDescription& leaf, const LeafShapeSettings& /*shape*/)
{
    leavesPositions_.Push(leaf.location_.position_);
}

void TreeHost::UpdateViews()
{
    StaticModel* staticModel = node_->GetComponent<StaticModel>();
    if (staticModel)
    {
        staticModel->SetModel(model_);
        for (unsigned i = 0; i < materials_.Size(); ++i)
        {
            staticModel->SetMaterial(i, materials_[i]);
        }
    }
}

void TreeHost::DoUpdate()
{
    // Generate tree
    leavesPositions_.Clear();
    GenerateChildren(*node_, *this);

    // Compute foliage center
    foliageCenter_ = Vector3::ZERO;
    for (const Vector3& position : leavesPositions_)
    {
        foliageCenter_ += position;
    }
    if (!leavesPositions_.Empty())
    {
        foliageCenter_ /= static_cast<float>(leavesPositions_.Size());
    }

    // Triangulate tree
    ModelFactory factory(context_);
    factory.Initialize(DefaultVertex::GetVertexElements(), true);
    TriangulateChildren(*node_, factory, *this);
    materials_ = factory.GetMaterials();
    model_ = factory.BuildModel(materials_);

    // Update model and materials
    UpdateViews();
}

//////////////////////////////////////////////////////////////////////////
TreeElement::TreeElement(Context* context)
    : ProceduralComponentAgent(context)
{
    ResetToDefault();
}

TreeElement::~TreeElement()
{

}

void TreeElement::RegisterObject(Context* context)
{
    URHO3D_COPY_BASE_ATTRIBUTES(ProceduralComponentAgent);

    URHO3D_MEMBER_ATTRIBUTE("Seed", unsigned, distribution_.seed_, 0, AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE("Frequency", unsigned, distribution_.frequency_, 0, AM_DEFAULT);

    URHO3D_MEMBER_ATTRIBUTE_ACCESSOR("Growth Location", Vector2, distribution_.location_, GetVector, SetVector, Vector2(0.0f, 1.0f), AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE_ACCESSOR("Growth Density", String, distribution_.density_, GetCurveString, SetCurveString, "one", AM_DEFAULT);
    URHO3D_MEMBER_ENUM_ATTRIBUTE("Distribution", TreeElementDistributionType, distribution_.distributionType_, branchDistributionNames, 0, AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE("Twirl angle step", float, distribution_.twirlStep_, 180.0f, AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE("Twirl angle random", float, distribution_.twirlNoise_, 0.0f, AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE("Twirl angle base", float, distribution_.twirlBase_, 0.0f, AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE_ACCESSOR("Growth Scale", Vector2, distribution_.growthScale_, GetResultRange, SetResultRange, Vector2::ONE, AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE_ACCESSOR("Growth Scale Curve", String, distribution_.growthScale_, GetCurveString, SetCurveString, "linear", AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE_ACCESSOR("Growth Angle", Vector2, distribution_.growthAngle_, GetResultRange, SetResultRange, Vector2::ZERO, AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE_ACCESSOR("Growth Angle Curve", String, distribution_.growthAngle_, GetCurveString, SetCurveString, "linear", AM_DEFAULT);

}

void TreeElement::ApplyAttributes()
{
    // Mark host as dirty
    TreeHost* root = node_->GetParentComponent<TreeHost>(true);
    if (!root)
    {
        URHO3D_LOGERROR("BranchGroup must have parent TreeHost");
        return;
    }

    root->MarkNeedUpdate();
}

void TreeElement::Triangulate(ModelFactory& factory, TreeHost& host) const
{
    DoTriangulate(factory, host);
    TriangulateChildren(*node_, factory, host);
}

//////////////////////////////////////////////////////////////////////////
BranchGroup::BranchGroup(Context* context)
    : TreeElement(context)
{
    ResetToDefault();
}

BranchGroup::~BranchGroup()
{
}

void BranchGroup::RegisterObject(Context* context)
{
    context->RegisterFactory<BranchGroup>(FLEXENGINE_CATEGORY);

    URHO3D_COPY_BASE_ATTRIBUTES(TreeElement);

    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Material", GetMaterialAttr, SetMaterialAttr, ResourceRef, ResourceRef(Material::GetTypeStatic()), AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE("UV Scale", Vector2, shape_.textureScale_, Vector2::ONE, AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE("Quality", float, shape_.quality_, 1.0f, AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE_ACCESSOR("Length", Vector2, shape_.length_, GetVector, SetVector, Vector2::ONE, AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE("Relative Length", bool, shape_.relativeLength_, true, AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE("Fake Ending", bool, shape_.fakeEnding_, false, AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE_ACCESSOR("Radius", Vector2, shape_.radius_, GetResultRange, SetResultRange, Vector2(0.5f, 0.1f), AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE_ACCESSOR("Radius Curve", String, shape_.radius_, GetCurveString, SetCurveString, "linear", AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE("Gravity Intensity", float, shape_.gravityIntensity_, 0.0f, AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE("Gravity Resistance", float, shape_.gravityResistance_, 0.5f, AM_DEFAULT);
}

void BranchGroup::Generate(TreeHost& host)
{
    // Initialize transform
    distribution_.position_ = node_->GetWorldPosition();
    distribution_.direction_ = node_->GetWorldRotation() * Vector3::UP;

    // Generate this level
    branches_.Clear();
    if (distribution_.frequency_ == 0)
    {
        branches_ = InstantiateBranchGroup(BranchDescription(), distribution_, shape_, minNumKnots_);
    }
    else
    {
        BranchGroup* parentGroup = node_->GetParentComponent<BranchGroup>();
        if (!parentGroup)
        {
            URHO3D_LOGERROR("BranchGroup with frequency > 0 must have parent BranchGroup");
            return;
        }
        for (const BranchDescription& parentBranch : parentGroup->GetBranches())
        {
            branches_ += InstantiateBranchGroup(parentBranch, distribution_, shape_, minNumKnots_);
        }
    }

    // Notify host
    for (const BranchDescription& branch : branches_)
    {
        host.OnBranchGenerated(branch, shape_);
    }

    // Generate children
    GenerateChildren(*node_, host);
}

void BranchGroup::DoTriangulate(ModelFactory& factory, TreeHost& host) const
{
    // #TODO Fixme
    TreeLodDescription lod;
    lod.branchTessellationQuality_.maxNumSegments_ = 100;
    lod.branchTessellationQuality_.minNumSegments_ = 4;
    lod.branchTessellationQuality_.minAngle_ = 10.0f;

    factory.SetMaterial(material_);
    for (const BranchDescription& branch : branches_)
    {
        if (!branch.fake_)
        {
            const TessellatedBranchPoints tessellatedPoints =
                TessellateBranch(branch.positionsCurve_, branch.radiusesCurve_, lod.branchTessellationQuality_);

            GenerateBranchGeometry(factory, tessellatedPoints, shape_, 5u); // #TODO Fixme
        }
    }
}

void BranchGroup::SetMaterialAttr(const ResourceRef& value)
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    material_ = cache->GetResource<Material>(value.name_);
}

ResourceRef BranchGroup::GetMaterialAttr() const
{
    return GetResourceRef(material_, Material::GetTypeStatic());
}

//////////////////////////////////////////////////////////////////////////
LeafGroup::LeafGroup(Context* context)
    : TreeElement(context)
{
    ResetToDefault();
}

LeafGroup::~LeafGroup()
{

}

void LeafGroup::RegisterObject(Context* context)
{
    context->RegisterFactory<LeafGroup>(FLEXENGINE_CATEGORY);

    URHO3D_COPY_BASE_ATTRIBUTES(TreeElement);

    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Material", GetMaterialAttr, SetMaterialAttr, ResourceRef, ResourceRef(Material::GetTypeStatic()), AM_DEFAULT);

    URHO3D_MEMBER_ATTRIBUTE_ACCESSOR("Size", Vector2, shape_.size_, GetVector, SetVector, Vector2::ONE, AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE("Scale", Vector3, shape_.scale_, Vector3::ONE, AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE_ACCESSOR("Adjust to Global", Vector2, shape_.adjustToGlobal_, GetVector, SetVector, Vector2::ZERO, AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE_ACCESSOR("Align Vertical", Vector2, shape_.alignVertical_, GetVector, SetVector, Vector2::ZERO, AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE("Junction Offset", Vector3, shape_.junctionOffset_, Vector3::ZERO, AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE("Gravity Intensity", Vector3, shape_.gravityIntensity_, Vector3::ZERO, AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE("Gravity Resistance", Vector3, shape_.gravityResistance_, Vector3::ONE * 0.5f, AM_DEFAULT);
    URHO3D_MEMBER_ATTRIBUTE("Bump Normals", float, shape_.bumpNormals_, 0.0f, AM_DEFAULT);
}

void LeafGroup::Generate(TreeHost& host)
{
    // Initialize transform
    distribution_.position_ = node_->GetWorldPosition();
    distribution_.direction_ = node_->GetWorldRotation() * Vector3::UP;

    // Generate this level
    leaves_.Clear();
    if (distribution_.frequency_ == 0)
    {
        leaves_ = InstantiateLeafGroup(BranchDescription(), distribution_, shape_);
    }
    else
    {
        BranchGroup* parentGroup = node_->GetParentComponent<BranchGroup>();
        if (!parentGroup)
        {
            URHO3D_LOGERROR("LeafGroup with frequency > 0 must have parent BranchGroup");
            return;
        }
        for (const BranchDescription& parentBranch : parentGroup->GetBranches())
        {
            leaves_ += InstantiateLeafGroup(parentBranch, distribution_, shape_);
        }
    }

    // Notify host
    for (const LeafDescription& leaf : leaves_)
    {
        host.OnLeafGenerated(leaf, shape_);
    }
}

void LeafGroup::DoTriangulate(ModelFactory& factory, TreeHost& host) const
{
    factory.SetMaterial(material_);
    for (const LeafDescription& leaf : leaves_)
    {
        GenerateLeafGeometry(factory, shape_, leaf.location_, host.GetFoliageCenter());
    }
}

void LeafGroup::SetMaterialAttr(const ResourceRef& value)
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    material_ = cache->GetResource<Material>(value.name_);
}

ResourceRef LeafGroup::GetMaterialAttr() const
{
    return GetResourceRef(material_, Material::GetTypeStatic());
}

}