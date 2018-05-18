#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

#include <GL/glew.h>
#include <freeglut/glut.h>
#include "textfile.h"
#include "glm.h"

#include "Matrices.h"

#define PI 3.1415926

#pragma comment (lib, "glew32.lib")
#pragma comment (lib, "freeglut.lib")

#ifndef GLUT_WHEEL_UP
# define GLUT_WHEEL_UP   0x0003
# define GLUT_WHEEL_DOWN 0x0004
#endif

#ifndef GLUT_KEY_ESC
# define GLUT_KEY_ESC 0x001B
#endif

#ifndef max
# define max(a,b) (((a)>(b))?(a):(b))
# define min(a,b) (((a)<(b))?(a):(b))
#endif

using namespace std;

// Shader attributes
GLint iLocPosition;
GLint iLocColor;
GLint iLocMVP;

struct camera
{
	Vector3 position;
	Vector3 center;
	Vector3 up_vector;
};

struct iLocLightInfo
{
	GLuint position;
	GLuint ambient;
	GLuint diffuse;
	GLuint specular;
}iLocLightInfo[3];

struct Group
{
	int numTriangles;
	GLfloat *vertices;
	GLfloat *normals;
	GLfloat ambient[4];
	GLfloat diffuse[4];
	GLfloat specular[4];
	GLfloat shininess;
};

struct Model
{
	int numGroups;
	GLMmodel *obj;
	Group *group;
	Matrix4 N;
	Vector3 position = Vector3(0,0,0);
	Vector3 scale = Vector3(1,1,1);
	Vector3 rotation = Vector3(0,0,0);	// Euler form
};

struct project_setting
{
	GLfloat nearClip, farClip;
	GLfloat fovy;
	GLfloat aspect;
	GLfloat left, right, top, bottom;
};

// Shader attributes
GLuint vaoHandle;
GLuint vboHandles[2];
GLuint positionBufferHandle;
GLuint colorBufferHandle;

// Shader attributes for uniform variables
GLuint iLocP;
GLuint iLocV;
GLuint iLocN;

Matrix4 view_matrix;
Matrix4 project_matrix;

project_setting proj;
camera main_camera;

int current_x, current_y;

Model* models;	// store the models we load
vector<string> filenames; // .obj filename list
int cur_idx = 0; // represent which model should be rendered now
int color_mode = 0;
bool use_wire_mode = false;
float rotateVal = 0.0f;
int timeInterval = 33;
bool isRotate = true;
float scaleOffset = 0.65f;

int operateMode = 0;
bool drag = 0;
enum {
	opTranslate, opRotate, opScale, opCamera, opEye,opUp
};
string operateName[] = { "Translate model","Rotate model","Scale model",
"Translate camera position","Translate camera center","Translate camera up direction"};

Matrix4 translate(Vector3 vec)
{
	Matrix4 mat = Matrix4();
	mat.identity();
	for (int i = 0; i < 3; i++)mat[i * 4 + 3] = vec[i];
	return mat;
}

Matrix4 scaling(Vector3 vec)
{
	Matrix4 mat=Matrix4();
	mat[0] = vec.x;
	mat[5] = vec.y;
	mat[10] = vec.z;
	return mat;
}

Matrix4 rotateX(GLfloat val)
{
	Matrix4 mat = Matrix4();
	float c = cos(val);
	float s = sin(val);
	mat[0] = 1;
	mat[5] = c;
	mat[6] = -s;
	mat[9] = s;
	mat[10] = c;
	mat[15] = 1;

	return mat;
}

Matrix4 rotateY(GLfloat val)
{
	Matrix4 mat = Matrix4();
	float c = cos(val);
	float s = sin(val);
	mat[0] = c;
	mat[2] = s;
	mat[5] = 1;
	mat[8] = -s;
	mat[10] = c;
	mat[15] = 1;
	return mat;
}

