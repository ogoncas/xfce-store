/* xfce-store: Flathub + Debian (apt) app store for XFCE, GTK3 */
#define _GNU_SOURCE
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <string.h>
#include <sys/utsname.h>

#define APP_VERSION "1.2"
#define APP_ID "io.github.xfcestore.Store"   /* replace with an ID under a domain/account you own */
#define ICON_SZ 40
#define FLATHUB_URL "https://dl.flathub.org/repo/flathub.flatpakrepo"
#define CACHE_VER "v3"
#define APPSTREAM_MAX_AGE (24 * 3600)        /* seconds before the Flatpak appstream is refreshed on start */
#define _(s) tr(s)

typedef enum { SRC_FLATPAK, SRC_APT, N_SRC } Src;
static const char *SRC_NAME[N_SRC] = {"Flatpak", "Debian"};
typedef struct { char *name, *id, *desc, *icon, *markup, *remote, *skey; Src src; guint cats; } Entry;

/* ---------- i18n: English is the source language and the fallback ---------- */
static GHashTable *TR;

static const char *tr(const char *s) {
    const char *t = TR ? g_hash_table_lookup(TR, s) : NULL;
    return t ? t : s;
}

static void i18n_init(const char *lang) {   /* lang: "auto", "en" or "pt" */
    static const char *PT[][2] = {
        {"Software Store", "Loja de Aplicativos"},
        {"Install apps from Flathub and the Debian repositories.", "Instale apps do Flathub e dos repositórios Debian."},
        {"All apps", "Todos os apps"}, {"Installed", "Instalados"}, {"Multimedia", "Multimídia"},
        {"Office", "Escritório"}, {"Graphics", "Gráficos"}, {"Games", "Jogos"},
        {"Development", "Desenvolvimento"}, {"Education & Science", "Educação e Ciência"},
        {"Utilities", "Utilitários"}, {"System", "Sistema"},
        {"Search apps…", "Buscar aplicativos…"}, {"All sources", "Todas as fontes"},
        {"Install", "Instalar"}, {"Remove", "Remover"}, {"✔ Installed", "✔ Instalado"},
        {"%u apps", "%u apps"}, {"No apps found", "Nenhum app encontrado"},
        {"Loading catalogs…", "Carregando catálogos…"},
        {"Updating Flatpak catalogs…", "Atualizando catálogos Flatpak…"},
        {"Installing %s…", "Instalando %s…"}, {"Removing %s…", "Removendo %s…"},
        {"Updating package lists (apt update)…", "Atualizando lista de pacotes (apt update)…"},
        {"Details (log)", "Detalhes (log)"}, {"Reload catalogs", "Recarregar catálogos"},
        {"Update package lists", "Atualizar lista de pacotes"}, {"About", "Sobre"},
        {"Settings", "Configurações"}, {"Close", "Fechar"}, {"Flatpak repositories", "Repositórios Flatpak"},
        {"Enable Flathub", "Ativar o Flathub"}, {"Flathub is enabled.", "O Flathub está ativado."},
        {"Flathub is not enabled.", "O Flathub não está ativado."}, {"Name", "Nome"}, {"Add", "Adicionar"},
        {"Remove selected", "Remover selecionado"}, {"Repository name", "Nome do repositório"},
        {"Repository URL (.flatpakrepo)", "URL do repositório (.flatpakrepo)"},
        {"Show log area on the main screen", "Mostrar a área de logs na tela inicial"},
        {"Initial setup", "Configuração inicial"},
        {"Welcome! Choose how the store should start. You can change this later in Settings.",
         "Bem-vindo! Escolha como a loja deve começar. Você pode mudar isso depois em Configurações."},
        {"Enable Flathub (recommended)", "Ativar o Flathub (recomendado)"}, {"Continue", "Continuar"},
        {"flatpak is not installed.", "O flatpak não está instalado."}, {"Done.", "Concluído."},
        {"Language", "Idioma"}, {"Type", "Tipo"}, {"Automatic (system)", "Automático (sistema)"}, {"User", "Usuário"},
        {"Restart the store to apply the language.", "Reinicie a loja para aplicar o idioma."},
        {"Flathub is already enabled.", "O Flathub já está ativado."},
        {"Operation failed. Enable the log in Settings for details.",
         "A operação falhou. Ative o log em Configurações para ver os detalhes."},
        {"flatpak not found (sudo apt install flatpak): only Debian will be available.",
         "flatpak não encontrado (sudo apt install flatpak): só o Debian estará disponível."},
        {"== Done ==", "== Concluído =="}, {"== Failed ==", "== Falhou =="},
        {"Cancel", "Cancelar"},
        {"Checking what the removal would affect…", "Verificando o que a remoção afetaria…"},
        {"Removing %s will also remove these packages:", "Remover %s também removerá estes pacotes:"},
        {"An operation is still running. Wait for it to finish before closing.",
         "Uma operação ainda está em andamento. Aguarde o fim antes de fechar."}};
    /* Brazilian Portuguese is the only Portuguese translation; it is used for every pt_* locale
       because it is closer to pt_PT than English is. */
    gboolean pt = !g_strcmp0(lang, "pt") || (g_strcmp0(lang, "en") && g_str_has_prefix(g_get_language_names()[0], "pt"));
    if (!pt) return;
    TR = g_hash_table_new(g_str_hash, g_str_equal);
    for (guint i = 0; i < G_N_ELEMENTS(PT); i++) g_hash_table_insert(TR, (gpointer)PT[i][0], (gpointer)PT[i][1]);
}

/* ---------- categories ---------- */
enum { CAT_ALL, CAT_INST, CAT_NET, CAT_MEDIA, CAT_OFFICE, CAT_GFX, CAT_GAMES, CAT_DEV, CAT_EDU, CAT_UTIL, CAT_SYS, N_CATS };
static const char *CAT_NAME[] = {"All apps", "Installed", "Internet", "Multimedia", "Office", "Graphics", "Games",
    "Development", "Education & Science", "Utilities", "System"};
static const char *CAT_ICON[] = {"view-grid-symbolic", "emblem-ok-symbolic", "network-workgroup-symbolic",
    "applications-multimedia-symbolic", "x-office-document-symbolic", "applications-graphics-symbolic",
    "applications-games-symbolic", "applications-engineering-symbolic", "applications-science-symbolic",
    "applications-utilities-symbolic", "preferences-system-symbolic"};
/* freedesktop categories -> store categories */
static const char *CAT_KEYS[] = {NULL, NULL,
    " Network WebBrowser Email Chat InstantMessaging IRCClient P2P FileTransfer News RemoteAccess VideoConference Feed ",
    " AudioVideo Audio Video Music Player Recorder Midi Mixer Sequencer Tuner TV DiscBurning AudioVideoEditing ",
    " Office WordProcessor Spreadsheet Presentation Calendar ContactManagement Database Finance Dictionary Publishing ProjectManagement ",
    " Graphics 2DGraphics 3DGraphics RasterGraphics VectorGraphics Photography Scanning OCR Viewer ",
    " Game ArcadeGame BoardGame CardGame RolePlaying Simulation StrategyGame LogicGame ActionGame AdventureGame BlocksGame Emulator Sports ",
    " Development IDE Debugger Building GUIDesigner Profiling RevisionControl WebDevelopment Translation ",
    " Education Science Math Astronomy Biology Chemistry Physics Geography Geology Electronics Engineering Languages Literature Maps History Robotics ArtificialIntelligence DataVisualization MedicalSoftware NumericalAnalysis ",
    " Utility TextEditor Archiving Compression FileManager Calculator Accessibility Clock TextTools Documentation ",
    " System Settings Monitor PackageManager Security TerminalEmulator FileTools Filesystem HardwareSettings DesktopSettings Printing "};

static guint cat_bit(const char *c) {
    char *t = g_strdup_printf(" %s ", c); guint r = 0;
    for (int i = CAT_NET; i < N_CATS; i++) if (strstr(CAT_KEYS[i], t)) r |= 1u << i;
    g_free(t); return r;
}

static guint sec_bit(const char *s) {   /* Debian section -> category */
    static const struct { const char *n; int c; } M[] = {
        {"games", CAT_GAMES}, {"graphics", CAT_GFX}, {"sound", CAT_MEDIA}, {"video", CAT_MEDIA},
        {"web", CAT_NET}, {"net", CAT_NET}, {"mail", CAT_NET}, {"news", CAT_NET}, {"comm", CAT_NET}, {"httpd", CAT_NET},
        {"text", CAT_OFFICE}, {"doc", CAT_OFFICE}, {"tex", CAT_OFFICE}, {"database", CAT_OFFICE},
        {"devel", CAT_DEV}, {"vcs", CAT_DEV}, {"debug", CAT_DEV}, {"java", CAT_DEV}, {"python", CAT_DEV},
        {"perl", CAT_DEV}, {"ruby", CAT_DEV}, {"golang", CAT_DEV}, {"rust", CAT_DEV}, {"haskell", CAT_DEV},
        {"lisp", CAT_DEV}, {"ocaml", CAT_DEV}, {"php", CAT_DEV}, {"javascript", CAT_DEV}, {"interpreters", CAT_DEV},
        {"embedded", CAT_DEV},
        {"science", CAT_EDU}, {"math", CAT_EDU}, {"electronics", CAT_EDU}, {"hamradio", CAT_EDU}, {"education", CAT_EDU},
        {"admin", CAT_SYS}, {"kernel", CAT_SYS}, {"shells", CAT_SYS}, {"otherosfs", CAT_SYS},
        {"x11", CAT_UTIL}, {"gnome", CAT_UTIL}, {"kde", CAT_UTIL}, {"xfce", CAT_UTIL}, {"editors", CAT_UTIL},
        {"utils", CAT_UTIL}, {"fonts", CAT_UTIL}};
    if (s && strchr(s, '/')) s = strchr(s, '/') + 1;      /* strip "contrib/", "non-free/" */
    for (guint i = 0; s && i < G_N_ELEMENTS(M); i++) if (!strcmp(s, M[i].n)) return 1u << M[i].c;
    return 1u << CAT_UTIL;
}

