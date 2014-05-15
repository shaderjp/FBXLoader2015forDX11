// *********************************************************************************************************************
/// 
/// @file 		CFBXLoader.cpp
/// @brief		FBXのパース用クラス
/// 
/// @author 	Masafumi Takahashi
/// @date 		2012/07/26
/// 
// *********************************************************************************************************************


#include "CFBXLoader.h"

namespace FBX_LOADER
{

CFBXLoader::CFBXLoader()
{
	mSdkManager = nullptr;
	mScene = nullptr;
}

CFBXLoader::~CFBXLoader()
{
	Release();
}

//
void CFBXLoader::Release()
{
	m_meshNodeArray.clear();

	if(mImporter)
	{
		mImporter->Destroy();
		mImporter = nullptr;
	}

	if(mScene)
	{
		mScene->Destroy();
		mScene = nullptr;
	}

	if( mSdkManager )
	{
		mSdkManager->Destroy();
		mSdkManager = nullptr;
	}
}

HRESULT CFBXLoader::LoadFBX(const char* filename, const eAXIS_SYSTEM axis)
{
	if(!filename)
		return E_FAIL;

	HRESULT hr = S_OK;

	InitializeSdkObjects( mSdkManager, mScene );
	if(!mSdkManager)
		return E_FAIL;

	// インポータ作成
    int lFileFormat = -1;
    mImporter = FbxImporter::Create(mSdkManager,"");

	if (!mSdkManager->GetIOPluginRegistry()->DetectReaderFileFormat(filename, lFileFormat) )
    {
        // Unrecognizable file format. Try to fall back to FbxImporter::eFBX_BINARY
        lFileFormat = mSdkManager->GetIOPluginRegistry()->FindReaderIDByDescription( "FBX binary (*.fbx)" );;
    }

     // Initialize the importer by providing a filename.
    if(!mImporter || mImporter->Initialize(filename, lFileFormat) == false)
		return E_FAIL;
  
	//
	if( !mImporter || mImporter->Import(mScene) == false )
		return E_FAIL;

	FbxAxisSystem OurAxisSystem = FbxAxisSystem::DirectX;

	if(axis==eAXIS_OPENGL)
		OurAxisSystem = FbxAxisSystem::OpenGL;
		
	// DirectX系
    FbxAxisSystem SceneAxisSystem = mScene->GetGlobalSettings().GetAxisSystem();
	if(SceneAxisSystem != OurAxisSystem)
	{
		FbxAxisSystem::DirectX.ConvertScene(mScene);
	}

    // 単位系の統一
	// 不要でもいいかも
    FbxSystemUnit SceneSystemUnit = mScene->GetGlobalSettings().GetSystemUnit();
    if( SceneSystemUnit.GetScaleFactor() != 1.0 )
    {
        // センチメーター単位にコンバートする
        FbxSystemUnit::cm.ConvertScene( mScene );
    }

	// 三角形化(三角形以外のデータでもコレで安心)
	TriangulateRecursive(mScene->GetRootNode());

	Setup();

	return hr;
}

//
void CFBXLoader::InitializeSdkObjects(FbxManager*& pManager, FbxScene*& pScene)
{
    //The first thing to do is to create the FBX Manager which is the object allocator for almost all the classes in the SDK
    pManager = FbxManager::Create();
    if( !pManager )
    {
        FBXSDK_printf("Error: Unable to create FBX Manager!\n");
        exit(1);
    }
	else FBXSDK_printf("Autodesk FBX SDK version %s\n", pManager->GetVersion());

	//Create an IOSettings object. This object holds all import/export settings.
	FbxIOSettings* ios = FbxIOSettings::Create(pManager, IOSROOT);
	pManager->SetIOSettings(ios);

	//Load plugins from the executable directory (optional)
	FbxString lPath = FbxGetApplicationDirectory();
	pManager->LoadPluginsDirectory(lPath.Buffer());

    //Create an FBX scene. This object holds most objects imported/exported from/to files.
    pScene = FbxScene::Create(pManager, "My Scene");
	if( !pScene )
    {
        FBXSDK_printf("Error: Unable to create FBX scene!\n");
        exit(1);
    }
}

// 三角形化
void CFBXLoader::TriangulateRecursive(FbxNode* pNode)
{
	FbxNodeAttribute* lNodeAttribute = pNode->GetNodeAttribute();

    if (lNodeAttribute)
    {
        if (lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh ||
            lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eNurbs ||
            lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eNurbsSurface ||
            lNodeAttribute->GetAttributeType() == FbxNodeAttribute::ePatch)
        {
			FbxGeometryConverter lConverter(pNode->GetFbxManager());
			// これでどんな形状も三角形化
#if 0
            lConverter.TriangulateInPlace(pNode);	// 古い手法
#endif // 0
			lConverter.Triangulate( mScene, true );
        }
    }

	const int lChildCount = pNode->GetChildCount();
    for (int lChildIndex = 0; lChildIndex < lChildCount; ++lChildIndex)
    {
		// 子ノードを探索
        TriangulateRecursive(pNode->GetChild(lChildIndex));
    }
}

//
FbxNode&	CFBXLoader::GetRootNode()
{
	return *mScene->GetRootNode();
}

void CFBXLoader::Setup()
{
	// RootNodeから探索していく
	if(mScene->GetRootNode())
	{
		SetupNode(mScene->GetRootNode(), "null");
	}
}

void CFBXLoader::SetupNode(FbxNode* pNode, std::string parentName)
{
	if(!pNode)
		return ;

	FBX_MESH_NODE meshNode;

	meshNode.name = pNode->GetName();
	meshNode.parentName = parentName;

	ZeroMemory( &meshNode.elements, sizeof(MESH_ELEMENTS) );

	FbxMesh* lMesh = pNode->GetMesh();	

	if(lMesh)
	{
		const int lVertexCount = lMesh->GetControlPointsCount();

		if (lVertexCount>0)
		{
			// 頂点があるならノードにコピー
			CopyVertexData(lMesh, &meshNode);
		}
	}

	// マテリアル
	const int lMaterialCount = pNode->GetMaterialCount();
	for(int i=0;i<lMaterialCount;i++)
	{
		FbxSurfaceMaterial* mat = pNode->GetMaterial(i);
		if(!mat)
			continue;

		FBX_MATERIAL_NODE destMat;
		CopyMatrialData(mat, &destMat);

		meshNode.m_materialArray.push_back(destMat);
	}

	//
	ComputeNodeMatrix(pNode, &meshNode);

	m_meshNodeArray.push_back(meshNode);

	const int lCount = pNode->GetChildCount();
	for (int i = 0; i < lCount; i++)
	{
		SetupNode(pNode->GetChild(i), meshNode.name);
	}
}

//
void CFBXLoader::SetFbxColor(FBX_MATRIAL_ELEMENT& destColor,const FbxDouble3 srcColor)
{
	destColor.a = 1.0f;
	destColor.r = static_cast<float>(srcColor[0]);
	destColor.g = static_cast<float>(srcColor[1]);
	destColor.b = static_cast<float>(srcColor[2]);
}

//
FbxDouble3 CFBXLoader::GetMaterialProperty(
		const FbxSurfaceMaterial * pMaterial,
		const char * pPropertyName,
        const char * pFactorPropertyName,
        FBX_MATRIAL_ELEMENT*			pElement)
{
	pElement->type = FBX_MATRIAL_ELEMENT::ELEMENT_NONE;

    FbxDouble3 lResult(0, 0, 0);
    const FbxProperty lProperty = pMaterial->FindProperty(pPropertyName);
    const FbxProperty lFactorProperty = pMaterial->FindProperty(pFactorPropertyName);
    if (lProperty.IsValid() && lFactorProperty.IsValid())
    {
        lResult = lProperty.Get<FbxDouble3>();
        double lFactor = lFactorProperty.Get<FbxDouble>();
        if (lFactor != 1)
        {
            lResult[0] *= lFactor;
            lResult[1] *= lFactor;
            lResult[2] *= lFactor;
        }

		pElement->type = FBX_MATRIAL_ELEMENT::ELEMENT_COLOR;
    }

    if (lProperty.IsValid())
    {
		int existTextureCount = 0;
        const int lTextureCount = lProperty.GetSrcObjectCount<FbxFileTexture>();

		for(int i=0;i<lTextureCount;i++)
		{
			FbxFileTexture* lFileTexture = lProperty.GetSrcObject<FbxFileTexture>(i);
			if(!lFileTexture)
				continue;

			FbxString uvsetName = lFileTexture->UVSet.Get();
			std::string uvSetString = uvsetName.Buffer();
			std::string filepath = lFileTexture->GetFileName();

			pElement->textureSetArray[uvSetString].push_back(filepath);
			existTextureCount++;
		}
	
		const int lLayeredTextureCount = lProperty.GetSrcObjectCount<FbxLayeredTexture>();

		for(int i=0;i<lLayeredTextureCount;i++)
		{
			FbxLayeredTexture* lLayeredTexture = lProperty.GetSrcObject<FbxLayeredTexture>(i);

			const int lTextureFileCount = lLayeredTexture->GetSrcObjectCount<FbxFileTexture>();

			for(int j=0;j<lTextureFileCount;j++)
			{
				FbxFileTexture* lFileTexture = lLayeredTexture->GetSrcObject<FbxFileTexture>(j);
				if(!lFileTexture)
					continue;

				FbxString uvsetName = lFileTexture->UVSet.Get();
				std::string uvSetString = uvsetName.Buffer();
				std::string filepath = lFileTexture->GetFileName();

				pElement->textureSetArray[uvSetString].push_back(filepath);
				existTextureCount++;
			}
		}

		if(existTextureCount > 0)
		{
			if(pElement->type == FBX_MATRIAL_ELEMENT::ELEMENT_COLOR)
				pElement->type = FBX_MATRIAL_ELEMENT::ELEMENT_BOTH;
			else
				pElement->type = FBX_MATRIAL_ELEMENT::ELEMENT_TEXTURE;
		}
     }

    return lResult;
}

//
void CFBXLoader::CopyMatrialData(FbxSurfaceMaterial* mat, FBX_MATERIAL_NODE* destMat)
{
	if(!mat)
		return ;

	if ( mat->GetClassId().Is( FbxSurfaceLambert::ClassId ) ) 
	{
		destMat->type = FBX_MATERIAL_NODE::MATERIAL_LAMBERT;
	}
	else if ( mat->GetClassId().Is( FbxSurfacePhong::ClassId ) ) 
	{
		destMat->type = FBX_MATERIAL_NODE::MATERIAL_PHONG;
	}

	const FbxDouble3 lEmissive = GetMaterialProperty(mat,
		FbxSurfaceMaterial::sEmissive, FbxSurfaceMaterial::sEmissiveFactor, &destMat->emmisive);
	SetFbxColor(destMat->emmisive, lEmissive );

    const FbxDouble3 lAmbient = GetMaterialProperty(mat,
        FbxSurfaceMaterial::sAmbient, FbxSurfaceMaterial::sAmbientFactor, &destMat->ambient);
	SetFbxColor(destMat->ambient, lAmbient );

    const FbxDouble3 lDiffuse = GetMaterialProperty(mat,
        FbxSurfaceMaterial::sDiffuse, FbxSurfaceMaterial::sDiffuseFactor, &destMat->diffuse);
	SetFbxColor(destMat->diffuse, lDiffuse );

    const FbxDouble3 lSpecular = GetMaterialProperty(mat,
        FbxSurfaceMaterial::sSpecular, FbxSurfaceMaterial::sSpecularFactor, &destMat->specular);
	SetFbxColor(destMat->specular, lSpecular );

	//
	FbxProperty lTransparencyFactorProperty = mat->FindProperty(FbxSurfaceMaterial::sTransparencyFactor);
	if(lTransparencyFactorProperty.IsValid())
	{
		double lTransparencyFactor = lTransparencyFactorProperty.Get<FbxDouble>();
		destMat->TransparencyFactor = static_cast<float>(lTransparencyFactor);
	}

	// Specular Power
    FbxProperty lShininessProperty = mat->FindProperty(FbxSurfaceMaterial::sShininess);
    if (lShininessProperty.IsValid())
    {
        double lShininess = lShininessProperty.Get<FbxDouble>();
		destMat->shininess = static_cast<float>(lShininess);
	}
}

//
void CFBXLoader::ComputeNodeMatrix(FbxNode* pNode, FBX_MESH_NODE* meshNode)
{
	if(!pNode || !meshNode)
	{
		return ;
	}

	FbxAnimEvaluator* lEvaluator = mScene->GetAnimationEvaluator();
	FbxMatrix lGlobal;
	lGlobal.SetIdentity();

	if(pNode != mScene->GetRootNode())
	{
		lGlobal= lEvaluator->GetNodeGlobalTransform(pNode);

		FBXMatrixToFloat16( &lGlobal, meshNode->mat4x4 );
	}
	else
	{
		FBXMatrixToFloat16( &lGlobal, meshNode->mat4x4 );
	}
}

//
void CFBXLoader::CopyVertexData(FbxMesh*	pMesh, FBX_MESH_NODE* meshNode)
{
	if(!pMesh)
		return ;

	int lPolygonCount = pMesh->GetPolygonCount();

	FbxVector4 pos, nor;
	
	meshNode->elements.numPosition = 1;
	meshNode->elements.numNormal = 1;

	unsigned int indx = 0;
	
	for(int i=0;i<lPolygonCount;i++)
	{
		// ポリゴン内の頂点数(一応、三角形化してるので3点のはずだがチェック)
		int lPolygonsize = pMesh->GetPolygonSize(i);

		for(int pol=0;pol<lPolygonsize;pol++)
		{
			int index = pMesh->GetPolygonVertex(i, pol);
			meshNode->indexArray.push_back(indx);

			pos = pMesh->GetControlPointAt(index);
			pMesh->GetPolygonVertexNormal(i,pol,nor);

			meshNode->m_positionArray.push_back( pos );
			meshNode->m_normalArray.push_back( nor );
	
			++indx;
		}
	}

	// UV処理(UVは2つ以上ある場合があるので別処理)
	FbxStringList	uvsetName;
	pMesh->GetUVSetNames(uvsetName);
	int numUVSet = uvsetName.GetCount();
	meshNode->elements.numUVSet = numUVSet;

	bool unmapped = false;

	for(int uv=0;uv<numUVSet;uv++)
	{
		meshNode->uvsetID[uvsetName.GetStringAt(uv)] = uv;
		for(int i=0;i<lPolygonCount;i++)
		{
			int lPolygonsize = pMesh->GetPolygonSize(i);

			for(int pol=0;pol<lPolygonsize;pol++)
			{
				FbxString name = uvsetName.GetStringAt(uv);
				FbxVector2 texCoord;
				pMesh->GetPolygonVertexUV( i, pol, name, texCoord, unmapped);
				meshNode->m_texcoordArray.push_back(texCoord);
			}
		}
	}
}

FBX_MESH_NODE& CFBXLoader::GetNode(const unsigned int id)
{
	return m_meshNodeArray[id];
}

}	// FBX_LOADER