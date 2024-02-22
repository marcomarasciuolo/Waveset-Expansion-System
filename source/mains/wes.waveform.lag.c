/**
   @file
   wes.waveform.lag.c

   @name
   wes.waveform.lag~

   @realname
   wes.waveform.lag~

   @type
   object

   @module
   waveset

   @author
   Marco Marasciuolo

   @digest
    Waveset spacing

   @description
   Inserts a portion of silence at the end of each waveset.

   @discussion

   @category
   waveset basic

   @keywords
   buffer, waveset

   @seealso
    wes.waveform.uniform~, wes.waveform.reduction~,  wes.waveform.shift~,  wes.waveform.interpolate~
   
   @owner
   Marco Marasciuolo
*/
#include "ext.h"
#include "ext_obex.h"
#include "foundation/llllobj.h"
#include "foundation/llll_commons_ext.h"
#include "math/bach_math_utilities.h"
#include "ears.object.h"



typedef struct _buf_wavelag {
    t_earsbufobj        e_ob;
    long sampMin_in;
    long cross_in;
    float lag_in;
    t_llll  *envin;
    
} t_buf_wavelag;




// Prototypes
t_buf_wavelag*         buf_wavelag_new(t_symbol *s, short argc, t_atom *argv);
void            buf_wavelag_free(t_buf_wavelag *x);
void            buf_wavelag_bang(t_buf_wavelag *x);
void            buf_wavelag_anything(t_buf_wavelag *x, t_symbol *msg, long ac, t_atom *av);

void buf_wavelag_assist(t_buf_wavelag *x, void *b, long m, long a, char *s);
void buf_wavelag_inletinfo(t_buf_wavelag *x, void *b, long a, char *t);

void wavelag_bang(t_buf_wavelag *x, t_buffer_obj *buffer, t_buffer_obj *out, t_buffer_obj *mod, double modVal, int modType);
long ears_resample_linear(float *in, long num_in_frames, float **out, long num_out_frames, double factor, long num_channels);


// Globals and Statics
static t_class    *s_tag_class = NULL;
static t_symbol    *ps_event = NULL;

EARSBUFOBJ_ADD_IO_METHODS(wavelag)

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
    
    CLASS_NEW_CHECK_SIZE(c, "wes.waveform.lag~",
                         (method)buf_wavelag_new,
                         (method)buf_wavelag_free,
                         sizeof(t_buf_wavelag),
                         (method)NULL,
                         A_GIMME,
                         0L);
    
    // @method symbol @digest Process buffers
    // @description A buffer name will trigger the buffer processing and output the processed
    // buffer name (depending on the <m>naming</m> attribute).
    EARSBUFOBJ_DECLARE_COMMON_METHODS_HANDLETHREAD(wavelag)
    
    CLASS_ATTR_LONG(c, "minSamp", 0, t_buf_wavelag, sampMin_in);
    CLASS_ATTR_LONG(c, "cross", 0, t_buf_wavelag, cross_in);
    CLASS_ATTR_FLOAT(c, "lagmult", 0, t_buf_wavelag, lag_in);
   

    earsbufobj_class_add_outname_attr(c);
    earsbufobj_class_add_blocking_attr(c);
    earsbufobj_class_add_naming_attr(c);
    
    earsbufobj_class_add_polyout_attr(c);

    class_register(CLASS_BOX, c);
    s_tag_class = c;
    ps_event = gensym("event");
}

void buf_wavelag_assist(t_buf_wavelag *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET) {
        if (a == 0) // @in 0 @type symbol @digest Buffer name(s)
            sprintf(s, "symbol: Buffer Names");
        else if (a == 1) // @in 1 @type number/symbol @digest multiply silence portion
            sprintf(s, "Buffer/float: multiply silence portion"); // @multiply silence portion
    } else {
        sprintf(s, "New Buffer Names"); // @out 0 @type symbol/list @digest Output buffer names(s)
                                            // @description Name of the buffer
    }
}

void buf_wavelag_inletinfo(t_buf_wavelag *x, void *b, long a, char *t)
{
    if (a)
        *t = 1;
}


t_buf_wavelag *buf_wavelag_new(t_symbol *s, short argc, t_atom *argv)
{
    t_buf_wavelag *x;
    long true_ac = attr_args_offset(argc, argv);
    
    x = (t_buf_wavelag*)object_alloc_debug(s_tag_class);
    if (x) {
        
        x->envin = llll_from_text_buf("1", false);
        x->sampMin_in = 15;
        x->cross_in = 1;
        x->lag_in = 1;
   
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


void buf_wavelag_free(t_buf_wavelag *x)
{
    llll_free(x->envin);
    earsbufobj_free((t_earsbufobj *)x);
    
}




void buf_wavelag_bang(t_buf_wavelag *x)
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
        
        wavelag_bang(x, in, out, mod, modVal, modType);
        
        if (earsbufobj_iter_progress((t_earsbufobj *)x, count, num_buffers)) break;
    }
    
    earsbufobj_mutex_unlock((t_earsbufobj *)x);

    earsbufobj_outlet_buffer((t_earsbufobj *)x, 0);
}


void buf_wavelag_anything(t_buf_wavelag *x, t_symbol *msg, long ac, t_atom *av)
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
            
            buf_wavelag_bang(x);
        
        } else if (inlet == 1) {
            llll_free(x->envin);
            x->envin = llll_clone(parsed);
        }
        
    }
    llll_free(parsed);
}


void wavelag_bang(t_buf_wavelag *x, t_buffer_obj *buffer, t_buffer_obj *out, t_buffer_obj *mod, double modVal, int modType) {
    
    t_float        *tab;
    t_float        *envelope;
    
    int minsampl = CLAMP(x->sampMin_in, 1, 5000);
    int ncross = CLAMP(x->cross_in, 1, 1000);
    float lagmultiply = CLAMP(x->lag_in, 0, 1000);
    
    
    
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
      
        int m = 0, g = 1, minsamplcount = 0, h = 0, b = 0, j, k, ncrossindex = 0, crosscount = 0;
        float silence;
        
        // waveset segmentation
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
             ears_resample_linear(envelope, envelopeFrames, &envOnset, crosscount, ((float)crosscount/(float)envelopeFrames), 1);
        } else {
            for (int gg = 0 ; gg < crosscount ; gg++) {
                envOnset[gg] = modVal;
            }
        }
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        
        
        while (m < frames) {
         
            
            if (h > maxmemory) {
                
                maxmemory = maxmemory + round(maxmemory/4);
                dataout =  (double*) sysmem_resizeptrclear(dataout, maxmemory * sizeof(double));
            }
            
            dataout[h] = inbuffer[m];
            
            
            if (m == zerocrossindex[g]-1) {
                
                if (modType == 1) {
                    silence = lagmultiply;
                } else {
                    silence = 1;
                }
                while (b <= ((zerocrossindex[g] - zerocrossindex[g  - 1])) * (CLAMP(envOnset[g], 0, 500) * silence) ) {
                    b++;
                    h++;
                    
                    if (h >= maxmemory) {
                        maxmemory = maxmemory + round(maxmemory/4);
                        dataout = (double*) sysmem_resizeptrclear(dataout, maxmemory * sizeof(double));
                    }
                    dataout[h] = 0;

                }
                b = 0;
                g++;
            }
            m++;
            h++;
        }
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        
        if (z == 1) {
             frameout = h - 1;;
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