static void entry_free(gpointer p) {
    Entry *e = p; g_free(e->name); g_free(e->id); g_free(e->desc); g_free(e->icon); g_free(e->markup);
    g_free(e->remote); g_free(e->skey); g_free(e);
}

/* ---------- on-disk catalog cache (tab separated, keyed by source mtime) ---------- */
static gint64 mtime(const char *path) { GStatBuf st; return g_stat(path, &st) == 0 ? (gint64)st.st_mtime : 0; }

static char *cache_file(Src s) {
    char *dir = g_build_filename(g_get_user_cache_dir(), "xfce-store", NULL);
    g_mkdir_with_parents(dir, 0755);
    char *f = g_build_filename(dir, s == SRC_FLATPAK ? "flathub.tsv" : "apt.tsv", NULL);
    g_free(dir); return f;
}

static GPtrArray *cache_load(Src s, gint64 key) {
    char *f = cache_file(s), *txt = NULL, *hdr = g_strdup_printf(CACHE_VER "\t%" G_GINT64_FORMAT, key);
    GPtrArray *out = NULL;
    if (key && g_file_get_contents(f, &txt, NULL, NULL)) {
        char **l = g_strsplit(txt, "\n", -1);
        if (!g_strcmp0(l[0], hdr)) {
            out = g_ptr_array_new_with_free_func(entry_free);
            for (int i = 1; l[i]; i++) {
                char **c = g_strsplit(l[i], "\t", 6);
                if (g_strv_length(c) != 6) { g_strfreev(c); continue; }
                Entry *e = g_new0(Entry, 1);
                e->name = c[0]; e->id = c[1]; e->desc = c[2]; e->src = s;
                e->cats = g_ascii_strtoull(c[5], NULL, 10);
                if (*c[3]) e->icon = c[3]; else g_free(c[3]);
                if (*c[4]) e->remote = c[4]; else g_free(c[4]);
                g_free(c[5]); g_free(c);
                g_ptr_array_add(out, e);
            }
        }
        g_strfreev(l); g_free(txt);
    }
    g_free(f); g_free(hdr); return out;
}

static void put(GString *g, const char *s) {
    for (; s && *s; s++) g_string_append_c(g, *s == '\t' || *s == '\n' || *s == '\r' ? ' ' : *s);
    g_string_append_c(g, '\t');
}

static void cache_save(Src s, gint64 key, GPtrArray *a) {
    if (!key) return;
    GString *g = g_string_new(NULL);
    g_string_append_printf(g, CACHE_VER "\t%" G_GINT64_FORMAT "\n", key);
    for (guint i = 0; i < a->len; i++) {
        Entry *e = a->pdata[i];
        put(g, e->name); put(g, e->id); put(g, e->desc); put(g, e->icon); put(g, e->remote);
        g_string_append_printf(g, "%u\n", e->cats);
    }
    char *f = cache_file(s); g_file_set_contents(f, g->str, g->len, NULL);
    g_free(f); g_string_free(g, TRUE);
}

/* ---------- background load job: task data shared by both catalog threads ---------- */
typedef struct { char **specs; guint gen; } Load;   /* specs: "scope\tname" of Flatpak remotes (NULL for apt) */
static void load_free(gpointer p) { Load *l = p; g_strfreev(l->specs); g_free(l); }

/* ---------- Flathub catalog: appstream.xml.gz parsed in a worker thread ---------- */
enum { F_NONE, F_ID, F_NAME, F_SUM, F_CAT, F_ICON };
typedef struct { GPtrArray *out; Entry *cur; GString *txt; int depth, field; const char *dir, *remote; } P;

static const char *attr(const char **n, const char **v, const char *k) {
    for (; *n; n++, v++) if (!strcmp(*n, k)) return *v;
    return NULL;
}

static void p_start(GMarkupParseContext *c, const char *el, const char **an, const char **av, gpointer d, GError **e) {
    P *p = d; p->depth++;
    if (p->depth == 2 && !strcmp(el, "component")) {
        const char *t = attr(an, av, "type");
        p->cur = t && (!strcmp(t, "desktop-application") || !strcmp(t, "desktop")) ? g_new0(Entry, 1) : NULL;
        if (p->cur) { p->cur->src = SRC_FLATPAK; p->cur->remote = g_strdup(p->remote); }
    } else if (p->cur && p->depth == 3 && !attr(an, av, "xml:lang")) {   /* untranslated fields only */
        if (!strcmp(el, "id")) p->field = F_ID;
        else if (!strcmp(el, "name")) p->field = F_NAME;
        else if (!strcmp(el, "summary")) p->field = F_SUM;
        else if (!strcmp(el, "icon") && !g_strcmp0(attr(an, av, "type"), "cached") && !g_strcmp0(attr(an, av, "width"), "64"))
            p->field = F_ICON;
    } else if (p->cur && p->depth == 4 && !strcmp(el, "category")) p->field = F_CAT;
    if (p->field) g_string_truncate(p->txt, 0);
}

static void p_end(GMarkupParseContext *c, const char *el, gpointer d, GError **e) {
    P *p = d;
    if (p->field) {
        const char *t = p->txt->str; Entry *x = p->cur;
        switch (p->field) {
        case F_ID: if (!x->id) { x->id = g_strdup(t); if (g_str_has_suffix(x->id, ".desktop")) x->id[strlen(x->id) - 8] = 0; } break;
        case F_NAME: if (!x->name) x->name = g_strdup(t); break;
        case F_SUM: if (!x->desc) x->desc = g_strdup(t); break;
        case F_CAT: x->cats |= cat_bit(t); break;
        case F_ICON: if (!x->icon) x->icon = g_build_filename(p->dir, "icons", "64x64", t, NULL); break;
        }
        p->field = F_NONE;
    }
    if (p->depth == 2 && p->cur) {
        if (!p->cur->cats) p->cur->cats = 1u << CAT_UTIL;           /* uncategorized apps are still browsable */
        if (p->cur->id && p->cur->name) g_ptr_array_add(p->out, p->cur); else entry_free(p->cur);
        p->cur = NULL;
    }
    p->depth--;
}

static void p_text(GMarkupParseContext *c, const char *t, gsize len, gpointer d, GError **e) {
    P *p = d; if (p->field) g_string_append_len(p->txt, t, len);
}

/* Flatpak's own architecture names differ from uname() on 32-bit x86 and ARM */
static const char *fp_arch(void) {
    static char arch[32]; struct utsname u; uname(&u);
    const char *m = u.machine;
    if (!strcmp(m, "i686") || !strcmp(m, "i586") || !strcmp(m, "i486") || !strcmp(m, "i386")) m = "i386";
    else if (g_str_has_prefix(m, "armv")) m = "arm";
    g_strlcpy(arch, m, sizeof arch);
    return arch;
}

static char *fp_dir(const char *remote, const char *scope) {   /* appstream dir of a remote */
    const char *env = g_getenv(!strcmp(scope, "user") ? "FLATPAK_USER_DIR" : "FLATPAK_SYSTEM_DIR");
    char *base = env && *env ? g_strdup(env)
               : !strcmp(scope, "user") ? g_build_filename(g_get_user_data_dir(), "flatpak", NULL)
               : g_strdup("/var/lib/flatpak");
    char *d = g_build_filename(base, "appstream", remote, fp_arch(), "active", NULL);
    g_free(base); return d;
}

static void parse_appstream(GPtrArray *out, const char *remote, const char *file, const char *dir) {
    GFile *gf = g_file_new_for_path(file);
    GFileInputStream *fs = g_file_read(gf, NULL, NULL);
    if (fs) {
        GZlibDecompressor *z = g_zlib_decompressor_new(G_ZLIB_COMPRESSOR_FORMAT_GZIP);
        GInputStream *in = g_converter_input_stream_new(G_INPUT_STREAM(fs), G_CONVERTER(z));
        P p = {out, NULL, g_string_new(NULL), 0, 0, dir, remote};
        GMarkupParser mp = {p_start, p_end, p_text, NULL, NULL};
        GMarkupParseContext *mc = g_markup_parse_context_new(&mp, 0, &p, NULL);
        char buf[65536]; gssize n;
        while ((n = g_input_stream_read(in, buf, sizeof buf, NULL, NULL)) > 0)
            if (!g_markup_parse_context_parse(mc, buf, n, NULL)) break;
        if (p.cur) entry_free(p.cur);
        g_markup_parse_context_free(mc); g_string_free(p.txt, TRUE);
        g_object_unref(in); g_object_unref(z); g_object_unref(fs);
    }
    g_object_unref(gf);
}

static char *appstream_file(const char *spec, char **name, char **dir) {   /* spec: "scope\tname" */
    char **p = g_strsplit(spec, "\t", 2);
    *name = g_strdup(p[1]); *dir = fp_dir(p[1], p[0]); g_strfreev(p);
    return g_build_filename(*dir, "appstream.xml.gz", NULL);
}

static void fp_thread(GTask *t, gpointer src, gpointer data, GCancellable *c) {
    char **specs = ((Load *)data)->specs, *n, *d;
    gint64 key = 0;                                 /* changes when a remote or its appstream changes */
    for (int i = 0; specs[i]; i++) {
        char *f = appstream_file(specs[i], &n, &d);
        key += mtime(f) + g_str_hash(specs[i]); g_free(f); g_free(n); g_free(d);
    }
    GPtrArray *out = cache_load(SRC_FLATPAK, key);
    if (!out) {
        out = g_ptr_array_new_with_free_func(entry_free);
        for (int i = 0; specs[i]; i++) {
            char *f = appstream_file(specs[i], &n, &d);
            parse_appstream(out, n, f, d); g_free(f); g_free(n); g_free(d);
        }
        cache_save(SRC_FLATPAK, key, out);
    }
    g_task_return_pointer(t, out, (GDestroyNotify)g_ptr_array_unref);
}

