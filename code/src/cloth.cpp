#include "cloth.h"

static glm::vec3 const g(0.0f, -9.81f, 0.0f);

static float K_struct = 30.0f;
static float K_shear = 30.0f;
static float K_bend = 20.0f;

static float K_wind = 30.0f;


static glm::vec3 getSpringForce(float const K, glm::vec3 const& u, float const L_rest)
{
	float const L = glm::l2Norm(u);
	glm::vec3 spring_f = K * (L - L_rest) * u / L;
	return spring_f;
}


Cloth::Cloth(int width, int height, Shader& program) :
	Nu(width), Nv(height), m_forces(Nu*Nv, glm::vec3(0.0f)),
	m_speed(Nu*Nv, glm::vec3(0.0f)), m_mass(1.0f), m_texture("code/res/synthwave.jpg"),
	is_wind(false), wind_strength(0.001f), wind_direction(glm::vec3(0.0f, 0.0f, 1.0f))
{
	assert(Nu > 1 && Nv > 1);

	m_texture.Bind(0);
	program.setUniform1i("u_Texture", 0);

	initMesh();

	glGenVertexArrays(1, &m_vao);
	glBindVertexArray(m_vao);

	std::cout << " vao : " << m_vao << std::endl;

	glGenBuffers(1, &m_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex)*m_vertices.size(), &m_vertices[0], GL_DYNAMIC_DRAW);

	std::cout << " vbo : " << m_vbo << std::endl;

	int offset = 0;

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offset);

	offset += sizeof(glm::vec3);

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offset);

	offset += sizeof(glm::vec3);

	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offset);

	glGenBuffers(1, &m_ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int)*m_indices.size(), &m_indices[0], GL_STATIC_DRAW);

	std::cout << " ibo : " << m_ibo << std::endl;

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

Cloth::~Cloth()
{
	if (m_vao != 0)
	{
		glDeleteVertexArrays(1, &m_vao);
	}

	if (m_vbo != 0)
	{
		glDeleteBuffers(1, &m_vbo);
	}

	if (m_ibo != 0)
	{
		glDeleteBuffers(1, &m_ibo);
	}
}

void Cloth::initMesh()
{
	float step_u = 1.0f / (Nu-1);
	float step_v = 1.0f / (Nv-1);

	for (int j = 0; j < Nv; j++)
	{
		for (int i = 0; i < Nu; i++)
		{
			glm::vec3 position(-0.5f + step_u * (float)i, 0.0f, -0.5f + step_v * (float)j);
			glm::vec3 normal(0.0f);
			glm::vec2 uv(step_u * (float)i, step_v * (float)j);

			Vertex vertex; vertex.position = position; vertex.normal = normal; vertex.uv_coord = uv;
			m_vertices.push_back(vertex);

			if (j < Nv-1 && i < Nu-1)
			{
				// Triangle 1
				m_indices.push_back(index(i, j));
				m_indices.push_back(index(i + 1, j));
				m_indices.push_back(index(i + 1, j + 1));

				// Triangle 2
				m_indices.push_back(index(i + 1, j + 1));
				m_indices.push_back(index(i, j + 1));
				m_indices.push_back(index(i, j));
			}
		}
	}

	updateNormals();

	assert(m_indices.size() == (Nu-1) * (Nv-1) * 2 * 3 && m_vertices.size() == Nu*Nv);
}

void Cloth::updateForces()
{
	assert(m_forces.size() == m_vertices.size());

	int const N_total = Nu * Nv;
	glm::vec3 const g_normalized = g / (float)N_total;

	static float L_rest = 1.0f / (Nu - 1.0f);
	static float L_diag = sqrt(pow(1.0f / (Nu - 1.0f), 2) + pow(1.0f / (Nv - 1.0f), 2));
	static float L_far = 2.0f / (Nu - 1.0f);

	for (int kv = 0; kv < Nv; kv++)
	{
		for (int ku = 0; ku < Nu; ku++)
		{
			glm::vec3 F = glm::vec3(0.0f);
			
			if (ku > 0)
			{
				glm::vec3 u_left = getPosition(ku - 1, kv) - getPosition(ku, kv);
				F += getSpringForce(K_struct, u_left, L_rest);
			}
			if (kv > 0)
			{
				glm::vec3 u_down = getPosition(ku, kv - 1) - getPosition(ku, kv);
				F += getSpringForce(K_struct, u_down, L_rest);
			}
			if (ku < Nu-1)
			{
				glm::vec3 u_right = getPosition(ku + 1, kv) - getPosition(ku, kv);
				F += getSpringForce(K_struct, u_right, L_rest);
			}
			if (kv < Nv - 1)
			{
				glm::vec3 u_up = getPosition(ku, kv + 1) - getPosition(ku, kv);
				F += getSpringForce(K_struct, u_up, L_rest);
			}
			if (ku > 0 && kv > 0)
			{
				glm::vec3 u_down_left = getPosition(ku-1, kv - 1) - getPosition(ku, kv);
				F += getSpringForce(K_shear, u_down_left, L_diag);
			}
			if (ku < Nu-1 && kv < Nv-1)
			{
				glm::vec3 u_up_right = getPosition(ku + 1, kv + 1) - getPosition(ku, kv);
				F += getSpringForce(K_shear, u_up_right, L_diag);
			}
			if (ku > 0 && kv < Nv - 1)
			{
				glm::vec3 u_up_left = getPosition(ku - 1, kv + 1) - getPosition(ku, kv);
				F += getSpringForce(K_shear, u_up_left, L_diag);
			}
			if (ku < Nu-1 && kv > 0)
			{
				glm::vec3 u_down_right = getPosition(ku + 1, kv - 1) - getPosition(ku, kv);
				F += getSpringForce(K_shear, u_down_right, L_diag);
			}
			if (ku > 1)
			{
				glm::vec3 u_far_left = getPosition(ku - 2, kv) - getPosition(ku, kv);
				F += getSpringForce(K_bend, u_far_left, L_far);
			}
			if (kv > 1)
			{
				glm::vec3 u_far_down = getPosition(ku, kv - 2) - getPosition(ku, kv);
				F += getSpringForce(K_bend, u_far_down, L_far);
			}
			if (ku < Nu-2)
			{
				glm::vec3 u_far_right = getPosition(ku + 2, kv) - getPosition(ku, kv);
				F += getSpringForce(K_bend, u_far_right, L_far);
			}
			if (kv < Nv - 2)
			{
				glm::vec3 u_far_up = getPosition(ku, kv + 2) - getPosition(ku, kv);
				F += getSpringForce(K_bend, u_far_up, L_far);
			}

			if (is_wind)
			{
				glm::vec3 normalized_wind_dir;
				if (wind_direction == glm::vec3(0.0f, 0.0f, 0.0f))
					normalized_wind_dir = glm::vec3(0.0f, 0.0f, 0.0f);
				else
					normalized_wind_dir = normalize(wind_direction);
				glm::vec3 normal = getNormal(ku, kv);
				glm::vec3 f_wind = wind_strength * K_wind * glm::dot(normal, normalized_wind_dir) * normal;
				F += f_wind;
			}

			m_forces[index(ku, kv)] = F + g_normalized;
		}
	}
	
	m_forces[index(0, Nv-1)] = glm::vec3(0.0f);
	m_forces[index(Nu-1, Nv-1)] = glm::vec3(0.0f);

	/////
	/////
	///// 
	/////
	/////

	securityCheck();
}

