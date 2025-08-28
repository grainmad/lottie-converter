#ifndef LOTTIE_TO_GIF_RENDER_GIF_H
#define LOTTIE_TO_GIF_RENDER_GIF_H

#include <cmath>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>
#include <rlottie.h>

extern "C" {
#include "../libgifski/gifski.h"
}

void render_gif(
	const std::string& lottieData,
	const size_t width,
	const size_t height,
	const std::filesystem::path& output_file,
	double fps = 50,
	size_t threads_count = 0,
	uint8_t quality = 90
);

#endif //LOTTIE_TO_GIF_RENDER_GIF_H