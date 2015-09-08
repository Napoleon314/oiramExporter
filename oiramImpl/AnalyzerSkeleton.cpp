#include "stdafx.h"
#include "Analyzer.h"
#include <IGame\IGame.h>
#include <algorithm>
#include "requisites.h"
#include "LogManager.h"
#include "strutil.h"
#include "Optimizer.h"

void Analyzer::
processTag(IGameNode* tagNode)
{
	// �����ص����ڸ��������ĹǼ���
	IGameNode* rootNode = getRootNode(tagNode);
	auto& skeleton = mSkeletonMap[rootNode];
	skeleton->tagArray.push_back(tagNode);
}


void Analyzer::
processSkeleton(const SceneNodeMap& sceneNodeMap)
{
	for (auto& skeletonItor : mSkeletonMap)
	{
		IGameNode* rootNode = static_cast<IGameNode*>(skeletonItor.first);
		auto& skeleton = skeletonItor.second;

		// ��������ȡ���ڵ������
		if (config.prependRenaming)
			skeleton->skeletonName = config.maxName + '_' + Mchar2Ansi(rootNode->GetName());
		else
			skeleton->skeletonName = Mchar2Ansi(rootNode->GetName());
		strLegalityCheck(skeleton->skeletonName);

		// �������ص�
		for (size_t n = 0; n < skeleton->tagArray.size(); ++n)
			dumpBone(static_cast<IGameNode*>(skeleton->tagArray[n]), true);

		// ���������ʱ�ĳ�ʼ����
		for (auto& boneItor : skeleton->boneMap)
		{
			IGameNode* pBone = static_cast<IGameNode*>(boneItor.first);
			auto& bone = boneItor.second;
			Point3 initTranslation, initScale;
			Quat initRotation;
			decomposeNodeMatrix(pBone, rootNode, initTranslation, initScale, initRotation, 0, true, bone->isTag);
			bone->initTranslation = initTranslation * config.unitMultiplier;
			bone->initScale = initScale;
			bone->initRotation = initRotation;
		}
	}

	// ���������ؼ�����
	for (auto& skeletonItor : mSkeletonMap)
	{
		IGameNode* rootNode = static_cast<IGameNode*>(skeletonItor.first);
		auto& skeleton = skeletonItor.second;

		LogManager::getSingleton().logMessage(false, "Exporting: %s.skeleton", skeleton->skeletonName.c_str());

		auto sceneNodeItor = sceneNodeMap.find(rootNode);
		if (sceneNodeItor != sceneNodeMap.end())
		{
			const SceneNode& sceneNode = sceneNodeItor->second;
			const AnimationDesc& animDescs = sceneNode.animationDescs;

			oiram::BoneMap& boneMap = skeleton->boneMap;
			auto boneBeginItor = boneMap.begin(), boneEndItor = boneMap.end();
			for (auto boneItor = boneBeginItor; boneItor != boneEndItor; ++boneItor)
			{
				LogManager::getSingleton().setProgress(static_cast<int>(100.0f * std::distance(boneMap.begin(), boneItor) / boneMap.size()));

				IGameNode* pBone = static_cast<IGameNode*>(boneItor->first);
				auto& bone = boneItor->second;

				// �����Ƿ��Ѿ������, ���ǲ���Ҫ����
				if (!bone->nonuniformScaleChecked)
				{
					// ��¼����
					size_t animationCount = animDescs.size();
					bone->animations.resize(animationCount);
					for (size_t i = 0; i < animationCount; ++i)
					{
						oiram::Animation& animation = bone->animations[i];
						const auto& animDesc = animDescs[i];
						TimeValue	timeStart = animDesc.start * config.ticksPerFrame,
							timeEnd = animDesc.end * config.ticksPerFrame;
						// ��֡��
						size_t keyCount = (animDesc.end - animDesc.start + 1);
						animation.keyFrames.resize(keyCount);
						for (TimeValue frameTime = timeStart, n = 0; frameTime <= timeEnd; frameTime += config.ticksPerFrame, ++n)
						{
							oiram::KeyFrame& keyFrame = animation.keyFrames[n];
							// �õ�ÿһ֡�����ı任����
							Point3 translation, scale;
							Quat rotation;
							decomposeNodeMatrix(pBone, rootNode, translation, scale, rotation, frameTime, false, bone->isTag);

							// ���������Ϣ
							keyFrame.frameTime = frameTime;
							keyFrame.keyTime = (frameTime - timeStart) / config.ticksPerFrame * config.framePerSecond;
							keyFrame.translation = (translation - Point3(bone->initTranslation.x, bone->initTranslation.y, bone->initTranslation.z)) * config.unitMultiplier;
							keyFrame.rotation = Inverse(Quat(bone->initRotation.x, bone->initRotation.y, bone->initRotation.z, bone->initRotation.w)) * rotation;
							keyFrame.scale = scale / Point3(bone->initScale.x, bone->initScale.y, bone->initScale.z);
						}

						animation.animationRange.Set(animation.keyFrames.front().frameTime, animation.keyFrames.back().frameTime);
					}

					// �Ż���������, ֻ���¹ؼ�֡
					size_t animCount = bone->animations.size();
					for (size_t n = 0; n < animCount; ++n)
					{
						oiram::Animation& animation = bone->animations[n];
						std::vector<oiram::KeyFrame>& keyFrames = animation.keyFrames;
						animation.animationRange = Optimizer::getSingleton().optimizeAnimation(keyFrames);
					}

					// ���õȱ������Ѽ���
					bone->nonuniformScaleChecked = true;

					// ���¹ǼܵĶ���ʱ��
					for (size_t i = 0; i < animationCount; ++i)
					{
						oiram::Animation& animation = bone->animations[i];
						if (!animation.animationRange.Empty())
							skeleton->animationRange.Set(std::min(skeleton->animationRange.Start(), animation.animationRange.Start()),
							std::max(skeleton->animationRange.End(), animation.animationRange.End()));
					}
				}
			}
		}
	}
}