void Cloth::updatePosition(float const dt)
{
	assert(m_forces.size() == m_speed.size());
	assert(m_forces.size() == m_vertices.size());

	float const mu = 0.1f;

	for (int ku = 0; ku < Nu; ku++)
	{
		for (int kv = 0; kv < Nv; kv++)
		{
			glm::vec3 force = m_forces[index(ku, kv)];
			glm::vec3 speed = m_speed[index(ku, kv)];

			speed = (1-mu*dt)*speed + dt * force;
			m_speed[index(ku, kv)] = speed;

			m_vertices[index(ku, kv)].position += speed * dt;

		}
	}

	securityCheck();
	
	updateNormals();

	updateBuffer();
}

void Cloth::updateNormals()
{
	glm::vec3 up;
	glm::vec3 left;

	for (int kv = 0; kv < Nv; kv++)
	{
		for (int ku = 0; ku < Nu; ku++)
		{
			glm::vec3 normal = glm::vec3(0.0f);

			if (ku > 0 && kv > 0)
			{
				up = getPosition(ku - 1, kv) - getPosition(ku, kv);
				left = getPosition(ku, kv - 1) - getPosition(ku, kv);
				normal += glm::cross(up, left);
			}
			if (ku < Nu - 1 && kv < Nv - 1)
			{
				up = getPosition(ku + 1, kv) - getPosition(ku, kv);
				left = getPosition(ku, kv + 1) - getPosition(ku, kv);
				normal += glm::cross(up, left);
			}
			if (ku > 0 && kv < Nv - 1)
			{
				up = getPosition(ku, kv + 1) - getPosition(ku, kv);
				left = getPosition(ku - 1, kv) - getPosition(ku, kv);
				normal += glm::cross(up, left);
			}
			if (ku < Nu - 1 && kv > 0)
			{
				up = getPosition(ku, kv - 1) - getPosition(ku, kv);
				left = getPosition(ku + 1, kv) - getPosition(ku, kv);
				normal += glm::cross(up, left);
			}

			m_vertices[index(ku, kv)].normal = glm::normalize(normal);
		}
	}

	assert(m_vertices.size() == Nu * Nv);
}

void Cloth::updateBuffer()
{
	assert(m_vbo != -1 && m_vbo != 0);

	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Vertex)*m_vertices.size(), &m_vertices[0]);
}

void Cloth::render()
{
	m_texture.Bind(0);

	glBindVertexArray(m_vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
	glDrawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, (void*)0);
}

unsigned int Cloth::index(int i, int j)
{
	assert(i >= 0 && i < Nu && j >= 0 && j < Nv);
	return (unsigned int)(j * Nu + i);
}

Vertex Cloth::getVertex(int i, int j)
{
	assert(i >= 0 && i < Nu && j >= 0 && j < Nv);

	int a = j * Nu + i;

	return m_vertices[a];
}

glm::vec3 Cloth::getPosition(int i, int j)
{
	assert(i >= 0 && i < Nu && j >= 0 && j < Nv);

	int a = j * Nu + i;

	return m_vertices[a].position;
}

glm::vec3 Cloth::getNormal(int i, int j)
{
	assert(i >= 0 && i < Nu && j >= 0 && j < Nv);

	int a = j * Nu + i;

	return m_vertices[a].normal;
}

void Cloth::securityCheck()
{
	// security check for divergence

	static float const LIMIT = 30.0f;

	for (int ku = 0; ku < Nu; ku++)
	{
		for (int kv = 0; kv < Nv; kv++)
		{
			glm::vec3 const& p = getPosition(ku, kv);

			if (glm::l2Norm(p) > LIMIT)
				exit(2);
		}
	}
}
