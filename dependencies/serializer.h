#ifndef _Serializer_hpp__
#define _Serializer_hpp__

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include "type.h"
#include "requisites.h"
#include "scene.h"
#include <windows.h>

namespace oiram
{
	// ������Ϣ
	struct Material
	{
		// ���㷽��
		enum Operation {	Op_Source1, Op_Source2, Op_Modulate, Op_Modulate_x2, Op_Modulate_x4,
							Op_Add, Op_Add_Signed, Op_Add_Smooth, Op_Substract,
							Op_Blend_Diffuse_Alpha, Op_Blend_Texture_Alpha, Op_Blend_Current_Alpha,
							Op_Manual, Op_Dotproduct, Op_Diffuse_Colour, Op_Default	};

		// ����Դ
		enum Source { Src_Current, Src_Texture, Src_Diffuse, Src_Specular, Src_Manual };

		// ��������
		enum TextureUnit { TU_Unknow, TU_Operation, TU_DiffuseMap, TU_LightMap, TU_NormalMap, TU_SpecularMap, TU_EmissiveMap, TU_ReflectionMap };

		// ��ɫ����
		struct ColourOpEx
		{
			Operation					operation;	// ��Ϸ�ʽ
			Source						source1;	// ����Դ1
			Source						source2;	// ����Դ2
		};

		// Alpha����
		struct AlphaOpEx
		{
			Operation					operation;	// ��Ϸ�ʽ
			Source						source1;	// ����Դ1
			Source						source2;	// ����Դ2
		};

		// ����
		struct TextureSlot
		{
			std::string					name;		// ����
			TextureUnit					texunit;	// ����
			int							mapChannel;	// ͨ��
			ColourOpEx					colourOpEx;	// color��Ϸ�ʽ
			AlphaOpEx					alphaOpEx;	// alpha��Ϸ�ʽ
			std::vector<std::string>	frames;		// ����֡
			float						frameTime;	// ֡��ʱ��(ֻ������֡��Ч)
		};
		typedef std::vector<std::unique_ptr<TextureSlot>> TextureSlotContainer;
		TextureSlotContainer	textureSlots;			// ����

		// ����Ѱַģʽ
		enum TextureAdressMode { wrap, clamp, mirror };
		struct UVCoordinate
		{
			TextureAdressMode	u_mode;		// UѰַģʽ
			TextureAdressMode	v_mode;		// VѰַģʽ
			oiram::matrix		transform;	// �������
		};
		typedef std::map<int, std::unique_ptr<UVCoordinate>> UVCoordinateMap;
		UVCoordinateMap			uvCoordinateMap;		// UVͨ��->����

		std::string				rootName;				// �����ʵ�����
		std::string				name;					// ���ʵ�����

		bool					twoSided;				// ˫��
		bool					phongShading;			// ����ģ��
		bool					alphaBlend;				// ͸��
		bool					alphaTest;				// �ο�
		bool					addBlend;				// ����

		vec4					diffuseColour;			// ��������ɫ
		bool					diffuseColourEnable;	// ��������ɫ����
		vec4					emissiveColour;			// �Է�����ɫ
		bool					emissiveColourEnable;	// �Է�����ɫ����
		vec4					specularColour;			// �߹���ɫ
		bool					specularColourEnable;	// �߹���ɫ����

		bool					packedTexcoords;		// ����������(������diffuseMap+lightMap+vertexCompression)

		// ͬһ�����ʿ��Ա����農̬��̬������, ��ʹ��shaderʱҪ���ݶ�/��������ʹ��ʲô��Ⱦ
		std::set<std::string>	extended;				// ��չ����(����FF������)

		bool					isUsed;					// �Ƿ񱻼�����ʹ�õ���(��ʱmesh��material��multi, ��ʵ��ֻ�õ�����һ��)

