#include "ui/preview/ModelRenderWidget.h"



ModelRenderWidget::ModelRenderWidget(QWidget *parent)
    : QtOpenGLViewer(parent)
{
    setIs3D(true);
}

void
ModelRenderWidget::initializeGL()
{
#ifdef __gl3w_h_
    if (gl3wInit() != GL3W_OK)
    {
        throw std::runtime_error("failed to intialise openGL, this shouldn't happen");
    }
#endif
#ifdef __glew_h__
    if (glewInit() != GLEW_OK)
    {
        throw std::runtime_error("failed to intialise openGL, this shouldn't happen");
    }
#else
    initializeOpenGLFunctions();
#endif
    //turn this on if you add a debug callback function, it's pretty cool
    //glEnable(GL_DEBUG_OUTPUT);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_BACK);
    clearModels();
    loadShaders();
    loadModel();
    loadUniforms();
}

void
ModelRenderWidget::drawScene()
{

    multiDrawScene();
    return;

    drawAxes();
    
    glEnable (GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    
    updateUniform();

    glUseProgram(m_shaderProgram);
    glBindVertexArray(m_glVertexArray);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_glElementBufferObject);

    // CLEARLY this isn't correct
    // Since this is just for previewing, I don't care to get this perfect
    // Divide this by 3 and some faces will be missing
    // Pas in `m_indexCount` directly and faces will draw over each other
    
    //glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indexCount / 2.9), GL_UNSIGNED_SHORT, 0);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indexCount / 2.9), GL_UNSIGNED_SHORT, nullptr);

    glBindVertexArray(0);
    glUseProgram(0);
}

void ModelRenderWidget::multiDrawScene()
{
    drawAxes();

    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);

    updateUniform();

    glUseProgram(m_shaderProgram);

    int testTgt = modelCount - 1;

    for (uint16_t i = 0; i < modelCount; i++)
    {
        if (vertexArrays.size() == 0) break;
        glBindVertexArray(vertexArrays[i]);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffers[i]);
        //need to investigate and fix this at some point, look into how HLODS are defined
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCounts[i] / 2.9), GL_UNSIGNED_SHORT, nullptr);
    }
    
    //if (vertexArrays.size() == modelCount && modelCount >= 1)
    //{
    //    glBindVertexArray(vertexArrays[testTgt]);
    //    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffers[testTgt]);
    //    //need to investigate and fix this at some point, look into how HLODS are defined
    //    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCounts[testTgt]), GL_UNSIGNED_SHORT, nullptr);
    //}
   

    glBindVertexArray(0);
    glUseProgram(0);
}

void
ModelRenderWidget::loadModel(WarframeExporter::Model::ModelBodyInternal& modelInternal)
{
    makeCurrent();

    m_indexCount = modelInternal.indices.size();

    glBindVertexArray(m_glVertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, m_glVertexBufferObject);

    size_t combinedVertSize = (sizeof(float) * 3) + 3;
    std::vector<unsigned char> buf(combinedVertSize * modelInternal.positions.size());

    // Check if model has no/empty vertex colors
    bool fillWithWhite = false;
    if (modelInternal.AO.size() == 0)
        fillWithWhite = true;
    else
    {
        size_t sum = 0;
        for (const uint8_t& x : modelInternal.AO)
            sum += x;
        if (sum == 0)
            fillWithWhite = true;
    }
    
    // Copy Vertex position/color into buffer
    constexpr uint32_t all1 = 0xFFFFFFFF;
    for (size_t i = 0; i < modelInternal.positions.size(); i++)
    {
        size_t bufPtr = i * combinedVertSize;

        memcpy(&buf[0] + bufPtr, &modelInternal.positions[i][0], sizeof(float) * 3);
        bufPtr += sizeof(float) * 3;

        if (fillWithWhite)
            memcpy(&buf[0] + bufPtr, &all1, 3);
        else
        {
            memcpy(&buf[0] + bufPtr++, &modelInternal.AO[i], 1);
            memcpy(&buf[0] + bufPtr++, &modelInternal.AO[i], 1);
            memcpy(&buf[0] + bufPtr++, &modelInternal.AO[i], 1);
        }
    }
    glBufferData(GL_ARRAY_BUFFER, combinedVertSize * modelInternal.positions.size(), buf.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_glElementBufferObject);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint16_t) * modelInternal.indices.size(), modelInternal.indices.data(), GL_STATIC_DRAW);

    // vertex positions
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, combinedVertSize, (void*)0);
    glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, GL_TRUE, combinedVertSize, (void*)(sizeof(float) * 3));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    
    paintGL();
}