Matrix4 rotateZ(GLfloat val)
{
	Matrix4 mat = Matrix4();
	float c = cos(val);
	float s = sin(val);
	mat[0] = c;
	mat[1] = -s;
	mat[4] = s;
	mat[5] = c;
	mat[10] = 1;
	mat[15] = 1;
	return mat;
}

Matrix4 rotate(Vector3 vec)
{
	return rotateX(vec.x)*rotateY(vec.y)*rotateZ(vec.z);
}

void setViewingMatrix()
{
	Matrix4 T = translate(-main_camera.position);
	Matrix4 R = Matrix4().identity();
	Vector3 rz = (main_camera.position - main_camera.center).normalize();
	Vector3 rx = (main_camera.center - main_camera.position).cross(main_camera.up_vector - main_camera.position).normalize();
	Vector3 ry = rz.cross(rx);

	for (int i = 0; i < 3; i++)R[i] = rx[i];
	for (int i = 0; i < 3; i++)R[i + 4] = ry[i];
	for (int i = 0; i < 3; i++)R[i + 8] = rz[i];
	
	/*
	Vector3 fwd = (main_camera.center - main_camera.position).normalize();
	Vector3 left = main_camera.up_vector.cross(fwd);
	Vector3 up = fwd.cross(left).normalize();

	for (int i = 0; i < 3; i++)R[i] = fwd[i];
	for (int i = 0; i < 3; i++)R[i + 4] = up[i];
	for (int i = 0; i < 3; i++)R[i + 8] = left[i];
	*/

	view_matrix = R * T;
}

void setOrthogonal()
{
	float r = 1.F, t = 1.F, f = 8.F, n = 0.F;

	project_matrix = Matrix4(
		1 / r, 0, 0, 0,
		0, 1 / t, 0, 0,
		0, 0, -2.0 / (f - n), -(f + n) / (f - n),
		0, 0, 0, 1
	);
}

void setPerspective()
{
	float r = 1.F, t = 1.F, f = 16.F, n = 1.F;

	project_matrix = Matrix4(
		n / r, 0, 0, 0,
		0, n / t, 0, 0,
		0, 0, (f + n) / (n - f), 2 * f*n / (n - f),
		0, 0, -1, 0
	);
}

