
/**
   @file
   wes.repeat.simplify.c

   @name
   wes.repeat.simplify~

   @realname
   wes.repeat.simplify~

   @type
   object

   @module
   waveset

   @author
   Marco Marasciuolo

   @digest
    Repeats waveset

   @description
    Repeats one waveset every n.

   @discussion

   @category
   waveset basic

   @keywords
   buffer, waveset

   @seealso
    wes.repeat.overlap~ , wes.repeat.enveloping~, wes.repeat.attract~ , wes.repeat.pendulum~
   
   @owner
   Marco Marasciuolo
*/
#include "ext.h"
#include "ext_obex.h"
#include "foundation/llllobj.h"
#include "foundation/llll_commons_ext.h"
#include "math/bach_math_utilities.h"
#include "ears.object.h"



typedef struct _buf_wavesimplify {
    t_earsbufobj        e_ob;
    long sampMin_in;
    long nCross_in;
    int nextWave_in;
    t_llll  *envin;
    
} t_buf_wavesimplify;




// Prototypes
t_buf_wavesimplify*         buf_wavesimplify_new(t_symbol *s, short argc, t_atom *argv);
void            buf_wavesimplify_free(t_buf_wavesimplify *x);
void            buf_wavesimplify_bang(t_buf_wavesimplify *x);
void            buf_wavesimplify_anything(t_buf_wavesimplify *x, t_symbol *msg, long ac, t_atom *av);

void buf_wavesimplify_assist(t_buf_wavesimplify *x, void *b, long m, long a, char *s);
void buf_wavesimplify_inletinfo(t_buf_wavesimplify *x, void *b, long a, char *t);

void wavesimplify_bang(t_buf_wavesimplify *x, t_buffer_obj *buffer, t_buffer_obj *out, t_buffer_obj *mod, double modVal, int modType);
long ears_resample_linear(float *in, long num_in_frames, float **out, long num_out_frames, double factor, long num_channels);


// Globals and Statics
static t_class    *s_tag_class = NULL;
static t_symbol    *ps_event = NULL;

EARSBUFOBJ_ADD_IO_METHODS(wavesimplify)

/**********************************************************************/
// Class Definition and Life Cycle

void C74_EXPORT ext_main(void* moduleRef)
{
    common_symbols_init();
    llllobj_common_symbols_init();
    
    if (llllobj_check_version(bach_get_current_llll_version()) || llllobj_test()) {
        ears_error_bachcheck();
        return;
    }
    
    t_class *c;
    
    CLASS_NEW_CHECK_SIZE(c, "wes.repeat.simplify~",
                         (method)buf_wavesimplify_new,
                         (method)buf_wavesimplify_free,
                         sizeof(t_buf_wavesimplify),
                         (method)NULL,
                         A_GIMME,
                         0L);
    
    // @method symbol @digest Process buffers
    // @description A symbol with buffer name will trigger the buffer processing and output the processed
    // buffer name (depending on the <m>naming</m> attribute).
    EARSBUFOBJ_DECLARE_COMMON_METHODS_HANDLETHREAD(wavesimplify)
    
    CLASS_ATTR_LONG(c, "minSamp", 0, t_buf_wavesimplify, sampMin_in);
    CLASS_ATTR_LONG(c, "nCross", 0, t_buf_wavesimplify, nCross_in);
    CLASS_ATTR_LONG(c, "nextWaveMult", 0, t_buf_wavesimplify, nextWave_in);
   

    earsbufobj_class_add_outname_attr(c);
    earsbufobj_class_add_blocking_attr(c);
    earsbufobj_class_add_naming_attr(c);
    
    earsbufobj_class_add_polyout_attr(c);

    class_register(CLASS_BOX, c);
    s_tag_class = c;
    ps_event = gensym("event");
}

void buf_wavesimplify_assist(t_buf_wavesimplify *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET) {
        if (a == 0) // @in 0 @type symbol @digest Buffer name(s)
            sprintf(s, "symbol: Buffer Names");
        else if (a == 1) // @in 1 @type number/symbol @digest number of waveset skip
            sprintf(s, "Buffer/float: number of waveset skip"); // @"); // @number of waveset skip
    } else {
        sprintf(s, "New Buffer Names"); // @out 0 @type symbol/list @digest Output buffer names(s)
                                            // @description Name of the buffer
    }
}

void buf_wavesimplify_inletinfo(t_buf_wavesimplify *x, void *b, long a, char *t)
{
    if (a)
        *t = 1;
}


