#ifndef _Exporter_hpp__
#define _Exporter_hpp__

#include "scene.h"
#include "strutil.h"

class IGameScene;
class IGameNode;
class IGameMesh;
class IGameMaterial;
class Analyzer;

namespace oiram
{
	struct Mesh;
	class Serializer;
}

class Exporter
{
public:
	Exporter(oiram::Serializer* serializer);
	~Exporter();

	// ʵ�ʵ����ӿ�
	bool Export(bool exportSelected);

private:
	SceneNodeType getSceneNodeType(INode* node);
	// �ڵ��Ƿ�����Ƥ
	bool isNodeSkinned(INode* node);
	// �ڵ��Ƿ��ж��㶯��
	bool isNodeMorphed(Animatable* node);
	// �ڵ��Ƿ�����
	bool isNodeHidden(IGameNode* gameNode);
	// �����ӽڵ�
	void dumpNodes(IGameNode* gameNode);
	// �����Զ�������
	void processNodeUserDefinedProperties(IGameNode* gameNode, SceneNode& sceneNode);
	// ����������Ϣ
	void processAnimation(SceneNode& sceneNode);

private:
	SceneNodeMap			mSceneNodeMap;			// ���г����ڵ���Ϣ
	SceneNodeNameSet		mSceneNodeNameSet;		// ���г����ڵ������б�
	SceneNodeInstanceMap	mSceneNodeInstanceMap;	// ���νڵ�����
	Analyzer*				mAnalyzer;				// ������
	oiram::Serializer*		mSerializer;			// ���л�
};

#endif
