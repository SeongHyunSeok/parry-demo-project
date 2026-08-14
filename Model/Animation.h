#ifndef SKINNEDDATA_H
#define SKINNEDDATA_H

#include "../Core/d3dUtil.h"
#include "../Core/MathHelper.h"
#include <concepts>

///<summary>
/// A Keyframe defines the bone transformation at an instant in time.
///</summary>
class AnimationCurve
{
public:
	AnimationCurve();
	~AnimationCurve();

public:
	std::vector<float> keyTimes;
	std::vector<float> keyValues;
public:
	void SetKeyCount(int nKeys);
	float GetValueByLinearInterpolation(float timePosition) const;
};


///<summary>
/// A BoneAnimation is defined by a list of keyframes.  For time
/// values inbetween two keyframes, we interpolate between the
/// two nearest keyframes that bound the time.  
///
/// We assume an animation always has two keyframes.
///</summary>
enum class AnimationCurveType : int
{
	TX = 0, TY, TZ,
	RX, RY, RZ,
	SX, SY, SZ,
	Count
};

enum class AnimationBlendMode : int {
	Additive = 0,
	Override,
	OverridePassthrough
};

typedef std::array<AnimationCurve, (size_t)(AnimationCurveType::Count)> AnimationCurvesPerCurveType;

class AnimationLayer
{
public:
	AnimationLayer();
	~AnimationLayer();

public:
	float weight = 1.0f;
	AnimationBlendMode blendMode = AnimationBlendMode::Additive;
	std::vector<AnimationCurvesPerCurveType> animationCurves;
public:
	float GetClipStartTime()const;
	float GetClipEndTime()const;

	void GetInterpolatedSRT(int boneFrameIndex, float timePosition, XMFLOAT3& scales, XMFLOAT3& rotations, XMFLOAT3& translations) const;
};


///<summary>
/// Examples of AnimationClips are "Walk", "Run", "Attack", "Defend".
/// An AnimationClip requires a BoneAnimation for every bone to form
/// the animation clip.    
///</summary>

struct BoneTransformInfo
{
	XMFLOAT3 scale = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4 rotationQuaternion = { 0.0f, 0.0f, 0.0f, 0.0f };
	XMFLOAT3 translation = { 0.0f, 0.0f, 0.0f };
};

class AnimationSet
{
public:
	AnimationSet() { throw std::runtime_error("unreachable code!"); };
	AnimationSet(float startTime, float endTime, std::string& name);
	AnimationSet(float startTime, float endTime, std::string&& name);
	~AnimationSet();
	AnimationSet(AnimationSet&& other) {
		this->name = move(other.name);
		this->animationLayers = move(other.animationLayers);
		startTime = other.startTime;
		endTime = other.endTime;
	}

	AnimationSet& operator=(AnimationSet&& other) {
		this->name = move(other.name);
		this->animationLayers = move(other.animationLayers);
		startTime = other.startTime;
		endTime = other.endTime;
		return *this;
	}

public:
	std::string name;
	std::vector<AnimationLayer> animationLayers;

	float startTime = 0.0f;
	float endTime = 0.0f;

public:
	void Interpolate(
		float timePosition, 
		std::vector<BoneTransformInfo>& boneToParents,
		DirectX::XMFLOAT3& rawRootBoneTranslation,
		float totalWeight) const;

	float GetClipStartTime()const;
	float GetClipEndTime()const;
};

enum class EquipmentType : UINT {
	OneHandSword = 0,
	Shield,
	TwoHandSword,
	Count
};

struct EquipmentLocation {
	EquipmentLocation() {}
	EquipmentLocation(UINT refBoneInedex, DirectX::FXMMATRIX& trans)
		: referenceBoneIndex{ refBoneInedex }
	{
		XMStoreFloat4x4(&localTransform, trans);
	}

	UINT referenceBoneIndex = -1;
	DirectX::XMFLOAT4X4 localTransform = MathHelper::Identity4x4();
};