/* ---------- Debian catalog: AppStream (DEP-11) first, Debtags via apt-cache dumpavail as a complement ---------- */
static const char *ICON_BASES[] = {"/var/lib/swcatalog/icons", "/var/lib/app-info/icons",
                                   "/usr/share/swcatalog/icons", "/usr/share/app-info/icons", NULL};
static const char *YAML_BASES[] = {"/var/lib/swcatalog/yaml", "/var/lib/app-info/yaml",
                                   "/usr/share/swcatalog/yaml", "/usr/share/app-info/yaml", NULL};

static GHashTable *apt_icon_index(void) {   /* AppStream icon files are named <package>_<icon>.png */
    GHashTable *ic = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    for (int i = 0; ICON_BASES[i]; i++) {
        GDir *d = g_dir_open(ICON_BASES[i], 0, NULL); const char *s;
        while (d && (s = g_dir_read_name(d))) {
            char *sd = g_build_filename(ICON_BASES[i], s, "64x64", NULL);
            GDir *d2 = g_dir_open(sd, 0, NULL); const char *f;
            while (d2 && (f = g_dir_read_name(d2))) {
                const char *u = strchr(f, '_'); if (!u) continue;
                char *k = g_strndup(f, u - f);
                if (g_hash_table_contains(ic, k)) g_free(k); else g_hash_table_insert(ic, k, g_build_filename(sd, f, NULL));
            }
            if (d2) g_dir_close(d2);
            g_free(sd);
        }
        if (d) g_dir_close(d);
    }
    return ic;
}

/* --- DEP-11 YAML (the AppStream catalog that apt/appstream keep in /var/lib/swcatalog/yaml) --- */
enum { D_NONE, D_NAME, D_SUM, D_CAT };
typedef struct { char *pkg, *name, *sum; guint cats; gboolean app; } Dep11;

static char *yaml_str(const char *s) {   /* trims and unquotes a YAML scalar */
    while (*s == ' ') s++;
    gsize n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\r')) n--;
    GString *g = g_string_new(NULL);
    if (n >= 2 && (s[0] == '\'' || s[0] == '"') && s[n - 1] == s[0]) {
        char q = s[0];
        for (gsize i = 1; i < n - 1; i++) {
            if (q == '\'' && s[i] == '\'' && s[i + 1] == '\'') i++;          /* '' -> ' */
            else if (q == '"' && s[i] == '\\' && i + 1 < n - 1) i++;         /* \" -> " */
            g_string_append_c(g, s[i]);
        }
    } else g_string_append_len(g, s, n);
    return g_string_free(g, FALSE);
}

static void dep11_flush(Dep11 *d, GPtrArray *out, GHashTable *seen, GHashTable *ic) {
    if (d->app && d->pkg && d->name && !g_hash_table_contains(seen, d->pkg)) {
        Entry *e = g_new0(Entry, 1);
        const char *ip = g_hash_table_lookup(ic, d->pkg);
        e->name = d->name; d->name = NULL;
        e->desc = d->sum ? d->sum : g_strdup(""); d->sum = NULL;
        e->id = g_strdup(d->pkg); e->icon = g_strdup(ip ? ip : d->pkg);
        e->src = SRC_APT; e->cats = d->cats ? d->cats : 1u << CAT_UTIL;
        g_hash_table_add(seen, g_strdup(d->pkg)); g_ptr_array_add(out, e);
    }
    g_free(d->pkg); g_free(d->name); g_free(d->sum); memset(d, 0, sizeof *d);
}

static void scan_dep11_file(const char *path, GPtrArray *out, GHashTable *seen, GHashTable *ic) {
    GFile *gf = g_file_new_for_path(path);
    GFileInputStream *fs = g_file_read(gf, NULL, NULL);
    if (fs) {
        GInputStream *in = G_INPUT_STREAM(fs); GZlibDecompressor *z = NULL;
        if (g_str_has_suffix(path, ".gz")) {
            z = g_zlib_decompressor_new(G_ZLIB_COMPRESSOR_FORMAT_GZIP);
            in = g_converter_input_stream_new(G_INPUT_STREAM(fs), G_CONVERTER(z));
        }
        GDataInputStream *ds = g_data_input_stream_new(in);
        Dep11 d; memset(&d, 0, sizeof d);
        int sect = D_NONE; char *l;
        while ((l = g_data_input_stream_read_line(ds, NULL, NULL, NULL))) {
            if (!strcmp(l, "---") || !strcmp(l, "...")) { dep11_flush(&d, out, seen, ic); sect = D_NONE; }
            else if (g_str_has_prefix(l, "- ")) {                    /* list item of a top-level key */
                if (sect == D_CAT) { char *c = yaml_str(l + 2); d.cats |= cat_bit(c); g_free(c); }
            } else if (*l == ' ') {                                   /* nested value: only the untranslated "C" locale */
                const char *t = l; while (*t == ' ') t++;
                if (sect == D_NAME && !d.name && g_str_has_prefix(t, "C: ")) d.name = yaml_str(t + 3);
                else if (sect == D_SUM && !d.sum && g_str_has_prefix(t, "C: ")) d.sum = yaml_str(t + 3);
            } else if (*l && *l != '#') {                             /* top-level key */
                char *c = strchr(l, ':');
                if (c) {
                    gsize n = c - l; sect = D_NONE;
                    if (n == 4 && !strncmp(l, "Type", 4)) { char *t = yaml_str(c + 1); d.app = !strcmp(t, "desktop-application"); g_free(t); }
                    else if (n == 7 && !strncmp(l, "Package", 7)) { g_free(d.pkg); d.pkg = yaml_str(c + 1); }
                    else if (n == 4 && !strncmp(l, "Name", 4)) sect = D_NAME;
                    else if (n == 7 && !strncmp(l, "Summary", 7)) sect = D_SUM;
                    else if (n == 10 && !strncmp(l, "Categories", 10)) sect = D_CAT;
                }
            }
            g_free(l);
        }
        dep11_flush(&d, out, seen, ic);
        g_object_unref(ds);
        if (z) { g_object_unref(in); g_object_unref(z); }
        g_object_unref(fs);
    }
    g_object_unref(gf);
}

static void scan_dep11(GPtrArray *out, GHashTable *seen, GHashTable *ic) {
    for (int i = 0; YAML_BASES[i]; i++) {
        GDir *d = g_dir_open(YAML_BASES[i], 0, NULL); const char *f;
        while (d && (f = g_dir_read_name(d)))
            if (g_str_has_suffix(f, ".yml.gz") || g_str_has_suffix(f, ".yml")) {
                char *p = g_build_filename(YAML_BASES[i], f, NULL);
                scan_dep11_file(p, out, seen, ic); g_free(p);
            }
        if (d) g_dir_close(d);
    }
}

/* --- Debtags: graphical programs without an AppStream entry --- */
/* takes ownership of id/sec/desc; keeps only graphical programs */
static void apt_add(GPtrArray *out, GHashTable *seen, GHashTable *ic, char *id, char *sec, char *desc, GString *tags) {
    if (id && tags->len && strstr(tags->str, "role::program") &&
        (strstr(tags->str, "interface::x11") || strstr(tags->str, "uitoolkit::")) && !g_hash_table_contains(seen, id)) {
        Entry *e = g_new0(Entry, 1);
        const char *ip = g_hash_table_lookup(ic, id);
        e->name = g_strdup(id); e->id = g_strdup(id); e->desc = g_strdup(desc ? desc : "");
        e->icon = g_strdup(ip ? ip : id); e->src = SRC_APT; e->cats = sec_bit(sec);
        g_hash_table_add(seen, g_strdup(id)); g_ptr_array_add(out, e);
    }
    g_free(id); g_free(sec); g_free(desc); g_string_truncate(tags, 0);
}

static GPtrArray *scan_apt(void) {
    GPtrArray *out = g_ptr_array_new_with_free_func(entry_free);
    GHashTable *ic = apt_icon_index(), *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    scan_dep11(out, seen, ic);
    const char *v[] = {"apt-cache", "dumpavail", NULL};
    GSubprocess *sp = g_subprocess_newv(v, G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE, NULL);
    if (sp) {
        GDataInputStream *ds = g_data_input_stream_new(g_subprocess_get_stdout_pipe(sp));
        GString *tags = g_string_new(NULL); char *id = NULL, *sec = NULL, *desc = NULL; gboolean in_tag = FALSE;
        for (;;) {
            char *l = g_data_input_stream_read_line(ds, NULL, NULL, NULL);
            if (!l || !*l) {                         /* end of paragraph */
                apt_add(out, seen, ic, id, sec, desc, tags); id = sec = desc = NULL; in_tag = FALSE;
                if (!l) break;
            } else if (*l == ' ' || *l == '\t') { if (in_tag) g_string_append(tags, l); }   /* folded Tag: line */
            else {
                in_tag = FALSE;
                if (g_str_has_prefix(l, "Package: ")) { g_free(id); id = g_strdup(l + 9); }
                else if (g_str_has_prefix(l, "Section: ")) { g_free(sec); sec = g_strdup(l + 9); }
                else if ((g_str_has_prefix(l, "Description:") || g_str_has_prefix(l, "Description-en:")) && !desc) {
                    const char *c = strchr(l, ':') + 1; while (*c == ' ') c++;
                    desc = g_strdup(c);
                } else if (g_str_has_prefix(l, "Tag: ")) { in_tag = TRUE; g_string_assign(tags, l + 5); }
            }
            g_free(l);
        }
        g_string_free(tags, TRUE); g_object_unref(ds); g_object_unref(sp);
    }
    g_hash_table_destroy(ic); g_hash_table_destroy(seen);
    return out;
}

