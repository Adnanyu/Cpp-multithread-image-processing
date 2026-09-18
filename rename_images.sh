#!/bin/bash

# Set the folder path containing the images
folder_path="images2"

# Change directory to the folder containing the images
cd "$folder_path" || exit

# Initialize a counter
count=1

# Loop through each image file in the folder
for file in *.jpg *.jpeg *.png *.gif *.bmp *.tiff *.tif; do
    # Check if the file is an image
    if [ -f "$file" ]; then
        # Get the file extension
        extension="${file##*.}"
        
        # Generate the new filename with the format imgN.extension
        new_filename="img${count}.$extension"
        
        # Rename the file
        mv "$file" "$new_filename"
        
        # Increment the counter
        ((count++))
    fi
done

echo "All images have been renamed."
