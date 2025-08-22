#pragma once
//#include "GL/gl3w.h"
#include "model/ModelExporterGltf.h"

#include "QtOpenGLViewer.h"

class ModelRenderWidget : public QtOpenGLViewer
{
    GLuint m_glVertexArray            = 0;
    GLuint m_glVertexBufferObject     = 0;
    GLuint m_glElementBufferObject    = 0;
    GLuint m_shaderProgram            = 0;
    GLuint m_uniformBufferObject      = 0;
    size_t m_indexCount               = 0;

    std::vector<GLuint> vertexArrays;
    std::vector<GLuint> vertexBuffers;
    std::vector<GLuint> elementBuffers; //index buffers
    std::vector<size_t> indexCounts;

    uint16_t modelCount;

    size_t vertexBufferHead = 0;
    std::vector<size_t> vertexBufferOffsets;
    size_t elementBufferHead = 0;
    std::vector<size_t> elementBufferOffsets;

    


public:
    ModelRenderWidget(QWidget *parent = NULL);

    void initializeGL() override;
    void drawScene() override;
    void multiDrawScene();

    void loadModel(WarframeExporter::Model::ModelBodyInternal& modelInternal);
    void loadModelForMultipleDraw(WarframeExporter::Model::ModelBodyInternal& model);
    void clearModels();

private:
    void loadShaders();
    void loadModel();
    void loadUniforms();
    

    void updateUniform();
};