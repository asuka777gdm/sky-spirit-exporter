#pragma once
#include "catalog.hpp"
#include <d3d11.h>
namespace spirit {
void loadGameAssets();
void enrichCatalog(Json& catalog);
std::string itemIcon(const Json& row);
void beginIconFrame();
void setIconDevice(ID3D11Device* device);
ID3D11ShaderResourceView* iconTexture(ID3D11Device* device,const std::string& name);
void releaseIcons();
}