enum class AnimationType : UINT {
	Idle = 0,
	// Walk
	WalkForward,
	WalkForwardLeft45,
	WalkForwardRight45,
	WalkLeft,
	WalkRight,
	WalkBackward,
	WalkBackwardLeft45,
	WalkBackwardRight45,
	// Roll
	RollForward,
	RollForwardLeft45,
	RollForwardRight45,
	RollLeft,
	RollRight,
	RollBackward,
	RollBackwardLeft45,
	RollBackwardRight45,
	// Hit
	HitBackType1,
	HitBackType2,
	HitBackType3,
	HitFrontType1,
	HitFrontType2,
	HitFrontType3,
	HitFrontDown,
	HitFrontUp,
	// Parried
	Parried,
	// OneHandBaseAttack
	OneHandBaseAttackTypeC_1,
	OneHandBaseAttackTypeC_2,
	OneHandBaseAttackTypeC_3,
	OneHandBaseAttackTypeC_4,
	OneHandBaseAttackTypeC_5,
	OneHandBaseAttackTypeC_6,
	OneHandBaseAttackTypeC_7,
	// OneHandBaseFatal
	OneHandBaseFatalAttack,
	OneHandBaseFatalHit,
	// OneHandBaseShield
	OneHandBaseShieldStart,
	OneHandBaseShieldIdle,
	OneHandBaseShieldEnd,
	OneHandBaseShieldHitType1,
	OneHandBaseShieldHitType2,
	OneHandBaseShieldParryType1,
	OneHandBaseShieldParryType2,
	// OneHandBaseShieldWalk
	OneHandBaseShieldWalkForward,
	OneHandBaseShieldWalkForwardLeft45,
	OneHandBaseShieldWalkForwardRight45,
	OneHandBaseShieldWalkLeft,
	OneHandBaseShieldWalkRight,
	OneHandBaseShieldWalkBackward,
	OneHandBaseShieldWalkBackwardLeft45,
	OneHandBaseShieldWalkBackwardRight45,
	// TwoHandIdle
	TwoHandIdle,
	// TwoHandWalk
	TwoHandWalkForward,
	TwoHandWalkForwardLeft45,
	TwoHandWalkForwardRight45,
	TwoHandWalkLeft,
	TwoHandWalkRight,
	TwoHandWalkBackward,
	TwoHandWalkBackwardLeft45,
	TwoHandWalkBackwardRight45,
	// TwoHandRoll
	TwoHandRollForward,
	TwoHandRollForwardLeft45,
	TwoHandRollForwardRight45,
	TwoHandRollLeft,
	TwoHandRollRight,
	TwoHandRollBackward,
	TwoHandRollBackwardLeft45,
	TwoHandRollBackwardRight45,
	// TwoHandBaseAttack
	TwoHandBaseAttackTypeB_1,
	TwoHandBaseAttackTypeB_2,
	TwoHandBaseAttackTypeB_3,
	// TwoHandBlockHit
	TwoHandBlockHit,
	// TwoHandBlock
	TwoHandBlockIdle,
	TwoHandBlockWalkForward,
	TwoHandBlockWalkForwardLeft45,
	TwoHandBlockWalkForwardRight45,
	TwoHandBlockWalkLeft,
	TwoHandBlockWalkRight,
	TwoHandBlockWalkBackward,
	TwoHandBlockWalkBackwardLeft45,
	TwoHandBlockWalkBackwardRight45,
	// TwoHandBaseFatal
	TwoHandBaseFatalAttack,
	TwoHandBaseFatalHit,
	// EnemyAttack
	EnemyAttackType_1,
	EnemyAttackType_2,
	EnemyAttackType_3,
	EnemyAttackType_1_Offbeat,
	EnemyAttackType_2_Offbeat,
	EnemyAttackType_3_Offbeat,


};

struct AnimationEffectiveTimeInfo
{
	AnimationEffectiveTimeInfo() {}
	AnimationEffectiveTimeInfo(float effectiveStartPos, float effectiveEndPos) 
		: effectiveStartTimePos(effectiveStartPos), effectiveEndTimePos(effectiveEndPos) {}

	float effectiveStartTimePos = 0.0f;
	float effectiveEndTimePos = 0.0f;
};

struct AnimationPlayInfo {
	AnimationType animationType = AnimationType::Idle;

	float timePosition = 0.0f;

	bool startingAnimation = true;

	XMFLOAT3 prevRootBoneRawTranslation = { 0.0f, 0.0f,0.0f };
	XMFLOAT3 curRootBoneRawTranslation = { 0.0f, 0.0f,0.0f };
};

