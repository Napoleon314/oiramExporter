#ifndef _requisites_hpp__
#define _requisites_hpp__

#include <string>
#include <vector>
#include <io.h>
#include "type.h"

enum ExportObject
{
	EO_Scene	= 1 << 0,
	EO_Geometry = 1 << 1,
	EO_Light	= 1 << 2,
	EO_Camera	= 1 << 3,
	EO_Helper	= 1 << 4,
	EO_Spline	= 1 << 5,
	EO_Target	= 1 << 6,
	EO_PhysX	= 1 << 7,
	EO_APEX		= 1 << 8,

	EO_All		= EO_Scene | EO_Geometry | EO_Light | EO_Camera | EO_Helper | EO_Spline | EO_Target | EO_PhysX | EO_APEX
};

// ����(С�����λ, Ĭ��0.00001)
#define MIN_PRECISION 3
#define MAX_PRECISION 6
#define DEFAULT_PRECISION 5

enum RenderingType
{
	RT_FixedFunction,
	RT_Programmable
};

enum CompressionType
{
	CT_Original,
	CT_PNG,
	CT_TGA,
	CT_DXTC,
	CT_ETC1,
	CT_ETC2,
	CT_PVRTC2_4BPP,
};

enum CompressionQuality
{
	CQ_Low,
	CQ_Normal,
	CQ_High
};

enum TextureFiltering
{
	TF_Bilinear,
	TF_Trilinear,
	TF_Anisotropic_8,
	TF_Anisotropic_16,
};

struct LODesc
{
	int			reduction;
	int			value1;
	std::string	value2;
};

struct Config
{
	int									exportObject;				// ��������
	bool								dotSceneUTF8;				// ��UTF8�������.scene
	bool								prependRenaming;			// ������(��max�ļ�����Ϊǰ׺, �ӵ���scene֮�����е��ļ�����)
	bool								fullSkeletal;				// ����ȫ���Ǽ�
	bool								onlyPhysX;					// ֻ���PhysX����
	RenderingType						renderingType;				// ��Ⱦ��ʽ
	int									skeletonMaxBones;			// ��������(GPU skinning�ܼĴ�������Ӱ��)
	int									optimizationEpsilon;		// �Ż��ľ��ȷ�ֵ(Ӱ��geometry, skeleton, morph, animation node)

	CompressionType						imageCompressionType;		// ͼ��ѹ������
	CompressionQuality					imageCompressionQuality;	// ѹ������
	bool								imageGenerationMipmaps;		// ����mipmaps
	TextureFiltering					imageTextureFiltering;		// ���˷�ʽ
	bool								imagePowerOfTwo;			// ͼ�������2����
	int									imageMaxSize;				// ͼ�����ߴ�
	float								imageScale;					// ͼ������

	std::string							outputFolder;				// ���Ŀ¼
	bool								package;					// �ļ����

	std::vector<LODesc>					lodDescs;					// LOD��Ϣ

	// ��̬��Ϣ��¼
	bool								mobilePlatform;				// �ƶ�ƽ̨
	std::string							maxName;					// �ļ�����
	std::string							exportPath;					// ����·��
	float								unitMultiplier;				// ��λ����(����max�ĵ�λ����)
	int									ticksPerFrame;				// ���/֡
	float								framePerSecond;				// ֡��
};

extern Config config;


inline void strLegalityCheck(std::string& str)
{
	// �Ƿ��ַ��������滻Ϊ�»���
	std::replace_if(str.begin(), str.end(),
		[](std::string::value_type c) { return (c == ' ' || c == '/' || c == '\\' || c == ':' || c == '#' || c == '.'); },
		'_');
}


// ����Ψһ�ڵ�����
class UniqueNameGenerator
{
public:
	static UniqueNameGenerator& getSingleton(){
		static UniqueNameGenerator msSingleton;
		return msSingleton;
	}

	void setPrependName(const std::string& prepend) { mPrependName = prepend; }
	void clear() { mStringPool.clear(); }
	std::string generate(const std::string& str, const std::string& group, bool prependNaming, bool legalityChecking)
	{
		// ������Ƶ���Ч��
		std::string uniqueStr;
		if (prependNaming)
		{
			if (mPrependName.empty())
				uniqueStr = str;
			else
				uniqueStr = mPrependName + '_' + str;
		}
		else
		{
			uniqueStr = str;
		}

		// �ַ����Ϸ��Լ��
		if (legalityChecking)
			strLegalityCheck(uniqueStr);

		// Ϊ�˱���ڵ���������, ���ж��Ƿ���ڸ�����, ������������ƺ����������ʹ֮Ψһ
		while (mStringPool[group].count(uniqueStr))
			uniqueStr += static_cast<wchar_t>(rand() % 10 + '0');
		mStringPool[group].insert(uniqueStr);

		return uniqueStr;
	}

private:
	std::string										mPrependName;
	std::map<std::string, std::set<std::string>>	mStringPool;
};


#ifdef assert
	#undef assert
	#define assert(exp) if(!(exp)){ DebugBreak(); }
#endif

#ifdef _UNICODE
	#if defined(MAX_RELEASE) && MAX_RELEASE > 14000
		inline std::string Mstr2Ansi(const MSTR& str)
		{
			return std::string(str.ToACP());
		}

		inline MSTR FromACP(const char* str)
		{
			return MSTR::FromACP(str);
		}
	#else
		#include "strutil.h"

		inline std::string Mstr2Ansi(const MSTR& mstr)
		{
			return str::to_utf8(mstr.data());
		}
		inline MSTR Ansi2Mstr(const std::string& str)
		{
			return str::to_wcs(str).c_str();
		}
	#endif

	#define ToString(v) std::to_wstring(v)
#else
	#define Mstr2Ansi(str) (str.operator const char *())
	#define Mchar2Ansi(str) (str)
	#define Ansi2Mstr(str) (str.c_str())

	#define ToString(v) std::to_string(v)
#endif

#endif