t_buf_wavesimplify *buf_wavesimplify_new(t_symbol *s, short argc, t_atom *argv)
{
    t_buf_wavesimplify *x;
    long true_ac = attr_args_offset(argc, argv);
    
    x = (t_buf_wavesimplify*)object_alloc_debug(s_tag_class);
    if (x) {
        
        x->envin = llll_from_text_buf("1", false);
        x->sampMin_in = 15;
        x->nCross_in = 1;
        x->nextWave_in = 1;
   
        earsbufobj_init((t_earsbufobj *)x,  0);
        
        // @arg 0 @name outnames @optional 1 @type symbol
        // @digest Output buffer names
        // @description @copy EARS_DOC_OUTNAME_ATTR
        
        t_llll *args = llll_parse(true_ac, argv);
        t_llll *names = earsbufobj_extract_names_from_args((t_earsbufobj *)x, args);
        
        if (args && args->l_head) {
            llll_clear(x->envin);
            llll_appendhatom_clone(x->envin, &args->l_head->l_hatom);
        }
        
        attr_args_process(x, argc, argv);
        
        earsbufobj_setup((t_earsbufobj *)x, "E4", "E", names);
        
        llll_free(args);
        llll_free(names);
    }
    return x;
}


void buf_wavesimplify_free(t_buf_wavesimplify *x)
{
    llll_free(x->envin);
    earsbufobj_free((t_earsbufobj *)x);
    
}




void buf_wavesimplify_bang(t_buf_wavesimplify *x)
{
    long num_buffers = earsbufobj_get_instore_size((t_earsbufobj *)x, 0);
    earsbufobj_refresh_outlet_names((t_earsbufobj *)x);
    earsbufobj_resize_store((t_earsbufobj *)x, EARSBUFOBJ_IN, 0, num_buffers, true);
    
    earsbufobj_mutex_lock((t_earsbufobj *)x);
    earsbufobj_init_progress((t_earsbufobj *)x, num_buffers);
    
    t_llllelem *el = x->envin->l_head;
    t_buffer_obj *mod = NULL;
    double modVal = 1;
    int modType = 0;

    for (long count = 0; count < num_buffers; count++) {
        t_buffer_obj *in = earsbufobj_get_inlet_buffer_obj((t_earsbufobj *)x, 0, count);
        t_buffer_obj *out = earsbufobj_get_outlet_buffer_obj((t_earsbufobj *)x, 0, count);
        
        t_llll *env = earsbufobj_llllelem_to_env_samples((t_earsbufobj *)x, el, in);
        
        if (hatom_gettype(&env->l_head->l_hatom) == H_SYM) {
            
            t_buffer_ref *ref = buffer_ref_new((t_object *)x, hatom_getsym(&env->l_head->l_hatom));
            mod = buffer_ref_getobject(ref);
            modType = 1;
            
        } else {
            modVal = hatom_getdouble(&el->l_hatom);
            modType = 2;
        }
        
        llll_free(env);
        
        wavesimplify_bang(x, in, out, mod, modVal, modType);
        
        if (earsbufobj_iter_progress((t_earsbufobj *)x, count, num_buffers)) break;
    }
    
    earsbufobj_mutex_unlock((t_earsbufobj *)x);

    earsbufobj_outlet_buffer((t_earsbufobj *)x, 0);
}


void buf_wavesimplify_anything(t_buf_wavesimplify *x, t_symbol *msg, long ac, t_atom *av)
{
    long inlet = earsbufobj_proxy_getinlet((t_earsbufobj *) x);
    
    t_llll *parsed = earsbufobj_parse_gimme((t_earsbufobj *) x, LLLL_OBJ_VANILLA, msg, ac, av);
    if (!parsed) return;
    
    if (parsed && parsed->l_head) {
        if (inlet == 0) {
            long num_bufs = llll_get_num_symbols_root(parsed);
            earsbufobj_resize_store((t_earsbufobj *)x, EARSBUFOBJ_IN, 0, num_bufs, true);
            earsbufobj_resize_store((t_earsbufobj *)x, EARSBUFOBJ_OUT, 0, num_bufs, true);
            earsbufobj_store_buffer_list((t_earsbufobj *)x, parsed, 0);
            
            buf_wavesimplify_bang(x);
        
        } else if (inlet == 1) {
            llll_free(x->envin);
            x->envin = llll_clone(parsed);
        }
        
    }
    llll_free(parsed);
}


