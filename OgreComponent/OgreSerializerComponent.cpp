#include "stdafx.h"
#include "OgreSerializerComponent.h"
#include <sstream>
#include <OgreMeshFileFormat.h>
#include <OgreSkeletonFileFormat.h>
#include <OgreHardwareVertexBuffer.h>
#include <OgreRenderOperation.h>
#include "strutil.h"
#include "rc/resource.h"

extern HINSTANCE hInstance;

namespace oiram
{
	using namespace Ogre;

	OgreSerializer::
	OgreSerializer()
	{
	}


	OgreSerializer::
	~OgreSerializer()
	{
	}


	const char* OgreSerializer::
	getName()const
	{
		static char name[] = "OgreComponent";
		return name;
	}


	void OgreSerializer::
	exportMaterial(const oiram::MaterialContainer& materials)
	{
		std::map<std::string, MaterialScript> materialContainer;	// �������� -> �ű�
		for (auto& material : materials)
		{
			if (material->isUsed)
			{
				auto& materialScript = materialContainer[material->rootName];
				auto& script = materialScript.script;
				auto& dependencyMap = materialScript.dependencyMap;

				std::ostringstream scriptStream;
				dumpMaterialScript(*material, dependencyMap, scriptStream);
				script += scriptStream.str();
			}
		}

		// �������ÿ���ű�
		for (auto& material : materialContainer)
		{
			auto& materialName = material.first;
			auto& materialScript = material.second;

			// ���ʽű���Ϊ��
			if (!materialScript.script.empty())
			{
				// �����ű��ļ�
				std::string scriptFile = mConfig.exportPath +materialName + ".material";
				FILE* fp = fopen(scriptFile.c_str(), "wt");
				if (fp)
				{
					// ��дͷ�ļ�����
					std::ostringstream dependencyStream;
					for (auto& dependency : materialScript.dependencyMap)
					{
						for (auto& parent : dependency.second)
							dependencyStream << "import " << parent << " from \"" << dependency.first << ".material\"\n";
					}

					std::string dependencies = dependencyStream.str();
					if (!dependencies.empty())
					{
						fwrite(dependencies.c_str(), dependencies.length(), 1, fp);
						fputc('\n', fp);
					}

					// д��ű�
					fwrite(materialScript.script.c_str(), materialScript.script.length(), 1, fp);
					fclose(fp);
				}
			}
		}
	}


