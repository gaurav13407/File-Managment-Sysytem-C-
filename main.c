#include <gtk/gtk.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <jpeglib.h>
#include <png.h>
#include <hpdf.h>

GtkWidget *progress_bar;
GtkTextBuffer *log_buffer;

void log_message(const char *message) {
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(log_buffer, &end);
    gtk_text_buffer_insert(log_buffer, &end, message, -1);
    gtk_text_buffer_insert(log_buffer, &end, "\n", -1);
}

void convert_txt_to_pdf(const char *txt_file, const char *pdf_file) {
    FILE *fp = fopen(txt_file, "r");
    if (!fp) {
        log_message("Failed to open TXT file.");
        return;
    }

    HPDF_Doc pdf = HPDF_New(NULL, NULL);
    if (!pdf) {
        log_message("Failed to create PDF.");
        fclose(fp);
        return;
    }

    HPDF_Page page = HPDF_AddPage(pdf);
    HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
    HPDF_Page_SetFontAndSize(page, HPDF_GetFont(pdf, "Helvetica", NULL), 12);
    
    float y = 800;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        HPDF_Page_BeginText(page);
        HPDF_Page_TextOut(page, 50, y, line);
        HPDF_Page_EndText(page);
        y -= 15;
        if (y < 50) {
            page = HPDF_AddPage(pdf);
            y = 800;
        }
    }

    fclose(fp);
    HPDF_SaveToFile(pdf, pdf_file);
    HPDF_Free(pdf);
    log_message("TXT to PDF conversion done.");
}

void convert_png_to_jpg(const char *input_png, const char *output_jpg) {
    FILE *fp = fopen(input_png, "rb");
    if (!fp) {
        log_message("Failed to open PNG file.");
        return;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(png);
    if (!png || !info) {
        log_message("Failed to initialize libpng.");
        fclose(fp);
        return;
    }

    if (setjmp(png_jmpbuf(png))) {
        log_message("Error during PNG read.");
        fclose(fp);
        png_destroy_read_struct(&png, &info, NULL);
        return;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    int width = png_get_image_width(png, info);
    int height = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    if (bit_depth == 16) png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

    png_read_update_info(png, info);

    png_bytep *rows = (png_bytep *)malloc(sizeof(png_bytep) * height);
    for (int y = 0; y < height; y++)
        rows[y] = (png_byte *)malloc(png_get_rowbytes(png, info));

    png_read_image(png, rows);
    fclose(fp);

    FILE *out = fopen(output_jpg, "wb");
    if (!out) {
        log_message("Failed to open JPG file.");
        return;
    }

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, out);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_start_compress(&cinfo, TRUE);

    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = malloc(width * 3);
        for (int x = 0; x < width; x++) {
            row_pointer[0][x*3]     = rows[cinfo.next_scanline][x*4];
            row_pointer[0][x*3 + 1] = rows[cinfo.next_scanline][x*4 + 1];
            row_pointer[0][x*3 + 2] = rows[cinfo.next_scanline][x*4 + 2];
        }
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
        free(row_pointer[0]);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(out);

    for (int y = 0; y < height; y++) free(rows[y]);
    free(rows);
    png_destroy_read_struct(&png, &info, NULL);
    log_message("PNG to JPG conversion done.");
}

void *convert_file(void *arg) {
    char *filename = (char *)arg;
    log_message("Starting conversion...");

    if (strstr(filename, ".txt")) {
        log_message("Text file detected.");
        convert_txt_to_pdf(filename, "converted_output.pdf");
    } else if (strstr(filename, ".png")) {
        log_message("PNG file detected.");
        convert_png_to_jpg(filename, "converted_output.jpg");
    } else {
        log_message("Unsupported format.");
    }

    g_idle_add((GSourceFunc)gtk_progress_bar_pulse, progress_bar);
    log_message("Conversion complete.");
    free(filename);
    return NULL;
}

void on_file_drop(GtkWidget *widget, GdkDragContext *context,
                  gint x, gint y, GtkSelectionData *data,
                  guint info, guint time, gpointer user_data) {
    gchar **uris = gtk_selection_data_get_uris(data);
    for (int i = 0; uris[i] != NULL; i++) {
        gchar *filename = g_filename_from_uri(uris[i], NULL, NULL);
        if (filename) {
            pthread_t thread;
            pthread_create(&thread, NULL, convert_file, strdup(filename));
            pthread_detach(thread);
            g_free(filename);
        }
    }
    g_strfreev(uris);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "File Converter");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    GtkWidget *label = gtk_label_new("Drag and Drop Files Below:");
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    GtkWidget *drop_area = gtk_label_new("Drop Files Here");
    gtk_widget_set_size_request(drop_area, 200, 100);
    gtk_box_pack_start(GTK_BOX(vbox), drop_area, TRUE, TRUE, 0);

    GtkTargetEntry target = {"text/uri-list", 0, 0};
    gtk_drag_dest_set(drop_area, GTK_DEST_DEFAULT_ALL, &target, 1, GDK_ACTION_COPY);
    g_signal_connect(drop_area, "drag-data-received", G_CALLBACK(on_file_drop), NULL);

    progress_bar = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(vbox), progress_bar, FALSE, FALSE, 0);

    GtkWidget *text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), text_view, TRUE, TRUE, 0);

    log_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
