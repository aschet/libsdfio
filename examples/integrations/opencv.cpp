// OpenCV: a surface as an image of heights.
//
// The row stride of a call may be negative, with the pointer at the last row:
// the rows are then written from the bottom up. An image counts its rows from
// the top and a surface from y = 0, so the first row of a Mat is the largest y,
// and no copy is made. A Mat that is not continuous, such as a region of
// interest, is read into as it is: its step is the stride.

#include <sdfio/file.h>
#include <sdfio/sdfio.h>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cstddef>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace
{

using Reader = std::unique_ptr<sdfio_reader, decltype(&sdfio_reader_free)>;
using Writer = std::unique_ptr<sdfio_writer, decltype(&sdfio_writer_free)>;

void check(sdfio_status status, const char *message)
{
    if (status != SDFIO_OK)
    {
        throw std::runtime_error(std::string(sdfio_status_string(status)) + ": "
                                 + message);
    }
}

Reader open(const char *path)
{
    Reader reader(sdfio_reader_new(NULL), sdfio_reader_free);
    check(sdfio_open_file(reader.get(), path),
          sdfio_reader_error_message(reader.get()));
    return reader;
}

// The format of an image that is read and written bottom up: the first row of
// the file is the last row in memory, so the rows run backwards.
sdfio_format bottom_up(const cv::Mat &image)
{
    sdfio_format format = sdfio_format_packed(SDFIO_FLOAT32);
    format.row_stride = -static_cast<ptrdiff_t>(image.step);
    return format;
}

cv::Mat read_heights(sdfio_reader *reader)
{
    const sdfio_header *header = sdfio_get_header(reader);
    cv::Mat heights(static_cast<int>(header->rows),
                    static_cast<int>(header->columns), CV_32F);
    check(sdfio_read_heights_strided(reader, bottom_up(heights),
                                     heights.ptr(heights.rows - 1)),
          sdfio_reader_error_message(reader));
    return heights;
}

cv::Mat smooth(const cv::Mat &heights)
{
    cv::Mat filled = heights.clone();
    cv::patchNaNs(filled, 0.0);
    cv::GaussianBlur(filled, filled, cv::Size(5, 5), 1.0);
    return filled;
}

void save_preview(const cv::Mat &heights, const char *path)
{
    cv::Mat preview;
    cv::normalize(heights, preview, 0, 255, cv::NORM_MINMAX, CV_8U);
    cv::imwrite(path, preview);
}

void write_heights(const char *path, sdfio_header header,
                   const cv::Mat &heights)
{
    header.data_type = SDFIO_DATA_BINARY32;
    header.z_scale = SDFIO_AUTO_Z_SCALE;
    Writer writer(sdfio_writer_new(NULL), sdfio_writer_free);
    check(sdfio_create_file(writer.get(), path, &header),
          sdfio_writer_error_message(writer.get()));
    check(sdfio_write_heights_strided(writer.get(), bottom_up(heights),
                                      heights.ptr(heights.rows - 1)),
          sdfio_writer_error_message(writer.get()));
    check(sdfio_write_trailer(writer.get(), nullptr, 0),
          sdfio_writer_error_message(writer.get()));
}

// A region of interest is read into without a copy; the rest of the image is
// left alone.
cv::Mat read_into_region(const char *path)
{
    const Reader reader = open(path);
    const sdfio_header *header = sdfio_get_header(reader.get());
    cv::Mat canvas = cv::Mat::zeros(200, 300, CV_32F);
    cv::Mat region = canvas(cv::Rect(100, 50, static_cast<int>(header->columns),
                                     static_cast<int>(header->rows)));
    check(sdfio_read_heights_strided(reader.get(), bottom_up(region),
                                     region.ptr(region.rows - 1)),
          sdfio_reader_error_message(reader.get()));
    return canvas;
}

} // namespace

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : SDFIO_EXAMPLE_FILE;
    try
    {
        const Reader reader = open(path);
        const cv::Mat heights = read_heights(reader.get());
        const cv::Mat blurred = smooth(heights);
        save_preview(blurred, "preview.png");
        write_heights("smooth.sdf", *sdfio_get_header(reader.get()), blurred);
        const cv::Mat canvas = read_into_region(path);
        std::cout << "OpenCV " << CV_VERSION << ": " << heights.cols << " x "
                  << heights.rows << ", canvas " << canvas.cols << " x "
                  << canvas.rows << "\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
