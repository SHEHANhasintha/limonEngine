//
// Created by engin on 14.09.2016.
//

#include "MeshAsset.h"


MeshAsset::MeshAsset(AssetManager *assetManager, const aiMesh *currentMesh, const Material *material,
                     const BoneNode *meshSkeleton, const glm::mat4 &parentTransform, const bool isPartOfAnimated)
        : material(
        material), parentTransform(parentTransform), isPartOfAnimated(isPartOfAnimated) {
    triangleCount = currentMesh->mNumFaces;
    if (!currentMesh->HasPositions()) {
        throw "No position found"; //Not going to process if mesh is empty
    }

    vertexCount = currentMesh->mNumVertices;
    setTriangles(currentMesh);

    uint_fast32_t vbo;
    assetManager->getGlHelper()->bufferVertexData(vertices, faces, vao, vbo, 2, ebo);
    bufferObjects.push_back(vbo);

    assetManager->getGlHelper()->bufferNormalData(normals, vao, vbo, 4);
    bufferObjects.push_back(vbo);

    if (!textureCoordinates.empty()) {
        assetManager->getGlHelper()->bufferVertexTextureCoordinates(textureCoordinates, vao, vbo, 3);
        bufferObjects.push_back(vbo);
    }

    //loadBoneInformation
    if (currentMesh->HasBones()) {
        this->bones = true;

        assert(meshSkeleton != NULL);
        this->skeleton = meshSkeleton;
        //we will create a bone* vector so we can load the weight information to GPU. The same vector should be
        //copied and filled with animation information on Model class
        //an a name->bone* map, for animation.
        fillBoneMap(this->skeleton);

        boneIDs.resize(vertices.size());
        boneWeights.resize(vertices.size());
        for (unsigned int j = 0; j < currentMesh->mNumBones; ++j) {
            uint_fast32_t boneID = boneIdMap[currentMesh->mBones[j]->mName.C_Str()];
            /*
             * Assimp has a bone array with weight lists for vertices,
             * we need a vertex array with weight list for bones.
             * This loop should generate that
             */
            for (uint_fast32_t k = 0; k < currentMesh->mBones[j]->mNumWeights; ++k) {
                if(currentMesh->mBones[j]->mWeights[k].mWeight > 0.0f) {
                    addWeightToVertex(boneID, currentMesh->mBones[j]->mWeights[k].mVertexId,
                                      currentMesh->mBones[j]->mWeights[k].mWeight);
                }
            }


        }
        std::cout << "Animation added for mesh" << std::endl;

        assetManager->getGlHelper()->bufferExtraVertexData(boneIDs, vao, vbo, 5);
        bufferObjects.push_back(vbo);

        assetManager->getGlHelper()->bufferExtraVertexData(boneWeights, vao, vbo, 6);
        bufferObjects.push_back(vbo);
    } else {
        this->bones = false;
    }
}

