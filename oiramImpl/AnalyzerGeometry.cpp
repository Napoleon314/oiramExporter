#include "stdafx.h"
#include "Analyzer.h"
#include <IGame/IGameModifier.h>
#include <algorithm>
#include <xfunctional>
#include "requisites.h"
#include "scene.h"
#include "strutil.h"
#include "LogManager.h"

Analyzer::
Analyzer()
{
	createDefaultMaterial();
}


Analyzer::
~Analyzer()
{
}


void Analyzer::
processGeometry(IGameNode* gameNode, const SceneNodeMap& sceneNodeMap)
{
	assert(sceneNodeMap.count(gameNode));
	const SceneNode* sceneNode = &sceneNodeMap.find(gameNode)->second;

	LogManager::getSingleton().logMessage(false, "Exporting: %s", sceneNode->name.c_str());
	LogManager::getSingleton().setProgress(0);

	// �������������
	GMatrix nodeInvWorldTM;
	GMatrix localTM = gameNode->GetLocalTM();
	// �ж��Ƿ���Ҫ��ת����
	bool flipNormal = localTM.Parity() == -1;

	IGameObject* gameObject = gameNode->GetIGameObject();
	IGameMesh* gameMesh = static_cast<IGameMesh*>(gameObject);

	// mMesh�ᱻ����ʹ��, ������Ҫ��ÿ�ε���ǰ��ʼ������
	mMesh.initialize();
	// �ڵ�����(max����node����, �����ﱣ֤����Ψһ)
	mMesh.name = sceneNode->meshName;

	mMesh.hasSkeleton = sceneNode->hasSkeleton;
	// ȡ����Ƥ����
	IGameSkin* skin = nullptr;
	if (mMesh.hasSkeleton && gameObject->IsObjectSkinned())
	{
		skin = gameObject->GetIGameSkin();
		skin->GetInitSkinTM(mInitSkinTM);
		
		nodeInvWorldTM = mInitSkinTM.Inverse();

		// �п��ܳ����޸�����ջ����Skin����Physique, ��ȴû����Ƥ�����
		// �ڵ���֮ǰ�Ѿ���֤mMesh.hasSkeleton == true��node��������ʵ�ʰ��˹�����
		int numSkinBones = skin->GetTotalBoneCount();
		assert(numSkinBones > 0);
		if (numSkinBones > 0)
		{
			IGameNode* rootNode = getRootNode(skin->GetIGameBone(0, true));
			assert(rootNode);
			if (rootNode)
			{
				// �õ��Ǽ�����
				if (config.prependRenaming)
					mMesh.skeletonName = config.maxName + '_' + Mchar2Ansi(rootNode->GetName());
				else
					mMesh.skeletonName = Mchar2Ansi(rootNode->GetName());
				strLegalityCheck(mMesh.skeletonName);

				// ���������Ǽ�
				if (config.fullSkeletal)
					dumpSkeletal(rootNode);
			}
		}
	}
	else
	{
		nodeInvWorldTM = gameNode->GetWorldTM().Inverse();
	}

	std::vector<TimeValue> morphKeyFrameTimes;
	// ����ж��㶯��, ��¼�¹ؼ�֡��ʱ��
	if (sceneNode->hasMorph)
		processVertexAnimation(gameNode, sceneNode, morphKeyFrameTimes);

	bool ret = gameMesh->InitializeData();

	// ���meshû��normalʱ����gameMesh->GetNumberOfVerts()�ᱨ��
	bool hasNormal = false;
	Mesh* mesh = gameMesh->GetMaxMesh();
	if (mesh && mesh->normalCount > 0)
		hasNormal = true;

	// ����tangentMapChannel
	int tangentMapChannel = -1;
	if (config.renderingType != RT_FixedFunction)
	{
		for (int mapChannel = -2; mapChannel <= 99; ++mapChannel)
		{
			if (gameMesh->GetNumberOfTangents(mapChannel) > 0)
			{
				tangentMapChannel = mapChannel;
				break;
			}
		}
	}

	// ������������Ϣ
	struct SubMeshDesc
	{
		int									matID;
		oiram::VertexDeclaration			vertexDeclaration;
		std::set<int>						mapChannelSet;
		std::vector<FaceEx*>				faces;
		size_t								numVertices;
		oiram::Geometry						geometry;
	};
	typedef std::unique_ptr<SubMeshDesc> SubMeshDescPtr;

	// material -> subMesh
	std::map<oiram::MaterialPtr, SubMeshDescPtr> subMeshDescMap;
	// ͳ��ʹ�ô������Ķ�������
	std::multiset<oiram::VertexDeclaration> vertexDeclSet;

	// �ܵ�����
	int faceTotalCount = 0;
	// �ռ����в�����Ϣ
	Tab<int> matIDs = gameMesh->GetActiveMatIDs();
	int matIDCount = matIDs.Count();
	for (int matIdx = 0; matIdx < matIDCount; ++matIdx)
	{
		int matID = matIDs[matIdx];
		auto faces = gameMesh->GetFacesFromMatID(matID);
		int numFaces = faces.Count();
		faceTotalCount += numFaces;
		assert(numFaces > 0);

		FaceEx* face = faces[0];
		// faces���еĶ��㶼Ӧ����ͬһ������
		IGameMaterial* gameMaterial = gameMesh->GetMaterialFromFace(face);

		// û�и����ʵ��潫������
		if (gameMaterial == nullptr)
		{
			LogManager::getSingleton().logMessage(true, "Ignore Submesh(matID: %d) with no material.", matID);
			continue;
		}

		// ����Ƕ��ز���
		if (gameMaterial->IsMultiType())
		{
			// ����ǿǲ���
			MCHAR* materialClass = gameMaterial->GetMaterialClass();
			if (_tcscmp(_T("Shell Material"), materialClass) == 0)
			{
				// ����Ӳ��ʳ���1��
				if (gameMaterial->GetSubMaterialCount() > 1)
				{
					// ��һ����original, �ڶ�����baked
					gameMaterial = gameMaterial->GetSubMaterial(0);
					// �ǲ����¿����Ƕ��ز���
					if (gameMaterial->IsMultiType())
					{
						// ���������Ӳ���
						int origSubCount = gameMaterial->GetSubMaterialCount();
						int index = 0;
						for (; index < origSubCount; ++index)
						{
							// matID����ƥ��
							int subMtlID = gameMaterial->GetMaterialID(index);
							if (subMtlID == matID)
								break;
						}
						// ���û���ҵ�, Ĭ��ʹ�õ�0��
						if (index == origSubCount)
							index = 0;

						gameMaterial = gameMaterial->GetSubMaterial(index);
					}
				}
			}
		}

		// ���ҵ���������
		assert(mMaterialMap.count(gameMaterial));
		auto& material = mMaterialMap[gameMaterial];

		// �²��ʲ���Ҫ������������
		auto subMeshDescItor = subMeshDescMap.find(material);
		if (subMeshDescItor == subMeshDescMap.end())
		{
			// ��ǲ�����ʹ��
			material->isUsed = true;

			// ���������Ϣ
			SubMeshDescPtr subMeshDesc(new SubMeshDesc);
			subMeshDesc->matID = matID;
			subMeshDesc->faces.reserve(numFaces);
			std::copy(faces.Addr(0), faces.Addr(0) + numFaces, std::back_inserter(subMeshDesc->faces));
			auto& vertexDeclaration = subMeshDesc->vertexDeclaration;
			auto& mapChannelSet = subMeshDesc->mapChannelSet;

			// oiram::Ves_Position
			vertexDeclaration = oiram::Ves_Position;

			// oiram::Ves_Normal
			if (hasNormal)
				vertexDeclaration |= oiram::Ves_Normal;

			// oiram::Ves_Diffuse
			{
				// ����ɫ�Ƿ����
				Point3 rgb(1.0f, 1.0f, 1.0f);
				if (gameMesh->GetColorVertex(face->color[0], rgb) && !rgb.Equals(Point3(-1.0f, -1.0f, -1.0f)))
					vertexDeclaration |= oiram::Ves_Diffuse;
			}

			// oiram::Ves_Texture_Coordinates
			{
				// ͳ����������ͨ��
				for (auto& textureSlot : material->textureSlots)
				{
					if (textureSlot->texunit != oiram::Material::TU_Operation)
					{
						int mapChannel = textureSlot->mapChannel;
						if (gameMesh->GetMaxMesh()->mapSupport(mapChannel))
							mapChannelSet.insert(mapChannel);
						else
							LogManager::getSingleton().logMessage(true, "Unsupported map channel : (%d).", mapChannel);
					}
				}
				// ��󲻳���8��
				const size_t MaxTexUnits = 8;
				assert(mapChannelSet.size() <= MaxTexUnits);
				size_t maxTexUnitNum = std::min(MaxTexUnits, mapChannelSet.size());
				for (size_t n = 0; n < maxTexUnitNum; ++n)
					vertexDeclaration |= oiram::Ves_Texture_Coordinate0 << n;
			}

			// oiram::Ves_Binormal, oiram::Ves_Tangent
			{
				// �������tangent frame����, ͬʱ��normalmap��ͼ
				if (tangentMapChannel != -1 &&
					gameMesh->GetFaceVertexTangentBinormal(face->meshFaceIndex, 0, tangentMapChannel) != -1)
				{
					bool hasNormalMap = false;
					for (auto& textureSlot : material->textureSlots)
					{
						if (textureSlot->texunit == oiram::Material::TU_NormalMap)
						{
							hasNormalMap = true;
							break;
						}
					}

					if (hasNormalMap)
						vertexDeclaration |= oiram::Ves_Binormal | oiram::Ves_Tangent;
				}
			}

			// oiram::Ves_Blend_Weights, oiram::Ves_Blend_Indices
			{
				// ����Ƿ���ڹ���������
				if (mMesh.hasSkeleton)
					vertexDeclaration |= oiram::Ves_Blend_Weights | oiram::Ves_Blend_Indices;
			}

			// oiram::Ves_VertexAnimationIndex
			{
				if (mMesh.hasMorph)
					vertexDeclaration |= oiram::Ves_VertexAnimationIndex;
			}

			// ��¼
			subMeshDescMap.insert(std::make_pair(material, std::move(subMeshDesc)));
			vertexDeclSet.insert(vertexDeclaration);
		}
		else
		{
			// ��¼��ͬ���ʵ���
			auto& subMeshDesc = subMeshDescItor->second;
			vertexDeclSet.insert(subMeshDesc->vertexDeclaration);
			std::copy(faces.Addr(0), faces.Addr(0) + numFaces, std::back_inserter(subMeshDesc->faces));
		}
	}

	// ��Ч������Ϊ0, ��������������Ч
	assert(faceTotalCount > 0 && !vertexDeclSet.empty());

	// ʹ�ô������Ķ�������
	size_t sharedVertexDeclarationUsed = 0;
	oiram::VertexDeclaration sharedVertexDeclaration = 0;
	// ��Ϊ��multimap, ���������forֻ��������е�key
	for (auto& itor = vertexDeclSet.begin(); itor != vertexDeclSet.end(); itor = vertexDeclSet.upper_bound(*itor))
	{
		auto& vertexDeclaration = *itor;
		size_t vertexDeclUsed = vertexDeclSet.count(vertexDeclaration);

		// ��¼ʹ�ô���������һ����������
		if (vertexDeclUsed > sharedVertexDeclarationUsed)
		{
			sharedVertexDeclarationUsed = vertexDeclUsed;
			sharedVertexDeclaration = vertexDeclaration;
		}
	}

	// ��������������
	assert(sharedVertexDeclaration != 0);
	mMesh.sharedGeometry.reset(new oiram::GeometryData);
	// �����εĶ�������
	size_t sharedVertexCount = 0;

	for (auto& desc : subMeshDescMap)
	{
		auto& material = desc.first;
		auto& subMeshDesc = desc.second;

		// ���㶥������
		size_t numVertices = subMeshDesc->faces.size() * 3;
		subMeshDesc->numVertices = numVertices;

		// ������������, ��¼��������
		auto& geometry = subMeshDesc->geometry;
		if (sharedVertexDeclaration == subMeshDesc->vertexDeclaration)
		{
			geometry = mMesh.sharedGeometry;
			sharedVertexCount += numVertices;
		}
		else
		{
			// Ԥ�����ڴ�
			geometry.reset(new oiram::GeometryData);
			geometry->vertexBuffer.reserve(numVertices);
		}

		geometry->vertexDeclaration = subMeshDesc->vertexDeclaration;
		geometry->material = material;
	}
	// Ԥ�����ڴ�
	if (mMesh.sharedGeometry)
		mMesh.sharedGeometry->vertexBuffer.reserve(sharedVertexCount);

	// ���������δ���ģ�͵Ķ�������
	int faceProgress = 0;
	Interface* coreInterface = GetCOREInterface();
	TimeValue sceneStartTime = GetIGameInterface()->GetSceneStartTime();
	for (auto& desc : subMeshDescMap)
	{
		auto& material = desc.first;
		auto& subMeshDesc = desc.second;

		auto matID = subMeshDesc->matID;
		auto& mapChannelSet = subMeshDesc->mapChannelSet;
		auto& geometry = subMeshDesc->geometry;
		auto& vertexDeclaration = geometry->vertexDeclaration;
		auto& faces = subMeshDesc->faces;
		auto& numVertices = subMeshDesc->numVertices;

		// ��¼��������
		std::string materialName = material->name;
		assert(!materialName.empty());
		// ���ʹ��shading
		if (config.renderingType != RT_FixedFunction)
		{
			std::string extended;
			if (mMesh.hasSkeleton)
				extended = "_skeleton";
			else if (mMesh.hasMorph)
				extended = "_morph";
			else
				extended = "_static";
			material->extended.insert(extended);
			if (!materialName.empty())
				materialName += extended;
		}

		// ��дSubMesh
		std::unique_ptr<oiram::SubMesh> subMesh(new oiram::SubMesh);
		subMesh->materialName = materialName;
		subMesh->matID = matID;
		subMesh->geometry = geometry;

		// ��Ķ�������, ����˫�������С
		size_t numVertex = 3;
		std::vector<oiram::Vertex> vertices(numVertex);
		std::vector<std::vector<oiram::MorphVertex>> morphTrackVertices;
		if (mMesh.hasMorph)
		{
			morphTrackVertices.resize(numVertex);
			// ���ݹؼ�֡���������涯������
			std::for_each(morphTrackVertices.begin(), morphTrackVertices.end(), 
				std::bind2nd(std::mem_fun_ref(&std::vector<oiram::MorphVertex>::resize), morphKeyFrameTimes.size()));
		}

		// ��������
		std::set<unsigned short> boneAssignments;

		// ����Ϊ��λ������
		size_t numFaces = faces.size();
		for (size_t f = 0; f < numFaces; ++f, ++faceProgress)
		{
			// ˢ��һ�ν�����
			LogManager::getSingleton().setProgress(static_cast<int>(static_cast<float>(faceProgress) / faceTotalCount * 100));

			// ����Ϊ��λ��¼����ID
			FaceEx* face = faces[f];
			for (int v = 0; v < 3; ++v)
			{
				auto& vertex = vertices[v];
				int vertexIndex = face->vert[v];

				// ���㶯����ȡ����ʱ��ı䵱ǰ֡ʱ��
				if (mMesh.hasMorph)
					coreInterface->SetTime(sceneStartTime, FALSE);

				// position
				if (vertexDeclaration & oiram::Ves_Position)
				{
					Point3 position = gameMesh->GetVertex(vertexIndex, false);
					position = position * nodeInvWorldTM;
					position *= config.unitMultiplier;

					vertex.vec3Position = position;
					updateVertexBounds(vertex.vec3Position);
				}

				// normal
				if (vertexDeclaration & oiram::Ves_Normal)
				{
					// ���ݶ�������ȡ��ģ�Ϳռ�ķ���(˫��Ķ���ʹ��ͬ���Ķ�������)
					Point3 normal;
					gameMesh->GetNormal(face->norm[v], normal, true);
					// �����Ҫ��ת������ȡ��
					if (flipNormal)
						normal *= -1.0f;

					vertex.vec3Normal = normal;
				}

				// diffuse
				if (vertexDeclaration & oiram::Ves_Diffuse)
				{
					// ����ɫ��BGRת��ΪRGB
					Point3 rgb(1,1,1);
					gameMesh->GetColorVertex(face->color[v], rgb);
					std::swap(rgb.y, rgb.z);
					float a = 1.0;
					gameMesh->GetAlphaVertex(face->alpha[v], a);
					// ��ɫȡ������ʱ�Ǹ���, ����������Է���
					unsigned char argb[4] = {	static_cast<unsigned char>(fabs(a)		* 255),
												static_cast<unsigned char>(fabs(rgb.x)  * 255),
												static_cast<unsigned char>(fabs(rgb.y)  * 255),
												static_cast<unsigned char>(fabs(rgb.z)  * 255)	};
					vertex.diffuse = argb[0] << 24 | argb[1] << 16 | argb[2] << 8 | argb[3];
				}

				if (vertexDeclaration & oiram::Ves_Texture_Coordinate0)
				{
					// ��¼���������UV��Ϣ
					vertex.vec2Texcoords.clear();
					for (auto& mapChannel : mapChannelSet)
					{
						int mapVertexIndex = gameMesh->GetFaceTextureVertex(face->meshFaceIndex, v, mapChannel);
						Point3 mapVertex = gameMesh->GetMapVertex(mapChannel, mapVertexIndex);

						// Programmable pipeline��Ҫ��������С���������궼����ǯ�Ƶ���Ч����
						if (config.renderingType != RT_FixedFunction)
						{
							const float TexcoordRange = 500.0f;
							mapVertex.x = std::min(TexcoordRange, std::max(mapVertex.x, -TexcoordRange));
							mapVertex.y = std::min(TexcoordRange, std::max(mapVertex.y, -TexcoordRange));
							if (oiram::fzero(mapVertex.x))
								mapVertex.x = 0;
							if (oiram::fzero(mapVertex.y))
								mapVertex.y = 0;
						}

						assert(material->uvCoordinateMap.count(mapChannel));
						auto& uvCoordinate = material->uvCoordinateMap[mapChannel];
						GMatrix uvTransform = toGMatrix(uvCoordinate->transform);
						mapVertex = mapVertex * uvTransform;
						// mirror����tex_address_modeҪ����Ϊmirror֮��, map vertexҲҪ����2, ��Ч������maxһ����
						if (uvCoordinate->u_mode == oiram::Material::mirror)
							mapVertex.x *= 2;
						if (uvCoordinate->v_mode == oiram::Material::mirror)
							mapVertex.y *= 2;

						// OGL��UVԭ�������½�
						oiram::vec2 uv(mapVertex.x, 1.0f - mapVertex.y);
						vertex.vec2Texcoords.push_back(uv);
						updateUVBounds(uv);
					}
				}

				if (vertexDeclaration & oiram::Ves_Tangent)
				{
					int tangentIndex = gameMesh->GetFaceVertexTangentBinormal(face->meshFaceIndex, v, tangentMapChannel);
					Point3	tangent = gameMesh->GetTangent(tangentIndex, tangentMapChannel),
							binormal = gameMesh->GetBinormal(tangentIndex, tangentMapChannel),
							normal(vertex.vec3Normal.x, vertex.vec3Normal.y, vertex.vec3Normal.z);

					// Gram-Schmidt orthogonalize
					tangent = (tangent - normal * DotProd(normal, tangent)).Normalize();
					// Calculate handedness
					float handedness = (DotProd(CrossProd(normal, tangent), binormal) < 0.0f) ? -1.0f : 1.0f;

					vertex.vec4Tangent.set(tangent.x, tangent.y, tangent.z, handedness);
					vertex.vec3Binormal.set(binormal.x, binormal.y, binormal.z);
				}

				if (vertexDeclaration & oiram::Ves_Blend_Weights)
				{
					float weight = 0.0f;
					int numBones = skin->GetNumberOfBones(vertexIndex);
					// numBones��ֵ���ܻ���-1 ...
					if (numBones > 0)
					{
						// ��Ƥ����
						struct SkinBone {
							IGameNode*	boneNode;	// �ڵ�
							float		weight;		// Ȩ��
							// ��Ȩ���ɴ�С����
							bool operator < (const SkinBone& rhs)const { return weight > rhs.weight; }
						};

						// ��С��Ȩ�ؽ�������
						const float weightEpsilon = 0.002f;
						// ����ֻ��reserve���ռ�, ��Ϊboneָ�벻һ����Ч, ����bone������Ŀǰ����һ��
						std::vector<SkinBone> skinBones;
						skinBones.reserve(numBones);
						for (int idx = 0; idx < numBones; ++idx)
						{
							IGameNode* boneNode = skin->GetIGameBone(vertexIndex, idx);
							if (boneNode)
							{
								float weight = skin->GetWeight(vertexIndex, idx);
								if (weight > weightEpsilon)
									skinBones.push_back({ boneNode, weight });
							}
						}

						// �ܴ���4������Ӱ��, ֻȡǰ4��Ȩ�����Ĺ���, ����֤Ȩ�غ�Ϊ1
						numBones = static_cast<int>(skinBones.size());
						if (numBones > 4)
						{
							// Ȩ�ذ���������
							std::sort(skinBones.begin(), skinBones.end());
							// ֻ��ǰ4��
							skinBones.resize(4);
							numBones = 4;
						}

						if (numBones > 0)
						{
							// ��Ȩ��ֵ
							float weightTotal = 0.0f;
							for (auto& skinBone : skinBones)
								weightTotal += skinBone.weight;

							if (!oiram::fequal(weightTotal, 1.0f))
							{
								// ��һ��, ��֤Ȩ�غ�Ϊ1
								float weightFactor = 1.0f / weightTotal;
								int idx = 0;
								weightTotal = 0.0f;
								for (; idx < numBones - 1; ++idx)
								{
									float& weight = skinBones[idx].weight;
									weight *= weightFactor;
									weightTotal += weight;
								}
								skinBones[idx].weight = 1.0f - weightTotal;
							}
						}

						// Ȩ�غ�����(����4��������index�������õ���ʱ�ᱻ���Ե���Чֵ0xff, Ȩ��Ĭ��Ϊ1.0)
						Point4 weights(1, 1, 1, 1);
						unsigned long indices = 0xffffffff;
						unsigned char* subIndices = reinterpret_cast<unsigned char*>(&indices);
						int boneIndex = 0;
						for (; boneIndex < numBones; ++boneIndex)
						{
							const SkinBone& bone = skinBones[boneIndex];
							weights[boneIndex] = bone.weight;
							unsigned short boneHandle = dumpBone(bone.boneNode);
							*subIndices++ = static_cast<unsigned char>(boneHandle);
							boneAssignments.insert(boneHandle);
						}

						vertex.vec4BlendWeight = weights;
						vertex.blendIndex = indices;
					}
				}

				if (mMesh.hasMorph)
				{
					// �ռ����йؼ�֡�Ķ���ͷ�����Ϣ
					for (size_t k = 0; k < morphKeyFrameTimes.size(); ++k)
					{
						TimeValue t = morphKeyFrameTimes[k];
						coreInterface->SetTime(t, FALSE);
						auto& morphVertex = morphTrackVertices[v][k];

						// position
						{
							Point3 position = gameMesh->GetVertex(vertexIndex, false);
							position = position * nodeInvWorldTM;
							position *= config.unitMultiplier;

							morphVertex.vec3Position = position;
							updateVertexBounds(morphVertex.vec3Position);
						}

						// normal
						{
							Point3 normal;
							gameMesh->GetNormal(face->norm[v], normal, true);
							// �����Ҫ��ת������ȡ��
							if (flipNormal)
								normal *= -1.0f;

							morphVertex.vec3Normal = normal;
						}
					}
				}
			}

			for (size_t v = 0; v < numVertex; ++v)
			{
				oiram::Vertex& vertex = vertices[v];

				size_t index = geometry->vertexBuffer.size();

				// ��¼�����ؼ�֡��Ϣ
				if (mMesh.hasMorph)
				{
					const auto& morphTrack = morphTrackVertices[v];
					for (size_t k = 0; k < morphKeyFrameTimes.size(); ++k)
					{
						// ���ݹؼ�֡ʱ�䱣�涥�㶯��
						const auto& t = morphKeyFrameTimes[k];
						const auto& morphVertex = morphTrack[k];
						auto& morphKeyFrame = geometry->morphAnimationTrack[t];
						morphKeyFrame.push_back(morphVertex);
					}
					vertex.animationIndex = static_cast<unsigned long>(index);
				}

				// ���ν�������Ϣ����vertexBuffer��
				geometry->vertexBuffer.push_back(vertex);
			}
		}

		// һ��submesh�൱��һ��batch, ���ʹ��GPU skinning, ��ô�������������ܼĴ���������Ӱ��
		// ����sm2.0ֻ��256���Ĵ���, �൱��64��matrix, ����������uniform, ͨ��ֻ��60��matrix�������ٿɹ�ʹ��
		if (mMesh.hasSkeleton)
			LogManager::getSingleton().logMessage(false, "Bone assignments size = %d.", boneAssignments.size());

		// ���ɶ�������
		size_t numIndices = geometry->vertexBuffer.size();
		assert(numIndices > 0);
		subMesh->indexBuffer.use32BitIndices = numIndices > 65535;
		if (subMesh->indexBuffer.use32BitIndices)
			subMesh->indexBuffer.uiIndexBuffer.resize(numVertices);
		else
			subMesh->indexBuffer.usIndexBuffer.resize(numVertices);

		size_t indexStart = numIndices - numVertices;
		for (size_t idx = 0; idx < numVertices; ++idx)
		{
			if (subMesh->indexBuffer.use32BitIndices)
				subMesh->indexBuffer.uiIndexBuffer[idx] = static_cast<unsigned int>(idx + indexStart);
			else
				subMesh->indexBuffer.usIndexBuffer[idx] = static_cast<unsigned short>(idx + indexStart);
		}

		mMesh.subMeshes.push_back(std::move(subMesh));
	}

	gameNode->ReleaseIGameObject();

	LogManager::getSingleton().setProgress(100);
}


