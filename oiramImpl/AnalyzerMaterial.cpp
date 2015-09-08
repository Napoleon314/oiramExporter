#include "stdafx.h"
#include "Analyzer.h"
#include <IGame/IGame.h>
#include <IGame/IGameObject.h> 
#include <stdmat.h>
#if defined(MAX_RELEASE_R12_ALPHA) && MAX_RELEASE >= MAX_RELEASE_R12_ALPHA
	#include <IFileResolutionManager.h>
#pragma comment(lib, "assetmanagement.lib")
#else
	#include <IPathConfigMgr.h>
#endif
#include <fstream>
#include <algorithm>
#include "normalrender.h"
#include "mix.h"
#include "ImageLoader.h"
#include "ImageCompressor.h"
#include "requisites.h"
#include "scene.h"
#include "strutil.h"
#include "LogManager.h"

bool Analyzer::
materialComp(const oiram::Material& lhs, const oiram::Material& rhs)
{
	bool result = true;
	result = result && (lhs.twoSided == rhs.twoSided);
	result = result && (lhs.phongShading == rhs.phongShading);
	result = result && (lhs.alphaBlend == rhs.alphaBlend);
	result = result && (lhs.alphaTest == rhs.alphaTest);
	result = result && (lhs.addBlend == rhs.addBlend);

	result = result && (lhs.diffuseColour.equals(rhs.diffuseColour));
	result = result && (lhs.diffuseColourEnable == rhs.diffuseColourEnable);
	result = result && (lhs.emissiveColour.equals(rhs.emissiveColour));
	result = result && (lhs.emissiveColourEnable == rhs.emissiveColourEnable);
	result = result && (lhs.specularColour.equals(rhs.specularColour));
	result = result && (lhs.specularColourEnable == rhs.specularColourEnable);

	result = result && (lhs.textureSlots.size() == rhs.textureSlots.size());
	if (result)
	{
		for (size_t t = 0; t < lhs.textureSlots.size(); ++t)
		{
			auto& x = lhs.textureSlots[t];
			auto& y = rhs.textureSlots[t];

			result = result && (x->name == y->name);
			result = result && (x->texunit == y->texunit);
			result = result && (x->mapChannel == y->mapChannel);
			result = result && (x->colourOpEx.operation == y->colourOpEx.operation);
			result = result && (x->colourOpEx.source1 == y->colourOpEx.source1);
			result = result && (x->colourOpEx.source2 == y->colourOpEx.source2);
			result = result && (x->alphaOpEx.operation == y->alphaOpEx.operation);
			result = result && (x->alphaOpEx.source1 == y->alphaOpEx.source1);
			result = result && (x->alphaOpEx.source2 == y->alphaOpEx.source2);
			result = result && (x->frames.size() == y->frames.size());
			if (result)
			{
				for (size_t f = 0; f < x->frames.size(); ++f)
				{
					auto& a = x->frames[f];
					auto& b = y->frames[f];
					result = result && (a == b);
				}
			}
			result = result && (oiram::fequal(x->frameTime, y->frameTime));
		}
	}	

	result = result && (lhs.uvCoordinateMap.size() == rhs.uvCoordinateMap.size());
	if (result)
	{
		for (auto& uvCoordinateItor : lhs.uvCoordinateMap)
		{
			auto& channel = uvCoordinateItor.first;
			auto& coordinate = uvCoordinateItor.second;

			auto findItor = rhs.uvCoordinateMap.find(channel);
			result = result && (findItor != rhs.uvCoordinateMap.end());
			if (result)
			{
				result = result && (coordinate->u_mode == findItor->second->u_mode);
				result = result && (coordinate->v_mode == findItor->second->v_mode);
			}
		}
	}

	return result;
}


void Analyzer::
createDefaultMaterial()
{
	// û�в��ʵ������materialָ����nullptr, ��ӦΪĬ�ϲ���
	std::shared_ptr<oiram::Material> defaultMaterial(new oiram::Material);
	defaultMaterial->rootName = defaultMaterial->name = "";
	defaultMaterial->twoSided = false;
	defaultMaterial->phongShading = false;
	defaultMaterial->alphaBlend = false;
	defaultMaterial->alphaTest = false;
	defaultMaterial->addBlend = false;
	defaultMaterial->diffuseColourEnable = false;
	defaultMaterial->emissiveColourEnable = false;
	defaultMaterial->specularColourEnable = false;

	mMaterialMap.insert(std::make_pair(nullptr, defaultMaterial));
}


