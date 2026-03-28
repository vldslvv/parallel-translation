#pragma once
#include <memory>
#include <string_view>

#include "writers/formatted_writer.hpp"

class PdfWriter {
  public:
    explicit PdfWriter(std::string_view path);
    ~PdfWriter();
    PdfWriter(PdfWriter&&) noexcept;
    PdfWriter& operator=(PdfWriter&&) noexcept;
    PdfWriter(const PdfWriter&) = delete;
    PdfWriter& operator=(const PdfWriter&) = delete;

    bool is_open() const;
    int write_pair(std::string_view original, std::string_view translation);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    int start_new_page();
    int flush_page();
};

FormattedWriter make_pdf_formatted_writer(PdfWriter& writer);