void Analyzer::
updateVertexBounds(const oiram::vec3& position)
{
	mMesh.vertexBoundingBox.pmax.x = std::max(mMesh.vertexBoundingBox.pmax.x, position.x);
	mMesh.vertexBoundingBox.pmax.y = std::max(mMesh.vertexBoundingBox.pmax.y, position.y);
	mMesh.vertexBoundingBox.pmax.z = std::max(mMesh.vertexBoundingBox.pmax.z, position.z);

	mMesh.vertexBoundingBox.pmin.x = std::min(mMesh.vertexBoundingBox.pmin.x, position.x);
	mMesh.vertexBoundingBox.pmin.y = std::min(mMesh.vertexBoundingBox.pmin.y, position.y);
	mMesh.vertexBoundingBox.pmin.z = std::min(mMesh.vertexBoundingBox.pmin.z, position.z);
}


void Analyzer::
updateUVBounds(const oiram::vec2& uv)
{
	mMesh.uvBoundingBox.pmax.x = std::max(mMesh.uvBoundingBox.pmax.x, uv.x);
	mMesh.uvBoundingBox.pmax.y = std::max(mMesh.uvBoundingBox.pmax.y, uv.y);

	mMesh.uvBoundingBox.pmin.x = std::min(mMesh.uvBoundingBox.pmin.x, uv.x);
	mMesh.uvBoundingBox.pmin.y = std::min(mMesh.uvBoundingBox.pmin.y, uv.y);
}


