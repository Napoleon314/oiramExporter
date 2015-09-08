#include "stdafx.h"
#include "Exporter.h"
#include <modstack.h>
#include <CS/Phyexp.h>
#include <CS/bipexp.h>
#include <iskin.h>
#include <IGame/IGame.h>
#include <tchar.h>
#include "Analyzer.h"
#include "Serializer.h"
#include "LogManager.h"
#include "DotSceneExport.h"
#include "rapidxml.hpp"
#include "rapidxml_print.hpp"
#include "strutil.h"
#include "requisites.h"
#include "Optimizer.h"

Exporter::
Exporter(oiram::Serializer* serializer)
:	mSerializer(serializer), mAnalyzer(new Analyzer)
{
	// ����
	const float maxPrecision = MAX_PRECISION - config.optimizationEpsilon + 1.0f;
	float epsilon = std::min(0.01f, powf(0.1f * maxPrecision, static_cast<float>(config.optimizationEpsilon)));
	Optimizer::getSingleton().setEpsilon(epsilon);
}


Exporter::
~Exporter()
{
	delete mAnalyzer;
}


bool Exporter::
Export(bool exportSelected)
{
	// ��max�ļ�����Ϊ���Ŀ¼, ���Ŀ¼�Ѵ����������׺���ϵ�ǰʱ��
	if (_access(config.exportPath.c_str(), 0) != -1)
	{
		auto oldname = config.exportPath;
		oldname.pop_back();
		time_t tm;
		time(&tm);
		auto newname = oldname + '_' + std::to_string(tm);
		rename(oldname.c_str(), newname.c_str());
	}
	CreateDirectory(Ansi2Mstr(config.exportPath), NULL);

	// ��ʼ��
	IGameScene* gameScene = GetIGameInterface();
	GetConversionManager()->SetCoordSystem(IGameConversionManager::IGAME_OGL);
	bool result = gameScene->InitialiseIGame(exportSelected);
	assert(result);

	// �õ����нڵ�
	int nodeCount = gameScene->GetTopLevelNodeCount();
	for (int nodeIdx = 0; nodeIdx < nodeCount; ++nodeIdx)
	{
		IGameNode* node = gameScene->GetTopLevelNode(nodeIdx);
		dumpNodes(node);
	}

	// ��֤ÿ���������һ����(Ŀ����������Ψһ����ַ���ÿ���������һ����)
	srand(4172485);

	// ���
	UniqueNameGenerator::getSingleton().clear();
	// �Ƿ��Զ����ǰ׺
	if (config.prependRenaming)
		UniqueNameGenerator::getSingleton().setPrependName(config.maxName);

	// �ռ����нڵ����Ϣ
	NodeContainer meshNodes, physxNodes, tagNodes;
	for (auto& sceneNodeItor : mSceneNodeMap)
	{
		IGameNode* gameNode = sceneNodeItor.first;
		SceneNode& sceneNode = sceneNodeItor.second;

		// �Զ�������
		processNodeUserDefinedProperties(gameNode, sceneNode);

		// max����node����, �����ﱣ֤����Ψһ. �Ƿ���ҪΨһ������, Ĭ��Ϊtrue
		auto uniqueItor = sceneNode.userDefinedPropertyMap.find("unique");
		bool useUniqueName = !(uniqueItor != sceneNode.userDefinedPropertyMap.end() && uniqueItor->second == "false");
		std::string gameNodeName = Mchar2Ansi(gameNode->GetName());
		if (useUniqueName)
			gameNodeName = UniqueNameGenerator::getSingleton().generate(gameNodeName, "node", true, true);

		if (mSceneNodeNameSet.count(gameNodeName))
			LogManager::getSingleton().logMessage(true, "Scene node name has been exists: \"%s\".", gameNodeName.c_str());
		else
			mSceneNodeNameSet.insert(gameNodeName);

		// ��ʼ��
		INode* maxNode = gameNode->GetMaxNode();
		sceneNode.name = sceneNode.meshName = gameNodeName;
		sceneNode.nodeReference = &sceneNode;
		sceneNode.hasUV = false;
		sceneNode.hasSkeleton = isNodeSkinned(maxNode);
		sceneNode.hasMorph = isNodeMorphed(maxNode);

		// �ж��Ƿ������õ�����ļ�����
		Object* objectRef = maxNode->GetObjOrWSMRef();
		auto result = mSceneNodeInstanceMap.insert(std::make_pair(objectRef, &sceneNode));
		if (!result.second)
		{
			// �ڵ�����
			sceneNode.nodeReference = result.first->second;

			// ����Ϊ���õļ������mesh����
			sceneNode.meshName = sceneNode.nodeReference->meshName;
			
			// ������õļ�����Ĳ����뵱ǰ�����������ͬ, �򽫲�����������Ա�ע����Ҫ��������
			if (sceneNode.nodeReference->materialFileName == sceneNode.materialFileName)
				sceneNode.materialFileName.clear();
			else
			{
				// �����������
				IGameMaterial* gameMaterial = gameNode->GetNodeMaterial();
				if (gameMaterial)
				{
					sceneNode.materialFileName = Mchar2Ansi(gameMaterial->GetMaterialName());
					strLegalityCheck(sceneNode.materialFileName);
				}
			}
		}

		// ������Ϣ
		processAnimation(sceneNode);

		// �ռ�MESH
		if (sceneNode.type == SceneNodeType::Geometry)
			meshNodes.push_back(gameNode);

		// �ռ�PhysX
		if (sceneNode.type == SceneNodeType::PhysX)
			physxNodes.push_back(gameNode);

		// �ռ�TAG HELPER(��ΪdumpNodes��֤���丸�ڵ�϶��Ѿ��������, �����������д�ڴ�ѭ����)
		if (sceneNode.type == SceneNodeType::Helper)
		{
			IGameNode* rootNode = Analyzer::getRootNode(gameNode);
			if (rootNode)
			{
				// ���ڵ��ǹ���, �����Զ��������б�עtag = true����Ϊ�ǹ��ص�
				assert(mSceneNodeMap.count(gameNode));
				const SceneNode& sceneNode = mSceneNodeMap[gameNode];
				if (sceneNode.type == SceneNodeType::Bone)
				{
					auto tagItor = sceneNode.userDefinedPropertyMap.find("tag");
					if (tagItor != sceneNode.userDefinedPropertyMap.end() &&
						tagItor->second == "true")
					{
						tagNodes.push_back(gameNode);
					}
				}
			}
		}
	}

	// ����material
	LogManager::getSingleton().logMessage(false, "Exporting materials...");
	for (auto& gameNode : meshNodes)
	{
		IGameMaterial* gameRootMaterial = gameNode->GetNodeMaterial();
		if (gameRootMaterial != nullptr)
		{
			auto rootMaterialName = UniqueNameGenerator::getSingleton().generate(Mchar2Ansi(gameRootMaterial->GetMaterialName()), "rootMaterial", true, true);
			mAnalyzer->processMaterial(gameRootMaterial, rootMaterialName);
		}
		else
		{
			LogManager::getSingleton().logMessage(true, "Node material is null: \"%s\" .", gameNode->GetName());
		}
	}

	LogManager::getSingleton().logMessage(false, "---------------------------------------------------------");

	// ����tag
	for (auto& gameNode : tagNodes)
		mAnalyzer->processTag(gameNode);

	// ����mesh
	LogManager::getSingleton().logMessage(false, "Exporting geometries...");

	for (auto& gameNode : meshNodes)
	{
		assert(mSceneNodeMap.count(gameNode));
		SceneNode* sceneNode = &mSceneNodeMap[gameNode];

		// ���õĽڵ���ʵ�ʽڵ��Ƿ���ͬ, ������õ�meshֻ��Ҫ����һ��
		if (sceneNode->nodeReference == sceneNode)
		{
			// ����
			mAnalyzer->processGeometry(gameNode, mSceneNodeMap);

			// �����ϡ�VertexCache����������
			Optimizer::getSingleton().optimizeMesh(mAnalyzer->mMesh);

			// ��������ѹ��
			Optimizer::getSingleton().vertexCompression(mAnalyzer->mMesh, sceneNode);

			// д�����
			mSerializer->exportMesh(mAnalyzer->mMesh, sceneNode->animationDescs);
		}
		else
		{
			// ���Ʒ�Χ��Ϣ
			sceneNode->positionCenterX = sceneNode->nodeReference->positionCenterX;
			sceneNode->positionCenterY = sceneNode->nodeReference->positionCenterY;
			sceneNode->positionCenterZ = sceneNode->nodeReference->positionCenterZ;
			sceneNode->positionExtentX = sceneNode->nodeReference->positionExtentX;
			sceneNode->positionExtentY = sceneNode->nodeReference->positionExtentY;
			sceneNode->positionExtentZ = sceneNode->nodeReference->positionExtentZ;

			sceneNode->hasUV = sceneNode->nodeReference->hasUV;
			sceneNode->uvCenterU = sceneNode->nodeReference->uvCenterU;
			sceneNode->uvCenterV = sceneNode->nodeReference->uvCenterV;
			sceneNode->uvExtentU = sceneNode->nodeReference->uvExtentU;
			sceneNode->uvExtentV = sceneNode->nodeReference->uvExtentV;
		}
	}

	LogManager::getSingleton().logMessage(false, "---------------------------------------------------------");

	// ��������
	LogManager::getSingleton().logMessage(false, "Exporting textures...");
	mAnalyzer->exportTextures();

	// ��������
	mSerializer->exportMaterial(mAnalyzer->mMaterials);

	LogManager::getSingleton().logMessage(false, "---------------------------------------------------------");

	// ����skeleton
	if (!mAnalyzer->mSkeletonMap.empty())
	{
		LogManager::getSingleton().logMessage(false, "Exporting skeletons...");

		mAnalyzer->processSkeleton(mSceneNodeMap);

		// ����skeleton
		for (auto& skeletonElement : mAnalyzer->mSkeletonMap)
		{
			IGameNode* rootNode = static_cast<IGameNode*>(skeletonElement.first);
			auto& skeleton = skeletonElement.second;

			AnimationDesc animDescs;
			auto sceneNodeItor = mSceneNodeMap.find(rootNode);
			if (sceneNodeItor != mSceneNodeMap.end())
				animDescs = sceneNodeItor->second.animationDescs;
			mSerializer->exportSkeleton(*skeleton, animDescs);
		}

		LogManager::getSingleton().logMessage(false, "---------------------------------------------------------");
	}

	if ((config.exportObject & EO_PhysX) &&
		!physxNodes.empty())
	{
		LogManager::getSingleton().logMessage(false, "Exporting PhysX...");

		for (auto& gameNode : physxNodes)
		{
			assert(mSceneNodeMap.count(gameNode));
			SceneNode* sceneNode = &mSceneNodeMap[gameNode];

			// ���physx�Զ�������
			auto physxItor = sceneNode->userDefinedPropertyMap.find("physx");
			if (physxItor != sceneNode->userDefinedPropertyMap.end())
			{
				int physxFlag = std::stoi(physxItor->second);
				Optimizer::getSingleton().dumpPhysXCookMesh(gameNode, sceneNode->name, physxFlag);
			}
		}

		LogManager::getSingleton().logMessage(false, "---------------------------------------------------------");
	}


	// �������.scene�ļ�
	if (config.exportObject & EO_Scene)
	{
		LogManager::getSingleton().logMessage(false, "Exporting scene: %s.scene", config.maxName.c_str());

		auto dotSceneFileName = config.exportPath + config.maxName + ".scene";
		dotSceneExport mDotSceneExport(dotSceneFileName, mSceneNodeMap);

		LogManager::getSingleton().logMessage(false, "---------------------------------------------------------");
	}

	// ���
	if (config.package)
		mSerializer->exportPackage(config.exportPath, config.prependRenaming ? config.maxName + "_" + config.maxName : config.maxName);

	// �ͷ���Դ
	gameScene->ReleaseIGame();

	return true;
}