void wavesimplify_bang(t_buf_wavesimplify *x, t_buffer_obj *buffer, t_buffer_obj *out, t_buffer_obj *mod, double modVal, int modType) {
    
    t_float        *tab;
    t_float        *envelope;
    
    int minsampl = CLAMP(x->sampMin_in, 1, 5000);
    int ncross = CLAMP(x->nCross_in, 1, 1000);
    int nextWaveMult = CLAMP(x->nextWave_in, 0, 5000);
    
    
    long        frames, sampleRate, envelopeFrames ;
    
    tab = ears_buffer_locksamples(buffer);
    frames = buffer_getframecount(buffer);
    sampleRate = buffer_getsamplerate(buffer);

    
    envelope = ears_buffer_locksamples(mod);
    envelopeFrames = buffer_getframecount(mod);


    
    int nchan;
    nchan = buffer_getchannelcount(buffer);
    double *inbuffer =  (double*) sysmem_newptr(frames * sizeof(double));
    
    
    int maxcross = round(frames / (minsampl * ncross));
    int *zerocrossindex = (int*) sysmem_newptr(maxcross * sizeof(int));
    int maxmemory = (int)(frames * 4);
    double *dataout =  (double*) sysmem_newptr(maxmemory * sizeof(double));
    
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    int z, v , ver, frameout = 0;
    for (z = 1 ; z < (nchan + 1) ; z++) {
        int inc = 0;

        for (v = 0 ; v < (frames * nchan) ; v++) {
            ver = (v % nchan) + 1;
            if (ver == z) {
                inc++;
                inbuffer[inc] = tab[v];
            }
        }
      
        int  g = 1, minsamplcount = 0, h = 0, j, k, ncrossindex = 0, crosscount = 0, n = 0, newPeriod, currPeriod, nextPeriod, currIndexA, currIndexB, muteFadeIn, nextWaveCount, nextWave;
       
        
        // qwaveset segmentation
        for (j = 0 ; j < frames ; j++) {
            minsamplcount++;
            
            if (inbuffer [j] >= 0 && inbuffer [j - 1] <= 0 && minsamplcount > minsampl) {
                minsamplcount = 0;
                ncrossindex++;
                
                if (ncrossindex == ncross) {
                    ncrossindex = 0;
                    crosscount++;
                    if (crosscount >= maxcross) {
                        maxcross = maxcross + round(maxcross/4);
                        zerocrossindex = (int*) sysmem_resizeptrclear(zerocrossindex, maxcross * sizeof(int));
                    }
                    
                    zerocrossindex[crosscount] = j;
                }
                
            }
        }
        
        zerocrossindex[0] = 0;
        
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        float *envOnset = (float *)bach_newptr(crosscount * sizeof(float));
        if (modType == 1) {
            nextWaveCount = nextWaveMult;
             ears_resample_linear(envelope, envelopeFrames, &envOnset, crosscount, ((float)crosscount/(float)envelopeFrames), 1);
        } else {
            nextWaveCount = modVal;
            for (int gg = 0 ; gg < crosscount ; gg++) {
                envOnset[gg] = modVal;
            }
        }
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        
        if (nextWaveCount >= crosscount) {
            post("too many nextWave, total number of waveset is: %d", crosscount);
            
        } else {
            
            while (g <= crosscount && (g + nextWaveCount) <= crosscount) {
                
                if (modType == 1) {
                    nextWave = round(CLAMP(envOnset[g], 0 , 1) * nextWaveMult);
                } else {
                    nextWave = CLAMP(modVal, 0 , 5000);
                }
                
                currPeriod = (zerocrossindex[g] - zerocrossindex[g  - 1]);
                
                nextPeriod = (zerocrossindex[g + nextWave] - zerocrossindex[(g + nextWave) - 1]);
                
                int distance = zerocrossindex[g + nextWave] - zerocrossindex[g];
                
                if ((g + (nextWave * 2)) >= crosscount) {
                    muteFadeIn = 0;
                } else {
                    muteFadeIn = 1;
                }
                
                if (distance < currPeriod || distance < nextPeriod) {
                    newPeriod = MAX(currPeriod, nextPeriod);
                } else {
                    newPeriod = ((int)((float)distance/nextPeriod)) * nextPeriod;
                }
                
                
                while (n < newPeriod) {
                    
                    float fadeIn = sin(((float)n/(float)newPeriod) * (PI * 0.5));
                    float fadeOut = cos(((float)n/(float)newPeriod) * (PI * 0.5));
                    
                    currIndexA = zerocrossindex[g  - 1] + (n % currPeriod);
                    currIndexB = zerocrossindex[(g + nextWave) - 1] + (n % nextPeriod);
                    
                    
                    if (h >= maxmemory) {
                        maxmemory = maxmemory + round(maxmemory/4);
                        dataout =  (double*)sysmem_resizeptrclear(dataout, maxmemory * sizeof(double));
                    }
                    
                    dataout[h] = (inbuffer[currIndexA] * fadeOut) +  (inbuffer[currIndexB] * fadeIn * muteFadeIn);
                    
                    
                    h++;
                    n++;
                }
                
                n = 0;
                
                if (nextWave == 0) {
                    g++;
                } else {
                    g = g + nextWave;
                }
                
            }
        }
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        
        if (z == 1) {
             frameout = h - 1;
        }
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        
        ears_buffer_set_size_and_numchannels((t_object *) x, out, frameout, nchan);
        ears_buffer_set_sr((t_object *) x, out, sampleRate);
        
        float *outtab = ears_buffer_locksamples(out);
        
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        inc = 0;
        for (k = 0 ; k < (frameout * nchan) ; k++) {
            ver = (k % nchan) + 1;
            if (ver == z) {
                inc++;
                double f = dataout[inc];
                outtab[k] = f;
            }
            
        }
        ears_buffer_unlocksamples(out);
        bach_freeptr(envOnset);
    }
    
    buffer_unlocksamples(buffer);
    buffer_unlocksamples(mod);
    sysmem_freeptr(zerocrossindex);
    sysmem_freeptr(dataout);
    sysmem_freeptr(inbuffer);
    
    return;
}
