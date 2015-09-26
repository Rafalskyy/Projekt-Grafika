//Copyright (C) 2010-2012 by Jason L. McKesson
//This file is licensed under the MIT License.


#include <string>
#include <vector>
#include <stack>
#include <math.h>
#include <stdio.h>
#include <glload/gl_3_3.h>
#include <glutil/glutil.h>
#include <GL/freeglut.h>
#include "../framework/framework.h"
#include "../framework/Mesh.h"
#include "../framework/directories.h"
#include <glimg/glimg.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#define ARRAY_COUNT( array ) (sizeof( array ) / (sizeof( array[0] ) * (sizeof( array ) != sizeof(void*) || sizeof( array[0] ) <= sizeof(void*))))

struct ProgramData
{
	GLuint theProgram;
	GLuint globalUniformBlockIndex;
	GLuint modelToWorldMatrixUnif;
	GLuint baseColorUnif;
};

float g_fzNear = 1.0f;
float g_fzFar = 1000.0f;

ProgramData Texture;
ProgramData ObjectColor;
ProgramData UniformColorTint;

GLuint g_GlobalMatricesUBO;

static const int g_iGlobalMatricesBindingIndex = 0;

ProgramData LoadProgram(const std::string &strVertexShader, const std::string &strFragmentShader)
{
	std::vector<GLuint> shaderList;

	shaderList.push_back(Framework::LoadShader(GL_VERTEX_SHADER, strVertexShader));
	shaderList.push_back(Framework::LoadShader(GL_FRAGMENT_SHADER, strFragmentShader));

	ProgramData data;
	data.theProgram = Framework::CreateProgram(shaderList);
	data.modelToWorldMatrixUnif = glGetUniformLocation(data.theProgram, "modelToWorldMatrix");
	data.globalUniformBlockIndex = glGetUniformBlockIndex(data.theProgram, "GlobalMatrices");
	data.baseColorUnif = glGetUniformLocation(data.theProgram, "baseColor");

	glUniformBlockBinding(data.theProgram, data.globalUniformBlockIndex, g_iGlobalMatricesBindingIndex);

	return data;
}

void InitializeProgram()
{
	Texture = LoadProgram("PosOnlyWorldTransformUBO.vert", "ColorUniform.frag");
	ObjectColor = LoadProgram("PosColorWorldTransformUBO.vert", "ColorPassthrough.frag");
	UniformColorTint = LoadProgram("PosColorWorldTransformUBO.vert", "ColorMultUniform.frag");

	glGenBuffers(1, &g_GlobalMatricesUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, g_GlobalMatricesUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4) * 2, NULL, GL_STREAM_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glBindBufferRange(GL_UNIFORM_BUFFER, g_iGlobalMatricesBindingIndex, g_GlobalMatricesUBO, 0, sizeof(glm::mat4) * 2);
}

GLuint g_checkerTexture = 0;

void LoadCheckerTexture()
{
	try
	{
		std::string filename(LOCAL_FILE_DIR);
		filename += "checker.dds";

		std::auto_ptr<glimg::ImageSet> pImageSet(glimg::loaders::dds::LoadFromFile(filename.c_str()));

		glGenTextures(1, &g_checkerTexture);
		glBindTexture(GL_TEXTURE_2D, g_checkerTexture);

		for(int mipmapLevel = 0; mipmapLevel < pImageSet->GetMipmapCount(); mipmapLevel++)
		{
			glimg::SingleImage image = pImageSet->GetImage(mipmapLevel, 0, 0);
			glimg::Dimensions dims = image.GetDimensions();

			glTexImage2D(GL_TEXTURE_2D, mipmapLevel, GL_RGB8, dims.width, dims.height, 0,
				GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, image.GetImageData());
		}

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, pImageSet->GetMipmapCount() - 1);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	catch(std::exception &e)
	{
		printf("%s\n", e.what());
		throw;
	}
}



