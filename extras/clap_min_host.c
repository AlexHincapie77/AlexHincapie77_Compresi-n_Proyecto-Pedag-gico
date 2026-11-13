// ============================================================================
//  clap_min_host.c — Mini-host CLAP para validar el plugin
//  1. dlopen + init + factory + create
//  2. enumera parámetros
//  3. cambia "Num Bands" y "Preset" vía EVENTOS (como lo hace un host real)
//  4. process con audio — verifica salida, NaN, clipping
//  5. state save/load round-trip
// ============================================================================
#include <clap/clap.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

static int g_failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) printf("  [OK] %s\n", msg); \
    else { printf("  [FALLA] %s\n", msg); g_failures++; } \
} while (0)

// ─── Host callbacks ───────────────────────────────────────────────────────
static const void *host_get_extension(const clap_host_t *h, const char *id)
{
    (void)h; (void)id;
    return NULL;
}
static void host_request_restart(const clap_host_t *h) { (void)h; }
static void host_request_process(const clap_host_t *h) { (void)h; }
static void host_request_callback(const clap_host_t *h) { (void)h; }

static const clap_host_t g_host = {
    .clap_version = CLAP_VERSION_INIT,
    .host_data = NULL,
    .name = "MiniHost-Test",
    .vendor = "Test",
    .url = "https://test.example",
    .version = "1.0",
    .get_extension = host_get_extension,
    .request_restart = host_request_restart,
    .request_process = host_request_process,
    .request_callback = host_request_callback,
};

// ─── Lista de eventos de entrada ──────────────────────────────────────────
#define MAX_EVENTS 8
typedef struct {
    clap_event_header_t *events[MAX_EVENTS];
    uint32_t count;
} event_list_t;

static uint32_t ev_size(const clap_input_events_t *l)
{ return ((event_list_t *)l->ctx)->count; }
static const clap_event_header_t *ev_get(const clap_input_events_t *l, uint32_t i)
{ return ((event_list_t *)l->ctx)->events[i]; }

// ─── Output events (descartados) ──────────────────────────────────────────
static bool out_events_try_push(const clap_output_events_t *l, const clap_event_header_t *e)
{ (void)l; (void)e; return true; }
static const clap_output_events_t g_out_events = {
    .ctx = NULL, .try_push = out_events_try_push,
};

// ─── Stream de estado en memoria ──────────────────────────────────────────
typedef struct {
    uint8_t *data;
    uint64_t size;
    uint64_t capacity;
    uint64_t pos;
} mem_stream_t;

static int64_t mem_write(const clap_ostream_t *s, const void *buffer, uint64_t size)
{
    mem_stream_t *m = (mem_stream_t *)s->ctx;
    while (m->size + size > m->capacity)
    {
        m->capacity = m->capacity ? m->capacity * 2 : 4096;
        m->data = realloc(m->data, m->capacity);
    }
    memcpy(m->data + m->size, buffer, size);
    m->size += size;
    return (int64_t)size;
}
static int64_t mem_read(const clap_istream_t *s, void *buffer, uint64_t size)
{
    mem_stream_t *m = (mem_stream_t *)s->ctx;
    uint64_t avail = m->size - m->pos;
    if (avail == 0) return 0;   // 0 = EOF según el contrato CLAP (-1 = error)
    uint64_t n = size < avail ? size : avail;
    memcpy(buffer, m->data + m->pos, n);
    m->pos += n;
    return (int64_t)n;
}