void ModelRenderWidget::loadModelForMultipleDraw(WarframeExporter::Model::ModelBodyInternal& modelInternal)
{
    makeCurrent();
    struct Vertex
    {
        glm::vec3 position;
        unsigned char ao[4]{ 0 };

        void fillWithValue(uint8_t val = 0xFF)
        {
            for (uint8_t i = 0; i < 4; i++)
                ao[i] = val;
        }
    };

    constexpr size_t sizeofVertex = sizeof(Vertex);
    alignas(16) std::vector<Vertex> verts;
    verts.resize(modelInternal.positions.size());
    int newCount = modelCount + 1;
    vertexArrays.resize(newCount);
    elementBuffers.resize(newCount);
    indexCounts.resize(newCount);
    vertexBuffers.resize(newCount);

    glGenVertexArrays(1, &vertexArrays[modelCount]);
    glBindVertexArray(vertexArrays[modelCount]);
    glGenBuffers(1, &vertexBuffers[modelCount]);
    glGenBuffers(1, &elementBuffers[modelCount]);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffers[modelCount]);
    constexpr size_t vertSize = sizeof(glm::vec3) + 4;
    constexpr size_t combinedVertSize = (sizeof(float) * 3) + 3;
    std::vector<unsigned char> buf(vertSize * modelInternal.positions.size());

    // Check if model has no/empty vertex colors
    bool fillWithWhite = false;
    if (modelInternal.AO.size() == 0)
        fillWithWhite = true;
    else
    {
        size_t sum = 0;
        for (const uint8_t& x : modelInternal.AO)
            sum += x;
        if (sum == 0)
            fillWithWhite = true;
    }

    for (size_t i = 0; i < modelInternal.positions.size(); i++)
    {
        Vertex vert;
        vert.position = modelInternal.positions[i];
        if (fillWithWhite)
            vert.fillWithValue();
        else
            vert.fillWithValue(modelInternal.AO[i]);
        verts[i] = vert;
    }
    //throw all the data into the buffers we just bound
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffers[modelCount]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint16_t) * modelInternal.indices.size(), modelInternal.indices.data(), GL_STATIC_DRAW);
    indexCounts[modelCount] = modelInternal.indices.size();

    // set all the attributes and enable them
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)(sizeof(glm::vec3)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    modelCount++;
    


}
/*
void ModelRenderWidget::loadModelForMultipleDraw(WarframeExporter::Model::ModelBodyInternal& modelInternal)
{
    //m_glVertexArray
    //m_glVertexBufferObject
    //m_glElementBufferObject
    size_t combinedVertSize = (sizeof(float) * 3) + 3;
    std::vector<unsigned char> buf(combinedVertSize * modelInternal.positions.size());

    // Check if model has no/empty vertex colors
    bool fillWithWhite = false;
    if (modelInternal.AO.size() == 0)
        fillWithWhite = true;
    else
    {
        size_t sum = 0;
        for (const uint8_t& x : modelInternal.AO)
            sum += x;
        if (sum == 0)
            fillWithWhite = true;
    }

    // Copy Vertex position/color into buffer
    constexpr uint32_t all1 = 0xFFFFFFFF;
    for (size_t i = 0; i < modelInternal.positions.size(); i++)
    {
        size_t bufPtr = i * combinedVertSize;
        memcpy(&buf[0] + bufPtr, &modelInternal.positions[i][0], sizeof(float) * 3);
        bufPtr += sizeof(float) * 3;
        if (fillWithWhite)
            memcpy(&buf[0] + bufPtr, &all1, 3);
        else
        {
            memcpy(&buf[0] + bufPtr++, &modelInternal.AO[i], 1);
            memcpy(&buf[0] + bufPtr++, &modelInternal.AO[i], 1);
            memcpy(&buf[0] + bufPtr++, &modelInternal.AO[i], 1);
        }
    }

    vertexBufferOffsets.push_back(vertexBufferHead);
    elementBufferOffsets.push_back(elementBufferHead);
    glBindBuffer(GL_ARRAY_BUFFER, m_glVertexBufferObject);
    glBufferSubData(GL_ARRAY_BUFFER, vertexBufferHead, combinedVertSize * modelInternal.positions.size(), buf.data());
    vertexBufferHead += combinedVertSize * modelInternal.positions.size();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_glElementBufferObject);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, elementBufferHead, sizeof(uint16_t) * modelInternal.indices.size(), modelInternal.indices.data());
    elementBufferHead += sizeof(uint16_t) * modelInternal.indices.size();

    modelCount++;
    

}
*/