SceneNodeType Exporter::
getSceneNodeType(INode* node)
{
	auto name = node->GetName();
	ObjectState os = node->EvalWorldState(0);
	if (os.obj)
	{
		SClass_ID scid = os.obj->SuperClassID();
		switch (scid)
		{
		case CAMERA_CLASS_ID:
			return SceneNodeType::Camera;

		case LIGHT_CLASS_ID:
			return SceneNodeType::Light;

		case HELPER_CLASS_ID:
			return SceneNodeType::Helper;

		case SPLINESHAPE_CLASS_ID:
			return SceneNodeType::Spline;

		case GEOMOBJECT_CLASS_ID:
			if (node->IsTarget())
				return SceneNodeType::Target;
			else
			{
				auto classid = os.obj->ClassID();
				if (classid == BONE_OBJ_CLASSID)
					return SceneNodeType::Bone;
				else
				{
					Control* ctrl = node->GetTMController();
					if (ctrl)
					{
						Class_ID cid = ctrl->ClassID();
						if (cid == BIPSLAVE_CONTROL_CLASS_ID ||
							cid == BIPBODY_CONTROL_CLASS_ID)
							return SceneNodeType::Bone;
					}

					if (os.obj->CanConvertToType(Class_ID(TRIOBJ_CLASS_ID, 0)))
					{
						SceneNodeType type = SceneNodeType::UnSupported;
						TriObject* tri = (TriObject *)os.obj->ConvertToType(0, Class_ID(TRIOBJ_CLASS_ID, 0));
						int numFaces = tri->mesh.numFaces;
						// ����û�ж���Ķ���
						if (numFaces > 0)
						{
							// ����Զ����������Ƿ���physx
							MSTR mstr;
							node->GetUserPropBuffer(mstr);
							std::string str = Mstr2Ansi(mstr);
							if (str.find("physx") != std::string::npos)
							{
								type = SceneNodeType::PhysX;
							}
							else
							{
								// �ղ��ʵ����彫����Ϊ��û�������, ���Ҳ��ᱻ����
								Mtl* mtl = node->GetMtl();
								if (mtl)
								{
									Class_ID classID = mtl->ClassID();
									if (classID == Class_ID(DMTL_CLASS_ID, 0))
									{
										type = SceneNodeType::Geometry;
									}
									else
									if (classID == Class_ID(BAKE_SHELL_CLASS_ID, 0))
									{
										type = SceneNodeType::Geometry;
									}
									else
									if (classID == Class_ID(MULTI_CLASS_ID, 0) && mtl->IsMultiMtl())
									{
										std::set<int> matIDs;
										for (int i = 0; i < numFaces; ++i)
											matIDs.insert(tri->mesh.faces[i].getMatID());
										for (auto matID : matIDs)
										{
											Mtl* subMtl = mtl->GetSubMtl(matID);
											if (subMtl)
											{
												type = SceneNodeType::Geometry;
												break;
											}
										}
									}
								}								
							}
						}

						if (tri != os.obj)
							tri->DeleteMe();

						return type;
					}
				}
			}
			break;
		}
	}

	return SceneNodeType::UnSupported;
}
	
	
bool Exporter::
isNodeSkinned(INode* node)
{
	Object *pObj = node->GetObjectRef();
	if (pObj)
	{
		// Is it a derived object?   
		while (pObj->SuperClassID() == GEN_DERIVOB_CLASS_ID)
		{
			// Yes -> Cast
			IDerivedObject *pDerivedObj = static_cast<IDerivedObject*>(pObj);

			// Iterate over all entries of the modifier stack
			int ModStackIndex = 0;
			while (ModStackIndex < pDerivedObj->NumModifiers())
			{
				// Get current modifier
				Modifier* pMod = pDerivedObj->GetModifier(ModStackIndex);

				// ��ʹ�޸�����ջ����Physique����Skin, Ҳ��Ҫ��һ��ȷ������Ƥ��Ϣ
				Class_ID classID = pMod->ClassID();
				if (classID == Class_ID(PHYSIQUE_CLASS_ID_A, PHYSIQUE_CLASS_ID_B))
				{
					bool isSkinned = false;
					IPhysiqueExport* pPhysique = (IPhysiqueExport *)pMod->GetInterface(I_PHYINTERFACE);
					if (pPhysique)
					{
						IPhyContextExport* pPhysiqueContext = (IPhyContextExport *)pPhysique->GetContextInterface(node);
						if (pPhysiqueContext)
						{
							int numVertices = pPhysiqueContext->GetNumberVertices();
							isSkinned = numVertices > 0;

							pPhysique->ReleaseContextInterface(pPhysiqueContext);
						}
						pMod->ReleaseInterface(I_PHYINTERFACE, pPhysique);
					}

					return isSkinned;
				}
				else if (classID == SKIN_CLASSID)
				{
					bool isSkinned = false;
					ISkin* pSkin = (ISkin*)pMod->GetInterface(I_SKIN);
					if (pSkin)
					{
						ISkinContextData* pSkinContext = pSkin->GetContextInterface(node);
						if (pSkinContext)
						{
							int numAssignedBones = pSkinContext->GetNumAssignedBones(0);
							isSkinned = numAssignedBones > 0;
						}
						pMod->ReleaseInterface(I_SKIN, pSkin);
					}

					return isSkinned;
				}

				// Next modifier stack entry
				ModStackIndex++;
			}
			pObj = pDerivedObj->GetObjRef();
		}
	}

	return false;
}