// replaced
void traverseColorModel(Model &m)
{
	m.numGroups = m.obj->numgroups;
	m.group = new Group[m.numGroups];
	GLMgroup* group = m.obj->groups;

	GLfloat maxVal[3] = { -100000, -100000, -100000 };
	GLfloat minVal[3] = { 100000, 100000, 100000 };

	int curGroupIdx = 0;

	// Iterate all the groups of this model
	while (group)
	{
		m.group[curGroupIdx].vertices = new GLfloat[group->numtriangles * 9];
		m.group[curGroupIdx].normals = new GLfloat[group->numtriangles * 9];
		m.group[curGroupIdx].numTriangles = group->numtriangles;
		
		// Fetch material information
		memcpy(m.group[curGroupIdx].ambient, m.obj->materials[group->material].ambient, sizeof(GLfloat) * 4);
		memcpy(m.group[curGroupIdx].diffuse, m.obj->materials[group->material].diffuse, sizeof(GLfloat) * 4);
		memcpy(m.group[curGroupIdx].specular, m.obj->materials[group->material].specular, sizeof(GLfloat) * 4);

		m.group[curGroupIdx].shininess = m.obj->materials[group->material].shininess;

		// For each triangle in this group
		for (int i = 0; i < group->numtriangles; i++)
		{
			int triangleIdx = group->triangles[i];
			int indv[3] = {
				m.obj->triangles[triangleIdx].vindices[0],
				m.obj->triangles[triangleIdx].vindices[1],
				m.obj->triangles[triangleIdx].vindices[2]
			};
			int indn[3] = {
				m.obj->triangles[triangleIdx].nindices[0],
				m.obj->triangles[triangleIdx].nindices[1],
				m.obj->triangles[triangleIdx].nindices[2]
			};

			// For each vertex in this triangle
			for (int j = 0; j < 3; j++)
			{
				m.group[curGroupIdx].vertices[i * 9 + j * 3 + 0] = m.obj->vertices[indv[j] * 3 + 0];
				m.group[curGroupIdx].vertices[i * 9 + j * 3 + 1] = m.obj->vertices[indv[j] * 3 + 1];
				m.group[curGroupIdx].vertices[i * 9 + j * 3 + 2] = m.obj->vertices[indv[j] * 3 + 2];
				m.group[curGroupIdx].normals[i * 9 + j * 3 + 0] = m.obj->normals[indn[j] * 3 + 0];
				m.group[curGroupIdx].normals[i * 9 + j * 3 + 1] = m.obj->normals[indn[j] * 3 + 1];
				m.group[curGroupIdx].normals[i * 9 + j * 3 + 2] = m.obj->normals[indn[j] * 3 + 2];

				maxVal[0] = max(maxVal[0], m.group[curGroupIdx].vertices[i * 9 + j * 3 + 0]);
				maxVal[1] = max(maxVal[1], m.group[curGroupIdx].vertices[i * 9 + j * 3 + 1]);
				maxVal[2] = max(maxVal[2], m.group[curGroupIdx].vertices[i * 9 + j * 3 + 2]);

				minVal[0] = min(minVal[0], m.group[curGroupIdx].vertices[i * 9 + j * 3 + 0]);
				minVal[1] = min(minVal[1], m.group[curGroupIdx].vertices[i * 9 + j * 3 + 1]);
				minVal[2] = min(minVal[2], m.group[curGroupIdx].vertices[i * 9 + j * 3 + 2]);
			}
		}
		group = group->next;
		curGroupIdx++;
	}

	// Normalize the model
	float norm_scale = max(max(abs(maxVal[0] - minVal[0]), abs(maxVal[1] - minVal[1])), abs(maxVal[2] - minVal[2]));
	Matrix4 S, T;

	m.scale = Vector3(2 / norm_scale * scaleOffset, 2 / norm_scale * scaleOffset, 2 / norm_scale * scaleOffset);
	//m.position = Vector3((maxVal[0] + minVal[0]) / 2, (maxVal[1] + minVal[1]) / 2, (maxVal[2] + minVal[2]) / 2);
	m.position = Vector3();
	m.N = S * T;

}

// replaced
void showStatus() {
	Model m = models[cur_idx];
	cout << "\n========================================================\n";
	cout << "Model name: " << filenames[cur_idx] << "\n";
	cout << "Mode: " << operateName[operateMode] << "\n";
	cout << "Position: " << m.position << "\n";
	cout << "Rotation: " << m.rotation << "\n";
	cout << "Scale: " << m.scale << "\n";
	cout << "========================================================\n";
}

// replaced
void loadOBJModel()
{
	models = new Model[filenames.size()];
	int idx = 0;
	for (string filename : filenames)
	{
		models[idx].obj = glmReadOBJ((char*)filename.c_str());
		traverseColorModel(models[idx++]);
	}
}

void onIdle()
{
	glutPostRedisplay();
}

// replaced
void drawModel(Model& m)
{
	int groupNum = m.numGroups;
	Matrix4 T, R, S;
	T = translate(m.position);
	R = rotate(m.rotation);
	S = scaling(m.scale);

	// [TODO] Assign MVP correct value
	// [HINT] MVP = projection_matrix * view_matrix * ??? * ??? * ???
	Matrix4 MVP = project_matrix * view_matrix * T * S * R;

	for (int i = 0; i < groupNum; i++)
	{

		glBindBuffer(GL_ARRAY_BUFFER, positionBufferHandle);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float)*m.group[i].numTriangles * 9, m.group[i].vertices, GL_DYNAMIC_DRAW);

		glUniformMatrix4fv(iLocN, 1, GL_FALSE, MVP.getTranspose());
		float fakeColor[] = {
			0.7f, 0.8f, 0.3f, 1.0f
		};

		glUniform4fv(iLocLightInfo[0].position, 1, fakeColor);
		glUniform4fv(iLocLightInfo[0].ambient, 1, m.group[i].ambient);
		glDrawArrays(GL_TRIANGLES, 0, m.group[i].numTriangles * 3);
	}
}

