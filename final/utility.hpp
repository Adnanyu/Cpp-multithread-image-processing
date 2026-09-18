#include <fstream>
#include <png.h>
#include <filesystem>
#include <mutex>
#include <cstring>


namespace fs = std::filesystem;

struct ImageData {
    int width;
    int height;
    png_byte color_type;
    png_byte bit_depth;
    png_bytep* row_pointers;
    std::mutex read_write_mutex;
};




void read_png_file(const std::string& filename, ImageData& imageData);

void write_png_file(const std::string& filename, ImageData& imageData);

png_bytep calculate_average(png_bytep row, int x, int y, int channels, ImageData& imageData);

void apply_blur(int start_row, int end_row, ImageData& imageData);

void process_png_file_multithreaded(const std::string& input_path, const std::string& output_path, int num_threads_per_image);

void apply_filter_on_images(const std::vector<std::string>& image_paths, const std::string& output_folder, int numb_threads_of_folder, int num_threads_per_image);