void ModelRenderWidget::clearModels()
{
    modelCount = 0;
    vertexArrays.clear();
    vertexBuffers.clear();
    elementBuffers.clear();
    indexCounts.clear();

}

void
ModelRenderWidget::loadShaders()
{
    const char* shaderCode = R"""(#version 420 core
        in vec3 inCol;
        out vec4 FragColor;

        void main()
        {
            FragColor = vec4(inCol, 1.0);
        })""";

    unsigned int shader;

    shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(shader, 1, &shaderCode, NULL);
    glCompileShader(shader);

    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, shader);
    glLinkProgram(m_shaderProgram);

    glDeleteShader(shader);
    
    const char* vertexShaderCode = R"""(#version 420 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aCol;

        out vec3 inCol;

        layout(binding = 0) uniform MatrixBlock
        {
            mat4 projection;
            mat4 modelview;
        };
        
        void main()
        {
            inCol = aCol;
            gl_Position = projection * modelview * vec4(aPos, 1.0);
        })""";

    unsigned int vertexShader;

    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderCode, NULL);
    glCompileShader(vertexShader);

    glAttachShader(m_shaderProgram, vertexShader);
    glLinkProgram(m_shaderProgram);

    glDeleteShader(vertexShader);
}

void
ModelRenderWidget::loadModel()
{
    //glGenVertexArrays(1, &m_glVertexArray);
    //glGenBuffers(1, &m_glVertexBufferObject);
    //glBindBuffer(GL_ARRAY_BUFFER, m_glVertexBufferObject);
    //glBufferData(GL_ARRAY_BUFFER, (10000000 * sizeof(glm::vec3)), 0, GL_STATIC_DRAW);
    //glGenBuffers(1, &m_glElementBufferObject);
    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_glElementBufferObject);
    //glBufferData(GL_ELEMENT_ARRAY_BUFFER, (30000000 * sizeof(uint16_t)), 0, GL_STATIC_DRAW);
    
}

void
ModelRenderWidget::loadUniforms()
{
    glUniformBlockBinding(m_shaderProgram, 0, 0);

    glGenBuffers(1, &m_uniformBufferObject);

    glBindBuffer(GL_UNIFORM_BUFFER, m_uniformBufferObject);
    glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glBindBufferRange(GL_UNIFORM_BUFFER, 0, m_uniformBufferObject, 0, 2 * sizeof(glm::mat4));
}

void
ModelRenderWidget::updateUniform()
{
    float projection[16];
    float modelview[16];


    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);


    //modelView = camera.view().toVector4D();

    glBindBuffer(GL_UNIFORM_BUFFER, m_uniformBufferObject);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), projection);
    glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), modelview);
}