void drawPlane()
{
	GLfloat vertices[18]{ 1.0, -1.0, -1.0,
						  1.0, -1.0, 1.0,
						 -1.0, -1.0, -1.0,
						  1.0, -1.0, 1.0,
						 -1.0, -1.0, 1.0,
						 -1.0, -1.0, -1.0 };

	GLfloat colors[18]{ 0.0,0.8,0.0,
						0.0,0.5,0.8,
						0.0,0.8,0.0,
						0.0,0.5,0.8,
						0.0,0.5,0.8, 
						0.0,0.8,0.0 };


	Matrix4 MVP = project_matrix*view_matrix;
	GLfloat mvp[16];

	// row-major ---> column-major
	mvp[0] = MVP[0];  mvp[4] = MVP[1];   mvp[8] = MVP[2];    mvp[12] = MVP[3];
	mvp[1] = MVP[4];  mvp[5] = MVP[5];   mvp[9] = MVP[6];    mvp[13] = MVP[7];
	mvp[2] = MVP[8];  mvp[6] = MVP[9];   mvp[10] = MVP[10];   mvp[14] = MVP[11];
	mvp[3] = MVP[12]; mvp[7] = MVP[13];  mvp[11] = MVP[14];   mvp[15] = MVP[15];

	glVertexAttribPointer(iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, vertices);
	glVertexAttribPointer(iLocColor, 3, GL_FLOAT, GL_FALSE, 0, colors);

	glUniformMatrix4fv(iLocMVP, 1, GL_FALSE, mvp);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

// replaced
void onDisplay(void)
{
	// clear canvas
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);	// clear canvas to color(0,0,0)->black
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//drawTriangle();
	drawModel(models[cur_idx]);

	glutSwapBuffers();
}

void showShaderCompileStatus(GLuint shader, GLint *shaderCompiled)
{
	glGetShaderiv(shader, GL_COMPILE_STATUS, shaderCompiled);
	if (GL_FALSE == (*shaderCompiled))
	{
		GLint maxLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

		// The maxLength includes the NULL character.
		GLchar *errorLog = (GLchar*)malloc(sizeof(GLchar) * maxLength);
		glGetShaderInfoLog(shader, maxLength, &maxLength, &errorLog[0]);
		fprintf(stderr, "%s", errorLog);

		glDeleteShader(shader);
		free(errorLog);
	}
}

// replaced
void setVertexArrayObject()
{
	// Create and setup the vertex array object
	glGenVertexArrays(1, &vaoHandle);
	glBindVertexArray(vaoHandle);

	// Enable the vertex attribute arrays
	glEnableVertexAttribArray(0);	// Vertex position
	glEnableVertexAttribArray(1);	// Vertex color

									// Map index 0 to the position buffer
	glBindBuffer(GL_ARRAY_BUFFER, positionBufferHandle);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	// Map index 1 to the color buffer
	glBindBuffer(GL_ARRAY_BUFFER, colorBufferHandle);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
}

// replaced
void setVertexBufferObjects()
{
	// Triangles Data
	float positionData[] = {
		-0.8f, -0.8f, 0.0f,
		0.8f, -0.8f, 0.0f,
		0.0f, 0.8f, 0.0f };

	float colorData[] = {
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f };

	glGenBuffers(2, vboHandles);

	positionBufferHandle = vboHandles[0];
	colorBufferHandle = vboHandles[1];

	// Populate the position buffer
	glBindBuffer(GL_ARRAY_BUFFER, positionBufferHandle);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 9, positionData, GL_STATIC_DRAW);

	// Populate the color buffer
	glBindBuffer(GL_ARRAY_BUFFER, colorBufferHandle);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 9, colorData, GL_STATIC_DRAW);
}

