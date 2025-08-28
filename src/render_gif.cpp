#include "render_gif.h"
#include <mutex>
#include <atomic>

#define lp_COLOR_DEPTH 8
#define lp_COLOR_BYTES 4

void render_gif(
	const std::string& lottie_data,
	const size_t width,
	const size_t height,
	const std::filesystem::path& output_file,
	double fps,
	size_t threads_count,
	uint8_t quality
) {
	static unsigned int cache_counter = 0;
	const auto cache_counter_str = std::to_string(++cache_counter);
	
	auto player = rlottie::Animation::loadFromData(lottie_data, cache_counter_str);
	if (!player) throw std::runtime_error("can not load lottie animation");
	
	const size_t player_frame_count = player->totalFrame();
	const double player_fps = player->frameRate();
	if (fps == 0.0) fps = player_fps;
	
	const double duration = player_frame_count / (double)player_fps;
	const double step = player_fps / fps;
	const size_t output_frame_count = static_cast<size_t>(fps * duration);
	
	if (threads_count == 0) {
		threads_count = std::thread::hardware_concurrency();
	}
	
	// 初始化gifski
	GifskiSettings settings = {
		.width = static_cast<uint32_t>(width),
		.height = static_cast<uint32_t>(height),
		.quality = quality,
		.fast = false,
		.repeat = 0  // 0表示无限循环
	};
	
	gifski* g = gifski_new(&settings);
	if (!g) {
		throw std::runtime_error("Failed to create gifski encoder");
	}
	
	// 设置输出文件
	GifskiError err = gifski_set_file_output(g, output_file.string().c_str());
	if (err != GIFSKI_OK) {
		gifski_finish(g);
		throw std::runtime_error("Failed to set output file: " + std::to_string(err));
	}
	
	// 创建线程来渲染帧并添加到gifski
	std::vector<std::thread> threads;
	std::mutex gifski_mutex;  // 保护gifski_add_frame_rgba调用
	std::atomic<bool> error_occurred(false);
	std::string error_message;
	
	for (size_t i = 0; i < threads_count; ++i) {
		threads.emplace_back([i, output_frame_count, step, width, height, threads_count, &lottie_data, cache_counter_str, g, &gifski_mutex, &error_occurred, &error_message, fps]() {
			try {
				auto local_player = rlottie::Animation::loadFromData(lottie_data, cache_counter_str);
				if (!local_player) {
					error_occurred = true;
					error_message = "Failed to load animation in thread";
					return;
				}
				
				uint32_t* const buffer = new uint32_t[width * height];
				
				for (size_t j = i; j < output_frame_count && !error_occurred; j += threads_count) {
					rlottie::Surface surface(buffer, width, height, width * lp_COLOR_BYTES);
					local_player->renderSync(static_cast<size_t>(round(j * step)), surface);
					
					// 处理BGRA到RGBA的转换和透明度预乘
					unsigned char* pixel_data = reinterpret_cast<unsigned char*>(buffer);
					size_t total_bytes = width * height * lp_COLOR_BYTES;
					for (size_t k = 0; k < total_bytes; k += lp_COLOR_BYTES) {
						// rlottie输出的是BGRA格式，需要转换为RGBA
						unsigned char b = pixel_data[k];
						unsigned char g = pixel_data[k + 1];
						unsigned char r = pixel_data[k + 2];
						unsigned char a = pixel_data[k + 3];
						
						// 处理透明度预乘 (rlottie已经预乘了，需要反预乘)
						if (a != 0 && a != 255) {
							r = (r * 255) / a;
							g = (g * 255) / a;
							b = (b * 255) / a;
						}
						
						// 转换为RGBA格式
						pixel_data[k] = r;
						pixel_data[k + 1] = g;
						pixel_data[k + 2] = b;
						pixel_data[k + 3] = a;
					}
					
					// 计算时间戳
					double timestamp = j / fps;
					
					// 线程安全地添加帧到gifski
					{
						std::lock_guard<std::mutex> lock(gifski_mutex);
						if (!error_occurred) {
							GifskiError frame_err = gifski_add_frame_rgba(
								g,
								static_cast<uint32_t>(j),
								static_cast<uint32_t>(width),
								static_cast<uint32_t>(height),
								pixel_data,
								timestamp
							);
							if (frame_err != GIFSKI_OK) {
								error_occurred = true;
								error_message = "Failed to add frame: " + std::to_string(frame_err);
							}
						}
					}
				}
				
				delete[] buffer;
			} catch (const std::exception& e) {
				error_occurred = true;
				error_message = e.what();
			}
		});
	}
	
	// 等待所有线程完成
	for (auto& thread : threads) {
		if (thread.joinable()) {
			thread.join();
		}
	}
	
	// 检查是否有错误发生
	if (error_occurred) {
		gifski_finish(g);
		throw std::runtime_error("Error during rendering: " + error_message);
	}
	
	// 完成编码
	err = gifski_finish(g);
	if (err != GIFSKI_OK) {
		throw std::runtime_error("Failed to finish encoding: " + std::to_string(err));
	}
}