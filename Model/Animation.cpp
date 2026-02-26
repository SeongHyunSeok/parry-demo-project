#include "Animation.h"
#include "../Core/FileUtility.h"

using namespace DirectX;


AnimationCurve::AnimationCurve()
{
}

AnimationCurve::~AnimationCurve()
{
}

void AnimationCurve::SetKeyCount(int nKeys)
{
	keyTimes.resize(nKeys);
	keyValues.resize(nKeys);
}

float AnimationCurve::GetValueByLinearInterpolation(float timePosition) const
{
	for (int k = 0; k < (keyTimes.size() - 1); ++k)
	{
		if ((keyTimes[k] <= timePosition) && (timePosition < keyTimes[k + 1]))
		{
			float lerpPercent = (timePosition - keyTimes[k]) / (keyTimes[k + 1] - keyTimes[k]);
			return(keyValues[k] * (1.0f - lerpPercent) + keyValues[k + 1] * lerpPercent);
		}
	}
	return(keyValues[keyValues.size() - 1]);
}

AnimationLayer::AnimationLayer()
{
}

AnimationLayer::~AnimationLayer()
{
}

void AnimationLayer::GetInterpolatedSRT(int boneFrameIndex, float timePosition, XMFLOAT3& scales, XMFLOAT3& rotations, XMFLOAT3& translations) const
{
	for (int i = 0; i < (int)AnimationCurveType::Count; ++i) {
		if (animationCurves[boneFrameIndex][i].keyValues.size() > 0) {
			AnimationCurveType curveType = static_cast<AnimationCurveType>(i);
			float interpolationResult = animationCurves[boneFrameIndex][i].GetValueByLinearInterpolation(timePosition);
			switch (curveType) {
			case AnimationCurveType::TX:
				translations.x = interpolationResult; break;
			case AnimationCurveType::TY:
				translations.y = interpolationResult; break;
			case AnimationCurveType::TZ:
				translations.z = interpolationResult; break;
			case AnimationCurveType::RX:
				rotations.x = interpolationResult; break;
			case AnimationCurveType::RY:
				rotations.y = interpolationResult; break;
			case AnimationCurveType::RZ:
				rotations.z = interpolationResult; break;
			case AnimationCurveType::SX:
				scales.x = interpolationResult; break;
			case AnimationCurveType::SY:
				scales.y = interpolationResult; break;
			case AnimationCurveType::SZ:
				scales.z = interpolationResult; break;
			default:
				break;
			}
		}
	}
}

AnimationSet::AnimationSet(float startTime, float endTime, std::string& name)
	: startTime(startTime), endTime(endTime), name(name)
{
	length = endTime - startTime;
}

AnimationSet::AnimationSet(float startTime, float endTime, std::string&& name)
	: startTime(startTime), endTime(endTime), name(move(name))
{
	length = endTime - startTime;
}

AnimationSet::~AnimationSet()
{
}