static gint64 apt_key(void) {   /* changes when the apt index or any AppStream data changes */
    gint64 key = mtime("/var/cache/apt/pkgcache.bin");
    for (int i = 0; ICON_BASES[i]; i++) key += mtime(ICON_BASES[i]);
    for (int i = 0; YAML_BASES[i]; i++) {
        GDir *d = g_dir_open(YAML_BASES[i], 0, NULL); const char *f;
        while (d && (f = g_dir_read_name(d))) { char *p = g_build_filename(YAML_BASES[i], f, NULL); key += mtime(p); g_free(p); }
        if (d) g_dir_close(d);
    }
    return key;
}

static void apt_thread(GTask *t, gpointer src, gpointer data, GCancellable *c) {
    gint64 key = apt_key();
    GPtrArray *out = cache_load(SRC_APT, key);
    if (!out) { out = scan_apt(); cache_save(SRC_APT, key, out); }
    g_task_return_pointer(t, out, (GDestroyNotify)g_ptr_array_unref);
}

/* ---------- application state and helpers ---------- */
typedef struct {
    GtkWidget *win, *entry, *combo, *tree, *btn, *prog, *info, *log, *exp;
    GtkWidget *prefs, *pf_status, *pf_btn, *pf_msg, *pf_name, *pf_url, *pf_list;
    GtkListStore *store, *rstore;   /* rstore: name, url, disabled, scope, scope label */
    GPtrArray *catalog[N_SRC], *all, *extra;   /* all: merged+sorted view (not owner); extra: apt-cache hits */
    GHashTable *inst[N_SRC], *apt_ids, *icons;
    int cat, busy; guint gen, pulse, load_gen[N_SRC];
    gboolean acting, has_fp, flathub_on, show_log, setup_done, setup_asked, fail;
    char *lang;
} App;

typedef void (*DoneFn)(App *, gboolean ok, const char *out, gpointer ud);
typedef struct { App *a; DoneFn fn; gpointer ud; GSubprocess *p; } Job;

static gboolean have(const char *prog) { char *s = g_find_program_in_path(prog); gboolean r = s != NULL; g_free(s); return r; }
static void set_status(App *a, const char *t) { gtk_label_set_text(GTK_LABEL(a->info), t); }
static gboolean is_inst(App *a, Entry *e) { return g_hash_table_contains(a->inst[e->src], e->id); }
static const char *src_label(Entry *e) { return !e->remote ? SRC_NAME[SRC_APT] : !strcmp(e->remote, "flathub") ? "Flathub" : e->remote; }

static gboolean pulse_cb(gpointer d) { gtk_progress_bar_pulse(GTK_PROGRESS_BAR(((App *)d)->prog)); return TRUE; }

static void busy_add(App *a, int d) {
    a->busy += d;
    gtk_widget_set_visible(a->prog, a->busy > 0);
    if (a->busy > 0 && !a->pulse) a->pulse = g_timeout_add(100, pulse_cb, a);
    if (a->busy <= 0 && a->pulse) { g_source_remove(a->pulse); a->pulse = 0; }
}

static void log_append(App *a, const char *t) {
    GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(a->log));
    GtkTextIter it; gtk_text_buffer_get_end_iter(b, &it);
    gtk_text_buffer_insert(b, &it, t, -1); gtk_text_buffer_insert(b, &it, "\n", -1);
}

static void job_done(GObject *o, GAsyncResult *r, gpointer d) {
    Job *j = d; char *out = NULL;
    g_subprocess_communicate_utf8_finish(G_SUBPROCESS(o), r, &out, NULL, NULL);
    j->fn(j->a, g_subprocess_get_successful(j->p), out ? out : "", j->ud);
    g_free(out); g_object_unref(j->p); g_free(j);
}

/* Runs a command asynchronously; fn must call busy_add(a, -1).
   stdin is closed right away so nothing can wait for input; c_locale forces LC_ALL=C for commands
   whose output is parsed, so column formats and keywords never depend on the user's language. */
static void run_ex(App *a, const char *const *argv, DoneFn fn, gpointer ud, gboolean c_locale) {
    GError *e = NULL; busy_add(a, 1);
    GSubprocessLauncher *l = g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                                       G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_MERGE);
    if (c_locale) g_subprocess_launcher_setenv(l, "LC_ALL", "C", TRUE);
    GSubprocess *p = g_subprocess_launcher_spawnv(l, argv, &e);
    g_object_unref(l);
    if (!p) { log_append(a, e->message); g_error_free(e); fn(a, FALSE, "", ud); return; }
    Job *j = g_new0(Job, 1); j->a = a; j->fn = fn; j->ud = ud; j->p = p;
    g_subprocess_communicate_utf8_async(p, NULL, NULL, job_done, j);
}
#define run(a, v, f, u) run_ex(a, v, f, u, FALSE)

static Entry *selected(App *a) {
    GtkTreeModel *m; GtkTreeIter it; Entry *e = NULL;
    if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(a->tree)), &m, &it))
        gtk_tree_model_get(m, &it, 0, &e, -1);
    return e;
}

static void refresh_btn(App *a) {
    Entry *e = selected(a);
    gtk_widget_set_sensitive(a->btn, e && !a->acting);
    gtk_button_set_label(GTK_BUTTON(a->btn), e && is_inst(a, e) ? _("Remove") : _("Install"));
    if (!e || a->fail) return;
    char *s = g_strdup_printf("%s  ·  %s  ·  %s", e->name, src_label(e), e->id);
    set_status(a, s); g_free(s);
}

