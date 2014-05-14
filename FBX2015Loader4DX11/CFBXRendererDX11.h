// *********************************************************************************************************************
/// 
/// @file 		CFBXRendererDX11.h
/// @brief		FBXのパース用クラスからDirect3D11の描画を行うクラス
/// 
/// @author 	Masafumi Takahashi
/// @date 		2012/07/26
/// 
// *********************************************************************************************************************


#pragma once

#include "CFBXLoader.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

namespace FBX_LOADER
{

struct	VERTEX_DATA
{
	DirectX::XMFLOAT3	vPos;
	DirectX::XMFLOAT3	vNor;
	DirectX::XMFLOAT2	vTexcoord;
};

struct MATERIAL_CONSTANT_DATA
{
	DirectX::XMFLOAT4	ambient;
	DirectX::XMFLOAT4	diffuse;
	DirectX::XMFLOAT4	specular;
	DirectX::XMFLOAT4	emmisive;
};

struct MATERIAL_DATA
{
	DirectX::XMFLOAT4	ambient;
	DirectX::XMFLOAT4	diffuse;
	DirectX::XMFLOAT4	specular;
	DirectX::XMFLOAT4	emmisive;
	float specularPower;
	float TransparencyFactor;		// 透過度

	MATERIAL_CONSTANT_DATA materialConstantData;

	ID3D11ShaderResourceView*	pSRV;
	ID3D11SamplerState*         pSampler;
	ID3D11Buffer*				pMaterialCb;

	MATERIAL_DATA()
	{
		pSRV = nullptr;
		pSampler = nullptr;
		pMaterialCb = nullptr;
	}
	
	void Release()
	{
		if(pMaterialCb)
		{
			pMaterialCb->Release();
			pMaterialCb = nullptr;
		}

		if(pSRV)
		{
			pSRV->Release();
			pSRV = nullptr;
		}

		if(pSampler)
		{
			pSampler->Release();
			pSampler = nullptr;
		}
	}
};

struct	MESH_NODE
{
	ID3D11Buffer*		m_pVB;
	ID3D11Buffer*		m_pIB;
	ID3D11InputLayout*	m_pInputLayout;
	
	DWORD	vertexCount;
	DWORD	indexCount;

	MATERIAL_DATA materialData;

	float	mat4x4[16];

	// INDEX BUFFERのBIT
	enum INDEX_BIT
	{
		INDEX_NOINDEX = 0,
		INDEX_16BIT,		// 16bitインデックス
		INDEX_32BIT,		// 32bitインデックス
	};
	INDEX_BIT	m_indexBit;

	MESH_NODE()
	{
		m_pVB = nullptr;
		m_pIB = nullptr;
		m_pInputLayout = nullptr;
		m_indexBit = INDEX_NOINDEX;
		vertexCount = 0;
		indexCount = 0;
	}

	void Release()
	{
		materialData.Release();

		if(m_pInputLayout)
		{
			m_pInputLayout->Release();
			m_pInputLayout = nullptr;
		}
		if(m_pIB)
		{
			m_pIB->Release();
			m_pIB = nullptr;
		}
		if(m_pVB)
		{
			m_pVB->Release();
			m_pVB = nullptr;
		}
	}

	void SetIndexBit( const size_t indexCount)
	{
#if 0
		if(indexCount==0)
			m_indexBit = INDEX_NOINDEX;
		else if(indexCount < 0xffff)
			m_indexBit = INDEX_16BIT;
		else if(indexCount >=  0xffff)
			m_indexBit = INDEX_32BIT;
#else
		// 現状、16bitインデックス対応はまだ
		m_indexBit = INDEX_NOINDEX;
		if(indexCount!=0)
			m_indexBit = INDEX_32BIT;
#endif
	};
};

class CFBXRenderDX11
{
	CFBXLoader*		m_pFBX;
	
	std::vector<MESH_NODE>	m_meshNodeArray;

	HRESULT CreateNodes(ID3D11Device*	pd3dDevice);
	HRESULT VertexConstruction(ID3D11Device*	pd3dDevice,FBX_MESH_NODE &fbxNode, MESH_NODE& meshNode);
	HRESULT MaterialConstruction(ID3D11Device*	pd3dDevice,FBX_MESH_NODE &fbxNode,  MESH_NODE& meshNode);

	HRESULT CreateVertexBuffer( ID3D11Device*	pd3dDevice, ID3D11Buffer** pBuffer, void* pVertices, uint32_t stride, uint32_t vertexCount );
	HRESULT CreateIndexBuffer( ID3D11Device*	pd3dDevice, ID3D11Buffer** pBuffer, void* pIndices, uint32_t indexCount );

public:
	CFBXRenderDX11();
	~CFBXRenderDX11();

	void Release();

	HRESULT LoadFBX(const char* filename, ID3D11Device*	pd3dDevice);
	HRESULT CreateInputLayout(ID3D11Device*	pd3dDevice, const void* pShaderBytecodeWithInputSignature, size_t BytecodeLength, D3D11_INPUT_ELEMENT_DESC* pLayout, unsigned int layoutSize);

	HRESULT RenderAll( ID3D11DeviceContext* pImmediateContext);
	HRESULT RenderNode( ID3D11DeviceContext* pImmediateContext, const size_t nodeId );
	HRESULT RenderNodeInstancing( ID3D11DeviceContext* pImmediateContext, const size_t nodeId, const uint32_t InstanceCount );
	HRESULT RenderNodeInstancingIndirect( ID3D11DeviceContext* pImmediateContext, const size_t nodeId, ID3D11Buffer* pBufferForArgs,  const uint32_t AlignedByteOffsetForArgs );

	size_t GetNodeCount(){ return m_meshNodeArray.size(); }

	MESH_NODE& GetNode( const int id ){ return m_meshNodeArray[id]; };
	void	GetNodeMatrix( const int id, float* mat4x4 ){ memcpy(mat4x4, m_meshNodeArray[id].mat4x4, sizeof(float)*16); };
	MATERIAL_DATA& GetNodeMaterial( const size_t id ){ return m_meshNodeArray[id].materialData; };
};

}	// namespace FBX_LOADER