		Material() : twoSided(false), phongShading(true), alphaBlend(false), alphaTest(false), addBlend(false),
			diffuseColourEnable(true), emissiveColourEnable(true), specularColourEnable(true), 
			packedTexcoords(false), isUsed(false) {}
	};
	typedef std::shared_ptr<Material> MaterialPtr;
	typedef std::map<void*, MaterialPtr> MaterialMap;
	typedef std::vector<MaterialPtr> MaterialContainer;


	// ����Ԫ��
	enum VertexElementSemantic
	{
		Ves_Position				= 1 <<  1,
		Ves_Normal					= 1 <<  2,
		Ves_Diffuse					= 1 <<  3,
		Ves_Texture_Coordinate0		= 1 <<  4,
		Ves_Texture_Coordinate1		= 1 <<  5,
		Ves_Texture_Coordinate2		= 1 <<  6,
		Ves_Texture_Coordinate3		= 1 <<  7,
		Ves_Texture_Coordinate4		= 1 <<  8,
		Ves_Texture_Coordinate5		= 1 <<  9,
		Ves_Texture_Coordinate6		= 1 << 10,
		Ves_Texture_Coordinate7		= 1 << 11,
        Ves_Binormal				= 1 << 12,
        Ves_Tangent					= 1 << 13,
		Ves_Blend_Weights			= 1 << 14,
        Ves_Blend_Indices			= 1 << 15,

		Ves_VertexAnimationIndex	= 1 << 16,
	};

	// ��������
	struct Vertex
	{
		vec3					vec3Position;		// ����
		short4					short4Position;		// ����short4

		unsigned long			diffuse;			// ��ɫ

		vec3					vec3Normal;			// ����float3
		ubyte4					ubyte4Normal;		// ����unsigned char4

		std::vector<vec2>		vec2Texcoords;		// ��������uv0, uv1, ... float2
		std::vector<short2>		short2Texcoords;	// ��������uv0, uv1, ... short2
		short4					short4Texcoord;		// ��ֻ��diffuseMap��lightMapʱʹ��һ��short4��xyzw����������uv

		vec4					vec4Tangent;		// ����(w������)float4
		ubyte4					ubyte4Tangent;		// ����(w������)unsigned char4

		vec3					vec3Binormal;		// ������float3
		ubyte4					ubyte4Binormal;		// ������unsigned char4

		vec4					vec4BlendWeight;	// ����Ȩ��float4
		ubyte4					ubyte4BlendWeight;	// ����Ȩ��unsigned char4
		unsigned long			blendIndex;			// ��������

		unsigned long			animationIndex;		// ��������
	};

	// ���ζ�������
	struct MorphVertex
	{
		vec3			vec3Position;	// ����float3
		short4			short4Position;	// ����short4

		vec3			vec3Normal;		// ����float3
		ubyte4			ubyte4Normal;	// ����unsigned char4
	};
	// ���ιؼ�֡ʱ��
	typedef int MorphKeyTick;
	// ���ιؼ�֡��������
	typedef std::vector<MorphVertex> MorphKeyFrame;
	// ���ιؼ�֡�켣
	typedef std::map<MorphKeyTick, MorphKeyFrame> MorphAnimationTrack;
	// ���ζ���
	typedef std::vector<std::vector<MorphKeyTick>> MorphAnimations;

	// LOD
	struct LOD
	{
		unsigned int	indexCount;		// ��������������
		void*			indexBuffer;	// ���������Ļ���

		LOD() : indexCount(0), indexBuffer(0) {}
		~LOD() { delete []indexBuffer; }
	};

	// �������� -> ���㻺��
	typedef unsigned int VertexDeclaration;
	typedef std::vector<Vertex> VertexBuffer;
	struct IndexBuffer
	{
		bool						use32BitIndices;		// �Ƿ�Ϊ32bits����
		std::vector<unsigned int>	uiIndexBuffer;			// 32bits��������
		std::vector<unsigned short>	usIndexBuffer;			// 16bits��������

		IndexBuffer() : use32BitIndices(false) {}
	};

	struct GeometryData
	{
		MaterialPtr					material;				// ����

