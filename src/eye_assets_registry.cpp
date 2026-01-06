#include "eye_assets.h"

extern const EyeAsset kEyeDefault;
extern const EyeAsset kEyeBig;
extern const EyeAsset kEyeDragon;
extern const EyeAsset kEyeGoat;

namespace
{
const EyeAsset *const kEyeAssets[] = {
    &kEyeDefault,
    &kEyeBig,
    &kEyeDragon,
    &kEyeGoat};
}

const EyeAsset *activeEye = kEyeAssets[0];

size_t eyeAssetCount()
{
  return sizeof(kEyeAssets) / sizeof(kEyeAssets[0]);
}

const EyeAsset *getEyeAsset(size_t index)
{
  if (index >= eyeAssetCount())
  {
    return nullptr;
  }

  return kEyeAssets[index];
}