/* ---------- list view ---------- */
static GdkPixbuf *icon_for(App *a, Entry *e) {
    const char *k = e->icon && *e->icon ? e->icon : e->id;   /* never share one cache slot between different apps */
    GdkPixbuf *pb = g_hash_table_lookup(a->icons, k);
    if (pb) return pb;
    GtkIconTheme *th = gtk_icon_theme_get_default();
    if (*k == '/') pb = gdk_pixbuf_new_from_file_at_size(k, ICON_SZ, ICON_SZ, NULL);
    for (const char *n[] = {k, e->id, "package-x-generic", NULL}, **p = n; !pb && *p; p++)
        if (**p != '/' && **p) pb = gtk_icon_theme_load_icon(th, *p, ICON_SZ, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
    if (pb) g_hash_table_insert(a->icons, g_strdup(k), pb);
    return pb;
}

static const char *entry_markup(Entry *e) {   /* built once, on first draw */
    if (!e->markup) e->markup = g_markup_printf_escaped("<b>%s</b>\n<small>%s</small>", e->name, e->desc ? e->desc : "");
    return e->markup;
}

enum { K_ICON, K_TEXT, K_SRC, K_STATUS };
typedef struct { App *a; int kind; } Cell;

static void cell_func(GtkTreeViewColumn *col, GtkCellRenderer *r, GtkTreeModel *m, GtkTreeIter *it, gpointer d) {
    Cell *c = d; Entry *e; gtk_tree_model_get(m, it, 0, &e, -1);
    switch (c->kind) {
    case K_ICON: g_object_set(r, "pixbuf", icon_for(c->a, e), NULL); break;
    case K_TEXT: g_object_set(r, "markup", entry_markup(e), NULL); break;
    case K_SRC: g_object_set(r, "text", src_label(e), NULL); break;
    default: g_object_set(r, "text", is_inst(c->a, e) ? _("✔ Installed") : "", NULL);
    }
}

static gboolean match(App *a, Entry *e, int sm, const char *q) {
    if (sm && e->src != (Src)(sm - 1)) return FALSE;
    if (a->cat == CAT_INST && !is_inst(a, e)) return FALSE;
    if (a->cat >= CAT_NET && !(e->cats & (1u << a->cat))) return FALSE;
    return !*q || strcasestr(e->name, q) || strcasestr(e->id, q) || (e->desc && strcasestr(e->desc, q));
}

/* The list store holds raw Entry pointers, so it must be emptied before any Entry is freed. */
static void rebuild(App *a) {
    const char *q = gtk_entry_get_text(GTK_ENTRY(a->entry));
    int sm = gtk_combo_box_get_active(GTK_COMBO_BOX(a->combo)); guint n = 0;
    GPtrArray *lists[] = {a->all, a->cat == CAT_ALL ? a->extra : NULL};
    gtk_tree_view_set_model(GTK_TREE_VIEW(a->tree), NULL);   /* detached: much faster bulk insert */
    gtk_list_store_clear(a->store);
    for (int i = 0; i < 2; i++)
        for (guint j = 0; lists[i] && j < lists[i]->len; j++)
            if (match(a, lists[i]->pdata[j], sm, q)) { gtk_list_store_insert_with_values(a->store, NULL, -1, 0, lists[i]->pdata[j], -1); n++; }
    gtk_tree_view_set_model(GTK_TREE_VIEW(a->tree), GTK_TREE_MODEL(a->store));
    char *s = g_strdup_printf(n ? _("%u apps") : _("No apps found"), n);
    set_status(a, s); g_free(s);
    refresh_btn(a);
}

/* ---------- data loading ---------- */
static void compute_keys(GPtrArray *arr) {   /* locale-aware, accent-insensitive-ish sort keys, computed once */
    for (guint i = 0; i < arr->len; i++) {
        Entry *e = arr->pdata[i];
        if (e->skey) continue;
        if (g_utf8_validate(e->name, -1, NULL)) {
            char *f = g_utf8_casefold(e->name, -1);
            e->skey = g_utf8_collate_key(f, -1); g_free(f);
        } else e->skey = g_strdup(e->name);
    }
}

static int cmp_name(gconstpointer x, gconstpointer y) {
    return strcmp((*(Entry **)x)->skey, (*(Entry **)y)->skey);
}

static void on_catalog(GObject *o, GAsyncResult *r, gpointer d) {
    App *a = d; GTask *t = G_TASK(r);
    Src s = GPOINTER_TO_INT(g_task_get_source_tag(t));
    guint gen = ((Load *)g_task_get_task_data(t))->gen;
    GPtrArray *n = g_task_propagate_pointer(t, NULL); busy_add(a, -1);
    if (!n) return;
    if (gen != a->load_gen[s]) { g_ptr_array_unref(n); return; }   /* superseded by a newer load of the same source */
    gtk_list_store_clear(a->store);                 /* the list points into the old entries */
    g_ptr_array_set_size(a->all, 0);                /* drop references before freeing the old entries */
    g_ptr_array_unref(a->catalog[s]); a->catalog[s] = n;
    for (int i = 0; i < N_SRC; i++) {
        compute_keys(a->catalog[i]);
        for (guint j = 0; j < a->catalog[i]->len; j++) g_ptr_array_add(a->all, a->catalog[i]->pdata[j]);
    }
    g_ptr_array_sort(a->all, cmp_name);
    if (s == SRC_APT) {
        g_hash_table_remove_all(a->apt_ids);
        for (guint j = 0; j < n->len; j++) g_hash_table_add(a->apt_ids, g_strdup(((Entry *)n->pdata[j])->id));
    }
    rebuild(a);
}

/* "scope\tname" of every enabled remote; a name present in both installations is used once, user first */
static char **enabled_remotes(App *a) {
    GPtrArray *v = g_ptr_array_new();
    GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    GtkTreeModel *m = GTK_TREE_MODEL(a->rstore); GtkTreeIter it;
    for (int pass = 0; pass < 2; pass++)
        for (gboolean ok = gtk_tree_model_get_iter_first(m, &it); ok; ok = gtk_tree_model_iter_next(m, &it)) {
            char *n, *sc; gboolean off; gtk_tree_model_get(m, &it, 0, &n, 2, &off, 3, &sc, -1);
            if (!off && !strcmp(sc, pass ? "system" : "user") && !g_hash_table_contains(seen, n)) {
                g_ptr_array_add(v, g_strdup_printf("%s\t%s", sc, n)); g_hash_table_add(seen, n);
            } else g_free(n);
            g_free(sc);
        }
    g_hash_table_destroy(seen); g_ptr_array_add(v, NULL);
    return (char **)g_ptr_array_free(v, FALSE);
}

static const char *remote_flag(App *a, const char *remote) {   /* installation flag for installing from a remote */
    char **sp = enabled_remotes(a); const char *r = "--system";
    for (int i = 0; sp[i]; i++)
        if (!strcmp(strchr(sp[i], '\t') + 1, remote)) { r = g_str_has_prefix(sp[i], "user") ? "--user" : "--system"; break; }
    g_strfreev(sp); return r;
}

static gboolean appstream_stale(App *a) {   /* missing or older than APPSTREAM_MAX_AGE */
    char **sp = enabled_remotes(a); gboolean stale = FALSE; gint64 now = g_get_real_time() / G_USEC_PER_SEC;
    for (int i = 0; sp[i] && !stale; i++) {
        char *n, *d, *f = appstream_file(sp[i], &n, &d); gint64 m = mtime(f);
        stale = !m || now - m > APPSTREAM_MAX_AGE;
        g_free(f); g_free(n); g_free(d);
    }
    g_strfreev(sp); return stale;
}

static void load_catalog(App *a, Src s) {
    Load *ld = g_new0(Load, 1); ld->gen = ++a->load_gen[s];
    if (s == SRC_FLATPAK) ld->specs = enabled_remotes(a);
    GTask *t = g_task_new(NULL, NULL, on_catalog, a);
    g_task_set_source_tag(t, GINT_TO_POINTER(s));
    g_task_set_task_data(t, ld, load_free);
    busy_add(a, 1);
    g_task_run_in_thread(t, s == SRC_FLATPAK ? fp_thread : apt_thread);
    g_object_unref(t);
}

static void fill_set(GHashTable *h, const char *out, gboolean dpkg) {
    g_hash_table_remove_all(h);
    char **l = g_strsplit(out, "\n", -1);
    for (int i = 0; l[i]; i++) {
        const char *s = l[i];
        if (dpkg) {                                 /* "ii \t<package>"; "hi" is installed but held */
            if (!((s[0] == 'i' || s[0] == 'h') && s[1] == 'i') || !(s = strchr(s, '\t'))) continue;
            if (*++s) g_hash_table_insert(h, g_strdup(s), NULL);
        } else if (*s) {                            /* "<app id>\t<installation>" */
            char **f = g_strsplit(s, "\t", 2);
            if (!strchr(f[0], ' '))                 /* a header line has spaces; app IDs never do */
                g_hash_table_insert(h, g_strdup(f[0]), g_strdup(f[1] && *f[1] ? f[1] : "user"));
            g_strfreev(f);
        }
    }
    g_strfreev(l);
}

static void on_installed(App *a, gboolean ok, const char *out, gpointer u) {
    Src s = GPOINTER_TO_INT(u);
    fill_set(a->inst[s], ok ? out : "", s == SRC_APT); busy_add(a, -1);
    if (a->cat == CAT_INST) rebuild(a); else { gtk_widget_queue_draw(a->tree); refresh_btn(a); }
}

static void refresh_installed(App *a) {
    const char *fp[] = {"flatpak", "list", "--app", "--columns=application,installation", NULL};
    const char *dp[] = {"dpkg-query", "-W", "-f=${db:Status-Abbrev}\t${Package}\n", NULL};
    if (a->has_fp) run_ex(a, fp, on_installed, GINT_TO_POINTER(SRC_FLATPAK), TRUE);
    run_ex(a, dp, on_installed, GINT_TO_POINTER(SRC_APT), TRUE);
}

/* flatpak remotes: query -> load cached catalog -> refresh appstream only if stale -> reload catalog */
static void prefs_refresh(App *a);
static void show_setup(App *a);

static void on_appstream_done(App *a, gboolean ok, const char *out, gpointer u) {
    if (!ok) log_append(a, out);
    busy_add(a, -1); load_catalog(a, SRC_FLATPAK);
}

static void update_appstream(App *a) {
    const char *v[] = {"flatpak", "update", "--appstream", "--noninteractive", NULL};
    set_status(a, _("Updating Flatpak catalogs…")); run(a, v, on_appstream_done, NULL);
}

static void on_remotes(App *a, gboolean ok, const char *out, gpointer u) {
    int enabled = 0; a->flathub_on = FALSE;
    gtk_list_store_clear(a->rstore);
    char **l = g_strsplit(ok ? out : "", "\n", -1);
    for (int i = 0; l[i]; i++) {
        char **f = g_strsplit(l[i], "\t", 3);       /* name, url, options (user/system, disabled…) */
        if (g_strv_length(f) == 3 && g_ascii_strcasecmp(f[0], "Name")) {
            gboolean off = strstr(f[2], "disabled") != NULL, user = strstr(f[2], "user") != NULL;
            gtk_list_store_insert_with_values(a->rstore, NULL, -1, 0, f[0], 1, f[1], 2, off,
                                              3, user ? "user" : "system", 4, user ? _("User") : _("System"), -1);
            if (!off) { enabled++; if (!strcmp(f[0], "flathub")) a->flathub_on = TRUE; }
        }
        g_strfreev(f);
    }
    g_strfreev(l); busy_add(a, -1);
    load_catalog(a, SRC_FLATPAK);
    if (enabled && appstream_stale(a)) update_appstream(a);
    prefs_refresh(a);
    if (!a->setup_done) show_setup(a);              /* first run: ask once the real state is known */
}

static void query_remotes(App *a) {
    const char *v[] = {"flatpak", "remotes", "--columns=name,url,options", NULL};
    run_ex(a, v, on_remotes, NULL, TRUE);
}

static void on_remote_changed(App *a, gboolean ok, const char *out, gpointer u) {
    const char *nl = strchr(out, '\n');
    char *m = ok ? g_strdup(_("Done.")) : nl ? g_strndup(out, nl - out) : g_strdup(out);
    log_append(a, out); busy_add(a, -1);
    if (a->pf_msg) gtk_label_set_text(GTK_LABEL(a->pf_msg), m); else if (!ok) set_status(a, m);
    g_free(m);
    if (ok) query_remotes(a);
}

static void add_remote(App *a, const char *name, const char *url) {
    const char *v[] = {"flatpak", "remote-add", "--user", "--if-not-exists", name, url, NULL};
    run(a, v, on_remote_changed, NULL);
}

/* ---------- install / remove / update ---------- */
static void on_action_done(App *a, gboolean ok, const char *out, gpointer u) {
    log_append(a, out); log_append(a, ok ? _("== Done ==") : _("== Failed =="));
    if (!ok) gtk_expander_set_expanded(GTK_EXPANDER(a->exp), TRUE);
    a->acting = FALSE; busy_add(a, -1); refresh_installed(a); refresh_btn(a);
    a->fail = !ok && !a->show_log;
    if (a->fail) set_status(a, _("Operation failed. Enable the log in Settings for details."));
}

static void on_aptupd_done(App *a, gboolean ok, const char *out, gpointer u) {
    on_action_done(a, ok, out, u); if (ok) load_catalog(a, SRC_APT);
}

static void start_action(App *a, const char *msg, const char *const *v, DoneFn fn, gpointer ud, gboolean c_locale) {
    a->acting = TRUE; a->fail = FALSE; refresh_btn(a); set_status(a, msg);
    run_ex(a, v, fn, ud, c_locale);
}

static gboolean confirm_remove(App *a, const char *id, GPtrArray *extra) {
    GString *g = g_string_new(NULL);
    for (guint i = 0; i < extra->len && i < 30; i++) g_string_append_printf(g, "• %s\n", (char *)extra->pdata[i]);
    if (extra->len > 30) g_string_append_printf(g, "… (+%u)\n", extra->len - 30);
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(a->win), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
                                          _("Removing %s will also remove these packages:"), id);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d), "%s", g->str);
    gtk_dialog_add_buttons(GTK_DIALOG(d), _("Cancel"), GTK_RESPONSE_CANCEL, _("Remove"), GTK_RESPONSE_OK, NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_CANCEL);
    gboolean ok = gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_OK;
    gtk_widget_destroy(d); g_string_free(g, TRUE);
    return ok;
}