		VertexDeclaration			vertexDeclaration;		// ��������
		VertexBuffer				vertexBuffer;			// ���㻺��
		
		MorphAnimationTrack			morphAnimationTrack;	// ���ζ����켣
		
		std::vector<LOD>			indexLODs;				// ��������LOD����

		GeometryData() : vertexDeclaration(0) {}
	};
	typedef std::shared_ptr<GeometryData> Geometry;

	// ��������
	struct SubMesh
	{
		std::string					materialName;			// ��������
		int							matID;					// ����ID
		Geometry					geometry;				// ��������
		IndexBuffer					indexBuffer;			// ��������

		SubMesh() : matID(-1) {}
	};
	typedef std::vector<std::unique_ptr<SubMesh>> SubMeshContainer;


	struct Mesh
	{
		std::string				name;					// ����
		bool					hasSkeleton;			// �Ƿ��й���������Ϣ
		std::string				skeletonName;			// ��������
		bool					hasMorph;				// �Ƿ��ж��㶯����Ϣ
		box3					vertexBoundingBox;		// ���������
		box2					uvBoundingBox;			// �������������

		Geometry				sharedGeometry;			// ����������
		SubMeshContainer		subMeshes;				// ����������
		MorphAnimations			morphAnimations;		// ���ζ���

		Mesh() { initialize(); }

		void initialize()
		{
			name.clear();
			hasSkeleton = false;
			skeletonName.clear();
			hasMorph = false;
			vertexBoundingBox.Init();
			uvBoundingBox.Init();
			sharedGeometry.reset();
			subMeshes.clear();
			morphAnimations.clear();
		}
	};


	// �����ؼ�֡
	struct KeyFrame
	{
		vec4		rotation;		// ��ת
		vec3		translation;	// λ��
		vec3		scale;			// ����
		float		fov;			// FOV
		int			frameTime;		// ֡ʱ��(tick)
		float		keyTime;		// ֡ʱ��(��)
	};

	// ��������
	struct Animation
	{
		interval				animationRange;	// ����ʱ��
		std::vector<KeyFrame>	keyFrames;		// �ؼ�֡
	};

	// ����
	struct Bone
	{
		bool						isTag;					// �Ƿ�Ϊ���ص�
		bool						nonuniformScaleChecked;	// �ȱ����ż��

		std::string					name;					// ����
		unsigned short				handle;					// ���
		unsigned short				parentHandle;			// ���ڵ���

		// ��ʼ��Ϣ
		vec3						initTranslation;		// λ��
		vec4						initRotation;			// ��ת
		vec3						initScale;				// ����

		matrix						initMatrix;				// ����
		typedef std::map<int, matrix> TransformMap;
		TransformMap				worldTM;				// ʱ��->�任ӳ���

		std::vector<Animation>		animations;			// ����

		Bone() : isTag(false), nonuniformScaleChecked(false), handle(0), parentHandle(0) {}
	};
	typedef std::map<void*, std::unique_ptr<Bone>> BoneMap;

	// �Ǽ�
	struct Skeleton
	{
		std::string					skeletonName;	// ��������
		interval					animationRange;	// ����ʱ��
		BoneMap						boneMap;		// ����ӳ���
		std::vector<void*>			tagArray;		// ���ص�
	};
	typedef std::map<void*, std::unique_ptr<Skeleton>> SkeletonMap;


	// ���л�����
	class Serializer
	{
	public:
		Serializer() {}
		virtual ~Serializer() = 0 {};

		virtual const char* getName()const = 0;
		void setConfig(const Config& config) { mConfig = config; }

		virtual void exportMaterial(const oiram::MaterialContainer& materials) = 0;
		virtual void exportMesh(const oiram::Mesh& mesh, const AnimationDesc& animDescs) = 0;
		virtual void exportSkeleton(const oiram::Skeleton& skeleton, const AnimationDesc& animDescs) = 0;
		virtual void exportPackage(const std::string& directory, const std::string& name) = 0;

	protected:
		Config					mConfig;
	};
}

#endif