void Analyzer::
processVertexAnimation(IGameNode* gameNode, const SceneNode* sceneNode, std::vector<TimeValue>& morphKeyFrameTimes)
{
	// ����ж��㶯��, ��¼�¹ؼ�֡��ʱ��
	IGameControl* gameControl = gameNode->GetIGameControl();
	if (gameControl &&
		gameControl->IsAnimated(IGAME_POINT3))
	{
		IGameKeyTab samples;
		if (gameControl->GetQuickSampledKeys(samples, IGAME_POINT3))
		{
			// ȷ���Ƿ��ж���
			int keyFrameCount = samples.Count();
			if (keyFrameCount > 0)
			{
				mMesh.hasMorph = true;

				// ���ݶ�����Ϣ�����ռ��ؼ�֡��Ϣ
				size_t animationCount = sceneNode->animationDescs.size();
				mMesh.morphAnimations.resize(animationCount);
				for (size_t n = 0; n < animationCount; ++n)
				{
					const auto& animDesc = sceneNode->animationDescs[n];
					TimeValue	timeStart = animDesc.start * config.ticksPerFrame, 
								timeEnd = animDesc.end * config.ticksPerFrame;

					// ��β�ؼ�֡���뱣��
					auto& morphAnimations = mMesh.morphAnimations[n];
					morphAnimations.push_back(timeStart);
					morphKeyFrameTimes.push_back(timeStart);
					for (int i = 0; i < keyFrameCount; ++i)
					{
						TimeValue t = samples[i].t;
						if (t >= timeStart && t <= timeEnd)
						{
							morphAnimations.push_back(t);
							morphKeyFrameTimes.push_back(t);
						}
					}
					morphAnimations.push_back(timeEnd);
					morphKeyFrameTimes.push_back(timeEnd);

					// ȷ���ؼ�֡ʱ��Ψһ
					std::sort(morphAnimations.begin(), morphAnimations.end());
					morphAnimations.erase(std::unique(morphAnimations.begin(), morphAnimations.end()), morphAnimations.end());
				}
				std::sort(morphKeyFrameTimes.begin(), morphKeyFrameTimes.end());
				morphKeyFrameTimes.erase(std::unique(morphKeyFrameTimes.begin(), morphKeyFrameTimes.end()), morphKeyFrameTimes.end());
			}
		}
	}
}


oiram::box3 Analyzer::
fromBox3(Box3 box)
{
	assert(!box.IsEmpty());

	// ��max����ϵת������������ϵ
	std::swap(box.pmin.y, box.pmin.z);
	box.pmin.z = -box.pmin.z;
	std::swap(box.pmax.y, box.pmax.z);
	box.pmax.z = -box.pmax.z;

	// ȡ����box��ʱpmax��С��pmin�ķ���, ����ȷ��pmin�����з�����С��pmax
	if (box.pmin.x > box.pmax.x)
		std::swap(box.pmin.x, box.pmax.x);
	if (box.pmin.y > box.pmax.y)
		std::swap(box.pmin.y, box.pmax.y);
	if (box.pmin.z > box.pmax.z)
		std::swap(box.pmin.z, box.pmax.z);

	return	{ { box.pmin.x, box.pmin.y, box.pmin.z }, { box.pmax.x, box.pmax.y, box.pmax.z } };
}
