#pragma once

#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <glm.hpp>
#include <ext.hpp>
#include <gtx/norm.hpp>
#include <GL/glew.h>

#include "texture.h"
#include "Shader.h"

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 uv_coord;
};

class Cloth {

private:
	unsigned int m_vao, m_vbo, m_ibo;
	float m_mass;

	int Nu, Nv;
	std::vector<Vertex> m_vertices;
	std::vector<unsigned int> m_indices;

	std::vector<glm::vec3> m_forces;
	std::vector<glm::vec3> m_speed;

	Texture m_texture;

public:
	Cloth(int width, int height, Shader& program);
	~Cloth();

	bool is_wind;
	float wind_strength;
	glm::vec3 wind_direction;

	void initMesh();
	void updateForces();
	void updatePosition(float const dt);
	void updateNormals();
	void updateBuffer();

	void render();
	unsigned int index(int i, int j);

	Vertex getVertex(int i, int j);
	glm::vec3 getPosition(int i, int j);
	glm::vec3 getNormal(int i, int j);

	void securityCheck();

};