class AnimationSets
{
public:
	AnimationSets();
	~AnimationSets();
public:
	std::unordered_map<AnimationType, AnimationSet*> animationSets;
	std::vector<int> boneHierarchy;
	std::vector<DirectX::XMFLOAT4X4> boneOffsets;
	const std::unordered_map<AnimationType, AnimationEffectiveTimeInfo>* animationEffectiveTimeInfos;
	std::unordered_map<EquipmentType, EquipmentLocation> equipmentLocations;
	std::unordered_map<std::string, AnimationSet> rawAnimationSets;
public:
	void generateAnimationSet(const std::unordered_map<AnimationType, std::string>& mvTypeToName);
	UINT BoneCount()const;

	float GetClipStartTime(const AnimationType clipType)const;
	float GetClipEndTime(const AnimationType clipType)const;

	// In a real project, you'd want to cache the result if there was a chance
	// that you were calling this several times with the same clipName at 
	// the same timePos.
	void GetFinalTransforms(
		AnimationPlayInfo& playInfo,
		std::vector<DirectX::XMFLOAT4X4>& toRootTransforms,
		std::vector<DirectX::XMFLOAT4X4>& finalTransforms,
		DirectX::XMFLOAT3& rootBoneTranslation,
		bool blending,
		float blendingWeight,
		AnimationPlayInfo* blendingTargetPlayInfo = nullptr)const;

	void SetAnimationEffectiveTimeInfos(const std::unordered_map<AnimationType, AnimationEffectiveTimeInfo>* infos);
};

enum class AnimationPlayType : int {
	Once = 0,
	Loop,
};

struct AnimationTrack
{
public:
	AnimationTrack() { throw std::runtime_error("unreachable code!"); };
	AnimationTrack(float startTime, float endTime, AnimationPlayType playType)
		: startPosition(startTime), endPosition(endTime), animationPlayType(playType) {}
	~AnimationTrack() {}
public:
	float speed = 1.0f;
	float startPosition = 0.0f;
	float endPosition = 0.0f;

	AnimationPlayType animationPlayType = AnimationPlayType::Once;
public:
	void SetSpeed(float speed) { this->speed = speed; }
	void SetPeriod(float startPosition, float endPosition) {
		this->startPosition = startPosition;
		this->endPosition = endPosition;
	}
	void SetAnimationPlayType(AnimationPlayType type) { animationPlayType = type; }
};

const float gAnimationBlendingTime = 0.2f;
const UINT gTotalAnimationPlayInfoCount = 2;

class AnimationController
{
public:
	AnimationController(AnimationSets* animationSets);
	~AnimationController();
public:
	AnimationSets* animationSets = nullptr;

	std::vector<XMFLOAT4X4> toRootTransforms;
	std::vector<DirectX::XMFLOAT4X4> finalTransforms;

	std::unordered_map<AnimationType, AnimationTrack> animationTracks;

	XMFLOAT3 rootBoneTranslationResult = { 0.0f, 0.0f,0.0f };

	bool blending = false;
	float blendingWeight = 0.0f;
	AnimationPlayInfo animationPlayInfos[2];
	UINT curAnimationPlayIndex = 0;
public:
	void SetAnimation(AnimationType type);
	void SetAnimation(AnimationType type, float speed);

	void SetTrackSpeed(AnimationType type, float speed);
	void SetTrackPeriod(AnimationType type, float startTime, float endTime);
	void SetTrackAnimationPlayType(AnimationType type, AnimationPlayType playType);

	// Called every frame and increments the time position, interpolates the 
	// animations for each bone based on the current animation clip, and 
	// generates the final transforms which are ultimately set to the effect
	// for processing in the vertex shader.
	void AdvanceTime(float elapsedTime);

	DirectX::XMFLOAT3 GetRootBoneTranslationDiff();

	bool IsAnimationEnd();
	bool IsAnimationStarting();
	bool IsTimposInEffectiveFrame(float offset);
private:
	void UpdateTimePosition(float elapsedTime);
	void UpdateTimePosition(float elapsedTime, UINT playInfoIndex);
	DirectX::XMFLOAT3 CalcRawTranslationDiff(UINT playInfoIndex);
};

struct CALLBACKKEY
{
	float time = 0.0f;
	void* callbackData = NULL;
};

class AnimationCallbackHandler
{
public:
	AnimationCallbackHandler() {}
	~AnimationCallbackHandler() {}

public:
	virtual void HandleCallback(void* pCallbackData, float fTrackPosition) {}
};

#endif // SKINNEDDATA_H
