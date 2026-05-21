#ifndef __PATHS_H__
#define __PATHS_H__

// useful for consteval const char *
#define PathConcat(PATH1, PATH2) PATH1 PATH2
#define ImageFolder "./assets/images"
#define VideoFolder "./assets/videos"
#define ExportFolder "./assets/export"
#define AssetsFolder "./assets"

#define IMAGE(PATH) PathConcat(ImageFolder, "/" PATH)
#define EXPORT(PATH) PathConcat(ExportFolder, "/" PATH)

// Useful later on, when absolute paths are required
// #include <filesystem>
// std::filesystem::current_path()

#endif // __PATHS_H__