// replaced
void setUniformVariables(GLuint p)
{
	iLocP = glGetUniformLocation(p, "um4p");
	iLocV = glGetUniformLocation(p, "um4v");
	iLocN = glGetUniformLocation(p, "um4n");

	iLocLightInfo[0].position = glGetUniformLocation(p, "light[0].Position");
	iLocLightInfo[0].ambient = glGetUniformLocation(p, "light[0].La");
	iLocLightInfo[0].diffuse = glGetUniformLocation(p, "light[0].Ld");
	iLocLightInfo[0].specular = glGetUniformLocation(p, "light[0].Ls");

	iLocLightInfo[1].position = glGetUniformLocation(p, "light[1].Position");
	iLocLightInfo[1].ambient = glGetUniformLocation(p, "light[1].La");
	iLocLightInfo[1].diffuse = glGetUniformLocation(p, "light[1].Ld");
	iLocLightInfo[1].specular = glGetUniformLocation(p, "light[1].Ls");

	iLocLightInfo[2].position = glGetUniformLocation(p, "light[2].Position");
	iLocLightInfo[2].ambient = glGetUniformLocation(p, "light[2].La");
	iLocLightInfo[2].diffuse = glGetUniformLocation(p, "light[2].Ld");
	iLocLightInfo[2].specular = glGetUniformLocation(p, "light[2].Ls");
}

// replaced
void setShaders()
{
	GLuint v, f, p;
	char *vs = NULL;
	char *fs = NULL;

	v = glCreateShader(GL_VERTEX_SHADER);
	f = glCreateShader(GL_FRAGMENT_SHADER);

	vs = textFileRead("shader.vert");
	fs = textFileRead("shader.frag");

	glShaderSource(v, 1, (const GLchar**)&vs, NULL);
	glShaderSource(f, 1, (const GLchar**)&fs, NULL);

	free(vs);
	free(fs);

	// compile vertex shader
	glCompileShader(v);
	GLint vShaderCompiled;
	showShaderCompileStatus(v, &vShaderCompiled);
	if (!vShaderCompiled) system("pause"), exit(123);

	// compile fragment shader
	glCompileShader(f);
	GLint fShaderCompiled;
	showShaderCompileStatus(f, &fShaderCompiled);
	if (!fShaderCompiled) system("pause"), exit(456);

	p = glCreateProgram();

	// bind shader
	glAttachShader(p, f);
	glAttachShader(p, v);

	// link program
	glLinkProgram(p);
	glUseProgram(p);

	setUniformVariables(p);
	setVertexBufferObjects();
	setVertexArrayObject();
}

