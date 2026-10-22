// A C++ image class over the C interface, as most programs write it first.
//
// The handles belong to objects that free them, a call that fails is an
// exception with the message of the library, and the heights are read into a
// two-dimensional array where it is: image[y][x]. Everything else a program
// does with a surface works on this class and never sees the C interface again.

#include <sdfio/file.h>
#include <sdfio/sdfio.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
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

class Image
{
  public:
    Image(std::size_t columns, std::size_t rows, double x_scale, double y_scale)
        : columns_(columns), rows_(rows), x_scale_(x_scale), y_scale_(y_scale),
          heights_(std::make_unique<float[]>(columns * rows))
    {
        std::fill_n(heights_.get(), columns * rows, NAN);
    }

    static Image read(const std::filesystem::path &path)
    {
        Reader reader(sdfio_reader_new(NULL), sdfio_reader_free);
        check(sdfio_open_file(reader.get(), path.string().c_str()),
              sdfio_reader_error_message(reader.get()));
        const sdfio_header *header = sdfio_get_header(reader.get());
        Image image(header->columns, header->rows, header->x_scale,
                    header->y_scale);
        check(sdfio_read_heights(
                  reader.get(), sdfio_metres(SDFIO_FLOAT32),
                  image.heights_.get(),
                  static_cast<ptrdiff_t>(image.columns_ * sizeof(float))),
              sdfio_reader_error_message(reader.get()));
        return image;
    }

    void write(const std::filesystem::path &path) const
    {
        sdfio_header header;
        sdfio_header_init(&header);
        header.data_type = SDFIO_DATA_INT16;
        header.columns = columns_;
        header.rows = rows_;
        header.x_scale = x_scale_;
        header.y_scale = y_scale_;
        Writer writer(sdfio_writer_new(NULL), sdfio_writer_free);
        check(sdfio_create_file(writer.get(), path.string().c_str(), &header),
              sdfio_writer_error_message(writer.get()));
        check(sdfio_write_heights(
                  writer.get(), sdfio_metres(SDFIO_FLOAT32), heights_.get(),
                  static_cast<ptrdiff_t>(columns_ * sizeof(float))),
              sdfio_writer_error_message(writer.get()));
        check(sdfio_write_trailer(writer.get(), nullptr, 0),
              sdfio_writer_error_message(writer.get()));
    }

    std::size_t columns() const
    {
        return columns_;
    }
    std::size_t rows() const
    {
        return rows_;
    }
    double x_scale() const
    {
        return x_scale_;
    }
    double y_scale() const
    {
        return y_scale_;
    }

    // The row y, an array of columns() heights: image[y][x]. The rows follow
    // each other in one block of memory, as the interface of the library reads
    // and writes them.
    float *operator[](std::size_t y)
    {
        return heights_.get() + y * columns_;
    }
    const float *operator[](std::size_t y) const
    {
        return heights_.get() + y * columns_;
    }
    bool measured(std::size_t x, std::size_t y) const
    {
        return !std::isnan((*this)[y][x]);
    }

  private:
    std::size_t columns_;
    std::size_t rows_;
    double x_scale_;
    double y_scale_;
    std::unique_ptr<float[]> heights_;
};

Image ripples()
{
    Image image(160, 120, 1e-6, 1e-6);
    for (std::size_t y = 0; y < image.rows(); ++y)
    {
        for (std::size_t x = 0; x < image.columns(); ++x)
        {
            const bool hole = x > 60 && x < 70 && y > 40 && y < 50;
            if (!hole)
            {
                image[y][x] = static_cast<float>(
                    2e-6 * std::sin(0.1 * static_cast<double>(x))
                    * std::cos(0.07 * static_cast<double>(y)));
            }
        }
    }
    return image;
}

} // namespace

int main()
{
    try
    {
        const Image written = ripples();
        written.write("image.sdf");
        const Image read = Image::read("image.sdf");
        std::size_t differing = 0;
        for (std::size_t y = 0; y < read.rows(); ++y)
        {
            for (std::size_t x = 0; x < read.columns(); ++x)
            {
                const bool same =
                    read.measured(x, y) == written.measured(x, y)
                    && (!read.measured(x, y)
                        || std::fabs(read[y][x] - written[y][x]) < 1e-10F);
                differing += same ? 0 : 1;
            }
        }
        std::cout << read.columns() << " x " << read.rows() << ", " << differing
                  << " points differ\n";
        return differing == 0 ? 0 : 1;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