bool Exporter::
isNodeMorphed(Animatable* node)
{
	SClass_ID sid = node->SuperClassID();
	if (node->IsAnimated() && (sid == CTRL_MORPH_CLASS_ID))
		return true;

	return false;
}


bool Exporter::
isNodeHidden(IGameNode* gameNode)
{
	bool isHidden = gameNode->IsNodeHidden();
	// ����ڵ������ص�, ͬʱ���ӽڵ�, ֻҪ�κ�һ���ӽڵ��Ƿ�����״̬, ��ô�ýڵ�ͱ���Ҫ����, ���͸�Ϊhelper
	if (isHidden)
	{
		int count = gameNode->GetChildCount();
		for (int n = 0; n < count; ++n)
		{
			IGameNode* childNode = gameNode->GetNodeChild(n);
			if (!isNodeHidden(childNode))
			{
				isHidden = false;
				break;
			}
		}
	}

	return isHidden;
}


void Exporter::
dumpNodes(IGameNode* gameNode)
{
	// �������صĽڵ�
	bool isHidden = isNodeHidden(gameNode);
	if (!isHidden)
	{
		auto type = getSceneNodeType(gameNode->GetMaxNode());
		bool exportObject = false;
		switch (type)
		{
		case SceneNodeType::Geometry:
			exportObject = (config.exportObject & EO_Geometry) ? true : false;
			break;

		case SceneNodeType::Light:
			exportObject = (config.exportObject & EO_Light) ? true : false;
			break;

		case SceneNodeType::Camera:
			exportObject = (config.exportObject & EO_Camera) ? true : false;
			break;

		case SceneNodeType::Bone:
			exportObject = true;
			break;

		case SceneNodeType::Helper:
			exportObject = (config.exportObject & EO_Helper) ? true : false;
			break;

		case SceneNodeType::Spline:
			exportObject = (config.exportObject & EO_Spline) ? true : false;
			break;

		case SceneNodeType::Target:
			exportObject = (config.exportObject & EO_Target) ? true : false;
			break;

		case SceneNodeType::Apex:
		case SceneNodeType::PhysX:
		case SceneNodeType::UnSupported:
			exportObject = true;
			break;
		}

		// ��������ֱ����Ϊ�����ؽڵ�
		if (exportObject)
		{
			SceneNode& sceneNode = mSceneNodeMap[gameNode];
			sceneNode.type = type;
		}

		// �ݹ鴦�������ӽڵ�
		int count = gameNode->GetChildCount();
		for (int n = 0; n < count; ++n)
		{
			IGameNode* childNode = gameNode->GetNodeChild(n);
			dumpNodes(childNode);
		}
	}
}


