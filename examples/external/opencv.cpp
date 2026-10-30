#include <sdfio/file.h>
#include <sdfio/sdfio.h>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <cstddef>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace
{

using Reader = std::unique_ptr<sdfio_reader, decltype(&sdfio_reader_free)>;
using Writer = std::unique_ptr<sdfio_writer, decltype(&sdfio_writer_free)>;
using Header = std::unique_ptr<sdfio_header, decltype(&sdfio_header_free)>;

void check(sdfio_status status, const char *message)
{
    if (status != SDFIO_OK)
    {
        throw std::runtime_error(std::string(sdfio_status_string(status)) + ": "
                                 + message);
    }
}

Header copy_header(const sdfio_header *source)
{
    sdfio_header *header = nullptr;
    sdfio_header_create(sdfio_default_allocator(), &header);
    sdfio_header_copy(header, source);
    return {header, sdfio_header_free};
}

Reader create_reader()
{
    sdfio_reader *reader = nullptr;
    sdfio_reader_create(sdfio_default_allocator(), &reader);
    sdfio_reader_add_binary(reader);
    sdfio_reader_add_ascii(reader);
    return {reader, sdfio_reader_free};
}

Writer create_writer()
{
    sdfio_writer *writer = nullptr;
    sdfio_writer_create(sdfio_default_allocator(), &writer);
    sdfio_writer_add_binary(writer);
    return {writer, sdfio_writer_free};
}

Reader open(const char *path)
{
    Reader reader = create_reader();
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
    sdfio_layout layout = *sdfio_layout_packed(SDFIO_TYPE_FLOAT);
    layout.row_stride = -static_cast<ptrdiff_t>(image.step);
    return layout;
}

cv::Mat read_heights(sdfio_reader *reader)
{
    const sdfio_header *header = sdfio_reader_header(reader);
    cv::Mat heights(static_cast<int>(sdfio_header_rows(header)),
                    static_cast<int>(sdfio_header_columns(header)), CV_32F);
    const sdfio_layout layout = bottom_up(heights);
    check(sdfio_read_heights(reader, &layout, heights.ptr(heights.rows - 1)),
          sdfio_reader_error_message(reader));
    return heights;
}

// Only the points that are measured enter the average: the blur of the heights
// is divided by the blur of the mask of these points. A point that is not
// measured stays NaN.
cv::Mat smooth(const cv::Mat &heights)
{
    cv::Mat measured;
    cv::compare(heights, heights, measured, cv::CMP_EQ);
    cv::Mat weights;
    measured.convertTo(weights, CV_32F, 1.0 / 255.0);
    cv::Mat values = heights.clone();
    values.setTo(0.0, ~measured);
    cv::GaussianBlur(values, values, cv::Size(5, 5), 1.0);
    cv::GaussianBlur(weights, weights, cv::Size(5, 5), 1.0);
    cv::Mat smoothed;
    cv::divide(values, weights, smoothed);
    smoothed.setTo(std::numeric_limits<float>::quiet_NaN(), ~measured);
    return smoothed;
}

void write_heights(const char *path, const sdfio_header *source,
                   const cv::Mat &heights)
{
    Header header = copy_header(source);
    sdfio_header_set_data_type(header.get(), SDFIO_TYPE_FLOAT);
    sdfio_header_set_format(header.get(), SDFIO_BINARY);
    Writer writer = create_writer();
    check(sdfio_create_file(writer.get(), path, header.get()),
          sdfio_writer_error_message(writer.get()));
    const sdfio_layout layout = bottom_up(heights);
    check(sdfio_write_heights(writer.get(), &layout,
                              heights.ptr(heights.rows - 1)),
          sdfio_writer_error_message(writer.get()));
    check(sdfio_end_file(writer.get()),
          sdfio_writer_error_message(writer.get()));
}

cv::Mat read_into_region(const char *path)
{
    const Reader reader = open(path);
    const sdfio_header *header = sdfio_reader_header(reader.get());
    cv::Mat canvas = cv::Mat::zeros(200, 300, CV_32F);
    cv::Mat region =
        canvas(cv::Rect(100, 50, static_cast<int>(sdfio_header_columns(header)),
                        static_cast<int>(sdfio_header_rows(header))));
    const sdfio_layout layout = bottom_up(region);
    check(
        sdfio_read_heights(reader.get(), &layout, region.ptr(region.rows - 1)),
        sdfio_reader_error_message(reader.get()));
    return canvas;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::cerr << "usage: " << argv[0] << " input.sdf output.sdf\n";
        return 1;
    }
    const char *input = argv[1];
    const char *output = argv[2];
    try
    {
        const Reader reader = open(input);
        const cv::Mat smoothed = smooth(read_heights(reader.get()));
        write_heights(output, sdfio_reader_header(reader.get()), smoothed);
        read_into_region(input);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
