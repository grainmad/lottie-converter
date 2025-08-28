#include <array>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <zlib.h>
#include <argparse/argparse.hpp>
#include "render_gif.h"

// 使用zlib解压.tgs文件
std::string decompress_tgs(const std::filesystem::path& tgs_file) {
	std::ifstream file(tgs_file, std::ios::binary);
	if (!file.is_open()) {
		throw std::runtime_error("Cannot open .tgs file");
	}
	
	// 读取整个文件
	std::vector<char> compressed_data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();
	
	// zlib解压
	z_stream strm = {};
	strm.avail_in = compressed_data.size();
	strm.next_in = reinterpret_cast<Bytef*>(compressed_data.data());
	
	if (inflateInit2(&strm, 16 + MAX_WBITS) != Z_OK) {
		throw std::runtime_error("Failed to initialize zlib for decompression");
	}
	
	std::string decompressed_data;
	char buffer[4096];
	int ret;
	
	do {
		strm.avail_out = sizeof(buffer);
		strm.next_out = reinterpret_cast<Bytef*>(buffer);
		
		ret = inflate(&strm, Z_NO_FLUSH);
		if (ret == Z_STREAM_ERROR) {
			inflateEnd(&strm);
			throw std::runtime_error("zlib stream error during decompression");
		}
		
		if (ret != Z_OK && ret != Z_STREAM_END) {
			inflateEnd(&strm);
			throw std::runtime_error("zlib decompression failed");
		}
		
		decompressed_data.append(buffer, sizeof(buffer) - strm.avail_out);
	} while (ret != Z_STREAM_END);
	
	inflateEnd(&strm);
	return decompressed_data;
}

void convert_to_gif(
	const std::filesystem::path& file_path,
	const size_t width,
	const size_t height,
	const std::filesystem::path& output,
	double fps,
	const size_t threads_count,
	const uint8_t quality
) {
	std::string lottie_data;
	
	if (file_path.extension() == ".tgs") {
		// 直接解压.tgs文件到内存
		lottie_data = decompress_tgs(file_path);
	} else {
		// 读取.json文件
		std::ifstream input_file(file_path);
		if (!input_file.is_open()) {
			throw std::runtime_error("can not open lottie file");
		}
		lottie_data = std::string((std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>());
		input_file.close();
	}
	
	render_gif(
		lottie_data,
		width,
		height,
		output,
		fps,
		threads_count,
		quality
	);
}

int main(int argc, const char** argv) {
	argparse::ArgumentParser program("lottie_to_gif", "<local-build>");
	program.add_description(
		"Lottie animations (.json) and Telegram stickers (.tgs) to animated GIF converter"
	);
	program.add_epilog(
		"It's open-source project: https://github.com/ed-asriyan/lottie-converter\n"
		"Author: Ed Asriyan <contact.lottie-converter@asriyan.me>"
	);

	program.add_argument("path")
		.required()
		.help("path to .json or .tgs file to convert");

	program.add_argument("-o", "--output")
		.default_value(std::string(""))
		.help("output file path");

	program.add_argument("-w", "--width")
		.default_value(std::string("512"))
		.help("output image width");

	program.add_argument("-h", "--height")
		.default_value(std::string("512"))
		.help("output image height");

	program.add_argument("-f", "--fps")
		.default_value(std::string("50"))
		.help("output frame rate");

	program.add_argument("-t", "--threads")
		.default_value(std::string("0"))
		.help("numbers of threads to use. If 0, number of CPUs is used");

	program.add_argument("-q", "--quality")
		.default_value(std::string("90"))
		.help("output quality (1-100)");

	try {
		program.parse_args(argc, argv);
	} catch (const std::runtime_error& err) {
		std::cout << err.what() << std::endl;
		std::cout << program;
		return -2;
	}

	const auto file_path = std::filesystem::path(program.get<std::string>("path"));
	const auto width = std::stoi(program.get<std::string>("--width"));
	const auto height = std::stoi(program.get<std::string>("--height"));
	const auto fps = std::stod(program.get<std::string>("--fps"));
	auto output_str = program.get<std::string>("--output");
	const auto threads = std::stoi(program.get<std::string>("--threads"));
	const auto quality = std::stoi(program.get<std::string>("--quality"));

	// 如果没有指定输出文件，使用输入文件名加.gif扩展名
	std::filesystem::path output;
	if (output_str.empty()) {
		output = file_path;
		output.replace_extension(".gif");
	} else {
		output = std::filesystem::path(output_str);
	}

	try {
		convert_to_gif(file_path, width, height, output, fps, threads, quality);
		std::cout << "GIF saved to: " << output << std::endl;
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return -1;
	}

	return 0;
}