void AnimationSet::Interpolate(
	float timePosition, 
	std::vector<BoneTransformInfo>& boneToParents,
	DirectX::XMFLOAT3& rawRootBoneTranslation,
	float totalWeight) const
{
	for (int boneFrameIndex = 0; boneFrameIndex < boneToParents.size(); ++boneFrameIndex) {
		XMFLOAT3 scaleResult = { 0.f, 0.f, 0.f };
		XMFLOAT3 rotationResult = { 0.f, 0.f, 0.f };
		XMFLOAT3 translationResult = { 0.f, 0.f, 0.f };

		for (const AnimationLayer& layer : animationLayers) {
			XMFLOAT3 scale = { 0.f, 0.f, 0.f };
			XMFLOAT3 rotation = { 0.f, 0.f, 0.f };
			XMFLOAT3 translation = { 0.f, 0.f, 0.f };
			layer.GetInterpolatedSRT(boneFrameIndex, timePosition, scale, rotation, translation);

			switch (layer.blendMode) {
			case AnimationBlendMode::Additive:
				scaleResult = Vector3::Add(scaleResult, scale);
				rotationResult = Vector3::Add(rotationResult, rotation);
				translationResult = Vector3::Add(translationResult, translation);
				break;
			case AnimationBlendMode::Override:
				scaleResult = scale;
				rotationResult = rotation;
				translationResult = translation;
				break;
			case AnimationBlendMode::OverridePassthrough:
				scaleResult = Vector3::Add(scaleResult, scale, layer.weight);
				rotationResult = Vector3::Add(rotationResult, rotation, layer.weight);
				translationResult = Vector3::Add(translationResult, translation, layer.weight);
				break;
			}
		}
		if (boneFrameIndex == 0) {
			rawRootBoneTranslation = translationResult;
		}
		boneToParents[boneFrameIndex].scale = Vector3::Add(boneToParents[boneFrameIndex].scale, scaleResult, totalWeight);
		boneToParents[boneFrameIndex].rotation = Vector3::Add(boneToParents[boneFrameIndex].rotation, rotationResult, totalWeight);
		boneToParents[boneFrameIndex].translation = Vector3::Add(boneToParents[boneFrameIndex].translation, translationResult, totalWeight);

		//XMMATRIX S = XMMatrixScaling(scaleResult.x, scaleResult.y, scaleResult.z);
		//XMMATRIX R = XMMatrixMultiply(XMMatrixMultiply(XMMatrixRotationX(rotationResult.x), XMMatrixRotationY(rotationResult.y)), XMMatrixRotationZ(rotationResult.z));
		//XMMATRIX T = XMMatrixTranslation(translationResult.x, translationResult.y, translationResult.z);
		//XMStoreFloat4x4(&boneToParents[boneFrameIndex], XMMatrixMultiply(XMMatrixMultiply(S, R), T));
	}
}

float AnimationSet::GetClipStartTime()const
{
	return startTime;
}

float AnimationSet::GetClipEndTime()const
{
	return endTime;
}


AnimationSets::AnimationSets()
{
}

AnimationSets::~AnimationSets()
{
}

void AnimationSets::generateAnimationSet(const std::unordered_map<AnimationType, std::string>& aniTypeToName)
{
	for (const auto& it : aniTypeToName) {
		animationSets[it.first] = &rawAnimationSets[it.second];
	}
}

UINT AnimationSets::BoneCount()const
{
	return boneHierarchy.size();
}

float AnimationSets::GetClipStartTime(const AnimationType clipType)const
{
	auto clip = animationSets.find(clipType);
	return clip->second->GetClipStartTime();
}

float AnimationSets::GetClipEndTime(const AnimationType clipType)const
{
	auto clip = animationSets.find(clipType);
	return clip->second->GetClipEndTime();
}

