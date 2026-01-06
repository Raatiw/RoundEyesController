#pragma once

#include <stddef.h>

void animatedGifSetup();
void animatedGifLoop();
size_t animatedGifFileCount();
bool animatedGifOpenAtIndex(size_t index);
bool animatedGifIsReady();