glm::mat4 CalcLookAtMatrix(const glm::vec3 &cameraPt, const glm::vec3 &lookPt, const glm::vec3 &upPt)
{
	glm::vec3 lookDir = glm::normalize(lookPt - cameraPt);
	glm::vec3 upDir = glm::normalize(upPt);

	glm::vec3 rightDir = glm::normalize(glm::cross(lookDir, upDir));
	glm::vec3 perpUpDir = glm::cross(rightDir, lookDir);

	glm::mat4 rotMat(1.0f);
	rotMat[0] = glm::vec4(rightDir, 0.0f);
	rotMat[1] = glm::vec4(perpUpDir, 0.0f);
	rotMat[2] = glm::vec4(-lookDir, 0.0f);

	rotMat = glm::transpose(rotMat);

	glm::mat4 transMat(1.0f);
	transMat[3] = glm::vec4(-cameraPt, 1.0f);

	return rotMat * transMat;
}

Framework::Mesh *g_pConeMesh = NULL;
Framework::Mesh *g_pCylinderMesh = NULL;
Framework::Mesh *g_pCubeTintMesh = NULL;
Framework::Mesh *g_pCubeColorMesh = NULL;
Framework::Mesh *g_pPlaneMesh = NULL;
Framework::Mesh *g_pSphereMesh = NULL;

//Called after the window and OpenGL are initialized. Called exactly once, before the main loop.
void init()
{
	InitializeProgram();

	try
	{
		g_pConeMesh = new Framework::Mesh("UnitConeTint.xml");
		g_pCylinderMesh = new Framework::Mesh("UnitCylinderTint.xml");
		g_pCubeTintMesh = new Framework::Mesh("UnitCubeTint.xml");
		g_pCubeColorMesh = new Framework::Mesh("UnitCubeColor.xml");
		g_pPlaneMesh = new Framework::Mesh("UnitPlane.xml");
		g_pSphereMesh= new Framework::Mesh("UnitSphere.xml");
	}
	catch(std::exception &except)
	{
		printf("%s\n", except.what());
		throw;
	}

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CW);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LEQUAL);
	glDepthRange(0.0f, 1.0f);
	glEnable(GL_DEPTH_CLAMP);

	LoadCheckerTexture();
}

static float g_fYAngle = 0.0f;
static float g_fXAngle = 0.0f;

//Trees are 3x3 in X/Z, and fTrunkHeight+fConeHeight in the Y.
void DrawTree(glutil::MatrixStack &modelMatrix, float fTrunkHeight = 2.0f, float fConeHeight = 3.0f)
{
	//Draw trunk.
	{
		glutil::PushStack push(modelMatrix);

		modelMatrix.Scale(glm::vec3(1.0f, fTrunkHeight, 1.0f));
		modelMatrix.Translate(glm::vec3(0.0f, 0.5f, 0.0f));

		glUseProgram(UniformColorTint.theProgram);
		glUniformMatrix4fv(UniformColorTint.modelToWorldMatrixUnif, 1, GL_FALSE, glm::value_ptr(modelMatrix.Top()));
		glUniform4f(UniformColorTint.baseColorUnif, 0.694f, 0.4f, 0.106f, 1.0f);
		g_pCylinderMesh->Render();
		glUseProgram(0);
	}

	//Draw the treetop
	{
		glutil::PushStack push(modelMatrix);

		modelMatrix.Translate(glm::vec3(0.0f, fTrunkHeight, 0.0f));
		modelMatrix.Scale(glm::vec3(3.0f, fConeHeight, 3.0f));

		glUseProgram(UniformColorTint.theProgram);
		glUniformMatrix4fv(UniformColorTint.modelToWorldMatrixUnif, 1, GL_FALSE, glm::value_ptr(modelMatrix.Top()));
		glUniform4f(UniformColorTint.baseColorUnif, 0.0f, 1.0f, 0.0f, 1.0f);
		g_pConeMesh->Render();
		glUseProgram(0);
	}
}

const float g_fColumnBaseHeight = 0.25f;

