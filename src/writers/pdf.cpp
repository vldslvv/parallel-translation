#include "writers/pdf.hpp"

#include <filesystem>
#include <string>

#include <spdlog/spdlog.h>

#include <PDFWriter/AbstractContentContext.h>
#include <PDFWriter/EPDFVersion.h>
#include <PDFWriter/PDFPage.h>
#include <PDFWriter/PDFRectangle.h>
// PDFUsedFont is part of PDFHummus but its header transitively includes FreeType
// headers (ft2build.h). FreeType is an explicit Conan dependency (freetype/2.13.2);
// its include path is resolved via find_path(FREETYPE_INCLUDE_DIR) in the root
// CMakeLists.txt and propagated from the Conan-managed package — no system path assumed.
#include <PDFWriter/PDFUsedFont.h>
#include <PDFWriter/PDFWriter.h>
#include <PDFWriter/PageContentContext.h>

#include "common/exit_codes.hpp"

static std::string find_italic_font() {
    static const char* candidates[] = {
        "/usr/share/fonts/liberation-fonts/LiberationSans-Italic.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Italic.ttf",
        "/usr/share/fonts/TTF/LiberationSans-Italic.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Italic.ttf",
    };
    for (const char* path : candidates)
        if (std::filesystem::exists(path))
            return path;
    return {};
}

static std::string find_font() {
    static const char* candidates[] = {
        "/usr/share/fonts/liberation-fonts/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/TTF/LiberationSans-Regular.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
    };
    for (const char* path : candidates) {
        if (std::filesystem::exists(path))
            return path;
    }
    return {};
}

struct PdfWriter::Impl {
    PDFWriter pdf;
    PDFUsedFont* font = nullptr;
    PDFUsedFont* italic_font = nullptr;
    PDFPage* current_page = nullptr;
    PageContentContext* ctx = nullptr;
    double cursor_y = 0;
    bool open = false;

    static constexpr double kPageWidth = 595.0; // A4
    static constexpr double kPageHeight = 842.0;
    static constexpr double kMargin = 50.0;
    static constexpr double kFontSize = 10.0;
    static constexpr double kLineHeight = 14.0;
    static constexpr double kPairGap = 8.0;

    int flush_page() {
        if (ctx == nullptr)
            return 0;
        auto status = pdf.EndPageContentContext(ctx);
        ctx = nullptr;
        if (status != PDFHummus::eSuccess) {
            spdlog::error("cannot end page content context");
            return exit_code::output_error;
        }
        status = pdf.WritePageAndRelease(current_page);
        current_page = nullptr;
        if (status != PDFHummus::eSuccess) {
            spdlog::error("cannot write PDF page");
            return exit_code::output_error;
        }
        return 0;
    }

    int start_new_page() {
        current_page = new PDFPage();
        current_page->SetMediaBox(PDFRectangle(0, 0, kPageWidth, kPageHeight));
        ctx = pdf.StartPageContentContext(current_page);
        if (ctx == nullptr) {
            spdlog::error("cannot start page content context");
            delete current_page;
            current_page = nullptr;
            return exit_code::output_error;
        }
        cursor_y = kPageHeight - kMargin;
        return 0;
    }

    // Writes a single physical line of text, paginating if needed.
    int write_physical_line(const std::string& line,
                            const AbstractContentContext::TextOptions& opts) {
        if (cursor_y < kMargin + kLineHeight) {
            if (auto rc = flush_page(); rc != 0)
                return rc;
            if (auto rc = start_new_page(); rc != 0)
                return rc;
        }
        ctx->WriteText(kMargin, cursor_y, line, opts);
        cursor_y -= kLineHeight;
        return 0;
    }