void AnimationSets::GetFinalTransforms(AnimationPlayInfo& playInfo,
	std::vector<DirectX::XMFLOAT4X4>& toRootTransforms,
	std::vector<DirectX::XMFLOAT4X4>& finalTransforms,
	DirectX::XMFLOAT3& rootBoneTranslation,
	bool blending,
	float blendingWeight,
	AnimationPlayInfo* blendingTargetPlayInfo) const
{
	UINT numBones = boneHierarchy.size();

	std::vector<BoneTransformInfo> toParentBoneTransformInfo(numBones);
	auto clip = animationSets.find(playInfo.animationType);
	clip->second->Interpolate(playInfo.timePosition,
		toParentBoneTransformInfo, 
		playInfo.curRootBoneRawTranslation,
		1.0f - blendingWeight);

	if (blending && blendingTargetPlayInfo) {
		// Interpolate all the bones of this clip at the given time instance.
		auto blendingTargetClip = animationSets.find(blendingTargetPlayInfo->animationType);
		blendingTargetClip->second->Interpolate(
			blendingTargetPlayInfo->timePosition,
			toParentBoneTransformInfo, 
			blendingTargetPlayInfo->curRootBoneRawTranslation,
			blendingWeight);
	}

	std::vector<XMFLOAT4X4> toParentTransforms(numBones);
	for (int boneFrameIndex = 0; boneFrameIndex < numBones; ++boneFrameIndex) {
		XMMATRIX S = XMMatrixScaling(toParentBoneTransformInfo[boneFrameIndex].scale.x, toParentBoneTransformInfo[boneFrameIndex].scale.y, toParentBoneTransformInfo[boneFrameIndex].scale.z);
		XMMATRIX R = XMMatrixMultiply(XMMatrixMultiply(XMMatrixRotationX(toParentBoneTransformInfo[boneFrameIndex].rotation.x), XMMatrixRotationY(toParentBoneTransformInfo[boneFrameIndex].rotation.y)), XMMatrixRotationZ(toParentBoneTransformInfo[boneFrameIndex].rotation.z));
		XMMATRIX T = XMMatrixTranslation(toParentBoneTransformInfo[boneFrameIndex].translation.x, toParentBoneTransformInfo[boneFrameIndex].translation.y, toParentBoneTransformInfo[boneFrameIndex].translation.z);
		XMStoreFloat4x4(&toParentTransforms[boneFrameIndex], XMMatrixMultiply(XMMatrixMultiply(S, R), T));
	}

	rootBoneTranslation = {
		toParentTransforms[0]._41,
		toParentTransforms[0]._42,
		toParentTransforms[0]._43
	};

	toParentTransforms[0]._41 = 0;
	toParentTransforms[0]._42 = 0;
	toParentTransforms[0]._43 = 0;

	//
	// Traverse the hierarchy and transform all the bones to the root space.
	//

	// The root bone has index 0.  The root bone has no parent, so its toRootTransform
	// is just its local bone transform.

	toRootTransforms[0] = toParentTransforms[0];

	// Now find the toRootTransform of the children.
	for (UINT i = 1; i < numBones; ++i)
	{
		XMMATRIX toParent = XMLoadFloat4x4(&toParentTransforms[i]);

		int parentIndex = boneHierarchy[i];
		XMMATRIX parentToRoot = XMLoadFloat4x4(&toRootTransforms[parentIndex]);

		XMMATRIX toRoot = XMMatrixMultiply(toParent, parentToRoot);

		XMStoreFloat4x4(&toRootTransforms[i], toRoot);
	}

	// Premultiply by the bone offset transform to get the final transform.
	for (UINT i = 0; i < numBones; ++i)
	{
		XMMATRIX offset = XMLoadFloat4x4(&boneOffsets[i]);
		XMMATRIX toRoot = XMLoadFloat4x4(&toRootTransforms[i]);
		XMMATRIX finalTransform = XMMatrixMultiply(offset, toRoot);
		XMStoreFloat4x4(&finalTransforms[i], XMMatrixTranspose(finalTransform));
	}
}

AnimationController::AnimationController(AnimationSets* animationSets)
	:animationSets(animationSets)
{
	if (!animationSets) return;

	finalTransforms.resize(animationSets->BoneCount());
	toRootTransforms.resize(animationSets->BoneCount());

	for (const auto& it : animationSets->animationSets) {
		animationTracks.emplace(
			std::piecewise_construct,
			std::forward_as_tuple(it.first),
			std::forward_as_tuple(it.second->GetClipStartTime(), it.second->GetClipEndTime()));
	}
}

AnimationController::~AnimationController()
{
}

void AnimationController::SetAnimation(AnimationType type)
{
	curAnimationPlayIndex = (curAnimationPlayIndex + 1) % gTotalAnimationPlayInfoCount;
	blending = true;
	AnimationPlayInfo& playInfo = animationPlayInfos[curAnimationPlayIndex];
	playInfo.animationType = type;
	playInfo.timePosition = 0.0f;
	playInfo.startingAnimation = true;
	playInfo.prevRootBoneRawTranslation = { 0.0f, 0.0f,0.0f };
	playInfo.curRootBoneRawTranslation = { 0.0f, 0.0f,0.0f };
}

void AnimationController::SetTrackSpeed(AnimationType type, float speed)
{
	animationTracks[type].SetSpeed(speed);
}

void AnimationController::SetTrackPeriod(AnimationType type, float startTime, float endTime)
{
	animationTracks[type].SetPeriod(startTime, endTime);
}

void AnimationController::SetTrackAnimationType(AnimationType type, AnimationPlayType playType)
{
	animationTracks[type].SetAnimationPlayType(playType);
}