//Columns are 1x1 in the X/Z, and fHieght units in the Y.
void DrawColumn(glutil::MatrixStack &modelMatrix, float fHeight = 5.0f)
{
	//Draw the bottom of the column.
	{
		glutil::PushStack push(modelMatrix);

		modelMatrix.Scale(glm::vec3(1.0f, g_fColumnBaseHeight, 1.0f));
		modelMatrix.Translate(glm::vec3(0.0f, 0.5f, 0.0f));

		glUseProgram(UniformColorTint.theProgram);
		glUniformMatrix4fv(UniformColorTint.modelToWorldMatrixUnif, 1, GL_FALSE, glm::value_ptr(modelMatrix.Top()));
		glUniform4f(UniformColorTint.baseColorUnif, 1.0f, 1.0f, 1.0f, 1.0f);
		g_pCubeTintMesh->Render();
		glUseProgram(0);
	}

	//Draw the top of the column.
	{
		glutil::PushStack push(modelMatrix);

		modelMatrix.Translate(glm::vec3(0.0f, fHeight - g_fColumnBaseHeight, 0.0f));
		modelMatrix.Scale(glm::vec3(1.0f, g_fColumnBaseHeight, 1.0f));
		modelMatrix.Translate(glm::vec3(0.0f, 0.5f, 0.0f));

		glUseProgram(UniformColorTint.theProgram);
		glUniformMatrix4fv(UniformColorTint.modelToWorldMatrixUnif, 1, GL_FALSE, glm::value_ptr(modelMatrix.Top()));
		glUniform4f(UniformColorTint.baseColorUnif, 0.9f, 0.9f, 0.9f, 0.9f);
		g_pCubeTintMesh->Render();
		glUseProgram(0);
	}

	//Draw the main column.
	{
		glutil::PushStack push(modelMatrix);

		modelMatrix.Translate(glm::vec3(0.0f, g_fColumnBaseHeight, 0.0f));
		modelMatrix.Scale(glm::vec3(0.8f, fHeight - (g_fColumnBaseHeight * 2.0f), 0.8f));
		modelMatrix.Translate(glm::vec3(0.0f, 0.5f, 0.0f));

		glUseProgram(UniformColorTint.theProgram);
		glUniformMatrix4fv(UniformColorTint.modelToWorldMatrixUnif, 1, GL_FALSE, glm::value_ptr(modelMatrix.Top()));
		glUniform4f(UniformColorTint.baseColorUnif, 0.9f, 0.9f, 0.9f, 0.9f);
		g_pCylinderMesh->Render();
		glUseProgram(0);
	}
}

struct TreeData
{
	float fXPos;
	float fZPos;
	float fTrunkHeight;
	float fConeHeight;
};