void Analyzer::
processMaterial(IGameMaterial* gameMaterial, const std::string& rootMaterialName)
{
	// ����Ƕ��ز���
	if (gameMaterial->IsMultiType())
	{
		// ����ǿǲ���
		MCHAR* materialClass = gameMaterial->GetMaterialClass();
		if (_tcscmp(materialClass, _T("Shell Material")) == 0)
		{
			// ����Ӳ��ʳ���1��
			if (gameMaterial->GetSubMaterialCount() > 1)
			{
				// ���ڶ�����Ϊ��lightingMap��
				IGameMaterial* gameOrigMaterial = gameMaterial->GetSubMaterial(0);
				IGameMaterial* gameBakedMaterial = gameMaterial->GetSubMaterial(1);

				// �ǲ����¿����Ƕ��ز���
				if (gameOrigMaterial->IsMultiType() &&
					gameBakedMaterial->IsMultiType())
				{
					// ���ε�����Ϣ
					int origSubCount = gameOrigMaterial->GetSubMaterialCount(),
						bakedSubCount = gameBakedMaterial->GetSubMaterialCount();
					assert(origSubCount >= bakedSubCount);
					for (int n = 0; n < origSubCount; ++n)
					{
						IGameMaterial* gameOrigSubMaterial = gameOrigMaterial->GetSubMaterial(n);
						int bakedIndex = std::min(n, bakedSubCount - 1);
						IGameMaterial* gameBakedSubMaterial = gameBakedMaterial->GetSubMaterial(bakedIndex);
						auto origSubMaterial = dumpMaterialProperty(gameOrigSubMaterial, rootMaterialName), 
							bakedSubMaterial = dumpMaterialProperty(gameBakedSubMaterial, rootMaterialName);

						// ���ڶ�����ʵĵ�diffuseMap��ص�һ�������ΪlightingMap
						auto textureSlotItor = std::find_if(bakedSubMaterial->textureSlots.begin(), bakedSubMaterial->textureSlots.end(), 
							[](const oiram::Material::TextureSlotContainer::value_type& textureSlot){
								return textureSlot->name == "diffuseMap"; });
						if (textureSlotItor != bakedSubMaterial->textureSlots.end())
						{
							auto& textureSlot = *textureSlotItor;
							textureSlot->name = "lightMap";
							textureSlot->texunit = oiram::Material::TU_LightMap;
							// ʹ��diffuseMap��alphaͨ��
							textureSlot->alphaOpEx.operation = oiram::Material::Op_Source1;
							textureSlot->alphaOpEx.source1 = oiram::Material::Src_Current;
							textureSlot->alphaOpEx.source2 = oiram::Material::Src_Texture;

							if (origSubMaterial->uvCoordinateMap.count(textureSlot->mapChannel))
								LogManager::getSingleton().logMessage(true, "Diffuse map and lightmap, using the same channel : (%d).", textureSlot->mapChannel);

							// ��lightingMap��map channleҲ����Դ������
							int bakedMapChannel = textureSlot->mapChannel;
							origSubMaterial->uvCoordinateMap.insert(
								std::make_pair(bakedMapChannel, std::move(bakedSubMaterial->uvCoordinateMap[bakedMapChannel])));

							origSubMaterial->textureSlots.push_back(std::move(textureSlot));

							// �ǹ̶�����Ĭ��ʹ�ö���ѹ��, ͬʱUVͨ������1��
							if (config.renderingType != RT_FixedFunction &&
								origSubMaterial->uvCoordinateMap.size() > 1)
								origSubMaterial->packedTexcoords = true;
						}
						addMaterial(gameOrigSubMaterial, origSubMaterial);
					}
				}
				else
				{
					auto	origMaterial = dumpMaterialProperty(gameOrigMaterial, rootMaterialName),
							bakedMaterial = dumpMaterialProperty(gameBakedMaterial, rootMaterialName);

					// ���ڶ�����ʵĵ�diffuseMap��ص�һ�������ΪlightingMap
					auto textureSlotItor = std::find_if(bakedMaterial->textureSlots.begin(), bakedMaterial->textureSlots.end(), 
						[](const oiram::Material::TextureSlotContainer::value_type& textureSlot){
							return textureSlot->name == "diffuseMap"; });
					if (textureSlotItor != bakedMaterial->textureSlots.end())
					{
						auto& textureSlot = *textureSlotItor;
						textureSlot->name = "lightMap";
						textureSlot->texunit = oiram::Material::TU_LightMap;
						// ʹ��diffuseMap��alphaͨ��
						textureSlot->alphaOpEx.operation = oiram::Material::Op_Source1;
						textureSlot->alphaOpEx.source1 = oiram::Material::Src_Current;
						textureSlot->alphaOpEx.source2 = oiram::Material::Src_Texture;

						// ��lightingMap��map channleҲ����Դ������
						int bakedMapChannel = textureSlot->mapChannel;
						origMaterial->uvCoordinateMap.insert(
							std::make_pair(bakedMapChannel, std::move(bakedMaterial->uvCoordinateMap[bakedMapChannel])));

						origMaterial->textureSlots.push_back(std::move(textureSlot));

						// �ǹ̶�����Ĭ��ʹ�ö���ѹ��, ͬʱUVͨ������1��
						if (config.renderingType != RT_FixedFunction &&
							origMaterial->uvCoordinateMap.size() > 1)
							origMaterial->packedTexcoords = true;
					}
					addMaterial(gameOrigMaterial, origMaterial);
				}
			}
			// �����һ�����
			else
			{
				auto origMaterial = dumpMaterialProperty(gameMaterial->GetSubMaterial(0), rootMaterialName);
				addMaterial(gameMaterial, origMaterial);
			}
		}
		else
		{
			// ���δ������Ӳ���
			for (int n = 0; n < gameMaterial->GetSubMaterialCount(); ++n)
				processMaterial(gameMaterial->GetSubMaterial(n), rootMaterialName);
		}
	}
	// ����Ψһ�Ĳ���
	else
	{
		auto material = dumpMaterialProperty(gameMaterial, rootMaterialName);
		addMaterial(gameMaterial, material);
	}
}


