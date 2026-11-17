#ifndef PDFLIGHT_LIBS_H__
#define PDFLIGHT_LIBS_H__

/* The few poppler-glib (0.84), GLib and cairo calls pdflight uses. The
 * device ships these libraries for Green (.tmp_update/lib and lib/parasyte)
 * but the toolchain has no headers for them, so they are declared here. */

typedef struct _PopplerDocument PopplerDocument;
typedef struct _PopplerPage PopplerPage;
typedef struct {
    unsigned int domain;
    int code;
    char *message;
} GError;

PopplerDocument *poppler_document_new_from_file(const char *uri, const char *password, GError **error);
int poppler_document_get_n_pages(PopplerDocument *document);
PopplerPage *poppler_document_get_page(PopplerDocument *document, int index);
void poppler_page_get_size(PopplerPage *page, double *width, double *height);

char *g_filename_to_uri(const char *filename, const char *hostname, GError **error);
void g_object_unref(void *object);
void g_free(void *mem);

typedef struct _cairo cairo_t;
typedef struct _cairo_surface cairo_surface_t;
typedef void (*cairo_destroy_func_t)(void *data);
#define CAIRO_STATUS_SUCCESS 0
#define CAIRO_FORMAT_RGB24 1
#define CAIRO_MIME_TYPE_JPEG "image/jpeg"

void poppler_page_render(PopplerPage *page, cairo_t *cairo);

cairo_surface_t *cairo_image_surface_create(int format, int width, int height);
unsigned char *cairo_image_surface_get_data(cairo_surface_t *surface);
int cairo_image_surface_get_stride(cairo_surface_t *surface);
cairo_surface_t *cairo_pdf_surface_create(const char *filename, double width, double height);
void cairo_pdf_surface_set_size(cairo_surface_t *surface, double width, double height);
int cairo_surface_set_mime_data(cairo_surface_t *surface, const char *mime_type, const unsigned char *data,
                                unsigned long length, cairo_destroy_func_t destroy, void *closure);
void cairo_surface_flush(cairo_surface_t *surface);
void cairo_surface_finish(cairo_surface_t *surface);
int cairo_surface_status(cairo_surface_t *surface);
void cairo_surface_destroy(cairo_surface_t *surface);
cairo_t *cairo_create(cairo_surface_t *target);
int cairo_status(cairo_t *cr);
void cairo_destroy(cairo_t *cr);
void cairo_save(cairo_t *cr);
void cairo_restore(cairo_t *cr);
void cairo_scale(cairo_t *cr, double sx, double sy);
void cairo_set_source_rgb(cairo_t *cr, double red, double green, double blue);
void cairo_set_source_surface(cairo_t *cr, cairo_surface_t *surface, double x, double y);
void cairo_paint(cairo_t *cr);
void cairo_show_page(cairo_t *cr);

#endif // PDFLIGHT_LIBS_H__