static const TreeData g_forest[] =
{
	{-45.0f, -40.0f, 2.0f, 3.0f},
	{-42.0f, -35.0f, 2.0f, 3.0f},
	{-39.0f, -29.0f, 2.0f, 4.0f},
	{-44.0f, -26.0f, 3.0f, 3.0f},
	{-40.0f, -22.0f, 2.0f, 4.0f},
	{-36.0f, -15.0f, 3.0f, 3.0f},
	{-41.0f, -11.0f, 2.0f, 3.0f},
	{-37.0f, -6.0f, 3.0f, 3.0f},
	{-45.0f, 0.0f, 2.0f, 3.0f},
	{-39.0f, 4.0f, 3.0f, 4.0f},
	{-36.0f, 8.0f, 2.0f, 3.0f},
	{-44.0f, 13.0f, 3.0f, 3.0f},
	{-42.0f, 17.0f, 2.0f, 3.0f},
	{-38.0f, 23.0f, 3.0f, 4.0f},
	{-41.0f, 27.0f, 2.0f, 3.0f},
	{-39.0f, 32.0f, 3.0f, 3.0f},
	{-44.0f, 37.0f, 3.0f, 4.0f},
	{-36.0f, 42.0f, 2.0f, 3.0f},

	{-32.0f, -45.0f, 2.0f, 3.0f},
	{-30.0f, -42.0f, 2.0f, 4.0f},
	{-34.0f, -38.0f, 3.0f, 5.0f},
	{-33.0f, -35.0f, 3.0f, 4.0f},
	{-29.0f, -28.0f, 2.0f, 3.0f},
	{-26.0f, -25.0f, 3.0f, 5.0f},
	{-35.0f, -21.0f, 3.0f, 4.0f},
	{-31.0f, -17.0f, 3.0f, 3.0f},
	{-28.0f, -12.0f, 2.0f, 4.0f},
	{-29.0f, -7.0f, 3.0f, 3.0f},
	{-26.0f, -1.0f, 2.0f, 4.0f},
	{-32.0f, 6.0f, 2.0f, 3.0f},
	{-30.0f, 10.0f, 3.0f, 5.0f},
	{-33.0f, 14.0f, 2.0f, 4.0f},
	{-35.0f, 19.0f, 3.0f, 4.0f},
	{-28.0f, 22.0f, 2.0f, 3.0f},
	{-33.0f, 26.0f, 3.0f, 3.0f},
	{-29.0f, 31.0f, 3.0f, 4.0f},
	{-32.0f, 38.0f, 2.0f, 3.0f},
	{-27.0f, 41.0f, 3.0f, 4.0f},
	{-31.0f, 45.0f, 2.0f, 4.0f},
	{-28.0f, 48.0f, 3.0f, 5.0f},

	{-25.0f, -48.0f, 2.0f, 3.0f},
	{-20.0f, -42.0f, 3.0f, 4.0f},
	{-22.0f, -39.0f, 2.0f, 3.0f},
	{-19.0f, -34.0f, 2.0f, 3.0f},
	{-23.0f, -30.0f, 3.0f, 4.0f},
	{-24.0f, -24.0f, 2.0f, 3.0f},
	{-16.0f, -21.0f, 2.0f, 3.0f},
	{-17.0f, -17.0f, 3.0f, 3.0f},
	{-25.0f, -13.0f, 2.0f, 4.0f},
	{-23.0f, -8.0f, 2.0f, 3.0f},
	{-17.0f, -2.0f, 3.0f, 3.0f},
	{-16.0f, 1.0f, 2.0f, 3.0f},
	{-19.0f, 4.0f, 3.0f, 3.0f},
	{-22.0f, 8.0f, 2.0f, 4.0f},
	{-21.0f, 14.0f, 2.0f, 3.0f},
	{-16.0f, 19.0f, 2.0f, 3.0f},
	{-23.0f, 24.0f, 3.0f, 3.0f},
	{-18.0f, 28.0f, 2.0f, 4.0f},
	{-24.0f, 31.0f, 2.0f, 3.0f},
	{-20.0f, 36.0f, 2.0f, 3.0f},
	{-22.0f, 41.0f, 3.0f, 3.0f},
	{-21.0f, 45.0f, 2.0f, 3.0f},

	{-12.0f, -40.0f, 2.0f, 4.0f},
	{-11.0f, -35.0f, 3.0f, 3.0f},
	{-10.0f, -29.0f, 1.0f, 3.0f},
	{-9.0f, -26.0f, 2.0f, 2.0f},
	{-6.0f, -22.0f, 2.0f, 3.0f},
	{-15.0f, -15.0f, 1.0f, 3.0f},
	{-8.0f, -11.0f, 2.0f, 3.0f},
	{-14.0f, -6.0f, 2.0f, 4.0f},
	{-12.0f, 0.0f, 2.0f, 3.0f},
	{-7.0f, 4.0f, 2.0f, 2.0f},
	{-13.0f, 8.0f, 2.0f, 2.0f},
	{-9.0f, 13.0f, 1.0f, 3.0f},
	{-13.0f, 17.0f, 3.0f, 4.0f},
	{-6.0f, 23.0f, 2.0f, 3.0f},
	{-12.0f, 27.0f, 1.0f, 2.0f},
	{-8.0f, 32.0f, 2.0f, 3.0f},
	{-10.0f, 37.0f, 3.0f, 3.0f},
	{-11.0f, 42.0f, 2.0f, 2.0f},


	{15.0f, 5.0f, 2.0f, 3.0f},
	{15.0f, 10.0f, 2.0f, 3.0f},
	{15.0f, 15.0f, 2.0f, 3.0f},
	{15.0f, 20.0f, 2.0f, 3.0f},
	{15.0f, 25.0f, 2.0f, 3.0f},
	{15.0f, 30.0f, 2.0f, 3.0f},
	{15.0f, 35.0f, 2.0f, 3.0f},
	{15.0f, 40.0f, 2.0f, 3.0f},
	{15.0f, 45.0f, 2.0f, 3.0f},

	{25.0f, 5.0f, 2.0f, 3.0f},
	{25.0f, 10.0f, 2.0f, 3.0f},
	{25.0f, 15.0f, 2.0f, 3.0f},
	{25.0f, 20.0f, 2.0f, 3.0f},
	{25.0f, 25.0f, 2.0f, 3.0f},
	{25.0f, 30.0f, 2.0f, 3.0f},
	{25.0f, 35.0f, 2.0f, 3.0f},
	{25.0f, 40.0f, 2.0f, 3.0f},
	{25.0f, 45.0f, 2.0f, 3.0f},
};

