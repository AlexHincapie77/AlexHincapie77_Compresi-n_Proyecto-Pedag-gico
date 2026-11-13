// ============================================================================
//  multiband_meter.c — Host CLAP de medición de saturación (v2, structs reales)
// ============================================================================
#include <clap/clap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dlfcn.h>

// ─── Mock host (name/vendor/version son campos directos, no callbacks) ────
static const void *host_ext(const clap_host_t *h, const char *id) { (void)h; (void)id; return NULL; }
static void host_req_restart(const clap_host_t *h) { (void)h; }
static void host_req_process(const clap_host_t *h) { (void)h; }
static void host_req_callback(const clap_host_t *h) { (void)h; }

static clap_host_t g_host = {
    { 1, 2, 7 },        /* clap_version */
    NULL,               /* host_data */
    "MedidorSat",       /* name */
    "PedagogicalAudio", /* vendor */
    "https://example.test", /* url */
    "1.0",              /* version */
    host_ext,
    host_req_restart,
    host_req_process,
    host_req_callback
};

// ─── Lista de eventos de entrada (pre-llenada antes de process) ───────────
#define MAX_EV 8
typedef struct {
    clap_input_events_t iface;
    clap_event_header_t *evs[MAX_EV];
    uint32_t count;
} ev_list_in;

static uint32_t evl_size(const struct clap_input_events *l)
{ return ((ev_list_in *)l)->count; }
static const clap_event_header_t *evl_get(const struct clap_input_events *l, uint32_t i)
{ return ((ev_list_in *)l)->evs[i]; }
static void evl_reset(ev_list_in *l) { l->count = 0; }
static void evl_init(ev_list_in *l)
{
    memset(l, 0, sizeof(*l));
    l->iface.ctx = l;
    l->iface.size = evl_size;
    l->iface.get = evl_get;
}
static void evl_add(ev_list_in *l, clap_event_header_t *e)
{ if (l->count < MAX_EV) l->evs[l->count++] = e; }

// ─── Lista de eventos de salida (papelera) ────────────────────────────────
typedef struct { clap_output_events_t iface; } ev_list_out;
static bool evl_out_push(const struct clap_output_events *l, const clap_event_header_t *e)
{ (void)l; (void)e; return true; }
static void evl_out_init(ev_list_out *l)
{
    memset(l, 0, sizeof(*l));
    l->iface.ctx = l;
    l->iface.try_push = evl_out_push;
}

// ─── Parámetros ────────────────────────────────────────────────────────────
static const clap_plugin_params_t *g_params = NULL;
static const clap_plugin_t *g_plugin = NULL;

static clap_id find_param_by_name(const char *name)
{
    uint32_t n = g_params->count(g_plugin);
    for (uint32_t i = 0; i < n; ++i)
    {
        clap_param_info_t info;
        if (g_params->get_info(g_plugin, i, &info))
            if (strcmp(info.name, name) == 0)
                return info.id;
    }
    return CLAP_INVALID_ID;
}

// ─── Generador de señal tipo música ────────────────────────────────────────
static long g_t = 0;
static float gen_sample(long t)
{
    const double sr = 44100.0;
    double s =
        0.24 * sin(2.0 * M_PI * 55.0  * t / sr) +
        0.20 * sin(2.0 * M_PI * 220.0 * t / sr) +
        0.16 * sin(2.0 * M_PI * 1000.0 * t / sr) +
        0.10 * sin(2.0 * M_PI * 3500.0 * t / sr);
    long phase = t % 22050;
    if (phase < 5292)
    {
        double env = exp(-phase / 1200.0);
        s += 0.55 * env * sin(2.0 * M_PI * 60.0 * phase / sr);
    }
    return (float)s;    // pico ≈ 0.85
}

// ─── Medición ──────────────────────────────────────────────────────────────
typedef struct {
    double peak, sum_sq, sum;
    long n, nonfinite, limited;
} meas_t;