void Exporter::
processNodeUserDefinedProperties(IGameNode* gameNode, SceneNode& sceneNode)
{
	// �õ�ȫ�����Զ�������
	MSTR str;
	gameNode->GetMaxNode()->GetUserPropBuffer(str);
	std::string userPropBuffer = Mstr2Ansi(str);
	if (!userPropBuffer.empty())
	{
		// ���н���, ��\nΪ�ָ�����ֳ���������
		std::vector<std::string> properties = str::explode("\n", userPropBuffer);
		for (size_t n = 0; n < properties.size(); ++n)
		{
			// ��=Ϊ�ָ���ȡ��key��value
			std::string& line = properties[n];
			std::vector<std::string> prop = str::explode("=", line);
			if (prop.size() > 1)
			{
				std::string propKey = str::trim(prop[0]);
				std::string propValue = str::trim(prop[1]);
				sceneNode.userDefinedPropertyMap.insert(std::make_pair(propKey, propValue));
			}
		}
	}

// max2011֮��������PhysX, ��PhysX����Ĭ�ϸ�ÿһ���ڵ����"LastPose=undefined"���Զ�������, �����жϲ�����ȥ��
#if defined(MAX_RELEASE_R13_ALPHA) && MAX_RELEASE >= MAX_RELEASE_R13_ALPHA
	auto LastPoseItor = sceneNode.userDefinedPropertyMap.find("LastPose");
	if (LastPoseItor != sceneNode.userDefinedPropertyMap.end() &&
		LastPoseItor->second == "undefined")
	{
		sceneNode.userDefinedPropertyMap.erase(LastPoseItor);
	}
#endif
}


void Exporter::
processAnimation(SceneNode& sceneNode)
{
	auto range = sceneNode.userDefinedPropertyMap.equal_range("anim");
	for (auto itor = range.first; itor != range.second; ++itor)
	{
		std::string str = itor->second;
		auto descs = str::explode(",", str);
		if (descs.size() == 3)
		{
			Animation anim = {
				descs[0],
				std::stoi(descs[1]),
				std::stoi(descs[2])};
			sceneNode.animationDescs.push_back(anim);
		}
	}

	if (sceneNode.animationDescs.empty() &&
		(sceneNode.hasMorph || sceneNode.hasSkeleton))
	{
		Animation anim = {
			"_auto_",
			GetIGameInterface()->GetSceneStartTime() / GetIGameInterface()->GetSceneTicks(),
			GetIGameInterface()->GetSceneEndTime() / GetIGameInterface()->GetSceneTicks() };
		sceneNode.animationDescs.push_back(anim);
	}
}
