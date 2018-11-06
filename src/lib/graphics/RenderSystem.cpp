#include <Precompiled.hpp>
#include <graphics/RenderSystem.hpp>

#include <component/Query.hpp>

#include <scene/Hovered.hpp>
#include <scene/Scene.hpp>
#include <scene/Selected.hpp>
#include <scene/Transform.hpp>
#include <ui/Window.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

#include <imgui.h>
#include <imgui_impl_opengl3.h>

using namespace eng;
using namespace eng::gfx;

RenderSystem::RenderSystem(Database& db) :
    m_meshTable(db.createTable<Mesh>())
{
    m_shaders.emplace_back(Shader(
        "../shaders/vertex.vert",
        "../shaders/fragment.frag"));
    m_shaders.emplace_back(Shader(
        "../shaders/vertex.vert",
        "../shaders/fragment_single.frag"));
    m_shaders.emplace_back(Shader(
        "../shaders/vertex.vert",
        "../shaders/fragment_single.frag"));

    m_textures.emplace_back(Texture(
        "../data/container.jpg", GL_CLAMP_TO_EDGE, GL_NEAREST, GL_RGB));
    m_textures.emplace_back(Texture(
        "../data/awesomeface.png", GL_REPEAT, GL_NEAREST, GL_RGBA));
}

RenderSystem::~RenderSystem()
{
}

void RenderSystem::update(const Scene&)
{
    query()
        .hasComponent<Added>()
        .hasComponent<Mesh>(m_meshTable)
        .execute([&](
            EntityId id,
            const Added&,
            Mesh& mesh)
    {
        // Generate and bind vertex array object
        glGenVertexArrays(1, &mesh.VAO);
        glBindVertexArray(mesh.VAO);
        
        // Generate buffers
        glGenBuffers(1, &mesh.VBOV);
        glGenBuffers(1, &mesh.VBOC);
        glGenBuffers(1, &mesh.EBO);

        // Bind vertex buffer object for vertices
        glBindBuffer(GL_ARRAY_BUFFER, mesh.VBOV);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * mesh.vertices.size(), &mesh.vertices[0], GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*) 0);
        glEnableVertexAttribArray(0);

        // Bind vertex buffer object for vertex colors
        glBindBuffer(GL_ARRAY_BUFFER, mesh.VBOC);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * mesh.colors.size(), &mesh.colors[0], GL_STATIC_DRAW);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*) 0);
        glEnableVertexAttribArray(1);

        // Bind element buffer object for vertex indices
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned) * mesh.indices.size(), &mesh.indices[0], GL_STATIC_DRAW);
    });
        
    query()
        .hasComponent<Updated>()
        .hasComponent<Transform>()
        .hasComponent<Mesh>(m_meshTable)
        .execute([&](
            EntityId id,
            const Updated&,
            const Transform& transform,
            Mesh& mesh)
    {
        // Compute axis-aligned bounding box for updated rotation and scale,
        // so that it fully contains the mesh in any orientation
        {
            // TODO: Sooo... we might not want to orientate the AABB at all after its 
            // initially constructed from the mesh vertices, but instead use and expose
            // the OBB as the bounding volume for the mesh.

            mesh.aabb.clear();

            mat4 rotate = glm::mat4_cast(transform.rotation);
            mat4 scale = glm::scale(mat4(1.0f), transform.scale);
            mat4 mat = rotate * scale;

            for (auto& v : mesh.vertices)
            {
                vec4 p = mat * vec4(v, 1.0f);
                // p /= p.w; // Always 1.0 since we ignore position

                mesh.aabb.expand(p);
            }
        }

        {
            // ALTERNATIVE:
            //vec4 xa = model[0] * mesh.aabb.min().x;
            //vec4 xb = model[0] * mesh.aabb.max().x;

            //vec4 ya = model[1] * mesh.aabb.min().y;
            //vec4 yb = model[1] * mesh.aabb.max().y;

            //vec4 za = model[2] * mesh.aabb.min().z;
            //vec4 zb = model[2] * mesh.aabb.max().z;

            //vec4 min = glm::min(xa, xb) + glm::min(ya, yb) + glm::min(za, zb) + model[3];
            //min /= min.w;

            //vec4 max = glm::max(xa, xb) + glm::max(ya, yb) + glm::max(za, zb) + model[3];
            //max /= max.w;

            //mesh.aabb.clear();
            //mesh.aabb.expand(min);
            //mesh.aabb.expand(max);
        }

        // Compute object oriented bounding box
        {
            // NOTE: Use untransformed AABB so mesh rotation doesn't affect extents
            AABB aabb;
            for (auto& v : mesh.vertices)
            {
                aabb.expand(v);
            }

            mat4 scale = glm::scale(mat4(1.0f), transform.scale);

            mesh.obb.halfExtents = scale * vec4(aabb.halfExtents(), 1.0f);

            // Don't include scale in model matrix, because scale should only affect
            // the OBB's half extents
            mat4 translate = glm::translate(mat4(1.0f), transform.position);
            mat4 rotate = glm::mat4_cast(transform.rotation);
            mat4 model = translate * rotate;

            vec4 pos = model * vec4(aabb.center(), 1.0f);
            pos /= pos.w;

            mesh.obb.position = vec3(pos);
            mesh.obb.rotation = mat3(model);
        }
    });

    query()
        .hasComponent<Deleted>()
        .hasComponent<Mesh>(m_meshTable)
        .execute([&](
            EntityId id, 
            const Deleted&,
            Mesh& mesh)
    {
        glDeleteVertexArrays(1, &mesh.VAO);
        glDeleteBuffers(1, &mesh.VBOV);
        glDeleteBuffers(1, &mesh.VBOC);
        glDeleteBuffers(1, &mesh.EBO);
    });
}

