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
class AnimationSet
{
public:
	AnimationSet() { throw std::runtime_error("unreachable code!"); };
	AnimationSet(float startTime, float endTime, std::string& name);
	AnimationSet(float startTime, float endTime, std::string&& name);
	~AnimationSet();

public:
	std::string name;
	std::vector<AnimationLayer> animationLayers;

	float startTime = 0.0f;
	float endTime = 0.0f;
	float length = 0.0f;

public:
	void Interpolate(float timePosition, std::vector<DirectX::XMFLOAT4X4>& boneTransforms) const;

	float GetClipStartTime()const;
	float GetClipEndTime()const;
};

class AnimationSets
{
public:
	AnimationSets();
	~AnimationSets();
public:
	std::unordered_map<std::string, AnimationSet> animationSets;
	std::vector<int> boneHierarchy;
	std::vector<DirectX::XMFLOAT4X4> boneOffsets;
public:
	UINT BoneCount()const;

	float GetClipStartTime(const std::string& clipName)const;
	float GetClipEndTime(const std::string& clipName)const;

	// In a real project, you'd want to cache the result if there was a chance
	// that you were calling this several times with the same clipName at 
	// the same timePos.
	void GetFinalTransforms(const std::string& clipName, float timePos,
		std::vector<DirectX::XMFLOAT4X4>& finalTransforms)const;
};


enum class AnimationType : int {
	Once = 0,
	Loop,
};

struct AnimationTrack
{
public:
	AnimationTrack() { throw std::runtime_error("unreachable code!"); };
	AnimationTrack(float startTime, float endTime) 
		: startPosition(startTime), endPosition(endTime) {}
	~AnimationTrack() {}
public:
	float speed = 1.0f;
	float startPosition = 0.0f;
	float endPosition = 0.0f;

	AnimationType animationType = AnimationType::Loop;
public:
	void SetSpeed(float speed) { this->speed = speed; }
	void SetPeriod(float startPosition, float endPosition) {
		this->startPosition = startPosition;
		this->endPosition = endPosition;
	}
	void SetAnimationType(AnimationType type) { animationType = type; }
};

class AnimationController
{
public:
	AnimationController(AnimationSets* animationSets);
	~AnimationController();
public:
	AnimationSets* animationSets = nullptr;

	std::vector<DirectX::XMFLOAT4X4> finalTransforms;

	std::unordered_map<std::string, AnimationTrack> animationTracks;

	float timePosition = 0.0f;
	std::string curAnimationName;
public:
	void SetAnimation(std::string& name);

	void SetTrackSpeed(std::string& name, float speed);
	void SetTrackPeriod(std::string& name, float startTime, float endTime);
	void SetTrackAnimationType(std::string& name, AnimationType type);

	// Called every frame and increments the time position, interpolates the 
	// animations for each bone based on the current animation clip, and 
	// generates the final transforms which are ultimately set to the effect
	// for processing in the vertex shader.
	void AdvanceTime(float elapsedTime);
private:
	void UpdateTimePosition(float elapsedTime, const AnimationTrack& track);
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