	void OgreSerializer::
	exportMesh(const oiram::Mesh& mesh, const AnimationDesc& animDescs)
	{
		std::string meshFile = mConfig.exportPath + mesh.name + ".mesh";
		FILE* fp = fopen(meshFile.c_str(), "wb");
		if (fp)
		{
			// �������ĳ���
			int streamLen = 0;

			// �ļ�ͷ
			{
				unsigned short head = M_HEADER;
				fwrite(&head, sizeof(head), 1, fp);
				fputs("[MeshSerializer_v1.8]\n", fp);
			}

			// Mesh��Ϣ
			{
				writeChunkHeader(M_MESH, streamLen, fp);
				// �Ƿ����������
				fwrite(&mesh.hasSkeleton, sizeof(mesh.hasSkeleton), 1, fp);

				int meshType = 0;	// static mesh
				if (mesh.hasSkeleton)
					meshType = 1;	// skeleton mesh
				else
					if (mesh.hasMorph)
					meshType = 2;	// morph mesh

				// ����й�����������д��
				if (mesh.sharedGeometry)
				{
					writeGeometry(mesh.sharedGeometry, meshType, fp);
					if (mesh.hasSkeleton)
						writeBoneAssignment(mesh.sharedGeometry, false, fp);
				}

				// ����sub mesh��Ϣ, �Ѹ��ݲ�ͬ���ʻ���
				for (auto& subMesh : mesh.subMeshes)
				{
					auto& geometry = subMesh->geometry;
					std::string materialName = subMesh->materialName;
					// Header
					writeChunkHeader(M_SUBMESH, streamLen, fp);

					// ��������Ҫ��\nΪ����
					if (materialName.empty())
						materialName = "BaseWhite";
					fputs(materialName.c_str(), fp);
					fputc('\n', fp);

					// �Ƿ�����
					bool useSharedVertices = geometry == mesh.sharedGeometry;
					fwrite(&useSharedVertices, sizeof(useSharedVertices), 1, fp);
				
					// ��������������
					unsigned int indexCount = static_cast<unsigned int>(
						subMesh->indexBuffer.use32BitIndices ? subMesh->indexBuffer.uiIndexBuffer.size() : 
															subMesh->indexBuffer.usIndexBuffer.size());
					fwrite(&indexCount, sizeof(indexCount), 1, fp);
					// �Ƿ�ʹ��32λ�Ķ����������ݸ�ʽ
					fwrite(&subMesh->indexBuffer.use32BitIndices, sizeof(subMesh->indexBuffer.use32BitIndices), 1, fp);
					if (subMesh->indexBuffer.use32BitIndices)
						fwrite(subMesh->indexBuffer.uiIndexBuffer.data(), sizeof(unsigned int) * indexCount, 1, fp);
					else
						fwrite(subMesh->indexBuffer.usIndexBuffer.data(), sizeof(unsigned short) * indexCount, 1, fp);
					
					// ��ʹ�ù��������Ҫд�Լ��ļ�������
					if (!useSharedVertices)
						writeGeometry(geometry, meshType, fp);

					// ���Ʒ�ʽ
					writeChunkHeader(M_SUBMESH_OPERATION, streamLen, fp);
					unsigned short opType = RenderOperation::OT_TRIANGLE_LIST;
					fwrite(&opType, sizeof(opType), 1, fp);

					// ����������İ���Ϣ
					if (mesh.hasSkeleton && !useSharedVertices)
						writeBoneAssignment(geometry, true, fp);
				}
			}

			// ��������
			if (mesh.hasSkeleton)
			{
				writeChunkHeader(M_MESH_SKELETON_LINK, streamLen, fp);
				std::string skeletonName = mesh.skeletonName + ".skeleton\n";
				fputs(skeletonName.c_str(), fp);
			}
			/*
			size_t lodCount = mConfig.lodDescs.size();
			if (lodCount > 0)
			{
				writeChunkHeader(M_MESH_LOD, streamLen, fp);
				std::string strategyName = "Distance\n";
				fputs(strategyName.c_str(), fp);
				unsigned short numLODLevels = static_cast<unsigned short>(lodCount + 1);
				fwrite(&numLODLevels, sizeof(numLODLevels), 1, fp);
				bool manual = false;
				fwrite(&manual, sizeof(manual), 1, fp);

				for (size_t n = 0; n < lodCount; ++n)
				{
					writeChunkHeader(M_MESH_LOD_USAGE, streamLen, fp);

					auto& desc = mConfig.lodDescs[n];
					float lodValue = static_cast<float>(desc.value1);
					fwrite(&lodValue, sizeof(lodValue), 1, fp);

					// ����sub mesh��Ϣ
					for (auto& subMesh : mesh.subMeshes)
					{
						writeChunkHeader(M_MESH_LOD_GENERATED, streamLen, fp);
						auto& lod = subMesh->geometry->indexLODs[n];
						unsigned int indexCount = lod.indexCount;
						fwrite(&indexCount, sizeof(indexCount), 1, fp);
						bool indexes32Bit = indexCount > 65535;
						fwrite(&indexes32Bit, sizeof(indexes32Bit), 1, fp);
						fwrite(lod.indexBuffer, (indexes32Bit ? sizeof(unsigned int) : sizeof(unsigned short)) * indexCount, 1, fp);
					}
				}
			}
			*/
			// ���㶯��
			if (mesh.hasMorph)
			{
				// �����ռ����е�geometry
				std::set<oiram::Geometry> geometrySet;
				if (mesh.sharedGeometry)
					geometrySet.insert(mesh.sharedGeometry);

				for (auto& subMesh : mesh.subMeshes)
					geometrySet.insert(subMesh->geometry);

				writeChunkHeader(M_ANIMATIONS, streamLen, fp);
				size_t animationCount = animDescs.size();
				for (size_t anim = 0; anim < animDescs.size(); ++anim)
				{
					auto& animDesc = animDescs[anim];

					writeChunkHeader(M_ANIMATION, streamLen, fp);
					fputs((animDesc.name + '\n').c_str(), fp);
					float length = (animDesc.end - animDesc.start) * mConfig.framePerSecond;
					fwrite(&length, sizeof(length), 1, fp);

					// ����sub mesh��Ϣ
					for (auto& geometry : geometrySet)
					{
						writeChunkHeader(M_ANIMATION_TRACK, streamLen, fp);
						// 1 == morph, 2 == pose
						unsigned short type = 1;
						fwrite(&type, sizeof(type), 1, fp);

						// 0 for shared geometry, 1+ for submesh index + 1
						unsigned short target = 0;
						if (geometry != mesh.sharedGeometry)
						{
							for (auto& subMesh : mesh.subMeshes)
							{
								++target;
								if (geometry == subMesh->geometry)
									break;
							}
						}
						fwrite(&target, sizeof(target), 1, fp);

						for (auto& keyFrameTick : mesh.morphAnimations[anim])
						{
							writeChunkHeader(M_ANIMATION_MORPH_KEYFRAME, streamLen, fp);
							float keyFrameTime = keyFrameTick / mConfig.ticksPerFrame * mConfig.framePerSecond;
							fwrite(&keyFrameTime, sizeof(keyFrameTime), 1, fp);
							bool includesNormals = true;
							fwrite(&includesNormals, sizeof(includesNormals), 1, fp);

							const auto& keyFrame = geometry->morphAnimationTrack[keyFrameTick];
							for (auto& morphVertex : keyFrame)
							{
								if (mConfig.renderingType != RT_FixedFunction)
									fwrite(&morphVertex.short4Position, sizeof(morphVertex.short4Position), 1, fp);
								else
									fwrite(&morphVertex.vec3Position, sizeof(morphVertex.vec3Position), 1, fp);

								if (includesNormals)
								{
									if (mConfig.renderingType != RT_FixedFunction)
										fwrite(&morphVertex.ubyte4Normal, sizeof(morphVertex.ubyte4Normal), 1, fp);
									else
										fwrite(&morphVertex.vec3Normal, sizeof(morphVertex.vec3Normal), 1, fp);
								}
							}
						}
					}
				}
			}

			// ������
			writeChunkHeader(M_MESH_BOUNDS, streamLen, fp);
			fwrite(&mesh.vertexBoundingBox.pmin, sizeof(mesh.vertexBoundingBox.pmin), 1, fp);
			fwrite(&mesh.vertexBoundingBox.pmax, sizeof(mesh.vertexBoundingBox.pmax), 1, fp);
			oiram::vec3 v = mesh.vertexBoundingBox.halfSize();
			float radius = std::sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
			fwrite(&radius, sizeof(radius), 1, fp);

			fclose(fp);
		}
	}


