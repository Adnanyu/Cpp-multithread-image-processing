#include <iostream>
#include <fstream>
#include <png.h>
#include <thread>
#include <vector>
#include <filesystem>
#include <mutex>
#include <cstring>
#include <atomic>


namespace fs = std::filesystem;

struct ImageData {
    int width;
    int height;
    png_byte color_type;
    png_byte bit_depth;
    png_bytep* row_pointers;
    std::mutex read_write_mutex;
};



void read_png_file(const std::string& filename, ImageData& imageData) {
    FILE* fp = fopen(filename.c_str(), "rb");
    if (!fp) {
        std::cerr << "Failed to open file " << filename << std::endl;
        return;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        std::cerr << "Failed to create PNG read struct" << std::endl;
        fclose(fp);
        return;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        std::cerr << "Failed to create PNG info struct" << std::endl;
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        return;
    }

    if (setjmp(png_jmpbuf(png))) {
        std::cerr << "Error during PNG read initialization" << std::endl;
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    imageData.width = png_get_image_width(png, info);
    imageData.height = png_get_image_height(png, info);
    imageData.color_type = png_get_color_type(png, info);
    imageData.bit_depth = png_get_bit_depth(png, info);

    if (imageData.bit_depth == 16)
        png_set_strip_16(png);

    if (imageData.color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    if (imageData.color_type == PNG_COLOR_TYPE_GRAY && imageData.bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    if (imageData.color_type == PNG_COLOR_TYPE_RGB ||
        imageData.color_type == PNG_COLOR_TYPE_GRAY ||
        imageData.color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

    if (imageData.color_type == PNG_COLOR_TYPE_GRAY ||
        imageData.color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    if (imageData.row_pointers) {
        std::cerr << "Row pointers are already allocated" << std::endl;
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return;
    }

    imageData.row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * imageData.height);
    if (!imageData.row_pointers) {
        std::cerr << "Failed to allocate memory for row pointers" << std::endl;
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return;
    }

    for (int y = 0; y < imageData.height; y++) {
        imageData.row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png, info));
        if (!imageData.row_pointers[y]) {
            std::cerr << "Failed to allocate memory for row" << y << std::endl;
            for (int j = 0; j < y; j++) {
                free(imageData.row_pointers[j]);
            }
            free(imageData.row_pointers);
            png_destroy_read_struct(&png, &info, NULL);
            fclose(fp);
            return;
        }
    }

    png_read_image(png, imageData.row_pointers);
    fclose(fp);

    png_destroy_read_struct(&png, &info, NULL);
}

void write_png_file(const std::string& filename, ImageData& imageData) {
    FILE* fp = fopen(filename.c_str(), "wb");
    if (!fp) {
        std::cerr << "Failed to open file for writing: " << filename << std::endl;
        return;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        std::cerr << "Failed to create PNG write struct" << std::endl;
        fclose(fp);
        return;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        std::cerr << "Failed to create PNG info struct" << std::endl;
        png_destroy_write_struct(&png, NULL);
        fclose(fp);
        return;
    }

    if (setjmp(png_jmpbuf(png))) {
        std::cerr << "Error during PNG write initialization" << std::endl;
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        return;
    }

    png_init_io(png, fp);

    png_set_IHDR(
        png,
        info,
        imageData.width, imageData.height,
        8,
        PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );
    png_write_info(png, info);

    if (!imageData.row_pointers) {
        std::cerr << "Row pointers are null" << std::endl;
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        return;
    }

    png_write_image(png, imageData.row_pointers);
    png_write_end(png, NULL);

    for (int y = 0; y < imageData.height; y++) {
        free(imageData.row_pointers[y]);
    }
    free(imageData.row_pointers);
    imageData.row_pointers = NULL;

    fclose(fp);

    png_destroy_write_struct(&png, &info);
}

// png_bytep calculate_average(png_bytep row, int x, int y, int channels, ImageData& imageData) {
//     int total_red = 0, total_green = 0, total_blue = 0;
//     int neighbor_count = 0;

//     for (int dy = -4; dy <= 4; dy++) {
//         int ny = y + dy;
//         if (ny >= 0 && ny < imageData.height) {
//             for (int dx = -4; dx <= 4; dx++) {
//                 int nx = x + dx;
//                 if (nx >= 0 && nx < imageData.width) {
//                     png_bytep neighbor = &(imageData.row_pointers[ny][nx * channels]);
//                     total_red += neighbor[0];
//                     total_green += neighbor[1];
//                     total_blue += neighbor[2];
//                     neighbor_count++;
//                 }
//             }
//         }
//     }

//     png_bytep result = (png_bytep)malloc(channels * sizeof(png_byte));
//     if (neighbor_count > 0) {
//         result[0] = (png_byte)(total_red / neighbor_count);
//         result[1] = (png_byte)(total_green / neighbor_count);
//         result[2] = (png_byte)(total_blue / neighbor_count);
//     } else {
//         memcpy(result, &(row[x * channels]), channels * sizeof(png_byte));
//     }
//     return result;
// }

// void calculate_average_recursive(int x, int y, int kernel_radius, ImageData& imageData, int& total_red, int& total_green, int& total_blue, int& count) {
//     if (x < 0 || x >= imageData.width || y < 0 || y >= imageData.height) {
//         return; // Base case: out of bounds
//     }
//     png_bytep px = &(imageData.row_pointers[y][x * 4]);
//     total_red += px[0];
//     total_green += px[1];
//     total_blue += px[2];
//     count++;
    
//     if (kernel_radius > 0) {
//         calculate_average_recursive(x + 1, y, kernel_radius - 1, imageData, total_red, total_green, total_blue, count);
//         calculate_average_recursive(x - 1, y, kernel_radius - 1, imageData, total_red, total_green, total_blue, count);
//         calculate_average_recursive(x, y + 1, kernel_radius - 1, imageData, total_red, total_green, total_blue, count);
//         calculate_average_recursive(x, y - 1, kernel_radius - 1, imageData, total_red, total_green, total_blue, count);
//     }
// }

// png_bytep calculate_average(png_bytep row, int x, int y, int channels, ImageData& imageData) {
//     int total_red = 0, total_green = 0, total_blue = 0;
//     int count = 0;
//     int kernel_radius = 4; // Assuming a fixed radius for the example

//     calculate_average_recursive(x, y, kernel_radius, imageData, total_red, total_green, total_blue, count);

//     png_bytep result = (png_bytep)malloc(channels * sizeof(png_byte));
//     if (count > 0) {
//         result[0] = (png_byte)(total_red / count);
//         result[1] = (png_byte)(total_green / count);
//         result[2] = (png_byte)(total_blue / count);
//     } else {
//         memcpy(result, &(row[x * channels]), channels * sizeof(png_byte));
//     }
//     return result;
// }



// void apply_blur(int start_row, int end_row, ImageData& imageData) {
//     for (int y = start_row; y < end_row; y++) {
//         png_bytep row = imageData.row_pointers[y];
//         for (int x = 0; x < imageData.width; x++) {
//             png_bytep px = &(row[x * 4]);
//             png_bytep average_color = calculate_average(row, x, y, 4, imageData);
//             px[0] = average_color[0]; //re-assign red
//             px[1] = average_color[1]; //re-assign greem
//             px[2] = average_color[2]; //re-assign blue
//             free(average_color);
//         }
//     }
// }

void calculate_average(png_bytep row, int x, int y, int channels, ImageData& imageData, int current_dx, int current_dy, int& total_red, int& total_green, int& total_blue, int& neighbor_count) {
    if (current_dy > 4) {
        png_bytep result = &(row[x * channels]);
        if (neighbor_count > 0) {
            result[0] = (png_byte)(total_red / neighbor_count);
            result[1] = (png_byte)(total_green / neighbor_count);
            result[2] = (png_byte)(total_blue / neighbor_count);
        }
        return;
    }

    int nx = x + current_dx;
    int ny = y + current_dy;

    if (nx >= 0 && nx < imageData.width && ny >= 0 && ny < imageData.height) {
        png_bytep neighbor = &(imageData.row_pointers[ny][nx * channels]);
        total_red += neighbor[0];
        total_green += neighbor[1];
        total_blue += neighbor[2];
        neighbor_count++;
    }

    if (current_dx < 4) {
        calculate_average(row, x, y, channels, imageData, current_dx + 1, current_dy, total_red, total_green, total_blue, neighbor_count);
    } else {
        calculate_average(row, x, y, channels, imageData, - 4, current_dy + 1, total_red, total_green, total_blue, neighbor_count);
    }
}

void apply_blur(int start_row, int end_row, ImageData& imageData) {
    for (int y = start_row; y < end_row; y++) {
        png_bytep row = imageData.row_pointers[y];
        for (int x = 0; x < imageData.width; x++) {
            int total_red = 0, total_green = 0, total_blue = 0;
            int neighbor_count = 0;
            calculate_average(row, x, y, 4, imageData, -4, -4, total_red, total_green, total_blue, neighbor_count);
        }
    }
}


void process_single_image_multithread(const std::string& input_path, const std::string& output_path, int num_threads_per_image) {
    ImageData imageData;
    imageData.row_pointers = nullptr;
    read_png_file(input_path, imageData);

    if (!imageData.row_pointers) {
        std::cerr << "Failed to read PNG file: " << input_path << std::endl;
        return;
    }

    std::vector<std::thread> threads;
    int rows_per_thread = imageData.height / num_threads_per_image;

    for (int i = 0; i < num_threads_per_image; i++) {
        int start_row = i * rows_per_thread;
        int end_row = (i == num_threads_per_image - 1) ? imageData.height : (i + 1) * rows_per_thread;
        threads.emplace_back(apply_blur, start_row, end_row, std::ref(imageData));
    }

    for (auto& thread : threads) {
        thread.join();
    }

    write_png_file(output_path, imageData);
}


void apply_filter_on_images(const std::vector<std::string>& image_paths, const std::string& output_folder, int numb_threads_of_folder, int num_threads_per_image) {
    std::vector<std::thread> threads;
    std::mutex image_mutex;
    std::atomic<int> global_index = 0;

    auto process_image = [&]() {
        while (true) {
            int index = global_index++;
            if (index >= image_paths.size()) break; 
            std::string image_path = image_paths[index];

            
            std::string output_path = output_folder + "/processed_" + fs::path(image_path).filename().string();
            process_single_image_multithread(image_path, output_path, num_threads_per_image);
            // {
            //     std::lock_guard<std::mutex> lock(image_mutex);
            //     // std::cout << "Processed: " << image_path << std::endl;
            // }
        }
    };

    
    for (int i = 0; i < numb_threads_of_folder; ++i) {
        threads.emplace_back(process_image);
    }


    for (auto& thread : threads) {
        thread.join();
    }
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <folder_path> <output_folder_path> <numb_threads_of_folder> <num_threads_per_image>" << std::endl;
        return 1;
    }

    std::string folder_path = argv[1];
    std::string output_folder = argv[2];
    int num_image_threads = std::stoi(argv[3]);
    int num_section_threads = std::stoi(argv[4]);

    std::vector<std::string> image_paths;
    for (const auto& entry : fs::directory_iterator(folder_path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".png") {
            image_paths.push_back(entry.path().string());
        }
    }

    apply_filter_on_images(image_paths, output_folder, num_image_threads, num_section_threads);

    return 0;
}