/**
@file
wes.repeat.pendulum.c

@name
wes.repeat.pendulum~

@realname
wes.repeat.pendulum~

@type
object

@module
waveset

@author
Marco Marasciuolo

@digest
 Reads forwards and backwards

@description
 Read each group of wavesets in a zig-zag mode,  for each repetition change the pitch.

@discussion

@category
waveset basic

@keywords
buffer, waveset

@seealso
wes.repeat.overlap~ , wes.repeat.enveloping~ , wes.repeat.attract~, wes.repeat.simplify~

@owner
Marco Marasciuolo
*/
#include "ext.h"
#include "ext_obex.h"
#include "foundation/llllobj.h"
#include "foundation/llll_commons_ext.h"
#include "math/bach_math_utilities.h"
#include "ears.object.h"





typedef struct _buf_wavependulum {
    t_earsbufobj        e_ob;
    long sampMin_in;
    long cross_in;
    long nBackwards_in;
    long nWaveBack_in;
    
} t_buf_wavependulum;




// Prototypes
t_buf_wavependulum*         buf_wavependulum_new(t_symbol *s, short argc, t_atom *argv);
void            buf_wavependulum_free(t_buf_wavependulum *x);
void            buf_wavependulum_bang(t_buf_wavependulum *x);
void            buf_wavependulum_anything(t_buf_wavependulum *x, t_symbol *msg, long ac, t_atom *av);

void buf_wavependulum_assist(t_buf_wavependulum *x, void *b, long m, long a, char *s);
void buf_wavependulum_inletinfo(t_buf_wavependulum *x, void *b, long a, char *t);

void wavependulum_bang(t_buf_wavependulum *x, t_buffer_obj *buffer, t_buffer_obj *out);

// Globals and Statics
static t_class    *s_tag_class = NULL;
static t_symbol    *ps_event = NULL;

EARSBUFOBJ_ADD_IO_METHODS(wavependulum)

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
    
    CLASS_NEW_CHECK_SIZE(c, "wes.repeat.pendulum~",
                         (method)buf_wavependulum_new,
                         (method)buf_wavependulum_free,
                         sizeof(t_buf_wavependulum),
                         (method)NULL,
                         A_GIMME,
                         0L);
    
    // @method list/llll @digest Process buffers
    // @description A list or llll with buffer names will trigger the buffer processing and output the processed
    // buffer names (depending on the <m>naming</m> attribute).
    EARSBUFOBJ_DECLARE_COMMON_METHODS_HANDLETHREAD(wavependulum)
    
    CLASS_ATTR_LONG(c, "minSamp", 0, t_buf_wavependulum, sampMin_in);
    CLASS_ATTR_LONG(c, "cross", 0, t_buf_wavependulum, cross_in);
    CLASS_ATTR_LONG(c, "nBackwards", 0, t_buf_wavependulum, nBackwards_in);
    CLASS_ATTR_LONG(c, "nWaveBack", 0, t_buf_wavependulum, nWaveBack_in);

    earsbufobj_class_add_outname_attr(c);
    earsbufobj_class_add_blocking_attr(c);
    earsbufobj_class_add_naming_attr(c);
    
    earsbufobj_class_add_polyout_attr(c);

    class_register(CLASS_BOX, c);
    s_tag_class = c;
    ps_event = gensym("event");
}

void buf_wavependulum_assist(t_buf_wavependulum *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET) {
        if (a == 0) // @in 0 @type symbol @digest Buffer name(s)
            sprintf(s, "symbol: Buffer Names");
        else if (a == 1) // @in 1 @type number/symbol @digest number of repeat
            sprintf(s, "Buffer/float: number of repeat"); // @number of repeat
    } else {
        sprintf(s, "New Buffer Names"); // @out 0 @type symbol/list @digest Output buffer names(s)
                                            // @description Name of the buffer
    }
}

void buf_wavependulum_inletinfo(t_buf_wavependulum *x, void *b, long a, char *t)
{
    if (a)
        *t = 1;
}


t_buf_wavependulum *buf_wavependulum_new(t_symbol *s, short argc, t_atom *argv)
{
    t_buf_wavependulum *x;
    long true_ac = attr_args_offset(argc, argv);
    
    x = (t_buf_wavependulum*)object_alloc_debug(s_tag_class);
    if (x) {
        
        x->sampMin_in = 15;
        x->cross_in = 1;
        x->nBackwards_in = 3;
        x->nWaveBack_in = 3;
  
        earsbufobj_init((t_earsbufobj *)x,  0);
        
        // @arg 0 @name outnames @optional 1 @type symbol
        // @digest Output buffer names
        // @description @copy EARS_DOC_OUTNAME_ATTR
        
  
        
        t_llll *args = llll_parse(true_ac, argv);
        t_llll *names = earsbufobj_extract_names_from_args((t_earsbufobj *)x, args);
        
 
        
        attr_args_process(x, argc, argv);
        
        earsbufobj_setup((t_earsbufobj *)x, "E", "E", names);
        
        llll_free(args);
        llll_free(names);
    }
    return x;
}