	void OgreSerializer::
	exportSkeleton(const oiram::Skeleton& skeleton, const AnimationDesc& animDescs)
	{
		std::string skeletonFile = mConfig.exportPath + skeleton.skeletonName + ".skeleton";
		FILE* fp = fopen(skeletonFile.c_str(), "wb");
		if (fp)
		{
			// �������ĳ���
			int streamLen = 0;

			unsigned short head = M_HEADER;
			fwrite(&head, sizeof(head), 1, fp);
			fputs("[Serializer_v1.80]\n", fp);

			const oiram::vec3 UNIT_SCALE(1,1,1);
			// �������й�����ʼ��Ϣ
			for (auto& boneElement : skeleton.boneMap)
			{
				auto& bone = boneElement.second;
				// �Ƿ�������
				bool hasScale = !bone->initScale.equals(UNIT_SCALE);
				if (hasScale)
					streamLen = 48; // ��ӦOgre�а������ŵĴ�С
				else
					streamLen = 36; // ��ӦOgre��calcKeyFrameSizeWithoutScale�Ĵ�С

				writeChunkHeader(SKELETON_BONE, streamLen, fp);

				fputs((bone->name + '\n').c_str(), fp);
				fwrite(&bone->handle, sizeof(bone->handle), 1, fp);
				fwrite(&bone->initTranslation, sizeof(bone->initTranslation), 1, fp);
				fwrite(&bone->initRotation, sizeof(bone->initRotation), 1, fp);
				// �����������������д��
				if (hasScale)
					fwrite(&bone->initScale, sizeof(bone->initScale), 1, fp);
			}

			// ���������̳й�ϵ
			streamLen = 0;
			for (auto& boneElement : skeleton.boneMap)
			{
				auto& bone = boneElement.second;

				if (bone->parentHandle != static_cast<unsigned short>(-1))
				{
					writeChunkHeader(SKELETON_BONE_PARENT, streamLen, fp);
					fwrite(&bone->handle, sizeof(bone->handle), 1, fp);
					fwrite(&bone->parentHandle, sizeof(bone->parentHandle), 1, fp);
				}
			}

			// �õ��ù���������֡����Ч��Χ
			oiram::interval range(skeleton.animationRange.Start() / mConfig.ticksPerFrame, skeleton.animationRange.End() / mConfig.ticksPerFrame);
			// �����ؼ�֡
			streamLen = 0;
			size_t animationCount = animDescs.size();
			for (size_t anim = 0; anim < animationCount; ++anim)
			{
				const auto& animDesc = animDescs[anim];

				float	startTime = std::max(range.Start(), animDesc.start) * mConfig.framePerSecond,
						endTime = std::min(range.End(), animDesc.end) * mConfig.framePerSecond;

				writeChunkHeader(SKELETON_ANIMATION, streamLen, fp);
				fputs((animDesc.name + '\n').c_str(), fp);
				float length = endTime - startTime;
				fwrite(&length, sizeof(length), 1, fp);

				for (auto& boneElement : skeleton.boneMap)
				{
					auto& bone = boneElement.second;

					// ������Ϊ��(���ص�ͨ��û�ж���)
					if (!bone->animations.empty())
					{
						// �õ���ǰ����
						auto& animation = bone->animations[anim];
						// �����ؼ�֡��Ϊ��
						if (!animation.keyFrames.empty())
						{
							writeChunkHeader(SKELETON_ANIMATION_TRACK, streamLen, fp);
							fwrite(&bone->handle, sizeof(bone->handle), 1, fp);

							for (auto& frame : animation.keyFrames)
							{
								// �Ƿ�������
								bool hasScale = !frame.scale.equals(UNIT_SCALE);
								if (hasScale)
									streamLen = 48; // ��ӦOgre�а������ŵĴ�С
								else
									streamLen = 36; // ��ӦOgre��calcKeyFrameSizeWithoutScale�Ĵ�С

								writeChunkHeader(SKELETON_ANIMATION_TRACK_KEYFRAME, streamLen, fp);

								fwrite(&frame.keyTime, sizeof(frame.keyTime), 1, fp);
								fwrite(&frame.rotation, sizeof(frame.rotation), 1, fp);
								fwrite(&frame.translation, sizeof(frame.translation), 1, fp);
								// �����������������д��
								if (hasScale)
									fwrite(&frame.scale, sizeof(frame.scale), 1, fp);
							}
						}
					}
				}
			}

			fclose(fp);
		}
	}