/* result of "apt-get -s remove <id>": lines "Remv <package> [version]" list everything that would go */
static void on_remove_sim(App *a, gboolean ok, const char *out, gpointer u) {
    char *id = u;                                   /* owned here */
    if (!ok) { on_action_done(a, FALSE, out, NULL); g_free(id); return; }
    busy_add(a, -1);
    GPtrArray *extra = g_ptr_array_new_with_free_func(g_free);
    char **l = g_strsplit(out, "\n", -1);
    for (int i = 0; l[i]; i++) {
        if (!g_str_has_prefix(l[i], "Remv ")) continue;
        const char *n = l[i] + 5; gsize len = strcspn(n, " :");     /* drop version and ":arch" */
        char *nm = g_strndup(n, len);
        if (strcmp(nm, id)) g_ptr_array_add(extra, nm); else g_free(nm);
    }
    g_strfreev(l);
    if (!extra->len || confirm_remove(a, id, extra)) {
        const char *ar[] = {"pkexec", "env", "DEBIAN_FRONTEND=noninteractive", "apt-get", "remove", "-y", id, NULL};
        char *s = g_strdup_printf(_("Removing %s…"), id);
        set_status(a, s); g_free(s);
        run(a, ar, on_action_done, NULL);
    } else { a->acting = FALSE; refresh_btn(a); }
    g_ptr_array_unref(extra); g_free(id);
}

static void on_btn(GtkButton *b, App *a) {
    Entry *e = selected(a); if (!e || a->acting) return;
    gboolean in = is_inst(a, e);
    char *s;
    if (e->src == SRC_APT) {
        if (in) {                                   /* simulate first: removing can drag other packages along */
            const char *sim[] = {"apt-get", "-s", "remove", e->id, NULL};
            start_action(a, _("Checking what the removal would affect…"), sim, on_remove_sim, g_strdup(e->id), TRUE);
        } else {
            const char *ai[] = {"pkexec", "env", "DEBIAN_FRONTEND=noninteractive", "apt-get", "install", "-y", e->id, NULL};
            s = g_strdup_printf(_("Installing %s…"), e->name);
            start_action(a, s, ai, on_action_done, NULL, FALSE); g_free(s);
        }
        return;
    }
    const char *iscope = g_hash_table_lookup(a->inst[SRC_FLATPAK], e->id);
    const char *rflag = remote_flag(a, e->remote);
    const char *iflag = g_strcmp0(iscope, "user") ? "--system" : "--user";
    const char *fi[] = {"flatpak", "install", rflag, "--noninteractive", "-y", e->remote, e->id, NULL};
    const char *fu[] = {"flatpak", "uninstall", iflag, "--noninteractive", "-y", e->id, NULL};
    s = g_strdup_printf(in ? _("Removing %s…") : _("Installing %s…"), e->name);
    start_action(a, s, in ? fu : fi, on_action_done, NULL, FALSE); g_free(s);
}

/* ---------- menu actions ---------- */
static void act_reload(GSimpleAction *s, GVariant *p, gpointer d) {
    App *a = d;
    if (a->has_fp) update_appstream(a);             /* reloads the Flatpak catalog when it finishes */
    load_catalog(a, SRC_APT);
}

static void act_apt_update(GSimpleAction *s, GVariant *p, gpointer d) {
    App *a = d; if (a->acting) return;
    const char *v[] = {"pkexec", "env", "DEBIAN_FRONTEND=noninteractive", "apt-get", "update", NULL};
    start_action(a, _("Updating package lists (apt update)…"), v, on_aptupd_done, NULL, FALSE);
}

static void act_search(GSimpleAction *s, GVariant *p, gpointer d) { gtk_widget_grab_focus(((App *)d)->entry); }

static void act_about(GSimpleAction *s, GVariant *p, gpointer d) {
    gtk_show_about_dialog(GTK_WINDOW(((App *)d)->win), "program-name", _("Software Store"), "version", APP_VERSION,
        "comments", _("Install apps from Flathub and the Debian repositories."),
        "logo-icon-name", "system-software-install", "license-type", GTK_LICENSE_GPL_3_0, NULL);
}

/* ---------- search and navigation ---------- */
static void on_extra(App *a, gboolean ok, const char *out, gpointer u) {
    busy_add(a, -1);
    if (GPOINTER_TO_UINT(u) != a->gen) return;      /* superseded by a newer search */
    char **l = g_strsplit(out, "\n", -1); int n = 0;
    for (int i = 0; l[i] && n < 100; i++) {
        char *sep = strstr(l[i], " - ");
        if (!sep || (*sep = 0, g_hash_table_contains(a->apt_ids, l[i]))) continue;
        Entry *e = g_new0(Entry, 1);
        e->name = g_strdup(l[i]); e->id = g_strdup(l[i]); e->desc = g_strdup(sep + 3);
        e->icon = g_strdup(l[i]); e->src = SRC_APT;
        e->cats = 1u << CAT_UTIL;
        g_ptr_array_add(a->extra, e); n++;
    }
    g_strfreev(l);
    if (n) rebuild(a);
}

/* apt-cache search takes regular expressions (one per word, all must match) and treats a leading "-" as an
   option, so every word is escaped first. */
static void search_extra(App *a, const char *q) {
    GPtrArray *v = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(v, g_strdup("apt-cache")); g_ptr_array_add(v, g_strdup("search"));
    char **w = g_strsplit_set(q, " \t", -1); guint words = 0;
    for (int i = 0; w[i]; i++) {
        if (!*w[i]) continue;
        char *esc = g_regex_escape_string(w[i], -1);
        g_ptr_array_add(v, *esc == '-' ? g_strdup_printf("[-]%s", esc + 1) : g_strdup(esc));
        g_free(esc); words++;
    }
    g_strfreev(w);
    if (words) {
        g_ptr_array_add(v, NULL);
        run(a, (const char *const *)v->pdata, on_extra, GUINT_TO_POINTER(a->gen));   /* argv is copied at spawn */
    }
    g_ptr_array_unref(v);
}

static void on_search(GtkWidget *w, App *a) {
    const char *q = gtk_entry_get_text(GTK_ENTRY(a->entry));
    a->gen++;
    gtk_list_store_clear(a->store);                 /* extra entries are about to be freed */
    g_ptr_array_set_size(a->extra, 0);
    rebuild(a);
    if (g_utf8_strlen(q, -1) >= 3 && gtk_combo_box_get_active(GTK_COMBO_BOX(a->combo)) != 1 + SRC_FLATPAK)
        search_extra(a, q);                         /* also non-graphical packages */
}

static void on_sel(GtkTreeSelection *s, App *a) { a->fail = FALSE; refresh_btn(a); }

static void on_cat(GtkTreeSelection *s, App *a) {
    GtkTreeModel *m; GtkTreeIter it;
    if (!gtk_tree_selection_get_selected(s, &m, &it)) return;
    GtkTreePath *p = gtk_tree_model_get_path(m, &it);
    a->cat = gtk_tree_path_get_indices(p)[0]; gtk_tree_path_free(p);
    rebuild(a);
}

/* closing mid-operation would close the pipes of apt/dpkg, which could interrupt them half way */
static gboolean on_delete(GtkWidget *w, GdkEvent *ev, App *a) {
    if (!a->acting) return FALSE;
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(w), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "%s",
        _("An operation is still running. Wait for it to finish before closing."));
    gtk_dialog_run(GTK_DIALOG(d)); gtk_widget_destroy(d);
    return TRUE;
}

/* ---------- UI ---------- */
static char *themed_icon(const char *sym) {   /* symbolic icon, or the regular one if the theme lacks it */
    char *s = g_strdup(sym), *p = strstr(s, "-symbolic");
    if (gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), sym) || !p) return s;
    *p = 0; return s;
}

static void add_col(App *a, GtkCellRenderer *r, int kind, gboolean expand) {
    Cell *c = g_new(Cell, 1); c->a = a; c->kind = kind;
    GtkTreeViewColumn *col = gtk_tree_view_column_new();
    gtk_tree_view_column_pack_start(col, r, TRUE);
    gtk_tree_view_column_set_cell_data_func(col, r, cell_func, c, NULL);
    gtk_tree_view_column_set_expand(col, expand);
    gtk_tree_view_append_column(GTK_TREE_VIEW(a->tree), col);
}

static GtkWidget *scrolled(GtkWidget *child, GtkPolicyType h) {
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), h, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(sw), child);
    return sw;
}