void DrawForest(glutil::MatrixStack &modelMatrix)
{
	for(int iTree = 0; iTree < ARRAY_COUNT(g_forest); iTree++)
	{
		const TreeData &currTree = g_forest[iTree];

		glutil::PushStack push(modelMatrix);
		modelMatrix.Translate(glm::vec3(currTree.fXPos, 0.0f, currTree.fZPos));
		DrawTree(modelMatrix, currTree.fTrunkHeight, currTree.fConeHeight);
	}
}

static bool g_bDrawLookatPoint = false;
static glm::vec3 g_camTarget(20.0f, 0.4f, 35.0f);

//In spherical coordinates.
static glm::vec3 g_sphereCamRelPos(90.0f, -12.0f, 35.0f);

glm::vec3 ResolvePosition(){
	glutil::MatrixStack tempMat;

	float alpha = Framework::DegToRad(g_sphereCamRelPos.x);

	float fSinAlpha = sinf(alpha);
	float fCosAlpha = cosf(alpha);

	glm::vec3 dirToControl(1.0f, 0.0f, -1.0f);
	return dirToControl;
}

glm::vec3 ResolveCamPosition()
{
	glutil::MatrixStack tempMat;

	float phi = Framework::DegToRad(g_sphereCamRelPos.x);
	float theta = Framework::DegToRad(g_sphereCamRelPos.y + 90.0f);

	float fSinTheta = sinf(theta);
	float fCosTheta = cosf(theta);
	float fCosPhi = cosf(phi);
	float fSinPhi = sinf(phi);

	glm::vec3 dirToCamera(fSinTheta * fCosPhi, fCosTheta, fSinTheta * fSinPhi);
	return (dirToCamera * g_sphereCamRelPos.z) + g_camTarget;
}

glm::vec3 obliczSterowanie()
{
	glutil::MatrixStack tempMat;

	float alpha = Framework::DegToRad(g_sphereCamRelPos.x);

	float fSinAlpha = sinf(alpha);
	float fCosAlpha = cosf(alpha);

	glm::vec3 dirToControl(-4 * fCosAlpha, 0.0f, -4 * fSinAlpha);
	return dirToControl;
}

const float stalaGranicy = 96.0f;

bool granica() {
	glm::vec3 vector = obliczSterowanie();
	if (g_camTarget.x > stalaGranicy && vector.x <= 0 ||
		g_camTarget.x < -stalaGranicy && vector.x >= 0 ||
		g_camTarget.z > stalaGranicy && vector.z <= 0 ||
		g_camTarget.z < -stalaGranicy && vector.z >= 0)
	{ 
			return true;
	}
	if (g_camTarget.x > stalaGranicy ||
		g_camTarget.x < -stalaGranicy ||
		g_camTarget.z > stalaGranicy ||
		g_camTarget.z < -stalaGranicy)
	{
			return false;
	}
};

