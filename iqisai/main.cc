#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <tuple>
#include <vector>
#include <filesystem>
#include <fmt/format.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
extern "C"
{
#include "ffio.h"
#include "ffioPyApi.h"
#include "stb_image_write.h"
}

namespace fs = std::filesystem;
namespace py = pybind11;

int save_rgb_frame_as_jpg(unsigned char *rgb_data, int width, int height, const char *filename, int quality)
{
    // Assuming rgb_data is in RGB format, with 3 bytes per pixel
    int channels = 3; // RGB
    int stride_in_bytes = width * channels;

    // Write the image as JPEG
    int result = stbi_write_jpg(filename, width, height, channels, rgb_data, quality);

    if (result == 0)
    {
        fprintf(stderr, "Error in writing the JPEG image %s\n", filename);
        return -1;
    }
    return 0;
}

py::str extract_frame(const char *video, std::vector<std::tuple<int, int>> ts_vec, int per_sec, const char *save_dir, int start_ms)
{
    struct CodecParams codecParams;
    fs::path videoPath(video);
    fs::path savePath(save_dir);
    videoPath.stem();
    FFIO *ffio = api_newFFIO();
    if (ffio == nullptr)
    {
        return py::str("分配内存失败");
    }
    int ret = initFFIO(ffio, FFIO_MODE_DECODE, video, false, false, "", false, "", 0, 0, &codecParams);
    if (ret != 0)
    {
        finalizeFFIO(ffio);
        free(ffio);
        return py::str("初始化解码器失败");
    }
    int fps = 25;
    int step = (fps % per_sec != 0) ? int(fps / per_sec) + 1 : int(fps / per_sec), i = 0, j = 1;
    const int frame_ms = 1000 / fps;
    while (true)
    {
        if (i % fps == 0)
        {
            j = 0;
        }
        FFIOFrame *frame = decodeOneFrame(ffio, "");
        if (frame->data == nullptr)
        {
            break;
        }
        if (j % step == 0)
        {
            if (save_rgb_frame_as_jpg(frame->data, frame->width, frame->height,
                                      (savePath / fmt::format("{:08}.jpg", i * frame_ms + start_ms)).string().c_str(), 90) != 0)
            {
                finalizeFFIO(ffio);
                free(ffio);
                return py::str("保存图片失败");
            }
        }
        i++;
        j++;
    }
    // api_initFFIO(ffio, 0, video, false, false, "", false, "", 0, 0, &codecParams);
    finalizeFFIO(ffio);
    free(ffio);
    return py::str("");
}

PYBIND11_MODULE(iqisai, m)
{
    m.def("extract_frame", &extract_frame, py::call_guard<py::gil_scoped_release>());
}