    // Wraps text to fit within the printable width, then writes each wrapped line.
    int write_line(std::string_view text, PDFUsedFont* measure_font,
                   const AbstractContentContext::TextOptions& opts) {
        static constexpr double kMaxLineWidth = kPageWidth - (2 * kMargin);

        // Split into space-separated tokens and build wrapped lines greedily.
        std::string current_line;
        std::string_view remaining = text;
        while (!remaining.empty()) {
            // Extract next word (up to next space or end).
            auto space = remaining.find(' ');
            std::string word(remaining.substr(0, space));
            remaining = (space == std::string_view::npos) ? "" : remaining.substr(space + 1);

            // Build candidate by appending word to current_line (with separator if needed).
            std::string candidate = current_line;
            if (!candidate.empty())
                candidate += ' ';
            candidate += word;

            // CalculateTextAdvance sums per-glyph advance widths from the font's
            // metrics table and scales them by font size, returning a value in PDF
            // points. This is an advance-width measurement, not a visual bounding
            // box, so it slightly underestimates the visible extent of the last
            // glyph (its right-side bearing is not counted). For wrap decisions this
            // is acceptable: at worst one word is placed closer to the margin than
            // strictly necessary; text never overflows.
            double advance = measure_font->CalculateTextAdvance(candidate, kFontSize);
            if (advance > kMaxLineWidth && !current_line.empty()) {
                // Flush the current line and start a new one with this word.
                if (auto rc = write_physical_line(current_line, opts); rc != 0)
                    return rc;
                current_line = std::move(word);
            } else {
                current_line = std::move(candidate);
            }
        }
        // Write any remaining text.
        if (!current_line.empty()) {
            if (auto rc = write_physical_line(current_line, opts); rc != 0)
                return rc;
        }
        return 0;
    }
};

PdfWriter::PdfWriter(std::string_view path) : impl_(std::make_unique<Impl>()) {
    auto status = impl_->pdf.StartPDF(std::string(path), ePDFVersion17);
    if (status != PDFHummus::eSuccess) {
        spdlog::error("cannot create PDF: {}", path);
        return;
    }

    std::string font_path = find_font();
    if (font_path.empty()) {
        spdlog::error("cannot find system font for PDF writing");
        impl_->pdf.EndPDF();
        return;
    }

    impl_->font = impl_->pdf.GetFontForFile(font_path);
    if (impl_->font == nullptr) {
        spdlog::error("cannot load font '{}' for PDF writing", font_path);
        impl_->pdf.EndPDF();
        return;
    }

    std::string italic_path = find_italic_font();
    if (italic_path.empty()) {
        spdlog::warn("italic font not found, falling back to regular font for translations");
        impl_->italic_font = impl_->font;
    } else {
        impl_->italic_font = impl_->pdf.GetFontForFile(italic_path);
        if (impl_->italic_font == nullptr) {
            spdlog::warn("cannot load italic font '{}', falling back to regular", italic_path);
            impl_->italic_font = impl_->font;
        }
    }

    impl_->open = true;
    impl_->cursor_y = Impl::kPageHeight - Impl::kMargin;

    // Open the first page
    impl_->current_page = new PDFPage();
    impl_->current_page->SetMediaBox(PDFRectangle(0, 0, Impl::kPageWidth, Impl::kPageHeight));
    impl_->ctx = impl_->pdf.StartPageContentContext(impl_->current_page);
    if (impl_->ctx == nullptr) {
        spdlog::error("cannot start first page content context");
        impl_->open = false;
        impl_->pdf.EndPDF();
    }
}

PdfWriter::~PdfWriter() {
    if (!impl_ || !impl_->open)
        return;
    // Finalize the last open page
    if (impl_->ctx != nullptr) {
        impl_->pdf.EndPageContentContext(impl_->ctx);
        impl_->pdf.WritePageAndRelease(impl_->current_page);
        impl_->ctx = nullptr;
        impl_->current_page = nullptr;
    }
    impl_->pdf.EndPDF();
}

PdfWriter::PdfWriter(PdfWriter&&) noexcept = default;
PdfWriter& PdfWriter::operator=(PdfWriter&&) noexcept = default;

bool PdfWriter::is_open() const { return impl_ && impl_->open; }

int PdfWriter::start_new_page() { return impl_->start_new_page(); }

int PdfWriter::flush_page() { return impl_->flush_page(); }

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
int PdfWriter::write_pair(std::string_view original, std::string_view translation) {
    if (!is_open())
        return exit_code::output_error;

    AbstractContentContext::TextOptions regular_opts(impl_->font, Impl::kFontSize,
                                                     AbstractContentContext::eGray, 0);
    AbstractContentContext::TextOptions italic_opts(impl_->italic_font, Impl::kFontSize,
                                                    AbstractContentContext::eGray, 0);

    if (auto rc = impl_->write_line(original, impl_->font, regular_opts); rc != 0)
        return rc;
    if (auto rc = impl_->write_line(translation, impl_->italic_font, italic_opts); rc != 0)
        return rc;

    impl_->cursor_y -= Impl::kPairGap;
    return 0;
}

FormattedWriter make_pdf_formatted_writer(PdfWriter& writer) {
    return [&writer](std::string_view original, std::string_view translation) -> int {
        return writer.write_pair(original, translation);
    };
}