static GtkWidget *build_sidebar(void) {
    GtkListStore *st = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    for (int i = 0; i < N_CATS; i++) {
        char *ic = themed_icon(CAT_ICON[i]);
        gtk_list_store_insert_with_values(st, NULL, -1, 0, ic, 1, _(CAT_NAME[i]), -1); g_free(ic);
    }
    GtkWidget *t = gtk_tree_view_new_with_model(GTK_TREE_MODEL(st));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(t), FALSE);
    GtkTreeViewColumn *c = gtk_tree_view_column_new();
    GtkCellRenderer *ri = gtk_cell_renderer_pixbuf_new(), *rt = gtk_cell_renderer_text_new();
    g_object_set(ri, "xpad", 6, "ypad", 6, NULL);
    gtk_tree_view_column_pack_start(c, ri, FALSE); gtk_tree_view_column_pack_start(c, rt, TRUE);
    gtk_tree_view_column_add_attribute(c, ri, "icon-name", 0);
    gtk_tree_view_column_add_attribute(c, rt, "text", 1);
    gtk_tree_view_append_column(GTK_TREE_VIEW(t), c);
    GtkWidget *sw = scrolled(t, GTK_POLICY_NEVER);
    gtk_widget_set_size_request(sw, 190, -1);
    g_object_set_data(G_OBJECT(sw), "tree", t);
    return sw;
}

static GtkWidget *build_menu_button(void) {
    GMenu *m = g_menu_new(), *s1 = g_menu_new(), *s2 = g_menu_new();
    g_menu_append(s1, _("Reload catalogs"), "win.reload");
    g_menu_append(s1, _("Update package lists"), "win.apt-update");
    g_menu_append(s2, _("Settings"), "win.settings");
    g_menu_append(s2, _("About"), "win.about");
    g_menu_append_section(m, NULL, G_MENU_MODEL(s1)); g_menu_append_section(m, NULL, G_MENU_MODEL(s2));
    GtkWidget *b = gtk_menu_button_new();
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(b), G_MENU_MODEL(m));
    char *ic = themed_icon("view-more-symbolic");
    gtk_button_set_image(GTK_BUTTON(b), gtk_image_new_from_icon_name(ic, GTK_ICON_SIZE_BUTTON)); g_free(ic);
    return b;
}

/* ---------- configuration, first-run setup and settings window ---------- */
static char *config_path(void) { return g_build_filename(g_get_user_config_dir(), "xfce-store", "config.ini", NULL); }

static char *config_language(void) {   /* read before the UI exists; NULL means automatic */
    GKeyFile *kf = g_key_file_new(); char *p = config_path();
    g_key_file_load_from_file(kf, p, G_KEY_FILE_NONE, NULL);
    char *l = g_key_file_get_string(kf, "general", "language", NULL);
    g_key_file_free(kf); g_free(p); return l;
}

static void config_load(App *a) {
    GKeyFile *kf = g_key_file_new(); char *p = config_path();
    g_key_file_load_from_file(kf, p, G_KEY_FILE_NONE, NULL);
    a->setup_done = g_key_file_get_boolean(kf, "general", "setup_done", NULL);
    a->show_log = g_key_file_get_boolean(kf, "general", "show_log", NULL);
    g_free(a->lang); a->lang = g_key_file_get_string(kf, "general", "language", NULL);
    if (!a->lang) a->lang = g_strdup("auto");
    g_key_file_free(kf); g_free(p);
}

static void config_save(App *a) {
    GKeyFile *kf = g_key_file_new(); char *p = config_path(), *dir = g_path_get_dirname(p);
    g_key_file_set_boolean(kf, "general", "setup_done", a->setup_done);
    g_key_file_set_boolean(kf, "general", "show_log", a->show_log);
    g_key_file_set_string(kf, "general", "language", a->lang);
    g_mkdir_with_parents(dir, 0755); g_key_file_save_to_file(kf, p, NULL);
    g_free(dir); g_free(p); g_key_file_free(kf);
}

static void start(App *a) {
    refresh_installed(a);
    load_catalog(a, SRC_APT);
    if (a->has_fp) query_remotes(a);
    else log_append(a, _("flatpak not found (sudo apt install flatpak): only Debian will be available."));
}

/* Only "Continue" applies the choices. Closing the window with the "X" changes nothing and the
   question is asked again the next time the store starts. */
static void on_setup_response(GtkDialog *d, int resp, App *a) {
    GtkToggleButton *flat = g_object_get_data(G_OBJECT(d), "flat"), *log = g_object_get_data(G_OBJECT(d), "log");
    gboolean ok = resp == GTK_RESPONSE_OK;
    gboolean want_fp = ok && a->has_fp && !a->flathub_on && gtk_toggle_button_get_active(flat);
    if (ok) { a->show_log = gtk_toggle_button_get_active(log); a->setup_done = TRUE; }
    gtk_widget_destroy(GTK_WIDGET(d));
    if (!ok) return;
    gtk_widget_set_visible(a->exp, a->show_log);
    config_save(a);
    if (want_fp) add_remote(a, "flathub", FLATHUB_URL);
}

static void show_setup(App *a) {
    if (a->setup_asked) return;                     /* at most once per run, even if remotes are queried again */
    a->setup_asked = TRUE;
    GtkWidget *d = gtk_dialog_new_with_buttons(_("Initial setup"), GTK_WINDOW(a->win),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, _("Continue"), GTK_RESPONSE_OK, NULL);
    GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(d));
    gtk_container_set_border_width(GTK_CONTAINER(d), 12); gtk_box_set_spacing(GTK_BOX(box), 10);
    GtkWidget *intro = gtk_label_new(_("Welcome! Choose how the store should start. You can change this later in Settings."));
    gtk_label_set_line_wrap(GTK_LABEL(intro), TRUE); gtk_label_set_max_width_chars(GTK_LABEL(intro), 44);
    gtk_label_set_xalign(GTK_LABEL(intro), 0);
    gtk_box_pack_start(GTK_BOX(box), intro, FALSE, FALSE, 0);
    GtkWidget *flat = gtk_check_button_new_with_label(_("Enable Flathub (recommended)"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(flat), a->has_fp);
    gtk_widget_set_sensitive(flat, a->has_fp && !a->flathub_on);
    gtk_box_pack_start(GTK_BOX(box), flat, FALSE, FALSE, 0);
    if (!a->has_fp || a->flathub_on)
        gtk_box_pack_start(GTK_BOX(box), gtk_label_new(a->has_fp ? _("Flathub is already enabled.") : _("flatpak is not installed.")), FALSE, FALSE, 0);
    GtkWidget *log = gtk_check_button_new_with_label(_("Show log area on the main screen"));
    gtk_box_pack_start(GTK_BOX(box), log, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(d), "flat", flat); g_object_set_data(G_OBJECT(d), "log", log);
    g_signal_connect(d, "response", G_CALLBACK(on_setup_response), a);
    gtk_widget_show_all(d);
}

static void prefs_refresh(App *a) {
    if (!a->prefs) return;
    gtk_label_set_text(GTK_LABEL(a->pf_status), a->flathub_on ? _("Flathub is enabled.") : _("Flathub is not enabled."));
    gtk_widget_set_visible(a->pf_btn, !a->flathub_on);
}

static void on_enable_flathub(GtkButton *b, App *a) { add_remote(a, "flathub", FLATHUB_URL); }

static void on_add_remote(GtkButton *b, App *a) {
    const char *n = gtk_entry_get_text(GTK_ENTRY(a->pf_name)), *u = gtk_entry_get_text(GTK_ENTRY(a->pf_url));
    if (*n && *u) add_remote(a, n, u);
}

static void on_del_remote(GtkButton *b, App *a) {
    GtkTreeModel *m; GtkTreeIter it; char *n, *sc;
    if (!gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(a->pf_list)), &m, &it)) return;
    gtk_tree_model_get(m, &it, 0, &n, 3, &sc, -1);
    const char *v[] = {"flatpak", "remote-delete", !strcmp(sc, "user") ? "--user" : "--system", n, NULL};
    run(a, v, on_remote_changed, NULL); g_free(n); g_free(sc);
}

static void on_lang_changed(GtkComboBox *c, App *a) {
    g_free(a->lang); a->lang = g_strdup(gtk_combo_box_get_active_id(c));
    gtk_label_set_text(GTK_LABEL(g_object_get_data(G_OBJECT(c), "note")), _("Restart the store to apply the language."));
    config_save(a);
}

static void on_log_toggle(GtkToggleButton *t, App *a) {
    a->show_log = gtk_toggle_button_get_active(t);
    gtk_widget_set_visible(a->exp, a->show_log);
    config_save(a);
}

static void on_prefs_destroy(GtkWidget *w, App *a) {
    a->prefs = a->pf_status = a->pf_btn = a->pf_msg = a->pf_name = a->pf_url = a->pf_list = NULL;
}

static GtkWidget *heading(const char *t) {
    GtkWidget *l = gtk_label_new(NULL); char *m = g_markup_printf_escaped("<b>%s</b>", t);
    gtk_label_set_markup(GTK_LABEL(l), m); gtk_label_set_xalign(GTK_LABEL(l), 0); g_free(m);
    return l;
}

