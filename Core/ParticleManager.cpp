#include "ParticleManager.h"
#include "../Assets/Particles.h"

#include <numeric>
#include <iterator>


void ParticleManager::Initialize(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* commandList,
	DXGI_FORMAT backBufferFormat,
	DXGI_FORMAT depthStencilFormat,
	bool msaa4xState,
	UINT msaa4xQuality
) {
	d3dDevice = device;
	// Load Shaders
	for (const auto& particleTexture : ParticleAssets::PartileTexureFiles) {
		auto texMap = std::make_unique<Texture>();
		texMap->Name = particleTexture.first;
		texMap->Filename = particleTexture.second;

		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(d3dDevice, commandList, texMap->Filename.c_str(),
			texMap->Resource, texMap->UploadHeap));

		textures[texMap->Name] = std::move(texMap);
	}

	BuildParticleRootSignature();
	BuildPSOs(backBufferFormat, depthStencilFormat, msaa4xState, msaa4xQuality);

	BuildBillboardMeshGeometry(commandList);
	BuildParticleMaterialGpuBuffer(commandList);

	shaderParticles.reserve(MAX_BILLBOARD_PARTICLES);
}

void ParticleManager::BuildParticleRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 30, 0, 0);

	CD3DX12_ROOT_PARAMETER slotRootParameter[3];
	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsShaderResourceView(0, 1);
	slotRootParameter[1].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[2].InitAsConstantBufferView(0);

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		2, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		3, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	std::array<CD3DX12_STATIC_SAMPLER_DESC, 4> staticSamplers =
	{
		linearWrap, linearClamp, anisotropicWrap, anisotropicClamp
	};

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(d3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(particleRootSig.GetAddressOf())));
}

void ParticleManager::BuildPSOs(
	DXGI_FORMAT backBufferFormat,
	DXGI_FORMAT depthStencilFormat,
	bool msaa4xState,
	UINT msaa4xQuality
) {
	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	shaders["ParticleBillboardVS"] = d3dUtil::CompileShader(L"Core\\Shaders\\ParticleBillboard.hlsl", nullptr, "VS", "vs_5_1");
	shaders["ParticleBillboardGS"] = d3dUtil::CompileShader(L"Core\\Shaders\\ParticleBillboard.hlsl", nullptr, "GS", "gs_5_1");
	shaders["ParticleBillboardPS"] = d3dUtil::CompileShader(L"Core\\Shaders\\ParticleBillboard.hlsl", alphaTestDefines, "PS", "ps_5_1");

	particleInputLayouts[(UINT)ParticlePsoType::BILLBOARD] = 
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "SIZE", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "UP", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "LOOK", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "MATIDX", 0, DXGI_FORMAT_R32_UINT, 0, 64, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC billboardPsoDesc;
	ZeroMemory(&billboardPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	billboardPsoDesc.InputLayout = { particleInputLayouts[(UINT)ParticlePsoType::BILLBOARD].data(), (UINT)particleInputLayouts[(UINT)ParticlePsoType::BILLBOARD] .size()};
	billboardPsoDesc.pRootSignature = particleRootSig.Get();
	billboardPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(shaders["ParticleBillboardVS"]->GetBufferPointer()),
		shaders["ParticleBillboardVS"]->GetBufferSize()
	};
	billboardPsoDesc.GS =
	{
		reinterpret_cast<BYTE*>(shaders["ParticleBillboardGS"]->GetBufferPointer()),
		shaders["ParticleBillboardGS"]->GetBufferSize()
	};
	billboardPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(shaders["ParticleBillboardPS"]->GetBufferPointer()),
		shaders["ParticleBillboardPS"]->GetBufferSize()
	};
	billboardPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	billboardPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	billboardPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	billboardPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;

	billboardPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;

	billboardPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	billboardPsoDesc.SampleMask = UINT_MAX;

	billboardPsoDesc.NumRenderTargets = 1;
	billboardPsoDesc.RTVFormats[0] = backBufferFormat;
	billboardPsoDesc.SampleDesc.Count = msaa4xState ? 4 : 1;
	billboardPsoDesc.SampleDesc.Quality = msaa4xState ? (msaa4xQuality - 1) : 0;
	billboardPsoDesc.DSVFormat = depthStencilFormat;

	ThrowIfFailed(d3dDevice->CreateGraphicsPipelineState(&billboardPsoDesc, IID_PPV_ARGS(&particlePSOs[(UINT)ParticlePsoType::BILLBOARD])));
}

