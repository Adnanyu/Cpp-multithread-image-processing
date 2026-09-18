# Parallel PNG Blur Processor

A C++ command-line tool that applies a blur filter to every PNG in a folder, using multiple threads to process many images at once.
I built this tool as part of my systems programming course in my junior year(I know I'm late to upload but later is better than never :) ).
The goal of this project was to apply what we learned in the course such as parallelism, memory allocation, segmentation faults, and multi-core processing — to a real compute-heavy workload. Image blurring fits well: every pixel depends on dozens of neighbors, so the difference between a serial and a parallel implementation is immediately visible in wall-clock time.

Point it at a directory of images, and it writes blurred copies to an output directory.

---

## Before / After

| Before | After |
| --- | --- |
| <img width="400" alt="blur_before" src="https://github.com/user-attachments/assets/1b4eb01e-52d7-4e77-8598-25c9a654c1d8" /> | <img width="400" alt="blur_after" src="https://github.com/user-attachments/assets/bb425c26-38cb-44cd-8134-9bb9acd04f4a" /> |


---

## Build

```bash
sudo apt install libpng-dev
g++ -O3 -std=c++17 blur.cpp utility.cpp -o blur -lpng
```

## Usage

```bash
./blur <input_folder> <output_folder> <folder_threads> <threads_per_image>
```

| Argument | Meaning |
| --- | --- |
| `input_folder` | Folder to scan for `.png` files |
| `output_folder` | Where results go — must already exist |
| `folder_threads` | How many images to process at the same time |
| `threads_per_image` | How many threads split up a single image |

Example:

```bash
mkdir -p images_out
./blur images images_out 8 4
```

Results are written as `processed_<original_name>.png`.

---

## How it works

**1. Scan.** The program walks the input folder and collects every `.png` file it finds.

**2. Split the work two ways.** This is the core idea. Images are divided among a pool of threads, so several are being processed at once. Then each individual image is split into horizontal bands, and a second set of threads blurs those bands in parallel. With `8 4`, up to 32 threads are working at any moment.

**3. Decode.** libpng reads each file and normalizes it to 8-bit RGBA, regardless of what the original was — grayscale, palette-based, or 16-bit per channel all come out in the same format, so the blur code doesn't have to special-case anything.

**4. Blur.** For every pixel, the filter averages the red, green and blue values of the pixels in a 9×9 square around it and writes the average back. Pixels near the edges simply average over fewer neighbors. Alpha is left alone.

**5. Encode.** libpng writes the finished image to the output folder and releases its memory.

On an 8-core machine, 2000 images finish in about a minute — roughly 7.6× faster than doing them one at a time.

---

## Project layout

```
blur.cpp      entry point — argument parsing and folder scan
utility.cpp   PNG reading/writing, the blur filter, threading
utility.hpp   shared struct and declarations
```

---

## Notes & future work

The blur currently writes results back into the same buffer it reads from, which means the output is an approximation rather than a textbook box blur, and it isn't fully deterministic when a single image is split across threads. Using a separate destination buffer would fix both.

Other ideas:

- Reusable thread pool instead of creating threads per image
- Separable blur (two 1D passes instead of one 2D pass) for a big speedup
- Gaussian blur as an alternative filter
- SIMD acceleration
- Support for JPEG and BMP
