#ifndef _Analyzer_hpp__
#define _Analyzer_hpp__

#include "serializer.h"
#include <IGame/IGameType.h>
#include <box3.h>
class IGameNode;
class IGameMaterial;
struct SceneNode;

class Analyzer
{
public:
	Analyzer();
	~Analyzer();

	// ͳ�Ƽ����ɲ�������
	void processMaterial(IGameMaterial* gameMaterial, const std::string& rootMaterialName);
	// ������������
	void exportTextures();

	// ͳ�Ƽ����ɼ�������
	void processGeometry(IGameNode* gameNode, const SceneNodeMap& sceneNodeMap);
	// ͳ�ƹ��ص�
	void processTag(IGameNode* tagNode);
	// ���������Ϣ
	void processSkeleton(const SceneNodeMap& sceneNodeMap);

	// �õ��ڵ�ĸ��ڵ�
	static IGameNode* getRootNode(IGameNode* gameNode);

public:
	oiram::MaterialMap			mMaterialMap;	// ������Ϣ��
	oiram::MaterialContainer	mMaterials;		// ���в���(Ψһ)
	oiram::Mesh					mMesh;			// ������Ϣ
	oiram::SkeletonMap			mSkeletonMap;	// ������Ϣ��
	GMatrix						mInitSkinTM;


private:
	// �������ԱȽ�
	bool materialComp(const oiram::Material& lhs, const oiram::Material& rhs);
	// ����Ĭ�ϲ���
	void createDefaultMaterial();
	// ��λ��ͼ
	bool getMapPath(MSTR& mapPath);
	// ���Ӳ���
	void addMaterial(IGameMaterial* gameMaterial, const std::shared_ptr<oiram::Material>& oiramMaterial);
	// ����������Ϣ
	std::shared_ptr<oiram::Material> dumpMaterialProperty(IGameMaterial* gameMaterial, const std::string& rootMaterialName);

	// ���¶��������
	void updateVertexBounds(const oiram::vec3& position);
	// �����������������
	void updateUVBounds(const oiram::vec2& uv);
	// �������㶯���ؼ�֡��Ϣ
	void processVertexAnimation(IGameNode* gameNode, const SceneNode* sceneNode, std::vector<TimeValue>& morphKeyFrameTimes);
	// ���������Ǽ�
	void dumpSkeletal(IGameNode* gameNode);
	// ������ع�����Ϣ
	unsigned short dumpBone(IGameNode* pBone, bool isTag=false);
	// ���������
	void checkMirroredMatrix(GMatrix& tm);
	// �õ�ָ��ʱ��֡�ľ���
	GMatrix getNodeTransform(IGameNode* pNode, oiram::Bone& bone, TimeValue t);
	// ȡ���ڵ�����ڸ��ڵ����ת��λ�á����ŵ���Ϣ
	void decomposeNodeMatrix(IGameNode* pBone, IGameNode* rootNode, Point3& nodePos, Point3& nodeScale, Quat& nodeOriet, TimeValue t, bool exportInitMatrix=false, bool isTag=false);

	// Box3 -> oiram:box3
	oiram::box3 fromBox3(Box3 box);
	// oiram::matrix -> GMatrix
	GMatrix toGMatrix(const oiram::matrix& m);
	// GMatrix -> oiram::matrix
	oiram::matrix fromGMatrix(const GMatrix& gm);

	// �Ż�����
	void optimizeMesh();
	// ��������ѹ��
	void vertexCompression(SceneNode* sceneNode);
};

#endif