void ParticleManager::BuildBillboardMeshGeometry(ID3D12GraphicsCommandList* commandList)
{
	std::vector<uint16_t> indices(MAX_BILLBOARD_PARTICLES);
	std::iota(indices.begin(), indices.end(), 0);

	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	billboardParticleVertices.Name = "BillboardParticles";

	billboardParticleVertices.VertexBufferCPU = nullptr;
	billboardParticleVertices.VertexBufferGPU = nullptr;
	billboardParticleVertices.VertexByteStride = sizeof(ShaderParticle);

	billboardParticleVertices.IndexBufferCPU = nullptr;
	billboardParticleVertices.IndexBufferGPU = d3dUtil::CreateDefaultBuffer(d3dDevice, commandList, indices.data(), ibByteSize, billboardParticleVertices.IndexBufferUploader);
	billboardParticleVertices.IndexFormat = DXGI_FORMAT_R16_UINT;
	billboardParticleVertices.IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;
	billboardParticleVertices.DrawArgs["points"] = submesh;
}

void ParticleManager::BuildParticleMaterialGpuBuffer(ID3D12GraphicsCommandList* commandList)
{
	const UINT vbByteSize = (UINT)(std::size(ParticleAssets::ParticleMaterials)) * sizeof(ParticleMaterial);
	materialBuffer = d3dUtil::CreateDefaultBuffer(d3dDevice,
		commandList, ParticleAssets::ParticleMaterials, vbByteSize, materialBufferUploader);
}

void ParticleManager::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, UINT cbvSrvUavDescriptorSize)
{
	hCpuSrvStart = hCpuSrv;
	hGpuSrvStart = hGpuSrv;

	// Build SRV
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor = hCpuSrv;
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	for (const auto& texture : textures) {
		const D3D12_RESOURCE_DESC& resourceDesc = texture.second->Resource->GetDesc();
		srvDesc.Format = resourceDesc.Format;
		srvDesc.Texture2D.MipLevels = resourceDesc.MipLevels;
		d3dDevice->CreateShaderResourceView(texture.second->Resource.Get(), &srvDesc, hDescriptor);
		// next descriptor
		hDescriptor.Offset(1, cbvSrvUavDescriptorSize);
	}
}

void ParticleManager::Update(float dt, FrameResource* curFrameResource, const XMFLOAT3& cameraUp, const XMFLOAT3& cameraLook)
{
	billboardParticleCount = 0;
	shaderParticles.clear();

	for (auto it = particleEmitters.begin(); it != particleEmitters.end();) {
		it->Update(dt, cameraUp, cameraLook);
		if (it->isFinished) {
			it = particleEmitters.erase(it);
		}
		else {
			if (it->particleProperties->renderer.mode == ParticleRenderMode::BILLBOARD) {
				for (const auto& particle : it->particles) {
					if (billboardParticleCount < MAX_BILLBOARD_PARTICLES) {
						shaderParticles.emplace_back(ShaderParticle{ particle.position, particle.size, particle.up, particle.look, particle.color, particle.materialIndex });
						// 	curFrameResource->BillbarodParticleVB->CopyData(billboardParticleCount++, sp);
					}
				}
			}

			++it;
		}
	}

	memcpy(curFrameResource->BillbarodParticleVB->mMappedData, shaderParticles.data(), sizeof(ShaderParticle) * (shaderParticles.size()));
	billboardParticleCount += (shaderParticles.size());

	billboardParticleVertices.VertexBufferGPU = curFrameResource->BillbarodParticleVB->Resource();
	const UINT vbByteSize = billboardParticleCount * sizeof(ShaderParticle);
	const UINT ibByteSize = billboardParticleCount * sizeof(std::uint16_t);
	billboardParticleVertices.VertexBufferByteSize = vbByteSize;
	billboardParticleVertices.IndexBufferByteSize = ibByteSize;
}

void ParticleManager::Render(
	ID3D12GraphicsCommandList* commandList,
	D3D12_GPU_VIRTUAL_ADDRESS passCBaddr
) {
	RenderBillBoardParticles(commandList, passCBaddr);
}

void ParticleManager::CreateParticle(std::string particleName, DirectX::XMFLOAT3 position, DirectX::XMFLOAT3 size, DirectX::XMFLOAT3 rotation)
{
	const ParticleProperties& properties = ParticleAssets::ParticleProps.at(particleName);
	particleEmitters.emplace_back(
		ParticleEmitter {
			position, size, rotation, &properties
		}
	);
}

void ParticleManager::RenderBillBoardParticles(
	ID3D12GraphicsCommandList* commandList,
	D3D12_GPU_VIRTUAL_ADDRESS passCBaddr
) {
	if (billboardParticleCount <= 0)
		return;

	commandList->SetGraphicsRootSignature(particleRootSig.Get());

	commandList->SetGraphicsRootShaderResourceView(0, materialBuffer->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(1, hGpuSrvStart);
	commandList->SetGraphicsRootConstantBufferView(2, passCBaddr);

	commandList->SetPipelineState(particlePSOs[(UINT)ParticlePsoType::BILLBOARD].Get());

	auto vbv = billboardParticleVertices.VertexBufferView();
	auto ibv = billboardParticleVertices.IndexBufferView();
	commandList->IASetVertexBuffers(0, 1, &vbv);
	commandList->IASetIndexBuffer(&ibv);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
	commandList->DrawIndexedInstanced(billboardParticleCount, 1, 0, 0, 0);
}