// ��ͼ��Դ
struct BitmapResource
{
	std::string						rename;			// �������������
	std::string						srcFilePath;	// Դ·��
	std::string						dstFilePath;	// Ŀ��·��
	oiram::Material::TextureUnit	texunit;		// ����
	BitmapResource() : texunit(oiram::Material::TextureUnit::TU_Unknow) {}
};

void Analyzer::
exportTextures()
{
	// mMaterialMap������ܻ���һЩ��ͬ�Ĳ���, �ռ���mMaterials�еĲ��ʾ���Ψһ
	mMaterials.reserve(mMaterialMap.size());
	for (auto& materialPair : mMaterialMap)
	{
		auto& material = materialPair.second;
		if (material->isUsed &&
			materialPair.first &&
			std::find(mMaterials.begin(), mMaterials.end(), material) == mMaterials.end())
		{
			mMaterials.push_back(material);
		}
	}

	// ����Ǳ���ԭ��ͼ����Ҫ�����ļ������Ƿ���Ч��
	bool legalityChecking = config.imageCompressionType != CT_Original;
	// ��������
	std::map<std::string, BitmapResource> bitmapMap;	// ������Ϣ��
	for (auto& material : mMaterials)
	{
		// �������е�����(��������֡)
		for (auto& textureSlot : material->textureSlots)
		{
			for (auto& frame : textureSlot->frames)
			{
				auto result = bitmapMap.insert(std::make_pair(frame, BitmapResource()));
				if (result.second)
				{
					auto& resource = result.first->second;
					// �������ļ���
					std::string::size_type pos = frame.find_last_of('\\');
					resource.rename = pos == std::string::npos ? frame : frame.substr(pos + 1);
					// �ļ���Ψһ
					resource.rename = UniqueNameGenerator::getSingleton().generate(resource.rename, "bitmap", true, legalityChecking);
					// Դ�ļ�·��
					resource.srcFilePath = frame;
					// Ŀ���ļ�·��
					resource.dstFilePath = config.exportPath + resource.rename;
					resource.texunit = textureSlot->texunit;
				}
			}
		}
	}

	// ���������ռ�������ͼ
	LogManager::getSingleton().setProgress(0);
	size_t textureProgress = 0, numTextures = bitmapMap.size();
	for (auto& texture : bitmapMap)
	{
		auto& resource = texture.second;
		// ������ͼ
		bool originalCopy = true;
		std::string imageName = resource.rename;
		LogManager::getSingleton().logMessage(false, "Exporting: %s", imageName.c_str());
		LogManager::getSingleton().setProgress(static_cast<int>(static_cast<float>(textureProgress++) / numTextures * 100));

		// FreeImageֻ�ܶ�dds������дdds, ���Ե�Դ��ͼ��DDSʱʹ��pvrtclib����
		CompressionType compressionType = config.imageCompressionType;
		std::string::size_type pos = resource.srcFilePath.find_last_of('.');
		if (pos != std::string::npos)
		{
			std::string ext = str::lowercase(resource.srcFilePath.substr(pos));
			if (ext == ".dds")
				compressionType = CT_DXTC;
		}

		bool isNormalMap = resource.texunit == oiram::Material::TU_NormalMap;
		bool isLightMap = resource.texunit == oiram::Material::TU_LightMap;
		bool isCompression = false;
		// ͨ��PVRTCLib����ѹ����ͼ
		if (compressionType != CT_Original)
		{
			// ֱ�ӽ�ѹ����ʽ�ĺ�׺��ӵ�ԭ��ͼ�ļ����ĺ���
			switch (config.imageCompressionType)
			{
			case CT_PNG:
				imageName += ".png";
				break;

			case CT_TGA:
				imageName += ".tga";
				break;

			case CT_DXTC:
				imageName += ".dds";
				isCompression = true;
				break;

			case CT_ETC1:
			case CT_ETC2:
				imageName += ".ktx";
				isCompression = true;
				break;

			case CT_PVRTC2_4BPP:
				imageName += ".pvr";
				isCompression = true;
				break;
			}

			if (isCompression)
			{
				originalCopy = !ImageCompressor::compress(compressionType, resource.srcFilePath, resource.dstFilePath,
					isNormalMap, isLightMap, config.imageCompressionQuality, config.imageGenerationMipmaps);

				// ѹ��ʧ��, ��Ϊ��ͨ����, ����������Ϣ
				if (originalCopy)
					LogManager::getSingleton().logMessage(true, "Image compression failure: \"%s\" to \"%s\".",
					resource.srcFilePath.c_str(), resource.dstFilePath.c_str());
			}
		}

		// �������ͨ����, ������ѹ��ʧ��
		if (originalCopy)
		{
			const std::string& srcImageName = resource.srcFilePath;
			std::string dstImageName = config.exportPath + imageName;

			bool result = false;
			ImageLoader& imageLoader = ImageLoader::getSingleton();
			if (imageLoader.load(srcImageName, isNormalMap))
			{
				if (config.imageCompressionType == CT_PNG)
					ImageLoader::getSingleton().saveAsPNG();
				else
				if (config.imageCompressionType == CT_TGA)
					ImageLoader::getSingleton().saveAsTGA();

				result = imageLoader.save(dstImageName);
				if (!result)
					result = TRUE == CopyFile(srcImageName.c_str(), dstImageName.c_str(), FALSE);
			}

			if (!result)
				LogManager::getSingleton().logMessage(true, "Copy image failure: \"%s\" to \"%s\".", srcImageName.c_str(), dstImageName.c_str());
		}

		resource.rename = imageName;
	}

	// �����������ͼ�ļ���
	for (auto& material : mMaterials)
	{
		// �������е�����(��������֡)
		for (auto& textureSlot : material->textureSlots)
		{
			for (auto& frame : textureSlot->frames)
			{
				assert(bitmapMap.count(frame));
				auto& resource = bitmapMap[frame];

				// ��������
				frame = resource.rename;
			}
		}
	}
}