void display()
{
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth(1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if(g_pConeMesh && g_pCylinderMesh && g_pCubeTintMesh && g_pCubeColorMesh && g_pPlaneMesh)
	{
		const glm::vec3 &camPos = ResolveCamPosition();

		glutil::MatrixStack camMatrix;
		camMatrix.SetMatrix(CalcLookAtMatrix(camPos, g_camTarget, glm::vec3(0.0f, 1.0f, 0.0f)));

		glBindBuffer(GL_UNIFORM_BUFFER, g_GlobalMatricesUBO);
		glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(camMatrix.Top()));
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		glutil::MatrixStack modelMatrix;

		//Render the ground plane.
		{
			glutil::PushStack push(modelMatrix);

			modelMatrix.Scale(glm::vec3(200.0f, 1.0f, 200.0f));

			glUseProgram(Texture.theProgram);

			glUniformMatrix4fv(Texture.modelToWorldMatrixUnif, 1, GL_FALSE, glm::value_ptr(modelMatrix.Top()));
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, g_checkerTexture);
			g_pPlaneMesh->Render("tex");
			glUseProgram(0);

		}

		//Draw the trees
		DrawForest(modelMatrix);

		//Draw the building.
		{
			glutil::PushStack push(modelMatrix);
			modelMatrix.Translate(glm::vec3(20.0f, 0.0f, -10.0f));

		}

		{

			glutil::MatrixStack tempMat;

			float alpha = Framework::DegToRad(g_sphereCamRelPos.x + 90.0f);

			float fSinAlpha = sinf(alpha);
			float fCosAlpha = cosf(alpha);

			glutil::PushStack push(modelMatrix);

			modelMatrix.Translate(glm::vec3((g_camTarget.x + (fCosAlpha/2)), g_camTarget.y , (g_camTarget.z + (fSinAlpha/2))));
			modelMatrix.Scale(1.0f, 2.0f, 1.0f);

			glUseProgram(UniformColorTint.theProgram);
			glUniformMatrix4fv(UniformColorTint.modelToWorldMatrixUnif, 1, GL_FALSE, glm::value_ptr(modelMatrix.Top()));
			glUniform4f(UniformColorTint.baseColorUnif, 0.694f, 0.4f, 0.106f, 1.0f);
			g_pCylinderMesh->Render();
			glUseProgram(0);

		}

		{


			glutil::PushStack push(modelMatrix);

			glutil::MatrixStack tempMat;

			float alpha = Framework::DegToRad(g_sphereCamRelPos.x + 90.0f);

			float fSinAlpha =  sinf(alpha);
			float fCosAlpha =  cosf(alpha);

			glm::vec3 vector (((g_camTarget.x + fCosAlpha)), (g_camTarget.y + 2.0f), (g_camTarget.z + fSinAlpha));

			modelMatrix.Translate(vector);
			modelMatrix.Scale(1.0f, 1.5f, 1.0f);

			glUseProgram(UniformColorTint.theProgram);
			glUniformMatrix4fv(UniformColorTint.modelToWorldMatrixUnif, 1, GL_FALSE, glm::value_ptr(modelMatrix.Top()));
			glUniform4f(UniformColorTint.baseColorUnif, 0.694f, 0.4f, 0.106f, 1.0f);
			g_pCylinderMesh->Render();
			glUseProgram(0);

		}

		{


			glutil::PushStack push(modelMatrix);

			glutil::MatrixStack tempMat;

			float alpha = Framework::DegToRad(g_sphereCamRelPos.x + 90.0f);

			float fSinAlpha =   sinf(alpha);
			float fCosAlpha =   cosf(alpha);

			modelMatrix.Translate(glm::vec3((g_camTarget.x - fCosAlpha), (g_camTarget.y + 2.0f), (g_camTarget.z - fSinAlpha)));
			modelMatrix.Scale(1.0f, 1.5f, 1.0f);

			glUseProgram(UniformColorTint.theProgram);
			glUniformMatrix4fv(UniformColorTint.modelToWorldMatrixUnif, 1, GL_FALSE, glm::value_ptr(modelMatrix.Top()));
			glUniform4f(UniformColorTint.baseColorUnif, 0.694f, 0.4f, 0.106f, 1.0f);
			g_pCylinderMesh->Render();
			glUseProgram(0);

		}


		{
			glutil::PushStack push(modelMatrix);

			glutil::MatrixStack tempMat;

			float alpha = Framework::DegToRad(g_sphereCamRelPos.x + 90.0f);

			float fSinAlpha = sinf(alpha);
			float fCosAlpha = cosf(alpha);

			modelMatrix.Translate(glm::vec3((g_camTarget.x - (fCosAlpha/2)), g_camTarget.y, g_camTarget.z - (fSinAlpha/2)));
			modelMatrix.Scale(1.0f, 2.0f, 1.0f);

			glUseProgram(UniformColorTint.theProgram);
			glUniformMatrix4fv(UniformColorTint.modelToWorldMatrixUnif, 1, GL_FALSE, glm::value_ptr(modelMatrix.Top()));
			glUniform4f(UniformColorTint.baseColorUnif, 0.694f, 0.4f, 0.106f, 1.0f);
			g_pCylinderMesh->Render();
			glUseProgram(0);
		}

		{
			glutil::PushStack push(modelMatrix);

			modelMatrix.Translate(glm::vec3(g_camTarget.x, g_camTarget.y + 2.0f, g_camTarget.z));
			modelMatrix.Scale(2.0f, 2.0f, 2.0f);

			glUseProgram(UniformColorTint.theProgram);
			glUniformMatrix4fv(UniformColorTint.modelToWorldMatrixUnif, 1, GL_FALSE, glm::value_ptr(modelMatrix.Top()));
			glUniform4f(UniformColorTint.baseColorUnif, 0.694f, 0.4f, 0.106f, 1.0f);
			g_pCylinderMesh->Render();
			glUseProgram(0);
		}

		{
			glutil::PushStack push(modelMatrix);

			modelMatrix.Translate(glm::vec3(g_camTarget.x, g_camTarget.y + 4.0f, g_camTarget.z));
			modelMatrix.Scale(2.0f, 2.0f, 2.0f);

			glUseProgram(UniformColorTint.theProgram);
			glUniformMatrix4fv(UniformColorTint.modelToWorldMatrixUnif, 1, GL_FALSE, glm::value_ptr(modelMatrix.Top()));
			glUniform4f(UniformColorTint.baseColorUnif, 0.694f, 0.4f, 0.106f, 1.0f);
			g_pSphereMesh->Render();
			glUseProgram(0);
		}
	}

	glutSwapBuffers();
}