void onMouse(int who, int state, int x, int y)
{
	printf("%18s(): (%d, %d) ", __FUNCTION__, x, y);

	switch (who)
	{
		case GLUT_LEFT_BUTTON:
			current_x = x;
			current_y = y;
			if (!state)drag = 1;
			else drag = 0;
			break;
		case GLUT_MIDDLE_BUTTON: printf("middle button "); break;
		case GLUT_RIGHT_BUTTON:
			current_x = x;
			current_y = y;
			break;
		case GLUT_WHEEL_UP:
			printf("wheel up      \n");
			// [TODO] assign corresponding operation
			if (operateMode == opCamera)
			{
				main_camera.position.z -= 0.025;
				setViewingMatrix();
				printf("Camera Position = ( %f , %f , %f )\n", main_camera.position.x, main_camera.position.y, main_camera.position.z);
			}
			else if (operateMode == opEye)
			{
				main_camera.center.z += 0.1;
				setViewingMatrix();
				printf("Camera Viewing Direction = ( %f , %f , %f )\n", main_camera.center.x, main_camera.center.y, main_camera.center.z);
			}
			else if (operateMode == opUp)
			{
				main_camera.up_vector.z += 0.33;
				setViewingMatrix();
				printf("Camera Up Vector = ( %f , %f , %f )\n", main_camera.up_vector.x, main_camera.up_vector.y, main_camera.up_vector.z);
			}
			else if (operateMode == opTranslate)
			{
				models[cur_idx].position.z += 0.1;
			}
			else if (operateMode == opScale)
			{
				models[cur_idx].scale.z += 1.025;
			}
			else if (operateMode == opRotate)
			{
				models[cur_idx].rotation.z += (PI/180.0) * 5;
			}
			break;
		case GLUT_WHEEL_DOWN:
			printf("wheel down    \n");
			// [TODO] assign corresponding operation
			if (operateMode==opCamera)
			{
				main_camera.position.z += 0.025;
				setViewingMatrix();
				printf("Camera Position = ( %f , %f , %f )\n", main_camera.position.x, main_camera.position.y, main_camera.position.z);
			}
			else if (operateMode == opEye)
			{
				main_camera.center.z -= 0.33;
				setViewingMatrix();
				printf("Camera Viewing Direction = ( %f , %f , %f )\n", main_camera.center.x, main_camera.center.y, main_camera.center.z);
			}
			else if (operateMode == opUp)
			{
				main_camera.up_vector.z -= 0.33;
				setViewingMatrix();
				printf("Camera Up Vector = ( %f , %f , %f )\n", main_camera.up_vector.x, main_camera.up_vector.y, main_camera.up_vector.z);
			}
			else if (operateMode == opTranslate)
			{
				models[cur_idx].position.z -= 0.33;
			}
			else if (operateMode == opScale)
			{
				models[cur_idx].scale.z -= 1.025;
			}
			else if (operateMode == opRotate)
			{
				models[cur_idx].rotation.z -= (PI / 180.0) * 5;
			}
			break;
		default:                 
			printf("0x%02X          ", who); break;
	}

	switch (state)
	{
		case GLUT_DOWN: printf("start "); break;
		case GLUT_UP:   printf("end   "); break;
	}

	printf("\n");
}

void onMouseMotion(int x, int y)
{
	int diff_x = x - current_x;
	int diff_y = y - current_y;
	current_x = x;
	current_y = y;

	// [TODO] assign corresponding operation
	if (!drag);
	else if (operateMode == opCamera)
	{
		main_camera.position.x += diff_x*(1.0 / 400.0);
		main_camera.position.y += diff_y*(1.0 / 400.0);
		setViewingMatrix();
		printf("Camera Position = ( %f , %f , %f )\n", main_camera.position.x, main_camera.position.y, main_camera.position.z);
	}
	else if (operateMode == opEye)
	{
		main_camera.center.x += diff_x*(1.0 / 400.0);
		main_camera.center.y += diff_y*(1.0 / 400.0);
		setViewingMatrix();
		printf("Camera Viewing Direction = ( %f , %f , %f )\n", main_camera.center.x, main_camera.center.y, main_camera.center.z);
	}
	else if (operateMode == opUp)
	{
		main_camera.up_vector.x += diff_x*0.1;
		main_camera.up_vector.y += diff_y*0.1;
		setViewingMatrix();
		printf("Camera Up Vector = ( %f , %f , %f )\n", main_camera.up_vector.x, main_camera.up_vector.y, main_camera.up_vector.z);
	}
	else if (operateMode == opTranslate)
	{
		models[cur_idx].position.x += diff_x*(1.0 / 400.0);
		models[cur_idx].position.y -= diff_y*(1.0 / 400.0);
	}
	else if (operateMode == opScale)
	{
		models[cur_idx].scale.x += diff_x*0.025;
		models[cur_idx].scale.y += diff_y*0.025;
	}
	else if (operateMode == opRotate)
	{
		models[cur_idx].rotation.x += PI / 180.0*diff_y*(400 / 400.0);
		models[cur_idx].rotation.y += PI / 180.0*diff_x*(400 / 400.0);
	}
	printf("%18s(): (%d, %d) mouse move\n", __FUNCTION__, x, y);
}