static void meas_add(meas_t *m, float v, double ceiling)
{
    if (!isfinite(v)) { m->nonfinite++; return; }
    double a = fabs((double)v);
    if (a > m->peak) m->peak = a;
    m->sum_sq += (double)v * (double)v;
    m->sum += (double)v;
    m->n++;
    if (a >= 0.95 * ceiling) m->limited++;
}

static void meas_report(const char *label, const meas_t *m)
{
    double peakDb = 20.0 * log10(m->peak + 1e-12);
    double rmsDb  = 20.0 * log10(sqrt(m->sum_sq / (m->n ? m->n : 1)) + 1e-12);
    double dc     = m->sum / (m->n ? m->n : 1);
    double limPct = 100.0 * m->limited / (m->n ? m->n : 1);
    printf("  %-26s peak %6.2f dBFS | rms %6.2f | DC %+.5f | NaN %ld | lim %5.1f%%\n",
           label, peakDb, rmsDb, dc, m->nonfinite, limPct);
}

// ─── Procesa bloques con eventos opcionales y mide ────────────────────────
#define BLOCK 256

static void run_and_measure(const clap_plugin_t *p, long nBlocks,
                            const char *label)
{
    static float inL[BLOCK], inR[BLOCK], outL[BLOCK], outR[BLOCK];
    static float scL[BLOCK], scR[BLOCK];      // sidechain (silencio)
    float *inCh[4]  = { inL, inR, scL, scR };
    float *outCh[2] = { outL, outR };
    const double ceiling = 0.891;

    ev_list_in evin;  evl_init(&evin);
    ev_list_out evout; evl_out_init(&evout);
    meas_t m; memset(&m, 0, sizeof(m));

    for (long b = 0; b < nBlocks; ++b)
    {
        for (int i = 0; i < BLOCK; ++i)
        {
            float s = gen_sample(g_t++);
            inL[i] = s; inR[i] = s * 0.96f;
            scL[i] = scR[i] = 0.0f;
            outL[i] = outR[i] = 0.0f;
        }

        clap_audio_buffer_t abuf_in  = { (float *const *)inCh,  NULL, 4, 0, 0 };
        clap_audio_buffer_t abuf_out = { (float *const *)outCh, NULL, 2, 0, 0 };
        clap_process_t proc;
        memset(&proc, 0, sizeof(proc));
        proc.steady_time = -1;
        proc.frames_count = BLOCK;
        proc.audio_inputs  = &abuf_in;
        proc.audio_inputs_count = 1;
        proc.audio_outputs = &abuf_out;
        proc.audio_outputs_count = 1;
        proc.in_events  = &evin.iface;
        proc.out_events = &evout.iface;

        p->process(p, &proc);
        evl_reset(&evin);

        for (int i = 0; i < BLOCK; ++i)
        {
            meas_add(&m, outL[i], ceiling);
            meas_add(&m, outR[i], ceiling);
        }
    }
    if (label) meas_report(label, &m);
}

