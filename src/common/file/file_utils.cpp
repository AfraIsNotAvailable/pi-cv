#include "opencv2/opencv.hpp"
#include "file_utils.h"
#include "logger.h"

#include <string>
#include <fstream>
#include <sstream>

std::string FileUtils::readFile(const std::string &fileName)
{
  std::ifstream file(fileName);
  if (file.is_open()) {
    std::stringstream fileStream;
    fileStream << file.rdbuf();
    file.close();
    return fileStream.str();
  }
  else {
    ERROR("Failed to open file {}. Returning empty string.", fileName);
    return "";
  }
}

cv::Mat FileUtils::readImage(const std::string &fileName, const cv::ImreadModes mode)
{
  return imread(fileName, mode);
}

#include <chrono>
#include <sys/time.h>
#include <ctime>
#include <sstream>
#include <filesystem>
#include <unistd.h>

using std::chrono::duration_cast;
using std::chrono::seconds;
using std::chrono::system_clock;
using std::filesystem::current_path;

#include "paths.h"

// this function is not exposed. if needed, do so
std::string nextImageName()
{
  // Generate a file name bound to be unique
  const auto secSinceEpoch = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
  std::stringstream fName;
  fName << secSinceEpoch << ".bmp";
  // Make a path to the file
  std::filesystem::path path;
  path += current_path();
  path += "/assets/export/";
  path += fName.str();

  return path.string();
}

void FileUtils::saveImage(const cv::Mat &img, const std::string &fileName)
{
  imwrite(fileName, img);
  DEBUG("Export {}", fileName);
}

void FileUtils::quickSave(const cv::Mat &img)
{
  std::string fileName = nextImageName();
  FileUtils::saveImage(img, fileName);
}

#include <cstdlib>
#include <array>
#include <memory>

std::string FileUtils::openFileDialog(const std::string &title, const std::string &startDir)
{
  // Use zenity (GTK) or kdialog (KDE) for native file picker
  // zenity is more universally available on Linux
  std::string command = "zenity --file-selection";
  command += " --title=\"" + title + "\"";
  command += " --filename=\"" + startDir + "/\"";
  command += " --file-filter=\"Images | *.bmp *.png *.jpg *.jpeg *.tiff *.tif\"";
  command += " 2>/dev/null";  // suppress GTK warnings
  
  std::array<char, 512> buffer;
  std::string result;
  
  // Open pipe to read command output
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
  if (!pipe) {
    ERROR("Failed to open file dialog");
    return "";
  }
  
  // Read the selected path
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  
  // Remove trailing newline
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  
  if (!result.empty()) {
    DEBUG("File selected: {}", result);
  }
  
  return result;
}
