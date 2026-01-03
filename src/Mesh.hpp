#pragma once
#include "GL/glew.h"

class Mesh {
public:
	Mesh();

	void CreateMesh(GLfloat* vertices, unsigned int* indexes, unsigned int numVertex, unsigned int numberIndexes);
	void RenderMesh();
	void ClearMesh();
	~Mesh();
private:
	GLuint VAO, VBO, IBO;
	GLsizei indexCount;
};