void Analyzer::
dumpSkeletal(IGameNode* gameNode)
{
	dumpBone(gameNode);

	// �ݹ鴦�������ӽڵ�
	int count = gameNode->GetChildCount();
	for (int n = 0; n < count; ++n)
	{
		IGameNode* childNode = gameNode->GetNodeChild(n);
		dumpSkeletal(childNode);
	}
}


unsigned short Analyzer::
dumpBone(IGameNode* pBone, bool isTag)
{
	// ��ʼ����������ʱ��
	IGameNode* rootNode = getRootNode(pBone);
	auto& skeleton = mSkeletonMap[rootNode];
	if (!skeleton)
		skeleton.reset(new oiram::Skeleton);

	auto& bone = skeleton->boneMap[pBone];
	if (!bone)
	{
		bone.reset(new oiram::Bone);
		// ��������Ϣ
		bone->isTag = isTag;
		bone->name = UniqueNameGenerator::getSingleton().generate(Mchar2Ansi(pBone->GetName()), "bone", false, false);
		bone->handle = static_cast<unsigned short>(skeleton->boneMap.size()) - 1;
		bone->parentHandle = -1;
		// ��0֡������Ϣ
		GMatrix initTM = getNodeTransform(pBone, *bone, 0);
		bone->initMatrix = fromGMatrix(initTM); // �ѱ�֤checkMirroredMatrix

		// �ݹ鴦������
		IGameNode* pParentBone = pBone->GetNodeParent();
		if (pParentBone)
			bone->parentHandle = dumpBone(pParentBone);	// ���ڵ���ֱ�Ӹ�ֵ
	}

	return bone->handle;
}


void Analyzer::
checkMirroredMatrix(GMatrix& tm)
{
	// �����п���ʹ�ñ�׼��������������, ����GMatrix�е���ת����ȷ, ͨ����żУ�����ж�
	if (tm.Parity() == -1)
		tm[2] *= -1.0f; // ������תһ��
}


