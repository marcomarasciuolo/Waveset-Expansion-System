
/**
@file
wes.repeat.overlap.c

@name
wes.repeat.overlap~

@realname
wes.repeat.overlap~

@type
object

@module
waveset

@author
Marco Marasciuolo

@digest
 Repeats and overlap 

@description
Repeats each wavesets and overlap n. layer.

@discussion

@category
waveset basic

@keywords
buffer, waveset

@seealso
wes.repeat.enveloping~, wes.repeat.attract~ , wes.repeat.pendulum~, wes.repeat.simplify~, 

@owner
Marco Marasciuolo
 
 */
#include "ext.h"
#include "ext_obex.h"
#include "foundation/llllobj.h"
#include "foundation/llll_commons_ext.h"
#include "math/bach_math_utilities.h"
#include "ears.object.h"





typedef struct _buf_repeatoverlap {
    t_earsbufobj        e_ob;
    long sampMin_in;
    int  repeatMult_in;
    long cross_in;
    long nOverlap_in;
    int maxOutChannel_in;
    t_llll  *envin;
    
} t_buf_repeatoverlap;




// Prototypes
t_buf_repeatoverlap*         buf_repeatoverlap_new(t_symbol *s, short argc, t_atom *argv);
void            buf_repeatoverlap_free(t_buf_repeatoverlap *x);
void            buf_repeatoverlap_bang(t_buf_repeatoverlap *x);
void            buf_repeatoverlap_anything(t_buf_repeatoverlap *x, t_symbol *msg, long ac, t_atom *av);

void buf_repeatoverlap_assist(t_buf_repeatoverlap *x, void *b, long m, long a, char *s);
void buf_repeatoverlap_inletinfo(t_buf_repeatoverlap *x, void *b, long a, char *t);

void wavesetrepeat_bang(t_buf_repeatoverlap *x, t_buffer_obj *buffer, t_buffer_obj *out, t_buffer_obj *mod, double modVal, int modType);

// Globals and Statics
static t_class    *s_tag_class = NULL;
static t_symbol    *ps_event = NULL;

EARSBUFOBJ_ADD_IO_METHODS(repeatoverlap)

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
    
    CLASS_NEW_CHECK_SIZE(c, "wes.repeat.overlap~",
                         (method)buf_repeatoverlap_new,
                         (method)buf_repeatoverlap_free,
                         sizeof(t_buf_repeatoverlap),
                         (method)NULL,
                         A_GIMME,
                         0L);
    
     // @method symbol @digest Process buffers
    // @description A symbol with buffer name will trigger the buffer processing and output the processed
    // buffer name (depending on the <m>naming</m> attribute).
    EARSBUFOBJ_DECLARE_COMMON_METHODS_HANDLETHREAD(repeatoverlap)
    
    CLASS_ATTR_LONG(c, "minsamp", 0, t_buf_repeatoverlap, sampMin_in);
    CLASS_ATTR_LONG(c, "repeatmult", 0, t_buf_repeatoverlap, repeatMult_in);
    CLASS_ATTR_LONG(c, "cross", 0, t_buf_repeatoverlap, cross_in);
    CLASS_ATTR_LONG(c, "overlap", 0, t_buf_repeatoverlap, nOverlap_in);
    CLASS_ATTR_LONG(c, "maxoutchannel", 0, t_buf_repeatoverlap, maxOutChannel_in);

    earsbufobj_class_add_outname_attr(c);
    earsbufobj_class_add_blocking_attr(c);
    earsbufobj_class_add_naming_attr(c);
    
    earsbufobj_class_add_polyout_attr(c);

    class_register(CLASS_BOX, c);
    s_tag_class = c;
    ps_event = gensym("event");
}

void buf_repeatoverlap_assist(t_buf_repeatoverlap *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET) {
        if (a == 0) // @in 0 @type symbol @digest Buffer name(s)
            sprintf(s, "symbol: Buffer Names");
        else if (a == 1) // @in 1 @type number/symbol @digest number of repeat
            sprintf(s, "Buffer/float: multiply silence portion"); // @number of repeat
    } else {
        sprintf(s, "New Buffer Names"); // @out 0 @type symbol/list @digest Output buffer names(s)
                                            // @description Name of the buffer
    }
}

void buf_repeatoverlap_inletinfo(t_buf_repeatoverlap *x, void *b, long a, char *t)
{
    if (a)
        *t = 1;
}


t_buf_repeatoverlap *buf_repeatoverlap_new(t_symbol *s, short argc, t_atom *argv)
{
    t_buf_repeatoverlap *x;
    long true_ac = attr_args_offset(argc, argv);
    
    x = (t_buf_repeatoverlap*)object_alloc_debug(s_tag_class);
    if (x) {
        
        x->envin = llll_from_text_buf("1", false);
        x->sampMin_in = 100;
        x->repeatMult_in = 10;
        x->cross_in = 1;
        x->nOverlap_in = 2;
        x->maxOutChannel_in = 2;
  
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


void buf_repeatoverlap_free(t_buf_repeatoverlap *x)
{
    llll_free(x->envin);
    earsbufobj_free((t_earsbufobj *)x);
}




void buf_repeatoverlap_bang(t_buf_repeatoverlap *x)
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
        
        wavesetrepeat_bang(x, in, out, mod, modVal, modType);
        
        if (earsbufobj_iter_progress((t_earsbufobj *)x, count, num_buffers)) break;
    }
    
    earsbufobj_mutex_unlock((t_earsbufobj *)x);

    earsbufobj_outlet_buffer((t_earsbufobj *)x, 0);
}


