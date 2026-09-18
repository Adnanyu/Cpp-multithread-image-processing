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


namespace fs = std::filesystem;



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

    fclose(fp);

    png_destroy_write_struct(&png, &info);
}


void calculate_average(png_bytep row, int x, int y, int channels, ImageData& imageData, png_bytep result) {
    int total_red = 0, total_green = 0, total_blue = 0;
    int neighbor_count = 0;

    for (int dy = -4; dy <= 4; dy++) {
        int ny = y + dy;
        if (ny >= 0 && ny < imageData.height) {
            for (int dx = -4; dx <= 4; dx++) {
                int nx = x + dx;
                if (nx >= 0 && nx < imageData.width) {
                    png_bytep neighbor = &(imageData.row_pointers[ny][nx * channels]);
                    total_red += neighbor[0];
                    total_green += neighbor[1];
                    total_blue += neighbor[2];
                    neighbor_count++;
                }
            }
        }
    }

    if (neighbor_count > 0) {
        result[0] = (png_byte)(total_red / neighbor_count);
        result[1] = (png_byte)(total_green / neighbor_count);
        result[2] = (png_byte)(total_blue / neighbor_count);
    } else {
        memcpy(result, &(row[x * channels]), channels * sizeof(png_byte));
    }
}



void apply_blur(int start_row, int end_row, ImageData& imageData) {
    png_bytep result = (png_bytep)malloc(4 * sizeof(png_byte));
    if (!result) {
        std::cerr << "Failed to allocate memory for result array" << std::endl;
        return;
    }

    for (int y = start_row; y < end_row; y++) {
        png_bytep row = imageData.row_pointers[y];
        for (int x = 0; x < imageData.width; x++) {
            png_bytep px = &(row[x * 4]);
            calculate_average(row, x, y, 4, imageData, result);
            px[0] = result[0]; // Re-assign red
            px[1] = result[1]; // Re-assign green
            px[2] = result[2]; // Re-assign blue
        }
    }

    free(result);
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

    write_png_file(input_path, imageData);
}

void apply_filter_on_images(const std::vector<std::string>& image_paths, const std::string& output_folder, int numb_threads_of_folder, int num_threads_per_image) {
    std::vector<std::thread> threads(numb_threads_of_folder);
    // std::mutex image_mutex;
    // std::atomic<int> global_index = 0;

    int numImages = image_paths.size();
    int imagesPerThread = numImages / numb_threads_of_folder;

    for (int i = 0; i < numb_threads_of_folder; ++i) {
        int start = i * imagesPerThread;
        int end = (i == numb_threads_of_folder - 1) ? numImages : (i + 1) * imagesPerThread;

        threads[i] = std::thread([&, start, end]() {
            for (int j = start; j < end; ++j) {
                std::string image_path = image_paths[j];
                std::string output_path = output_folder + "/processed_" + fs::path(image_path).filename().string();
                process_single_image_multithread(image_path, output_path, num_threads_per_image);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}























// void apply_filter_on_images(const std::vector<std::string>& image_paths, const std::string& output_folder, int numb_threads_of_folder, int num_threads_per_image) {
//   std::vector<std::thread> threads(numb_threads_of_folder);
//   std::mutex image_mutex; // Not strictly necessary in this approach
//   std::atomic<int> global_index = 0;
//   int numImages = image_paths.size();
//   int imagesPerThread = numImages / numb_threads_of_folder;

//   for (int i = 0; i < numb_threads_of_folder; ++i) {
//     int start = i * imagesPerThread;
//     int end = (i == numb_threads_of_folder - 1) ? numImages : (i + 1) * imagesPerThread;
//     threads[i] = std::thread([&, start, end]() {
//       for (int j = start; j < end; ++j) {
//         int image_index = global_index++; // Atomic increment for unique index
//         if (image_index >= numImages) break; // Handle potential out-of-bounds access
//         std::string image_path = image_paths[image_index];
//         std::string output_path = output_folder + "/processed_" + fs::path(image_path).filename().string();
//         process_single_image_multithread(image_path, output_path, num_threads_per_image);
//       }
//     });
//   }

//   for (auto& thread : threads) {
//     thread.join();
//   }
// }














































// void process_images(const std::vector<std::string>& image_paths, const std::string& output_folder, int numb_threads_of_folder, int num_threads_per_image) {
//     std::vector<std::thread> threads;
//     std::mutex image_mutex;
//     std::atomic<int> global_index = 0;

//     auto process_image = [&](int thread_index) {
//         for (int i = thread_index; i < image_paths.size(); i += numb_threads_of_folder) {
//             std::string image_path = image_paths[i];
//             std::string output_path = output_folder + "/processed_" + fs::path(image_path).filename().string();
//             process_single_image_multithread(image_path, output_path, num_threads_per_image);
//             {
//                 std::lock_guard<std::mutex> lock(image_mutex);
//                 std::cout << "Processed: " << image_path << std::endl;
//             }
//         }
//     };

//     for (int i = 0; i < numb_threads_of_folder; ++i) {
//         threads.emplace_back(process_image, i);
//     }

//     for (auto& thread : threads) {
//         thread.join();
//     }
// }





// void process_images(const std::vector<std::string>& image_paths, const std::string& output_folder, int numb_threads_of_folder, int num_threads_per_image) {
//     std::vector<std::thread> threads;
//     std::mutex image_mutex;
//     std::atomic<int> global_index = 0;

//     auto process_image = [&]() {
//         while (true) {
//             int index = global_index++;
//             if (index >= image_paths.size()) break; 
//             std::string image_path = image_paths[index];

            
//             std::string output_path = output_folder + "/processed_" + fs::path(image_path).filename().string();
//             process_single_image_multithread(image_path, output_path, num_threads_per_image);
//             // {
//             //     std::lock_guard<std::mutex> lock(image_mutex);
//             //     // std::cout << "Processed: " << image_path << std::endl;
//             // }
//         }
//     };

    
//     for (int i = 0; i < numb_threads_of_folder; ++i) {
//         threads.emplace_back(process_image);
//     }


//     for (auto& thread : threads) {
//         thread.join();
//     }
// }

// int main() {


//     // Divide the image paths among threads
//     size_t numImages = image_paths.size();
//     size_t imagesPerThread = numImages / numThreads;

//     for (int i = 0; i < numThreads; ++i) {
//         size_t startIdx = i * imagesPerThread;
//         size_t endIdx = (i == numThreads - 1) ? numImages : (i + 1) * imagesPerThread;

//         // Assign a thread to process a subset of images
//         threads[i] = std::thread([&, startIdx, endIdx]() {
//             for (size_t j = startIdx; j < endIdx; ++j) {
//                 std::string output_image_path = output_folder + "/" + image_paths[j].filename().string();
//                 process_image(image_paths[j], fs::path(output_image_path));
//             }
//         });
//     }

//     // Join threads
//     for (auto& t : threads) {
//         if (t.joinable()) {
//             t.join();
//         }
//     }

//     return 0;
// }