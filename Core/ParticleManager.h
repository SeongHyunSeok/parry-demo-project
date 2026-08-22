#pragma once

#include "ParticleEmitter.h"
#include "FrameResource.h"
#include "Texture.h"
#include "MeshGeometry.h"

#include <list>
#include <array>
#include <random>

enum class ParticlePsoType : UINT
{
	BILLBOARD = 0,
	COUNT
};

class ParticleManager
{
	std::list<ParticleEmitter> particleEmitters;
	
	std::unordered_map<std::string, std::unique_ptr<Texture>> textures;
	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrvStart;
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrvStart;
	Microsoft::WRL::ComPtr <ID3D12Resource> materialBuffer = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialBufferUploader = nullptr;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> shaders;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> particleRootSig;
	std::array<std::vector<D3D12_INPUT_ELEMENT_DESC>, (size_t)(ParticlePsoType::COUNT)> particleInputLayouts;
	std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, (size_t)(ParticlePsoType::COUNT)> particlePSOs;

	ID3D12Device* d3dDevice;

	MeshGeometry billboardParticleVertices;
	UINT billboardParticleCount;
	std::vector<ShaderParticle> shaderParticles;

	std::random_device randomDevice;
	std::mt19937 randomEngine;
public:
	void Initialize(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList,
		DXGI_FORMAT backBufferFormat,
		DXGI_FORMAT depthStencilFormat,
		bool msaa4xState,
		UINT msaa4xQuality
	);
	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		UINT cbvSrvUavDescriptorSize
	);

	void Update(float dt, FrameResource* curFrameResource, const XMFLOAT3& cameraUp, const XMFLOAT3& cameraLook);
	void Render(ID3D12GraphicsCommandList* commandList, D3D12_GPU_VIRTUAL_ADDRESS passCBaddr);

	void CreateParticle(
		std::string particleName,
		DirectX::XMFLOAT3 position,
		DirectX::XMFLOAT3 size,
		DirectX::XMFLOAT3 rotation
	);
private:
	void BuildParticleRootSignature();
	void BuildPSOs(
		DXGI_FORMAT backBufferFormat,
		DXGI_FORMAT depthStencilFormat,
		bool msaa4xState,
		UINT msaa4xQuality
	);
	void BuildBillboardMeshGeometry(ID3D12GraphicsCommandList* commandList);
	void BuildParticleMaterialGpuBuffer(ID3D12GraphicsCommandList* commandList);

	void RenderBillBoardParticles(
		ID3D12GraphicsCommandList* commandList,
		D3D12_GPU_VIRTUAL_ADDRESS passCBaddr
	);
public:
	const static unsigned int MAX_BILLBOARD_PARTICLES = 10000;
};

