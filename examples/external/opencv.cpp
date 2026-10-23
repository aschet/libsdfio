// OpenCV: reads a surface into an image, smooths it, writes it back, and reads
// a surface into a region of an image. The argument is the SDF file to read.

#include <sdfio/file.h>
#include <sdfio/sdfio.h>

#include <opencv2/core.hpp>
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

sdfio_config make_config()
{
    sdfio_config config = {};
    config.reader_formats[0] = &sdfio_binary_reader_format;
    config.reader_formats[1] = &sdfio_ascii_reader_format;
    return config;
}

const sdfio_config config = make_config();

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
    Reader reader(sdfio_reader_new(&config), sdfio_reader_free);
    check(sdfio_open_file(reader.get(), path),
          sdfio_reader_error_message(reader.get()));
    return reader;
}

// An image counts its rows from the top and a surface from y = 0. The layout
// has a negative row stride and the pointer given with it is at the last row,
// so the first row of the file is the last row in memory and no copy is made.
// The step of the image is the row stride, so that an image that is not
// continuous, such as a region of interest, is used as it is.
sdfio_layout bottom_up(const cv::Mat &image)
{
    sdfio_layout layout = sdfio_layout_packed(SDFIO_TYPE_FLOAT);
    layout.row_stride = -static_cast<ptrdiff_t>(image.step);
    return layout;
}

cv::Mat read_heights(sdfio_reader *reader)
{
    const sdfio_header *header = sdfio_get_header(reader);
    cv::Mat heights(static_cast<int>(header->rows),
                    static_cast<int>(header->columns), CV_32F);
    check(sdfio_read_heights(reader, bottom_up(heights),
                             heights.ptr(heights.rows - 1)),
          sdfio_reader_error_message(reader));
    return heights;
}

cv::Mat smooth(const cv::Mat &heights)
{
    cv::Mat filled = heights.clone();
    // A point that is not measured is NaN.
    cv::patchNaNs(filled, 0.0);
    cv::GaussianBlur(filled, filled, cv::Size(5, 5), 1.0);
    return filled;
}

void write_heights(const char *path, sdfio_header header,
                   const cv::Mat &heights)
{
    header.data_type = SDFIO_TYPE_FLOAT;
    Writer writer(sdfio_writer_new(&config), sdfio_writer_free);
    check(sdfio_create_file(writer.get(), path, &header),
          sdfio_writer_error_message(writer.get()));
    check(sdfio_write_heights(writer.get(), bottom_up(heights),
                              heights.ptr(heights.rows - 1)),
          sdfio_writer_error_message(writer.get()));
    check(sdfio_end_trailer(writer.get()),
          sdfio_writer_error_message(writer.get()));
}

cv::Mat read_into_region(const char *path)
{
    const Reader reader = open(path);
    const sdfio_header *header = sdfio_get_header(reader.get());
    cv::Mat canvas = cv::Mat::zeros(200, 300, CV_32F);
    cv::Mat region = canvas(cv::Rect(100, 50, static_cast<int>(header->columns),
                                     static_cast<int>(header->rows)));
    check(sdfio_read_heights(reader.get(), bottom_up(region),
                             region.ptr(region.rows - 1)),
          sdfio_reader_error_message(reader.get()));
    return canvas;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: " << argv[0] << " file.sdf\n";
        return 1;
    }
    const char *path = argv[1];
    try
    {
        const Reader reader = open(path);
        const cv::Mat smoothed = smooth(read_heights(reader.get()));
        write_heights("smooth.sdf", *sdfio_get_header(reader.get()), smoothed);
        read_into_region(path);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