void MeshAsset::setTriangles(const aiMesh *currentMesh) {
    //In this part, the "if"s can be put in for, but then we will check them for each iteration. I am
    // not sure if that creates enough performance difference, it can be checked.
    if(isPartOfAnimated) {
        if (currentMesh->HasTextureCoords(0)) {
            for (int j = 0; j < currentMesh->mNumVertices; ++j) {
                vertices.push_back(glm::vec4(GLMConverter::AssimpToGLM(currentMesh->mVertices[j]), 1.0f));
                normals.push_back(glm::vec4(GLMConverter::AssimpToGLM(currentMesh->mNormals[j]), 1.0f));
                textureCoordinates.push_back(
                        glm::vec2(currentMesh->mTextureCoords[0][j].x, currentMesh->mTextureCoords[0][j].y));

            }
        } else {
            for (int j = 0; j < currentMesh->mNumVertices; ++j) {
                vertices.push_back(glm::vec4(GLMConverter::AssimpToGLM(currentMesh->mVertices[j]), 1.0f));
                normals.push_back(glm::vec4(GLMConverter::AssimpToGLM(currentMesh->mNormals[j]), 1.0f));
            }
        }
    } else {

        if (currentMesh->HasTextureCoords(0)) {
            for (int j = 0; j < currentMesh->mNumVertices; ++j) {
                vertices.push_back(parentTransform * glm::vec4(GLMConverter::AssimpToGLM(currentMesh->mVertices[j]), 1.0f));
                normals.push_back(parentTransform * glm::vec4(GLMConverter::AssimpToGLM(currentMesh->mNormals[j]), 1.0f));
                textureCoordinates.push_back(
                        glm::vec2(currentMesh->mTextureCoords[0][j].x, currentMesh->mTextureCoords[0][j].y));

            }
        } else {
            for (int j = 0; j < currentMesh->mNumVertices; ++j) {
                vertices.push_back(parentTransform * glm::vec4(GLMConverter::AssimpToGLM(currentMesh->mVertices[j]), 1.0f));
                normals.push_back(parentTransform * glm::vec4(GLMConverter::AssimpToGLM(currentMesh->mNormals[j]), 1.0f));
            }
        }
    }

    for (int j = 0; j < currentMesh->mNumFaces; ++j) {
        faces.push_back(glm::vec3(currentMesh->mFaces[j].mIndices[0],
                                  currentMesh->mFaces[j].mIndices[1],
                                  currentMesh->mFaces[j].mIndices[2]));
    }

}

bool MeshAsset::addWeightToVertex(uint_fast32_t boneID, unsigned int vertex, float weight) {
    //the weights are suppose to be ordered,
    for (int i = 0; i < 4; ++i) { //we allow only 4 bones per vertex
        if (boneWeights[vertex][i] < weight) {
            //shift to open slot
            if(boneWeights[vertex][i] > 0.0f) {
                for (int j = 3; j > i; --j) {
                    boneWeights[vertex][j] = boneWeights[vertex][j - 1];
                    boneIDs[vertex][j] = boneIDs[vertex][j - 1];
                }
            }
            boneWeights[vertex][i] = weight;
            boneIDs[vertex][i] = boneID;
            return true;
        }
    }
    return false;
}

btTriangleMesh * MeshAsset::getBulletMesh() {
    //Turns out bullet shapes does not copy meshes, so we should return a copy, not the original;
    btTriangleMesh *copyMesh = new btTriangleMesh();

    //FIXME since we don't implement animated physics, this is not used, but is should be
    /*
    if(isPartOfAnimated) {
        for (int j = 0; j < faces.size(); ++j) {
            copyMesh->addTriangle(GLMConverter::GLMToBlt(glm::vec4(vertices[faces[j][0]], 1.0f)),
                                  GLMConverter::GLMToBlt(glm::vec4(vertices[faces[j][1]], 1.0f)),
                                  GLMConverter::GLMToBlt(glm::vec4(vertices[faces[j][2]], 1.0f)));
        }
    */
    for (int j = 0; j < faces.size(); ++j) {
        copyMesh->addTriangle(GLMConverter::GLMToBlt(parentTransform * glm::vec4(vertices[faces[j][0]], 1.0f)),
                              GLMConverter::GLMToBlt(parentTransform * glm::vec4(vertices[faces[j][1]], 1.0f)),
                              GLMConverter::GLMToBlt(parentTransform * glm::vec4(vertices[faces[j][2]], 1.0f)));
    }
    return copyMesh;
    shapeCopies.push_back(copyMesh);
}

bool MeshAsset::hasBones() const {
    return bones;
}

void MeshAsset::fillBoneMap(const BoneNode *boneNode) {
    if (boneNode == NULL) {
        return;
    }
    boneIdMap[boneNode->name] = boneNode->boneID;
    for (int i = 0; i < boneNode->children.size(); ++i) {
        fillBoneMap(boneNode->children[i]);
    }
}