void AnimationController::AdvanceTime(float elapsedTime)
{
	UpdateTimePosition(elapsedTime);

	animationPlayInfos[curAnimationPlayIndex].prevRootBoneRawTranslation
		= animationPlayInfos[curAnimationPlayIndex].curRootBoneRawTranslation;

	blendingWeight = 0.0f;
	UINT prevAniPlayInfoIndex = (curAnimationPlayIndex + 1) % gTotalAnimationPlayInfoCount;
	if (blending) {
		animationPlayInfos[prevAniPlayInfoIndex].prevRootBoneRawTranslation
			= animationPlayInfos[prevAniPlayInfoIndex].curRootBoneRawTranslation;

		if (animationPlayInfos[curAnimationPlayIndex].timePosition <= gAnimationBlendingTime) {
			blendingWeight = (1.0f - animationPlayInfos[curAnimationPlayIndex].timePosition / gAnimationBlendingTime);
		}
		else {
			blending = false;
		}
	}

	// Compute the final transforms for this time position.
	animationSets->GetFinalTransforms(
		animationPlayInfos[curAnimationPlayIndex],
		toRootTransforms, finalTransforms, 
		rootBoneTranslationResult,
		blending, blendingWeight, &animationPlayInfos[prevAniPlayInfoIndex]
	);
}

void AnimationController::UpdateTimePosition(float elapsedTime)
{
	if (blending) {
		for (UINT i = 0; i < gTotalAnimationPlayInfoCount; ++i) {
			UpdateTimePosition(elapsedTime, i);
		}
	}
	else {
		UpdateTimePosition(elapsedTime, curAnimationPlayIndex);
	}
}

void AnimationController::UpdateTimePosition(float elapsedTime, UINT playInfoIndex) {
	 AnimationPlayInfo& playInfo = animationPlayInfos[playInfoIndex];
	const AnimationTrack& track = animationTracks[playInfo.animationType];

	playInfo.timePosition += elapsedTime * track.speed;

	switch (track.animationPlayType)
	{
	case AnimationPlayType::Loop:
	{
		if (playInfo.timePosition < track.startPosition) playInfo.timePosition = track.startPosition;
		if (playInfo.timePosition > track.endPosition) playInfo.timePosition = track.startPosition;
		break;
	}
	case AnimationPlayType::Once:
		if (playInfo.timePosition < track.startPosition) playInfo.timePosition = track.startPosition;
		if (playInfo.timePosition > track.endPosition) playInfo.timePosition = track.endPosition;
		break;
	}
	if (playInfo.timePosition <= track.startPosition || playInfo.timePosition >= track.endPosition) {
		playInfo.startingAnimation = true;
	}
}

DirectX::XMFLOAT3 AnimationController::GetRootBoneTranslationDiff() {
	XMFLOAT3 rawTransDiff = CalcRawTranslationDiff(curAnimationPlayIndex);

	if (!blending) {
		return rawTransDiff;
	}

	XMFLOAT3 rawBlendingTransDiff = CalcRawTranslationDiff((curAnimationPlayIndex + 1) % gTotalAnimationPlayInfoCount);

	return Vector3::Add(
		Vector3::ScalarProduct(rawTransDiff, 1 - blendingWeight, false),
		Vector3::ScalarProduct(rawBlendingTransDiff, blendingWeight, false)
	);
}

DirectX::XMFLOAT3 AnimationController::CalcRawTranslationDiff(UINT playInfoIndex) {
	AnimationPlayInfo& playInfo = animationPlayInfos[playInfoIndex];
	if (playInfo.startingAnimation) {
		playInfo.startingAnimation = false;
		return XMFLOAT3{ 0.0f, 0.0f, 0.0f };
	}
	return Vector3::Subtract(playInfo.curRootBoneRawTranslation, playInfo.prevRootBoneRawTranslation);
}

bool AnimationController::ChangeAnimationByUserInput(AnimationType type) {
	if (IsChangeAnimationByUserInputPossible()) {
		if (animationPlayInfos[curAnimationPlayIndex].animationType != type) {
			SetAnimation(type);
		}
		return true;
	}
	return false;
}

bool AnimationController::IsChangeAnimationByUserInputPossible() {
	// TODO
	return true;
}