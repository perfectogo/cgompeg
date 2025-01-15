#include <MagickWand/MagickWand.h>

int main(int argc, char **argv) {
    if (argc != 4) {
        printf("Usage: %s input_image output_image width\n", argv[0]);
        return 1;
    }

    const char *input = argv[1];
    const char *output = argv[2];
    size_t width = atoi(argv[3]);
    size_t height;

    MagickWandGenesis(); // Initialize MagickWand environment

    MagickWand *wand = NewMagickWand(); // Create a wand object

    if (MagickReadImage(wand, input) == MagickFalse) {
        printf("Failed to read image: %s\n", input);
        wand = DestroyMagickWand(wand);
        MagickWandTerminus(); // Cleanup MagickWand environment
        return 1;
    }

    // Calculate height to maintain aspect ratio
    height = MagickGetImageHeight(wand) * width / MagickGetImageWidth(wand);

    // Resize the image
    if (MagickResizeImage(wand, width, height, LanczosFilter, 1.0) == MagickFalse) {
        printf("Failed to resize image.\n");
        wand = DestroyMagickWand(wand);
        MagickWandTerminus();
        return 1;
    }

    // Write the output image
    if (MagickWriteImages(wand, output, MagickTrue) == MagickFalse) {
        printf("Failed to write image: %s\n", output);
        wand = DestroyMagickWand(wand);
        MagickWandTerminus();
        return 1;
    }

    // Cleanup
    wand = DestroyMagickWand(wand);
    MagickWandTerminus();

    printf("Image resized successfully: %s\n", output);
    return 0;
}