void buf_wavependulum_free(t_buf_wavependulum *x)
{
    earsbufobj_free((t_earsbufobj *)x);
}




void buf_wavependulum_bang(t_buf_wavependulum *x)
{
    long num_buffers = earsbufobj_get_instore_size((t_earsbufobj *)x, 0);
    earsbufobj_refresh_outlet_names((t_earsbufobj *)x);
    earsbufobj_resize_store((t_earsbufobj *)x, EARSBUFOBJ_IN, 0, num_buffers, true);
    
    earsbufobj_mutex_lock((t_earsbufobj *)x);
    earsbufobj_init_progress((t_earsbufobj *)x, num_buffers);
    
    for (long count = 0; count < num_buffers; count++) {
        t_buffer_obj *in = earsbufobj_get_inlet_buffer_obj((t_earsbufobj *)x, 0, count);
        t_buffer_obj *out = earsbufobj_get_outlet_buffer_obj((t_earsbufobj *)x, 0, count);


        wavependulum_bang(x, in, out);
        
        if (earsbufobj_iter_progress((t_earsbufobj *)x, count, num_buffers)) break;
    }
    
    earsbufobj_mutex_unlock((t_earsbufobj *)x);

    earsbufobj_outlet_buffer((t_earsbufobj *)x, 0);
}


void buf_wavependulum_anything(t_buf_wavependulum *x, t_symbol *msg, long ac, t_atom *av)
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
            
            buf_wavependulum_bang(x);
        }
    }
    llll_free(parsed);
}


void wavependulum_bang(t_buf_wavependulum *x, t_buffer_obj *buffer, t_buffer_obj *out) {

    t_float        *tab;

    int minsampl = CLAMP(x->sampMin_in, 1, 5000);
    int ncross = CLAMP(x->cross_in, 1, 1000);
    int nBackwards = CLAMP(x->nBackwards_in, 1, 5000);
    int nWaveBack = CLAMP(x->nWaveBack_in, 1, 5000);
    

    long        frames, sampleRate;

    tab = ears_buffer_locksamples(buffer);
    frames = buffer_getframecount(buffer);
    sampleRate = buffer_getsamplerate(buffer);
    
    int nchan;
    nchan = buffer_getchannelcount(buffer);
    double *inbuffer =  (double*) sysmem_newptr(frames * sizeof(double));
    
    int maxcross = round(frames / (minsampl * ncross));
    int *zerocrossindex = (int*) sysmem_newptr(maxcross * sizeof(int));
   
    int maxmemory = (int)(frames);
    double *dataout =  (double*) sysmem_newptr(maxmemory * sizeof(double));
    
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
        
        int  g = 1, minsamplcount = 0, h = 0, b = 0, j, k, ncrossindex = 0, a, n = 0, indice = 0,  currPeriod, newPeriod;
        double bCF, aCF, res, idxD, scaleCF;
        int crosscount = 0;
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        
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
        
        if (nBackwards % 2 < 1) {
            nBackwards =  nBackwards + 1;
        }
        
        
        g = nWaveBack;
        int f, rev;
        
        while (g <= crosscount) {
            
            
            while (indice < nBackwards) {
                //indice++;
                
                currPeriod = (zerocrossindex[g] - zerocrossindex[g  - nWaveBack]);
                
                //newPeriod = CLAMP(round(currPeriod * 1./(nBackwards - (indice - 1))), 2, frames);
                
                newPeriod = CLAMP(round(currPeriod * (1 - ((float)indice / nBackwards))), 2, frames);
                
                
                
                while (n < newPeriod) {
                    
                    
                    if (indice % 2 > 0) {
                        f = n;
                        rev = 1;
                    } else {
                        f = newPeriod - n;
                        rev = -1;
                    }
                    
                    
                    scaleCF = ((double)currPeriod - 1) / ((double)newPeriod - 1);;
                    idxD = (double)scaleCF * f;
                    a = (int)idxD;
                    b = a + 1;
                    bCF = idxD - a;
                    aCF = 1.0 - bCF;
                    res = (double)(aCF * inbuffer[a + zerocrossindex[g - nWaveBack]] + bCF * inbuffer[b + zerocrossindex[g - nWaveBack]]);
                    
                    if (h >= maxmemory) {
                        maxmemory = maxmemory + round(maxmemory/4);
                        dataout =  (double*)sysmem_resizeptrclear(dataout, maxmemory * sizeof(double));
                        
                    }
                    
                    dataout[h] = (res * rev) * 0.9;
                    n++;
                    h++;
                    
                }
                n = 0;
                indice++;
            }
            indice = 0;
            g++;
        }
        
        if (z == 1) {
            frameout = h - 1;;
        }
        
        ears_buffer_set_size_and_numchannels((t_object *) x, out, frameout, nchan);
        ears_buffer_set_sr((t_object *) x, out, sampleRate);
        
        float *outtab = ears_buffer_locksamples(out);
        
        
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
    }
    
    
    ears_buffer_unlocksamples(buffer);
    sysmem_freeptr(zerocrossindex);
    sysmem_freeptr(dataout);
    sysmem_freeptr(inbuffer);
    return;
}
