#include "stdafx.h"
#include <max.h>
#include <algorithm>
#include <functional>
#include <iterator>
#include "requisites.h"
#include "serializer.h"
#include "Optimizer.h"

// std::advance����֤iterator�Ƿ�Խ��, ����ʵ��ȷ��iterator��Ч
template <typename container_type>
typename container_type::iterator advanceEx(container_type& container, 
											typename container_type::iterator itor, 
											typename container_type::difference_type offset)
{
	container_type::difference_type	before = -std::distance(container.begin(), itor),
									after = std::distance(itor, container.end());
	offset = std::max(before, std::min(after, offset));
	if (offset != 0)
		std::advance(itor, offset);

	return itor;
}


// ��֡�Ƿ���ͬ
bool KeyFrameEquals(const oiram::KeyFrame& lhs, const oiram::KeyFrame& rhs, float epsilon)
{
	return (lhs.translation.equals(rhs.translation, epsilon) &&
			lhs.scale.equals(rhs.scale, epsilon) &&
			lhs.rotation.equals(rhs.rotation, epsilon));
}

Point3 fromVec3(const oiram::vec3& p){ return Point3(p.x, p.y, p.z); }

// ��ǰ֡�Ƿ����ͨ��ǰ��֡�Ĳ�ֵ����
bool KeyFrameLerpEquals(const oiram::KeyFrame& keyFrame1, const oiram::KeyFrame& keyFrame2, const oiram::KeyFrame& keyFrame3, float epsilon)
{
	// �����2�Źؼ�֡��ʱ�����
	float t = (keyFrame2.keyTime - keyFrame1.keyTime) / (keyFrame3.keyTime - keyFrame1.keyTime);

	// �����ֵ����
	oiram::KeyFrame lerpKeyFrame = {
		Slerp(	Quat(keyFrame1.rotation.x, keyFrame1.rotation.y, keyFrame1.rotation.z, keyFrame1.rotation.w), 
				Quat(keyFrame3.rotation.x, keyFrame3.rotation.y, keyFrame3.rotation.z, keyFrame3.rotation.w), t	),
		fromVec3(keyFrame1.translation) + (fromVec3(keyFrame3.translation) - fromVec3(keyFrame1.translation)) * t,
		fromVec3(keyFrame1.scale) + (fromVec3(keyFrame3.scale) - fromVec3(keyFrame1.scale)) * t };

	// �����2�Źؼ�֡�Ƿ������ǵ�1��3�Źؼ�֡�Ĳ�ֵ����
	return KeyFrameEquals(keyFrame2, lerpKeyFrame, epsilon);
}

// ��Ԫ��������
struct KeyFrameOperator : public std::binary_function<oiram::KeyFrame, oiram::KeyFrame, bool>
{
	static float epsilon;	// ���ȿ���
};
float KeyFrameOperator::epsilon = 1e-6f;

// operator ==
struct KeyFrameEqualTo : public KeyFrameOperator
{
	bool operator ()(const oiram::KeyFrame& lhs, const oiram::KeyFrame& rhs)const
	{
		return KeyFrameEquals(lhs, rhs, epsilon);
	}
};

// operator !=
struct KeyFrameNotEqualTo : public KeyFrameOperator
{
	bool operator ()(const oiram::KeyFrame& lhs, const oiram::KeyFrame& rhs)const
	{
		return !KeyFrameEquals(lhs, rhs, epsilon);
	}
};