bool Analyzer::
getMapPath(MSTR& mapPath)
{
// 2010(����)֮ǰ���ǿ���ͨ��IPathConfigMgr::GetPathConfigMgr()->GetFullFilePath��·�������б��������ļ�
// ֮��ýӿڱ�ȡ����, ����ʹ��IFileResolutionManager::GetFullFilePath(const TCHAR* filePath, MaxSDK::AssetManagement::AssetType assetType). 
// ͨ���ӿڿ��Դ�������Ŀ·��������, Ȼ�󷵻�ʵ����ͼ·��
#if defined(MAX_RELEASE_R12_ALPHA) && MAX_RELEASE >= MAX_RELEASE_R12_ALPHA
	MSTR mstrFullFilePath = IFileResolutionManager::GetInstance()->GetFullFilePath(mapPath, MaxSDK::AssetManagement::kBitmapAsset);
#else
	MSTR mstrFullFilePath = IPathConfigMgr::GetPathConfigMgr()->GetFullFilePath(mapPath, false);
#endif

	bool result = !mstrFullFilePath.isNull();
	if (result)
		mapPath = mstrFullFilePath;
	else
		LogManager::getSingleton().logMessage(true, "Cannot find map: \"%s\".", mapPath);

	return result;
}


void Analyzer::
addMaterial(IGameMaterial* gameMaterial, const std::shared_ptr<oiram::Material>& oiramMaterial)
{
	// �����еĲ����в���
	for (auto& materialPair : mMaterialMap)
	{
		auto& storageMaterial = materialPair.second;
		// ����²��������еĲ���������ͬ
		if (materialComp(*storageMaterial, *oiramMaterial))
		{
			// ��ֱ��ʹ�����еĲ���, ��������
			mMaterialMap.insert(std::make_pair(gameMaterial, storageMaterial));
			return;
		}
	}

	// ����²���
	mMaterialMap.insert(std::make_pair(gameMaterial, oiramMaterial));
}