//Called whenever the window is resized. The new window size is given, in pixels.
//This is an opportunity to call glViewport or glScissor to keep up with the change in size.
void reshape (int w, int h)
{
	glutil::MatrixStack persMatrix;
	persMatrix.Perspective(45.0f, (w / (float)h), g_fzNear, g_fzFar);

	glBindBuffer(GL_UNIFORM_BUFFER, g_GlobalMatricesUBO);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(persMatrix.Top()));
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glViewport(0, 0, (GLsizei) w, (GLsizei) h);
	glutPostRedisplay();
}

//Called whenever a key on the keyboard was pressed.
//The key is given by the ''key'' parameter, which is in ASCII.
//It's often a good idea to have the escape key (ASCII value 27) call glutLeaveMainLoop() to 
//exit the program.

void keyboard(unsigned char key, int x, int y)
{
	glm::vec3 tempCamTarget;
	switch (key)
	{
	case 27:
		delete g_pConeMesh;
		g_pConeMesh = NULL;
		delete g_pCylinderMesh;
		g_pCylinderMesh = NULL;
		delete g_pCubeTintMesh;
		g_pCubeTintMesh = NULL;
		delete g_pCubeColorMesh;
		g_pCubeColorMesh = NULL;
		delete g_pPlaneMesh;
		g_pPlaneMesh = NULL;
		glutLeaveMainLoop();
		return;
	case 'w': if (granica())g_camTarget = obliczSterowanie() + g_camTarget; break;
	case 's': if (granica())g_camTarget = -obliczSterowanie() + g_camTarget; break;
	case 'd': g_sphereCamRelPos.x += 11.25f; break;
	case 'a': g_sphereCamRelPos.x -= 11.25f; break;
	case 'e': g_sphereCamRelPos.y -= 11.25f; break;
	case 'q': g_sphereCamRelPos.y += 11.25f; break;
	case 'W': if (granica())g_camTarget = obliczSterowanie() / 4.0f + g_camTarget; break;
	case 'S': if (granica())g_camTarget = -obliczSterowanie() / 4.0f + g_camTarget; break;
	case 'D': g_sphereCamRelPos.x += 1.125f; break;
	case 'A': g_sphereCamRelPos.x -= 1.125f; break;
	case 'E': g_sphereCamRelPos.y -= 1.125f; break;
	case 'Q': g_sphereCamRelPos.y += 1.125f; break;
		
	case 32:
		g_bDrawLookatPoint = !g_bDrawLookatPoint;
		printf("Target: %f, %f, %f\n", g_camTarget.x, g_camTarget.y, g_camTarget.z);
		printf("Position: %f, %f, %f\n", g_sphereCamRelPos.x, g_sphereCamRelPos.y, g_sphereCamRelPos.z);
		break;
	}

	g_sphereCamRelPos.y = glm::clamp(g_sphereCamRelPos.y, -78.75f, -1.0f);
	g_camTarget.y = g_camTarget.y > 0.0f ? g_camTarget.y : 0.0f;
	g_sphereCamRelPos.z = g_sphereCamRelPos.z > 5.0f ? g_sphereCamRelPos.z : 5.0f;

	glutPostRedisplay();
}


unsigned int defaults(unsigned int displayMode, int &width, int &height) {return displayMode;}