GMatrix Analyzer::
getNodeTransform(IGameNode* pNode, oiram::Bone& bone, TimeValue t)
{
	// ��ΪpNode->GetWorldTM(t)�ǳ���ʱ(������ͨ���������и��ڵ����ó�)
	// ���Ա��洦����ı任����, �Թ�������ֱ�Ӳ���ʹ��
	auto transItor = bone.worldTM.find(t);
	if (transItor == bone.worldTM.end())
	{
		GMatrix tm = pNode->GetWorldTM(t);
		// ����ԭ��
		tm = tm * mInitSkinTM.Inverse();
		checkMirroredMatrix(tm);

		transItor = bone.worldTM.insert(std::make_pair(t, fromGMatrix(tm))).first;
	}

	return toGMatrix(transItor->second);
}


void Analyzer::
decomposeNodeMatrix(IGameNode* pBone, IGameNode* rootNode, Point3& nodePos, Point3& nodeScale, Quat& nodeOriet, TimeValue t, bool exportInitMatrix, bool isTag)
{
	auto& boneMap = mSkeletonMap[rootNode]->boneMap;
	auto& bone = boneMap[pBone];
	GMatrix tm;
	// ��ʼ������Ϣ
	if (exportInitMatrix)
	{
		tm = toGMatrix(bone->initMatrix);

		IGameNode* parentNode = pBone->GetNodeParent();
		if (parentNode)
		{
			// û�취, GMatrix::Inverse()��Ȼ����const��
			GMatrix ptm = toGMatrix(boneMap[parentNode]->initMatrix);
			tm *= ptm.Inverse();
		}
	}
	else
	{
		// ʱ��t����֡�Ĺ�����Ϣ
		tm = getNodeTransform(pBone, *bone, t);

		IGameNode* parentNode = pBone->GetNodeParent();
		if (parentNode)
		{
			auto& parentBone = boneMap[parentNode];
			GMatrix ptm = getNodeTransform(parentNode, *parentBone, t);

			tm *= ptm.Inverse();
		}
	}

	nodePos = tm.Translation();

	// ���ص������ʼ��Ϊ1
	if (isTag)
	{
		nodeScale.Set(1,1,1);
		bone->nonuniformScaleChecked = true;
	}
	else
	{
		nodeScale = tm.Scaling();

		if (!bone->nonuniformScaleChecked)
		{
			// ����0����
			const float epsilon = 1e-5f;
			if (nodeScale.x < epsilon) nodeScale.x = epsilon;
			if (nodeScale.y < epsilon) nodeScale.y = epsilon;
			if (nodeScale.z < epsilon) nodeScale.z = epsilon;

			// ���Nonuniform scale
			const float tolerance = 1e-3f;
			float	xyScale = nodeScale.x / nodeScale.y,
					xzScale = nodeScale.x / nodeScale.z,
					yzScale = nodeScale.y / nodeScale.z;
			if (!oiram::fequal(xyScale, xzScale, tolerance) ||
				!oiram::fequal(xyScale, yzScale, tolerance) ||
				!oiram::fequal(xyScale, yzScale, tolerance))
			{
				bone->nonuniformScaleChecked = true;
				LogManager::getSingleton().logMessage(true, "Nonuniform scale found: \"%s\".", pBone->GetName());
			}
		}
	}

	nodeOriet = tm.Rotation();
	// ��ȡGMatrix��scale,translation��rotationʱʵ�����ǵ���decomp_affine, ��Щ��ûת��
	// ��GMatrix��֮ǰ�Ѿ�ת��������ϵ, ����û��ϵ, ����quaternion������������ϵ, ����wҪȡ��
	nodeOriet.w = -nodeOriet.w;
}


GMatrix Analyzer::
toGMatrix(const oiram::matrix& m)
{
	GMatrix gm;
	memcpy(&(gm[0][0]), &m, sizeof(m));
	return gm;
}


oiram::matrix Analyzer::
fromGMatrix(const GMatrix& gm)
{
	oiram::matrix m;
	memcpy(&m, &(gm[0][0]), sizeof(m));
	return m;
}


IGameNode* Analyzer::
getRootNode(IGameNode* gameNode)
{
	// �ݹ�ֱ�����������ڵ�
	if (gameNode)
	{
		IGameNode* parentNode = gameNode->GetNodeParent();
		if (parentNode == 0)
			return gameNode;
		else
			return getRootNode(parentNode);
	}

	return 0;
}