// λͼ
struct BitmapSlot
{
	std::string					name;		// ����
	BitmapTex*					bitmapTex;	// λͼ����
	oiram::Material::ColourOpEx	colourOpEx;	// color��Ϸ�ʽ
	oiram::Material::AlphaOpEx	alphaOpEx;	// alpha��Ϸ�ʽ
};

std::shared_ptr<oiram::Material> Analyzer::
dumpMaterialProperty(IGameMaterial* gameMaterial, const std::string& rootMaterialName)
{
	std::shared_ptr<oiram::Material> material(new oiram::Material);

	// �����ļ������� = �����ʵ�����
	material->rootName = rootMaterialName;

	material->name = Mchar2Ansi(gameMaterial->GetMaterialName());
	material->name = UniqueNameGenerator::getSingleton().generate(material->name, "material", true, true);

	LogManager::getSingleton().logMessage(false, "Exporting: %s", material->name.c_str());

	// ȡ����������ɫ
	IGameProperty* p = gameMaterial->GetDiffuseData();
	if (p && p->GetType() == IGAME_POINT3_PROP)
	{
		Point3 diffuse;
		p->GetPropertyValue(diffuse);

		// ���׵Ĳ���Ҫ����
		if (diffuse.Equals(Point3(1, 1, 1)))
			material->diffuseColourEnable = false;
		else
			material->diffuseColour.set(diffuse.x, diffuse.y, diffuse.z, 1.0f);
	}

	// ͸����
	p = gameMaterial->GetOpacityData();
	if (p && p->GetType() == IGAME_FLOAT_PROP)
	{
		float opacity = 0.0f;
		p->GetPropertyValue(opacity);

		if (material->diffuseColourEnable)
			material->diffuseColour.w = opacity;
	}

	// ȡ���߹���ɫ
	p = gameMaterial->GetSpecularData();
	if (p && p->GetType() == IGAME_POINT3_PROP)
	{
		Point3 specular;
		p->GetPropertyValue(specular);

		// ���ڵĲ���Ҫ����
		if (specular.Equals(Point3(0, 0, 0)))
			material->specularColourEnable = false;
		else
			material->specularColour.set(specular.x, specular.y, specular.z, 100.0f);
	}

	// �����
	p = gameMaterial->GetGlossinessData();
	if (p && p->GetType() == IGAME_FLOAT_PROP)
	{
		float glossiness = 0.0f;
		p->GetPropertyValue(glossiness);

		if (material->specularColourEnable)
			material->specularColour.w = glossiness * 255.0f;
	}

	// ȡ���Է�����ɫ
	p = gameMaterial->GetEmissiveData();
	if (p && p->GetType() == IGAME_POINT3_PROP)
	{
		Point3 emissive;
		p->GetPropertyValue(emissive);

		// ���ڲ���Ҫ����
		if (emissive.Equals(Point3(0,0,0)))
			material->emissiveColourEnable = false;
		else
			material->emissiveColour.set(emissive.x, emissive.y, emissive.z, 1.0f);
	}

	// ����˫����ʺ�����
	Mtl* maxMaterial = gameMaterial->GetMaxMaterial();
	Class_ID mtlClassID = maxMaterial->ClassID();
	if (mtlClassID == Class_ID(DMTL_CLASS_ID, 0))
	{
		StdMat2* maxStdMaterial = static_cast<StdMat2*>(maxMaterial);

		// ˫�����
		material->twoSided = maxStdMaterial->GetTwoSided() == TRUE;
		// ����ģ��
		material->phongShading = !maxStdMaterial->IsFaceted();

		// ֧��3dmax��ͼ: mix, normal, standard, ifl
		int numTexMaps = gameMaterial->GetNumberOfTextureMaps();
		for (int texMapIdx = 0; texMapIdx < numTexMaps; ++texMapIdx)
		{
			IGameTextureMap* gameTexMap = gameMaterial->GetIGameTextureMap(texMapIdx);
			int mapSlot = gameTexMap->GetStdMapSlot();
			/*
				  - ID_AM - Ambient (value 0)
				  - ID_DI - Diffuse (value 1)
				  - ID_SP - Specular (value 2)
				  - ID_SH - Shininess (value 3). In R3 and later this is called Glossiness.
				  - ID_SS - Shininess strength (value 4). In R3 and later this is called Specular Level.
				  - ID_SI - Self-illumination (value 5)
				  - ID_OP - Opacity (value 6)
				  - ID_FI - Filter color (value 7)
				  - ID_BU - Bump (value 8)
				  - ID_RL - Reflection (value 9)
				  - ID_RR - Refraction (value 10)
				  - ID_DP - Displacement (value 11)
			*/

			// û��enable��slot����Ҫ����
			if (gameTexMap &&
				(mapSlot == ID_SS ||	// workaround: IDSS�ڼ��MapEnabled�޷�ͨ��
				maxStdMaterial->MapEnabled(mapSlot)))
			{
				// �õ�Texmap�Ͷ�Ӧ����
				Texmap* texMap = gameTexMap->GetMaxTexmap();
				Class_ID texClassID = texMap->ClassID();
				std::vector<BitmapSlot> szBitmapSlots;

				// �����ͼ����
				if (texClassID == mixClassID)
				{
					Mix* mix = static_cast<Mix*>(texMap);
					Texmap* layerMap0 = mix->GetSubTexmap(0);
					Texmap* layerMap1 = mix->GetSubTexmap(1);
					Texmap* mixMap = mix->GetSubTexmap(2);
					assert(layerMap0 && layerMap1 && mixMap);
					IParamBlock2* paramBlock = mix->GetParamBlock(0);
					if (paramBlock &&
						paramBlock->GetInt(Mix::mix_map1_on) && layerMap0 &&
						paramBlock->GetInt(Mix::mix_map2_on) && layerMap1 &&
						paramBlock->GetInt(Mix::mix_mask_on) && mixMap)
					{
						BitmapTex* bitmapLayer0 = static_cast<BitmapTex*>(layerMap0);
						BitmapTex* bitmapLayer1 = static_cast<BitmapTex*>(layerMap1);
						BitmapTex* bitmapMix = static_cast<BitmapTex*>(mixMap);

						// ���ͼ������ʹ��alphaͨ�����л��
						bool alphaAsMono = bitmapMix->GetAlphaAsMono(TRUE) == TRUE;
						if (alphaAsMono)
						{
							bool sameBitmap = (MSTR(bitmapMix->GetMapName()) == MSTR(bitmapLayer1->GetMapName())) != 0;
							// ������ͼ��ڶ���ͼ��ͬ, �����ǹ�ѡ��alphaAsMono, ��ֻ�����2����ͼ
							if (sameBitmap)
							{
								BitmapSlot mixLayer0Map = { "mixLayer0Map", bitmapLayer0, 
									{ oiram::Material::Op_Source1, oiram::Material::Src_Texture, oiram::Material::Src_Current },
									{ oiram::Material::Op_Default, oiram::Material::Src_Texture, oiram::Material::Src_Current } };
								szBitmapSlots.push_back(mixLayer0Map);

								BitmapSlot mixLayer1Map = { "mixLayer1Map", bitmapLayer1, 
									{ oiram::Material::Op_Blend_Texture_Alpha, oiram::Material::Src_Texture, oiram::Material::Src_Current },
									{ oiram::Material::Op_Default, oiram::Material::Src_Texture, oiram::Material::Src_Current } };
								szBitmapSlots.push_back(mixLayer1Map);

								BitmapSlot mixOperationMap = { "mixOperation", nullptr,
									{ oiram::Material::Op_Modulate, oiram::Material::Src_Current, oiram::Material::Src_Diffuse },
									{ oiram::Material::Op_Modulate, oiram::Material::Src_Current, oiram::Material::Src_Diffuse } };
								szBitmapSlots.push_back(mixOperationMap);
							}
							else
							{
								BitmapSlot mixLayer0Map = { "mixLayer0Map", bitmapLayer0,
									{ oiram::Material::Op_Source1, oiram::Material::Src_Texture, oiram::Material::Src_Current },
									{ oiram::Material::Op_Default, oiram::Material::Src_Texture, oiram::Material::Src_Current } };
								szBitmapSlots.push_back(mixLayer0Map);

								BitmapSlot mixMap = { "mixMap", bitmapMix,
									{ oiram::Material::Op_Blend_Texture_Alpha, oiram::Material::Src_Texture, oiram::Material::Src_Current },
									{ oiram::Material::Op_Source1, oiram::Material::Src_Texture, oiram::Material::Src_Current } };
								szBitmapSlots.push_back(mixMap);

								BitmapSlot mixLayer1Map = { "mixLayer1Map", bitmapLayer1,
									{ oiram::Material::Op_Blend_Current_Alpha, oiram::Material::Src_Texture, oiram::Material::Src_Current },
									{ oiram::Material::Op_Default, oiram::Material::Src_Current, oiram::Material::Src_Current } };
								szBitmapSlots.push_back(mixLayer1Map);

								BitmapSlot mixOperationMap = { "mixOperation", nullptr,
									{ oiram::Material::Op_Modulate, oiram::Material::Src_Current, oiram::Material::Src_Diffuse },
									{ oiram::Material::Op_Modulate, oiram::Material::Src_Current, oiram::Material::Src_Diffuse } };
								szBitmapSlots.push_back(mixOperationMap);
							}
						}
						else
						{
							LogManager::getSingleton().logMessage(false, "warning: mono channel of mix amount map need output as alpha.");
						}
					}
				}
				else
				// NormalMap��ͼ����
				if (texClassID == GNORMAL_CLASS_ID)
				{
					Gnormal* gnormal = static_cast<Gnormal*>(texMap);
					IParamBlock2* paramBlock = gnormal->GetParamBlock(0);
					if (paramBlock && paramBlock->GetInt(Gnormal::gn_map1on))
					{
						Texmap* normalMap = gnormal->GetSubTexmap(0);
						assert(normalMap);
						if (normalMap)
						{
							BitmapTex* bitmapTex = static_cast<BitmapTex*>(normalMap);
							assert(bitmapTex);
							if (bitmapTex)
							{
								BitmapSlot bitmapSlot = { "normalMap", bitmapTex,
									{ oiram::Material::Op_Default, oiram::Material::Src_Current, oiram::Material::Src_Texture },
									{ oiram::Material::Op_Default, oiram::Material::Src_Current, oiram::Material::Src_Texture } };
								szBitmapSlots.push_back(bitmapSlot);
							}
						}
					}
				}
				else
				// ��ͨ��ͼ����
				if (texClassID == Class_ID(BMTEX_CLASS_ID, 0))
				{
					BitmapTex* bitmapTex = static_cast<BitmapTex*>(texMap);
					assert(bitmapTex);
					if (bitmapTex)
					{
						BitmapSlot bitmapSlot = { "diffuseMap", bitmapTex,
							{ oiram::Material::Op_Default, oiram::Material::Src_Current, oiram::Material::Src_Texture },
							{ oiram::Material::Op_Default, oiram::Material::Src_Current, oiram::Material::Src_Texture } };
						szBitmapSlots.push_back(bitmapSlot);
					}
				}

				// ��������λͼ
				for (auto& bitmapSlot : szBitmapSlots)
				{
					// ָ��Ϊnullptr������һ���������������
					if (bitmapSlot.bitmapTex == nullptr)
					{
						std::unique_ptr<oiram::Material::TextureSlot> textureSlot(new oiram::Material::TextureSlot);
						textureSlot->name = bitmapSlot.name;
						textureSlot->texunit = oiram::Material::TU_Operation;
						textureSlot->colourOpEx = bitmapSlot.colourOpEx;
						textureSlot->alphaOpEx = bitmapSlot.alphaOpEx;
						textureSlot->frames.clear();
						textureSlot->frameTime = 1.0f;
						material->textureSlots.push_back(std::move(textureSlot));
					}
					else
					{
						MSTR mstrTexturePath = bitmapSlot.bitmapTex->GetMapName();
						// �Ƿ��������ͼ, ��ͼ�ļ���λ
						if (getMapPath(mstrTexturePath))
						{
							std::unique_ptr<oiram::Material::TextureSlot> textureSlot(new oiram::Material::TextureSlot);

							textureSlot->frameTime = 1.0f;
							// ȡ���������Ϣ
							std::vector<std::string> frames;
							BitmapInfo bi;
							TheManager->GetImageInfo(&bi, mstrTexturePath);

							// ���������֡����
							int idx = mstrTexturePath.first('.');
							MCHAR* ext = nullptr;
							if (idx != -1)
								ext = mstrTexturePath.data() + idx + 1;
							if (bi.NumberFrames() > 0 &&
								ext && _tcscmp(ext, _T("ifl")) == 0)
							{
								textureSlot->frameTime = config.framePerSecond / bitmapSlot.bitmapTex->GetPlaybackRate();

								// �õ�����Ŀ¼
								MSTR path = mstrTexturePath.Substr(0, mstrTexturePath.last(_T('\\')) + 1);
								// ifl�ļ���\nΪ��λ���ָ�֡����
								std::ifstream fs(mstrTexturePath);
								std::string line;
								while (std::getline(fs, line))
								{
									// Ҫ����ҳ���Ҳ��ָ��Ŀ¼������
									MSTR mstrFrame = Ansi2Mstr(line);
									getMapPath(mstrFrame);
									textureSlot->frames.push_back(Mstr2Ansi(mstrFrame));
								}
							}
							else
								textureSlot->frames.push_back(Mstr2Ansi(mstrTexturePath));

							textureSlot->texunit = oiram::Material::TU_Unknow;
							textureSlot->mapChannel = bitmapSlot.bitmapTex->GetMapChannel();
							textureSlot->colourOpEx = bitmapSlot.colourOpEx;
							textureSlot->alphaOpEx = bitmapSlot.alphaOpEx;

							// ��¼uv�任����
							{
								Texmap* texmap = gameTexMap->GetMaxTexmap();

								std::unique_ptr<oiram::Material::UVCoordinate> uvCoordinate(new oiram::Material::UVCoordinate);

								// UV����
								int textureTiling = texmap->GetTextureTiling();
								if (textureTiling & U_MIRROR)
									uvCoordinate->u_mode = oiram::Material::mirror;
								else
								if (textureTiling & U_WRAP)
									uvCoordinate->u_mode = oiram::Material::wrap;
								else
									uvCoordinate->u_mode = oiram::Material::clamp;

								if (textureTiling & V_MIRROR)
									uvCoordinate->v_mode = oiram::Material::mirror;
								else
								if (textureTiling & V_WRAP)
									uvCoordinate->v_mode = oiram::Material::wrap;
								else
									uvCoordinate->v_mode = oiram::Material::clamp;

								// ����֤IGameUVGen::GetUVTransform()��bug, ����������swap(y, z); z = -z;�Ĳ���
								// ��ʵ����uvgen��������max����ϵ����z���Ϻ�y����, �����ǲ���Ҫ�������任��
								// ����ֱ����sdk�е�Texmap::GetUVTransform��������ȷ��
								Matrix3 uvtrans(TRUE);
								texmap->GetUVTransform(uvtrans);
								uvCoordinate->transform = fromGMatrix(GMatrix(uvtrans));
								material->uvCoordinateMap.insert(std::make_pair(textureSlot->mapChannel, std::move(uvCoordinate)));
							}

							int alphaSource = bitmapSlot.bitmapTex->GetAlphaSource();
							switch (mapSlot)
							{
							case ID_DI: // Diffuse Map
								textureSlot->name = bitmapSlot.name;
								textureSlot->texunit = oiram::Material::TU_DiffuseMap;
								material->diffuseColourEnable = false;
								break;

							case ID_SP: // Specular Level Map/Specular Map, ��Ϊspecular = specular map color * specular level, ����һ��ͬ��
							case ID_SS:
								textureSlot->name = "specularMap";
								textureSlot->texunit = oiram::Material::TU_SpecularMap;
								material->specularColourEnable = false;
								break;

							case ID_SI: // Self Illumination Map
								textureSlot->name = "emissiveMap";
								textureSlot->texunit = oiram::Material::TU_EmissiveMap;
								material->emissiveColourEnable = false;
								break;

							case ID_OP: // Opacity Map
								// ͸����ͼֻ������Ⱦ״̬, ���ص�����ͼ(��diffuseMap�ظ���), ��ͼ������Alphaͨ��
								if (alphaSource == ALPHA_FILE)
								{
									// �ƶ�ƽ̨��û��alphaTest, ����ʹ��alpha blendЧ��ҲԶ����discard
									if (config.mobilePlatform)
										material->alphaBlend = true;
									else
									{
										// ����"Ԥ��Alpha"������AlphaBlend����AlphaTest
										if (bitmapSlot.bitmapTex->GetPremultAlpha(TRUE))
										{
											material->alphaTest = true;
										}
										else
										{
											material->alphaBlend = true;
										}
									}
								}
								else if (alphaSource == ALPHA_RGB)
								{
									// û��alphaͨ��, ��Ϊ����RGB��ʽAdd
									material->addBlend = true;
								}
								continue;

							case ID_BU:	// Bump/Normal Map
								textureSlot->name = "normalMap";
								textureSlot->texunit = oiram::Material::TU_NormalMap;
								break;

							case ID_RL:	// Reflection Map
								textureSlot->name = "reflectionMap";
								textureSlot->texunit = oiram::Material::TU_ReflectionMap;
								break;

							default:
								continue;
							}

							bool dumpTextureSlot = true;
							// �̶�����ֻ����diffuseMap
							if (config.renderingType == RT_FixedFunction)
								dumpTextureSlot = mapSlot == ID_DI;
							
							if (dumpTextureSlot)
								material->textureSlots.push_back(std::move(textureSlot));
						}
					}
				}
			}
		}
	}

	return material;
}