void buf_repeatoverlap_anything(t_buf_repeatoverlap *x, t_symbol *msg, long ac, t_atom *av)
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
            
            buf_repeatoverlap_bang(x);
        
        } else if (inlet == 1) {
            llll_free(x->envin);
            x->envin = llll_clone(parsed);
        }
        
    }
    llll_free(parsed);
}


void wavesetrepeat_bang(t_buf_repeatoverlap *x, t_buffer_obj *buffer, t_buffer_obj *out, t_buffer_obj *mod, double modVal, int modType) {

    t_float        *tab;
    t_float        *envelope;

    int minsampl = CLAMP(x->sampMin_in, 1, 5000);
    int ncross = CLAMP(x->cross_in, 1, 1000);
    int repeatMult = CLAMP(x->repeatMult_in, 0, 5000) ;
    int nOverlap = CLAMP(x->nOverlap_in, 2, 500);
    int maxOutChannel = CLAMP(x->maxOutChannel_in, 1, 16);
    

    
    int  g = 1, minsamplcount = 0, h = 0, j, k, r = 0, ncrossindex = 0,  currPeriod, currIndexA, newPeriod, overlapOnset = 0 , overlapOnsetFactor = 0, oldPeriod = 0, newIndex = 0, oldIndex = 0;
    
    double windowA, hanning;
   
    int crosscount = 0;
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
    int maxmemory = (int)(frames * (modVal+1));
    double *dataout =  (double*) sysmem_newptr(maxmemory * sizeof(double));

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    
    int z, v , ver;
    for (z = 1 ; z < (nchan + 1) ; z++) {
        int inc = 0;
        
        for (v = 0 ; v < (frames * nchan) ; v++) {
            ver = (v % nchan) + 1;
            if (ver == z) {
                inc++;
                if (z == 1) {
                    inbuffer[inc] = tab[v];
                } else {
                    inbuffer[inc] = (inbuffer[inc] + tab[v]) * 0.5;
                }
            }
        }
    }
    
    
    
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
    
    int allocVal = modVal;
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    float *envOnset = (float *)bach_newptr(crosscount * sizeof(float));
    if (modType == 1) {
        
        ears_resample_linear(envelope, envelopeFrames, &envOnset, crosscount, ((float)crosscount/(float)envelopeFrames), 1);
        
        allocVal = repeatMult;
    } else {
        
        for (int gg = 0 ; gg < crosscount ; gg++) {
            envOnset[gg] = CLAMP(modVal, nOverlap, 5000);
        
        }
    }
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    
    int a = 0;
    while (a <= (frames * allocVal) * maxOutChannel) {
        if (a >= maxmemory) {
            
            maxmemory = maxmemory + round(maxmemory/4);
            dataout =  (double*)sysmem_resizeptrclear(dataout, maxmemory * sizeof(double));
        }
        dataout[a] = 0;
        a++;
        
    }
  
    
    
    int chOffset = 0;
    
    while (g <= crosscount) {
        
        
        
        currPeriod = (zerocrossindex[g] - zerocrossindex[g  - 1]);
        
        if (modType == 1) {
            newPeriod = currPeriod * CLAMP((envOnset[g] * repeatMult) + nOverlap, nOverlap, 5000);
        } else {
            newPeriod = currPeriod * envOnset[g];
        }
        
        
        if (maxOutChannel == 1) {
            chOffset = 0;
        } else if (chOffset >= maxOutChannel) {
            chOffset = 1;
        } else {
            chOffset++;
        }
        
        r = 0;
        
        while (r < newPeriod) {
            r++;
            
            float diffA = fabs(inbuffer[zerocrossindex[g - 1]]) + fabs(inbuffer[zerocrossindex[g] - 1]);
                            
          
                            
            float sampleCorrectionA = ((float)(r % currPeriod)/(currPeriod)) * diffA - inbuffer[zerocrossindex[g - 1]];
                            
           
            
            currIndexA = zerocrossindex[g  - 1] + (r % currPeriod);
            

            if ((oldIndex + r) >= maxmemory) {
                
                maxmemory = maxmemory + round(maxmemory/4);
                dataout =  (double*)sysmem_resizeptrclear(dataout, maxmemory * sizeof(double));
            }
            
            windowA = sin(((double)r/newPeriod) * PI);
            
            hanning = cos((PI*2) * ((double)r/(newPeriod-1))) * (-0.5) + 0.5;
            
            newIndex = (oldIndex + r) - overlapOnsetFactor;
            
            dataout[(newIndex * maxOutChannel) + chOffset] = ( dataout[(newIndex * maxOutChannel) + chOffset] + ((inbuffer[currIndexA] + sampleCorrectionA)  * hanning) ) * 0.9;
            
            
            
        }
        oldPeriod = newPeriod;
        overlapOnset = newPeriod / nOverlap;
        overlapOnsetFactor = oldPeriod - overlapOnset;
        oldIndex = newIndex;
        g++;
    }

    h = newIndex;

    ears_buffer_set_size_and_numchannels((t_object *) x, out, h, maxOutChannel);
    ears_buffer_set_sr((t_object *) x, out, sampleRate);
    
    float *outtab = ears_buffer_locksamples(out);

    
    for (k = 0 ; k < (h * maxOutChannel) ; k++) {
        double f = dataout[k];
        outtab[k] = f;
    }

    bach_freeptr(envOnset);
    buffer_unlocksamples(buffer);
    ears_buffer_unlocksamples(out);
    sysmem_freeptr(zerocrossindex);
    sysmem_freeptr(dataout);
    return;
}