	void OgreSerializer::
	exportPackage(const std::string& directory, const std::string& name)
	{
		// ����zip��
		std::string zipFileName = directory + name + ".zip";
		HZIP hz = CreateZip(zipFileName.c_str(), 0);
		ZRESULT result = ZR_OK;

		// ��������Ŀ¼�µ��ļ�
		WIN32_FIND_DATA findFileData = {0};
		std::string findFileName = directory + "*.*";
		HANDLE hFindFile = FindFirstFile(findFileName.c_str(), &findFileData);
		if (hFindFile != INVALID_HANDLE_VALUE)
		{
			do
			{
				// ���ν��ļ�ѹ��������ļ�
				if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					std::string fn = std::string(directory) + findFileData.cFileName;
					result = ZipAdd(hz, findFileData.cFileName, fn.c_str());
				}
			}while (FindNextFile(hFindFile, &findFileData));
			FindClose(hFindFile);
		}
		result = CloseZip(hz) && result;
	}


	void OgreSerializer::
	dumpMaterialScript(const oiram::Material& material, DependencyMap& dependencyMap, std::ostringstream& ss)
	{
		bool fixedFunction = mConfig.renderingType == RT_FixedFunction;
		if (fixedFunction)
		{
			ss << "material " << material.name;

			ss <<	"\n{\n"
					"\ttechnique\n"
					"\t{\n";

			// FFֻ֧��diffuseMap��lightMap
			std::vector<oiram::Material::TextureUnit> supportTexUnits = { oiram::Material::TU_DiffuseMap, oiram::Material::TU_LightMap };
			ss << dumpPass(material, supportTexUnits);

			ss <<	"\t}\n"
					"}\n";
		}
		else
		{
			// Programmable֧��������ͼ
			std::vector<oiram::Material::TextureUnit> supportTexUnits = { 
				oiram::Material::TU_Operation, oiram::Material::TU_DiffuseMap, oiram::Material::TU_LightMap, 
				oiram::Material::TU_NormalMap, oiram::Material::TU_SpecularMap, oiram::Material::TU_ReflectionMap
			};
			std::string genericPass = dumpPass(material, supportTexUnits);

			// ���Է�����ͼ������glow technique��glowPass��
			std::string glowPass;
			for (auto& textureSlot : material.textureSlots)
			{
				if (textureSlot->texunit == oiram::Material::TU_EmissiveMap)
				{
					std::vector<oiram::Material::TextureUnit> supportTexUnits = { oiram::Material::TU_EmissiveMap };
					glowPass = dumpPass(material, supportTexUnits);
					break;
				}
			}

			for (auto& extended : material.extended)
			{
				// �����Ĳ���
				std::string dependency = "generic", parent = "", technique = "generic";
				bool morph = extended == "_morph", skeleton = extended == "_skeleton";
				bool diffuseMap = false, lightMap = false, normalMap = false, specularMap = false, emissiveMap = false;
				for (auto& textureSlot : material.textureSlots)
				{
					switch (textureSlot->texunit)
					{
					default:
					case oiram::Material::TU_DiffuseMap:
						diffuseMap = true;
						break;

					case oiram::Material::TU_LightMap:
						lightMap = true;
						break;

					case oiram::Material::TU_NormalMap:
						normalMap = true;
						break;

					case oiram::Material::TU_SpecularMap:
						specularMap = true;
						break;

					case oiram::Material::TU_EmissiveMap:
						emissiveMap = true;
						break;

					case oiram::Material::TU_ReflectionMap:
						break;
					}
				}

				if (skeleton)
					parent = "generic_skeleton";
				else
				if (morph)
					parent = "generic_morph";
				else
				{
					parent = "generic";
				}

				if (!diffuseMap)
					parent += "_blank";
				else
				{
					if (lightMap)
						parent += "_baked";
					else
					{
						if (normalMap)
							parent += "_normal";

						if (specularMap)
							parent += "_specular";
					}

					if (emissiveMap)
						parent += "_emissive";
				}

				auto& parents = dependencyMap[dependency];
				if (std::find(parents.begin(), parents.end(), parent) == parents.end())
					parents.push_back(parent);

				// �̳в���
				ss << "material " << material.name << extended << " : " << parent << "\n{\n";

				// technique generic
				ss <<	"\ttechnique " << technique << "\n"
						"\t{\n";
				ss <<	genericPass;
				ss <<	"\t}\n";

				// technique glow
				if (!glowPass.empty())
				{
					ss <<	"\ttechnique glow\n"
							"\t{\n";
					ss <<	glowPass;
					ss <<	"\t}\n";
				}

				ss << "}\n";
			}
		}

		ss << '\n';
	}


	std::string OgreSerializer::
	dumpPass(const oiram::Material& material, const std::vector<oiram::Material::TextureUnit>& supportTexUnits)
	{
		std::ostringstream osPass;
		osPass <<	"\t\tpass\n"
					"\t\t{\n";

		if (!material.phongShading)
		{
			osPass <<	"\t\t\tshading flat\n";
		}

		if (material.diffuseColourEnable)
		{
			osPass <<	"\t\t\tdiffuse " << material.diffuseColour.x << ' '
										 << material.diffuseColour.y << ' '
										 << material.diffuseColour.z << ' '
										 << material.diffuseColour.w << '\n';
		}

		if (material.emissiveColourEnable)
		{
			osPass <<	"\t\t\temissive "	<< material.emissiveColour.x << ' '
											<< material.emissiveColour.y << ' '
											<< material.emissiveColour.z << '\n';
		}

		if (material.specularColourEnable)
		{
			osPass <<	"\t\t\tspecular "	<< material.specularColour.x << ' '
											<< material.specularColour.y << ' '
											<< material.specularColour.z << ' '
											<< material.specularColour.w << '\n';
		}

		// ˫��
		if (material.twoSided)
		{
			osPass << 	"\t\t\tcull_hardware none\n"
						"\t\t\tcull_software none\n";
		}

		// ��͸��
		if (material.alphaBlend)
		{
			osPass <<	"\t\t\tlighting off\n"
						"\t\t\tscene_blend alpha_blend\n"
						"\t\t\tdepth_write off\n"
						"\t\t\tfog_override true\n";
		}
		else
		// ����
		if (material.addBlend)
		{
			osPass <<	"\t\t\tlighting off\n"
						"\t\t\tscene_blend add\n"
						"\t\t\tdepth_write off\n"
						"\t\t\tfog_override true\n";
		}
		else
		// �ο�
		if (material.alphaTest)
		{
			osPass <<	"\t\t\talpha_rejection greater 128\n";
		}

		// ֻ����֧�ֵ�����Ԫ
		for (auto& textureSlot : material.textureSlots)
		{
			if (std::find(supportTexUnits.begin(), supportTexUnits.end(), textureSlot->texunit) != supportTexUnits.end())
			{
				// �ǹ̶�����, �����ǲ�������, ����Ҫ����
				if (mConfig.renderingType != RT_FixedFunction &&
					textureSlot->texunit == oiram::Material::TU_Operation)
					break;

				osPass << dumpTextureUnitState(*textureSlot, material.uvCoordinateMap);
			}
		}

		osPass << "\t\t}\n";

		return osPass.str();
	}


	std::string OgreSerializer::
	dumpTextureUnitState(const oiram::Material::TextureSlot& textureSlot, const oiram::Material::UVCoordinateMap& uvCoordinateMap)
	{
		const char* szOperation[] = {	"source1", "source2", "modulate", "modulate_x2", "modulate_x4",
										"add", "add_signed", "add_smooth", "subtract",
										"blend_diffuse_alpha", "blend_texture_alpha", "blend_current_alpha",
										"blend_manual", "dotproduct", "blend_diffuse_colour" };
		const char* szSource[] = { "src_current", "src_texture", "src_diffuse", "src_specular", "src_manual" };

		std::ostringstream osTex;
		osTex << "\n\t\t\ttexture_unit " << textureSlot.name << "\n"
			"\t\t\t{\n";

		if (mConfig.imageGenerationMipmaps)
		{
			if (mConfig.imageTextureFiltering == TF_Trilinear)
				osTex << "\t\t\t\tfiltering trilinear\n";
			else
			if (mConfig.imageTextureFiltering == TF_Anisotropic_8)
			{
				osTex << "\t\t\t\tfiltering anisotropic\n";
				osTex << "\t\t\t\tmax_anisotropy 8\n";
			}
			else
			if (mConfig.imageTextureFiltering == TF_Anisotropic_16)
			{
				osTex << "\t\t\t\tfiltering anisotropic\n";
				osTex << "\t\t\t\tmax_anisotropy 16\n";
			}
		}

		assert(uvCoordinateMap.count(textureSlot.mapChannel));
		auto texcoordItor = uvCoordinateMap.find(textureSlot.mapChannel);
		auto& uvCoordinate = texcoordItor->second;

		// �̶����߲ŵ��������������ת���±�
		if (mConfig.renderingType == RT_FixedFunction)
		{
			if (textureSlot.colourOpEx.operation != oiram::Material::Op_Default)
				osTex << "\t\t\t\tcolour_op_ex " << szOperation[textureSlot.colourOpEx.operation] << ' '
				<< szSource[textureSlot.colourOpEx.source1] << ' '
				<< szSource[textureSlot.colourOpEx.source2] << '\n';

			if (textureSlot.alphaOpEx.operation != oiram::Material::Op_Default)
				osTex << "\t\t\t\talpha_op_ex " << szOperation[textureSlot.alphaOpEx.operation] << ' '
				<< szSource[textureSlot.alphaOpEx.source1] << ' '
				<< szSource[textureSlot.alphaOpEx.source2] << '\n';

			// ��Operation���͵�texture unit����Ҫ����������Ϣ
			if (textureSlot.texunit != oiram::Material::TU_Operation)
			{
				auto texcoord = std::distance(uvCoordinateMap.begin(), texcoordItor);
				if (texcoord != 0)
					osTex << "\t\t\t\ttex_coord_set " << texcoord << '\n';
			}
		}

		// ��Operation���͵�texture unit����Ҫ����������Ϣ
		if (textureSlot.texunit != oiram::Material::TU_Operation)
		{
			const char* szTexAdressMode[] = { "wrap", "clamp", "mirror" };
			if (uvCoordinate->u_mode != oiram::Material::wrap || uvCoordinate->v_mode != oiram::Material::wrap)
				osTex << "\t\t\t\ttex_address_mode " << szTexAdressMode[uvCoordinate->u_mode] << ' ' <<
				szTexAdressMode[uvCoordinate->v_mode] << '\n';
		}

		size_t frameCount = textureSlot.frames.size();
		if (frameCount > 0)
		{
			// ��������֡
			if (frameCount > 1)
			{
				osTex << "\t\t\t\tanim_texture ";
				for (auto& frame : textureSlot.frames)
					osTex << frame << ' ';

				osTex << textureSlot.frameTime * frameCount << '\n';
			}
			else
			{
				auto& frame = textureSlot.frames[0];
				osTex << "\t\t\t\ttexture " << frame;
				// ���ǹ̶�����, ���Ҳ���normalmap, Ĭ�ϼ���gamma correction
				if (mConfig.renderingType != RT_FixedFunction &&
					textureSlot.texunit != oiram::Material::TU_NormalMap)
					osTex << " gamma";
				osTex << '\n';
			}
		}

		osTex << "\t\t\t}\n";

		return osTex.str();
	}


	// д���ݿ�ͷ��Ϣ
	void OgreSerializer::
	writeChunkHeader(unsigned short id, unsigned int size, FILE* fp)
	{
		fwrite(&id, sizeof(id), 1, fp);
		fwrite(&size, sizeof(size), 1, fp);
	}


	const long STREAM_OVERHEAD_SIZE = sizeof(unsigned short) + sizeof(unsigned int);

	void OgreSerializer::
	writeVertexElement(unsigned short source, unsigned short type, unsigned short semantic, unsigned short offset, unsigned short index, FILE* fp)
	{
		unsigned int streamLen = STREAM_OVERHEAD_SIZE + sizeof(unsigned short) * 5;
		writeChunkHeader(M_GEOMETRY_VERTEX_ELEMENT, streamLen, fp);
		fwrite(&source, sizeof(source), 1, fp);
		fwrite(&type, sizeof(type), 1, fp);
		fwrite(&semantic, sizeof(semantic), 1, fp);
		fwrite(&offset, sizeof(offset), 1, fp);
		fwrite(&index, sizeof(index), 1, fp);
	}


	void OgreSerializer::
	writeGeometry(const oiram::Geometry& geometry, int meshType, FILE* fp)
	{
		int streamLen = 0;
		// Header
		writeChunkHeader(M_GEOMETRY, streamLen, fp);
		// �������
		int vertCount = static_cast<int>(geometry->vertexBuffer.size());
		fwrite(&vertCount, sizeof(vertCount), 1, fp);
		// Vertex declaration
		writeChunkHeader(M_GEOMETRY_VERTEX_DECLARATION, streamLen, fp);
		// ����Ԫ��: Position, Normal, Diffuse, TexCoords, Tangent
		unsigned short source = 0, offset = 0;

		bool combineVertexBuffer = true;
		// �̶�������skeleton/morph animation������position/normal������һ��vertex buffer��
		if ((mConfig.renderingType == RT_FixedFunction && meshType != 0) ||
			// morph animation����cpu/gpu���㶼��������һ��vertex buffer��
			meshType == 2)
			combineVertexBuffer = false;

		// ������binormal
		auto vertexDeclaration = geometry->vertexDeclaration;
		vertexDeclaration &= ~oiram::Ves_Binormal;

		// ������������
		{
			if (vertexDeclaration & oiram::Ves_Position)
			{
				if (mConfig.renderingType != RT_FixedFunction)
				{
					writeVertexElement(source, VET_SHORT4, VES_POSITION, offset, 0, fp);
					offset += sizeof(short) * 4;
				}
				else
				{
					writeVertexElement(source, VET_FLOAT3, VES_POSITION, offset, 0, fp);
					offset += sizeof(float) * 3;
				}
			}

			if (vertexDeclaration & oiram::Ves_Normal)
			{
				if (mConfig.renderingType != RT_FixedFunction)
				{
					writeVertexElement(source, VET_UBYTE4, VES_NORMAL, offset, 0, fp);
					offset += sizeof(unsigned char) * 4;
				}
				else
				{
					writeVertexElement(source, VET_FLOAT3, VES_NORMAL, offset, 0, fp);
					offset += sizeof(float) * 3;
				}
			}

			// ������ܺϲ�VB, ��position��normal������һ��vertex buffer��, �����ķ�����һ����
			if (!combineVertexBuffer)
			{
				source = 1;
				offset = 0;
			}

			if (vertexDeclaration & oiram::Ves_Diffuse)
			{
				writeVertexElement(source, VET_COLOUR_ARGB, VES_DIFFUSE, offset, 0, fp);
				offset += sizeof(unsigned long);
			}

			if (vertexDeclaration & oiram::Ves_Texture_Coordinate0)
			{
				if (geometry->material->packedTexcoords)
				{
					writeVertexElement(source, VET_SHORT4, VES_TEXTURE_COORDINATES, offset, 0, fp);
					offset += sizeof(short) * 4;
				}
				else
				{
					for (unsigned int ch = 0; ch < geometry->vertexBuffer[0].vec2Texcoords.size(); ++ch)
					{
						if (mConfig.renderingType != RT_FixedFunction)
						{
							writeVertexElement(source, VET_SHORT2, VES_TEXTURE_COORDINATES, offset, ch, fp);
							offset += sizeof(short) * 2;
						}
						else
						{
							writeVertexElement(source, VET_FLOAT2, VES_TEXTURE_COORDINATES, offset, ch, fp);
							offset += sizeof(float) * 2;
						}
					}
				}
			}

			if (vertexDeclaration & oiram::Ves_Tangent)
			{
				if (mConfig.renderingType != RT_FixedFunction)
				{
					writeVertexElement(source, VET_UBYTE4, VES_TANGENT, offset, 0, fp);
					offset += sizeof(unsigned char) * 4;
				}
				else
				{
					writeVertexElement(source, VET_FLOAT4, VES_TANGENT, offset, 0, fp);
					offset += sizeof(float) * 4;
				}
			}
		}

		// ���Ϊһ��vertex buffer
		if (combineVertexBuffer)
		{
			unsigned short bindIndex = 0;
			// ���㻺��
			writeChunkHeader(M_GEOMETRY_VERTEX_BUFFER, streamLen, fp);
			// ��������
			fwrite(&bindIndex, sizeof(bindIndex), 1, fp);
			// �����С
			fwrite(&offset, sizeof(offset), 1, fp);

			// ��������
			writeChunkHeader(M_GEOMETRY_VERTEX_BUFFER_DATA, streamLen, fp);
			for (auto& vertex : geometry->vertexBuffer)
			{
				if (vertexDeclaration & oiram::Ves_Position)
				{
					if (mConfig.renderingType != RT_FixedFunction)
						fwrite(&vertex.short4Position, sizeof(vertex.short4Position), 1, fp);
					else
						fwrite(&vertex.vec3Position, sizeof(vertex.vec3Position), 1, fp);
				}

				if (vertexDeclaration & oiram::Ves_Normal)
				{
					if (mConfig.renderingType != RT_FixedFunction)
						fwrite(&vertex.ubyte4Normal, sizeof(vertex.ubyte4Normal), 1, fp);
					else
						fwrite(&vertex.vec3Normal, sizeof(vertex.vec3Normal), 1, fp);
				}

				if (vertexDeclaration & oiram::Ves_Diffuse)
					fwrite(&vertex.diffuse, sizeof(vertex.diffuse), 1, fp);

				if (vertexDeclaration & oiram::Ves_Texture_Coordinate0)
				{
					if (geometry->material->packedTexcoords)
					{
						fwrite(&vertex.short4Texcoord, sizeof(vertex.short4Texcoord), 1, fp);
					}
					else
					{
						if (mConfig.renderingType != RT_FixedFunction)
							fwrite(&vertex.short2Texcoords[0], sizeof(vertex.short2Texcoords[0]) * vertex.short2Texcoords.size(), 1, fp);
						else
							fwrite(&vertex.vec2Texcoords[0], sizeof(vertex.vec2Texcoords[0]) * vertex.vec2Texcoords.size(), 1, fp);
					}
				}

				if (vertexDeclaration & oiram::Ves_Tangent)
				{
					if (mConfig.renderingType != RT_FixedFunction)
						fwrite(&vertex.ubyte4Tangent, sizeof(vertex.ubyte4Tangent), 1, fp);
					else
						fwrite(&vertex.vec4Tangent, sizeof(vertex.vec4Tangent), 1, fp);
				}
			}
		}
		else // ���Ϊ����vertex buffer
		{
			unsigned short bindIndex = 0;
			unsigned short vertexSize = 0;
			{
				// ���㻺��
				writeChunkHeader(M_GEOMETRY_VERTEX_BUFFER, streamLen, fp);
				// ��������
				fwrite(&bindIndex, sizeof(bindIndex), 1, fp);
				// �����С = position + normal
				vertexSize = mConfig.renderingType != RT_FixedFunction ? 
								sizeof(short) * 4 + sizeof(unsigned char) * 4 :
								sizeof(float) * 3 + sizeof(float) * 3;
				fwrite(&vertexSize, sizeof(vertexSize), 1, fp);

				// ��������
				writeChunkHeader(M_GEOMETRY_VERTEX_BUFFER_DATA, streamLen, fp);
				for (auto& vertex : geometry->vertexBuffer)
				{
					if (vertexDeclaration & oiram::Ves_Position)
					{
						if (mConfig.renderingType != RT_FixedFunction)
							fwrite(&vertex.short4Position, sizeof(vertex.short4Position), 1, fp);
						else
							fwrite(&vertex.vec3Position, sizeof(vertex.vec3Position), 1, fp);
					}

					if (vertexDeclaration & oiram::Ves_Normal)
					{
						if (mConfig.renderingType != RT_FixedFunction)
							fwrite(&vertex.ubyte4Normal, sizeof(vertex.ubyte4Normal), 1, fp);
						else
							fwrite(&vertex.vec3Normal, sizeof(vertex.vec3Normal), 1, fp);
					}
				}

				// ��������
				if (offset > 0)
				{
					bindIndex = 1;
					vertexSize = offset;

					// ���㻺��
					writeChunkHeader(M_GEOMETRY_VERTEX_BUFFER, streamLen, fp);
					// ��������
					fwrite(&bindIndex, sizeof(bindIndex), 1, fp);
					// �����С
					fwrite(&vertexSize, sizeof(vertexSize), 1, fp);

					// ��������
					writeChunkHeader(M_GEOMETRY_VERTEX_BUFFER_DATA, streamLen, fp);
					for (auto& vertex : geometry->vertexBuffer)
					{
						if (vertexDeclaration & oiram::Ves_Diffuse)
							fwrite(&vertex.diffuse, sizeof(vertex.diffuse), 1, fp);

						if (vertexDeclaration & oiram::Ves_Texture_Coordinate0)
						{
							if (geometry->material->packedTexcoords)
							{
								fwrite(&vertex.short4Texcoord, sizeof(vertex.short4Texcoord), 1, fp);
							}
							else
							{
								if (mConfig.renderingType != RT_FixedFunction)
									fwrite(&vertex.short2Texcoords[0], sizeof(vertex.short2Texcoords[0]) * vertex.short2Texcoords.size(), 1, fp);
								else
									fwrite(&vertex.vec2Texcoords[0], sizeof(vertex.vec2Texcoords[0]) * vertex.vec2Texcoords.size(), 1, fp);
							}
						}

						if (vertexDeclaration & oiram::Ves_Tangent)
						{
							if (mConfig.renderingType != RT_FixedFunction)
								fwrite(&vertex.ubyte4Tangent, sizeof(vertex.ubyte4Tangent), 1, fp);
							else
								fwrite(&vertex.vec4Tangent, sizeof(vertex.vec4Tangent), 1, fp);
						}
					}
				}
			}
		}
	}


	void OgreSerializer::
	writeBoneAssignment(const oiram::Geometry& geometry, bool isSubMesh, FILE* fp)
	{
		int streamLen = 0;
		int vertCount = static_cast<int>(geometry->vertexBuffer.size());
		for (int v = 0; v < vertCount; ++v)
		{
			auto& vertex = geometry->vertexBuffer[v];
			const unsigned char* subIndices = reinterpret_cast<const unsigned char*>(&vertex.blendIndex);
			for (int n = 0; n < 4; ++n)
			{
				if (subIndices[n] == 0xff)
					break;

				writeChunkHeader(isSubMesh ? M_SUBMESH_BONE_ASSIGNMENT : M_MESH_BONE_ASSIGNMENT, streamLen, fp);
				unsigned int vertexIndex = v;
				fwrite(&vertexIndex, sizeof(unsigned int), 1, fp);
				unsigned short boneIndex = subIndices[n];
				fwrite(&boneIndex, sizeof(unsigned short), 1, fp);
				// ogreֻ֧��float4�Ĺ���Ȩ��
				float weight = vertex.vec4BlendWeight[n];
				fwrite(&weight, sizeof(float), 1, fp);
			}
		}
	}
}