static void act_settings(GSimpleAction *s, GVariant *p, gpointer d) {
    App *a = d;
    if (a->prefs) { gtk_window_present(GTK_WINDOW(a->prefs)); return; }
    GtkWidget *dlg = a->prefs = gtk_dialog_new_with_buttons(_("Settings"), GTK_WINDOW(a->win),
        GTK_DIALOG_DESTROY_WITH_PARENT, _("Close"), GTK_RESPONSE_CLOSE, NULL);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 520, 480);
    gtk_container_set_border_width(GTK_CONTAINER(dlg), 12);
    GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    gtk_box_set_spacing(GTK_BOX(box), 8);

    GtkWidget *fp = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);        /* Flatpak section */
    gtk_widget_set_sensitive(fp, a->has_fp);
    if (!a->has_fp) gtk_box_pack_start(GTK_BOX(fp), gtk_label_new(_("flatpak is not installed.")), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(fp), heading("Flathub"), FALSE, FALSE, 0);
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    a->pf_status = gtk_label_new(NULL); gtk_label_set_xalign(GTK_LABEL(a->pf_status), 0);
    a->pf_btn = gtk_button_new_with_label(_("Enable Flathub")); gtk_widget_set_no_show_all(a->pf_btn, TRUE);
    gtk_box_pack_start(GTK_BOX(row), a->pf_status, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), a->pf_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(fp), row, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(fp), heading(_("Flatpak repositories")), FALSE, FALSE, 0);
    a->pf_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(a->rstore));
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(a->pf_list), -1, _("Name"), gtk_cell_renderer_text_new(), "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(a->pf_list), -1, "URL", gtk_cell_renderer_text_new(), "text", 1, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(a->pf_list), -1, _("Type"), gtk_cell_renderer_text_new(), "text", 4, NULL);
    gtk_box_pack_start(GTK_BOX(fp), scrolled(a->pf_list, GTK_POLICY_AUTOMATIC), TRUE, TRUE, 0);
    GtkWidget *del = gtk_button_new_with_label(_("Remove selected"));
    gtk_widget_set_halign(del, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(fp), del, FALSE, FALSE, 0);
    GtkWidget *add = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6), *addb = gtk_button_new_with_label(_("Add"));
    a->pf_name = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(a->pf_name), _("Repository name"));
    a->pf_url = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(a->pf_url), _("Repository URL (.flatpakrepo)"));
    gtk_box_pack_start(GTK_BOX(add), a->pf_name, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(add), a->pf_url, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(add), addb, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(fp), add, FALSE, FALSE, 0);
    a->pf_msg = gtk_label_new(NULL); gtk_label_set_xalign(GTK_LABEL(a->pf_msg), 0);
    gtk_label_set_line_wrap(GTK_LABEL(a->pf_msg), TRUE);
    gtk_box_pack_start(GTK_BOX(fp), a->pf_msg, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), fp, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(box), heading("Interface"), FALSE, FALSE, 0);
    GtkWidget *lg = gtk_check_button_new_with_label(_("Show log area on the main screen"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lg), a->show_log);
    gtk_box_pack_start(GTK_BOX(box), lg, FALSE, FALSE, 0);
    GtkWidget *lrow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8), *lc = gtk_combo_box_text_new(), *note = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(note), 0);
    gtk_box_pack_start(GTK_BOX(lrow), gtk_label_new(_("Language")), FALSE, FALSE, 0);
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(lc), "auto", _("Automatic (system)"));
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(lc), "en", "English");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(lc), "pt", "Português (Brasil)");
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(lc), a->lang);
    g_object_set_data(G_OBJECT(lc), "note", note);
    g_signal_connect(lc, "changed", G_CALLBACK(on_lang_changed), a);
    gtk_box_pack_start(GTK_BOX(lrow), lc, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), lrow, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), note, FALSE, FALSE, 0);

    g_signal_connect(a->pf_btn, "clicked", G_CALLBACK(on_enable_flathub), a);
    g_signal_connect(addb, "clicked", G_CALLBACK(on_add_remote), a);
    g_signal_connect(del, "clicked", G_CALLBACK(on_del_remote), a);
    g_signal_connect(lg, "toggled", G_CALLBACK(on_log_toggle), a);
    g_signal_connect(dlg, "destroy", G_CALLBACK(on_prefs_destroy), a);
    g_signal_connect_swapped(dlg, "response", G_CALLBACK(gtk_widget_destroy), dlg);
    gtk_widget_show_all(dlg);
    prefs_refresh(a);
}

static void activate(GtkApplication *gapp, gpointer d) {
    static const GActionEntry acts[] = {{"reload", act_reload}, {"apt-update", act_apt_update},
                                        {"search", act_search}, {"about", act_about}, {"settings", act_settings}};
    GtkWindow *existing = gtk_application_get_active_window(gapp);   /* a second launch only raises the window */
    if (existing) { gtk_window_present(existing); return; }

    App *a = g_new0(App, 1);
    a->has_fp = have("flatpak");
    a->all = g_ptr_array_new();
    a->rstore = gtk_list_store_new(5, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING);
    a->extra = g_ptr_array_new_with_free_func(entry_free);
    a->apt_ids = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    a->icons = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
    for (int i = 0; i < N_SRC; i++) {
        a->catalog[i] = g_ptr_array_new_with_free_func(entry_free);
        a->inst[i] = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    }

    a->win = gtk_application_window_new(gapp);
    gtk_window_set_title(GTK_WINDOW(a->win), _("Software Store"));
    gtk_window_set_default_size(GTK_WINDOW(a->win), 1000, 650);
    gtk_window_set_icon_name(GTK_WINDOW(a->win), "system-software-install");
    g_action_map_add_action_entries(G_ACTION_MAP(a->win), acts, G_N_ELEMENTS(acts), a);
    gtk_application_set_accels_for_action(gapp, "win.reload", (const char *[]){"F5", NULL});
    gtk_application_set_accels_for_action(gapp, "win.search", (const char *[]){"<Control>f", NULL});
    gtk_application_set_accels_for_action(gapp, "win.settings", (const char *[]){"<Control>comma", NULL});

    GtkWidget *vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vb), 6);
    gtk_container_add(GTK_CONTAINER(a->win), vb);

    GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    a->entry = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(a->entry), _("Search apps…"));
    a->combo = gtk_combo_box_text_new();
    for (const char *s[] = {_("All sources"), SRC_NAME[0], SRC_NAME[1]}, **p = s; p < s + 3; p++)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(a->combo), *p);
    gtk_combo_box_set_active(GTK_COMBO_BOX(a->combo), 0);
    gtk_box_pack_start(GTK_BOX(top), a->entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(top), a->combo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(top), build_menu_button(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vb), top, FALSE, FALSE, 0);

    GtkWidget *side = build_sidebar();
    a->store = gtk_list_store_new(1, G_TYPE_POINTER);
    a->tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(a->store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(a->tree), FALSE);
    GtkCellRenderer *ri = gtk_cell_renderer_pixbuf_new(), *rt = gtk_cell_renderer_text_new(),
                    *rs = gtk_cell_renderer_text_new(), *rst = gtk_cell_renderer_text_new();
    g_object_set(ri, "xpad", 8, "ypad", 5, NULL);
    g_object_set(rt, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    g_object_set(rs, "sensitive", FALSE, "xpad", 8, NULL);      /* dimmed with the theme */
    g_object_set(rst, "xpad", 8, NULL);
    add_col(a, ri, K_ICON, FALSE); add_col(a, rt, K_TEXT, TRUE);
    add_col(a, rs, K_SRC, FALSE); add_col(a, rst, K_STATUS, FALSE);

    GtkWidget *pn = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_pack1(GTK_PANED(pn), side, FALSE, FALSE);
    gtk_paned_pack2(GTK_PANED(pn), scrolled(a->tree, GTK_POLICY_AUTOMATIC), TRUE, FALSE);
    gtk_box_pack_start(GTK_BOX(vb), pn, TRUE, TRUE, 0);

    GtkWidget *bot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    a->info = gtk_label_new(_("Loading catalogs…"));
    gtk_label_set_xalign(GTK_LABEL(a->info), 0);
    gtk_label_set_ellipsize(GTK_LABEL(a->info), PANGO_ELLIPSIZE_END);
    a->prog = gtk_progress_bar_new();
    gtk_widget_set_size_request(a->prog, 140, -1);
    gtk_widget_set_valign(a->prog, GTK_ALIGN_CENTER);
    gtk_widget_set_no_show_all(a->prog, TRUE);
    a->btn = gtk_button_new_with_label(_("Install"));
    gtk_widget_set_size_request(a->btn, 110, -1);
    gtk_widget_set_sensitive(a->btn, FALSE);
    gtk_box_pack_start(GTK_BOX(bot), a->info, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(bot), a->prog, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bot), a->btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vb), bot, FALSE, FALSE, 0);

    a->exp = gtk_expander_new(_("Details (log)"));
    a->log = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(a->log), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(a->log), TRUE);
    GtkWidget *lsw = scrolled(a->log, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(lsw, -1, 110);
    gtk_container_add(GTK_CONTAINER(a->exp), lsw);
    gtk_box_pack_start(GTK_BOX(vb), a->exp, FALSE, FALSE, 0);

    GtkWidget *cats = g_object_get_data(G_OBJECT(side), "tree");
    g_signal_connect(a->entry, "search-changed", G_CALLBACK(on_search), a);
    g_signal_connect(a->combo, "changed", G_CALLBACK(on_search), a);
    g_signal_connect(a->btn, "clicked", G_CALLBACK(on_btn), a);
    g_signal_connect(a->win, "delete-event", G_CALLBACK(on_delete), a);
    g_signal_connect(gtk_tree_view_get_selection(GTK_TREE_VIEW(a->tree)), "changed", G_CALLBACK(on_sel), a);
    g_signal_connect(gtk_tree_view_get_selection(GTK_TREE_VIEW(cats)), "changed", G_CALLBACK(on_cat), a);
    gtk_widget_show_all(a->win);
    gtk_widget_hide(a->prog);
    gtk_widget_grab_focus(a->entry);
    GtkTreePath *p0 = gtk_tree_path_new_first();
    gtk_tree_selection_select_path(gtk_tree_view_get_selection(GTK_TREE_VIEW(cats)), p0); gtk_tree_path_free(p0);

    config_load(a);
    gtk_widget_set_visible(a->exp, a->show_log);
    start(a);
    if (!a->setup_done && !a->has_fp) show_setup(a);   /* with flatpak, on_remotes opens it */
}

int main(int argc, char **argv) {
    char *lang = config_language();
    i18n_init(lang); g_free(lang);
    g_set_application_name(_("Software Store"));
#if GLIB_CHECK_VERSION(2, 74, 0)
    GtkApplication *app = gtk_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
#else
    GtkApplication *app = gtk_application_new(APP_ID, G_APPLICATION_FLAGS_NONE);
#endif
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int r = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return r;
}