// Envía un bloque con eventos de cambio de parámetros
static void send_param_block(const clap_plugin_t *p, clap_id *ids, double *vals, int n)
{
    static float inL[BLOCK], inR[BLOCK], outL[BLOCK], outR[BLOCK];
    static float scL[BLOCK], scR[BLOCK];
    float *inCh[4]  = { inL, inR, scL, scR };
    float *outCh[2] = { outL, outR };

    ev_list_in evin;  evl_init(&evin);
    ev_list_out evout; evl_out_init(&evout);

    static clap_event_param_value_t evs[MAX_EV];

    for (int i = 0; i < BLOCK; ++i)
    {
        float s = gen_sample(g_t++);
        inL[i] = s; inR[i] = s * 0.96f;
        scL[i] = scR[i] = 0.0f;
        outL[i] = outR[i] = 0.0f;
    }
    for (int k = 0; k < n && k < MAX_EV; ++k)
    {
        memset(&evs[k], 0, sizeof(evs[k]));
        evs[k].header.size = sizeof(evs[k]);
        evs[k].header.time = 0;
        evs[k].header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        evs[k].header.type = CLAP_EVENT_PARAM_VALUE;
        evs[k].header.flags = 0;
        evs[k].param_id = ids[k];
        evs[k].value = vals[k];
        evl_add(&evin, &evs[k].header);
    }

    clap_audio_buffer_t abuf_in  = { (float *const *)inCh,  NULL, 4, 0, 0 };
    clap_audio_buffer_t abuf_out = { (float *const *)outCh, NULL, 2, 0, 0 };
    clap_process_t proc;
    memset(&proc, 0, sizeof(proc));
    proc.steady_time = -1;
    proc.frames_count = BLOCK;
    proc.audio_inputs  = &abuf_in;
    proc.audio_inputs_count = 1;
    proc.audio_outputs = &abuf_out;
    proc.audio_outputs_count = 1;
    proc.in_events  = &evin.iface;
    proc.out_events = &evout.iface;
    fprintf(stderr, "[send_param_block: procesando con %d eventos]\n", n);
    p->process(p, &proc);
    fprintf(stderr, "[send_param_block: OK]\n");
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "uso: %s plugin.clap\n", argv[0]); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);   // sin buffer: ver prints antes de un crash

    fprintf(stderr, "[ck1] dlopen...\n");
    void *h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!h) { fprintf(stderr, "FAIL dlopen: %s\n", dlerror()); return 1; }
    fprintf(stderr, "[ck2] dlsym clap_entry...\n");
    const clap_plugin_entry_t *entry = (const clap_plugin_entry_t *)dlsym(h, "clap_entry");
    if (!entry) { fprintf(stderr, "FAIL: no clap_entry\n"); return 1; }
    fprintf(stderr, "[ck3] entry->init...\n");
    if (!entry->init(argv[1])) { fprintf(stderr, "FAIL init\n"); return 1; }
    fprintf(stderr, "[ck4] get_factory...\n");
    const clap_plugin_factory_t *factory =
        (const clap_plugin_factory_t *)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    fprintf(stderr, "[ck5] create_plugin...\n");
    const clap_plugin_t *p = factory->create_plugin(factory, &g_host, "com.pedagogicalaudio.compressor");
    if (!p) { fprintf(stderr, "FAIL create_plugin\n"); return 1; }
    fprintf(stderr, "[ck6] p->init...\n");
    if (!p->init(p)) { fprintf(stderr, "FAIL plugin init\n"); return 1; }
    fprintf(stderr, "[ck7] activate...\n");
    if (!p->activate(p, 44100.0, 32, 512)) { fprintf(stderr, "FAIL activate\n"); return 1; }
    fprintf(stderr, "[ck8] start_processing...\n");
    p->start_processing(p);
    fprintf(stderr, "[ck9] OK\n");

    fprintf(stderr, "[ck10] get_extension params...\n");
    g_params = (const clap_plugin_params_t *)p->get_extension(p, CLAP_EXT_PARAMS);
    if (!g_params) { fprintf(stderr, "FAIL: sin extensión params\n"); return 1; }
    g_plugin = p;
    fprintf(stderr, "[ck11] find numBands...\n");

    clap_id idNumBands = find_param_by_name("Num Bands");
    clap_id idPreset   = find_param_by_name("Preset");
    clap_id idAutoGain = find_param_by_name("Auto Gain");
    clap_id idAmk[4], idSat[4];
    for (int b = 0; b < 4; ++b)
    {
        char nm[64];
        snprintf(nm, sizeof(nm), "B%d Auto Makeup", b); idAmk[b] = find_param_by_name(nm);
        snprintf(nm, sizeof(nm), "B%d Saturation", b);  idSat[b] = find_param_by_name(nm);
    }
    fprintf(stderr, "[ck12] params found\n");
    printf("params → numBands=%u preset=%u autoGain=%u amk=[%u %u %u %u] sat=[%u %u %u %u]\n",
           idNumBands, idPreset, idAutoGain,
           idAmk[0], idAmk[1], idAmk[2], idAmk[3],
           idSat[0], idSat[1], idSat[2], idSat[3]);

    const long warm = 60;
    const long meas = 400;

    printf("\n════ SEÑAL: multitono+kick pico≈-1.4 dBFS · ceiling -1 dBFS ════\n");

    // Test de proceso SIN eventos (aisla el crash)
    fprintf(stderr, "[ck13] proceso SIN eventos...\n");
    run_and_measure(p, 4, "plain (sin eventos)");
    fprintf(stderr, "[ck13b] proceso sin eventos OK\n");

    // ── Escenario 1: defaults, 1→4 bandas ──
    printf("\n── ESCENARIO 1: defaults (thr 0 dB → sin GR), 1→4 bandas ──\n");
    {
        clap_id ids[6]; double vals[6];
        int n = 0;
        ids[n]=idPreset;   vals[n++]=0.0;              // Default
        ids[n]=idAutoGain;vals[n++]=0.0;
        for (int b=0;b<4;++b){ ids[n]=idAmk[b]; vals[n++]=0.0; }
        send_param_block(p, ids, vals, n);
    }
    for (int nb = 1; nb <= 4; ++nb)
    {
        fprintf(stderr, "[ck14] nb=%d send_param...\n", nb);
        clap_id ids[1] = { idNumBands };
        double vals[1] = { (nb - 1) / 3.0 };
        send_param_block(p, ids, vals, 1);
        run_and_measure(p, warm, NULL);
        char lbl[64]; snprintf(lbl, sizeof(lbl), "%d banda%s (medido)", nb, nb>1?"s":"");
        run_and_measure(p, meas, lbl);
    }

    // ── Escenario 2: preset «4-Band Stereo Master» ──
    printf("\n── ESCENARIO 2: preset «4-Band Stereo Master» (thr -15..-8, mk +1..+2) ──\n");
    {
        clap_id ids[2]; double vals[2];
        ids[0]=idPreset;   vals[0]=5.0/6.0;
        ids[1]=idNumBands; vals[1]=1.0;   // 4 bandas
        send_param_block(p, ids, vals, 2);
    }
    run_and_measure(p, warm, NULL);
    run_and_measure(p, meas, "preset 4-Band ×4 bandas");

    // ── Escenario 3: preset + AutoMK ×4 ──
    printf("\n── ESCENARIO 3: preset 4-Band + AutoMK ×4 ──\n");
    {
        clap_id ids[4]; double vals[4];
        for (int b=0;b<4;++b){ ids[b]=idAmk[b]; vals[b]=1.0; }
        send_param_block(p, ids, vals, 4);
    }
    run_and_measure(p, warm*2, NULL);
    run_and_measure(p, meas, "preset + AutoMK ×4");

    // ── Escenario 4: preset + AutoMK + Sat máx ──
    printf("\n── ESCENARIO 4: preset + AutoMK ×4 + Sat 100%% ×4 (peor caso) ──\n");
    {
        clap_id ids[8]; double vals[8];
        int n=0;
        for (int b=0;b<4;++b){ ids[n]=idSat[b]; vals[n++]=1.0; }
        for (int b=0;b<4;++b){ ids[n]=idAmk[b]; vals[n++]=1.0; }
        send_param_block(p, ids, vals, n);
    }
    run_and_measure(p, warm*2, NULL);
    run_and_measure(p, meas, "preset + AutoMK + Sat máx");

    // ── Escenario 5: + Auto-Gain global ──
    printf("\n── ESCENARIO 5: TODO activo (+ AutoGain global) ──\n");
    {
        clap_id ids[1] = { idAutoGain };
        double vals[1] = { 1.0 };
        send_param_block(p, ids, vals, 1);
    }
    run_and_measure(p, warm*4, NULL);
    run_and_measure(p, meas, "todo activo (peor caso)");

    printf("\n>>> FIN\n");
    p->stop_processing(p);
    p->deactivate(p);
    p->destroy(p);
    entry->deinit();
    dlclose(h);
    return 0;
}
