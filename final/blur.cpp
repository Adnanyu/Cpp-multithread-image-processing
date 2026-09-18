#include <iostream>
#include <fstream>
#include <png.h>
#include <thread>
#include <vector>
#include <filesystem>
#include <mutex>
#include <cstring>
#include <atomic>
#include "utility.hpp"



int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <folder_path> <output_folder_path> <numb_threads_of_folder> <num_threads_per_image>" << std::endl;
        return 1;
    }

    std::string folder_path = argv[1];
    std::string output_folder = argv[2];
    int numb_threads_of_folder = std::stoi(argv[3]);
    int num_threads_per_image = std::stoi(argv[4]);

    std::vector<std::string> image_paths;
    for (const auto& entry : fs::directory_iterator(folder_path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".png") {
            image_paths.push_back(entry.path().string());
        }
    }

    apply_filter_on_images(image_paths, output_folder, numb_threads_of_folder, num_threads_per_image);

    return 0;
}