#pragma once

#include <cstddef>

namespace mmd931::model {

namespace state {
constexpr std::size_t kMaterialCount = 0x0038;
constexpr std::size_t kAdditionalUvCount = 0x21f1;
constexpr std::size_t kVertexMorphTargetCount = 0x2234;
constexpr std::size_t kUvMorphTargetCount = 0x2238;
constexpr std::size_t kAdditionalUv1MorphTargetCount = 0x223c;
constexpr std::size_t kAdditionalUv2MorphTargetCount = 0x2240;
constexpr std::size_t kAdditionalUv3MorphTargetCount = 0x2244;
constexpr std::size_t kAdditionalUv4MorphTargetCount = 0x2248;
constexpr std::size_t kVertexMorphTargets = 0x2260;
constexpr std::size_t kBoneMorphAppendRecords = 0x2268;
constexpr std::size_t kUvMorphTargets = 0x2278;
constexpr std::size_t kAdditionalUv1MorphTargets = 0x2280;
constexpr std::size_t kAdditionalUv2MorphTargets = 0x2288;
constexpr std::size_t kAdditionalUv3MorphTargets = 0x2290;
constexpr std::size_t kAdditionalUv4MorphTargets = 0x2298;
constexpr std::size_t kMaterialMorphBase = 0x22a0;
constexpr std::size_t kMaterialMorphAdditive = 0x22a8;
constexpr std::size_t kMaterialMorphMultiplicative = 0x22b0;
constexpr std::size_t kBones = 0x2748;
constexpr std::size_t kMorphs = 0x2758;
constexpr std::size_t kLegacyMorphVertices = 0x2760;
constexpr std::size_t kRuntimeVertices = 0x2768;
constexpr std::size_t kMorphCount = 0x310c;
constexpr std::size_t kBoneCount = 0x3110;
constexpr std::size_t kMorphDeformationEnabled = 0x3119;
constexpr std::size_t kRigidBodies = 0x3568;
constexpr std::size_t kJoints = 0x3570;
constexpr std::size_t kRigidBodyCount = 0x3578;
constexpr std::size_t kJointCount = 0x357c;
constexpr std::size_t kPhysicsMode = 0x3ca6;
}  // namespace state

namespace bone {
constexpr std::size_t kStride = 0x270;
constexpr std::size_t kNamePointer = 0x28;
constexpr std::size_t kEnglishNamePointer = 0x30;
constexpr std::size_t kParentIndex = 0x38;
constexpr std::size_t kWorldMatrix = 0x3c;
constexpr std::size_t kPhysicsMatrix = 0x7c;
constexpr std::size_t kSavedWorldMatrix = 0xfc;
constexpr std::size_t kBindTranslation = 0x13c;
constexpr std::size_t kAnimationTranslation = 0x148;
constexpr std::size_t kAnimationRotation = 0x154;
constexpr std::size_t kEvaluatedTranslation = 0x174;
constexpr std::size_t kEvaluatedRotation = 0x180;
constexpr std::size_t kPhysicsTranslation = 0x190;
constexpr std::size_t kPhysicsRotation = 0x19c;
constexpr std::size_t kBoneType = 0x1ec;
constexpr std::size_t kInheritParentIndex = 0x1f0;
constexpr std::size_t kPhysicsEnabled = 0x1f4;
constexpr std::size_t kPhysicsStageGate = 0x1f5;
constexpr std::size_t kTransformLayer = 0x1f8;
constexpr std::size_t kTransformFlags = 0x1fc;
constexpr std::size_t kTransformInfluence = 0x200;
constexpr std::size_t kLocalAxis = 0x204;
constexpr std::size_t kExternalParentKey = 0x210;
constexpr std::size_t kIkTargetIndex = 0x22c;
constexpr std::size_t kIkLoopCount = 0x230;
constexpr std::size_t kIkAngleLimit = 0x234;
constexpr std::size_t kIkLinkCount = 0x238;
constexpr std::size_t kIkLinks = 0x240;
}  // namespace bone

namespace rigid_body {
constexpr std::size_t kStride = 0xc0;
constexpr std::size_t kName = 0x00;
constexpr std::size_t kWideName = 0x18;
constexpr std::size_t kWideEnglishName = 0x20;
constexpr std::size_t kBoneIndex = 0x28;
constexpr std::size_t kCollisionGroup = 0x2c;
constexpr std::size_t kNoCollisionGroup = 0x2e;
constexpr std::size_t kShapeType = 0x30;
constexpr std::size_t kShapeSize = 0x34;
constexpr std::size_t kLocalTranslation = 0x40;
constexpr std::size_t kLocalRotation = 0x4c;
constexpr std::size_t kMass = 0x58;
constexpr std::size_t kPhysicsMode = 0x5c;
constexpr std::size_t kRuntimeBodyId = 0x60;
constexpr std::size_t kLinearDamping = 0x64;
constexpr std::size_t kAngularDamping = 0x68;
constexpr std::size_t kRestitution = 0x6c;
constexpr std::size_t kFriction = 0x70;
constexpr std::size_t kRuntimeObject = 0x78;
constexpr std::size_t kInitialWorldMatrix = 0x80;
}  // namespace rigid_body

namespace joint {
constexpr std::size_t kStride = 0x98;
constexpr std::size_t kName = 0x00;
constexpr std::size_t kWideName = 0x18;
constexpr std::size_t kWideEnglishName = 0x20;
constexpr std::size_t kRigidBodyA = 0x28;
constexpr std::size_t kRigidBodyB = 0x2c;
constexpr std::size_t kPosition = 0x30;
constexpr std::size_t kRotation = 0x3c;
constexpr std::size_t kTranslationLower = 0x48;
constexpr std::size_t kTranslationUpper = 0x54;
constexpr std::size_t kRotationLower = 0x60;
constexpr std::size_t kRotationUpper = 0x6c;
constexpr std::size_t kSpringTranslation = 0x78;
constexpr std::size_t kSpringRotation = 0x84;
constexpr std::size_t kRuntimeConstraintId = 0x90;
constexpr std::size_t kMaxSeparation = 0x94;
}  // namespace joint

}  // namespace mmd931::model
