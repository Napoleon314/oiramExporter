#ifndef _Scene_Type_hpp__
#define _Scene_Type_hpp__

#include <string>
#include <map>
#include <vector>

class IGameNode;
class Object;

typedef std::multimap<std::string, std::string> UserDefinedPropertyMap;

// ������Ϣ
struct Animation
{
	std::string	name;
	int			start;
	int			end;
};
typedef std::vector<Animation> AnimationDesc;

// �����ڵ�����
enum class SceneNodeType{
	Geometry, Camera, Light, Bone, Helper, Spline, Target, PhysX, Apex, UnSupported
};

// �����ڵ�
struct SceneNode
{
	SceneNodeType			type;					// ����
	std::string				name;					// ����
	SceneNode*				nodeReference;			// ���õĳ����ڵ�
	std::string				meshName;				// mesh����
	std::string				materialFileName;		// �����ļ�����(������ͬmesh��ͬ����ʱ��Ч)
	bool					hasUV;					// �Ƿ���UV
	float					uvCenterU,				// UV0�ķ�Χ
							uvCenterV,
							uvExtentU,
							uvExtentV;
	float					positionCenterX,		// position�ķ�Χ
							positionCenterY,
							positionCenterZ,
							positionExtentX,
							positionExtentY,
							positionExtentZ;
	bool					hasSkeleton;			// �Ƿ�����Ƥ
	bool					hasMorph;				// �Ƿ��б��ζ���
	UserDefinedPropertyMap	userDefinedPropertyMap;	// �û��Զ�������ӳ���
	AnimationDesc			animationDescs;			// ������Ϣ
};

typedef std::map<IGameNode*, SceneNode> SceneNodeMap;
typedef std::set<std::string> SceneNodeNameSet;
typedef std::map<Object*, SceneNode*> SceneNodeInstanceMap;
typedef std::vector<IGameNode*> NodeContainer;

#endif