void onKeyboard(unsigned char key, int x, int y)
{
	printf("%18s(): (%d, %d) key: %c(0x%02X) ", __FUNCTION__, x, y, key, key);
	switch (key)
	{
	case GLUT_KEY_ESC: /* the Esc key */
		exit(0);
		break;
	case 'z':
		cur_idx = (cur_idx + filenames.size() - 1) % filenames.size();
		break;
	case 'x':
		cur_idx = (cur_idx + 1) % filenames.size();
		break;
	case 'w':
		use_wire_mode = !use_wire_mode;
		if (use_wire_mode)
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		else
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		break;
	case 'o':
		setOrthogonal();
		break;
	case 'p':
		setPerspective();
		break;
	case 'e':
		operateMode = opEye;
		break;
	case 'c':
		operateMode = opCamera;
		break;
	case 'i':
		showStatus();
		break;
	case 't':
		operateMode = opTranslate;
		break;
	case 's':
		operateMode = opScale;
		break;
	case 'r':
		operateMode = opRotate;
		break;
	}
	printf("\n");
}

void onKeyboardSpecial(int key, int x, int y) {
	printf("%18s(): (%d, %d) ", __FUNCTION__, x, y);
	switch (key)
	{
	case GLUT_KEY_LEFT:
		printf("key: LEFT ARROW");
		break;

	case GLUT_KEY_RIGHT:
		printf("key: RIGHT ARROW");
		break;

	default:
		printf("key: 0x%02X      ", key);
		break;
	}
	printf("\n");
}

void onWindowReshape(int width, int height)
{
	proj.aspect = width / height;

	printf("%18s(): %dx%d\n", __FUNCTION__, width, height);
}

// you can setup your own camera setting when testing
void initParameter()
{

	proj.left = -1;
	proj.right = 1;
	proj.top = 1;
	proj.bottom = -1;
	proj.nearClip = 1.0;
	proj.farClip = 10.0;
	proj.fovy = 60;

	main_camera.position = Vector3(0.0f, 0.5f, 2.0f);
	main_camera.center = Vector3(0.0f, 0.0f, 0.0f);
	main_camera.up_vector = Vector3(0.0f, 1.0f, 0.0f);

	setViewingMatrix();
	setOrthogonal();	//set default projection matrix as orthogonal matrix
}

void loadConfigFile()
{
	ifstream fin;
	string line;
	fin.open("../../config.txt", ios::in);
	if (fin.is_open())
	{
		while (getline(fin, line))
		{
			filenames.push_back(line);
		}
		fin.close();
	}
	else
	{
		cout << "Unable to open the config file!" << endl;
	}
	for (int i = 0; i < filenames.size(); i++)
		printf("%s\n", filenames[i].c_str());
}

int main(int argc, char **argv)
{
	loadConfigFile();

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);

	// create window
	glutInitWindowPosition(500, 100);
	glutInitWindowSize(800, 800);
	glutCreateWindow("CG HW1");

	glewInit();
	if (glewIsSupported("GL_VERSION_2_0")) {
		printf("Ready for OpenGL 2.0\n");
	}
	else {
		printf("OpenGL 2.0 not supported\n");
		system("pause");
		exit(1);
	}

	initParameter();

	// load obj models through glm
	loadOBJModel();

	// register glut callback functions
	glutDisplayFunc(onDisplay);
	glutIdleFunc(onIdle);
	glutKeyboardFunc(onKeyboard);
	glutSpecialFunc(onKeyboardSpecial);
	glutMouseFunc(onMouse);
	glutMotionFunc(onMouseMotion);
	glutReshapeFunc(onWindowReshape);

	// set up shaders here
	setShaders();

	glEnable(GL_DEPTH_TEST);

	// main loop
	glutMainLoop();

	// delete glm objects before exit
	for (int i = 0; i < filenames.size(); i++)
	{
		glmDelete(models[i].obj);
	}

	return 0;
}

