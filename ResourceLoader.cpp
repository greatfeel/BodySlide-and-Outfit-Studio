/*
BodySlide and Outfit Studio
Copyright (C) 2016  Caliente & ousnius
See the included LICENSE file
*/

#include "ResourceLoader.h"
#include "GLShader.h"
#include "ConfigurationManager.h"
#include "FSManager.h"
#include "FSEngine.h"
#include "SOIL2.h"

#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/tokenzr.h>

unsigned int ResourceLoader::currentTextureID = 0;

// OpenGL 4.2
PFNGLTEXSTORAGE1DPROC glTexStorage1D = nullptr;
PFNGLTEXSTORAGE2DPROC glTexStorage2D = nullptr;
PFNGLTEXSTORAGE3DPROC glTexStorage3D = nullptr;

// OpenGL 1.3
PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC glCompressedTexSubImage1D = nullptr;
PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glCompressedTexSubImage2D = nullptr;
PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glCompressedTexSubImage3D = nullptr;

// OpenGL 1.2
PFNGLTEXSUBIMAGE3DPROC glTexSubImage3D = nullptr;


ResourceLoader::ResourceLoader() {
}

ResourceLoader::~ResourceLoader() {
}

void ResourceLoader::InitGL() {
	glTexStorage1D = (PFNGLTEXSTORAGE1DPROC)wglGetProcAddress("glTexStorage1D");
	glTexStorage2D = (PFNGLTEXSTORAGE2DPROC)wglGetProcAddress("glTexStorage2D");
	glTexStorage3D = (PFNGLTEXSTORAGE3DPROC)wglGetProcAddress("glTexStorage3D");
	glCompressedTexSubImage1D = (PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC)wglGetProcAddress("glCompressedTexSubImage1D");
	glCompressedTexSubImage2D = (PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC)wglGetProcAddress("glCompressedTexSubImage2D");
	glCompressedTexSubImage3D = (PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC)wglGetProcAddress("glCompressedTexSubImage3D");
	glTexSubImage3D = (PFNGLTEXSUBIMAGE3DPROC)wglGetProcAddress("glTexSubImage3D");

	if (!glTexStorage1D || !glTexStorage2D || !glTexStorage3D || !glTexSubImage3D
		|| !glCompressedTexSubImage1D || !glCompressedTexSubImage2D || !glCompressedTexSubImage3D) {

		gliSupported = false;
		wxLogWarning("OpenGL features required for GLI_create_texture to work aren't there!");
	}

	glInitialized = true;
}

// File extension can be KTX or DDS
GLuint ResourceLoader::GLI_create_texture(gli::texture& texture) {
	if (!glInitialized)
		InitGL();

	if (!gliSupported)
		return 0;

	gli::gl glProfile(gli::gl::PROFILE_GL33);
	gli::gl::format const format = glProfile.translate(texture.format(), texture.swizzles());
	GLenum target = glProfile.translate(texture.target());

	++currentTextureID;
	glGenTextures(1, &currentTextureID);
	glBindTexture(target, currentTextureID);
	glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(texture.levels() - 1));
	glTexParameteri(target, GL_TEXTURE_SWIZZLE_R, format.Swizzles[0]);
	glTexParameteri(target, GL_TEXTURE_SWIZZLE_G, format.Swizzles[1]);
	glTexParameteri(target, GL_TEXTURE_SWIZZLE_B, format.Swizzles[2]);
	glTexParameteri(target, GL_TEXTURE_SWIZZLE_A, format.Swizzles[3]);

	glm::tvec3<GLsizei> const textureExtent(texture.extent());
	GLsizei const faceTotal = static_cast<GLsizei>(texture.layers() * texture.faces());

	switch (texture.target()) {
	case gli::TARGET_1D:
		glTexStorage1D(
			target, static_cast<GLint>(texture.levels()), format.Internal, textureExtent.x);
		break;

	case gli::TARGET_1D_ARRAY:
	case gli::TARGET_2D:
	case gli::TARGET_CUBE:
		glTexStorage2D(
			target, static_cast<GLint>(texture.levels()), format.Internal,
			textureExtent.x, texture.target() == gli::TARGET_2D ? textureExtent.y : faceTotal);
		break;

	case gli::TARGET_2D_ARRAY:
	case gli::TARGET_3D:
	case gli::TARGET_CUBE_ARRAY:
		glTexStorage3D(
			target, static_cast<GLint>(texture.levels()), format.Internal,
			textureExtent.x, textureExtent.y,
			texture.target() == gli::TARGET_3D ? textureExtent.z : faceTotal);
		break;

	default:
		assert(0);
		break;
	}

	for (size_t layer = 0; layer < texture.layers(); ++layer) {
		for (size_t face = 0; face < texture.faces(); ++face) {
			for (size_t level = 0; level < texture.levels(); ++level) {
				GLsizei const layerGL = static_cast<GLsizei>(layer);
				glm::tvec3<GLsizei> textureExtent(texture.extent(level));
				target = gli::is_target_cube(texture.target())
					? static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face)
					: target;

				switch (texture.target()) {
				case gli::TARGET_1D:
					if (gli::is_compressed(texture.format()))
						glCompressedTexSubImage1D(
						target, static_cast<GLint>(level), 0, textureExtent.x,
						format.Internal, static_cast<GLsizei>(texture.size(level)),
						texture.data(layer, face, level));
					else
						glTexSubImage1D(
						target, static_cast<GLint>(level), 0, textureExtent.x,
						format.External, format.Type,
						texture.data(layer, face, level));
					break;

				case gli::TARGET_1D_ARRAY:
				case gli::TARGET_2D:
				case gli::TARGET_CUBE:
					if (gli::is_compressed(texture.format()))
						glCompressedTexSubImage2D(
						target, static_cast<GLint>(level),
						0, 0,
						textureExtent.x,
						texture.target() == gli::TARGET_1D_ARRAY ? layerGL : textureExtent.y,
						format.Internal, static_cast<GLsizei>(texture.size(level)),
						texture.data(layer, face, level));
					else
						glTexSubImage2D(
						target, static_cast<GLint>(level),
						0, 0,
						textureExtent.x,
						texture.target() == gli::TARGET_1D_ARRAY ? layerGL : textureExtent.y,
						format.External, format.Type,
						texture.data(layer, face, level));
					break;

				case gli::TARGET_2D_ARRAY:
				case gli::TARGET_3D:
				case gli::TARGET_CUBE_ARRAY:
					if (gli::is_compressed(texture.format()))
						glCompressedTexSubImage3D(
						target, static_cast<GLint>(level),
						0, 0, 0,
						textureExtent.x, textureExtent.y,
						texture.target() == gli::TARGET_3D ? textureExtent.z : layerGL,
						format.Internal, static_cast<GLsizei>(texture.size(level)),
						texture.data(layer, face, level));
					else
						glTexSubImage3D(
						target, static_cast<GLint>(level),
						0, 0, 0,
						textureExtent.x, textureExtent.y,
						texture.target() == gli::TARGET_3D ? textureExtent.z : layerGL,
						format.External, format.Type,
						texture.data(layer, face, level));
					break;

				default:
					assert(0);
					break;
				}
			}
		}
	}

	return currentTextureID;
}

