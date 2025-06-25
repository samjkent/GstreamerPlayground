#include <gst/gst.h>
#include <gio/gio.h>

#define GST_G_IO_MODULE_DECLARE(name) \
extern void G_PASTE(g_io_, G_PASTE(name, _load)) (gpointer data)

#define GST_G_IO_MODULE_LOAD(name) \
G_PASTE(g_io_, G_PASTE(name, _load)) (NULL)

/* Declaration of static plugins */
  GST_PLUGIN_STATIC_DECLARE(coreelements);  GST_PLUGIN_STATIC_DECLARE(ogg);  GST_PLUGIN_STATIC_DECLARE(theora);  GST_PLUGIN_STATIC_DECLARE(vorbis);  GST_PLUGIN_STATIC_DECLARE(audioconvert);  GST_PLUGIN_STATIC_DECLARE(audioresample);  GST_PLUGIN_STATIC_DECLARE(playback);  GST_PLUGIN_STATIC_DECLARE(soup);  GST_PLUGIN_STATIC_DECLARE(opensles);

/* Declaration of static gio modules */
 

/* Call this function to load GIO modules */
static void
gst_android_load_gio_modules (void)
{
  GTlsBackend *backend;
  const gchar *ca_certs;

  

  ca_certs = g_getenv ("CA_CERTIFICATES");

  backend = g_tls_backend_get_default ();
  if (backend && ca_certs) {
    GTlsDatabase *db;
    GError *error = NULL;

    db = g_tls_file_database_new (ca_certs, &error);
    if (db) {
      g_tls_backend_set_default_database (backend, db);
      g_object_unref (db);
    } else {
      g_warning ("Failed to create a database from file: %s",
          error ? error->message : "Unknown");
    }
  }
}

/* This is called by gst_init() */
void
gst_init_static_plugins (void)
{
   GST_PLUGIN_STATIC_REGISTER(coreelements);  GST_PLUGIN_STATIC_REGISTER(ogg);  GST_PLUGIN_STATIC_REGISTER(theora);  GST_PLUGIN_STATIC_REGISTER(vorbis);  GST_PLUGIN_STATIC_REGISTER(audioconvert);  GST_PLUGIN_STATIC_REGISTER(audioresample);  GST_PLUGIN_STATIC_REGISTER(playback);  GST_PLUGIN_STATIC_REGISTER(soup);  GST_PLUGIN_STATIC_REGISTER(opensles);
  gst_android_load_gio_modules ();
}