void RenderSystem::beginFrame()
{
    glEnable(GL_DEPTH_TEST);
    //glEnable(GL_STENCIL_TEST);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void RenderSystem::render()
{
    auto camera = query().find<Camera>();
    assert(camera != nullptr && "No camera in scene");

    // Draw meshes
    query()
        .hasComponent<Transform>()
        .hasComponent<Mesh>()
        .execute([&](
            EntityId id, 
            const Transform& transform,
            const Mesh& mesh)
    {
        glEnable(GL_STENCIL_TEST);

        // Write to stencil buffer
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilMask(0xFF);

        m_shaders[0].use();
        m_shaders[0].setMat4("view", camera->viewMatrix);
        m_shaders[0].setMat4("projection", camera->projectionMatrix);
        m_shaders[0].setMat4("model", transform.modelMatrix());

        glBindVertexArray(mesh.VAO);
        glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glDisable(GL_STENCIL_TEST);
    });

    // Draw meshes' mouse hover outline
    query()
        .hasComponent<Transform>()
        .hasComponent<Mesh>()
        .hasComponent<Hovered>()
        .execute([&](
            EntityId id,
            const Transform& transform,
            const Mesh& mesh,
            const Hovered&)
    {
        glEnable(GL_STENCIL_TEST);
        glDisable(GL_DEPTH_TEST);

        // Disable writing to the stencil buffer
        glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
        glStencilMask(0x00);
        
        m_shaders[1].use();
        m_shaders[1].setMat4("view", camera->viewMatrix);
        m_shaders[1].setMat4("projection", camera->projectionMatrix);
        m_shaders[1].setMat4("model", transform.modelMatrix());
        m_shaders[1].setVec3("color", vec3(0.2f, 1.0f, 0.4f));

        glBindVertexArray(mesh.VAO);
        glLineWidth(4.0f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDrawElements(GL_LINE_STRIP, mesh.indices.size(), GL_UNSIGNED_INT, 0);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glLineWidth(1.0f);
        glBindVertexArray(0);

        glStencilMask(0xFF);

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
    });
    
    // Draw AABBs
    query()
        .hasComponent<Transform>()
        .hasComponent<Mesh>()
        .execute([&](
            EntityId id,
            const Transform& transform,
            const Mesh& mesh)
    {
        auto min = mesh.aabb.min();
        auto max = mesh.aabb.max();

        auto vertices = std::vector<vec3>
        {
            vec3(min.x, min.y, min.z),
            vec3(max.x, min.y, min.z),

            vec3(max.x, min.y, min.z),
            vec3(max.x, max.y, min.z),

            vec3(max.x, max.y, min.z),
            vec3(min.x, max.y, min.z),

            vec3(min.x, max.y, min.z),
            vec3(min.x, min.y, min.z),

            vec3(min.x, min.y, min.z),
            vec3(min.x, min.y, max.z),

            vec3(min.x, min.y, max.z),
            vec3(max.x, min.y, max.z),

            vec3(max.x, min.y, max.z),
            vec3(max.x, max.y, max.z),

            vec3(max.x, max.y, max.z),
            vec3(min.x, max.y, max.z),

            vec3(min.x, max.y, max.z),
            vec3(min.x, max.y, min.z),

            vec3(min.x, max.y, min.z),
            vec3(min.x, min.y, min.z),

            vec3(min.x, min.y, min.z),
            vec3(min.x, min.y, max.z),

            vec3(min.x, min.y, max.z),
            vec3(min.x, max.y, max.z),

            vec3(min.x, max.y, max.z),
            vec3(max.x, max.y, max.z),

            vec3(max.x, max.y, max.z),
            vec3(max.x, max.y, min.z),

            vec3(max.x, max.y, min.z),
            vec3(max.x, min.y, min.z),

            vec3(max.x, min.y, min.z),
            vec3(max.x, min.y, max.z)
        };

        unsigned int VAO;
        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);

        unsigned int VBO;
        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * vertices.size(), &vertices[0], GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*) 0);
        glEnableVertexAttribArray(0);

        mat4 translate = glm::translate(mat4(1.0f), transform.position);
        mat4 rotate = mat4(1.0f);
        mat4 scale = glm::scale(mat4(1.0f), vec3(1.0f));
        mat4 model = translate * rotate * scale;

        m_shaders[2].use();
        m_shaders[2].setMat4("view", camera->viewMatrix);
        m_shaders[2].setMat4("projection", camera->projectionMatrix);
        m_shaders[2].setMat4("model", model);
        m_shaders[2].setVec3("color", vec3(0.6f, 0.7f, 0.9f));

        glBindVertexArray(VAO);
        glDrawArrays(GL_LINES, 0, vertices.size());
        glBindVertexArray(0);
    });

    // Draw OBBs
    query()
        .hasComponent<Transform>()
        .hasComponent<Mesh>()
        .execute([&](
            EntityId id,
            const Transform& transform,
            const Mesh& mesh)
    {
        vec3 pos = mesh.obb.position;
        vec3 ext = mesh.obb.halfExtents;
        mat3 rot = mesh.obb.rotation;
        vec3 min = -ext;
        vec3 max = +ext;

        auto vertices = std::vector<vec3>
        {
            vec3(min.x, min.y, min.z),
            vec3(max.x, min.y, min.z),

            vec3(max.x, min.y, min.z),
            vec3(max.x, max.y, min.z),

            vec3(max.x, max.y, min.z),
            vec3(min.x, max.y, min.z),

            vec3(min.x, max.y, min.z),
            vec3(min.x, min.y, min.z),

            vec3(min.x, min.y, min.z),
            vec3(min.x, min.y, max.z),

            vec3(min.x, min.y, max.z),
            vec3(max.x, min.y, max.z),

            vec3(max.x, min.y, max.z),
            vec3(max.x, max.y, max.z),

            vec3(max.x, max.y, max.z),
            vec3(min.x, max.y, max.z),

            vec3(min.x, max.y, max.z),
            vec3(min.x, max.y, min.z),

            vec3(min.x, max.y, min.z),
            vec3(min.x, min.y, min.z),

            vec3(min.x, min.y, min.z),
            vec3(min.x, min.y, max.z),

            vec3(min.x, min.y, max.z),
            vec3(min.x, max.y, max.z),

            vec3(min.x, max.y, max.z),
            vec3(max.x, max.y, max.z),

            vec3(max.x, max.y, max.z),
            vec3(max.x, max.y, min.z),

            vec3(max.x, max.y, min.z),
            vec3(max.x, min.y, min.z),

            vec3(max.x, min.y, min.z),
            vec3(max.x, min.y, max.z)
        };

        unsigned int VAO;
        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);

        unsigned int VBO;
        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * vertices.size(), &vertices[0], GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*) 0);
        glEnableVertexAttribArray(0);

        mat4 translate = glm::translate(mat4(1.0f), pos);
        mat4 rotate = mat4(rot);
        //mat4 rotate = mat4(
        //    vec4(rot[0],  0),
        //    vec4(rot[1],  0),
        //    vec4(rot[2],  0),
        //    vec4(0, 0, 0, 1));
        //rotate = mat4(1);
        mat4 scale = glm::scale(mat4(1.0f), vec3(1.0f));
        mat4 model = translate * rotate * scale;

        m_shaders[2].use();
        m_shaders[2].setMat4("view", camera->viewMatrix);
        m_shaders[2].setMat4("projection", camera->projectionMatrix);
        m_shaders[2].setMat4("model", model);
        m_shaders[2].setVec3("color", vec3(1.0f, 0.9f, 0.3f));

        glBindVertexArray(VAO);
        glDrawArrays(GL_LINES, 0, vertices.size());
        glBindVertexArray(0);
    });
}

void RenderSystem::endFrame()
{
}