GLuint ResourceLoader::GLI_load_texture(const string& fileName) {
	gli::texture texture = gli::load(fileName);
	if (texture.empty())
		return 0;

	return GLI_create_texture(texture);
}

GLuint ResourceLoader::GLI_load_texture_from_memory(const char* buffer, size_t size) {
	gli::texture texture = gli::load(buffer, size);
	if (texture.empty())
		return 0;

	return GLI_create_texture(texture);
}

GLMaterial* ResourceLoader::AddMaterial(const string& textureFile, const string& vShaderFile, const string& fShaderFile) {
	MaterialKey key(textureFile, vShaderFile, fShaderFile);
	auto it = materials.find(key);
	if (it != materials.end())
		return it->second.get();

	GLuint textureID = 0;
	wxFileName fileName(textureFile);
	wxString fileExt = fileName.GetExt().Lower();
	if (fileExt == "dds" || fileExt == "ktx")
		textureID = GLI_load_texture(textureFile);

	if (!textureID)
		textureID = SOIL_load_OGL_texture(textureFile.c_str(), SOIL_LOAD_AUTO, ++currentTextureID, SOIL_FLAG_TEXTURE_REPEATS | SOIL_FLAG_MIPMAPS | SOIL_FLAG_GL_MIPMAPS);

	if (!textureID && Config.MatchValue("BSATextureScan", "true")) {
		if (Config["GameDataPath"].empty()) {
			wxLogWarning("Texture file '%s' not found.", textureFile);
			return nullptr;
		}

		wxMemoryBuffer data;
		wxString texFile = textureFile;
		texFile.Replace(Config["GameDataPath"], "");
		texFile.Replace("\\", "/");
		for (FSArchiveFile *archive : FSManager::archiveList()) {
			if (archive) {
				if (archive->hasFile(texFile.ToStdString())) {
					wxMemoryBuffer outData;
					archive->fileContents(texFile.ToStdString(), outData);

					if (!outData.IsEmpty()) {
						data = outData;
						break;
					}
				}
			}
		}

		if (!data.IsEmpty()) {
			byte* texBuffer = static_cast<byte*>(data.GetData());
			if (fileExt == "dds" || fileExt == "ktx")
				textureID = GLI_load_texture_from_memory((char*)texBuffer, data.GetBufSize());
			
			if (!textureID)
				textureID = SOIL_load_OGL_texture_from_memory(texBuffer, data.GetBufSize(), SOIL_LOAD_AUTO, ++currentTextureID, SOIL_FLAG_TEXTURE_REPEATS | SOIL_FLAG_MIPMAPS | SOIL_FLAG_GL_MIPMAPS);
		}
		else {
			wxLogWarning("Texture file '%s' not found.", textureFile);
			return nullptr;
		}
	}
	else if (!textureID) {
		wxLogWarning("Texture file '%s' not found.", textureFile);
		return nullptr;
	}

	auto& entry = materials[key];
	entry.reset(new GLMaterial(currentTextureID, vShaderFile.c_str(), fShaderFile.c_str()));
	return entry.get();
}

void ResourceLoader::Cleanup() {
	materials.clear();
}

size_t ResourceLoader::MatKeyHash::operator()(const MaterialKey& key) const {
	hash<string> strHash;
	return (strHash(get<0>(key)) ^ strHash(get<1>(key)) ^ strHash(get<2>(key)));
}
