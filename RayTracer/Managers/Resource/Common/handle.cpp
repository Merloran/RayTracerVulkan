#include "handle.hpp"
#include "texture.hpp"
#include "mesh.hpp"
#include "model.hpp"
#include "material.hpp"

const Handle<Model> Handle<Model>::sNone = { -1 };
const Handle<Mesh> Handle<Mesh>::sNone = { -1 };
const Handle<Material> Handle<Material>::sNone = { -1 };
const Handle<Texture> Handle<Texture>::sNone = { -1 };