oiram::interval Optimizer::
optimizeAnimation(std::vector<oiram::KeyFrame>& keyFrames, bool keepLength)
{
	// ��֡
	const oiram::KeyFrame emptyKeyFrame = { oiram::vec4(0,0,0,1), oiram::vec3(0,0,0), oiram::vec3(1,1,1), 0, 0 };
	// ��ʼ������
	KeyFrameOperator::epsilon = mEpsilon;

	std::vector<oiram::KeyFrame>::iterator beginItor, endItor;
	// ȫ�ǿ�֡
	if (std::find_if(keyFrames.begin(), keyFrames.end(), std::bind1st(KeyFrameNotEqualTo(), emptyKeyFrame)) == keyFrames.end())
	{
		// û�д��ڵı�Ҫ, �ɴ����
		//keyFrames.clear();
	}
	// Ϊȷ������������, ��ʼ�ͽ���֡һ�������, ���Խ�����������������[begin + 1, end - 1]
	else
	{
		// ɾ��β����ͬ֡
		if (!keepLength && keyFrames.size() > 1)
		{
			// ���һ֡
			oiram::KeyFrame lastKeyFrame = keyFrames.back();
			// ���������һ֡��ͬ����ʼ֡
			auto sameItor = std::find_if(keyFrames.rbegin(), keyFrames.rend(), std::bind1st(KeyFrameNotEqualTo(), lastKeyFrame)).base();
			// ��һ֡
			if (sameItor != keyFrames.end())
				++sameItor;
			// ɾ�������
			keyFrames.erase(sameItor, keyFrames.end());
		}

		// ɾ����֡
		if (keyFrames.size() > 2)
		{
			beginItor = advanceEx(keyFrames, keyFrames.begin(), 1);
			endItor = advanceEx(keyFrames, keyFrames.end(), -1);
			keyFrames.erase(std::remove_if(beginItor, endItor, std::bind1st(KeyFrameEqualTo(), emptyKeyFrame)), endItor);
		}

		// ɾ�������ظ�֡
		if (keyFrames.size() > 2)
		{
			beginItor = advanceEx(keyFrames, keyFrames.begin(), 1);
			endItor = advanceEx(keyFrames, keyFrames.end(), -1);
			std::vector<oiram::KeyFrame>::iterator adjacentItor;
			while ((adjacentItor = std::adjacent_find(beginItor, endItor, KeyFrameEqualTo())) != endItor)
			{
				keyFrames.erase(++adjacentItor);
				beginItor = advanceEx(keyFrames, keyFrames.begin(), 1);
				endItor = advanceEx(keyFrames, keyFrames.end(), -1);
			}
		}

		// ɾ������ͨ����ֵ���ɵĹؼ�֡(��Ϊѭ���ڻ�ɾ��֡, ���Ա���ÿ�ζ�ֱ�ӵ���keyFrames.end())
		if (keyFrames.size() > 2)
		{
			for (auto keyFrameItor = advanceEx(keyFrames, keyFrames.begin(), 1); keyFrameItor != advanceEx(keyFrames, keyFrames.end(), -1);)
			{
				// ����ʱ�����ϵ�������3���ؼ�֡
				auto	previousKeyItor = advanceEx(keyFrames, keyFrameItor, -1),
						nextKeyItor = advanceEx(keyFrames, keyFrameItor, 1);

				// ������abcd��֡, ��ǰָ��c, ��c����ͨ��b��d�Ĳ�ֵ����, ��ɾ��c��, erase�Ὣiteratorָ��d
				// ��ʱ��Ҫ��iteratorָ��b, ԭ������Ϊc��ɾ����, ��Ҫ��һ���ж��Ƿ�b����ͨ��a��d��ֵ����
				// ����ʱ���뱣֤ previousKeyItor != keyFrameItor != nextKeyItor
				if (previousKeyItor != keyFrameItor && keyFrameItor != nextKeyItor &&
					KeyFrameLerpEquals(*previousKeyItor, *keyFrameItor, *nextKeyItor, mEpsilon))
					keyFrameItor = advanceEx(keyFrames, keyFrames.erase(keyFrameItor), -1);
				else
					++keyFrameItor;
			}
		}

		// ���ֶ�������ʱ���ܵ������2֡��ȫ��ͬ, ��ʱ����ɾ����ͬ�ĵ�����2֡
		if (keepLength && keyFrames.size() > 2)
		{
			std::vector<oiram::KeyFrame>::iterator tailItor;
			tailItor = advanceEx(keyFrames, keyFrames.end(), -2);
			endItor = advanceEx(keyFrames, keyFrames.end(), -1);
			if (KeyFrameEquals(*tailItor, *endItor, KeyFrameOperator::epsilon))
				keyFrames.erase(tailItor);
		}
	}

	oiram::interval animationRange;
	// ���¶���ʱ��
	if (keyFrames.empty())
		animationRange.SetEmpty();
	else
		animationRange.Set(keyFrames.front().frameTime, keyFrames.back().frameTime);

	return animationRange;
}