static void make_param_event(clap_event_param_value_t *ev, clap_id id, double value)
{
    memset(ev, 0, sizeof(*ev));
    ev->header.size = sizeof(clap_event_param_value_t);
    ev->header.time = 0;
    ev->header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev->header.type = CLAP_EVENT_PARAM_VALUE;
    ev->header.flags = 0;
    ev->param_id = id;
    ev->note_id = -1;
    ev->port_index = -1;
    ev->channel = -1;
    ev->key = -1;
    ev->value = value;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "Uso: %s <plugin.clap>\n", argv[0]); return 2; }
    const char *path = argv[1];

    printf("=== Mini-host CLAP: %s ===\n", path);

    // 1) dlopen
    void *lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    CHECK(lib != NULL, "dlopen del .clap");
    if (!lib) { fprintf(stderr, "dlerror: %s\n", dlerror()); return 1; }

    // 2) clap_entry + init
    const clap_plugin_entry_t *entry = (const clap_plugin_entry_t *)dlsym(lib, "clap_entry");
    CHECK(entry != NULL, "símbolo clap_entry exportado");
    if (!entry) return 1;
    CHECK(entry->init("1.0.0"), "entry->init()");

    // 3) factory
    const clap_plugin_factory_t *factory =
        (const clap_plugin_factory_t *)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    CHECK(factory != NULL, "get_factory()");

    uint32_t count = factory->get_plugin_count(factory);
    CHECK(count == 1, "get_plugin_count() == 1");

    const clap_plugin_descriptor_t *desc = factory->get_plugin_descriptor(factory, 0);
    CHECK(desc != NULL, "descriptor no nulo");
    if (desc)
    {
        printf("  id:      %s\n  name:    %s\n  version: %s\n  vendor:  %s\n",
               desc->id, desc->name, desc->version, desc->vendor);
        CHECK(strcmp(desc->id, "com.pedagogicalaudio.compressor") == 0,
              "CLAP_ID correcto");
    }

    // 4) create + init
    const clap_plugin_t *plugin = factory->create_plugin(factory, &g_host, desc->id);
    CHECK(plugin != NULL, "create_plugin()");
    if (!plugin) return 1;
    CHECK(plugin->init(plugin), "plugin->init()");

    // 5) parámetros
    const clap_plugin_params_t *params =
        (const clap_plugin_params_t *)plugin->get_extension(plugin, CLAP_EXT_PARAMS);
    CHECK(params != NULL, "extensión params");
    clap_id idNumBands = CLAP_INVALID_ID, idPreset = CLAP_INVALID_ID, idThr0 = CLAP_INVALID_ID;
    if (params)
    {
        uint32_t nparams = params->count(plugin);
        printf("  parámetros: %u\n", nparams);
        CHECK(nparams > 50, "más de 50 parámetros");

        clap_param_info_t info;
        for (uint32_t i = 0; i < nparams; ++i)
        {
            if (params->get_info(plugin, i, &info))
            {
                if (strstr(info.name, "Num Bands")) idNumBands = info.id;
                if (strcmp(info.name, "Preset") == 0) idPreset = info.id;
                if (strstr(info.name, "B0 Threshold")) idThr0 = info.id;
            }
        }
        CHECK(idNumBands != CLAP_INVALID_ID, "parámetro 'Num Bands' visible");
        CHECK(idPreset != CLAP_INVALID_ID, "parámetro 'Preset' visible");
        CHECK(idThr0 != CLAP_INVALID_ID, "parámetro 'B0 Threshold' visible");
    }

    // 6) activate + start_processing
    const uint32_t sr = 44100, block = 256;
    CHECK(plugin->activate(plugin, sr, 256, 512), "activate(44100)");
    CHECK(plugin->start_processing(plugin), "start_processing()");

    // 7) process con eventos de parámetros + audio
    float inL[256], inR[256], outL[256], outR[256];
    for (uint32_t i = 0; i < block; ++i)
        inL[i] = inR[i] = 0.5f * sinf(2.0f * (float)M_PI * 220.0f * (float)i / sr);
    memset(outL, 0, sizeof(outL));
    memset(outR, 0, sizeof(outR));

    // eventos: PRIMERO Preset = Drum Bus Punch (0.5), DESPUÉS Num Bands = 4 (1.0)
    // (el orden importa: el preset Drum Bus usa 1 banda; pedimos 4 bandas al final)
    event_list_t evlist = {0};
    clap_event_param_value_t evBands, evPreset;
    if (idPreset != CLAP_INVALID_ID)
    {
        make_param_event(&evPreset, idPreset, 3.0 / 6.0);
        evlist.events[evlist.count++] = &evPreset.header;
    }
    if (idNumBands != CLAP_INVALID_ID)
    {
        make_param_event(&evBands, idNumBands, 1.0);
        evlist.events[evlist.count++] = &evBands.header;
    }

    clap_input_events_t in_events = {
        .ctx = &evlist, .size = ev_size, .get = ev_get,
    };

    clap_audio_buffer_t abuf_in[1], abuf_out[1];
    float *in_ptrs[2] = { inL, inR };
    float *out_ptrs[2] = { outL, outR };
    abuf_in[0].data32 = in_ptrs;   abuf_in[0].channel_count = 2;  abuf_in[0].latency = 0;
    abuf_out[0].data32 = out_ptrs; abuf_out[0].channel_count = 2; abuf_out[0].latency = 0;

    clap_process_t pr;
    memset(&pr, 0, sizeof(pr));
    pr.frames_count = block;
    pr.audio_inputs = abuf_in;   pr.audio_inputs_count = 1;
    pr.audio_outputs = abuf_out; pr.audio_outputs_count = 1;
    pr.in_events = &in_events;
    pr.out_events = &g_out_events;
    pr.steady_time = 0;

    int st = plugin->process(plugin, &pr);
    CHECK(st == CLAP_PROCESS_CONTINUE, "process() #1 (con eventos de params)");

    // verifica que los params tomaron los valores
    if (params && idNumBands != CLAP_INVALID_ID)
    {
        double v = -1;
        params->get_value(plugin, idNumBands, &v);
        printf("  Num Bands normalizado tras evento: %.3f\n", v);
        CHECK(v > 0.99, "Num Bands = 4 bandas aplicado");
    }
    if (params && idPreset != CLAP_INVALID_ID)
    {
        double v = -1;
        params->get_value(plugin, idPreset, &v);
        printf("  Preset normalizado tras evento: %.3f (esperado 0.5)\n", v);
        CHECK(v > 0.49 && v < 0.51, "Preset = Drum Bus Punch aplicado");
    }

    // 8) segundo process: audio puro (sin eventos)
    evlist.count = 0;
    st = plugin->process(plugin, &pr);
    CHECK(st == CLAP_PROCESS_CONTINUE, "process() #2 (audio puro)");

    int nonZero = 0, nanCount = 0, clip = 0;
    float maxAbs = 0.0f;
    for (uint32_t i = 0; i < block; ++i)
    {
        if (outL[i] != 0.0f || outR[i] != 0.0f) nonZero++;
        if (isnan(outL[i]) || isnan(outR[i])) nanCount++;
        float a = fmaxf(fabsf(outL[i]), fabsf(outR[i]));
        if (a > maxAbs) maxAbs = a;
        if (a > 1.0f) clip++;
    }
    printf("  salida: %d no-cero, max=%.4f, NaN=%d, clip=%d\n",
           nonZero, maxAbs, nanCount, clip);
    CHECK(nonZero > 100, "salida con audio");
    CHECK(nanCount == 0, "sin NaN");
    CHECK(clip == 0, "sin clipping — limiter OK");

    // 9) state round-trip
    const clap_plugin_state_t *state =
        (const clap_plugin_state_t *)plugin->get_extension(plugin, CLAP_EXT_STATE);
    CHECK(state != NULL, "extensión state");
    if (state)
    {
        mem_stream_t ms = {0};
        clap_ostream_t os = { .ctx = &ms, .write = mem_write };
        CHECK(state->save(plugin, &os), "state->save()");
        printf("  estado: %llu bytes\n", (unsigned long long)ms.size);
        CHECK(ms.size > 100, "estado con contenido");

        clap_istream_t is = { .ctx = &ms, .read = mem_read };
        CHECK(state->load(plugin, &is), "state->load() #1");
        ms.pos = 0;
        CHECK(state->load(plugin, &is), "state->load() #2 (repetido)");
        free(ms.data);
    }

    // 10) limpieza (protocolo correcto: stop → deactivate → destroy)
    plugin->stop_processing(plugin);
    CHECK(true, "stop_processing()");
    plugin->deactivate(plugin);
    CHECK(true, "deactivate()");
    plugin->destroy(plugin);
    CHECK(true, "destroy() sin crash");
    entry->deinit();
    CHECK(true, "deinit() sin crash");
    dlclose(lib);
    CHECK(true, "dlclose() sin crash");

    printf("\n=== RESULTADO: %s (%d fallos) ===\n",
           g_failures == 0 ? "TODO OK" : "HAY FALLOS", g_failures);
    return g_failures == 0 ? 0 : 1;
}
